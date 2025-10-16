/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  StateSettings.cpp
 *  The nModules Project
 *
 *  Holds all RC settings used by the State class.
 *  
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "../ModuleKit/LiteStep.h"
#include <strsafe.h>
#include "State.hpp"
#include "Color.h"

#include <algorithm>
#include "../Utilities/Unordered1To1Map.hpp"
#include "../Utilities/StringUtils.h"
#include <cmath>
#include <limits>

using ShadowLayer = State::ShadowLayer;
template<typename SettingType>
using SettingMap = const Unordered1To1Map<
    SettingType,
    LPCTSTR,
    std::hash<SettingType>,
    CaseInsensitive::Hash,
    std::equal_to<SettingType>,
    CaseInsensitive::Equal
>;


State::ShadowLayer::ShadowLayer()
    : offsetX(0.0f)
    , offsetY(4.0f)
    , blurRadius(0.0f)
    , spread(0.0f)
    , opacity(0.0f)
    , scaleX(1.0f)
    , scaleY(1.0f)
    , samples(8)
    , color(D2D1::ColorF(D2D1::ColorF::Black))
    , enabled(false)
{
}

static SettingMap<DWRITE_READING_DIRECTION> readingDirectionMap(
{
    { DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"LeftToRight" },
    { DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, L"RightToLeft" }
});

static SettingMap<DWRITE_WORD_WRAPPING> wordWrappingMap(
{
    { DWRITE_WORD_WRAPPING_NO_WRAP, L"NoWrap" },
    { DWRITE_WORD_WRAPPING_WRAP,    L"Wrap"   }
});

static SettingMap<DWRITE_FONT_STYLE> fontStyleMap(
{
    { DWRITE_FONT_STYLE_NORMAL,  L"Normal"     },
    { DWRITE_FONT_STYLE_OBLIQUE, L"Oblique"    },
    { DWRITE_FONT_STYLE_ITALIC,  L"Italic"     }
});

static SettingMap<DWRITE_TEXT_ALIGNMENT> textAlignmentMap(
{
    { DWRITE_TEXT_ALIGNMENT_LEADING,  L"Left"   },
    { DWRITE_TEXT_ALIGNMENT_CENTER,   L"Center" },
    { DWRITE_TEXT_ALIGNMENT_TRAILING, L"Right"  }
});

static SettingMap<DWRITE_PARAGRAPH_ALIGNMENT> paragraphAlignmentMap(
{
    { DWRITE_PARAGRAPH_ALIGNMENT_NEAR,     L"Top"     },
    { DWRITE_PARAGRAPH_ALIGNMENT_CENTER,   L"Middle"  },
    { DWRITE_PARAGRAPH_ALIGNMENT_FAR,      L"Bottom"  }
});

static SettingMap<DWRITE_TRIMMING_GRANULARITY> trimmingGranularityMap(
{
    { DWRITE_TRIMMING_GRANULARITY_CHARACTER,    L"Character" },
    { DWRITE_TRIMMING_GRANULARITY_WORD,         L"Word"      },
    { DWRITE_TRIMMING_GRANULARITY_NONE,         L"None"      }
});

static SettingMap<DWRITE_FONT_STRETCH> fontStretchMap(
{
    { DWRITE_FONT_STRETCH_NORMAL,           L"Normal"          },
    { DWRITE_FONT_STRETCH_ULTRA_CONDENSED,  L"Ultra Condensed" },
    { DWRITE_FONT_STRETCH_EXTRA_CONDENSED,  L"Extra Condensed" },
    { DWRITE_FONT_STRETCH_CONDENSED,        L"Condensed"       },
    { DWRITE_FONT_STRETCH_SEMI_CONDENSED,   L"Semi Condensed"  },
    { DWRITE_FONT_STRETCH_MEDIUM,           L"Medium"          },
    { DWRITE_FONT_STRETCH_SEMI_EXPANDED,    L"Semi Expanded"   },
    { DWRITE_FONT_STRETCH_EXPANDED,         L"Expanded"        },
    { DWRITE_FONT_STRETCH_EXTRA_EXPANDED,   L"Extra Expanded"  },
    { DWRITE_FONT_STRETCH_ULTRA_EXPANDED,   L"Ultra Expanded"  }
});

