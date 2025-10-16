/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  ErrorHandler.cpp
 *  The nModules Project
 *
 *  Functions for dealing with errors.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "LiteStep.h"
#include "ErrorHandler.h"
#include "../Utilities/Error.h"

#include <strsafe.h>
#include <string>

// The current global error level.
static ErrorHandler::Level gErrorLevel = ErrorHandler::Level::Warning;
static wchar_t gModuleName[64] = L"";
static SRWLOCK gLogLock = SRWLOCK_INIT;

static const wchar_t* LevelToCaption(ErrorHandler::Level level)
{
    switch (level)
    {
    case ErrorHandler::Level::Critical:
        return L"Critical";
    case ErrorHandler::Level::Warning:
        return L"Warning";
    case ErrorHandler::Level::Notice:
        return L"Notice";
    case ErrorHandler::Level::Debug:
        return L"Debug";
    default:
        return L"LiteStep";
    }
}

static const wchar_t* LevelToTag(ErrorHandler::Level level)
{
    switch (level)
    {
    case ErrorHandler::Level::Critical:
        return L"CRITICAL";
    case ErrorHandler::Level::Warning:
        return L"WARNING";
    case ErrorHandler::Level::Notice:
        return L"NOTICE";
    case ErrorHandler::Level::Debug:
        return L"DEBUG";
    default:
        return L"INFO";
    }
}

static UINT LevelToIcon(ErrorHandler::Level level)
{
    switch (level)
    {
    case ErrorHandler::Level::Critical:
        return MB_ICONERROR;
    case ErrorHandler::Level::Warning:
        return MB_ICONWARNING;
    case ErrorHandler::Level::Notice:
        return MB_ICONINFORMATION;
    case ErrorHandler::Level::Debug:
        return MB_ICONINFORMATION;
    default:
        return 0;
    }
}

static void LogErrorMessage(ErrorHandler::Level level, LPCWSTR message)
{
    if (message == nullptr || *message == L'\0')
    {
        return;
    }

    wchar_t basePath[MAX_PATH];
    if (!LiteStep::LSGetLitestepPathW(basePath, _countof(basePath)))
    {
        return;
    }

    wchar_t logPath[MAX_PATH];
    StringCchCopy(logPath, _countof(logPath), basePath);
    StringCchCat(logPath, _countof(logPath), L"logs\\litestep.log");

    wchar_t directory[MAX_PATH];
    StringCchCopy(directory, _countof(directory), logPath);
    wchar_t* lastSlash = wcsrchr(directory, L'\\');
    if (lastSlash != nullptr)
    {
        *lastSlash = L'\0';
        CreateDirectoryW(directory, nullptr);
    }

    AcquireSRWLockExclusive(&gLogLock);
    HANDLE hFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        ReleaseSRWLockExclusive(&gLogLock);
        return;
    }

    SYSTEMTIME st = { 0 };
    GetLocalTime(&st);

    wchar_t line[MAX_LINE_LENGTH + 256];
    if (*gModuleName != L'\0')
    {
        StringCchPrintf(line, _countof(line),
            L"[%02u-%02u-%04u %02u:%02u:%02u.%03u] [%s] [%s] %s\r\n",
            st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            LevelToTag(level), gModuleName, message);
    }
    else
    {
        StringCchPrintf(line, _countof(line),
            L"[%02u-%02u-%04u %02u:%02u:%02u.%03u] [%s] %s\r\n",
            st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            LevelToTag(level), message);
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (required > 1)
    {
        std::string utf8(static_cast<size_t>(required - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8.empty() ? nullptr : &utf8[0], required - 1, nullptr, nullptr);
        DWORD written = 0;
        WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }

    CloseHandle(hFile);
    ReleaseSRWLockExclusive(&gLogLock);
}


/// <summary>
/// Initializes error handling.
/// </summary>
void ErrorHandler::Initialize(LPCWSTR moduleName)
{
    StringCchCopy(gModuleName, _countof(gModuleName), moduleName);
}


/// <summary>
/// Sets the global error level.
/// </summary>
void ErrorHandler::SetLevel(Level level)
{
    gErrorLevel = level;
}


/// <summary>
/// Displays a formatted error message.
/// </summary>
/// <param name="level">The error level.</param>
/// <param name="format">The message to print.</param>
void ErrorHandler::Error(Level level, LPCWSTR format, ...)
{
    if (gErrorLevel < level)
    {
        return;
    }

    wchar_t message[MAX_LINE_LENGTH] = { 0 };

    if (format != nullptr)
    {
        va_list argList;
        va_start(argList, format);
        StringCchVPrintf(message, MAX_LINE_LENGTH, format, argList);
        va_end(argList);
    }

    LogErrorMessage(level, message);

    if (level == Level::Critical && *message != L'\0')
    {
        MessageBox(nullptr, message, LevelToCaption(level), MB_OK | LevelToIcon(level) | MB_SETFOREGROUND | MB_SYSTEMMODAL);
    }
}


/// <summary>
/// Displays a formatted error message with an HRESULT description.
/// </summary>
/// <param name="level">The error level.</param>
/// <param name="hr">The HRESULT value.</param>
/// <param name="format">The optional message to print.</param>
void ErrorHandler::ErrorHR(Level level, HRESULT hr, LPCWSTR format, ...)
{
    if (gErrorLevel < level)
    {
        return;
    }

    wchar_t message[MAX_LINE_LENGTH] = { 0 };

    if (format != nullptr)
    {
        va_list argList;
        va_start(argList, format);
        StringCchVPrintf(message, MAX_LINE_LENGTH, format, argList);
        va_end(argList);
        StringCchCat(message, _countof(message), L"\n\n");
    }

    LPTSTR end = wcschr(message, L'\0');
    if (end != nullptr)
    {
        size_t remaining = _countof(message) - static_cast<size_t>(end - message);
        DescriptionFromHR(hr, end, remaining);
    }

    if (*message != L'\0')
    {
        wchar_t logBuffer[MAX_LINE_LENGTH];
        StringCchPrintf(logBuffer, _countof(logBuffer), L"%ls (hr=0x%08X)", message, static_cast<unsigned int>(hr));
        LogErrorMessage(level, logBuffer);
    }

    if (level == Level::Critical && *message != L'\0')
    {
        MessageBox(nullptr, message, LevelToCaption(level), MB_OK | LevelToIcon(level) | MB_SETFOREGROUND | MB_SYSTEMMODAL);
    }
}

