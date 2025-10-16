#pragma once

#include "ThemeTypes.h"

namespace litestep
{
namespace themev2
{
    class Lexer
    {
    public:
        explicit Lexer(const SourceDocument& document);

        Token NextToken();
        std::size_t CurrentOffset() const noexcept;

    private:
        const SourceDocument& m_document;
        std::size_t m_offset;
        std::size_t m_length;

        void SkipWhitespaceAndComments();
        Token LexString();
        Token LexNumber();
        Token LexIdentifier();
        Token LexPunctuation(TokenType type, std::size_t startOffset, std::size_t length);
        Token MakeToken(TokenType type, std::size_t startOffset, std::size_t length, const std::wstring& lexeme);
        SourceLocation BuildLocation(std::size_t startOffset) const;

        wchar_t Peek() const;
        wchar_t PeekNext() const;
        wchar_t PeekAhead(std::size_t distance) const;
        wchar_t Advance();
        bool IsAtEnd() const;
    };
}
}

