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
#include "litestep.h"
#include "../utility/macros.h"
#include "../utility/core.hpp"
#include "../utility/logger.h"

#include <string>

// How long to wait for Explorer to start up as shell, in milliseconds
// Shouldn't use INFINITE since we may block forever in safe mode
#define EXPLORER_WAIT_TIMEOUT   20000

static std::wstring DescribeStartFlags(WORD flags)
{
    struct FlagEntry
    {
        WORD value;
        const wchar_t* name;
    };

    const FlagEntry entries[] =
    {
        { LSF_RUN_STARTUPAPPS,      L"RUN_STARTUPAPPS" },
        { LSF_FORCE_STARTUPAPPS,    L"FORCE_STARTUPAPPS" },
        { LSF_ALTERNATE_CONFIG,     L"ALTERNATE_CONFIG" },
        { LSF_RUN_LITESTEP,         L"RUN_LITESTEP" },
        { LSF_RUN_EXPLORER,         L"RUN_EXPLORER" },
        { LSF_CLOSE_EXPLORER,       L"CLOSE_EXPLORER" },
        { LSF_OVERLAY_MODE,         L"OVERLAY_MODE" }
    };

    std::wstring description;

    for (const auto& entry : entries)
    {
        if ((flags & entry.value) != 0)
        {
            if (!description.empty())
            {
                description.append(L", ");
            }

            description.append(entry.name);
        }
    }

    if (description.empty())
    {
        description.assign(L"none");
    }

    return description;
}

static std::wstring ToWideString(LPCTSTR value)
{
#ifdef UNICODE
    return value ? std::wstring(value) : std::wstring();
#else
    if (value == nullptr)
    {
        return std::wstring();
    }

    int required = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
    if (required <= 0)
    {
        return std::wstring();
    }

    std::wstring result;
    result.resize(static_cast<size_t>(required - 1));

    if (!result.empty())
    {
        MultiByteToWideChar(CP_ACP, 0, value, -1, result.data(), required);
    }
    else
    {
        wchar_t terminator = L'\0';
        MultiByteToWideChar(CP_ACP, 0, value, -1, &terminator, 1);
    }

    return result;
#endif
}




