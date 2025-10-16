#pragma once

#include "lsapidefines.h"

#ifndef LSAPI_API
#  if defined(LSAPI_INTERNAL)
#    define LSAPI_API __declspec(dllexport)
#  else
#    define LSAPI_API __declspec(dllimport)
#  endif
#endif

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

// Legacy comparers used by SettingsManager and other components.
struct stringicmp
{
    bool operator()(const std::wstring& s1, const std::wstring& s2) const
    {
        return (_wcsicmp(s1.c_str(), s2.c_str()) < 0);
    }
};

struct stringcmp
{
    bool operator()(const std::wstring& s1, const std::wstring& s2) const
    {
        return (wcscmp(s1.c_str(), s2.c_str()) < 0);
    }
};

namespace lsapi {

template <typename CharT>
struct DefaultWhitespace;

template <>
struct DefaultWhitespace<char>
{
    static const char* Get() { return " \t\n\r"; }
};

template <>
struct DefaultWhitespace<wchar_t>
{
    static const wchar_t* Get() { return L" \t\n\r"; }
};

/// <summary>
/// Common string helper routines shared across the LiteStep codebase.
/// Allocations follow the historical semantics from utility/stringutility and
/// modules/Utilities/StringUtils (callers are responsible for freeing via free
/// or delete[], depending on the returned pointer type).
/// </summary>
class LSAPI_API StringUtils final
{
public:
    StringUtils() = delete;

    static inline wchar_t* WcsFromMbs(const char* pszMBS)
    {
        if (!pszMBS)
        {
            return nullptr;
        }

        size_t length = strlen(pszMBS) + 1;
        auto buffer = new wchar_t[length];
        MultiByteToWideChar(CP_ACP, 0, pszMBS, -1, buffer, static_cast<int>(length));
        return buffer;
    }

    static inline char* MbsFromWcs(const wchar_t* pwzWCS)
    {
        if (!pwzWCS)
        {
            return nullptr;
        }

        size_t length = wcslen(pwzWCS) + 1;
        auto buffer = new char[length];
        WideCharToMultiByte(CP_ACP, 0, pwzWCS, -1, buffer, static_cast<int>(length), "?", nullptr);
        return buffer;
    }

    static inline LPSTR PartialDup(LPCSTR str, size_t cch)
    {
        if (!str)
        {
            return nullptr;
        }

        auto result = static_cast<LPSTR>(malloc(cch + 1));
        if (!result)
        {
            return nullptr;
        }

        memcpy(result, str, cch);
        result[cch] = '\0';
        return result;
    }

    static inline LPWSTR PartialDup(LPCWSTR str, size_t cch)
    {
        if (!str)
        {
            return nullptr;
        }

        auto result = static_cast<LPWSTR>(malloc(sizeof(WCHAR) * (cch + 1)));
        if (!result)
        {
            return nullptr;
        }

        memcpy(result, str, sizeof(WCHAR) * cch);
        result[cch] = L'\0';
        return result;
    }

    static inline LPSTR ReallocOverwrite(LPSTR dest, LPCSTR str)
    {
        if (!str)
        {
            return dest;
        }

        size_t cch = strlen(str);
        auto result = static_cast<LPSTR>(realloc(dest, cch + 1));
        if (!result)
        {
            return dest;
        }

        memcpy(result, str, cch);
        result[cch] = '\0';
        return result;
    }

    static inline LPWSTR ReallocOverwrite(LPWSTR dest, LPCWSTR str)
    {
        if (!str)
        {
            return dest;
        }

        size_t cch = wcslen(str);
        auto result = static_cast<LPWSTR>(realloc(dest, sizeof(WCHAR) * (cch + 1)));
        if (!result)
        {
            return dest;
        }

        memcpy(result, str, sizeof(WCHAR) * cch);
        result[cch] = L'\0';
        return result;
    }

    template <typename TString, std::enable_if_t<!std::is_pointer_v<TString>, int> = 0>
    static inline TString TrimCopy(const TString& value, const typename TString::value_type* whitespace = nullptr)
    {
        using CharT = typename TString::value_type;
        const CharT* ws = whitespace ? whitespace : DefaultWhitespace<CharT>::Get();
        const auto first = value.find_first_not_of(ws);
        if (first == TString::npos)
        {
            return TString();
        }

        const auto last = value.find_last_not_of(ws);
        return value.substr(first, last - first + 1);
    }

