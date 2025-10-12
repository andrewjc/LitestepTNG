/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*  WindowManager.cpp
*  The nModules Project
*
*  Monitors all existing top-level windows. Forwards notifications to the
*  taskbars.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "../nShared/LiteStep.h"
#include "ButtonSettings.hpp"
#include "TaskButton.hpp"
#include "Taskbar.hpp"
#include "WindowManager.h"
#include "../nShared/MonitorInfo.hpp"
#include "../nShared/LSModule.hpp"
#include <map>
#include <vector>
#include <assert.h>
#include <process.h>
#include "Constants.h"
#include <VersionHelpers.h>
#include <thread>
#include <algorithm>
#include "../nCoreCom/Core.h"


using std::vector;
using std::thread;


// All current taskbars
extern TaskbarMap gTaskbars;

extern LSModule gLSModule;

// The messages that the Window Manager wants from the core
const UINT gWMMessages[] = {
    // Standard HSHELL
    LM_WINDOWCREATED, LM_WINDOWACTIVATED, LM_WINDOWDESTROYED, LM_LANGUAGE,
    LM_REDRAW, LM_GETMINRECT, LM_WINDOWREPLACED, LM_WINDOWREPLACING, LM_MONITORCHANGED,

    // Progress bar
    LM_TASK_SETPROGRESSSTATE, LM_TASK_SETPROGRESSVALUE, LM_TASK_MARKASACTIVE,

    // MDI
    LM_TASK_REGISTERTAB, LM_TASK_UNREGISTERTAB, LM_TASK_SETACTIVETAB, LM_TASK_SETTABORDER,
    LM_TASK_SETTABPROPERTIES,

    // Overlay
    LM_TASK_SETOVERLAYICON, LM_TASK_SETOVERLAYICONDESC,

    // Thumbnail
    LM_TASK_SETTHUMBNAILTOOLTIP, LM_TASK_SETTHUMBNAILCLIP,

    // Buttons
    LM_TASK_THUMBBARADDBUTTONS, LM_TASK_THUMBBARUPDATEBUTTONS, LM_TASK_THUMBBARSETIMAGELIST, 0 };

namespace WindowManager
{
    // The currently active window
    HWND activeWindow = nullptr;

    // Contains all current top-level windows.
    WindowMap windowMap;

    // True if the windowmanager is running.
    bool isStarted = false;

    // True if we havent added existing windows yet.
    bool initializing = true;
}


/// <summary>
/// Starts the window manager.
/// </summary>
void WindowManager::Start()
{
    // Check that we aren't already running
    ASSERT(!isStarted);
    isStarted = true;

    // Get the currently active window
    SetActive(GetForegroundWindow());

    // If we are running something prior to Windows 8 we need to manually keep track of which
    // monitor a window is on.
    if (!IsWindows8OrGreater())
    {
        SetTimer(gLSModule.GetMessageWindow(), TIMER_CHECKMONITOR, 250, nullptr);
    }
    SetTimer(gLSModule.GetMessageWindow(), TIMER_MAINTENANCE, 250, nullptr);

    SendMessage(LiteStep::GetLitestepWnd(), LM_REGISTERMESSAGE, (WPARAM)gLSModule.GetMessageWindow(), (LPARAM)gWMMessages);
}


/// <summary>
/// Stops the window manager. Removes all windows from the taskbars and drops
/// the shellhook.
/// </summary>
void WindowManager::Stop()
{
    // Check that we are currently running
    ASSERT(isStarted);

    // Clean up
    SendMessage(LiteStep::GetLitestepWnd(), LM_UNREGISTERMESSAGE, (WPARAM)gLSModule.GetMessageWindow(), (LPARAM)gWMMessages);
    KillTimer(gLSModule.GetMessageWindow(), TIMER_CHECKMONITOR);
    KillTimer(gLSModule.GetMessageWindow(), TIMER_MAINTENANCE);
    activeWindow = nullptr;
    windowMap.clear();
    isStarted = false;
    initializing = true;
}


