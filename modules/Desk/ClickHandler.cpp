/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  ClickHandler.cpp
 *  The nModules Project
 *
 *  Handles clicks on the desktop.
 *  
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "../CoreCom/Core.h"
#include "../ModuleKit/LiteStep.h"
#include "../ModuleKit/MonitorInfo.hpp"

#include <algorithm>
#include <functional>
#include <strsafe.h>
#include <Windowsx.h>

#include "ClickHandler.hpp"


/// <summary>
/// Creates a new instance of the ClickHandler class.
/// </summary>
ClickHandler::ClickHandler() {
    LoadSettings();
}


/// <summary>
/// Destroys this instance of the ClickHandler class.
/// </summary>
ClickHandler::~ClickHandler() {
    m_clickHandlers.clear();
}


/// <summary>
/// Call this when a click is triggered.
/// </summary>
LRESULT ClickHandler::HandleMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, LPVOID) {
    ClickData cData;

    // Find the type of this click event
    switch (msg) {
        case WM_MOUSEWHEEL: cData.type = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? WHEELUP : WHEELDOWN; break;
        case WM_MOUSEHWHEEL: cData.type = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? WHEELRIGHT : WHEELLEFT; break;
        case WM_LBUTTONDOWN: cData.type = LEFTDOWN; break;
        case WM_LBUTTONUP: cData.type = LEFTUP; break;
        case WM_LBUTTONDBLCLK: cData.type = LEFTDOUBLE; break;
        case WM_MBUTTONDOWN: cData.type = MIDDLEDOWN; break;
        case WM_MBUTTONUP: cData.type = MIDDLEUP; break;
        case WM_MBUTTONDBLCLK: cData.type = MIDDLEDOUBLE; break;
        case WM_RBUTTONDOWN: cData.type = RIGHTDOWN; break;
        case WM_RBUTTONUP: cData.type = RIGHTUP; break;
        case WM_RBUTTONDBLCLK: cData.type = RIGHTDOUBLE; break;
        case WM_XBUTTONDOWN:
            switch (GET_XBUTTON_WPARAM(wParam)) {
                case XBUTTON1: cData.type = X1DOWN; break;
                case XBUTTON2: cData.type = X2DOWN; break;
                default: cData.type = UNKNOWN; break;
            }
            break;
        case WM_XBUTTONUP:
            switch (GET_XBUTTON_WPARAM(wParam)) {
                case XBUTTON1: cData.type = X1UP; break;
                case XBUTTON2: cData.type = X2UP; break;
                default: cData.type = UNKNOWN; break;
            }
            break;
        case WM_XBUTTONDBLCLK:
            switch (GET_XBUTTON_WPARAM(wParam)) {
                case XBUTTON1: cData.type = X1DOUBLE; break;
                case XBUTTON2: cData.type = X2DOUBLE; break;
                default: cData.type = UNKNOWN; break;
            }
            break;
        default: cData.type = UNKNOWN; break;
    }

    cData.mods = GET_KEYSTATE_WPARAM(wParam) & (4 | 8);
    // GET_X_LPARAM and GET_Y_LPARAM are relative to the desktop window.
    // cData.area is relative to the virual desktop.
    cData.area.left = cData.area.right = GET_X_LPARAM(lParam) + nCore::FetchMonitorInfo().GetVirtualDesktop().rect.left;
    cData.area.top = cData.area.bottom = GET_Y_LPARAM(lParam) + nCore::FetchMonitorInfo().GetVirtualDesktop().rect.top;

    for (vector<ClickData>::const_iterator iter = m_clickHandlers.begin(); iter != m_clickHandlers.end(); iter++) {
        if (Matches(cData, *iter)) {
            LiteStep::LSExecute(nullptr, iter->action, SW_SHOW);
        }
    }

    return DefWindowProc(window, msg, wParam, lParam);
}


/// <summary>
/// Reloads click settings.
/// </summary>
void ClickHandler::Refresh() {
    LoadSettings(true);
}


/// <summary>
/// Loads click settings.
/// </summary>
void ClickHandler::LoadSettings(bool /* bIsRefresh */) {
    LiteStep::IterateOverLines(L"*nDeskOn", [this] (LPCTSTR line) -> void {
        AddHandler(line);
    });
}


