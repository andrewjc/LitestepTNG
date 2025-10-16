//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// This is a part of the Litestep Shell source code.
//
// Copyright (C) 1997-2015  LiteStep Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include "shellhlp.h"
#include "core.hpp"
#include <MMSystem.h>



bool LSIsRunningOn64BitWindows()
{
#if defined(_WIN64)
    return true;
#else
    const HMODULE hKernel32 = GetModuleHandleW(L"KERNEL32.DLL");

    if (!hKernel32)
    {
        return false;
    }

    using IsWow64Process2Fn = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);

    if (const auto fnIsWow64Process2 =
        reinterpret_cast<IsWow64Process2Fn>(
            GetProcAddress(hKernel32, "IsWow64Process2")))
    {
        USHORT processMachine = 0;
        USHORT nativeMachine = 0;

        if (fnIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))
        {
            return processMachine != IMAGE_FILE_MACHINE_UNKNOWN;
        }

        return false;
    }

    using IsWow64ProcessFn = BOOL (WINAPI*)(HANDLE, PBOOL);

    if (const auto fnIsWow64Process =
        reinterpret_cast<IsWow64ProcessFn>(
            GetProcAddress(hKernel32, "IsWow64Process")))
    {
        BOOL isWow64 = FALSE;

        if (fnIsWow64Process(GetCurrentProcess(), &isWow64))
        {
            return isWow64 == TRUE;
        }
    }

    return false;