/// <summary>
/// Adds the specified top level window to the list of windows.
/// </summary>
void WindowManager::AddWindow(HWND hWnd)
{
    // Check that we are currently running
    ASSERT(isStarted);

    // If we should add this window to the taskbars
    if (IsTaskbarWindow(hWnd))
    {
        // Get the title here, we want to display it in the trace.
        WCHAR title[MAX_LINE_LENGTH];
        GetWindowTextW(hWnd, title, _countof(title));

        // Make sure we don't already have this window in the map.
        if (windowMap.find(hWnd) != windowMap.end())
        {
            TRACEW(L"AddWindow called with existing window!: %u %s", hWnd, title);
            return;
        }

        // Get information about the window
        WindowInformation &wndInfo = windowMap[hWnd];
        wndInfo.uMonitor = nCore::FetchMonitorInfo().MonitorFromHWND(hWnd);
        wndInfo.lastUpdateTime = GetTickCount64();
        wndInfo.updateDuringMaintenance = false;

        // Add it to any taskbar that wants it
        for (TaskbarMap::value_type &taskbar : gTaskbars)
        {
            TaskButton *taskButton = taskbar.second.AddTask(hWnd, wndInfo.uMonitor, initializing);

            // If the taskbar created a button for this window
            if (taskButton != nullptr)
            {
                // Add it to our list of buttons
                wndInfo.buttons.push_back(taskButton);

                // Set the icon and text of the window.
                taskButton->SetText(title);
            }
        }
        UpdateIcon(hWnd);
    }
}


/// <summary>
/// Removes/Adds the specified window to taskbars, based on the monitor change.
/// </summary>
void WindowManager::MonitorChanged(HWND hWnd, UINT monitor)
{
    // Check that we are currently running
    ASSERT(isStarted);

    WindowMap::iterator iter = windowMap.find(hWnd);
    if (iter != windowMap.end())
    {
        WCHAR title[MAX_LINE_LENGTH];
        GetWindowTextW(hWnd, title, _countof(title));
        iter->second.uMonitor = monitor;

        for (TaskbarMap::value_type &taskbar : gTaskbars)
        {
            TaskButton *out;
            if (taskbar.second.MonitorChanged(hWnd, monitor, &out))
            {
                if (out != nullptr)
                {
                    iter->second.buttons.push_back(out);
                    out->SetIcon(iter->second.hIcon);
                    if (iter->second.hOverlayIcon != nullptr)
                    {
                        out->SetOverlayIcon(iter->second.hOverlayIcon);
                    }
                    out->SetText(title);
                }
            }
            else
            {
                if (out != nullptr)
                {
                    iter->second.buttons.remove_if([out] (TaskButton *btn)
                    {
                        return btn == out;
                    });
                }
            }
        }
    }
    else
    {
        TRACE("MonitorChanged called with invalid HWND: %u", hWnd);
    }
}


/// <summary>
/// Updates the currently active window.
/// </summary>
void WindowManager::SetActive(HWND hWnd)
{
    // Check that we are currently running
    ASSERT(isStarted);

    // Remove the active flag from the previously active buttons
    WindowMap::iterator iter = windowMap.find(activeWindow);
    if (iter != windowMap.end())
    {
        for (TaskButton* button : iter->second.buttons)
        {
            button->Deactivate();
        }
    }

    // Swap the active window
    activeWindow = hWnd;

    // And set the flag for the now active buttons
    iter = windowMap.find(activeWindow);
    if (iter != windowMap.end())
    {
        for (TaskButton* button : iter->second.buttons)
        {
            button->Activate();
        }
    }
    else if (IsTaskbarWindow(hWnd)) // Steam...
    {
        AddWindow(hWnd);
    }
}


/// <summary>
/// Marks the specified window as minimized.
/// </summary>
void WindowManager::MarkAsMinimized(HWND hWnd)
{
    WindowMap::const_iterator iter = windowMap.find(hWnd);
    if (iter != windowMap.end())
    {
        for (TaskButton *button : iter->second.buttons)
        {
            button->ActivateState(TaskButton::State::Minimized);
        }
    }
}