/// <summary>
/// Parses a click line.
/// </summary>
ClickHandler::ClickData ClickHandler::ParseLine(LPCTSTR szLine) {
    // !nDeskOn <type> <mods> <action>
    // !nDeskOn <type> <mods> <left> <top> <right> <bottom> <action>
    TCHAR szToken[MAX_LINE_LENGTH];
    LPCTSTR pszNext = szLine;
    ClickData cData;
    cData.type = UNKNOWN;

    using namespace LiteStep;

    // Type
    LiteStep::GetToken(pszNext, szToken, &pszNext, false);
    cData.type = TypeFromString(szToken);

    // ModKeys
    LiteStep::GetToken(pszNext, szToken, &pszNext, false);
    cData.mods = ModsFromString(szToken);

    if (pszNext == nullptr)
    {
        cData.type = UNKNOWN;
        return cData;
    }

    // Guess that the rest is an action for now
    StringCchCopy(cData.action, sizeof(cData.action), pszNext);
    cData.area = nCore::FetchMonitorInfo().GetVirtualDesktop().rect;

    // Check if we have 4 valid coordinates followed by some action
    int left, top, width, height;
    if (LiteStep::GetToken(pszNext, szToken, &pszNext, false) == FALSE) return cData;
    if (pszNext == NULL) return cData;
    left = _wtoi(szToken);

    if (LiteStep::GetToken(pszNext, szToken, &pszNext, false) == FALSE) return cData;
    if (pszNext == NULL) return cData;
    top = _wtoi(szToken);

    if (LiteStep::GetToken(pszNext, szToken, &pszNext, false) == FALSE) return cData;
    if (pszNext == NULL) return cData;
    width = _wtoi(szToken);

    if (LiteStep::GetToken(pszNext, szToken, &pszNext, false) == FALSE) return cData;
    if (pszNext == NULL) return cData;
    height = _wtoi(szToken);

    // If these are all valid coordinates
    cData.area.left = left;
    cData.area.right =  left + width;
    cData.area.top = top;
    cData.area.bottom = top + height;

    // Then the rest is the action
    StringCchCopy(cData.action, sizeof(cData.action), pszNext);

    return cData;
}


/// <summary>
/// Parses a click line.
/// </summary>
void ClickHandler::AddHandler(LPCTSTR szLine) {
    ClickData cData = ParseLine(szLine);
    if (cData.type == UNKNOWN) {
        // TODO::Throw an error
        return;
    }

    this->m_clickHandlers.push_back(cData);
}


/// <summary>
/// Removes any handlers matching the spcified criterias.
/// </summary>
void ClickHandler::RemoveHandlers(LPCTSTR szLine) {
    std::remove_if(m_clickHandlers.begin(), m_clickHandlers.end(), std::bind(&ClickHandler::Matches, this, ParseLine(szLine), std::placeholders::_1));
}


/// <summary>
/// True if ClickData a is "in" ClickData b.
/// </summary>
bool ClickHandler::Matches(ClickData a, ClickData b) {
    return  a.type == b.type &&
            a.mods == b.mods &&
            a.area.left >= b.area.left &&
            a.area.right <= b.area.right &&
            a.area.top >= b.area.top &&
            a.area.bottom <= b.area.bottom;
}


/// <summary>
/// Gets the clicktype from a user input string.
/// </summary>
ClickHandler::ClickType ClickHandler::TypeFromString(LPCTSTR str) {
    if (_wcsicmp(str, L"WheelUp") == 0) return WHEELUP;
    if (_wcsicmp(str, L"WheelDown") == 0) return WHEELDOWN;
    if (_wcsicmp(str, L"WheelRight") == 0) return WHEELRIGHT;
    if (_wcsicmp(str, L"WheelLeft") == 0) return WHEELLEFT;

    if (_wcsicmp(str, L"LeftClickDown") == 0) return LEFTDOWN;
    if (_wcsicmp(str, L"LeftClickUp") == 0) return LEFTUP;
    if (_wcsicmp(str, L"LeftDoubleClick") == 0) return LEFTDOUBLE;

    if (_wcsicmp(str, L"MiddleClickDown") == 0) return MIDDLEDOWN;
    if (_wcsicmp(str, L"MiddleClickUp") == 0) return MIDDLEUP;
    if (_wcsicmp(str, L"MiddleDoubleClick") == 0) return MIDDLEDOUBLE;

    if (_wcsicmp(str, L"RightClickDown") == 0) return RIGHTDOWN;
    if (_wcsicmp(str, L"RightClickUp") == 0) return RIGHTUP;
    if (_wcsicmp(str, L"RightDoubleClick") == 0) return RIGHTDOUBLE;

    if (_wcsicmp(str, L"X1ClickDown") == 0) return X1DOWN;
    if (_wcsicmp(str, L"X1ClickUp") == 0) return X1UP;
    if (_wcsicmp(str, L"X1DoubleClick") == 0) return X1DOUBLE;

    if (_wcsicmp(str, L"X2ClickDown") == 0) return X2DOWN;
    if (_wcsicmp(str, L"X2ClickUp") == 0) return X2UP;
    if (_wcsicmp(str, L"X2DoubleClick") == 0) return X2DOUBLE;

    return UNKNOWN;
}


/// <summary>
/// Gets the mod value from a string.
/// </summary>
WORD ClickHandler::ModsFromString(LPTSTR str) {
    WORD ret = 0x0000;

    LPTSTR context;
    LPTSTR tok = wcstok_s(str, L"+", &context);
    while (tok != nullptr) {
        if (_wcsicmp(tok, L"ctrl") == 0) ret |= MK_CONTROL;
        else if (_wcsicmp(tok, L"mouseleft") == 0) ret |= MK_LBUTTON;
        else if (_wcsicmp(tok, L"mousemiddle") == 0) ret |= MK_MBUTTON;
        else if (_wcsicmp(tok, L"mouseright") == 0) ret |= MK_RBUTTON;
        else if (_wcsicmp(tok, L"shift") == 0) ret |= MK_SHIFT;
        else if (_wcsicmp(tok, L"mousex1") == 0) ret |= MK_XBUTTON1;
        else if (_wcsicmp(tok, L"mousex2") == 0) ret |= MK_XBUTTON2;
        tok = wcstok_s(nullptr, L"+", &context);
    }

    return ret;
}

