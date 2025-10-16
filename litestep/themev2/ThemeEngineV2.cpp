#include "ThemeEngineV2.h"

#include "Parser.h"

#include "../utility/logger.h"
#include "../lsapi/lsapi.h"

#include <algorithm>
#include <cwctype>

namespace litestep
{
namespace themev2
{
    ThemeEngineV2* ThemeEngineV2::s_instance = nullptr;

    ThemeEngineV2::ThemeEngineV2()
        : m_enabled(false),
          m_bangsRegistered(false),
          m_themeRoot(),
          m_structureFile(L"theme.lsx"),
          m_sourceManager(),
          m_structureSource(),
          m_document(),
          m_diagnostics()
    {
    }

    ThemeEngineV2::~ThemeEngineV2()
    {
        Shutdown();
    }

    HRESULT ThemeEngineV2::Initialize()
    {
        s_instance = this;
        m_enabled = ResolveEnvironmentFlag();

        if (!m_enabled)
        {
            Logger::Log(L"ThemeEngineV2: disabled (LSTHEME_V2_ENABLED not set).");
            return S_FALSE;
        }

        wchar_t pathBuffer[MAX_PATH] = { 0 };
        if (!LSGetLitestepPathW(pathBuffer, MAX_PATH))
        {
            Logger::Log(L"ThemeEngineV2: failed to resolve LiteStep root path.");
            return E_FAIL;
        }

        m_themeRoot.assign(pathBuffer);
        m_sourceManager = std::make_unique<SourceManager>(m_themeRoot);
        m_structureFile = ResolveThemeFilePath();

        const HRESULT hr = LoadStructure();
        if (SUCCEEDED(hr))
        {
            RegisterBangs();
        }

        return hr;
    }

    void ThemeEngineV2::Shutdown()
    {
        UnregisterBangs();
        ClearState();
        m_sourceManager.reset();
        m_enabled = false;

        if (s_instance == this)
        {
            s_instance = nullptr;
        }
    }

    HRESULT ThemeEngineV2::Reload()
    {
        if (!m_enabled)
        {
            Logger::Log(L"ThemeEngineV2: reload requested but engine is disabled.");
            return S_FALSE;
        }

        const HRESULT hr = LoadStructure();
        if (FAILED(hr))
        {
            Logger::Log(L"ThemeEngineV2: reload failed (hr=0x%08X).", hr);
        }
        else
        {
            Logger::Log(L"ThemeEngineV2: reload completed (nodes=%u, directives=%u).",
                static_cast<unsigned>(m_document.rootNodes.size()),
                static_cast<unsigned>(m_document.directives.size()));
        }

        return hr;
    }

    bool ThemeEngineV2::IsEnabled() const noexcept
    {
        return m_enabled;
    }

    const ThemeDocument& ThemeEngineV2::Document() const noexcept
    {
        return m_document;
    }

    const std::vector<Diagnostic>& ThemeEngineV2::Diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    void ThemeEngineV2::BangReloadThemeV2(HWND, LPCWSTR)
    {
        if (s_instance != nullptr)
        {
            s_instance->Reload();
        }
    }

    void ThemeEngineV2::BangInspectThemeV2(HWND, LPCWSTR)
    {
        if (s_instance == nullptr)
        {
            Logger::Log(L"ThemeEngineV2: inspect requested but engine not initialized.");
            return;
        }

        const auto& engine = *s_instance;
        const std::size_t errorCount = std::count_if(
            engine.m_diagnostics.begin(),
            engine.m_diagnostics.end(),
            [](const Diagnostic& diagnostic)
            {
                return diagnostic.severity == DiagnosticSeverity::Error;
            });

        Logger::Log(L"ThemeEngineV2: components=%u directives=%u diagnostics=%u (errors=%u).",
            static_cast<unsigned>(engine.m_document.rootNodes.size()),
            static_cast<unsigned>(engine.m_document.directives.size()),
            static_cast<unsigned>(engine.m_diagnostics.size()),
            static_cast<unsigned>(errorCount));
    }

