#include "Parser.h"

#include <algorithm>
#include <cwchar>

namespace litestep
{
namespace themev2
{
    namespace
    {
        std::wstring TrimLeft(const std::wstring& value)
        {
            std::wstring::size_type index = 0;
            while (index < value.length() && iswspace(value[index]))
            {
                ++index;
            }
            return value.substr(index);
        }

        std::wstring TrimRight(const std::wstring& value)
        {
            std::wstring::size_type index = value.length();
            while (index > 0 && iswspace(value[index - 1]))
            {
                --index;
            }
            return value.substr(0, index);
        }

        std::wstring Trim(const std::wstring& value)
        {
            return TrimRight(TrimLeft(value));
        }

        void AppendClassName(const std::wstring& name, std::vector<std::wstring>& classes)
        {
            if (name.empty())
            {
                return;
            }

            if (std::find(classes.begin(), classes.end(), name) == classes.end())
            {
                classes.push_back(name);
            }
        }

        void SplitClassString(const std::wstring& text, std::vector<std::wstring>& classes)
        {
            std::wstring current;
            for (wchar_t ch : text)
            {
                if (iswspace(ch))
                {
                    AppendClassName(current, classes);
                    current.clear();
                }
                else
                {
                    current.push_back(ch);
                }
            }

            AppendClassName(current, classes);
        }
    }

    Parser::Parser(const SourceDocument& document, std::vector<Diagnostic>& diagnostics)
        : m_document(document), m_lexer(document), m_diagnostics(diagnostics), m_tokens(), m_cursor(0)
    {
    }

    ThemeDocument Parser::Parse()
    {
        ThemeDocument document;

        while (!IsAtEnd())
        {
            const Token& token = LookAhead(0);

            if (token.type == TokenType::Hash)
            {
                const Token& next = LookAhead(1);
                const Token& third = LookAhead(2);

                if (next.type == TokenType::Identifier && third.type == TokenType::LBrace)
                {
                    document.rootNodes.push_back(ParseComponent());
                }
                else
                {
                    document.directives.push_back(ParseDirective());
                }
            }
            else if (token.type == TokenType::EndOfFile)
            {
                break;
            }
            else
            {
                ReportError(token, L"Unexpected token at top-level.");
                Advance();
            }
        }

        return document;
    }

    const Token& Parser::LookAhead(std::size_t distance)
    {
        while (m_cursor + distance >= m_tokens.size())
        {
            m_tokens.push_back(m_lexer.NextToken());
        }

        return m_tokens[m_cursor + distance];
    }

    const Token& Parser::Current()
    {
        return LookAhead(0);
    }

    bool Parser::Match(TokenType type)
    {
        if (Check(type))
        {
            Advance();
            return true;
        }

        return false;
    }

    bool Parser::Check(TokenType type)
    {
        if (IsAtEnd())
        {
            return false;
        }

        return LookAhead(0).type == type;
    }

    bool Parser::Expect(TokenType type, const wchar_t* message)
    {
        if (!Match(type))
        {
            ReportError(Current(), message);
            return false;
        }

        return true;
    }

    void Parser::Advance()
    {
        if (!IsAtEnd())
        {
            ++m_cursor;
        }
    }

    bool Parser::IsAtEnd()
    {
        return LookAhead(0).type == TokenType::EndOfFile;
    }

    Directive Parser::ParseDirective()
    {
        const Token& hashToken = LookAhead(0);
        Match(TokenType::Hash);

        const Token& nameToken = LookAhead(0);
        if (!Expect(TokenType::Identifier, L"Expected directive identifier."))
        {
            Synchronize();
            Directive directive;
            directive.location = hashToken.location;
            return directive;
        }

        Directive directive;
        directive.name = nameToken.lexeme;
        directive.location = hashToken.location;
        directive.argument = ExtractDirectiveArgument(hashToken, nameToken);

        std::size_t lineEnd = m_document.content.find(L'\n', hashToken.startOffset);
        if (lineEnd == std::wstring::npos)
        {
            lineEnd = m_document.content.length();
        }

        while (!IsAtEnd() && LookAhead(0).startOffset < lineEnd)
        {
            Advance();
        }

        return directive;
    }

    ComponentNode Parser::ParseComponent()
    {
        Match(TokenType::Hash);
        const Token& nameToken = LookAhead(0);
        if (!Expect(TokenType::Identifier, L"Expected component name after '#'."))
        {
            Synchronize();
            ComponentNode empty;
            empty.location = nameToken.location;
            return empty;
        }

        ComponentNode node;
        node.component = nameToken.lexeme;
        node.location = nameToken.location;

        if (!Expect(TokenType::LBrace, L"Expected '{' to start component body."))
        {
            Synchronize();
            return node;
        }

        while (!IsAtEnd())
        {
            const Token& token = LookAhead(0);

            if (token.type == TokenType::RBrace)
            {
                Advance();
                break;
            }

            if (token.type == TokenType::Hash)
            {
                const Token& next = LookAhead(1);
                const Token& third = LookAhead(2);
                if (next.type == TokenType::Identifier && third.type == TokenType::LBrace)
                {
                    node.children.push_back(ParseComponent());
                    continue;
                }
            }

            if (token.type == TokenType::Identifier && LookAhead(1).type == TokenType::Equals)
            {
                Attribute attribute = ParseAttribute();

                if (_wcsicmp(attribute.name.c_str(), L"id") == 0 &&
                    (attribute.value.kind == ValueKind::String || attribute.value.kind == ValueKind::Identifier))
                {
                    node.id = attribute.value.text;
                }
                else if (_wcsicmp(attribute.name.c_str(), L"name") == 0 &&
                    (attribute.value.kind == ValueKind::String || attribute.value.kind == ValueKind::Identifier))
                {
                    node.name = attribute.value.text;
                }
                else if (_wcsicmp(attribute.name.c_str(), L"class") == 0)
                {
                    if (attribute.value.kind == ValueKind::String || attribute.value.kind == ValueKind::Identifier)
                    {
                        SplitClassString(attribute.value.text, node.classes);
                    }
                    else if (attribute.value.kind == ValueKind::Array)
                    {
                        for (const Value& entry : attribute.value.arrayValues)
                        {
                            if (entry.kind == ValueKind::String || entry.kind == ValueKind::Identifier)
                            {
                                SplitClassString(entry.text, node.classes);
                            }
                        }
                    }
                }

                node.attributes.push_back(attribute);
                continue;
            }

            ReportError(token, L"Unexpected token inside component body.");
            Advance();
        }

        return node;
    }