static SettingMap<DWRITE_FONT_WEIGHT> fontWeightMap(
{
    { DWRITE_FONT_WEIGHT_THIN,         L"Thin"         },
    { DWRITE_FONT_WEIGHT_EXTRA_LIGHT,  L"Extra Light"  },
    { DWRITE_FONT_WEIGHT_ULTRA_LIGHT,  L"Ultra Light"  },
    { DWRITE_FONT_WEIGHT_LIGHT,        L"Light"        },
    { DWRITE_FONT_WEIGHT_SEMI_LIGHT,   L"Semi Light"   },
    { DWRITE_FONT_WEIGHT_REGULAR,      L"Regular"      },
    { DWRITE_FONT_WEIGHT_MEDIUM,       L"Medium"       },
    { DWRITE_FONT_WEIGHT_SEMI_BOLD,    L"Semi Bold"    },
    { DWRITE_FONT_WEIGHT_BOLD,         L"Bold"         },
    { DWRITE_FONT_WEIGHT_EXTRA_BOLD,   L"Extra Bold"   },
    { DWRITE_FONT_WEIGHT_ULTRA_BOLD,   L"Ultra Bold"   },
    { DWRITE_FONT_WEIGHT_BLACK,        L"Black"        },
    { DWRITE_FONT_WEIGHT_HEAVY,        L"Heavy"        },
    { DWRITE_FONT_WEIGHT_EXTRA_BLACK,  L"Extra Black"  },
    { DWRITE_FONT_WEIGHT_ULTRA_BLACK,  L"Ultra Black"  }
});


/// <summary>
/// Initalizes the class to all default settings.
/// </summary>
State::Settings::Settings()
    : cornerRadiusX(0.0f)
    , cornerRadiusY(0.0f)
    , fontSize(12.0f)
    , fontStretch(DWRITE_FONT_STRETCH_NORMAL)
    , fontStrokeWidth(0.0f)
    , fontStyle(DWRITE_FONT_STYLE_NORMAL)
    , fontWeight(DWRITE_FONT_WEIGHT_NORMAL)
    , outlineWidth(0.0f)
    , readingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT)
    , textAlign(DWRITE_TEXT_ALIGNMENT_LEADING)
    , textOffsetBottom(0.0f)
    , textOffsetLeft(0.0f)
    , textOffsetRight(0.0f)
    , textOffsetTop(0.0f)
    , textRotation(0.0f)
    , textTrimmingGranularity(DWRITE_TRIMMING_GRANULARITY_CHARACTER)
    , textVerticalAlign(DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
    , wordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)
{
    StringCchCopy(this->font, _countof(this->font), L"Arial");
    brushSettings[State::BrushType::Text].color = Color::Create(0xFFFFFFFF);
    cornerRadii.topLeft = { 0.0f, 0.0f };
    cornerRadii.topRight = { 0.0f, 0.0f };
    cornerRadii.bottomRight = { 0.0f, 0.0f };
    cornerRadii.bottomLeft = { 0.0f, 0.0f };
    shadowLayers.clear();
}