    template <typename TString, std::enable_if_t<!std::is_pointer_v<TString>, int> = 0>
    static inline void TrimInPlace(TString& value, const typename TString::value_type* whitespace = nullptr)
    {
        value = TrimCopy(value, whitespace);
    }

    template <typename TString, std::enable_if_t<!std::is_pointer_v<TString>, int> = 0>
    static inline TString TrimQuotesCopy(const TString& value)
    {
        if (value.size() >= 2)
        {
            using CharT = typename TString::value_type;
            const CharT first = value.front();
            const CharT last = value.back();
            if (first == last && (first == static_cast<CharT>('"') || first == static_cast<CharT>(0x27)))
            {
                return value.substr(1, value.size() - 2);
            }
        }

        return value;
    }

    template <typename TString, std::enable_if_t<!std::is_pointer_v<TString>, int> = 0>
    static inline void TrimQuotesInPlace(TString& value)
    {
        value = TrimQuotesCopy(value);
    }

    template <typename CharT>
    static inline std::basic_string<CharT> TrimCopy(const CharT* value, const CharT* whitespace = nullptr)
    {
        if (!value)
        {
            return std::basic_string<CharT>();
        }

        return TrimCopy(std::basic_string<CharT>(value), whitespace);
    }

    template <typename CharT>
    static inline std::basic_string<CharT> TrimQuotesCopy(const CharT* value)
    {
        if (!value)
        {
            return std::basic_string<CharT>();
        }

        return TrimQuotesCopy(std::basic_string<CharT>(value));
    }
};

} // namespace lsapi

inline wchar_t* WCSFromMBS(const char* pszMBS)
{
    return lsapi::StringUtils::WcsFromMbs(pszMBS);
}

inline char* MBSFromWCS(const wchar_t* pwzWCS)
{
    return lsapi::StringUtils::MbsFromWcs(pwzWCS);
}

/// <summary>
/// Hashing function for null-terminated strings.
/// </summary>
template <typename CharPtrType, typename CharTransformer>
struct StringHasher
{
    size_t operator()(CharPtrType str) const
    {
#if defined(_WIN64)
        static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
        const size_t _FNV_offset_basis = 14695981039346656037ULL;
        const size_t _FNV_prime = 1099511628211ULL;
#else
        static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
        const size_t _FNV_offset_basis = 2166136261U;
        const size_t _FNV_prime = 16777619U;
#endif
        size_t value = _FNV_offset_basis;
        for (CharPtrType chr = str; *chr != 0; ++chr)
        {
            value ^= static_cast<size_t>(CharTransformer()(*chr));
            value *= _FNV_prime;
        }

#if defined(_WIN64)
        value ^= value >> 32;
#endif
        return value;
    }
};

/// <summary>
/// Case sensitive functions for StringKeyedContainers.
/// </summary>
struct CaseSensitive
{
    struct Hash
    {
        size_t operator()(LPCWSTR str) const
        {
            struct CharTransformer
            {
                size_t operator()(wchar_t chr) const { return static_cast<size_t>(chr); }
            };
            return StringHasher<LPCWSTR, CharTransformer>()(str);
        }

        size_t operator()(LPCSTR str) const
        {
            struct CharTransformer
            {
                size_t operator()(char chr) const { return static_cast<size_t>(static_cast<unsigned char>(chr)); }
            };
            return StringHasher<LPCSTR, CharTransformer>()(str);
        }

        size_t operator()(const std::wstring& str) const { return operator()(str.c_str()); }
        size_t operator()(const std::string& str) const { return operator()(str.c_str()); }
    };

    struct Compare
    {
        bool operator()(LPCWSTR a, LPCWSTR b) const { return wcscmp(a, b) < 0; }
        bool operator()(LPCSTR a, LPCSTR b) const { return strcmp(a, b) < 0; }
        bool operator()(const std::wstring& a, const std::wstring& b) const { return wcscmp(a.c_str(), b.c_str()) < 0; }
        bool operator()(const std::string& a, const std::string& b) const { return strcmp(a.c_str(), b.c_str()) < 0; }
    };

