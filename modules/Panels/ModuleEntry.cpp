#include "Version.h"

#include "../ModuleKit/LiteStep.h"
#include "../ModuleKit/LSModule.hpp"
#include "../ModuleKit/Settings.hpp"
#include "../ModuleKit/Distance.hpp"
#include "../ModuleKit/ErrorHandler.h"
#include "../ModuleKit/Window.hpp"
#include "../ModuleKit/MonitorInfo.hpp"
#include "../Utilities/AlgorithmExtension.h"
#include "../Utilities/StringUtils.h"
#include "../CoreCom/Core.h"

#include <d2d1.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <map>
#include <strsafe.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using std::array;
using std::max;
using std::min;
using std::wstring;
using std::unique_ptr;
using std::vector;

namespace
{
class PanelManager;
extern PanelManager gPanelManager;

// Helpers --------------------------------------------------------------------

inline wstring ToLower(wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

inline bool SplitDelimited(const wstring &input, wchar_t delimiter, vector<wstring> &out)
{
    size_t start = 0;
    bool any = false;
    while (start <= input.length())
    {
        size_t end = input.find(delimiter, start);
        if (end == wstring::npos)
        {
            end = input.length();
        }
        wstring token = StringUtils::TrimCopy(input.substr(start, end - start));
        if (!token.empty())
        {
            out.emplace_back(std::move(token));
            any = true;
        }
        start = end + 1;
    }
    return any;
}

struct Insets
{
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
};

Insets ParseInsets(const wstring &value, int defaultAll)
{
    Insets result{ defaultAll, defaultAll, defaultAll, defaultAll };
    if (value.empty())
    {
        return result;
    }

    vector<wstring> tokens;
    if (!SplitDelimited(value, L',', tokens))
    {
        SplitDelimited(value, L' ', tokens);
    }

    auto parseInt = [](const wstring &token, int fallback) -> int
    {
        try
        {
            return std::stoi(token);
        }
        catch (...)
        {
            return fallback;
        }
    };

    if (tokens.size() == 1)
    {
        int v = parseInt(tokens[0], defaultAll);
        result = { v, v, v, v };
    }
    else if (tokens.size() == 2)
    {
        int vertical = parseInt(tokens[0], defaultAll);
        int horizontal = parseInt(tokens[1], defaultAll);
        result = { horizontal, vertical, horizontal, vertical };
    }
    else if (tokens.size() == 3)
    {
        int top = parseInt(tokens[0], defaultAll);
        int horizontal = parseInt(tokens[1], defaultAll);
        int bottom = parseInt(tokens[2], defaultAll);
        result = { horizontal, top, horizontal, bottom };
    }
    else if (tokens.size() >= 4)
    {
        result.left = parseInt(tokens[0], defaultAll);
        result.top = parseInt(tokens[1], defaultAll);
        result.right = parseInt(tokens[2], defaultAll);
        result.bottom = parseInt(tokens[3], defaultAll);
    }

    return result;
}

inline int SnapToGrid(int value, int grid)
{
    if (grid <= 1)
    {
        return value;
    }
    return static_cast<int>(std::round(static_cast<float>(value) / grid) * grid);
}

inline float SnapToGrid(float value, int grid)
{
    if (grid <= 1)
    {
        return value;
    }
    return std::round(value / grid) * grid;
}

enum class HorizontalAlignment
{
    Start,
    Center,
    End,
    Stretch
};

enum class VerticalAlignment
{
    Start,
    Center,
    End,
    Stretch
};

// Panel base -----------------------------------------------------------------

class Panel
{
public:
    Panel(const wstring &name, unique_ptr<Settings> settings)
        : mName(name)
        , mSettings(std::move(settings))
    {
        mX = mSettings->GetDistance(L"X", Distance(0.0f));
        mY = mSettings->GetDistance(L"Y", Distance(0.0f));
        mWidth = mSettings->GetDistance(L"Width", Distance(0.0f, 1.0f));
        mHeight = mSettings->GetDistance(L"Height", Distance(0.0f));

        TCHAR paddingBuffer[MAX_LINE_LENGTH] = { 0 };
        mSettings->GetString(L"Padding", paddingBuffer, _countof(paddingBuffer), L"");
        mPadding = ParseInsets(paddingBuffer, mSettings->GetInt(L"PaddingAll", 0));
        const int grid = GridUnit();
        mPadding.left = SnapToGrid(mPadding.left, grid);
        mPadding.right = SnapToGrid(mPadding.right, grid);
        mPadding.top = SnapToGrid(mPadding.top, grid);
        mPadding.bottom = SnapToGrid(mPadding.bottom, grid);
    }

    virtual ~Panel() = default;

    const wstring &Name() const { return mName; }

    void Layout()
    {
        const MonitorInfo &monitors = nCore::FetchMonitorInfo();
        const MonitorInfo::Monitor &desktop = monitors.GetVirtualDesktop();

        float availableWidth = static_cast<float>(desktop.width);
        float availableHeight = static_cast<float>(desktop.height);

        float width = max(1.0f, mWidth.Evaluate(availableWidth));
        float height = max(1.0f, mHeight.Evaluate(availableHeight));
        float x = desktop.rect.left + mX.Evaluate(availableWidth);
        float y = desktop.rect.top + mY.Evaluate(availableHeight);

        D2D1_RECT_F bounds{ x, y, x + width, y + height };
        Arrange(bounds);
    }

protected:
    struct ChildConfig
    {
        wstring name;
        Insets margin{ 0, 0, 0, 0 };
        HorizontalAlignment hAlign{ HorizontalAlignment::Stretch };
        VerticalAlignment vAlign{ VerticalAlignment::Stretch };
    };

    float ApplyChild(Window *window,
        const D2D1_RECT_F &cell,
        const ChildConfig &config,
        int gridUnit) const
    {
        if (!window)
        {
            return 0.0f;
        }

        const float innerLeft = cell.left + static_cast<float>(config.margin.left);
        const float innerTop = cell.top + static_cast<float>(config.margin.top);
        const float innerRight = cell.right - static_cast<float>(config.margin.right);
        const float innerBottom = cell.bottom - static_cast<float>(config.margin.bottom);

        float availableWidth = max(innerRight - innerLeft, 0.0f);
        float availableHeight = max(innerBottom - innerTop, 0.0f);

        SIZE desired = { static_cast<LONG>(availableWidth), static_cast<LONG>(availableHeight) };
        if (config.hAlign != HorizontalAlignment::Stretch || config.vAlign != VerticalAlignment::Stretch)
        {
            window->GetDesiredSize(static_cast<int>(availableWidth), static_cast<int>(availableHeight), &desired);
        }

        float targetWidth = (config.hAlign == HorizontalAlignment::Stretch)
            ? availableWidth
            : min(availableWidth, static_cast<float>(max(desired.cx, 1L)));

        float targetHeight = (config.vAlign == VerticalAlignment::Stretch)
            ? availableHeight
            : min(availableHeight, static_cast<float>(max(desired.cy, 1L)));

        float targetX = innerLeft;
        switch (config.hAlign)
        {
        case HorizontalAlignment::Start:
            targetX = innerLeft;
            break;
        case HorizontalAlignment::Center:
            targetX = innerLeft + (availableWidth - targetWidth) / 2.0f;
            break;
        case HorizontalAlignment::End:
            targetX = innerRight - targetWidth;
            break;
        case HorizontalAlignment::Stretch:
            targetX = innerLeft;
            break;
        }

        float targetY = innerTop;
        switch (config.vAlign)
        {
        case VerticalAlignment::Start:
            targetY = innerTop;
            break;
        case VerticalAlignment::Center:
            targetY = innerTop + (availableHeight - targetHeight) / 2.0f;
            break;
        case VerticalAlignment::End:
            targetY = innerBottom - targetHeight;
            break;
        case VerticalAlignment::Stretch:
            targetY = innerTop;
            break;
        }

        targetX = SnapToGrid(targetX, gridUnit);
        targetY = SnapToGrid(targetY, gridUnit);
        targetWidth = max(1.0f, SnapToGrid(targetWidth, gridUnit));
        targetHeight = max(1.0f, SnapToGrid(targetHeight, gridUnit));

        window->SetPosition(targetX, targetY, targetWidth, targetHeight);
        window->Show(SW_SHOWNOACTIVATE);
        return targetWidth;
    }

    int GridUnit() const
    {
        return max(1, mSettings->GetInt(L"GridUnit", 8));
    }

    unique_ptr<Settings> CreateChildSettings(const wstring &childKey) const
    {
        wchar_t buffer[MAX_RCCOMMAND];
        StringCchPrintf(buffer, _countof(buffer), L"Child%s", childKey.c_str());
        return unique_ptr<Settings>(mSettings->CreateChild(buffer));
    }

    ChildConfig LoadChildConfig(const wstring &childName, Settings *childSettings) const
    {
        ChildConfig config;
        config.name = childName;

        TCHAR marginBuffer[MAX_LINE_LENGTH] = { 0 };
        childSettings->GetString(L"Margin", marginBuffer, _countof(marginBuffer), L"");
        config.margin = ParseInsets(marginBuffer, childSettings->GetInt(L"MarginAll", 0));
        int grid = GridUnit();
        config.margin.left = SnapToGrid(config.margin.left, grid);
        config.margin.right = SnapToGrid(config.margin.right, grid);
        config.margin.top = SnapToGrid(config.margin.top, grid);
        config.margin.bottom = SnapToGrid(config.margin.bottom, grid);

        TCHAR hAlignBuffer[MAX_LINE_LENGTH] = { 0 };
        childSettings->GetString(L"HAlign", hAlignBuffer, _countof(hAlignBuffer), L"stretch");
        wstring hAlignStr = ToLower(hAlignBuffer);
        if (hAlignStr == L"center" || hAlignStr == L"middle")
        {
            config.hAlign = HorizontalAlignment::Center;
        }
        else if (hAlignStr == L"end" || hAlignStr == L"right")
        {
            config.hAlign = HorizontalAlignment::End;
        }
        else if (hAlignStr == L"stretch" || hAlignStr == L"fill")
        {
            config.hAlign = HorizontalAlignment::Stretch;
        }
        else
        {
            config.hAlign = HorizontalAlignment::Start;
        }

        TCHAR vAlignBuffer[MAX_LINE_LENGTH] = { 0 };
        childSettings->GetString(L"VAlign", vAlignBuffer, _countof(vAlignBuffer), L"stretch");
        wstring vAlignStr = ToLower(vAlignBuffer);
        if (vAlignStr == L"center" || vAlignStr == L"middle")
        {
            config.vAlign = VerticalAlignment::Center;
        }
        else if (vAlignStr == L"end" || vAlignStr == L"bottom")
        {
            config.vAlign = VerticalAlignment::End;
        }
        else if (vAlignStr == L"stretch" || vAlignStr == L"fill")
        {
            config.vAlign = VerticalAlignment::Stretch;
        }
        else
        {
            config.vAlign = VerticalAlignment::Start;
        }

        return config;
    }

    virtual void Arrange(const D2D1_RECT_F &bounds) = 0;

    const wstring mName;
    unique_ptr<Settings> mSettings;
    Distance mX;
    Distance mY;
    Distance mWidth;
    Distance mHeight;
    Insets mPadding;
};

// Grid panel -----------------------------------------------------------------

class GridPanel : public Panel
{
public:
    GridPanel(const wstring &name, unique_ptr<Settings> settings, const vector<wstring> &initialChildren)
        : Panel(name, std::move(settings))
    {
        LoadTracks(L"Columns", mColumns);
        LoadTracks(L"Rows", mRows);

        mColumnSpacing = mSettings->GetInt(L"ColumnSpacing", 0);
        mRowSpacing = mSettings->GetInt(L"RowSpacing", 0);

        vector<wstring> childNames = initialChildren;
        if (childNames.empty())
        {
            TCHAR buffer[MAX_LINE_LENGTH] = { 0 };
            if (mSettings->GetString(L"Children", buffer, _countof(buffer), L""))
            {
                SplitDelimited(buffer, L',', childNames);
            }
        }

        size_t implicitColumn = 0;
        for (const wstring &nameToken : childNames)
        {
            wstring childName = StringUtils::TrimCopy(nameToken);
            if (childName.empty())
            {
                continue;
            }

            auto childSettings = CreateChildSettings(childName);
            GridChild child;
            child.base = LoadChildConfig(childName, childSettings.get());
            child.column = max(0, childSettings->GetInt(L"Column", static_cast<int>(implicitColumn)));
            child.row = max(0, childSettings->GetInt(L"Row", 0));
            child.columnSpan = max(1, childSettings->GetInt(L"ColumnSpan", 1));
            child.rowSpan = max(1, childSettings->GetInt(L"RowSpan", 1));
            mChildren.emplace_back(std::move(child));

            ++implicitColumn;
            if (!mColumns.empty())
            {
                implicitColumn %= mColumns.size();
            }
        }
    }

protected:
    struct Track
    {
        bool star{ false };
        float starWeight{ 0.0f };
        Distance length;
    };

    struct GridChild
    {
        ChildConfig base;
        int column{ 0 };
        int row{ 0 };
        int columnSpan{ 1 };
        int rowSpan{ 1 };
    };

    void LoadTracks(LPCWSTR key, vector<Track> &tracks)
    {
        TCHAR buffer[MAX_LINE_LENGTH] = { 0 };
        if (!mSettings->GetString(key, buffer, _countof(buffer), L""))
        {
            return;
        }
        vector<wstring> tokens;
        SplitDelimited(buffer, L',', tokens);
        for (const wstring &tokenRaw : tokens)
        {
            wstring token = StringUtils::TrimCopy(tokenRaw);
            if (token.empty())
            {
                continue;
            }

            Track track;
            if (token.back() == L'*')
            {
                track.star = true;
                wstring weightString = StringUtils::TrimCopy(token.substr(0, token.length() - 1));
                track.starWeight = weightString.empty() ? 1.0f : static_cast<float>(::_wtof(weightString.c_str()));
                track.starWeight = max(0.0f, track.starWeight);
            }
            else
            {
                Distance distance;
                if (!Distance::Parse(token.c_str(), distance))
                {
                    distance = Distance(static_cast<float>(::_wtof(token.c_str())));
                }
                track.star = false;
                track.starWeight = 0.0f;
                track.length = distance;
            }
            tracks.emplace_back(track);
        }
    }

    void Arrange(const D2D1_RECT_F &bounds) override
    {
        const int gridUnit = GridUnit();
        const float innerLeft = bounds.left + static_cast<float>(mPadding.left);
        const float innerTop = bounds.top + static_cast<float>(mPadding.top);
        const float innerWidth = max(bounds.right - bounds.left - static_cast<float>(mPadding.left + mPadding.right), 0.0f);
        const float innerHeight = max(bounds.bottom - bounds.top - static_cast<float>(mPadding.top + mPadding.bottom), 0.0f);

        vector<float> columnSizes = ResolveTrackSizes(mColumns, innerWidth, static_cast<float>(mColumnSpacing));
        vector<float> rowSizes = ResolveTrackSizes(mRows, innerHeight, static_cast<float>(mRowSpacing));

        vector<float> columnOffsets(columnSizes.size());
        float currentX = innerLeft;
        for (size_t i = 0; i < columnSizes.size(); ++i)
        {
            columnOffsets[i] = currentX;
            currentX += columnSizes[i];
            if (i + 1 < columnSizes.size())
            {
                currentX += mColumnSpacing;
            }
        }

        vector<float> rowOffsets(rowSizes.size());
        float currentY = innerTop;
        for (size_t i = 0; i < rowSizes.size(); ++i)
        {
            rowOffsets[i] = currentY;
            currentY += rowSizes[i];
            if (i + 1 < rowSizes.size())
            {
                currentY += mRowSpacing;
            }
        }

        for (const GridChild &child : mChildren)
        {
            Window *window = nCore::System::FindRegisteredWindow(child.base.name.c_str());
            if (!window)
            {
                continue;
            }

            const size_t columnStart = min(static_cast<size_t>(child.column), columnSizes.empty() ? 0 : columnSizes.size() - 1);
            const size_t rowStart = min(static_cast<size_t>(child.row), rowSizes.empty() ? 0 : rowSizes.size() - 1);

            size_t columnEnd = min(columnStart + static_cast<size_t>(child.columnSpan), columnSizes.size());
            if (columnEnd == 0)
            {
                columnEnd = 1;
            }
            size_t rowEnd = min(rowStart + static_cast<size_t>(child.rowSpan), rowSizes.size());
            if (rowEnd == 0)
            {
                rowEnd = 1;
            }

            float cellLeft = columnSizes.empty() ? innerLeft : columnOffsets[columnStart];
            float cellRight = cellLeft;
            for (size_t i = columnStart; i < columnEnd; ++i)
            {
                if (i > columnStart)
                {
                    cellRight += mColumnSpacing;
                }
                cellRight += columnSizes.empty() ? innerWidth : columnSizes[i];
            }

            float cellTop = rowSizes.empty() ? innerTop : rowOffsets[rowStart];
            float cellBottom = cellTop;
            for (size_t i = rowStart; i < rowEnd; ++i)
            {
                if (i > rowStart)
                {
                    cellBottom += mRowSpacing;
                }
                cellBottom += rowSizes.empty() ? innerHeight : rowSizes[i];
            }

            D2D1_RECT_F cellRect{ cellLeft, cellTop, cellRight, cellBottom };
            ApplyChild(window, cellRect, child.base, gridUnit);
        }
    }

    vector<float> ResolveTrackSizes(const vector<Track> &tracks, float available, float spacing) const
    {
        if (tracks.empty())
        {
            return { available };
        }

        const size_t count = tracks.size();
        vector<float> sizes(count, 0.0f);
        const float totalSpacing = spacing * static_cast<float>(count > 0 ? count - 1 : 0);
        float workingAvailable = max(available - totalSpacing, 0.0f);

        float fixedTotal = 0.0f;
        float starWeight = 0.0f;
        for (size_t i = 0; i < count; ++i)
        {
            if (tracks[i].star)
            {
                starWeight += max(0.0f, tracks[i].starWeight);
                continue;
            }
            Distance lengthCopy = tracks[i].length;
            float value = lengthCopy.Evaluate(workingAvailable);
            value = max(0.0f, SnapToGrid(value, GridUnit()));
            sizes[i] = value;
            fixedTotal += value;
        }

        float remaining = max(workingAvailable - fixedTotal, 0.0f);
        if (starWeight > 0.0f)
        {
            const float unit = remaining / starWeight;
            for (size_t i = 0; i < count; ++i)
            {
                if (!tracks[i].star)
                {
                    continue;
                }
                float value = unit * max(0.0f, tracks[i].starWeight);
                value = max(0.0f, SnapToGrid(value, GridUnit()));
                sizes[i] = value;
            }
        }
        else if (remaining > 0.0f && count > 0)
        {
            sizes.back() += remaining;
        }

        return sizes;
    }

private:
    vector<Track> mColumns;
    vector<Track> mRows;
    int mColumnSpacing{ 0 };
    int mRowSpacing{ 0 };
    vector<GridChild> mChildren;
};

// Stack panel ----------------------------------------------------------------

class StackPanel : public Panel
{
public:
    enum class Orientation { Horizontal, Vertical };

    StackPanel(const wstring &name, unique_ptr<Settings> settings, const vector<wstring> &initialChildren)
        : Panel(name, std::move(settings))
    {
        TCHAR orientationBuffer[MAX_LINE_LENGTH] = { 0 };
        mSettings->GetString(L"Orientation", orientationBuffer, _countof(orientationBuffer), L"horizontal");
        wstring orientationValue = ToLower(orientationBuffer);
        mOrientation = (orientationValue == L"vertical") ? Orientation::Vertical : Orientation::Horizontal;

        mSpacing = mSettings->GetInt(L"Spacing", 0);

        vector<wstring> childNames = initialChildren;
        if (childNames.empty())
        {
            TCHAR buffer[MAX_LINE_LENGTH] = { 0 };
            if (mSettings->GetString(L"Children", buffer, _countof(buffer), L""))
            {
                SplitDelimited(buffer, L',', childNames);
            }
        }

        for (const wstring &nameToken : childNames)
        {
            wstring childName = StringUtils::TrimCopy(nameToken);
            if (childName.empty())
            {
                continue;
            }

            auto childSettings = CreateChildSettings(childName);
            StackChild child;
            child.base = LoadChildConfig(childName, childSettings.get());
            child.fixedPrimarySize = childSettings->GetInt(L"Size", -1);
            child.fixedSecondarySize = childSettings->GetInt(L"CrossSize", -1);
            mChildren.emplace_back(std::move(child));
        }
    }

protected:
    struct StackChild
    {
        ChildConfig base;
        int fixedPrimarySize{ -1 };
        int fixedSecondarySize{ -1 };
    };

    void Arrange(const D2D1_RECT_F &bounds) override
    {
        const int gridUnit = GridUnit();
        const float innerLeft = bounds.left + static_cast<float>(mPadding.left);
        const float innerTop = bounds.top + static_cast<float>(mPadding.top);
        const float availableWidth = max(bounds.right - bounds.left - static_cast<float>(mPadding.left + mPadding.right), 0.0f);
        const float availableHeight = max(bounds.bottom - bounds.top - static_cast<float>(mPadding.top + mPadding.bottom), 0.0f);

        float cursorX = innerLeft;
        float cursorY = innerTop;

        for (const StackChild &child : mChildren)
        {
            Window *window = nCore::System::FindRegisteredWindow(child.base.name.c_str());
            if (!window)
            {
                continue;
            }

            SIZE desired{ static_cast<LONG>(availableWidth), static_cast<LONG>(availableHeight) };
            window->GetDesiredSize(static_cast<int>(availableWidth), static_cast<int>(availableHeight), &desired);

            if (mOrientation == Orientation::Horizontal)
            {
                float width = (child.fixedPrimarySize > 0) ? static_cast<float>(child.fixedPrimarySize) : static_cast<float>(desired.cx);

                D2D1_RECT_F cell{ cursorX, innerTop, cursorX + width, innerTop + availableHeight };
                ApplyChild(window, cell, child.base, gridUnit);
                cursorX += width + mSpacing;
            }
            else
            {
                float height = (child.fixedPrimarySize > 0) ? static_cast<float>(child.fixedPrimarySize) : static_cast<float>(desired.cy);

                D2D1_RECT_F cell{ innerLeft, cursorY, innerLeft + availableWidth, cursorY + height };
                ApplyChild(window, cell, child.base, gridUnit);
                cursorY += height + mSpacing;
            }
        }
    }

private:
    Orientation mOrientation{ Orientation::Horizontal };
    int mSpacing{ 0 };
    vector<StackChild> mChildren;
};

// Panel manager --------------------------------------------------------------

class PanelManager
{
public:
    void Initialize()
    {
        LoadPanels();
        LayoutAll();
        RegisterBangs();
    }

    void Reload()
    {
        mPanels.clear();
        LoadPanels();
        LayoutAll();
    }

    void LayoutAll()
    {
        for (auto &panel : mPanels)
        {
            panel->Layout();
        }
    }

    void HandleTimer()
    {
        LayoutAll();
    }

    void Shutdown()
    {
        UnregisterBangs();
        mPanels.clear();
    }

    void CreateGridPanelFromBang(LPCTSTR args)
    {
        vector<wstring> tokens;
        ParseBangTokens(args, tokens);
        if (tokens.empty())
        {
            return;
        }
        vector<wstring> children;
        if (tokens.size() > 1)
        {
            children.assign(tokens.begin() + 1, tokens.end());
        }
        CreateGridPanel(tokens[0], children);
        LayoutAll();
    }

    void CreateStackPanelFromBang(LPCTSTR args)
    {
        vector<wstring> tokens;
        ParseBangTokens(args, tokens);
        if (tokens.empty())
        {
            return;
        }
        vector<wstring> children;
        if (tokens.size() > 1)
        {
            children.assign(tokens.begin() + 1, tokens.end());
        }
        CreateStackPanel(tokens[0], children);
        LayoutAll();
    }

private:
    void RegisterBangs()
    {
        LiteStep::AddBangCommandEx(L"!PanelsRefresh", [](HWND, LPCTSTR, LPCTSTR) {
            gPanelManager.LayoutAll();
        });
        LiteStep::AddBangCommandEx(L"!PanelsGridPanel", [](HWND, LPCTSTR, LPCTSTR args) {
            gPanelManager.CreateGridPanelFromBang(args);
        });
        LiteStep::AddBangCommandEx(L"!PanelsStackPanel", [](HWND, LPCTSTR, LPCTSTR args) {
            gPanelManager.CreateStackPanelFromBang(args);
        });
    }

    void UnregisterBangs()
    {
        LiteStep::RemoveBangCommand(L"!PanelsRefresh");
        LiteStep::RemoveBangCommand(L"!PanelsGridPanel");
        LiteStep::RemoveBangCommand(L"!PanelsStackPanel");
    }

    void LoadPanels()
    {
        LiteStep::IterateOverLineTokens(L"*PanelsGridPanel", [](LPCTSTR line) {
            vector<wstring> tokens;
            ParseBangTokens(line, tokens);
            if (!tokens.empty())
            {
                vector<wstring> children;
                if (tokens.size() > 1)
                {
                    children.assign(tokens.begin() + 1, tokens.end());
                }
                gPanelManager.CreateGridPanel(tokens[0], children);
            }
        });

        LiteStep::IterateOverLineTokens(L"*PanelsStackPanel", [](LPCTSTR line) {
            vector<wstring> tokens;
            ParseBangTokens(line, tokens);
            if (!tokens.empty())
            {
                vector<wstring> children;
                if (tokens.size() > 1)
                {
                    children.assign(tokens.begin() + 1, tokens.end());
                }
                gPanelManager.CreateStackPanel(tokens[0], children);
            }
        });
    }

    void CreateGridPanel(const wstring &name, const vector<wstring> &children)
    {
        if (name.empty())
        {
            return;
        }
        wchar_t prefix[MAX_RCCOMMAND];
        StringCchPrintf(prefix, _countof(prefix), L"Panels%s", name.c_str());
        auto settings = unique_ptr<Settings>(new Settings(prefix));
        mPanels.emplace_back(std::make_unique<GridPanel>(name, std::move(settings), children));
    }

    void CreateStackPanel(const wstring &name, const vector<wstring> &children)
    {
        if (name.empty())
        {
            return;
        }
        wchar_t prefix[MAX_RCCOMMAND];
        StringCchPrintf(prefix, _countof(prefix), L"Panels%s", name.c_str());
        auto settings = unique_ptr<Settings>(new Settings(prefix));
        mPanels.emplace_back(std::make_unique<StackPanel>(name, std::move(settings), children));
    }

    static void ParseBangTokens(LPCTSTR args, vector<wstring> &out)
    {
        if (!args)
        {
            return;
        }
        wchar_t buffers[16][MAX_RCCOMMAND];
        LPTSTR tokenPtrs[16];
        for (size_t i = 0; i < _countof(tokenPtrs); ++i)
        {
            tokenPtrs[i] = buffers[i];
        }
        int count = LiteStep::CommandTokenize(args, tokenPtrs, static_cast<int>(_countof(tokenPtrs)), nullptr);
        for (int i = 0; i < count; ++i)
        {
            if (tokenPtrs[i])
            {
                out.emplace_back(StringUtils::TrimCopy(tokenPtrs[i]));
            }
        }
    }

    vector<std::unique_ptr<Panel>> mPanels;
};

PanelManager gPanelManager;
} // namespace

// Module globals --------------------------------------------------------------

static LSModule gLSModule(TEXT(MODULE_NAME), TEXT(MODULE_AUTHOR), MakeVersion(MODULE_VERSION));

// LiteStep entrypoints --------------------------------------------------------

EXPORT_CDECL(int) initModuleW(HWND parent, HINSTANCE instance, LPCWSTR path)
{
    UNREFERENCED_PARAMETER(path);

    if (!gLSModule.Initialize(parent, instance))
    {
        return 1;
    }

    if (!gLSModule.ConnectToCore(MakeVersion(CORE_VERSION)))
    {
        return 1;
    }

    gPanelManager.Initialize();
    return 0;
}

EXPORT_CDECL(void) quitModule(HINSTANCE)
{
    gPanelManager.Shutdown();
    gLSModule.DeInitalize();
}

LRESULT WINAPI LSMessageHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case LM_REFRESH:
        gPanelManager.Reload();
        return 0;
    case WM_TIMER:
        gPanelManager.HandleTimer();
        return 0;
    default:
        break;
    }
    return DefWindowProc(window, message, wParam, lParam);
}