/// <summary>
/// Loads settings from an RC file using the specified defaults.
/// </summary>
void State::Settings::Load(const ::Settings * settings, const Settings * defaults)
{
    TCHAR buffer[MAX_PATH];
    if (!defaults)
    {
        defaults = this;
    }

    const float sentinel = std::numeric_limits<float>::quiet_NaN();

    auto readFloatWithDefault = [&](LPCWSTR key, float fallback, bool &hasValue) -> float {
        float value = settings->GetFloat(key, sentinel);
        if (std::isnan(value)) {
            hasValue = false;
            return fallback;
        }
        hasValue = true;
        return value;
    };

    bool hasCornerX = false;
    bool hasCornerY = false;
    this->cornerRadiusX = readFloatWithDefault(L"CornerRadiusX", defaults->cornerRadiusX, hasCornerX);
    this->cornerRadiusY = readFloatWithDefault(L"CornerRadiusY", defaults->cornerRadiusY, hasCornerY);

    this->cornerRadii = defaults->cornerRadii;

    auto isZeroCorner = [](const State::CornerRadius &corner) noexcept {
        return corner.x == 0.0f && corner.y == 0.0f;
    };
    const bool defaultsCornersEqual =
        defaults->cornerRadii.topLeft.x == defaults->cornerRadii.topRight.x &&
        defaults->cornerRadii.topLeft.y == defaults->cornerRadii.topRight.y &&
        defaults->cornerRadii.topRight.x == defaults->cornerRadii.bottomRight.x &&
        defaults->cornerRadii.topRight.y == defaults->cornerRadii.bottomRight.y &&
        defaults->cornerRadii.bottomRight.x == defaults->cornerRadii.bottomLeft.x &&
        defaults->cornerRadii.bottomRight.y == defaults->cornerRadii.bottomLeft.y;
    const bool defaultsMatchUniform = defaultsCornersEqual &&
        defaults->cornerRadii.topLeft.x == defaults->cornerRadiusX &&
        defaults->cornerRadii.topLeft.y == defaults->cornerRadiusY;
    const bool defaultsAllZero = isZeroCorner(defaults->cornerRadii.topLeft) &&
        isZeroCorner(defaults->cornerRadii.topRight) &&
        isZeroCorner(defaults->cornerRadii.bottomRight) &&
        isZeroCorner(defaults->cornerRadii.bottomLeft);
    bool defaultsCustom = !(defaultsCornersEqual && defaultsMatchUniform);
    if (defaultsAllZero && (defaults->cornerRadiusX != 0.0f || defaults->cornerRadiusY != 0.0f)) {
        defaultsCustom = false;
    }

    auto applyUniform = [&](State::CornerRadius &corner) {
        if (hasCornerX) {
            corner.x = this->cornerRadiusX;
        }
        if (hasCornerY) {
            corner.y = this->cornerRadiusY;
        }
    };

    if (hasCornerX || hasCornerY || defaults == this || !defaultsCustom) {
        applyUniform(this->cornerRadii.topLeft);
        applyUniform(this->cornerRadii.topRight);
        applyUniform(this->cornerRadii.bottomRight);
        applyUniform(this->cornerRadii.bottomLeft);
    }

    auto updateCorner = [&](State::CornerRadius &corner, LPCWSTR uniformKey, LPCWSTR keyX, LPCWSTR keyY) {
        float uniformValue = settings->GetFloat(uniformKey, sentinel);
        if (!std::isnan(uniformValue)) {
            corner.x = corner.y = uniformValue;
        }
        float xValue = settings->GetFloat(keyX, sentinel);
        if (!std::isnan(xValue)) {
            corner.x = xValue;
        }
        float yValue = settings->GetFloat(keyY, sentinel);
        if (!std::isnan(yValue)) {
            corner.y = yValue;
        }
    };

    updateCorner(this->cornerRadii.topLeft, L"CornerRadiusTopLeft", L"CornerRadiusTopLeftX", L"CornerRadiusTopLeftY");
    updateCorner(this->cornerRadii.topRight, L"CornerRadiusTopRight", L"CornerRadiusTopRightX", L"CornerRadiusTopRightY");
    updateCorner(this->cornerRadii.bottomRight, L"CornerRadiusBottomRight", L"CornerRadiusBottomRightX", L"CornerRadiusBottomRightY");
    updateCorner(this->cornerRadii.bottomLeft, L"CornerRadiusBottomLeft", L"CornerRadiusBottomLeftX", L"CornerRadiusBottomLeftY");

    const std::vector<ShadowLayer> *defaultShadowLayersPtr = defaults == this ? &shadowLayers : &defaults->shadowLayers;
    std::vector<ShadowLayer> initialShadowLayers = defaultShadowLayersPtr ? *defaultShadowLayersPtr : std::vector<ShadowLayer>();

    WCHAR presetBuffer[MAX_LINE_LENGTH];
    if (settings->GetLine(L"ShadowPreset", presetBuffer, _countof(presetBuffer), nullptr) && presetBuffer[0] != L'\0') {
        WCHAR tokenBuffers[16][MAX_RCCOMMAND];
        LPTSTR tokenPtrs[16];
        for (int i = 0; i < 16; ++i) {
            tokenPtrs[i] = tokenBuffers[i];
        }

        int presetCount = LiteStep::CommandTokenize(presetBuffer, tokenPtrs, _countof(tokenPtrs), nullptr);
        std::vector<State::ShadowLayer> presetLayers;
        bool matchedPreset = false;
        for (int i = 0; i < presetCount; ++i) {
            if (tokenPtrs[i] != nullptr && tokenPtrs[i][0] != L'\0') {
                std::vector<State::ShadowLayer> layers;
                if (State::TryGetShadowPreset(tokenPtrs[i], layers)) {
                    presetLayers.insert(presetLayers.end(), layers.begin(), layers.end());
                    matchedPreset = true;
                }
            }
        }

        if (matchedPreset) {
            initialShadowLayers = presetLayers;
        }
    }

    int defaultShadowCount = static_cast<int>(initialShadowLayers.size());
    int requestedShadowCount = settings->GetInt(L"ShadowLayerCount", defaultShadowCount);
    requestedShadowCount = std::max(0, requestedShadowCount);

    shadowLayers.clear();
    shadowLayers.reserve(requestedShadowCount);

    for (int index = 0; index < requestedShadowCount; ++index) {
        ShadowLayer layer = index < defaultShadowCount ? initialShadowLayers[index] : ShadowLayer();

        WCHAR key[64];
        auto readLayerFloat = [&](LPCWSTR format, float &target) {
            StringCchPrintf(key, _countof(key), format, index + 1);
            float value = settings->GetFloat(key, std::numeric_limits<float>::quiet_NaN());
            if (!std::isnan(value)) {
                target = value;
            }
        };

        readLayerFloat(L"Shadow%dOffsetX", layer.offsetX);
        readLayerFloat(L"Shadow%dOffsetY", layer.offsetY);
        readLayerFloat(L"Shadow%dBlur", layer.blurRadius);
        readLayerFloat(L"Shadow%dSpread", layer.spread);
        readLayerFloat(L"Shadow%dScaleX", layer.scaleX);
        readLayerFloat(L"Shadow%dScaleY", layer.scaleY);

        StringCchPrintf(key, _countof(key), L"Shadow%dOpacity", index + 1);
        float opacityValue = settings->GetFloat(key, std::numeric_limits<float>::quiet_NaN());
        if (!std::isnan(opacityValue)) {
            layer.opacity = opacityValue;
        }
        layer.opacity = std::max(0.0f, std::min(layer.opacity, 1.0f));

        StringCchPrintf(key, _countof(key), L"Shadow%dSamples", index + 1);
        layer.samples = std::max(1, settings->GetInt(key, layer.samples));

        StringCchPrintf(key, _countof(key), L"Shadow%dColor", index + 1);
        std::unique_ptr<IColorVal> defaultColor(Color::Create(Color::D2DToARGB(layer.color)));
        std::unique_ptr<IColorVal> shadowColor(settings->GetColor(key, defaultColor.get()));
        if (shadowColor) {
            layer.color = Color::ARGBToD2D(shadowColor->Evaluate());
        }

        StringCchPrintf(key, _countof(key), L"Shadow%dEnabled", index + 1);
        WCHAR enabledBuffer[MAX_LINE_LENGTH];
        if (settings->GetLine(key, enabledBuffer, _countof(enabledBuffer), nullptr) && enabledBuffer[0] != L'\0') {
            layer.enabled = LiteStep::ParseBool(enabledBuffer);
        } else {
            layer.enabled = layer.enabled || layer.opacity > 0.0f;
        }
        if (layer.scaleX <= 0.0f) {
            layer.scaleX = 1.0f;
        }
        if (layer.scaleY <= 0.0f) {
            layer.scaleY = 1.0f;
        }
        if (layer.samples <= 0) {
            layer.samples = 1;
        }
        if (layer.opacity <= 0.0f) {
            layer.enabled = false;
        }

        shadowLayers.push_back(layer);
    }

    settings->GetString(L"Font", buffer, _countof(buffer), defaults->font);
    StringCchCopy(this->font, _countof(this->font), buffer);
    this->fontSize = settings->GetFloat(L"FontSize", defaults->fontSize);

    settings->GetString(L"FontStretch", buffer, _countof(buffer), fontStretchMap.GetByA(defaults->fontStretch, L"Normal"));
    this->fontStretch = ParseFontStretch(buffer);

    settings->GetString(L"FontStyle", buffer, _countof(buffer), fontStyleMap.GetByA(defaults->fontStyle, L"Normal"));
    this->fontStyle = ParseFontStyle(buffer);

    settings->GetString(L"FontWeight", buffer, _countof(buffer), fontWeightMap.GetByA(defaults->fontWeight, L"Regular"));
    this->fontWeight = ParseFontWeight(buffer);

    this->outlineWidth = settings->GetFloat(L"OutlineWidth", defaults->outlineWidth);

    settings->GetString(L"ReadingDirection", buffer, _countof(buffer), readingDirectionMap.GetByA(defaults->readingDirection, L"LeftToRight"));
    this->readingDirection = ParseReadingDirection(buffer);

    settings->GetString(L"TextAlign", buffer, _countof(buffer), textAlignmentMap.GetByA(defaults->textAlign, L"Left"));
    this->textAlign = ParseTextAlignment(buffer);

    this->textOffsetBottom = settings->GetFloat(L"TextOffsetBottom", defaults->textOffsetBottom);
    this->textOffsetLeft = settings->GetFloat(L"TextOffsetLeft", defaults->textOffsetLeft);
    this->textOffsetRight = settings->GetFloat(L"TextOffsetRight", defaults->textOffsetRight);
    this->textOffsetTop = settings->GetFloat(L"TextOffsetTop", defaults->textOffsetTop);
    this->textRotation = settings->GetFloat(L"TextRotation", defaults->textRotation);

    this->fontStrokeWidth = settings->GetFloat(L"FontStrokeWidth", defaults->fontStrokeWidth);
    
    settings->GetString(L"TextTrimmingGranularity", buffer, _countof(buffer), trimmingGranularityMap.GetByA(defaults->textTrimmingGranularity, L"Character"));
    this->textTrimmingGranularity = ParseTrimmingGranularity(buffer);
    
    settings->GetString(L"TextVerticalAlign", buffer, _countof(buffer), paragraphAlignmentMap.GetByA(defaults->textVerticalAlign, L"Top"));
    this->textVerticalAlign = ParseParagraphAlignment(buffer);

    settings->GetString(L"WordWrapping", buffer, _countof(buffer), wordWrappingMap.GetByA(defaults->wordWrapping, L"NoWrap"));
    this->wordWrapping = ParseWordWrapping(buffer);

    // Load brushes
    this->brushSettings[State::BrushType::Background].Load(settings, &defaults->brushSettings[State::BrushType::Background]);
    
    ::Settings* outlineSettings = settings->CreateChild(L"Outline");
    this->brushSettings[State::BrushType::Outline].Load(outlineSettings, &defaults->brushSettings[State::BrushType::Outline]);

    ::Settings* textSettings = settings->CreateChild(L"Font");
    this->brushSettings[State::BrushType::Text].Load(textSettings, &defaults->brushSettings[State::BrushType::Text]);

    ::Settings* textOutlineSettings = textSettings->CreateChild(L"Stroke");
    this->brushSettings[State::BrushType::TextStroke].Load(textOutlineSettings, &defaults->brushSettings[State::BrushType::TextStroke]);
    
    delete outlineSettings;
    delete textSettings;
    delete textOutlineSettings;
}


