/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  WindowState.cpp
 *  The nModules Project
 *
 *  A state for a Window.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "Color.h"
#include "Factories.h"
#include "LiteStep.h"
#include "State.hpp"

#include "../Utilities/CommonD2D.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>
#include <Wincodec.h>


using namespace D2D1;
namespace {
  using CornerRadii = State::CornerRadii;
  using CornerRadius = State::CornerRadius;

  inline float ClampRadius(float value) {
    return value < 0.0f ? 0.0f : value;
  }

  CornerRadii NormalizeCornerRadii(const D2D1_RECT_F &rect, CornerRadii radii) {
    radii.topLeft.x = ClampRadius(radii.topLeft.x);
    radii.topLeft.y = ClampRadius(radii.topLeft.y);
    radii.topRight.x = ClampRadius(radii.topRight.x);
    radii.topRight.y = ClampRadius(radii.topRight.y);
    radii.bottomRight.x = ClampRadius(radii.bottomRight.x);
    radii.bottomRight.y = ClampRadius(radii.bottomRight.y);
    radii.bottomLeft.x = ClampRadius(radii.bottomLeft.x);
    radii.bottomLeft.y = ClampRadius(radii.bottomLeft.y);

    const float width = std::max(rect.right - rect.left, 0.0f);
    const float height = std::max(rect.bottom - rect.top, 0.0f);

    const float sumTop = radii.topLeft.x + radii.topRight.x;
    const float sumBottom = radii.bottomLeft.x + radii.bottomRight.x;
    const float sumLeft = radii.topLeft.y + radii.bottomLeft.y;
    const float sumRight = radii.topRight.y + radii.bottomRight.y;

    float scale = 1.0f;
    auto constrain = [&scale](float total, float limit) {
      if (total > 0.0f && limit > 0.0f) {
        scale = std::min(scale, limit / total);
      }
    };

    constrain(sumTop, width);
    constrain(sumBottom, width);
    constrain(sumLeft, height);
    constrain(sumRight, height);

    if (scale < 1.0f) {
      auto applyScale = [scale](CornerRadius &corner) {
        corner.x *= scale;
        corner.y *= scale;
      };
      applyScale(radii.topLeft);
      applyScale(radii.topRight);
      applyScale(radii.bottomRight);
      applyScale(radii.bottomLeft);
    }

    return radii;
  }

  HRESULT CreateRoundedGeometry(const D2D1_RECT_F &rect, const CornerRadii &radii, ID2D1Geometry **geometry) {
    if (geometry == nullptr) {
      return E_INVALIDARG;
    }

    *geometry = nullptr;

    ID2D1Factory *factory = nullptr;
    HRESULT hr = Factories::GetD2DFactory(reinterpret_cast<LPVOID*>(&factory));
    if (FAILED(hr) || factory == nullptr) {
      return hr;
    }

    ID2D1PathGeometry *path = nullptr;
    hr = factory->CreatePathGeometry(&path);
    if (FAILED(hr)) {
      return hr;
    }

    ID2D1GeometrySink *sink = nullptr;
    hr = path->Open(&sink);
    if (FAILED(hr)) {
      path->Release();
      return hr;
    }

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);

    auto addArc = [sink](D2D1_POINT_2F endPoint, const CornerRadius &corner) {
      if (corner.x <= 0.0f || corner.y <= 0.0f) {
        sink->AddLine(endPoint);
      } else {
        D2D1_ARC_SEGMENT arcSegment = {};
        arcSegment.point = endPoint;
        arcSegment.size = D2D1::SizeF(corner.x, corner.y);
        arcSegment.rotationAngle = 0.0f;
        arcSegment.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
        arcSegment.arcSize = D2D1_ARC_SIZE_SMALL;
        sink->AddArc(arcSegment);
      }
    };

    const CornerRadii normalized = NormalizeCornerRadii(rect, radii);

    D2D1_POINT_2F startPoint = D2D1::Point2F(rect.left + normalized.topLeft.x, rect.top);
    sink->BeginFigure(startPoint, D2D1_FIGURE_BEGIN_FILLED);

    sink->AddLine(D2D1::Point2F(rect.right - normalized.topRight.x, rect.top));
    addArc(D2D1::Point2F(rect.right, rect.top + normalized.topRight.y), normalized.topRight);