//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// IsOtherInstanceRunning
// Checks if another LiteStep instance is running already
//
bool IsOtherInstanceRunning(LPHANDLE phMutex)
{
    ASSERT(phMutex);
    bool bIsOther = false;

    *phMutex = CreateMutex(NULL, FALSE, _T("LiteStep"));

    if (*phMutex && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        bIsOther = true;
    }

    return bIsOther;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// ParseCommandLine
// Converts command line parameters to start flags
//
WORD ParseCommandLine(LPCTSTR pszCommandLine, LPTSTR pszFile, DWORD cchFile)
{
    ASSERT(pszCommandLine);

    const std::wstring commandLineText = ToWideString(pszCommandLine);
    Logger::Log(L"ParseCommandLine input: %ls", commandLineText.c_str());

    WORD wStartFlags = LSF_RUN_LITESTEP | LSF_RUN_STARTUPAPPS;

    TCHAR szToken[MAX_LINE_LENGTH] = { 0 };
    LPCTSTR pszNextToken = pszCommandLine;

    while (GetTokenW(pszNextToken, szToken, &pszNextToken, FALSE))
    {
        const std::wstring tokenText = ToWideString(szToken);
        if (szToken[0] == '-')
        {
            if (!_tcsicmp(szToken, _T("-nostartup")))
            {
                Logger::Log(L"Switch detected: -nostartup");
                wStartFlags &= ~LSF_RUN_STARTUPAPPS;
            }
            else if (!_tcsicmp(szToken, _T("-startup")))
            {
                Logger::Log(L"Switch detected: -startup");
                wStartFlags |= LSF_FORCE_STARTUPAPPS;
            }
            else if (!_tcsicmp(szToken, _T("-explorer")))
            {
                Logger::Log(L"Switch detected: -explorer");
                wStartFlags &= ~(LSF_RUN_LITESTEP | LSF_CLOSE_EXPLORER);
                wStartFlags |= LSF_RUN_EXPLORER;
            }
            else if (!_tcsicmp(szToken, _T("-closeexplorer")))
            {
                Logger::Log(L"Switch detected: -closeexplorer");
                wStartFlags &= ~LSF_RUN_EXPLORER;
                wStartFlags |= LSF_CLOSE_EXPLORER;
            }
            else if (!_tcsicmp(szToken, _T("-overlay")))
            {
                Logger::Log(L"Switch detected: -overlay");
                wStartFlags |= LSF_OVERLAY_MODE;
                wStartFlags &= ~LSF_CLOSE_EXPLORER;
            }
            else if (!_tcsicmp(szToken, _T("-nolite")))
            {
                Logger::Log(L"Switch detected: -nolite (deprecated)");
            }
            else
            {
                Logger::Log(L"Unknown switch encountered: %ls", tokenText.c_str());
            }
        }
        else
        {
            ASSERT(szToken[0] != '!');
            DWORD dwCopied = GetFullPathName(szToken, cchFile, pszFile, NULL);

            if (dwCopied == 0 || dwCopied > cchFile)
            {
                Logger::Log(L"Failed to resolve alternate config path for token: %ls", tokenText.c_str());
                pszFile[0] = _T('\0');
            }
            else
            {
                const std::wstring altConfigPath = ToWideString(pszFile);
                Logger::Log(L"Alternate config specified: %ls", altConfigPath.c_str());
            }

            wStartFlags |= LSF_ALTERNATE_CONFIG;
        }
    }

    Logger::Log(L"ParseCommandLine resulting flags: 0x%04X (%ls)",
        wStartFlags, DescribeStartFlags(wStartFlags).c_str());

    return wStartFlags;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// SendCommandLineBang
// Sends given !bang to a running litestep.exe instance
//
bool SendCommandLineBang(LPCTSTR pszCommand, LPCTSTR pszArgs)
{
    ASSERT(pszCommand);
    bool bSuccess = false;

    HWND hWnd = FindWindow(szMainWindowClass, szMainWindowTitle);

    if (IsWindow(hWnd))
    {
        LMBANGCOMMAND bangCommand;
        bangCommand.cbSize = sizeof(LMBANGCOMMAND);
        bangCommand.hWnd = NULL;

        HRESULT hr = StringCchCopy(
            bangCommand.wzCommand, MAX_BANGCOMMAND, pszCommand);

        if (SUCCEEDED(hr))
        {
            if (pszArgs)
            {
                hr = StringCchCopy(bangCommand.wzArgs, MAX_BANGARGS, pszArgs);
            }
            else
            {
                bangCommand.wzArgs[0] = '\0';
            }
        }

        if (SUCCEEDED(hr))
        {
            // Since we're a new, different litestep.exe process here, give the
            // other, "real" instance the right to set the foreground window
            TryAllowSetForegroundWindow(hWnd);

            COPYDATASTRUCT cds = { 0 };

            cds.cbData = sizeof(LMBANGCOMMAND);
            cds.dwData = LM_BANGCOMMANDW;
            cds.lpData = &bangCommand;

            if (SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&cds))
            {
                bSuccess = true;
            }
        }
    }

    return bSuccess;
}

static bool ForceShutdownExistingInstance(DWORD timeoutMs)
{
    if (timeoutMs == 0)
    {
        timeoutMs = 1;
    }

    Logger::Log(L"ForceShutdownExistingInstance invoked (timeout=%u ms).", timeoutMs);

    SendCommandLineBang(_T("!ShutDown"), NULL);
    Logger::Log(L"Sent !ShutDown bang to existing LiteStep instance.");

    HWND hExisting = FindWindow(szMainWindowClass, szMainWindowTitle);
    HANDLE hExistingProcess = NULL;

    if (IsWindow(hExisting))
    {
        Logger::Log(L"Existing LiteStep main window detected; requesting graceful shutdown.");
        DWORD existingProcessId = 0;
        GetWindowThreadProcessId(hExisting, &existingProcessId);

        if (existingProcessId != 0)
        {
            Logger::Log(L"Existing LiteStep process id=%u.", existingProcessId);
            hExistingProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, existingProcessId);

            if (!hExistingProcess)
            {
                Logger::Log(L"OpenProcess failed for existing LiteStep process (error=%u).", GetLastError());
            }
        }
        else
        {
            Logger::Log(L"Unable to resolve process id for existing LiteStep window.");
        }

        SendMessageTimeout(hExisting, WM_SYSCOMMAND, SC_CLOSE, 0, SMTO_ABORTIFHUNG, 2000, NULL);

        if (hExistingProcess)
        {
            DWORD waitSlice = timeoutMs / 2;
            if (waitSlice == 0)
            {
                waitSlice = timeoutMs;
            }

            const DWORD waitResult = WaitForSingleObject(hExistingProcess, waitSlice);
            if (waitResult != WAIT_OBJECT_0)
            {
                Logger::Log(L"Existing LiteStep process did not exit within %u ms, forcing termination.", waitSlice);
                TerminateProcess(hExistingProcess, 0);
                WaitForSingleObject(hExistingProcess, waitSlice);
            }
            else
            {
                Logger::Log(L"Existing LiteStep process exited gracefully within %u ms.", waitSlice);
            }
        }
        else
        {
            Logger::Log(L"No process handle available; relying on window polling.");
        }
    }
    else
    {
        Logger::Log(L"No LiteStep main window detected after shutdown request.");
    }

    if (hExistingProcess)
    {
        CloseHandle(hExistingProcess);
        hExistingProcess = NULL;
    }

    const DWORD startTick = GetTickCount();
    const DWORD deadline = startTick + timeoutMs;

    while (GetTickCount() < deadline)
    {
        if (FindWindow(szMainWindowClass, szMainWindowTitle) == NULL)
        {
            const DWORD elapsed = GetTickCount() - startTick;
            Logger::Log(L"Existing LiteStep instance terminated after %u ms.", elapsed);
            return true;
        }

        Sleep(100);
    }

    const bool closed = (FindWindow(szMainWindowClass, szMainWindowTitle) == NULL);
    if (!closed)
    {
        Logger::Log(L"Existing LiteStep instance still running after %u ms.", timeoutMs);
    }
    else
    {
        Logger::Log(L"Existing LiteStep instance closed during final check.");
    }

    return closed;
}
    //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// HandleCommandLineBang
