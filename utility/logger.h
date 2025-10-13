#pragma once

#include <string>

namespace Logger
{
    void Initialize(const std::wstring& basePath);
    void Shutdown();
    void Log(const wchar_t* format, ...);
}
