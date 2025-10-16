#pragma once

#include <cstddef>
#include <utility>
#include <string>
#include <vector>

namespace litestep
{
namespace themev2
{
    struct SourceLocation
    {
        std::wstring file;
        std::size_t line;
        std::size_t column;

        SourceLocation() noexcept;
        SourceLocation(std::wstring filePath, std::size_t lineNumber, std::size_t columnNumber) noexcept;
        bool IsValid() const noexcept;
    };

    enum class DiagnosticSeverity
    {
        Info,
        Warning,
        Error
    };

    struct Diagnostic
    {
        DiagnosticSeverity severity;
        std::wstring message;
        SourceLocation location;
    };

    enum class TokenType
    {
        EndOfFile,
        Identifier,
        Number,
        String,
        LBrace,
        RBrace,
        LBracket,
        RBracket,
        LParen,
        RParen,
        Comma,
        Colon,
        Equals,
        Dot,
        Hash,
        At,
        Plus,
        Minus,
        Star,
        Slash,
        Percent,
        Pipe,
        Exclamation,
        Caret,
        Ampersand,
        Question,
        Less,
        Greater,
        Unknown
    };

    struct Token
    {
        TokenType type;
        std::wstring lexeme;
        SourceLocation location;
        std::size_t startOffset;
        std::size_t length;
    };

    enum class ValueKind
    {
        Null,
        String,
        Number,
        Boolean,
        Identifier,
        Reference,
        Object,
        Array
    };

    struct Value;

    struct ObjectProperty
    {
        std::wstring key;
        Value value;
        SourceLocation location;

        ObjectProperty();
        ObjectProperty(const std::wstring& keyName, const Value& propertyValue, const SourceLocation& locationInfo);
    };

    struct Value
    {
        ValueKind kind;
        std::wstring text;
        double numberValue;
        bool boolValue;
        std::vector<ObjectProperty> objectProperties;
        std::vector<Value> arrayValues;
        SourceLocation location;

        Value();
        explicit Value(ValueKind kindValue);

        static Value CreateString(const std::wstring& textValue, const SourceLocation& locationInfo);
        static Value CreateNumber(const std::wstring& textValue, double numericValue, const SourceLocation& locationInfo);
        static Value CreateBoolean(bool boolValue, const SourceLocation& locationInfo);
        static Value CreateIdentifier(const std::wstring& identifier, const SourceLocation& locationInfo);
        static Value CreateReference(const std::wstring& reference, const SourceLocation& locationInfo);
        static Value CreateObject(const SourceLocation& locationInfo);
        static Value CreateArray(const SourceLocation& locationInfo);
    };

    struct Attribute
    {
        std::wstring name;
        Value value;
        SourceLocation location;
    };

    struct ComponentNode
    {
        std::wstring component;
        std::wstring id;
        std::wstring name;
        std::vector<std::wstring> classes;
        std::vector<Attribute> attributes;
        std::vector<ComponentNode> children;
        SourceLocation location;
    };

    struct Directive
    {
        std::wstring name;
        std::wstring argument;
        SourceLocation location;
    };

    struct SourceDocumentSegment
    {
        std::wstring filePath;
        std::size_t startOffset;
        std::size_t lineStart;
    };

    struct SourceDocument
    {
        std::wstring primaryFile;
        std::wstring content;
        std::vector<SourceDocumentSegment> segments;
    };

    struct ThemeDocument
    {
        std::vector<Directive> directives;
        std::vector<ComponentNode> rootNodes;
    };
}
}

inline SourceLocation::SourceLocation() noexcept : line(0), column(0)
{
}

inline SourceLocation::SourceLocation(std::wstring filePath, std::size_t lineNumber, std::size_t columnNumber) noexcept
    : file(std::move(filePath)), line(lineNumber), column(columnNumber)
{
}

inline bool SourceLocation::IsValid() const noexcept
{
    return !file.empty();
}

inline ObjectProperty::ObjectProperty()
    : key(), value(), location()
{
}

inline ObjectProperty::ObjectProperty(const std::wstring& keyName, const Value& propertyValue, const SourceLocation& locationInfo)
    : key(keyName), value(propertyValue), location(locationInfo)
{
}

inline Value::Value()
    : kind(ValueKind::Null), text(), numberValue(0.0), boolValue(false), objectProperties(), arrayValues(), location()
{
}

inline Value::Value(ValueKind kindValue)
    : kind(kindValue), text(), numberValue(0.0), boolValue(false), objectProperties(), arrayValues(), location()
{
}

inline Value Value::CreateString(const std::wstring& textValue, const SourceLocation& locationInfo)
{
    Value value(ValueKind::String);
    value.text = textValue;
    value.location = locationInfo;
    return value;
}

inline Value Value::CreateNumber(const std::wstring& textValue, double numericValue, const SourceLocation& locationInfo)
{
    Value value(ValueKind::Number);
    value.text = textValue;
    value.numberValue = numericValue;
    value.location = locationInfo;
    return value;
}

inline Value Value::CreateBoolean(bool boolValueParam, const SourceLocation& locationInfo)
{
    Value value(ValueKind::Boolean);
    value.boolValue = boolValueParam;
    value.text = boolValueParam ? L"true" : L"false";
    value.location = locationInfo;
    return value;
}

inline Value Value::CreateIdentifier(const std::wstring& identifier, const SourceLocation& locationInfo)
{
    Value value(ValueKind::Identifier);
    value.text = identifier;
    value.location = locationInfo;
    return value;
}

inline Value Value::CreateReference(const std::wstring& reference, const SourceLocation& locationInfo)
{
    Value value(ValueKind::Reference);
    value.text = reference;
    value.location = locationInfo;
    return value;
}

inline Value Value::CreateObject(const SourceLocation& locationInfo)
{
    Value value(ValueKind::Object);
    value.location = locationInfo;
    return value;
}

inline Value Value::CreateArray(const SourceLocation& locationInfo)
{
    Value value(ValueKind::Array);
    value.location = locationInfo;
    return value;
}