/// <summary>
/// Removes the specified window from all taskbars.
/// </summary>
void WindowManager::RemoveWindow(HWND hWnd)
{
    // Check that we are currently running
    ASSERT(isStarted);

    WindowMap::iterator iter = windowMap.find(hWnd);
    if (iter != windowMap.end())
    {
        // Remove all buttons
        for (TaskbarMap::value_type &taskbar : gTaskbars)
        {
            taskbar.second.RemoveTask(hWnd);
        }

        if (iter->second.hOverlayIcon != nullptr)
        {
            DestroyIcon(iter->second.hOverlayIcon);
        }

        windowMap.erase(iter);
    }
    else
    {
        //TRACE("RemoveWindow called with invalid HWND: %u", hWnd);
    }

    if (activeWindow == hWnd)
    {
        activeWindow = nullptr;
    }
}


/// <summary>
/// Updates the text and icon of the specified window.
/// </summary>
void WindowManager::UpdateWindow(HWND hWnd, LPARAM lParam)
{
    // Check that we are currently running
    ASSERT(isStarted);

    WindowMap::iterator iter = windowMap.find(hWnd);
    if (iter != windowMap.end())
    {
        // There are a few (very few) windows which update 100s of times/second (the cygwin
        // installer). We can't render that fast, and end up locking up the shell with our backlog
        // of HSHELL_REDRAW messages. Therefore, if we get called too frequently, we just defer the
        // update until the next maintenance cycle.
        if (GetTickCount64() - iter->second.lastUpdateTime < 100) {
            iter->second.updateDuringMaintenance = true;
            return;
        }

        // Update the text
        WCHAR title[MAX_LINE_LENGTH];
        GetWindowTextW(hWnd, title, _countof(title));
        for (TaskButton *button : iter->second.buttons)
        {
            button->SetText(title);
        }

        // Update the icon
        UpdateIcon(hWnd);

        // Check if we should be flashing
        if (lParam == HSHELL_HIGHBIT)
        {
            for (TaskButton *button : iter->second.buttons)
            {
                button->Flash();
            }
        }

        iter->second.lastUpdateTime = GetTickCount64();
        iter->second.updateDuringMaintenance = false;
    }
    else if (IsTaskbarWindow(hWnd))
    {
        AddWindow(hWnd);
    }
    else
    {
        TRACE("UpdateWindow called with invalid HWND: %u", hWnd);
    }
}


/// <summary>
/// Retrives the rectangle of the first taskbutton for the specified HWND.
/// </summary>
LRESULT WindowManager::GetMinRect(HWND hWnd, LPPOINTS lpPoints)
{
    // Check that we are currently running
    ASSERT(isStarted);

    WindowMap::const_iterator iter = windowMap.find(hWnd);
    if (iter != windowMap.end() && !iter->second.buttons.empty())
    {
        iter->second.buttons.front()->GetMinRect(lpPoints);
        return 1;
    }
    return 0;
}


/// <summary>
/// Updates the monitor information for each window.
/// </summary>
void WindowManager::UpdateWindowMonitors()
{
    std::list<std::pair<HWND, UINT>> mods;
    for (WindowMap::value_type &val : windowMap)
    {
        UINT monitor = nCore::FetchMonitorInfo().MonitorFromHWND(val.first);
        if (monitor != val.second.uMonitor)
        {
            mods.emplace_back(val.first, monitor);
        }
    }
    if (!mods.empty())
    {
        // Prevent all taskbars from painting during the update.
        std::list<Window::UpdateLock> updateLocks;
        for (auto &taskbar : gTaskbars)
        {
            updateLocks.emplace_back(taskbar.second.GetWindow());
        }

        for (std::pair<HWND, UINT> &mod : mods)
        {
            MonitorChanged(mod.first, mod.second);
        }

        // Clear update locks
        updateLocks.clear();
    }
}