DWRITE_FONT_STRETCH State::Settings::ParseFontStretch(LPCTSTR fontStretch)
{
    return fontStretchMap.GetByB(fontStretch, DWRITE_FONT_STRETCH_NORMAL);
}


DWRITE_FONT_STYLE State::Settings::ParseFontStyle(LPCTSTR fontStyle)
{
    return fontStyleMap.GetByB(fontStyle, DWRITE_FONT_STYLE_NORMAL);
}


DWRITE_FONT_WEIGHT State::Settings::ParseFontWeight(LPCTSTR weight)
{
    return fontWeightMap.GetByB(weight, DWRITE_FONT_WEIGHT_NORMAL);
}


DWRITE_TEXT_ALIGNMENT State::Settings::ParseTextAlignment(LPCTSTR textAlignment)
{
    return textAlignmentMap.GetByB(textAlignment, DWRITE_TEXT_ALIGNMENT_LEADING);
}


DWRITE_PARAGRAPH_ALIGNMENT State::Settings::ParseParagraphAlignment(LPCTSTR paragraphAlignment)
{
    return paragraphAlignmentMap.GetByB(paragraphAlignment, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}


DWRITE_TRIMMING_GRANULARITY State::Settings::ParseTrimmingGranularity(LPCTSTR trimmingGranularity)
{
    return trimmingGranularityMap.GetByB(trimmingGranularity, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
}


DWRITE_READING_DIRECTION State::Settings::ParseReadingDirection(LPCTSTR readingDirection)
{
    return readingDirectionMap.GetByB(readingDirection, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
}


DWRITE_WORD_WRAPPING State::Settings::ParseWordWrapping(LPCTSTR wordWrapping)
{
    return wordWrappingMap.GetByB(wordWrapping, DWRITE_WORD_WRAPPING_NO_WRAP);
}