    bool ThemeEngineV2::ResolveEnvironmentFlag() const
    {
        DWORD required = GetEnvironmentVariableW(L"LSTHEME_V2_ENABLED", nullptr, 0);
        if (required == 0)
        {
            return false;
        }

        std::wstring buffer;
        buffer.resize(required);
        DWORD written = GetEnvironmentVariableW(L"LSTHEME_V2_ENABLED", &buffer[0], required);
        if (written == 0)
        {
            return false;
        }

        if (!buffer.empty() && buffer.back() == L'\0')
        {
            buffer.pop_back();
        }

        std::wstring normalized = buffer;
        auto beginIt = std::find_if_not(normalized.begin(), normalized.end(), [](wchar_t ch) { return iswspace(ch); });
        auto endIt = std::find_if_not(normalized.rbegin(), normalized.rend(), [](wchar_t ch) { return iswspace(ch); }).base();

        if (beginIt >= endIt)
        {
            normalized.clear();
        }
        else
        {
            normalized.assign(beginIt, endIt);
        }

        std::transform(normalized.begin(), normalized.end(), normalized.begin(), towlower);

        return normalized == L"1" || normalized == L"true" ||
            normalized == L"yes" || normalized == L"on";
    }

    std::wstring ThemeEngineV2::ResolveThemeFilePath() const
    {
        DWORD required = GetEnvironmentVariableW(L"LSThemeV2File", nullptr, 0);
        if (required == 0)
        {
            return L"theme.lsx";
        }

        std::wstring buffer;
        buffer.resize(required);
        DWORD written = GetEnvironmentVariableW(L"LSThemeV2File", &buffer[0], required);
        if (written == 0)
        {
            return L"theme.lsx";
        }

        if (!buffer.empty() && buffer.back() == L'\0')
        {
            buffer.pop_back();
        }

        auto beginIt = std::find_if_not(buffer.begin(), buffer.end(), [](wchar_t ch) { return iswspace(ch); });
        auto endIt = std::find_if_not(buffer.rbegin(), buffer.rend(), [](wchar_t ch) { return iswspace(ch); }).base();

        std::wstring normalized;
        if (beginIt < endIt)
        {
            normalized.assign(beginIt, endIt);
        }

        if (normalized.empty())
        {
            return L"theme.lsx";
        }

        return normalized;
    }

    HRESULT ThemeEngineV2::LoadStructure()
    {
        if (!m_sourceManager)
        {
            return E_FAIL;
        }

        ClearState();

        SourceDocument document;
        std::vector<Diagnostic> diagnostics;

        const bool loaded = m_sourceManager->LoadStructuredDocument(
            m_structureFile,
            document,
            diagnostics);

        m_diagnostics = diagnostics;

        if (!loaded)
        {
            LogDiagnostics();
            return E_FAIL;
        }

        m_structureSource = document;

        Parser parser(m_structureSource, m_diagnostics);
        m_document = parser.Parse();

        LogDiagnostics();

        const bool hasErrors = std::any_of(
            m_diagnostics.begin(),
            m_diagnostics.end(),
            [](const Diagnostic& diagnostic)
            {
                return diagnostic.severity == DiagnosticSeverity::Error;
            });

        return hasErrors ? S_FALSE : S_OK;
    }

    void ThemeEngineV2::RegisterBangs()
    {
        if (m_bangsRegistered)
        {
            return;
        }

        if (AddBangCommandW(L"!ReloadThemeV2", BangReloadThemeV2))
        {
            m_bangsRegistered = true;
            AddBangCommandW(L"!InspectThemeV2", BangInspectThemeV2);
        }
        else
        {
            Logger::Log(L"ThemeEngineV2: failed to register bangs.");
        }
    }

    void ThemeEngineV2::UnregisterBangs()
    {
        if (!m_bangsRegistered)
        {
            return;
        }

        RemoveBangCommandW(L"!ReloadThemeV2");
        RemoveBangCommandW(L"!InspectThemeV2");
        m_bangsRegistered = false;
    }

    void ThemeEngineV2::ClearState()
    {
        m_document = ThemeDocument();
        m_structureSource = SourceDocument();
        m_diagnostics.clear();
    }

    void ThemeEngineV2::LogDiagnostics() const
    {
        if (m_diagnostics.empty())
        {
            Logger::Log(L"ThemeEngineV2: parsed '%ls' with no diagnostics.", m_structureFile.c_str());
            return;
        }

        for (const Diagnostic& diagnostic : m_diagnostics)
        {
            const wchar_t* severity = L"info";
            switch (diagnostic.severity)
            {
            case DiagnosticSeverity::Warning:
                severity = L"warning";
                break;
            case DiagnosticSeverity::Error:
                severity = L"error";
                break;
            default:
                severity = L"info";
                break;
            }

            Logger::Log(L"ThemeEngineV2 %ls: %ls (file=%ls line=%u column=%u)",
                severity,
                diagnostic.message.c_str(),
                diagnostic.location.file.c_str(),
                static_cast<unsigned>(diagnostic.location.line),
                static_cast<unsigned>(diagnostic.location.column));
        }
    }
}
}

