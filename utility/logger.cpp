#include "logger.h"

#include <Windows.h>
#include <ShlObj.h>
#include <strsafe.h>
#include <mutex>
#include <string>
#include <vector>
#include <cstdarg>

namespace
{
    class LoggerImpl
    {
    public:
        void Initialize(const std::wstring& basePath)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_initialized)
            {
                return;
            }

            wchar_t expandedBase[MAX_PATH] = { 0 };
            if (!basePath.empty())
            {
                StringCchCopyW(expandedBase, _countof(expandedBase), basePath.c_str());
            }

            std::wstring root = expandedBase;
            if (!root.empty() && root.back() != L'\\' && root.back() != L'/')
            {
                root.push_back(L'\\');
            }
            root += L"logs";

            SHCreateDirectoryExW(nullptr, root.c_str(), nullptr);

            std::wstring filePath = root;
            filePath += L"\\litestep.log";

            m_handle = CreateFileW(filePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (m_handle != INVALID_HANDLE_VALUE)
            {
                LARGE_INTEGER zero = { 0 };
                SetFilePointerEx(m_handle, zero, nullptr, FILE_END);

                DWORD fileSizeHigh = 0;
                DWORD fileSizeLow = GetFileSize(m_handle, &fileSizeHigh);
                if (fileSizeLow == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
                {
                    // ignore
                }
                else if (fileSizeHigh == 0 && fileSizeLow == 0)
                {
                    const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
                    DWORD written = 0;
                    WriteFile(m_handle, bom, sizeof(bom), &written, nullptr);
                }

                m_initialized = true;
                WriteLine(L"===== LiteStep logging started =====");
            }
        }

        void Shutdown()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_handle != INVALID_HANDLE_VALUE)
            {
                WriteLine(L"===== LiteStep logging shutdown =====");
                CloseHandle(m_handle);
                m_handle = INVALID_HANDLE_VALUE;
            }
            m_initialized = false;
        }

        void Log(const wchar_t* message)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_initialized || m_handle == INVALID_HANDLE_VALUE)
            {
                return;
            }

            WriteLine(message);
        }

    private:
        void WriteLine(const wchar_t* message)
        {
            SYSTEMTIME st = { 0 };
            GetLocalTime(&st);

            wchar_t prefix[64] = { 0 };
            StringCchPrintfW(prefix, _countof(prefix),
                L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] ",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

            std::wstring line = prefix;
            line += message;
            line += L"\r\n";

            int utf8Bytes = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (utf8Bytes <= 1)
            {
                return;
            }

            std::vector<char> buffer(static_cast<size_t>(utf8Bytes - 1));
            WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, buffer.data(), utf8Bytes - 1, nullptr, nullptr);

            DWORD written = 0;
            WriteFile(m_handle, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr);
        }

        HANDLE m_handle = INVALID_HANDLE_VALUE;
        bool m_initialized = false;
        std::mutex m_mutex;
    };

    LoggerImpl& GetLogger()
    {
        static LoggerImpl instance;
        return instance;
    }
}

namespace Logger
{
    void Initialize(const std::wstring& basePath)
    {
        GetLogger().Initialize(basePath);
    }

    void Shutdown()
    {
        GetLogger().Shutdown();
    }

    void Log(const wchar_t* format, ...)
    {
        wchar_t buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
        va_end(args);

        GetLogger().Log(buffer);
    }
}
