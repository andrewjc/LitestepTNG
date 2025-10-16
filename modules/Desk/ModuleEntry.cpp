/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  nDesk.cpp
 *  The nModules Project
 *
 *  Main .cpp file for the nDesk module.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "../ModuleKit/LiteStep.h"
#include "../CoreCom/Core.h"
#include "../ModuleKit/MonitorInfo.hpp"
#include "../ModuleKit/LSModule.hpp"
#include "DesktopPainter.hpp"
#include "ClickHandler.hpp"
#include "WorkArea.h"
#include "Bangs.h"
#include "Settings.h"
#include <dwmapi.h>
#include "../ModuleKit/Color.h"
#include "Version.h"
#include <ShObjIdl.h>


// The messages we want from the core
UINT gLSMessages[] = { LM_GETREVID, LM_REFRESH, 0 };

// Class pointers
DesktopPainter *g_pDesktopPainter;
ClickHandler *g_pClickHandler;

// The LSModule class
LSModule gLSModule(TEXT(MODULE_NAME), TEXT(MODULE_AUTHOR), MakeVersion(MODULE_VERSION));

static HWND gShellView = nullptr;

static BOOL CALLBACK EnumWorkerWindowProc(HWND window, LPARAM param)
{
    HWND* target = reinterpret_cast<HWND*>(param);
    HWND defView = FindWindowEx(window, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr)
    {
        *target = defView;
        return FALSE;
    }
    return TRUE;
}

static HWND LocateShellView()
{
    HWND shellView = nullptr;
    HWND progman = FindWindow(L"Progman", nullptr);
    if (progman != nullptr)
    {
        SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
        shellView = FindWindowEx(progman, nullptr, L"SHELLDLL_DefView", nullptr);
    }
    if (shellView == nullptr)
    {
        EnumWindows(EnumWorkerWindowProc, reinterpret_cast<LPARAM>(&shellView));
    }
    return shellView;
}

