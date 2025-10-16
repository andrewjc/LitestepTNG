//-------------------------------------------------------------------------------------------------
// /ModuleKit/Distance.cpp
// The nModules Project
//
// A distance. In the nModules, all distances are represented as a linear combination in the form
// d = a + b*parentLength + c*pixelSize, where pixelSize = DPI/96.
//-------------------------------------------------------------------------------------------------
#include "Distance.hpp"

#include <cmath>
#include <cwctype>
#include <stdlib.h>

namespace
{
    struct DistanceComponents
    {
        float pixels = 0.0f;
        float percent = 0.0f;
    };

    class DistanceExpressionParser
    {
    public:
        explicit DistanceExpressionParser(const wchar_t* expression)
            : m_cursor(expression)
        {
        }

        bool Parse(DistanceComponents& out)
        {
            SkipWhitespace();
            if (*m_cursor == L'\0')
            {
                return false;
            }

            if (!ParseSum(out))
            {
                return false;
            }

            SkipWhitespace();
            return (*m_cursor == L'\0');
        }

    private:
        static bool IsZero(float value)
        {
            return std::fabs(value) < 0.0001f;
        }

        void SkipWhitespace()
        {
            while (*m_cursor != L'\0' && iswspace(*m_cursor))
            {
                ++m_cursor;
            }
        }

        bool ParseSum(DistanceComponents& out)
        {
            if (!ParseProduct(out))
            {
                return false;
            }

            while (true)
            {
                SkipWhitespace();
                if (*m_cursor == L'+' || *m_cursor == L'-')
                {
                    const wchar_t op = *m_cursor++;
                    DistanceComponents rhs;
                    if (!ParseProduct(rhs))
                    {
                        return false;
                    }

                    if (op == L'+')
                    {
                        out.pixels += rhs.pixels;
                        out.percent += rhs.percent;
                    }
                    else
                    {
                        out.pixels -= rhs.pixels;
                        out.percent -= rhs.percent;
                    }
                }
                else
                {
                    break;
                }
            }

            return true;
        }

        bool ParseProduct(DistanceComponents& out)
        {
            if (!ParseFactor(out))
            {
                return false;
            }

            while (true)
            {
                SkipWhitespace();
                if (*m_cursor == L'*' || *m_cursor == L'/')
                {
                    const wchar_t op = *m_cursor++;
                    DistanceComponents rhs;
                    if (!ParseFactor(rhs))
                    {
                        return false;
                    }

                    if (op == L'*')
                    {
                        if (!Multiply(out, rhs))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (!Divide(out, rhs))
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    break;
                }
            }

            return true;
        }

        bool ParseFactor(DistanceComponents& out)
        {
            SkipWhitespace();

            bool negate = false;
            while (*m_cursor == L'+' || *m_cursor == L'-')
            {
                if (*m_cursor == L'-')
                {
                    negate = !negate;
                }
                ++m_cursor;
                SkipWhitespace();
            }

            if (*m_cursor == L'(')
            {
                ++m_cursor;
                if (!ParseSum(out))
                {
                    return false;
                }

                SkipWhitespace();
                if (*m_cursor != L')')
                {
                    return false;
                }
                ++m_cursor;
            }
            else
            {
                const wchar_t* start = m_cursor;
                float value = wcstof(m_cursor, const_cast<wchar_t**>(&m_cursor));
                if (m_cursor == start)
                {
                    return false;
                }

                SkipWhitespace();

                if (*m_cursor == L'%')
                {
                    ++m_cursor;
                    out.pixels = 0.0f;
                    out.percent = value / 100.0f;
                }
                else if (StartsWithInsensitive(L"dip"))
                {
                    m_cursor += 3;
                    out.pixels = value;
                    out.percent = 0.0f;
                }
                else
                {
                    out.pixels = value;
                    out.percent = 0.0f;
                }
            }

            if (negate)
            {
                out.pixels = -out.pixels;
                out.percent = -out.percent;
            }

            return true;
        }

        bool Multiply(DistanceComponents& lhs, const DistanceComponents& rhs)
        {
            if (!IsZero(lhs.percent) && !IsZero(rhs.percent))
            {
                return false;
            }

            DistanceComponents result;
            result.pixels = lhs.pixels * rhs.pixels;
            result.percent = lhs.pixels * rhs.percent + rhs.pixels * lhs.percent;

            lhs = result;
            return true;
        }

        bool Divide(DistanceComponents& lhs, const DistanceComponents& rhs)
        {
            if (!IsZero(rhs.percent))
            {
                return false;
            }

            const float divisor = rhs.pixels;
            if (IsZero(divisor))
            {
                return false;
            }

            lhs.pixels /= divisor;
            lhs.percent /= divisor;
            return true;
        }

        bool StartsWithInsensitive(LPCWSTR token)
        {
            const size_t len = wcslen(token);
            return _wcsnicmp(m_cursor, token, len) == 0;
        }

        const wchar_t* m_cursor;
    };
} // namespace

Distance::Distance() {}


Distance::Distance(float pixels)
  : mPixels(pixels)
  , mPercent(0)
  , mDips(0) {}


Distance::Distance(float pixels, float percent /*, float dips */)
  : mPixels(pixels)
  , mPercent(percent)
  , mDips(0) {}


/// <summary>
/// Subtraction operator
/// </summary>
Distance Distance::operator-(const Distance &other) {
  return Distance(mPixels - other.mPixels, mPercent - other.mPercent /*, mDips - other.mDips */);
}


/// <summary>
/// Addition operator
/// </summary>
Distance Distance::operator+(const Distance &other) {
  return Distance(mPixels + other.mPixels, mPercent + other.mPercent /*, mDips + other.mDips */);
}


/// <summary>
/// Multiplication operator
/// </summary>
Distance Distance::operator*(float factor) {
  return Distance(mPixels * factor, mPercent * factor /*, mDips * factor*/);
}


/// <summary>
/// Multiplication operator
/// </summary>
Distance Distance::operator/(float factor) {
  return Distance(mPixels / factor, mPercent / factor /*, mDips / factor*/);
}


/// <summary>
/// Returns the effective number of pixels.
/// </summary>
float Distance::Evaluate(float parentLength /*, float dpi*/) {
  return mPixels + parentLength * mPercent /* + dpi / 96.0f * mDips */;
}


/// <summary>
/// Parses a Distance from a string.
/// </summary>
/// <param name="distanceString">The string to parse.</param>
/// <param name="result">
/// If this is not null, and the function returns true, this will be set to the parsed related
/// number.
/// </param>
/// <return>true if the string is valid related number.</return>
bool Distance::Parse(LPCWSTR distanceString, Distance &out) {
  if (distanceString == nullptr) {
    return false;
  }

  DistanceComponents components;
  DistanceExpressionParser parser(distanceString);

  if (!parser.Parse(components))
  {
    return false;
  }

  out = Distance(components.pixels, components.percent /*, dips */);

  return true;
}