#endif
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Constants for older PSDKs
//
#if !defined(VER_SUITE_WH_SERVER)
#define VER_SUITE_WH_SERVER 0x00008000
#endif


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSSetEnvironmentStrings
// Wrapper around kernel32's SetEnvironmentStringsW.
//
BOOL LSSetEnvironmentStrings(LPWCH pszStrings)
{
    static auto fnSetEnvironmentStringsW = (BOOL(WINAPI*)(LPWCH))GetProcAddress(
        GetModuleHandle(L"KERNEL32.DLL"), "SetEnvironmentStringsW");
    if (fnSetEnvironmentStringsW)
    {
        return fnSetEnvironmentStringsW(pszStrings);
    }
    return FALSE;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSGetKnownFolderIDList
// Wrapper around SHGetKnownFolderIDList. Helper for GetShellFolderPath.
//
HRESULT LSGetKnownFolderIDList(REFKNOWNFOLDERID rfid, PIDLIST_ABSOLUTE* ppidl)
{
    HRESULT hr = E_FAIL;

    HMODULE hShell32 = GetModuleHandle(_T("SHELL32.DLL"));

    typedef HRESULT (WINAPI* SHGetKnownFolderIDListProc)(
        REFKNOWNFOLDERID, DWORD, HANDLE, PIDLIST_ABSOLUTE*);

    SHGetKnownFolderIDListProc fnSHGetKnownFolderIDList =
        (SHGetKnownFolderIDListProc)GetProcAddress(
        hShell32, "SHGetKnownFolderIDList");

    if (fnSHGetKnownFolderIDList)
    {
        hr = fnSHGetKnownFolderIDList(rfid, 0, NULL, ppidl);
    }
    else
    {
        hr = E_NOTIMPL;
    }

    return hr;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// GetShellFolderPath
//
// Wrapper around SHGetSpecialFolderLocation and Known Folder APIs.
// Provides access to user shell folders and the QuickLaunch folder.
bool GetShellFolderPath(int nFolder, LPTSTR ptzPath, size_t cchPath)
{
    ASSERT(cchPath >= MAX_PATH);
    ASSERT(ptzPath != nullptr);

    HRESULT hr = E_FAIL;
    PIDLIST_ABSOLUTE pidl = nullptr;

    if (nFolder == LS_CSIDL_QUICKLAUNCH)
    {
        hr = LSGetKnownFolderIDList(FOLDERID_QuickLaunch, &pidl);
    }
    else if (nFolder == CSIDL_ALTSTARTUP || nFolder == CSIDL_COMMON_ALTSTARTUP)
    {
        // Legacy alternate startup folders are not available on modern Windows.
        ptzPath[0] = L'\0';
        return false;
    }
    else
    {
        hr = SHGetSpecialFolderLocation(nullptr, nFolder, &pidl);
    }

    if (pidl != nullptr)
    {
        if (SUCCEEDED(hr) && !SHGetPathFromIDList(pidl, ptzPath))
        {
            hr = E_FAIL;
        }
        CoTaskMemFree(pidl);
    }

    if (FAILED(hr))
    {
        ptzPath[0] = L'\0';
    }

    return SUCCEEDED(hr);
}



//
// PathAddBackslashEx
//
// Checked version of PathAddBackslash which also handles quoted paths
//
// Return values:  S_OK          - backslash appended
//                 S_FALSE       - path already ended with a backslash
//                 E_OUTOFMEMORY - buffer too small
//                 E_FAIL        - other failure (invalid input string)
//
HRESULT PathAddBackslashEx(LPTSTR ptzPath, size_t cchPath)
{
    ASSERT(cchPath <= STRSAFE_MAX_CCH);
    ASSERT(NULL != ptzPath); ASSERT(0 != cchPath);

    HRESULT hr = E_FAIL;
    size_t cchCurrentLength = 0;

    if (SUCCEEDED(StringCchLength(ptzPath, cchPath, &cchCurrentLength)))
    {
        bool bHasQuote = false;
        LPTSTR ptzEnd = ptzPath + cchCurrentLength;

        if ((ptzEnd > ptzPath) && (*(ptzEnd-1) == _T('\"')))
        {
            --ptzEnd;
            bHasQuote = true;
        }

        if (ptzEnd > ptzPath)
        {
            if (*(ptzEnd-1) != _T('\\'))
            {
                if (cchPath - cchCurrentLength > 1)
                {
                    if (bHasQuote)
                    {
                        *(ptzEnd+1) = *ptzEnd;
                    }

                    *ptzEnd = _T('\\');

                    if (bHasQuote)
                    {
                        ++ptzEnd;
                    }

                    ASSERT((size_t)(ptzEnd - ptzPath) < cchPath);
                    *(ptzEnd+1) = _T('\0');

                    hr = S_OK;
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
            }
            else
            {
                hr = S_FALSE;
            }
        }
    }

    return hr;
}


//
// PathAddBackslashEx
//
// Checked version of PathAddBackslash which also handles quoted paths
//
// Return values:  S_OK          - backslash appended
//                 S_FALSE       - path already ended with a backslash
//                 E_OUTOFMEMORY - buffer too small
//                 E_FAIL        - other failure (invalid input string)
//
HRESULT PathAddBackslashExA(LPSTR pszPath, size_t cchPath)
{
    ASSERT(cchPath <= STRSAFE_MAX_CCH);
    ASSERT(nullptr != pszPath); ASSERT(0 != cchPath);

    HRESULT hr = E_FAIL;
    size_t cchCurrentLength = 0;

    if (SUCCEEDED(StringCchLengthA(pszPath, cchPath, &cchCurrentLength)))
    {
        bool bHasQuote = false;
        LPSTR ptzEnd = pszPath + cchCurrentLength;

        if ((ptzEnd > pszPath) && (*(ptzEnd-1) == _T('\"')))
        {
            --ptzEnd;
            bHasQuote = true;
        }

        if (ptzEnd > pszPath)
        {
            if (*(ptzEnd-1) != _T('\\'))
            {
                if (cchPath - cchCurrentLength > 1)
                {
                    if (bHasQuote)
                    {
                        *(ptzEnd+1) = *ptzEnd;
                    }

                    *ptzEnd = _T('\\');

                    if (bHasQuote)
                    {
                        ++ptzEnd;
                    }

                    ASSERT((size_t)(ptzEnd - pszPath) < cchPath);
                    *(ptzEnd+1) = _T('\0');

                    hr = S_OK;
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
            }
            else
            {
                hr = S_FALSE;
            }
        }
    }

    return hr;
}


//
// GetSystemString
//
bool GetSystemString(DWORD dwCode, LPTSTR ptzBuffer, DWORD cchBuffer)
{
    ASSERT(NULL != ptzBuffer); ASSERT(0 != cchBuffer);

    return (0 != FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwCode,
        0,
        ptzBuffer,
        cchBuffer,
        NULL
    ));
}


//
// CLSIDToString
// Mostly for debugging purposes (TRACE et al)
//
HRESULT CLSIDToString(REFCLSID rclsid, LPTSTR ptzBuffer, size_t cchBuffer)
{
    ASSERT(NULL != ptzBuffer); ASSERT(0 != cchBuffer);

    LPOLESTR pOleString = NULL;

    HRESULT hr = ProgIDFromCLSID(rclsid, &pOleString);

    if (FAILED(hr))
    {
        hr = StringFromCLSID(rclsid, &pOleString);
    }

    if (SUCCEEDED(hr) && pOleString)
    {
#if defined(UNICODE)
        hr = StringCchCopy(ptzBuffer, cchBuffer, pOleString);
#else // UNICODE
        int nReturn = WideCharToMultiByte(CP_ACP, 0, pOleString,
            (int)wcslen(pOleString), ptzBuffer, (int)cchBuffer, NULL, NULL);

        if (nReturn == 0)
        {
            hr = HrGetLastError();
        }
#endif
    }

    CoTaskMemFree(pOleString);

    return hr;
}


//
// Attempts to parse a string into a GUID.
// Mostly for debugging purposes.
//
BOOL LSGUIDFromString(LPCTSTR guidString, LPGUID guid)
{
    typedef BOOL (WINAPI* PROCTYPE)(LPCTSTR, LPGUID);
    static PROCTYPE proc = nullptr;

    if (proc == nullptr)
    {
        proc = (PROCTYPE)GetProcAddress(GetModuleHandle(_T("Shell32.dll")),
#if defined(_UNICODE)
            (LPCSTR)704
#else
            (LPCSTR)703
#endif
        );
    }

    if (proc)
    {
        return proc(guidString, guid);
    }

    return FALSE;
}



//
// LSGetModuleFileName
//
// Wrapper around GetModuleFileName that takes care of truncated buffers. If
// people are interested in the number of bytes written we could add another
// parameter (DWORD* pcchWritten)
//
bool LSGetModuleFileName(HINSTANCE hInst, LPTSTR pszBuffer, DWORD cchBuffer)
{
    bool bSuccess = false;

    DWORD cchCopied = GetModuleFileName(hInst, pszBuffer, cchBuffer);

    if (cchCopied == cchBuffer)
    {
        ASSERT(GetLastError() == ERROR_INSUFFICIENT_BUFFER);

        // GetModuleFileName doesn't null-terminate the buffer if it is too
        // small. Make sure that even in this error case the buffer is properly
        // terminated - some people don't check return values.
        pszBuffer[cchBuffer-1] = L'\0';
    }
    else if (cchCopied > 0 && cchCopied < cchBuffer)
    {
        bSuccess = true;
    }

    return bSuccess;
}


//
// LSGetModuleFileNameEx
//
// Wrapper around GetModuleFileNameEx that resolves the kernel32 export at runtime.
//
DWORD LSGetModuleFileNameEx(HANDLE hProcess, HMODULE hModule, LPTSTR pszBuffer, DWORD cchBuffer)
{
    typedef DWORD(WINAPI * GetModuleProc)(HANDLE hProcess, HMODULE hModule, LPTSTR pszBuffer, DWORD cchBuffer);
    static GetModuleProc proc = nullptr;

    if (proc == nullptr)
    {
        proc = (GetModuleProc)GetProcAddress(GetModuleHandle(_T("Kernel32.dll")), "K32GetModuleFileNameExW");
    }

    if (proc)
    {
        return proc(hProcess, hModule, pszBuffer, cchBuffer);
    }

    return 0;
}


//
// LSGetProcessImageFileName
//
// Wrapper around GetProcessImageFileName that resolves the kernel32 export at runtime.
//
DWORD LSGetProcessImageFileName(HANDLE hProcess, LPTSTR pszBuffer, DWORD cchBuffer)
{
    typedef DWORD(WINAPI * GetImageNameProc)(HANDLE hProcess,  LPTSTR pszBuffer, DWORD cchBuffer);
    static GetImageNameProc proc = nullptr;

    if (proc == nullptr)
    {
        proc = (GetImageNameProc)GetProcAddress(GetModuleHandle(_T("Kernel32.dll")), "K32GetProcessImageFileNameW");
    }

    if (proc)
    {
        return proc(hProcess, pszBuffer, cchBuffer);
    }

    return 0;
}


//
// TryAllowSetForegroundWindow
// Calls AllowSetForegroundWindow on platforms that support it
//
HRESULT TryAllowSetForegroundWindow(HWND hWnd)
{
    ASSERT(hWnd != NULL);
    HRESULT hr = E_FAIL;

    typedef BOOL (WINAPI* ASFWPROC)(DWORD);

    ASFWPROC pAllowSetForegroundWindow = (ASFWPROC)GetProcAddress(
        GetModuleHandle(_T("user32.dll")), "AllowSetForegroundWindow");

    if (pAllowSetForegroundWindow)
    {
        DWORD dwProcessId = 0;
        GetWindowThreadProcessId(hWnd, &dwProcessId);

        if (pAllowSetForegroundWindow(dwProcessId))
        {
            hr = S_OK;
        }
        else
        {
            hr = HrGetLastError();
        }
    }
    else
    {
        // this platform doesn't have ASFW (Win95, NT4), so the
        // target process is allowed to set the foreground window anyway
        hr = S_FALSE;
    }

    return hr;
}


//
// LSShutdownDialog
//
// LSShutdownDialog
//
void LSShutdownDialog(HWND hWnd)
{
    FARPROC fnProc = GetProcAddress(
        GetModuleHandleW(L"SHELL32.DLL"), (LPCSTR)((long)0x003C));

    if (fnProc)
    {
        typedef VOID (WINAPI* ExitWindowsDialogProc)(HWND, DWORD);
        ExitWindowsDialogProc fnExitWindowsDialog = (ExitWindowsDialogProc)fnProc;

        fnExitWindowsDialog(hWnd, 0);

        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            (GetAsyncKeyState(VK_MENU) & 0x8000))
        {
            PostQuitMessage(LRV_SHUTDOWN_DLG);
        }
    }
}



//
// LSPlaySystemSound
// Wrapper around PlaySound to play system event sounds
//
BOOL LSPlaySystemSound(LPCWSTR pwzSoundAlias)
{
    BOOL bResult = FALSE;

    // We want to avoid linking to winmm.dll as long as it's used just here
    HMODULE hWinMM = LoadLibrary(_T("winmm.dll"));

    if (hWinMM)
    {
        typedef BOOL (WINAPI* PlaySoundProc)(LPCWSTR, HMODULE, DWORD);

        PlaySoundProc fnPlaySound = (PlaySoundProc)
            GetProcAddress(hWinMM, "PlaySoundW");

                if (fnPlaySound)
        {
            // Use the system event sound routing and avoid fallback audio if the alias is unset.
            DWORD dwFlags = SND_ALIAS | SND_NODEFAULT | SND_SYSTEM | SND_ASYNC;
            bResult = fnPlaySound(pwzSoundAlias, NULL, dwFlags);
        }
        VERIFY(FreeLibrary(hWinMM));
    }

    return bResult;
}


//
// LS_THREAD_DATA
// for use in LSCreateThread/LSThreadThunk
//
struct LS_THREAD_DATA
{
    HANDLE hEvent;
    LPCSTR pszName;
    LPTHREAD_START_ROUTINE fnOrigFunc;
    LPVOID pOrigParam;
};


//
// LSThreadThunk
// for use LSCreateThread
//
DWORD WINAPI LSThreadThunk(LPVOID pParam)
{
    LS_THREAD_DATA* pData = (LS_THREAD_DATA*)pParam;
    ASSERT(pData != NULL);

    // create local copy
    LS_THREAD_DATA data = *pData;

    if (data.pszName)
    {
        DbgSetCurrentThreadName(data.pszName);
    }

    SetEvent(data.hEvent);
    return data.fnOrigFunc(data.pOrigParam);
}


//
// LSCreateThread
// The name param is intentionally CHAR as the debugger doesn't handle WCHAR
//
HANDLE LSCreateThread(LPCSTR pszName, LPTHREAD_START_ROUTINE fnStartAddres,
                      LPVOID lpParameter, LPDWORD pdwThreadId)
{
    DWORD dwDummy = 0;

    if (!pdwThreadId)
    {
        // On Win9x pdwThreadId must be valid
        pdwThreadId = &dwDummy;
    }

#if defined(MSVC_DEBUG)
    LS_THREAD_DATA data = { 0 };
    data.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    data.pszName = pszName;
    data.fnOrigFunc = fnStartAddres;
    data.pOrigParam = lpParameter;

    // Should be fine to assert this since this is debug-mode only code
    ASSERT(data.hEvent != NULL);

    HANDLE hThread = CreateThread(
        NULL, 0, LSThreadThunk, (LPVOID)&data, 0, pdwThreadId);

    if (hThread != NULL && data.hEvent)
    {
        WaitForSingleObject(data.hEvent, INFINITE);
    }

    return hThread;
#else
    return CreateThread(NULL, 0, fnStartAddres, lpParameter, 0, pdwThreadId);
    UNREFERENCED_PARAMETER(pszName);
#endif
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// GetWindowsVersion
//

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSDisableWow64FsRedirection
//
BOOL LSDisableWow64FsRedirection(PVOID* ppvOldValue)
{
#ifndef _WIN64
    typedef BOOL (WINAPI* Wow64DisableWow64FsRedirectionProc)(PVOID*);

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");

    const auto fnWow64DisableWow64FsRedirection =
        reinterpret_cast<Wow64DisableWow64FsRedirectionProc>(
            GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection"));

    if (fnWow64DisableWow64FsRedirection)
    {
        return fnWow64DisableWow64FsRedirection(ppvOldValue);
    }

    return TRUE;
#else
    UNREFERENCED_PARAMETER(ppvOldValue);
    return TRUE;
#endif
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSRevertWow64FsRedirection
//
BOOL LSRevertWow64FsRedirection(PVOID pvOldValue)
{
#ifndef _WIN64
    typedef BOOL (WINAPI* Wow64RevertWow64FsRedirectionProc)(PVOID);

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");

    const auto fnWow64RevertWow64FsRedirection =
        reinterpret_cast<Wow64RevertWow64FsRedirectionProc>(
            GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection"));

    if (fnWow64RevertWow64FsRedirection)
    {
        return fnWow64RevertWow64FsRedirection(pvOldValue);
    }

    return TRUE;
#else
    UNREFERENCED_PARAMETER(pvOldValue);
    return TRUE;
#endif
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSShellExecuteEx
//
BOOL LSShellExecuteEx(LPSHELLEXECUTEINFOW lpExecInfo)
{
    PVOID pvOldValue = nullptr;
    LSDisableWow64FsRedirection(&pvOldValue);

    BOOL bReturn = ShellExecuteExW(lpExecInfo);

    LSRevertWow64FsRedirection(pvOldValue);
    return bReturn;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSShellExecute
//
HINSTANCE LSShellExecute(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile,
                         LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShow)
{
    PVOID pvOldValue = nullptr;
    LSDisableWow64FsRedirection(&pvOldValue);

    HINSTANCE hinstResult = ShellExecuteW(
        hwnd, lpOperation, lpFile, lpParameters, lpDirectory, nShow);

    LSRevertWow64FsRedirection(pvOldValue);
    return hinstResult;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSActivateActCtxForDll
// Activates the custom activation context for the specified DLL
//
HANDLE LSActivateActCtxForDll(LPCTSTR pszDll, PULONG_PTR pulCookie)
{
    HANDLE hContext = INVALID_HANDLE_VALUE;

    typedef HANDLE (WINAPI* CreateActCtx_t)(PACTCTX pCtx);
    typedef BOOL (WINAPI* ActivateActCtx_t)(HANDLE hCtx, ULONG_PTR* pCookie);

#if defined(UNICODE)
    const auto fnCreateActCtx = reinterpret_cast<CreateActCtx_t>(
        GetProcAddress(GetModuleHandleW(L"KERNEL32"), "CreateActCtxW"));
#else
    const auto fnCreateActCtx = reinterpret_cast<CreateActCtx_t>(
        GetProcAddress(GetModuleHandleW(L"KERNEL32"), "CreateActCtxA"));
#endif

    const auto fnActivateActCtx = reinterpret_cast<ActivateActCtx_t>(
        GetProcAddress(GetModuleHandleW(L"KERNEL32"), "ActivateActCtx"));

    if (fnCreateActCtx != nullptr && fnActivateActCtx != nullptr)
    {
        ACTCTX act = { };
        act.cbSize = sizeof(act);
        act.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
        act.lpSource = pszDll;
        act.lpResourceName = MAKEINTRESOURCE(123);

        hContext = fnCreateActCtx(&act);

        if (hContext != INVALID_HANDLE_VALUE)
        {
            if (!fnActivateActCtx(hContext, pulCookie))
            {
                LSDeactivateActCtx(hContext, nullptr);
                hContext = INVALID_HANDLE_VALUE;
            }
        }
    }

    return hContext;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSActivateActCtxForClsid
// Activates the custom activation context for the specified CLSID
//
HANDLE LSActivateActCtxForClsid(REFCLSID rclsid, PULONG_PTR pulCookie)
{
    HANDLE hContext = INVALID_HANDLE_VALUE;
    TCHAR szCLSID[39] = { 0 };

    if (SUCCEEDED(CLSIDToString(rclsid, szCLSID, COUNTOF(szCLSID))))
    {
        TCHAR szSubkey[MAX_PATH] = { 0 };

        if (SUCCEEDED(StringCchPrintf(szSubkey, COUNTOF(szSubkey),
            _T("CLSID\\%ls\\InProcServer32"), szCLSID)))
        {
            TCHAR szDll[MAX_PATH] = { 0 };
            DWORD cbDll = sizeof(szDll);

            if (SHGetValue(HKEY_CLASSES_ROOT, szSubkey, nullptr, nullptr, szDll, &cbDll)
                == ERROR_SUCCESS)
            {
                hContext = LSActivateActCtxForDll(szDll, pulCookie);
            }
        }
    }

    return hContext;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSDeactivateActCtx
// Removes an activation context from the activation context stack
//
void LSDeactivateActCtx(HANDLE hActCtx, ULONG_PTR* pulCookie)
{
    typedef BOOL (WINAPI* DeactivateActCtx_t)(DWORD dwFlags, ULONG_PTR ulc);
    typedef void (WINAPI* ReleaseActCtx_t)(HANDLE hActCtx);

    const auto fnDeactivateActCtx = reinterpret_cast<DeactivateActCtx_t>(
        GetProcAddress(GetModuleHandleW(L"KERNEL32"), "DeactivateActCtx"));

    const auto fnReleaseActCtx = reinterpret_cast<ReleaseActCtx_t>(
        GetProcAddress(GetModuleHandleW(L"KERNEL32"), "ReleaseActCtx"));

    if (fnDeactivateActCtx != nullptr && fnReleaseActCtx != nullptr)
    {
        if (hActCtx != INVALID_HANDLE_VALUE)
        {
            if (pulCookie != nullptr)
            {
                fnDeactivateActCtx(0, *pulCookie);
            }

            fnReleaseActCtx(hActCtx);
        }
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// DescriptionFromHR
// Retrieves a description of a HRESULT error code.
//
HRESULT DescriptionFromHR(HRESULT hr, LPWSTR buf, size_t cchBuf)
{
    if (FACILITY_WINDOWS == HRESULT_FACILITY(hr))
    {
        hr = HRESULT_CODE(hr);
    }

    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, DWORD(cchBuf), nullptr) == 0)
    {
        return StringCchPrintfW(buf, cchBuf, L"Unknown error, 0x%.8X", hr);
    }

    return S_OK;
}
UINT GetWindowsVersion()
{
    using RtlGetVersionProc = LONG (WINAPI*)(LPOSVERSIONINFOW);

    const auto rtlGetVersion = reinterpret_cast<RtlGetVersionProc>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));

    OSVERSIONINFOEXW version = { };
    version.dwOSVersionInfoSize = sizeof(version);

    if (rtlGetVersion &&
        rtlGetVersion(reinterpret_cast<LPOSVERSIONINFOW>(&version)) == 0)
    {
        if (version.wProductType != VER_NT_WORKSTATION)
        {
            return WINVER_WINSERVER10;
        }
    }

    return WINVER_WIN10;
}

