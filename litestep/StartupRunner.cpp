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
#include "StartupRunner.h"
#include "../utility/core.hpp"
#include "../utility/logger.h"
#include <regstr.h>


#define ERK_NONE                0x0000
#define ERK_RUNSUBKEYS          0x0001 // runs key and its subkeys
#define ERK_DELETE              0x0002
#define ERK_WAITFOR_QUIT        0x0004 // wait until process exits
#define ERK_WAITFOR_IDLE        0x0008 // wait until process waits for input
#define ERK_WIN64_BOTH          0x0010 // run key from 32-bit and 64-bit branch

// Used internally by _RunRegKeys and _RunRegKeysWorker
#define ERK_WIN64_KEY32         0x0020 // run 32-bit key specifically
#define ERK_WIN64_KEY64         0x0040 // run 64-bit key specifically


StartupRunner::StartupRunner()
{
    // do nothing
}


StartupRunner::~StartupRunner()
{
    // do nothing
}


void StartupRunner::Run(BOOL bForce)
{
    Logger::Log(L"StartupRunner::Run invoked (force=%d).", static_cast<int>(bForce));

    HANDLE hThread = LSCreateThread("StartupRunner",
        StartupRunner::_ThreadProc, (LPVOID)(INT_PTR)bForce, NULL);

    if (hThread)
    {
        Logger::Log(L"StartupRunner worker thread created (handle=%p).", hThread);
        CloseHandle(hThread);
    }
    else
    {
        Logger::Log(L"StartupRunner worker thread creation failed (error=%u).", GetLastError());
    }
}

DWORD WINAPI StartupRunner::_ThreadProc(LPVOID lpData)
{
    bool bRunStartup = IsFirstRunThisSession(_T("StartupHasBeenRun"));
    BOOL bForceStartup = (lpData != 0);

    Logger::Log(L"StartupRunner::_ThreadProc started (force=%d, firstRun=%d).", static_cast<int>(bForceStartup), static_cast<int>(bRunStartup));

    // Maintain the session marker Explorer expects when running in modern Windows.
    IsFirstRunThisSession(_T("RunStuffHasBeenRun"));

    // by keeping the call to _IsFirstRunThisSession() above we make sure the
    // regkey is created even if we're in "force startup" mode
    if (bRunStartup || bForceStartup)
    {
        Logger::Log(L"StartupRunner executing startup sequence.");
        // Need to call CoInitializeEx for ShellExecuteEx
        VERIFY_HR(CoInitializeEx(
            NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));

        bool bHKLMRun = true;
        bool bHKCURun = true;
        bool bHKLMRunOnce = true;
        bool bHKCURunOnce = true;

        //
        // SHRestricted is not available on Windows 95
        //
        typedef DWORD (WINAPI* SHREST_PROC)(RESTRICTIONS);

        SHREST_PROC pSHRestricted = (SHREST_PROC)GetProcAddress(
            GetModuleHandle(_T("shell32.dll")), (LPCSTR)((long)0x0064));

        if (pSHRestricted)
        {
            bHKLMRun = !pSHRestricted(REST_NOLOCALMACHINERUN);
            bHKCURun = !pSHRestricted(REST_NOCURRENTUSERRUN);
            bHKLMRunOnce = !pSHRestricted(REST_NOLOCALMACHINERUNONCE);
            bHKCURunOnce = !pSHRestricted(REST_NOCURRENTUSERRUNONCE);
        }

        //
        // On Win64 there are separate 32-Bit and 64-Bit versions of
        // HKLM\Run, HKLM\RunOnce, and HKLM\RunOnceEx.
        // However, HKLM\...\Policies\Explorer\Run  is a shared key.
        //
        // There is only a single version of all HKCU keys.
        //
        if (bHKLMRunOnce)
        {
            Logger::Log(L"StartupRunner running HKLM\\RunOnce keys.");
            _RunRegKeys(HKEY_LOCAL_MACHINE, REGSTR_PATH_RUNONCE,
                (ERK_RUNSUBKEYS | ERK_DELETE |
                 ERK_WAITFOR_QUIT | ERK_WIN64_BOTH));
        }

        _RunRunOnceEx();

        if (bHKLMRun)
        {
            Logger::Log(L"StartupRunner running HKLM\\Run keys.");
            _RunRegKeys(HKEY_LOCAL_MACHINE, REGSTR_PATH_RUN, ERK_WIN64_BOTH);
        }

        Logger::Log(L"StartupRunner running HKLM policy Run keys.");
        _RunRegKeys(HKEY_LOCAL_MACHINE, REGSTR_PATH_RUN_POLICY, ERK_NONE);
        Logger::Log(L"StartupRunner running HKCU policy Run keys.");
        _RunRegKeys(HKEY_CURRENT_USER, REGSTR_PATH_RUN_POLICY, ERK_NONE);

        if (bHKCURun)
        {
            Logger::Log(L"StartupRunner running HKCU\\Run keys.");
            _RunRegKeys(HKEY_CURRENT_USER, REGSTR_PATH_RUN, ERK_NONE);
        }

        Logger::Log(L"StartupRunner running Startup menu entries.");
        _RunStartupMenu();

        if (bHKCURunOnce)
        {
            Logger::Log(L"StartupRunner running HKCU\\RunOnce keys.");
            _RunRegKeys(HKEY_CURRENT_USER, REGSTR_PATH_RUNONCE,
                (ERK_RUNSUBKEYS | ERK_DELETE));
        }

        CoUninitialize();
    }

    Logger::Log(L"StartupRunner::_ThreadProc exiting (return=%d).", static_cast<int>(bRunStartup));
    return bRunStartup;
}


