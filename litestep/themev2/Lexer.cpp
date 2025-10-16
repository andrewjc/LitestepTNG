#include "Lexer.h"

#include <cwctype>
#include <cstdlib>

namespace litestep
{
namespace themev2
{
    Lexer::Lexer(const SourceDocument& document)
        : m_document(document), m_offset(0), m_length(document.content.length())
    {
    }

    Token Lexer::NextToken()
    {
        SkipWhitespaceAndComments();

        if (IsAtEnd())
        {
            return MakeToken(TokenType::EndOfFile, m_offset, 0, std::wstring());
        }

        const wchar_t current = Peek();

        if (current == L'"')
        {
            return LexString();
        }

        auto isDigit = [](wchar_t ch) -> bool
        {
            return ch != 0 && std::iswdigit(ch) != 0;
        };

        bool numberStart = false;
        if (isDigit(current) || (current == L'.' && isDigit(PeekAhead(1))))
        {
            numberStart = true;
        }
        else if ((current == L'+' || current == L'-'))
        {
            const wchar_t next = PeekAhead(1);
            if (isDigit(next) || (next == L'.' && isDigit(PeekAhead(2))))
            {
                numberStart = true;
            }
        }

        if (numberStart)
        {
            return LexNumber();
        }

        if (std::iswalpha(current) || current == L'_')
        {
            return LexIdentifier();
        }

        const std::size_t start = m_offset;
        Advance();

        switch (current)
        {
        case L'{': return LexPunctuation(TokenType::LBrace, start, 1);
        case L'}': return LexPunctuation(TokenType::RBrace, start, 1);
        case L'[': return LexPunctuation(TokenType::LBracket, start, 1);
        case L']': return LexPunctuation(TokenType::RBracket, start, 1);
        case L'(': return LexPunctuation(TokenType::LParen, start, 1);
        case L')': return LexPunctuation(TokenType::RParen, start, 1);
        case L',': return LexPunctuation(TokenType::Comma, start, 1);
        case L':': return LexPunctuation(TokenType::Colon, start, 1);
        case L'=': return LexPunctuation(TokenType::Equals, start, 1);
        case L'.': return LexPunctuation(TokenType::Dot, start, 1);
        case L'#': return LexPunctuation(TokenType::Hash, start, 1);
        case L'@': return LexPunctuation(TokenType::At, start, 1);
        case L'+': return LexPunctuation(TokenType::Plus, start, 1);
        case L'-': return LexPunctuation(TokenType::Minus, start, 1);
        case L'*': return LexPunctuation(TokenType::Star, start, 1);
        case L'/': return LexPunctuation(TokenType::Slash, start, 1);
        case L'%': return LexPunctuation(TokenType::Percent, start, 1);
        case L'|': return LexPunctuation(TokenType::Pipe, start, 1);
        case L'!': return LexPunctuation(TokenType::Exclamation, start, 1);
        case L'^': return LexPunctuation(TokenType::Caret, start, 1);
        case L'&': return LexPunctuation(TokenType::Ampersand, start, 1);
        case L'?': return LexPunctuation(TokenType::Question, start, 1);
        case L'<': return LexPunctuation(TokenType::Less, start, 1);
        case L'>': return LexPunctuation(TokenType::Greater, start, 1);
        default:
            return LexPunctuation(TokenType::Unknown, start, 1);
        }
    }

    std::size_t Lexer::CurrentOffset() const noexcept
    {
        return m_offset;
    }

    void Lexer::SkipWhitespaceAndComments()
    {
        bool advanced = true;
        while (!IsAtEnd() && advanced)
        {
            advanced = false;

            while (!IsAtEnd() && std::iswspace(Peek()))
            {
                Advance();
                advanced = true;
            }

            if (IsAtEnd())
            {
                break;
            }

            if (Peek() == L'/' && PeekNext() == L'/')
            {
                Advance();
                Advance();
                while (!IsAtEnd() && Peek() != L'\n')
                {
                    Advance();
                }
                advanced = true;
            }
            else if (Peek() == L'/' && PeekNext() == L'*')
            {
                Advance();
                Advance();
                while (!IsAtEnd())
                {
                    if (Peek() == L'*' && PeekNext() == L'/')
                    {
                        Advance();
                        Advance();
                        break;
                    }
                    Advance();
                }
                advanced = true;
            }
        }
    }