// Grabs !bang command and any arguments from a command line and
// sends them to a running litestep.exe instance
//
int HandleCommandLineBang(LPCTSTR pszCommandLine)
{
    ASSERT(pszCommandLine);
    int nReturn = -1;

    // Can't just use MAX_BANGCOMMAND + MAX_BANGARGS since
    // there may be lots of whitespace on the command line
    TCHAR szBuffer[MAX_LINE_LENGTH] = { 0 };

    if (SUCCEEDED(StringCchCopy(szBuffer, COUNTOF(szBuffer), pszCommandLine)))
    {
        LPCTSTR pszArgs = NULL;
        LPTSTR pszBangEnd = _tcschr(szBuffer, _T(' '));

        if (pszBangEnd)
        {
            pszArgs = pszBangEnd + _tcsspn(pszBangEnd, _T(" "));

            // Cut off !bang arguments in szBuffer
            *pszBangEnd = _T('\0');
        }

        if (SendCommandLineBang(szBuffer, pszArgs))
        {
            nReturn = 0;
        }
    }

    return nReturn;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// StartExplorerShell
// Try to start Explorer in shell mode
//
bool StartExplorerShell(DWORD dwWaitTimeout)
{
    bool bStarted = false;
    TCHAR szOldShell[MAX_PATH] = { 0 };

    DWORD dwCopied = GetPrivateProfileString(_T("boot"), _T("shell"), NULL,
        szOldShell, COUNTOF(szOldShell), _T("system.ini"));

    // If this user account has limited access rights and
    // the shell is in HKLM, GetPrivateProfileString returns 0
    if (dwCopied > 0 && dwCopied < (COUNTOF(szOldShell)-1))
    {
        if (WritePrivateProfileString(
            _T("boot"), _T("shell"), _T("explorer.exe"), _T("system.ini")))
        {
            // We have successfully set Explorer as shell, now launch it...
            SHELLEXECUTEINFO sei = { 0 };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = _T("open");
            sei.lpFile = _T("%windir%\\explorer.exe");

            if (LSShellExecuteEx(&sei))
            {
                // If we don't wait here, there'll be a race condition:
                // We may reset the 'shell' setting before Explorer reads it.
                if (WaitForInputIdle(sei.hProcess, dwWaitTimeout) == 0)
                {
                    bStarted = true;
                }

                CloseHandle(sei.hProcess);
            }

            WritePrivateProfileString(
                _T("boot"), _T("shell"), szOldShell, _T("system.ini"));
        }
    }

    return bStarted;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// _tWinMain
// Main entry point. Chooses appropriate startup mode.
//
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR lpCmdLine, int)
{
    wchar_t szAppPath[MAX_PATH] = { 0 };
    std::wstring logBasePath;

    if (GetModuleFileNameW(nullptr, szAppPath, COUNTOF(szAppPath)) > 0)
    {
        PathRemoveFileSpecW(szAppPath);
        logBasePath.assign(szAppPath);
    }

    Logger::Initialize(logBasePath);

    LPCTSTR effectiveCmdLine = (lpCmdLine != nullptr) ? lpCmdLine : _T("");
    const std::wstring commandLineLogText = ToWideString(effectiveCmdLine);
    Logger::Log(L"_tWinMain starting. Command line=\"%ls\"", commandLineLogText.c_str());

    int nReturn = 0;

    if (lpCmdLine != nullptr && lpCmdLine[0] == _T('!'))
    {
        Logger::Log(L"Handling command-line bang request.");
        nReturn = HandleCommandLineBang(lpCmdLine);
        Logger::Log(L"Bang handling complete. Return code=%d", nReturn);
        Logger::Shutdown();
        return nReturn;
    }

    TCHAR szAltConfigFile[MAX_PATH] = { 0 };

    WORD wStartFlags = ParseCommandLine(
        effectiveCmdLine, szAltConfigFile, COUNTOF(szAltConfigFile));

    Logger::Log(L"Initial start flags: 0x%04X (%ls)",
        wStartFlags, DescribeStartFlags(wStartFlags).c_str());

    if (szAltConfigFile[0] != _T('\0'))
    {
        Logger::Log(L"Alternate config file requested: %ls", ToWideString(szAltConfigFile).c_str());
    }

    if (GetSystemMetrics(SM_CLEANBOOT))
    {
        Logger::Log(L"Safe mode detected. Forcing Explorer shell and skipping startup apps.");
        wStartFlags |= LSF_RUN_EXPLORER;
        wStartFlags &= ~LSF_RUN_STARTUPAPPS;
    }

    do
    {
        if (wStartFlags & LSF_RUN_EXPLORER)
        {
            Logger::Log(L"Attempting to start Explorer as shell.");

            if (StartExplorerShell(EXPLORER_WAIT_TIMEOUT))
            {
                Logger::Log(L"Explorer shell started successfully. Disabling LiteStep run.");
                wStartFlags &= ~LSF_RUN_LITESTEP;
            }
            else
            {
                Logger::Log(L"Explorer shell failed to start within timeout.");
                wStartFlags &= ~LSF_RUN_EXPLORER;
            }
        }

        if (wStartFlags & LSF_RUN_LITESTEP)
        {
            Logger::Log(L"Preparing LiteStep launch (flags=0x%04X).", wStartFlags);

            HANDLE hMutex = NULL;
            bool allowLiteStep = true;

            if (IsOtherInstanceRunning(&hMutex))
            {
                Logger::Log(L"Another LiteStep instance detected. Initiating shutdown.");

                if (hMutex)
                {
                    CloseHandle(hMutex);
                    hMutex = NULL;
                }

                const DWORD existingInstanceTimeout = 15000;

                if (!ForceShutdownExistingInstance(existingInstanceTimeout))
                {
                    Logger::Log(L"Failed to shut down existing LiteStep within %u ms.", existingInstanceTimeout);
                    MessageBox(NULL,
                        L"LiteStep could not close the previously running instance.",
                        L"LiteStep",
                        MB_ICONERROR | MB_OK);
                    allowLiteStep = false;
                    nReturn = LRV_NO_STEP;
                }
                else
                {
                    Logger::Log(L"Waiting for LiteStep mutex ownership after shutdown request.");

                    const DWORD waitDeadline = GetTickCount() + existingInstanceTimeout;
                    bool obtainedMutex = false;

                    while (GetTickCount() < waitDeadline)
                    {
                        HANDLE hRetry = NULL;

                        if (!IsOtherInstanceRunning(&hRetry))
                        {
                            hMutex = hRetry;
                            obtainedMutex = true;
                            break;
                        }

                        if (hRetry)
                        {
                            CloseHandle(hRetry);
                            hRetry = NULL;
                        }

                        Sleep(100);
                    }

                    if (!obtainedMutex)
                    {
                        Logger::Log(L"Timed out waiting for LiteStep mutex after shutdown sequence.");
                        MessageBox(NULL,
                            L"LiteStep could not take ownership after closing the previous instance.",
                            L"LiteStep",
                            MB_ICONERROR | MB_OK);
                        allowLiteStep = false;
                        nReturn = LRV_NO_STEP;
                    }
                    else
                    {
                        Logger::Log(L"LiteStep mutex acquired after shutting down previous instance.");
                    }
                }
            }

            if (allowLiteStep && (wStartFlags & LSF_RUN_LITESTEP))
            {
                Logger::Log(L"Invoking StartLitestep.");
                nReturn = StartLitestep(hInst, wStartFlags, szAltConfigFile);
                Logger::Log(L"StartLitestep returned %d.", nReturn);
            }

            if (hMutex)
            {
                CloseHandle(hMutex);
                hMutex = NULL;
                Logger::Log(L"Released LiteStep mutex handle.");
            }

            if (!allowLiteStep)
            {
                Logger::Log(L"LiteStep launch aborted due to existing instance conflict.");
                wStartFlags &= ~LSF_RUN_LITESTEP;
            }
            else if (nReturn == LRV_EXPLORER_START)
            {
                Logger::Log(L"LiteStep requested Explorer start; scheduling Explorer launch.");
                wStartFlags |= LSF_RUN_EXPLORER;
            }
        }
    } while (nReturn == LRV_EXPLORER_START && (wStartFlags & LSF_RUN_LITESTEP));

    Logger::Log(L"LiteStep shutting down with return code %d.", nReturn);
    Logger::Shutdown();
    return nReturn;
}