    struct Equal
    {
        bool operator()(LPCWSTR a, LPCWSTR b) const { return wcscmp(a, b) == 0; }
        bool operator()(LPCSTR a, LPCSTR b) const { return strcmp(a, b) == 0; }
        bool operator()(const std::wstring& a, const std::wstring& b) const { return wcscmp(a.c_str(), b.c_str()) == 0; }
        bool operator()(const std::string& a, const std::string& b) const { return strcmp(a.c_str(), b.c_str()) == 0; }
    };
};

/// <summary>
/// Case insensitive functions for StringKeyedContainers.
/// </summary>
struct CaseInsensitive
{
    struct Hash
    {
        size_t operator()(LPCWSTR str) const
        {
            struct CharTransformer
            {
                size_t operator()(wchar_t chr) const { return static_cast<size_t>(towlower(chr)); }
            };
            return StringHasher<LPCWSTR, CharTransformer>()(str);
        }

        size_t operator()(LPCSTR str) const
        {
            struct CharTransformer
            {
                size_t operator()(char chr) const
                {
                    return static_cast<size_t>(tolower(static_cast<unsigned char>(chr)));
                }
            };
            return StringHasher<LPCSTR, CharTransformer>()(str);
        }

        size_t operator()(const std::wstring& str) const { return operator()(str.c_str()); }
        size_t operator()(const std::string& str) const { return operator()(str.c_str()); }
    };

    struct Compare
    {
        bool operator()(LPCSTR a, LPCSTR b) const { return _stricmp(a, b) < 0; }
        bool operator()(LPCWSTR a, LPCWSTR b) const { return _wcsicmp(a, b) < 0; }
        bool operator()(const std::string& a, const std::string& b) const { return _stricmp(a.c_str(), b.c_str()) < 0; }
        bool operator()(const std::wstring& a, const std::wstring& b) const { return _wcsicmp(a.c_str(), b.c_str()) < 0; }
    };

    struct Equal
    {
        bool operator()(LPCSTR a, LPCSTR b) const { return _stricmp(a, b) == 0; }
        bool operator()(LPCWSTR a, LPCWSTR b) const { return _wcsicmp(a, b) == 0; }
        bool operator()(const std::string& a, const std::string& b) const { return _stricmp(a.c_str(), b.c_str()) == 0; }
        bool operator()(const std::wstring& a, const std::wstring& b) const { return _wcsicmp(a.c_str(), b.c_str()) == 0; }
    };
};

template <typename KeyType, typename Type, typename KeyOperators = CaseInsensitive,
          typename Allocator = std::allocator<std::pair<std::add_const<KeyType>, Type>>>
struct StringKeyedMaps
{
    using Map = std::map<KeyType, Type, typename KeyOperators::Compare, Allocator>;
    using ConstMap = const Map;
    using MultiMap = std::multimap<KeyType, Type, typename KeyOperators::Compare, Allocator>;
    using ConstMultiMap = const MultiMap;
    using UnorderedMap = std::unordered_map<KeyType, Type, typename KeyOperators::Hash, typename KeyOperators::Equal, Allocator>;
    using ConstUnorderedMap = const UnorderedMap;
    using UnorderedMultiMap = std::unordered_multimap<KeyType, Type, typename KeyOperators::Hash, typename KeyOperators::Equal, Allocator>;
    using ConstUnorderedMultiMap = const UnorderedMultiMap;
};

template <typename Type, typename Operators = CaseInsensitive,
          typename Allocator = std::allocator<Type>>
struct StringKeyedSets
{
    using Set = std::set<Type, typename Operators::Compare, Allocator>;
    using ConstSet = const Set;
    using MultiSet = std::multiset<Type, typename Operators::Compare, Allocator>;
    using ConstMultiSet = const MultiSet;
    using UnorderedSet = std::unordered_set<Type, typename Operators::Hash, typename Operators::Equal, Allocator>;
    using ConstUnorderedSet = const UnorderedSet;
    using UnorderedMultiSet = std::unordered_multiset<Type, typename Operators::Hash, typename Operators::Equal, Allocator>;
    using ConstUnorderedMultiSet = const UnorderedMultiSet;
};

#define WCSTOMBS(str) std::unique_ptr<char>(MBSFromWCS(str)).get()
#define MBSTOWCS(str) std::unique_ptr<wchar_t>(WCSFromMBS(str)).get()