    Token Lexer::LexString()
    {
        const std::size_t start = m_offset;
        Advance();

        std::wstring value;

        while (!IsAtEnd())
        {
            const wchar_t ch = Advance();
            if (ch == L'"')
            {
                break;
            }

            if (ch == L'\\' && !IsAtEnd())
            {
                const wchar_t escaped = Advance();
                switch (escaped)
                {
                case L'"': value.push_back(L'"'); break;
                case L'\\': value.push_back(L'\\'); break;
                case L'n': value.push_back(L'\n'); break;
                case L'r': value.push_back(L'\r'); break;
                case L't': value.push_back(L'\t'); break;
                default:
                    value.push_back(escaped);
                    break;
                }
            }
            else
            {
                value.push_back(ch);
            }
        }

        const std::size_t length = m_offset - start;
        return MakeToken(TokenType::String, start, length, value);
    }

    Token Lexer::LexNumber()
    {
        const std::size_t start = m_offset;

        if (Peek() == L'+' || Peek() == L'-')
        {
            Advance();
        }

        bool digitsBeforeDecimal = false;
        while (std::iswdigit(Peek()))
        {
            Advance();
            digitsBeforeDecimal = true;
        }

        bool digitsAfterDecimal = false;
        if (Peek() == L'.' && std::iswdigit(PeekNext()))
        {
            Advance();
            while (std::iswdigit(Peek()))
            {
                Advance();
                digitsAfterDecimal = true;
            }
        }

        if (!digitsBeforeDecimal && !digitsAfterDecimal)
        {
            m_offset = start;
            Advance();
            return LexPunctuation(TokenType::Unknown, start, 1);
        }

        if (Peek() == L'%')
        {
            Advance();
        }

        while (std::iswalpha(Peek()))
        {
            Advance();
        }

        const std::size_t length = m_offset - start;
        std::wstring lexeme = m_document.content.substr(start, length);
        return MakeToken(TokenType::Number, start, length, lexeme);
    }

    Token Lexer::LexIdentifier()
    {
        const std::size_t start = m_offset;

        while (!IsAtEnd())
        {
            const wchar_t ch = Peek();
            if (std::iswalnum(ch) || ch == L'_' || ch == L'-' || ch == L':')
            {
                Advance();
            }
            else
            {
                break;
            }
        }

        const std::size_t length = m_offset - start;
        std::wstring lexeme = m_document.content.substr(start, length);
        return MakeToken(TokenType::Identifier, start, length, lexeme);
    }

    Token Lexer::LexPunctuation(TokenType type, std::size_t startOffset, std::size_t length)
    {
        std::wstring lexeme;
        if (startOffset + length <= m_document.content.length())
        {
            lexeme = m_document.content.substr(startOffset, length);
        }
        return MakeToken(type, startOffset, length, lexeme);
    }

    Token Lexer::MakeToken(TokenType type, std::size_t startOffset, std::size_t length, const std::wstring& lexeme)
    {
        Token token;
        token.type = type;
        token.lexeme = lexeme;
        token.startOffset = startOffset;
        token.length = length;
        token.location = BuildLocation(startOffset);
        return token;
    }

    SourceLocation Lexer::BuildLocation(std::size_t startOffset) const
    {
        if (m_document.segments.empty())
        {
            return SourceLocation(m_document.primaryFile, 0, 0);
        }

        const SourceDocumentSegment* segment = &m_document.segments.front();
        for (const auto& candidate : m_document.segments)
        {
            if (candidate.startOffset <= startOffset)
            {
                segment = &candidate;
            }
            else
            {
                break;
            }
        }

        const std::size_t segmentStart = segment->startOffset;
        std::size_t line = segment->lineStart;
        std::size_t lastLineStart = segmentStart;
        const std::size_t length = m_document.content.length();
        const std::size_t limit = (startOffset < length) ? startOffset : length;

        for (std::size_t index = segmentStart; index < limit; ++index)
        {
            if (m_document.content[index] == L'\n')
            {
                ++line;
                lastLineStart = index + 1;
            }
        }

        const std::size_t column = (startOffset >= lastLineStart) ? (startOffset - lastLineStart + 1) : 1;
        const std::wstring& filePath = segment->filePath.empty() ? m_document.primaryFile : segment->filePath;
        return SourceLocation(filePath, line, column);
    }

    wchar_t Lexer::Peek() const
    {
        if (IsAtEnd())
        {
            return 0;
        }
        return m_document.content[m_offset];
    }

    wchar_t Lexer::PeekNext() const
    {
        return PeekAhead(1);
    }

    wchar_t Lexer::PeekAhead(std::size_t distance) const
    {
        const std::size_t index = m_offset + distance;
        if (index >= m_length)
        {
            return 0;
        }
        return m_document.content[index];
    }

    wchar_t Lexer::Advance()
    {
        if (IsAtEnd())
        {
            return 0;
        }
        return m_document.content[m_offset++];
    }

    bool Lexer::IsAtEnd() const
    {
        return m_offset >= m_length;
    }
}
}

