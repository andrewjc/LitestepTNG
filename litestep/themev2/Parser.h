#pragma once

#include "Lexer.h"

#include <vector>

namespace litestep
{
namespace themev2
{
    class Parser
    {
    public:
        Parser(const SourceDocument& document, std::vector<Diagnostic>& diagnostics);

        ThemeDocument Parse();

    private:
        const SourceDocument& m_document;
        Lexer m_lexer;
        std::vector<Diagnostic>& m_diagnostics;
        std::vector<Token> m_tokens;
        std::size_t m_cursor;

        const Token& LookAhead(std::size_t distance);
        const Token& Current();
        bool Match(TokenType type);
        bool Check(TokenType type);
        bool Expect(TokenType type, const wchar_t* message);
        void Advance();
        bool IsAtEnd();

        Directive ParseDirective();
        ComponentNode ParseComponent();
        Attribute ParseAttribute();
        Value ParseValue();
        Value ParseObjectLiteral();
        Value ParseArrayLiteral();
        Value ParseReference();

        void Synchronize();
        void ReportError(const Token& token, const std::wstring& message);
        std::wstring ExtractDirectiveArgument(const Token& hashToken, const Token& nameToken) const;
    };
}
}

