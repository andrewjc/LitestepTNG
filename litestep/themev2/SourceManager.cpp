#include "SourceManager.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <strsafe.h>

#include <utility>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <cstring>
#include <vector>

namespace litestep
{
namespace themev2
{
    namespace
    {
        std::wstring ToLookupKey(const std::wstring& path)
        {
            std::wstring normalized = path;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), towlower);
            return normalized;
        }

        void AppendDiagnostic(
            std::vector<Diagnostic>& diagnostics,
            DiagnosticSeverity severity,
            const std::wstring& message,
            const std::wstring& file)
        {
            Diagnostic diagnostic;
            diagnostic.severity = severity;
            diagnostic.message = message;
            diagnostic.location = SourceLocation(file, 0, 0);
            diagnostics.push_back(diagnostic);
        }
    }

    SourceManager::SourceManager(std::wstring baseDirectory)
        : m_baseDirectory(std::move(baseDirectory)), m_cache()
    {
    }

    SourceManager::~SourceManager() = default;

    const std::wstring& SourceManager::BaseDirectory() const noexcept
    {
        return m_baseDirectory;
    }

    bool SourceManager::LoadStructuredDocument(
        const std::wstring& entryFile,
        SourceDocument& outDocument,
        std::vector<Diagnostic>& diagnostics)
    {
        LoadContext context;

        std::wstring entryPath = entryFile;
        if (entryPath.empty())
        {
            entryPath.assign(L"theme.lsx");
        }

        if (PathIsRelativeW(entryPath.c_str()))
        {
            wchar_t combined[MAX_PATH] = { 0 };
            StringCchCopyW(combined, MAX_PATH, m_baseDirectory.c_str());
            PathAppendW(combined, entryPath.c_str());
            entryPath.assign(combined);
        }

        const std::wstring normalized = NormalizePath(entryPath);
        if (normalized.empty())
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Unable to resolve theme entry file path.", entryPath);
            return false;
        }

        const bool result = LoadDocumentRecursive(normalized, context, outDocument, diagnostics);
        if (result)
        {
            outDocument.primaryFile = normalized;
        }

        return result;
    }

    bool SourceManager::LoadStyleDocument(
        const std::wstring& entryFile,
        SourceDocument& outDocument,
        std::vector<Diagnostic>& diagnostics)
    {
        LoadContext context;

        std::wstring entryPath = entryFile;
        if (PathIsRelativeW(entryPath.c_str()))
        {
            wchar_t combined[MAX_PATH] = { 0 };
            StringCchCopyW(combined, MAX_PATH, m_baseDirectory.c_str());
            PathAppendW(combined, entryPath.c_str());
            entryPath.assign(combined);
        }

        const std::wstring normalized = NormalizePath(entryPath);
        if (normalized.empty())
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Unable to resolve style file path.", entryFile);
            return false;
        }

        const bool result = LoadDocumentRecursive(normalized, context, outDocument, diagnostics);
        if (result)
        {
            outDocument.primaryFile = normalized;
        }

        return result;
    }

    bool SourceManager::LoadDocumentRecursive(
        const std::wstring& absolutePath,
        LoadContext& context,
        SourceDocument& outDocument,
        std::vector<Diagnostic>& diagnostics)
    {
        const std::wstring canonicalPath = NormalizePath(absolutePath);
        if (canonicalPath.empty())
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Unable to normalize file path.", absolutePath);
            return false;
        }

        const std::wstring lookupKey = ToLookupKey(canonicalPath);

        auto cacheIter = m_cache.find(lookupKey);
        if (cacheIter != m_cache.end())
        {
            outDocument = cacheIter->second;
            return true;
        }

        if (context.activeStack.count(lookupKey) != 0)
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Detected recursive #include directive.", canonicalPath);
            return false;
        }

        context.activeStack.insert(lookupKey);

        std::wstring fileContents;
        if (!LoadDocumentFromDisk(canonicalPath, fileContents, diagnostics))
        {
            context.activeStack.erase(lookupKey);
            return false;
        }

        SourceDocument localDocument;
        localDocument.primaryFile = canonicalPath;

        const bool processed = ProcessFileContent(
            canonicalPath,
            fileContents,
            context,
            localDocument,
            diagnostics);

        context.activeStack.erase(lookupKey);

        if (!processed)
        {
            return false;
        }

        m_cache.insert(std::make_pair(lookupKey, localDocument));
        outDocument = localDocument;
        return true;
    }

    bool SourceManager::LoadDocumentFromDisk(
        const std::wstring& absolutePath,
        std::wstring& outContents,
        std::vector<Diagnostic>& diagnostics) const
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, absolutePath.c_str(), L"rb") != 0 || file == nullptr)
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Failed to open file.", absolutePath);
            return false;
        }

        std::vector<unsigned char> buffer;
        if (fseek(file, 0, SEEK_END) == 0)
        {
            long size = ftell(file);
            if (size < 0)
            {
                fclose(file);
                AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                    L"Failed to read file length.", absolutePath);
                return false;
            }

            buffer.resize(static_cast<std::size_t>(size));
            rewind(file);

            if (!buffer.empty())
            {
                const size_t read = fread(buffer.data(), 1, buffer.size(), file);
                if (read != buffer.size())
                {
                    fclose(file);
                    AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                        L"Failed to read file contents.", absolutePath);
                    return false;
                }
            }
        }

        fclose(file);

        if (buffer.empty())
        {
            outContents.clear();
            return true;
        }

        if (buffer.size() >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE)
        {
            const size_t charCount = (buffer.size() - 2) / sizeof(wchar_t);
            outContents.resize(charCount);
            memcpy(&outContents[0], buffer.data() + 2, charCount * sizeof(wchar_t));
            if (!outContents.empty() && outContents.back() == L'\0')
            {
                outContents.pop_back();
            }
            return true;
        }

        size_t offset = 0;
        if (buffer.size() >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
        {
            offset = 3;
        }

        const int required = MultiByteToWideChar(
            CP_UTF8,
            0,
            reinterpret_cast<const char*>(buffer.data() + offset),
            static_cast<int>(buffer.size() - offset),
            nullptr,
            0);
        if (required <= 0)
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Failed to convert file contents to Unicode.", absolutePath);
            return false;
        }

        outContents.resize(static_cast<std::size_t>(required));
        MultiByteToWideChar(
            CP_UTF8,
            0,
            reinterpret_cast<const char*>(buffer.data() + offset),
            static_cast<int>(buffer.size() - offset),
            &outContents[0],
            required);

        if (!outContents.empty() && outContents.back() == L'\0')
        {
            outContents.pop_back();
        }

        return true;
    }

    bool SourceManager::ProcessFileContent(
        const std::wstring& absolutePath,
        const std::wstring& fileContents,
        LoadContext& context,
        SourceDocument& outDocument,
        std::vector<Diagnostic>& diagnostics)
    {
        std::wstring chunk;
        std::size_t chunkLineStart = 0;
        std::size_t currentLine = 1;

        auto flushChunk = [&]()
        {
            if (!chunk.empty())
            {
                SourceDocumentSegment segment;
                segment.filePath = absolutePath;
                segment.startOffset = outDocument.content.size();
                segment.lineStart = chunkLineStart;
                outDocument.segments.push_back(segment);

                outDocument.content.append(chunk);
                chunk.clear();
                chunkLineStart = 0;
            }
        };

        std::size_t index = 0;
        const std::size_t length = fileContents.length();

        while (index < length)
        {
            const std::size_t lineStart = index;
            while (index < length && fileContents[index] != L'\r' && fileContents[index] != L'\n')
            {
                ++index;
            }

            std::wstring line = fileContents.substr(lineStart, index - lineStart);

            std::size_t newlineLength = 0;
            if (index < length)
            {
                if (fileContents[index] == L'\r' && index + 1 < length && fileContents[index + 1] == L'\n')
                {
                    newlineLength = 2;
                }
                else
                {
                    newlineLength = 1;
                }
            }

            index += newlineLength;
            const bool hadNewline = (newlineLength > 0);

            const std::wstring trimmed = TrimLeft(line);
            bool processedInclude = false;

            if (!trimmed.empty() && trimmed[0] == L'#')
            {
                const std::wstring includeDirective = L"#include";
                if (trimmed.compare(0, includeDirective.length(), includeDirective) == 0)
                {
                    flushChunk();

                    const std::size_t beforeAppend = outDocument.content.size();
                    const std::wstring includeToken = trimmed.substr(includeDirective.length());
                    if (!AppendInclude(absolutePath, includeToken, context, outDocument, diagnostics))
                    {
                        return false;
                    }

                    const bool includeEndsWithNewline =
                        beforeAppend != outDocument.content.size() &&
                        outDocument.content.back() == L'\n';

                    if (hadNewline && !includeEndsWithNewline)
                    {
                        SourceDocumentSegment newlineSegment;
                        newlineSegment.filePath = absolutePath;
                        newlineSegment.startOffset = outDocument.content.size();
                        newlineSegment.lineStart = currentLine;
                        outDocument.segments.push_back(newlineSegment);
                        outDocument.content.push_back(L'\n');
                    }

                    processedInclude = true;
                }
            }

            if (!processedInclude)
            {
                if (chunk.empty())
                {
                    chunkLineStart = currentLine;
                }

                chunk.append(line);

                if (hadNewline)
                {
                    chunk.push_back(L'\n');
                }
            }

            ++currentLine;
        }

        flushChunk();

        if (outDocument.segments.empty())
        {
            SourceDocumentSegment segment;
            segment.filePath = absolutePath;
            segment.startOffset = 0;
            segment.lineStart = 1;
            outDocument.segments.push_back(segment);
        }

        return true;
    }

    bool SourceManager::AppendInclude(
        const std::wstring& requestingPath,
        const std::wstring& includeToken,
        LoadContext& context,
        SourceDocument& outDocument,
        std::vector<Diagnostic>& diagnostics)
    {
        std::wstring includePath = Trim(includeToken);
        if (!includePath.empty() && includePath[0] == L'=')
        {
            includePath = Trim(includePath.substr(1));
        }

        if (!includePath.empty() &&
            ((includePath.front() == L'"' && includePath.back() == L'"') ||
             (includePath.front() == L'\'' && includePath.back() == L'\'')))
        {
            includePath = includePath.substr(1, includePath.length() - 2);
        }

        includePath = ExpandEnvironment(includePath);

        if (includePath.empty())
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Invalid #include directive.", requestingPath);
            return false;
        }

        wchar_t resolved[MAX_PATH] = { 0 };
        if (PathIsRelativeW(includePath.c_str()))
        {
            StringCchCopyW(resolved, MAX_PATH, DirectoryOf(requestingPath).c_str());
            PathAppendW(resolved, includePath.c_str());
        }
        else
        {
            StringCchCopyW(resolved, MAX_PATH, includePath.c_str());
        }

        const std::wstring normalized = NormalizePath(resolved);
        if (normalized.empty())
        {
            AppendDiagnostic(diagnostics, DiagnosticSeverity::Error,
                L"Failed to resolve include path.", includePath);
            return false;
        }

        SourceDocument includedDocument;
        if (!LoadDocumentRecursive(normalized, context, includedDocument, diagnostics))
        {
            return false;
        }

        const std::size_t insertionOffset = outDocument.content.size();
        outDocument.content.append(includedDocument.content);

        for (auto segment : includedDocument.segments)
        {
            segment.startOffset += insertionOffset;
            outDocument.segments.push_back(segment);
        }

        return true;
    }

    std::wstring SourceManager::NormalizePath(const std::wstring& path)
    {
        if (path.empty())
        {
            return std::wstring();
        }

        DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (required == 0)
        {
            return std::wstring();
        }

        std::wstring buffer;
        buffer.resize(required);

        DWORD written = GetFullPathNameW(path.c_str(), required, &buffer[0], nullptr);
        if (written == 0)
        {
            return std::wstring();
        }

        buffer.resize(written);
        return buffer;
    }

    std::wstring SourceManager::DirectoryOf(const std::wstring& path)
    {
        wchar_t buffer[MAX_PATH] = { 0 };
        StringCchCopyW(buffer, MAX_PATH, path.c_str());
        PathRemoveFileSpecW(buffer);
        return buffer;
    }

    std::wstring SourceManager::ExpandEnvironment(const std::wstring& value)
    {
        if (value.empty())
        {
            return value;
        }

        DWORD required = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (required == 0)
        {
            return value;
        }

        std::wstring buffer;
        buffer.resize(required);

        DWORD written = ExpandEnvironmentStringsW(value.c_str(), &buffer[0], required);
        if (written == 0)
        {
            return value;
        }

        if (!buffer.empty() && buffer.back() == L'\0')
        {
            buffer.pop_back();
        }

        return buffer;
    }

    std::wstring SourceManager::Trim(const std::wstring& value)
    {
        return TrimRight(TrimLeft(value));
    }

    std::wstring SourceManager::TrimLeft(const std::wstring& value)
    {
        std::wstring::size_type index = 0;
        while (index < value.length() && iswspace(value[index]))
        {
            ++index;
        }
        return value.substr(index);
    }

    std::wstring SourceManager::TrimRight(const std::wstring& value)
    {
        if (value.empty())
        {
            return value;
        }

        std::wstring::size_type index = value.length();
        while (index > 0 && iswspace(value[index - 1]))
        {
            --index;
        }
        return value.substr(0, index);
    }
}
}