/// <summary>
/// Processes shell hook messages.
/// </summary>
LRESULT WindowManager::ShellMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // A window is being minimized or restored, the system needs the coordinates of
        // the taskbutton for the window animation.
    case LM_GETMINRECT:
        {
            WINDOWPLACEMENT wp;
            GetWindowPlacement((HWND)wParam, &wp);
            if (wp.showCmd == SW_SHOWMINIMIZED || wp.showCmd == SW_SHOWMINNOACTIVE)
            {
                // The window is being minimized, might as well give the button a hint.
                MarkAsMinimized((HWND)wParam);
            }
        }
        return GetMinRect((HWND)wParam, (LPPOINTS)lParam);

        // The active input language has changed
    case LM_LANGUAGE:
        {
        }
        return 0;

        // Text/Icon/Blinking change
    case LM_REDRAW:
        {
            UpdateWindow((HWND)wParam, lParam);
        }
        return 0;

        // The active window has changed
    case LM_WINDOWACTIVATED:
        {
            SetActive((HWND)wParam);
        }
        return 0;

        // A new top level window has been created
    case LM_WINDOWCREATED:
        {
            AddWindow((HWND)wParam);
        }
        return 0;

        // A top level window has been destroyed
    case LM_WINDOWDESTROYED:
        {
            RemoveWindow((HWND)wParam);
        }
        return 0;

        // A top level window has been replaced
    case LM_WINDOWREPLACED:
        {
            AddWindow((HWND)lParam);
        }
        return 0;

        // A window is about to be replaced
    case LM_WINDOWREPLACING:
        {
            RemoveWindow((HWND)wParam);
        }
        return 0;

        // Windows 8+ A window has moved to a different monitor
    case LM_MONITORCHANGED:
        {
            MonitorChanged((HWND)wParam, nCore::FetchMonitorInfo().MonitorFromHWND((HWND)wParam));
        }
        return 0;

        // The display layout has changed.
    case NCORE_DISPLAYCHANGE:
        {
            // TODO:: Ensure this is called after the core updates its monitor info.
            UpdateWindowMonitors();
        }
        return 0;

        //
    case WM_TIMER:
        {
            switch(wParam)
            {
            case TIMER_CHECKMONITOR:
                {
                    UpdateWindowMonitors();
                }
                return 0;

            case TIMER_MAINTENANCE:
                {
                    RunWindowMaintenance();
                }
                return 0;
            }
        }
        return 0;

    case LM_TASK_SETOVERLAYICON:
        {
            SetOverlayIcon((HWND)wParam, (HICON)lParam);
        }
        return 0;

    case LM_TASK_SETPROGRESSSTATE:
        {
        }
        return 0;

    case LM_TASK_SETPROGRESSVALUE:
        {
        }
        return 0;

        //
    case WM_ADDED_EXISTING:
        {
            // Relayout all taskbars.
            initializing = false;
            for (auto &taskbar : gTaskbars)
            {
                taskbar.second.Relayout();
            }
        }
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


/// <summary>
/// Determines if a window should be shown on the taskbar.
/// </summary>
bool WindowManager::IsTaskbarWindow(HWND hWnd)
{
    // Make sure it's actually a window.
    if (!IsWindow(hWnd))
    {
        return false;
    }

    // And that it's visible
    if (!IsWindowVisible(hWnd))
    {
        return false;
    }

    LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);

    // Windows with the WS_EX_APPWINDOW style should always be shown
    if ((exStyle & WS_EX_APPWINDOW) == WS_EX_APPWINDOW)
    {
        return true;
    }
    else if (GetParent(hWnd) != NULL) // Windows with parents should not be shown
    {
        return false;
    }
    else if (GetWindow(hWnd, GW_OWNER) != NULL) // Windows with owners should not be shown
    {
        return false;
    }
    else if ((exStyle & WS_EX_TOOLWINDOW) == WS_EX_TOOLWINDOW) // Tool windows should not be shown on the taskbar
    {
        return false;
    }

    return true;
}


/// <summary>
/// Updates the icon of a particular HWND
/// </summary>
void WindowManager::SetIcon(HWND hWnd, HICON hIcon)
{
    WindowMap::iterator window = windowMap.find(hWnd);
    if (window != windowMap.end())
    {
        for (TaskButton *button : window->second.buttons)
        {
            button->SetIcon(hIcon);
        }
        if (window->second.hIcon != nullptr)
        {
            DestroyIcon(window->second.hIcon);
        }

        window->second.hIcon = CopyIcon(hIcon);
    }
}


/// <summary>
/// Updates the overlay icon of a particular HWND
/// </summary>
void WindowManager::SetOverlayIcon(HWND hWnd, HICON overlayIcon)
{
    WindowMap::iterator window = windowMap.find(hWnd);
    if (window != windowMap.end())
    {
        for (TaskButton *button : window->second.buttons)
        {
            button->SetOverlayIcon(overlayIcon);
        }
        if (window->second.hOverlayIcon != nullptr)
        {
            DestroyIcon(window->second.hOverlayIcon);
        }

        window->second.hOverlayIcon = CopyIcon(overlayIcon);
    }
}


