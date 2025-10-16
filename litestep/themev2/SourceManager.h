#pragma once

#include "ThemeTypes.h"

#include <unordered_map>
#include <unordered_set>

namespace litestep
{
namespace themev2
{
    class SourceManager
    {
    public:
        explicit SourceManager(std::wstring baseDirectory);
        ~SourceManager();

        const std::wstring& BaseDirectory() const noexcept;

        bool LoadStructuredDocument(
            const std::wstring& entryFile,
            SourceDocument& outDocument,
            std::vector<Diagnostic>& diagnostics);

        bool LoadStyleDocument(
            const std::wstring& entryFile,
            SourceDocument& outDocument,
            std::vector<Diagnostic>& diagnostics);

    private:
        struct LoadContext
        {
            std::unordered_set<std::wstring> activeStack;
        };

        std::wstring m_baseDirectory;
        std::unordered_map<std::wstring, SourceDocument> m_cache;

        bool LoadDocumentRecursive(
            const std::wstring& absolutePath,
            LoadContext& context,
            SourceDocument& outDocument,
            std::vector<Diagnostic>& diagnostics);

        bool LoadDocumentFromDisk(
            const std::wstring& absolutePath,
            std::wstring& outContents,
            std::vector<Diagnostic>& diagnostics) const;

        bool ProcessFileContent(
            const std::wstring& absolutePath,
            const std::wstring& fileContents,
            LoadContext& context,
            SourceDocument& outDocument,
            std::vector<Diagnostic>& diagnostics);

        bool AppendInclude(
            const std::wstring& requestingPath,
            const std::wstring& includeToken,
            LoadContext& context,
            SourceDocument& outDocument,
            std::vector<Diagnostic>& diagnostics);

        static std::wstring NormalizePath(const std::wstring& path);
        static std::wstring DirectoryOf(const std::wstring& path);
        static std::wstring ExpandEnvironment(const std::wstring& value);
        static std::wstring Trim(const std::wstring& value);
        static std::wstring TrimLeft(const std::wstring& value);
        static std::wstring TrimRight(const std::wstring& value);
    };
}
}

