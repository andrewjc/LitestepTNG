#pragma once
#include <Windows.h>
#include <string>

#include "ThemeTypes.h"
#include "SourceManager.h"

#include <memory>
#include <vector>

namespace litestep
{
namespace themev2
{
    class ThemeEngineV2
    {
    public:
        ThemeEngineV2();
        ~ThemeEngineV2();

        HRESULT Initialize();
        void Shutdown();
        HRESULT Reload();

        bool IsEnabled() const noexcept;
        const ThemeDocument& Document() const noexcept;
        const std::vector<Diagnostic>& Diagnostics() const noexcept;

    private:
        static ThemeEngineV2* s_instance;

        static void BangReloadThemeV2(HWND caller, LPCWSTR args);
        static void BangInspectThemeV2(HWND caller, LPCWSTR args);

        bool ResolveEnvironmentFlag() const;
        std::wstring ResolveThemeFilePath() const;
        HRESULT LoadStructure();
        void RegisterBangs();
        void UnregisterBangs();
        void ClearState();
        void LogDiagnostics() const;

        bool m_enabled;
        bool m_bangsRegistered;
        std::wstring m_themeRoot;
        std::wstring m_structureFile;
        std::unique_ptr<SourceManager> m_sourceManager;
        SourceDocument m_structureSource;
        ThemeDocument m_document;
        std::vector<Diagnostic> m_diagnostics;
    };
}
}