    sink->AddLine(D2D1::Point2F(rect.right, rect.bottom - normalized.bottomRight.y));
    addArc(D2D1::Point2F(rect.right - normalized.bottomRight.x, rect.bottom), normalized.bottomRight);

    sink->AddLine(D2D1::Point2F(rect.left + normalized.bottomLeft.x, rect.bottom));
    addArc(D2D1::Point2F(rect.left, rect.bottom - normalized.bottomLeft.y), normalized.bottomLeft);

    sink->AddLine(D2D1::Point2F(rect.left, rect.top + normalized.topLeft.y));
    addArc(D2D1::Point2F(rect.left + normalized.topLeft.x, rect.top), normalized.topLeft);

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    hr = sink->Close();
    sink->Release();

    if (FAILED(hr)) {
      path->Release();
      return hr;
    }

    *geometry = path;
    return S_OK;
  }

  std::wstring NormalizePresetName(const std::wstring &name) {
    std::wstring result;
    result.reserve(name.size());
    for (wchar_t ch : name) {
      result.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return result;
  }

  State::ShadowLayer MakeShadowLayer(float offsetX, float offsetY, float blurRadius, float spread, float opacity, float scaleX, float scaleY, int samples, const D2D1_COLOR_F &color) {
    State::ShadowLayer layer;
    layer.offsetX = offsetX;
    layer.offsetY = offsetY;
    layer.blurRadius = blurRadius;
    layer.spread = spread;
    layer.opacity = opacity;
    layer.scaleX = scaleX;
    layer.scaleY = scaleY;
    layer.samples = std::max(1, samples);
    layer.color = color;
    layer.enabled = opacity > 0.0f;
    return layer;
  }

  void AddShadowPreset(std::unordered_map<std::wstring, std::vector<State::ShadowLayer>> &map, std::initializer_list<std::wstring> names, const std::vector<State::ShadowLayer> &layers) {
    for (const auto &name : names) {
      map.emplace(name, layers);
    }
  }

  const std::unordered_map<std::wstring, std::vector<State::ShadowLayer>>& GetShadowPresetMap() {
    static const std::unordered_map<std::wstring, std::vector<State::ShadowLayer>> presets = []() {
      std::unordered_map<std::wstring, std::vector<State::ShadowLayer>> map;

      AddShadowPreset(map, { L"none", L"shadowless" }, {});

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 6.0f, 18.0f, 0.15f, 0.28f, 1.02f, 1.05f, 24, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        layers.push_back(MakeShadowLayer(0.0f, 20.0f, 40.0f, 0.32f, 0.18f, 1.10f, 1.28f, 36, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.95f)));
        AddShadowPreset(map, { L"macos", L"macos-floating", L"float" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 8.0f, 36.0f, 0.22f, 0.24f, 1.08f, 1.12f, 20, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        AddShadowPreset(map, { L"windows", L"windows11", L"win11" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 4.0f, 12.0f, 0.08f, 0.32f, 1.02f, 1.05f, 24, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        layers.push_back(MakeShadowLayer(0.0f, 18.0f, 48.0f, 0.30f, 0.22f, 1.12f, 1.24f, 48, D2D1::ColorF(0.02f, 0.02f, 0.03f, 1.0f)));
        layers.push_back(MakeShadowLayer(0.0f, 36.0f, 72.0f, 0.42f, 0.12f, 1.24f, 1.42f, 32, D2D1::ColorF(0.08f, 0.08f, 0.12f, 1.0f)));
        AddShadowPreset(map, { L"raytraced", L"ray" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 10.0f, 36.0f, 0.20f, 0.26f, 1.04f, 1.18f, 28, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        AddShadowPreset(map, { L"material", L"material-high" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 14.0f, 32.0f, 0.18f, 0.32f, 1.06f, 1.18f, 28, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        layers.push_back(MakeShadowLayer(0.0f, 34.0f, 56.0f, 0.28f, 0.18f, 1.15f, 1.36f, 36, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.92f)));
        AddShadowPreset(map, { L"design1-panel", L"design-panel", L"panel" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 18.0f, 52.0f, 0.32f, 0.30f, 1.28f, 1.12f, 36, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.95f)));
        layers.push_back(MakeShadowLayer(0.0f, 32.0f, 78.0f, 0.48f, 0.16f, 1.42f, 1.20f, 40, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.8f)));
        AddShadowPreset(map, { L"design1-dock", L"design-dock", L"dock" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 6.0f, 18.0f, 0.18f, 0.30f, 1.12f, 1.04f, 24, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        AddShadowPreset(map, { L"design1-pill", L"pill", L"chip" }, layers);
      }

      {
        std::vector<State::ShadowLayer> layers;
        layers.push_back(MakeShadowLayer(0.0f, 10.0f, 26.0f, 0.22f, 0.28f, 1.08f, 1.06f, 24, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)));
        layers.push_back(MakeShadowLayer(0.0f, 24.0f, 48.0f, 0.28f, 0.18f, 1.16f, 1.24f, 32, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.9f)));
        AddShadowPreset(map, { L"design1-tray", L"design-tray", L"tray" }, layers);
      }

      return map;
    }();
    return presets;
  }
}



State::State()
: settings(nullptr)
, textFormat(nullptr)
, mTextRender(new StateTextRender(this)) {
  this->settings = nullptr;
  this->textFormat = nullptr;
}


State::~State() {
  DiscardDeviceResources();
  SAFEDELETE(this->settings);
  SAFERELEASE(this->textFormat);
  SAFERELEASE(mTextRender);
}


void State::UpdatePosition(D2D1_RECT_F position, WindowData *windowData) {
  windowData->drawingArea.rect = position;

  windowData->textRotationOrigin.x = position.left + (position.right - position.left) / 2.0f;
  windowData->textRotationOrigin.y = position.top + (position.bottom - position.top) / 2.0f;

  windowData->textArea = windowData->drawingArea.rect;
  windowData->textArea.bottom -= mStateSettings.textOffsetBottom;
  windowData->textArea.top += mStateSettings.textOffsetTop;
  windowData->textArea.left += mStateSettings.textOffsetLeft;
  windowData->textArea.right -= mStateSettings.textOffsetRight;

  if (windowData->textLayout) {
    windowData->textLayout->SetMaxHeight(windowData->textArea.bottom - windowData->textArea.top);
    windowData->textLayout->SetMaxWidth(windowData->textArea.right - windowData->textArea.left);
  }

  windowData->drawingArea.rect.left += mStateSettings.outlineWidth / 2.0f;
  windowData->drawingArea.rect.right -= mStateSettings.outlineWidth / 2.0f;
  windowData->drawingArea.rect.top += mStateSettings.outlineWidth / 2.0f;
  windowData->drawingArea.rect.bottom -= mStateSettings.outlineWidth / 2.0f;

  if (windowData->shapeGeometry) {
    windowData->shapeGeometry->Release();
    windowData->shapeGeometry = nullptr;
  }

  windowData->effectiveCornerRadii = NormalizeCornerRadii(windowData->drawingArea.rect, mStateSettings.cornerRadii);
  windowData->shadowLayers = mStateSettings.shadowLayers;

  if ((windowData->drawingArea.rect.right - windowData->drawingArea.rect.left) > 0.0f &&
      (windowData->drawingArea.rect.bottom - windowData->drawingArea.rect.top) > 0.0f) {
    ID2D1Geometry *geometry = nullptr;
    if (SUCCEEDED(CreateRoundedGeometry(windowData->drawingArea.rect, windowData->effectiveCornerRadii, &geometry))) {
      windowData->shapeGeometry = geometry;
    }
  }

  windowData->drawingArea.radiusX = mStateSettings.cornerRadiusX;
  windowData->drawingArea.radiusY = mStateSettings.cornerRadiusY;
  windowData->outlineArea = windowData->drawingArea;

  for (BrushType type = BrushType(0); type != BrushType::Count; EnumIncrement(type)) {
    mBrushes[type].UpdatePosition(windowData->drawingArea.rect, &windowData->brushData[type]);
  }
}



void State::Load(const Settings * defaultSettings, const ::Settings * settings, LPCTSTR name) {
  ASSERT(!this->settings);
  this->settings = settings;
  mName = name;

  mStateSettings.Load(this->settings, defaultSettings);

  for (BrushType type = BrushType(0); type != BrushType::Count; EnumIncrement(type)) {
    mBrushes[type].Load(&mStateSettings.brushSettings[type]);
  }

  CreateTextFormat(this->textFormat);
}


void State::DiscardDeviceResources() {
  for (Brush & brush : mBrushes) {
    brush.Discard();
  }
}


void State::Paint(ID2D1RenderTarget* renderTarget, WindowData *windowData) {
  for (const ShadowLayer &shadow : windowData->shadowLayers) {
    RenderShadowLayer(renderTarget, windowData, shadow);
  }
  if (mBrushes[BrushType::Background].brush) {
    if (mBrushes[BrushType::Background].IsImageEdgeBrush()) {
      for (Brush::EdgeType type = Brush::EdgeType(0); type != Brush::EdgeType::Count;
        type = Brush::EdgeType(std::underlying_type<Brush::EdgeType>::type(type) + 1)) {
        renderTarget->FillRectangle(mBrushes[BrushType::Background].GetImageEdgeRectAndScaleBrush(type,
          &windowData->brushData[BrushType::Background]), mBrushes[BrushType::Background].brush);
      }
    } else {
      mBrushes[BrushType::Background].brush->SetTransform(windowData->brushData[BrushType::Background].brushTransform);
      if (windowData->shapeGeometry) {
        renderTarget->FillGeometry(windowData->shapeGeometry, mBrushes[BrushType::Background].brush);
      } else {
        renderTarget->FillRectangle(windowData->drawingArea.rect, mBrushes[BrushType::Background].brush);
      }
    }
  }
  if (mBrushes[BrushType::Outline].brush && mStateSettings.outlineWidth != 0) {
    mBrushes[BrushType::Outline].brush->SetTransform(windowData->brushData[BrushType::Outline].brushTransform);
    if (windowData->shapeGeometry) {
      renderTarget->DrawGeometry(windowData->shapeGeometry, mBrushes[BrushType::Outline].brush, mStateSettings.outlineWidth);
    } else {
      renderTarget->DrawRectangle(windowData->drawingArea.rect, mBrushes[BrushType::Outline].brush, mStateSettings.outlineWidth);
    }
  }
}


void State::PaintText(ID2D1RenderTarget* renderTarget, WindowData *windowData, Window *window) {
  if (mBrushes[BrushType::Text].brush && *window->GetText() != L'\0') {
    renderTarget->SetTransform(Matrix3x2F::Rotation(mStateSettings.textRotation, windowData->textRotationOrigin));
    mBrushes[BrushType::Text].brush->SetTransform(windowData->brushData[BrushType::Text].brushTransform);

    // TODO::Avoid re-creation of the layout here.
    if (!windowData->textLayout) {
      IDWriteFactory *dwFactory;
      if (SUCCEEDED(Factories::GetDWriteFactory((LPVOID*)&dwFactory))) {
        dwFactory->CreateTextLayout(window->GetText(), lstrlenW(window->GetText()), textFormat,
          windowData->textArea.right - windowData->textArea.left,
          windowData->textArea.bottom - windowData->textArea.top, &windowData->textLayout);
      }
    }

    if (windowData->textLayout) {
      windowData->textLayout->Draw(renderTarget, mTextRender, windowData->textArea.left, windowData->textArea.top);
    }

    renderTarget->SetTransform(Matrix3x2F::Identity());
  }
}


HRESULT State::ReCreateDeviceResources(ID2D1RenderTarget* renderTarget) {
  HRESULT hr = S_OK;

  for (Brush & brush : mBrushes) {
    RETURNONFAIL(hr, brush.ReCreate(renderTarget));
  }

  //this->drawingArea.rect = mBackBrush.brushPosition;
  return hr;
}


bool State::UpdateDWMColor(ARGB newColor, ID2D1RenderTarget *renderTarget) {
  bool ret = false;
  ret = mBrushes[BrushType::Background].UpdateDWMColor(newColor, renderTarget) || ret;
  ret = mBrushes[BrushType::Outline].UpdateDWMColor(newColor, renderTarget) || ret;
  ret = mBrushes[BrushType::Text].UpdateDWMColor(newColor, renderTarget) || ret;
  ret = mBrushes[BrushType::TextStroke].UpdateDWMColor(newColor, renderTarget) || ret;

  return ret;
}


/// <summary>
/// Gets the "Desired" size of the window, given the specified constraints.
/// </summary>
/// <param name="maxWidth">Out. The maximum width to return.</param>
/// <param name="maxHeight">Out. The maximum height to return.</param>
/// <param name="size">Out. The desired size will be placed in this SIZE.</param>
void State::GetDesiredSize(int maxWidth, int maxHeight, LPSIZE size, Window *window) {
  IDWriteFactory* factory = NULL;
  IDWriteTextLayout* textLayout = NULL;
  DWRITE_TEXT_METRICS metrics;
  maxWidth -= int(mStateSettings.textOffsetLeft + mStateSettings.textOffsetRight);
  maxHeight -= int(mStateSettings.textOffsetTop + mStateSettings.textOffsetBottom);

  Factories::GetDWriteFactory(reinterpret_cast<LPVOID*>(&factory));
  factory->CreateTextLayout(window->GetText(), lstrlenW(window->GetText()), this->textFormat, (float)maxWidth, (float)maxHeight, &textLayout);
  textLayout->GetMetrics(&metrics);
  SAFERELEASE(textLayout);

  size->cx = long(metrics.width + mStateSettings.textOffsetLeft + mStateSettings.textOffsetRight) + 1;
  size->cy = long(metrics.height + mStateSettings.textOffsetTop + mStateSettings.textOffsetBottom) + 1;
}


/// <summary>
/// Creates a textFormat based on the specified drawingSettings.
/// </summary>
/// <param name="drawingSettings">The settings to create the textformat with.</param>
/// <param name="textFormat">Out. The textformat.</param>
/// <returns>S_OK</returns>
HRESULT State::CreateTextFormat(IDWriteTextFormat *&textFormat) {
  // Create the text format
  IDWriteFactory *pDWFactory = nullptr;
  Factories::GetDWriteFactory(reinterpret_cast<LPVOID*>(&pDWFactory));
  pDWFactory->CreateTextFormat(
    mStateSettings.font,
    nullptr,
    mStateSettings.fontWeight,
    mStateSettings.fontStyle,
    mStateSettings.fontStretch,
    mStateSettings.fontSize,
    L"en-US",
    &textFormat);

  textFormat->SetTextAlignment(mStateSettings.textAlign);
  textFormat->SetParagraphAlignment(mStateSettings.textVerticalAlign);
  textFormat->SetWordWrapping(mStateSettings.wordWrapping);
  textFormat->SetReadingDirection(mStateSettings.readingDirection);

  // Set the trimming method
  DWRITE_TRIMMING trimmingOptions;
  trimmingOptions.delimiter = 0;
  trimmingOptions.delimiterCount = 0;
  trimmingOptions.granularity = mStateSettings.textTrimmingGranularity;
  textFormat->SetTrimming(&trimmingOptions, nullptr);

  return S_OK;
}


/// <summary>
/// Sets the text offsets for the specified state.
/// </summary>
/// <param name="left">The text offset from the left.</param>
/// <param name="top">The text offset from the top.</param>
/// <param name="right">The text offset from the right.</param>
/// <param name="bottom">The text offset from the bottom.</param>
/// <param name="state">The state to set the offsets for.</param>
void State::SetTextOffsets(float left, float top, float right, float bottom) {
  mStateSettings.textOffsetBottom = bottom;
  mStateSettings.textOffsetLeft = left;
  mStateSettings.textOffsetRight = right;
  mStateSettings.textOffsetTop = top;

  //this->textArea = this->drawingArea.rect;
  //this->textArea.bottom -= mStateSettings.textOffsetBottom;
  //this->textArea.top += mStateSettings.textOffsetTop;
  //this->textArea.left += mStateSettings.textOffsetLeft;
  //this->textArea.right -= mStateSettings.textOffsetRight;
}


void State::RenderShadowLayer(ID2D1RenderTarget* renderTarget, WindowData *windowData, const ShadowLayer &layer) const {
  if (!layer.enabled || layer.opacity <= 0.0f) {
    return;
  }

  const D2D1_RECT_F &rect = windowData->drawingArea.rect;
  D2D1_POINT_2F center = {
    (rect.left + rect.right) * 0.5f,
    (rect.top + rect.bottom) * 0.5f
  };

  ID2D1Geometry *geometry = windowData->shapeGeometry;

  const int samplesHint = layer.samples > 0 ? layer.samples : (layer.blurRadius > 0.0f ? static_cast<int>(std::min(64.0f, std::ceil(layer.blurRadius * 2.0f))) : 1);
  const int samples = std::max(1, samplesHint);

  std::vector<float> weights(samples, 1.0f / static_cast<float>(samples));
  if (layer.blurRadius > 0.0f && samples > 1) {
    const float sigma = std::max(layer.blurRadius, 0.5f);
    float sum = 0.0f;
    for (int i = 0; i < samples; ++i) {
      const float x = samples == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(samples - 1);
      const float distance = 1.0f - x;
      const float weight = std::exp(-(distance * distance) / (2.0f * sigma * sigma));
      weights[i] = weight;
      sum += weight;
    }
    if (sum > 0.0f) {
      for (float &value : weights) {
        value /= sum;
      }
    }
  }

  const float spreadScale = std::max(0.0f, layer.spread) + std::max(0.0f, layer.blurRadius) * 0.03f;
  const float scaleXBase = layer.scaleX <= 0.0f ? 1.0f : layer.scaleX;
  const float scaleYBase = layer.scaleY <= 0.0f ? 1.0f : layer.scaleY;

  D2D1_COLOR_F baseColor = layer.color;
  float opacity = layer.opacity;
  if (opacity < 0.0f) { opacity = 0.0f; }
  if (opacity > 1.0f) { opacity = 1.0f; }
  baseColor.a *= opacity;

  ID2D1SolidColorBrush *brush = nullptr;
  if (FAILED(renderTarget->CreateSolidColorBrush(baseColor, &brush)) || brush == nullptr) {
    return;
  }

  D2D1_MATRIX_3X2_F originalTransform;
  renderTarget->GetTransform(&originalTransform);

  for (int i = 0; i < samples; ++i) {
    const float position = samples == 1 ? 1.0f : static_cast<float>(i) / static_cast<float>(samples - 1);
    const float scaleFactor = 1.0f + spreadScale * position;
    const float scaleX = scaleXBase * scaleFactor;
    const float scaleY = scaleYBase * scaleFactor;

    D2D1_MATRIX_3X2_F transform =
      D2D1::Matrix3x2F::Translation(-center.x, -center.y) *
      D2D1::Matrix3x2F::Scale(scaleX, scaleY) *
      D2D1::Matrix3x2F::Translation(center.x + layer.offsetX, center.y + layer.offsetY);

    renderTarget->SetTransform(transform * originalTransform);

    D2D1_COLOR_F iterationColor = baseColor;
    iterationColor.a = baseColor.a * weights[i];
    brush->SetColor(iterationColor);

    if (geometry != nullptr) {
      renderTarget->FillGeometry(geometry, brush);
    } else {
      renderTarget->FillRectangle(rect, brush);
    }
  }

  renderTarget->SetTransform(originalTransform);
  brush->Release();
}


bool State::TryGetShadowPreset(const std::wstring &presetName, std::vector<ShadowLayer> &layersOut) {
  const auto &map = GetShadowPresetMap();
  std::wstring normalized = NormalizePresetName(presetName);
  auto it = map.find(normalized);
  if (it != map.end()) {
    layersOut = it->second;
    return true;
  }
  return false;
}


bool State::SetShadowPreset(const std::wstring &presetName) {
  return SetShadowPreset(std::vector<std::wstring>{ presetName });
}


bool State::SetShadowPreset(const std::vector<std::wstring> &presetNames) {
  if (presetNames.empty()) {
    mStateSettings.shadowLayers.clear();
    return true;
  }

  std::vector<ShadowLayer> combined;
  bool matched = false;
  combined.reserve(presetNames.size());

  for (const auto &name : presetNames) {
    std::vector<ShadowLayer> layers;
    if (TryGetShadowPreset(name, layers)) {
      combined.insert(combined.end(), layers.begin(), layers.end());
      matched = true;
    }
  }

  if (!matched) {
    return false;
  }

  mStateSettings.shadowLayers = combined;
  return true;
}


void State::ClearShadowLayers() {
  mStateSettings.shadowLayers.clear();
}


void State::SetCornerRadius(float radius) {
  SetCornerRadius(radius, radius);
}


void State::SetCornerRadius(float radiusX, float radiusY) {
  SetCornerRadius(Corner::TopLeft, radiusX, radiusY);
  SetCornerRadius(Corner::TopRight, radiusX, radiusY);
  SetCornerRadius(Corner::BottomRight, radiusX, radiusY);
  SetCornerRadius(Corner::BottomLeft, radiusX, radiusY);
}


void State::SetCornerRadius(Corner corner, float radius) {
  SetCornerRadius(corner, radius, radius);
}


void State::SetCornerRadius(Corner corner, float radiusX, float radiusY) {
  radiusX = std::max(radiusX, 0.0f);
  radiusY = std::max(radiusY, 0.0f);

  auto apply = [&](CornerRadius &target) {
    target.x = radiusX;
    target.y = radiusY;
  };

  switch (corner) {
  case Corner::TopLeft:
    apply(mStateSettings.cornerRadii.topLeft);
    break;
  case Corner::TopRight:
    apply(mStateSettings.cornerRadii.topRight);
    break;
  case Corner::BottomRight:
    apply(mStateSettings.cornerRadii.bottomRight);
    break;
  case Corner::BottomLeft:
    apply(mStateSettings.cornerRadii.bottomLeft);
    break;
  }

  mStateSettings.cornerRadiusX = mStateSettings.cornerRadii.topLeft.x;
  mStateSettings.cornerRadiusY = mStateSettings.cornerRadii.topLeft.y;
}


void State::SetCornerRadiusX(float radius) {
  radius = std::max(radius, 0.0f);
  mStateSettings.cornerRadiusX = radius;
  mStateSettings.cornerRadii.topLeft.x = radius;
  mStateSettings.cornerRadii.topRight.x = radius;
  mStateSettings.cornerRadii.bottomRight.x = radius;
  mStateSettings.cornerRadii.bottomLeft.x = radius;
}


void State::SetCornerRadiusY(float radius) {
  radius = std::max(radius, 0.0f);
  mStateSettings.cornerRadiusY = radius;
  mStateSettings.cornerRadii.topLeft.y = radius;
  mStateSettings.cornerRadii.topRight.y = radius;
  mStateSettings.cornerRadii.bottomRight.y = radius;
  mStateSettings.cornerRadii.bottomLeft.y = radius;
}


void State::SetOutlineWidth(float width) {
  mStateSettings.outlineWidth = width;
}


void State::SetReadingDirection(DWRITE_READING_DIRECTION direction) {
  this->textFormat->SetReadingDirection(direction);
}


void State::SetTextAlignment(DWRITE_TEXT_ALIGNMENT alignment) {
  this->textFormat->SetTextAlignment(alignment);
}


void State::SetTextRotation(float rotation) {
  mStateSettings.textRotation = rotation;
}


void State::SetTextTrimmingGranuality(DWRITE_TRIMMING_GRANULARITY granularity) {
  DWRITE_TRIMMING options;
  IDWriteInlineObject *trimmingSign;
  this->textFormat->GetTrimming(&options, &trimmingSign);
  options.granularity = granularity;
  this->textFormat->SetTrimming(&options, trimmingSign);
}


void State::SetTextVerticalAlign(DWRITE_PARAGRAPH_ALIGNMENT alignment) {
  this->textFormat->SetParagraphAlignment(alignment);
}


void State::SetWordWrapping(DWRITE_WORD_WRAPPING wrapping) {
  this->textFormat->SetWordWrapping(wrapping);
}


Brush* State::GetBrush(LPCTSTR brushName) {
  if (*brushName == L'\0') {
    return &mBrushes[BrushType::Background];
  } else if (_wcsicmp(brushName, L"Text") == 0) {
    return &mBrushes[BrushType::Text];
  } else if (_wcsicmp(brushName, L"Outline") == 0) {
    return &mBrushes[BrushType::Outline];
  } else if (_wcsicmp(brushName, L"TextStroke") == 0) {
    return &mBrushes[BrushType::TextStroke];
  }

  return nullptr;
}