void StartupRunner::_RunRunOnceEx()
{
    //
    // TODO: Figure out how this works on Win64
    //
    TCHAR szArgs[MAX_PATH] = { 0 };
    UINT uChars = GetSystemDirectory(szArgs, MAX_PATH);

    if (uChars > 0 && uChars < MAX_PATH)
    {
        if (SUCCEEDED(StringCchCat(szArgs, MAX_PATH, _T("\\iernonce.dll"))))
        {
            // The file doesn't exist on NT4
            if (PathFileExists(szArgs) && SUCCEEDED(StringCchCat(szArgs,
                MAX_PATH, _T(",RunOnceExProcess"))))
            {
                LSShellExecute(NULL,
                    _T("open"), _T("rundll32.exe"), szArgs, NULL, SW_NORMAL);
            }
        }
    }
}


void StartupRunner::_RunStartupMenu()
{
    _RunShellFolderContents(CSIDL_COMMON_STARTUP);
    _RunShellFolderContents(CSIDL_COMMON_ALTSTARTUP);

    _RunShellFolderContents(CSIDL_STARTUP);
    _RunShellFolderContents(CSIDL_ALTSTARTUP);
}


void StartupRunner::_RunShellFolderContents(int nFolder)
{
    TCHAR tzPath[MAX_PATH] = { 0 };

    if (GetShellFolderPath(nFolder, tzPath, COUNTOF(tzPath)))
    {
        if (tzPath[0])
        {
            TCHAR tzSearchPath[MAX_PATH] = { 0 };
            PathCombine(tzSearchPath, tzPath, _T("*.*"));

            WIN32_FIND_DATA findData = { 0 };
            HANDLE hSearch = FindFirstFile(tzSearchPath, &findData);

            while (hSearch != INVALID_HANDLE_VALUE)
            {
                if (!PathIsDirectory(findData.cFileName) &&
                    !(findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) &&
                    !(findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
                {
                    SHELLEXECUTEINFO seiCommand = { 0 };

                    seiCommand.cbSize = sizeof(SHELLEXECUTEINFO);
                    seiCommand.lpFile = findData.cFileName;
                    seiCommand.lpDirectory = tzPath;
                    seiCommand.nShow = SW_SHOWNORMAL;
                    seiCommand.fMask =
                        SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI;

                    if (!LSShellExecuteEx(&seiCommand))
                    {
                        TRACE("StartupRunner failed to launch '%ls'",
                            findData.cFileName);
                    }
                }

                if (!FindNextFile(hSearch, &findData))
                {
                    FindClose(hSearch);
                    hSearch = INVALID_HANDLE_VALUE;
                }
            }
        }
    }
    else
    {
        TRACE("Failed to get full path to Startup folder %d", nFolder);
    }
}


//
//
// _CreateSessionInfoKey
//
// Generates the SessionInfo key used to track first-run flags for the current logon session.
HKEY StartupRunner::_CreateSessionInfoKey()
{
    HKEY hkSessionInfo = NULL;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        DWORD dwSessionId = 0;
        DWORD dwOutSize = 0;
        TCHAR tzSessionInfo[128] = { 0 };

        if (GetTokenInformation(hToken, TokenSessionId,
            &dwSessionId, sizeof(dwSessionId), &dwOutSize))
        {
            if (SUCCEEDED(StringCchPrintf(tzSessionInfo, COUNTOF(tzSessionInfo),
                REGSTR_PATH_EXPLORER _T("\\\SessionInfo\\\%u"), dwSessionId)))
            {
                LONG lResult = RegCreateKeyEx(
                    HKEY_CURRENT_USER, tzSessionInfo, 0, NULL,
                    REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hkSessionInfo, NULL);

                if (lResult != ERROR_SUCCESS)
                {
                    hkSessionInfo = NULL;
                }
            }
        }

        CloseHandle(hToken);
    }

    return hkSessionInfo;
}

//
//
// IsFirstRunThisSession()
//
bool StartupRunner::IsFirstRunThisSession(LPCTSTR pszSubkey)
{
    HKEY hkSessionInfo = _CreateSessionInfoKey();

    if (hkSessionInfo == NULL)
    {
        return false;
    }

    bool bReturn = false;
    DWORD dwDisposition;
    HKEY hkStartup;

    LONG lResult = RegCreateKeyEx(
        hkSessionInfo, pszSubkey, 0, NULL, REG_OPTION_VOLATILE,
        KEY_WRITE, NULL, &hkStartup, &dwDisposition);

    if (lResult == ERROR_SUCCESS)
    {
        RegCloseKey(hkStartup);
        if (dwDisposition == REG_CREATED_NEW_KEY)
        {
            bReturn = true;
        }
    }

    RegCloseKey(hkSessionInfo);

    return bReturn;
}

//
// RunRegKeys
//
void StartupRunner::_RunRegKeys(HKEY hkParent, LPCTSTR ptzSubKey, DWORD dwFlags)
{
    if (LSIsRunningOn64BitWindows() && (dwFlags & ERK_WIN64_BOTH))
    {
        dwFlags &= ~ERK_WIN64_BOTH;
        _RunRegKeysWorker(hkParent, ptzSubKey, dwFlags | ERK_WIN64_KEY64);
        _RunRegKeysWorker(hkParent, ptzSubKey, dwFlags | ERK_WIN64_KEY32);
    }
    else
    {
        _RunRegKeysWorker(hkParent, ptzSubKey, dwFlags);
    }
}


//
// _RunRegKeysWorker
//
void StartupRunner::_RunRegKeysWorker(HKEY hkParent,
                                      LPCTSTR ptzSubKey, DWORD dwFlags)
{
    REGSAM samDesired = MAXIMUM_ALLOWED;

    if (dwFlags & ERK_WIN64_KEY32)
    {
        samDesired |= KEY_WOW64_32KEY;
    }

    if (dwFlags & ERK_WIN64_KEY64)
    {
        samDesired |= KEY_WOW64_64KEY;
    }

    HKEY hkey = NULL;

    LONG lResult = RegOpenKeyEx(hkParent, ptzSubKey, 0, samDesired, &hkey);

    if (lResult == ERROR_SUCCESS)
    {
        //
        // Run the key itself
        //
        for (DWORD dwLoop = 0; ; ++dwLoop)
        {
            TCHAR szName[MAX_PATH] = { 0 };
            TCHAR szValue[MAX_LINE_LENGTH] = { 0 };

            DWORD cchName = COUNTOF(szName);
            DWORD cbValue = sizeof(szValue);
            DWORD dwType;

            lResult = RegEnumValue(hkey, dwLoop, szName, &cchName,
                NULL, &dwType, (LPBYTE)szValue, &cbValue);

            if (lResult == ERROR_MORE_DATA)
            {
                // tzNameBuffer too small?
                continue;
            }
            else if (lResult == ERROR_SUCCESS)
            {
                if ((dwType == REG_SZ) || (dwType == REG_EXPAND_SZ))
                {
                    if (szValue[0])
                    {
                        _SpawnProcess(szValue, dwFlags);
                    }

                    if ((dwFlags & ERK_DELETE) && (szName[0] != _T('!')))
                    {
                        if (RegDeleteValue(hkey, szName) == ERROR_SUCCESS)
                        {
                            --dwLoop;
                        }
                    }
                }
            }
            else
            {
                break;
            }
        }

        //
        // Run subkeys as well?
        //
        if (dwFlags & ERK_RUNSUBKEYS)
        {
            dwFlags &= ~(ERK_RUNSUBKEYS);

            for (DWORD dwLoop = 0; ; ++dwLoop)
            {
                TCHAR szName[MAX_PATH] = { 0 };

                LONG lResult2 = RegEnumKey(
                    hkey, dwLoop, szName, COUNTOF(szName));

                if (lResult2 == ERROR_MORE_DATA)
                {
                    // szName too small?
                    continue;
                }
                else if (lResult2 == ERROR_SUCCESS)
                {
                    _RunRegKeys(hkey, szName, dwFlags);

                    if (dwFlags & ERK_DELETE)
                    {
                        if (RegDeleteKey(hkey, szName) == ERROR_SUCCESS)
                        {
                            --dwLoop;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
        }

        RegCloseKey(hkey);
    }
}


void StartupRunner::_SpawnProcess(LPTSTR ptzCommandLine, DWORD dwFlags)
{
    ASSERT(!(dwFlags & ERK_WAITFOR_QUIT && dwFlags & ERK_WAITFOR_IDLE));

    //
    // The following cases need to be supported:
    //
    // 1. "C:\Program Files\App\App.exe" -params
    // 2. C:\Program Files\App\App.exe -params
    // 3. App.exe -params  (App.exe is in %path% or HKLM->REGSTR_PATH_APPPATHS)
    // and all the above cases without arguments.
    //
    // Note that 'App.exe' may contain spaces too
    //
    // CreateProcess handles 1 and 2, ShellExecuteEx handles 1 and 3.
    //
    // ShellExecuteEx works with UAC. CreateProcess does not.
    // Therefore, we have to attempt to emulate CreateProcess's path parsing
    // when we detect case 2.
    //
    TCHAR tzToken[MAX_LINE_LENGTH] = { 0 };
    LPTSTR ptzArgs = nullptr;

    GetTokenW(ptzCommandLine, tzToken, const_cast<LPCTSTR*>(&ptzArgs), FALSE);

    HANDLE hProcess = nullptr;

    // If the first character is a quote, assume that the first token is the complete path.
    // If the first token does not contain a \ or the first token does not contain a :, assume it's a relative path.
    if (*_tcsspnp(ptzCommandLine, _T(" \t")) == _T('"') || !_tcschr(tzToken, _T('\\')) || !_tcschr(tzToken, _T(':')))
    {
        hProcess = _ShellExecuteEx(tzToken, ptzArgs);
    }
    else
    {
        // This is an approximation of how CreateProcess determines which file to launch.
        ptzArgs = ptzCommandLine;
        do
        {
            ptzArgs = _tcschr(ptzArgs, _T(' '));
            if (ptzArgs != nullptr)
            {
                *ptzArgs++ = _T('\0');
            }
            if (PathFileExists(ptzCommandLine) && PathIsDirectory(ptzCommandLine) == FALSE)
            {
                hProcess = _ShellExecuteEx(ptzCommandLine, ptzArgs);
                break;
            }
            if (ptzArgs != nullptr)
            {
                *(ptzArgs-1) = _T(' ');
            }

        } while (ptzArgs != nullptr);
    }

    if (hProcess != nullptr)
    {
        if (dwFlags & ERK_WAITFOR_QUIT)
        {
            WaitForSingleObject(hProcess, INFINITE);
        }
        else if (dwFlags & ERK_WAITFOR_IDLE)
        {
            WaitForInputIdle(hProcess, INFINITE);
        }

        CloseHandle(hProcess);
    }
#ifdef _DEBUG
    else
    {
        TCHAR tzError[4096];
        DescriptionFromHR(HrGetLastError(), tzError, _countof(tzError));
        TRACE("StartupRunner failed to launch '%ls', %ls", ptzCommandLine, tzError);
    }
#endif
}


HANDLE StartupRunner::_ShellExecuteEx(LPCTSTR ptzExecutable, LPCTSTR ptzArgs)
{
    HANDLE hReturn = NULL;

    SHELLEXECUTEINFO sei = { 0 };
    sei.cbSize = sizeof(sei);
    sei.lpFile = ptzExecutable;
    sei.lpParameters = ptzArgs;
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = \
        SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;

    if (LSShellExecuteEx(&sei))
    {
        hReturn = sei.hProcess;
    }

    return hReturn;
}
