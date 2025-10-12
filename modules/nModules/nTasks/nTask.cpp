/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  nTask.cpp
 *  The nModules Project
 *
 *  Main .cpp file for the nTask module.
 *  
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "../nShared/LiteStep.h"
#include "../nShared/LSModule.hpp"
#include "../nShared/ErrorHandler.h"
#include "../nCoreCom/Core.h"
#include "Constants.h"
#include "nTask.h"
#include "TestWindow.hpp"
#include "Taskbar.hpp"
#include "Version.h"
#include "WindowManager.h"


using std::map;

// The LSModule class
LSModule gLSModule(TEXT(MODULE_NAME), TEXT(MODULE_AUTHOR), MakeVersion(MODULE_VERSION));

// The messages we want from the core
const UINT gLSMessages[] = { LM_GETREVID, LM_REFRESH, LM_FULLSCREENACTIVATED,
    LM_FULLSCREENDEACTIVATED, 0 };
const UINT sCoreMessages[] = { NCORE_DISPLAYCHANGE, NCORE_SETTINGCHANGE, 0 };

// All the taskbars we currently have loaded
TaskbarMap gTaskbars;

// Set to whether or not windows are activated by hovering over them. In this
// mode we should automatically move the mouse cursor to the center of the
// window when activated.
BOOL gActiveWindowTracking = FALSE;

/// <summary>
/// Called by the LiteStep core when this module is loaded.
/// </summary>
EXPORT_CDECL(int) initModuleW(HWND parent, HINSTANCE instance, LPCWSTR /* path */)
{
    if (!gLSModule.Initialize(parent, instance))
    {
        return 1;
    }
    
    if (!gLSModule.ConnectToCore(MakeVersion(CORE_VERSION)))
    {
        return 1;
    }
    nCore::RegisterForCoreMessages(gLSModule.GetMessageWindow(), sCoreMessages);

    SystemParametersInfo(SPI_GETACTIVEWINDOWTRACKING, 0, &gActiveWindowTracking, 0);

    // Load settings
    LoadSettings();

    // Start the window manager
    WindowManager::Start();
     
    // Register a bang command for creating our test window
    LiteStep::AddBangCommand(L"!nTaskTestWindow", [] (HWND, LPCTSTR) -> void
    {
        TestWindow::Create();
    });

    return 0;
}


/// <summary>
/// Called by the core when this module is about to be unloaded.
/// </summary>
void quitModule(HINSTANCE /* hDllInstance */)
{
    LiteStep::RemoveBangCommand(L"!nTaskTestWindow");

    TestWindow::DestroyAll();
    WindowManager::Stop();
    DestroyTaskbars();
    gLSModule.DeInitalize();
}


/// <summary>
/// Handles the main window's messages.
/// </summary>
/// <param name="hWnd">The window the message is for.</param>
/// <param name="uMsg">The type of message.</param>
/// <param name="wParam">wParam</param>
/// <param name="lParam">lParam</param>
LRESULT WINAPI LSMessageHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
    case WM_CREATE:
        {
            // Add the existing windows in a little bit so we dont hinder startup.
            SetTimer(window, TIMER_ADD_EXISTING, 50, NULL);

            SendMessage(LiteStep::GetLitestepWnd(), LM_REGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
        }
        return 0;

    case NCORE_SETTINGCHANGE:
      if (wParam == SPI_SETACTIVEWINDOWTRACKING) {
        SystemParametersInfo(SPI_GETACTIVEWINDOWTRACKING, 0, &gActiveWindowTracking, 0);
      }
      return 0;

    case WM_DESTROY:
        {
            SendMessage(LiteStep::GetLitestepWnd(), LM_UNREGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
        }
        return 0;

    case WM_TIMER:
        {
            switch(wParam)
            {
            case TIMER_ADD_EXISTING:
                {
                    KillTimer(window, TIMER_ADD_EXISTING);
                    WindowManager::AddExisting();
                }
                return 0;

            default:
                {
                }
                return WindowManager::ShellMessage(window, message, wParam, lParam);
            }
        }
        break;

    case LM_FULLSCREENACTIVATED:
        {
            for (auto &taskbar : gTaskbars)
            {
                taskbar.second.GetWindow()->FullscreenActivated((HMONITOR) wParam, (HWND) lParam);
            }
        }
        return 0;

    case LM_FULLSCREENDEACTIVATED:
        {
            for (auto &taskbar : gTaskbars)
            {
                taskbar.second.GetWindow()->FullscreenDeactivated((HMONITOR) wParam);
            }
        }
        return 0;

    case LM_REFRESH:
        {
            WindowManager::Stop();
            DestroyTaskbars();
            WindowManager::Start();
            LoadSettings();
            WindowManager::AddExisting();
        }
        return 0;

    case LM_GETMINRECT:
    case LM_LANGUAGE:
    case LM_REDRAW:
    case LM_WINDOWACTIVATED:
    case LM_WINDOWCREATED:
    case LM_WINDOWDESTROYED:
    case LM_WINDOWREPLACED:
    case LM_WINDOWREPLACING:
    case LM_MONITORCHANGED:
    case NCORE_DISPLAYCHANGE:
    case WM_ADDED_EXISTING:
    case LM_TASK_SETPROGRESSSTATE:
    case LM_TASK_SETPROGRESSVALUE:
    case LM_TASK_MARKASACTIVE:
    case LM_TASK_REGISTERTAB:
    case LM_TASK_UNREGISTERTAB:
    case LM_TASK_SETACTIVETAB:
    case LM_TASK_SETTABORDER:
    case LM_TASK_SETTABPROPERTIES:
    case LM_TASK_SETOVERLAYICON:
    case LM_TASK_SETOVERLAYICONDESC:
    case LM_TASK_SETTHUMBNAILTOOLTIP:
    case LM_TASK_SETTHUMBNAILCLIP:
    case LM_TASK_THUMBBARADDBUTTONS:
    case LM_TASK_THUMBBARUPDATEBUTTONS:
    case LM_TASK_THUMBBARSETIMAGELIST:
      return WindowManager::ShellMessage(window, message, wParam, lParam);
    }
    return DefWindowProc(window, message, wParam, lParam);
}


/// <summary>
/// Destroys all taskbars.
/// </summary>
void DestroyTaskbars()
{
    gTaskbars.clear();
}


/// <summary>
/// Reads through the .rc files and creates Taskbars.
/// </summary>
void LoadSettings()
{
    LiteStep::IterateOverLineTokens(L"*nTaskbar", CreateTaskbar);
}


/// <summary>
/// Creates a new taskbar
/// </summary>
void CreateTaskbar(LPCTSTR taskbarName)
{
    if (gTaskbars.find(taskbarName) == gTaskbars.end())
    {
        gTaskbars.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(taskbarName),
            std::forward_as_tuple(taskbarName)
            );
    }
    else
    {
        ErrorHandler::Error(ErrorHandler::Level::Critical, L"Attempt to (re)create the already existing taskbar %s!", taskbarName);
    }
}