static void ResizeShellView()
{
    if (gShellView == nullptr || g_pDesktopPainter == nullptr)
    {
        return;
    }

    const auto &desktop = nCore::FetchMonitorInfo().GetVirtualDesktop();
    SetWindowPos(gShellView, HWND_BOTTOM, 0, 0, desktop.width, desktop.height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static void AttachShellViewToDesktop()
{
    if (g_pDesktopPainter == nullptr)
    {
        return;
    }

    HWND shellView = LocateShellView();
    if (shellView == nullptr)
    {
        return;
    }

    HWND host = g_pDesktopPainter->GetWindow();
    if (GetParent(shellView) != host)
    {
        SetParent(shellView, host);
        LONG_PTR style = GetWindowLongPtr(shellView, GWL_STYLE);
        style |= WS_CHILD | WS_VISIBLE;
        style &= ~WS_POPUP;
        SetWindowLongPtr(shellView, GWL_STYLE, style);
    }

    ResizeShellView();

    ShowWindow(shellView, SW_SHOWNOACTIVATE);
    UpdateWindow(shellView);

    HWND listView = FindWindowEx(shellView, nullptr, L"SysListView32", nullptr);
    if (listView != nullptr)
    {
        LONG_PTR listStyle = GetWindowLongPtr(listView, GWL_STYLE);
        listStyle |= WS_VISIBLE;
        SetWindowLongPtr(listView, GWL_STYLE, listStyle);
        ShowWindow(listView, SW_SHOWNORMAL);
    }

    gShellView = shellView;
}




/// <summary>
/// Called by the LiteStep core when this module is loaded.
/// </summary>
EXPORT_CDECL(int) initModuleW(HWND /* parent */, HINSTANCE instance, LPCWSTR /* path */) {
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hInstance = instance;
    wc.lpszClassName = L"DesktopBackgroundClass";
    wc.hIconSm = 0;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_DBLCLKS;

    if (!gLSModule.ConnectToCore(MakeVersion(CORE_VERSION))) {
        return 1;
    }

    // Initialize
    g_pClickHandler = new ClickHandler();
    g_pDesktopPainter = nullptr; // Initialized on WM_CREATE

    if (!gLSModule.Initialize(nullptr, instance, &wc, nullptr)) {
        delete g_pClickHandler;
        return 1;
    }

    SetParent(g_pDesktopPainter->GetWindow(), GetDesktopWindow());
    SetWindowLongPtr(g_pDesktopPainter->GetWindow(), GWL_STYLE, GetWindowLongPtr(g_pDesktopPainter->GetWindow(), GWL_STYLE) | WS_CHILD | WS_CLIPCHILDREN);
    SetWindowPos(g_pDesktopPainter->GetWindow(), HWND_BOTTOM, nCore::FetchMonitorInfo().GetVirtualDesktop().rect.left,
      nCore::FetchMonitorInfo().GetVirtualDesktop().rect.top, nCore::FetchMonitorInfo().GetVirtualDesktop().width,
      nCore::FetchMonitorInfo().GetVirtualDesktop().height, SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    ShowWindow(g_pDesktopPainter->GetWindow(), SW_SHOWNOACTIVATE);
    BOOL excludeFromPeek = TRUE;
    DwmSetWindowAttribute(g_pDesktopPainter->GetWindow(), DWMWA_EXCLUDED_FROM_PEEK, &excludeFromPeek, sizeof(BOOL));
    AttachShellViewToDesktop();

    // Load bang commands
    Bangs::_Register();

    // Load settings
    nDesk::Settings::Load();
            AttachShellViewToDesktop();

    // Reset the work area for all monitors
    WorkArea::ResetWorkAreas(&nCore::FetchMonitorInfo());
    WorkArea::LoadSettings(&nCore::FetchMonitorInfo());
            AttachShellViewToDesktop();

    return 0;
}


/// <summary>
/// Called by the LiteStep core when this module is about to be unloaded.
/// </summary>
void quitModule(HINSTANCE /* instance */) {
    // Reset the work area for all monitors
    WorkArea::ResetWorkAreas(&nCore::FetchMonitorInfo());

    // Unregister bangs
    Bangs::_Unregister();

    // Delete global classes
    if (g_pDesktopPainter) delete g_pDesktopPainter;
    if (g_pClickHandler) delete g_pClickHandler;

    //
    if (gShellView != nullptr)
    {
        SetParent(gShellView, GetDesktopWindow());
        gShellView = nullptr;
    }
    gLSModule.DeInitalize();
}


/// <summary>
/// Handles the main window's messages.
/// </summary>
/// <param name="hWnd">The window the message is for.</param>
/// <param name="uMsg">The type of message.</param>
/// <param name="wParam">wParam</param>
/// <param name="lParam">lParam</param>
LRESULT WINAPI LSMessageHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message)
    {
    case WM_CREATE:
        {
            SendMessage(LiteStep::GetLitestepWnd(), LM_REGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
            g_pDesktopPainter = new DesktopPainter(window);
        }
        return 0;

    case WM_DESTROY:
        {
            SendMessage(LiteStep::GetLitestepWnd(), LM_UNREGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
        }
        return 0;

    case LM_REFRESH:
        {
            g_pClickHandler->Refresh();
            WorkArea::LoadSettings(&nCore::FetchMonitorInfo(), true);
            nDesk::Settings::Load();
        }
        return 0;

    case WM_PAINT:
    case WM_ERASEBKGND:
        return g_pDesktopPainter->HandleMessage(window, message, wParam, lParam);

    case WM_WINDOWPOSCHANGING:
        {
            // Keep the hWnd at the bottom of the window stack
            LPWINDOWPOS c = LPWINDOWPOS(lParam);
            c->hwnd = window;
            c->hwndInsertAfter = HWND_BOTTOM;
            c->flags &= ~SWP_HIDEWINDOW;
            c->flags |= SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOMOVE | SWP_SHOWWINDOW;
            c->x = nCore::FetchMonitorInfo().GetVirtualDesktop().rect.left;
            c->y = nCore::FetchMonitorInfo().GetVirtualDesktop().rect.top;
            c->cx = nCore::FetchMonitorInfo().GetVirtualDesktop().width;
            c->cy = nCore::FetchMonitorInfo().GetVirtualDesktop().height;
        }
        return 0;

    case NCORE_DISPLAYCHANGE:
        {
            // TODO::Ensure
            g_pDesktopPainter->Resize();
            nDesk::Settings::OnResolutionChange();
            WorkArea::LoadSettings(&nCore::FetchMonitorInfo());
            InvalidateRect(nullptr, nullptr, TRUE);
        }
        break;

    case WM_SETTINGCHANGE:
        {
            if (wParam == SPI_SETDESKWALLPAPER) {
                g_pDesktopPainter->UpdateWallpaper();
                return 0;
            }
        }
        break;

    case WM_CLOSE:
        // If someone tries to exit the desktop window, lets make it a windows shutdown.
        // PostMessage(GetLitestepWnd(), LM_RECYCLE, 3, 0);
        return 0;

    case WM_SYSCOMMAND:
        switch (wParam) {
            // For using the standard alt+F4 to shutdown windows
            case SC_CLOSE:
                // PostMessage(GetLitestepWnd(), LM_RECYCLE, 3, 0);
                return 0;
            default:
                break;
        }
        break;

    case WM_CHILDACTIVATE:
    case WM_NCACTIVATE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ACTIVATEAPP:
    case WM_ACTIVATE:
    case WM_PARENTNOTIFY:
        SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        break;
    }

    if (g_pDesktopPainter) {
        return g_pDesktopPainter->HandleMessage(window, message, wParam, lParam);
    }

    return DefWindowProc(window, message, wParam, lParam);
}