/// <summary>
/// Sets the progress state for the specified window
/// </summary>
void WindowManager::SetProgressState(HWND hWnd, TBPFLAG state)
{
    WindowMap::iterator window = windowMap.find(hWnd);
    if (window != windowMap.end())
    {
        for (TaskButton *button : window->second.buttons)
        {
            button->SetProgressState(state);
        }
        window->second.progressState = state;
    }
}


/// <summary>
/// Sets the progress value for the specified window
/// </summary>
void WindowManager::SetProgressValue(HWND hWnd, USHORT progress)
{
    WindowMap::iterator window = windowMap.find(hWnd);
    if (window != windowMap.end())
    {
        for (TaskButton *button : window->second.buttons)
        {
            button->SetProgressValue(progress);
        }
        window->second.progress = progress;
    }
}


/// <summary>
/// Updates the icon of a particular HWND
/// </summary>
void CALLBACK WindowManager::UpdateIconCallback(HWND hWnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult)
{
    // We really only expect WM_GETICON messages.
    if (uMsg == WM_GETICON)
    {
        // If we got an icon back, use it.
        if (lResult != 0)
        {
            SetIcon(hWnd, (HICON)lResult);
        }
        else switch (dwData)
        {
        case ICON_BIG:
            {
                SendMessageCallback(hWnd, WM_GETICON, ICON_SMALL, NULL, UpdateIconCallback, ICON_SMALL);
            }
            break;

        case ICON_SMALL:
            {
                SendMessageCallback(hWnd, WM_GETICON, ICON_SMALL2, NULL, UpdateIconCallback, ICON_SMALL2);
            }
            break;

        case ICON_SMALL2:
            {
                HICON hIcon;
                hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICON);
                if (hIcon == nullptr)
                {
                    hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICONSM);
                }
                SetIcon(hWnd, hIcon);
            }
            break;
        }
    }
}


/// <summary>
/// Updates the icon of a particular HWND
/// </summary>
void WindowManager::UpdateIcon(HWND hWnd)
{
    SendMessageCallback(hWnd, WM_GETICON, ICON_BIG, NULL, UpdateIconCallback, ICON_BIG);
}


/// <summary>
/// Sends LM_WINDOWCREATED for all existing top-level windows.
/// </summary>
void WindowManager::AddExisting()
{
    thread([] () -> void
    {
        EnumDesktopWindows(nullptr, (WNDENUMPROC)[] (HWND window, LPARAM) -> BOOL
        {
            if (IsTaskbarWindow(window))
            {
                PostMessage(gLSModule.GetMessageWindow(), LM_WINDOWCREATED, (WPARAM)window, 0);
            }
            return TRUE;
        }, 0);

        PostMessage(gLSModule.GetMessageWindow(), WM_ADDED_EXISTING, 0, 0);
    }).detach();
}


/// <summary>
/// Removes any invalid windows, and rechecks the minimized state of windows.
/// </summary>
void WindowManager::RunWindowMaintenance()
{
    // Check that we are currently running
    ASSERT(isStarted);

    // Prevent all taskbars from painting during the update.
    std::vector<Window::UpdateLock*> updateLocks(gTaskbars.size());
    int i = 0;
    for (auto &taskbar : gTaskbars)
    {
        updateLocks[i++] = new Window::UpdateLock(taskbar.second.GetWindow());
    }

    vector<HWND> removals;
    for (WindowMap::iterator iter = windowMap.begin(); iter != windowMap.end(); iter++)
    {
        if (!IsWindow(iter->first))
        {
            removals.push_back(iter->first);
        }
        else
        {
            if (iter->second.updateDuringMaintenance)
            {
                UpdateWindow(iter->first, 0);
            }
            for (TaskButton *button : iter->second.buttons)
            {
                if (IsIconic(iter->first))
                {
                    button->ActivateState(TaskButton::State::Minimized);
                }
                else
                {
                    button->ClearState(TaskButton::State::Minimized);
                }
            }
        }
    }
    for (HWND hwnd : removals)
    {
        RemoveWindow(hwnd);
    }

    // Clear update locks
    for (Window::UpdateLock *updateLock : updateLocks)
    {
        delete updateLock;
    }
}