    Attribute Parser::ParseAttribute()
    {
        const Token& nameToken = LookAhead(0);
        Match(TokenType::Identifier);

        Attribute attribute;
        attribute.name = nameToken.lexeme;
        attribute.location = nameToken.location;

        if (!Expect(TokenType::Equals, L"Expected '=' after attribute name."))
        {
            attribute.value = Value();
            return attribute;
        }

        attribute.value = ParseValue();

        if (Check(TokenType::Comma))
        {
            Advance();
        }

        return attribute;
    }

    Value Parser::ParseValue()
    {
        const Token& token = LookAhead(0);
        switch (token.type)
        {
        case TokenType::String:
            Advance();
            return Value::CreateString(token.lexeme, token.location);
        case TokenType::Number:
        {
            wchar_t* endPtr = nullptr;
            double numericValue = std::wcstod(token.lexeme.c_str(), &endPtr);
            Advance();
            return Value::CreateNumber(token.lexeme, numericValue, token.location);
        }
        case TokenType::Identifier:
        {
            if (_wcsicmp(token.lexeme.c_str(), L"true") == 0)
            {
                Advance();
                return Value::CreateBoolean(true, token.location);
            }

            if (_wcsicmp(token.lexeme.c_str(), L"false") == 0)
            {
                Advance();
                return Value::CreateBoolean(false, token.location);
            }

            Advance();
            return Value::CreateIdentifier(token.lexeme, token.location);
        }
        case TokenType::LBrace:
            return ParseObjectLiteral();
        case TokenType::LBracket:
            return ParseArrayLiteral();
        case TokenType::At:
            return ParseReference();
        default:
            ReportError(token, L"Unexpected token in value expression.");
            Advance();
            return Value();
        }
    }

    Value Parser::ParseObjectLiteral()
    {
        const Token& openToken = LookAhead(0);
        Match(TokenType::LBrace);

        Value object = Value::CreateObject(openToken.location);

        while (!IsAtEnd())
        {
            if (Check(TokenType::RBrace))
            {
                Advance();
                break;
            }

            const Token& keyToken = LookAhead(0);
            if (!Match(TokenType::Identifier))
            {
                ReportError(keyToken, L"Expected identifier in object literal.");
                Synchronize();
                break;
            }

            if (!Expect(TokenType::Equals, L"Expected '=' in object literal."))
            {
                Synchronize();
                break;
            }

            Value propertyValue = ParseValue();
            object.objectProperties.push_back(ObjectProperty(keyToken.lexeme, propertyValue, keyToken.location));

            if (Check(TokenType::Comma))
            {
                Advance();
            }
        }

        return object;
    }

    Value Parser::ParseArrayLiteral()
    {
        const Token& openToken = LookAhead(0);
        Match(TokenType::LBracket);

        Value array = Value::CreateArray(openToken.location);

        while (!IsAtEnd())
        {
            if (Check(TokenType::RBracket))
            {
                Advance();
                break;
            }

            Value element = ParseValue();
            array.arrayValues.push_back(element);

            if (Check(TokenType::Comma))
            {
                Advance();
            }
        }

        return array;
    }

    Value Parser::ParseReference()
    {
        const Token& atToken = LookAhead(0);
        Match(TokenType::At);

        const Token& identifierToken = LookAhead(0);
        if (!Match(TokenType::Identifier))
        {
            ReportError(identifierToken, L"Expected identifier after '@'.");
            return Value::CreateReference(std::wstring(), atToken.location);
        }

        return Value::CreateReference(identifierToken.lexeme, atToken.location);
    }

    void Parser::Synchronize()
    {
        while (!IsAtEnd())
        {
            const Token& token = LookAhead(0);
            if (token.type == TokenType::Hash || token.type == TokenType::RBrace)
            {
                return;
            }

            Advance();
        }
    }

    void Parser::ReportError(const Token& token, const std::wstring& message)
    {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.message = message;
        diagnostic.location = token.location;
        m_diagnostics.push_back(diagnostic);
    }

    std::wstring Parser::ExtractDirectiveArgument(const Token& hashToken, const Token& nameToken) const
    {
        std::size_t lineEnd = m_document.content.find(L'\n', hashToken.startOffset);
        if (lineEnd == std::wstring::npos)
        {
            lineEnd = m_document.content.length();
        }

        const std::size_t argumentStart = nameToken.startOffset + nameToken.length;
        if (argumentStart >= lineEnd)
        {
            return std::wstring();
        }

        std::wstring argument = m_document.content.substr(argumentStart, lineEnd - argumentStart);
        return Trim(argument);
    }
}
}

