//--------------------------------------------------------------------------------------
// StateBangs.cpp
// The nModules Project
//
// Bangs for states
//
//--------------------------------------------------------------------------------------
#include "StateBangs.h"
#include "LiteStep.h"
#include <strsafe.h>
#include "../CoreCom/Core.h"


static std::function<Window* (LPCTSTR)> windowFinder;


static State *FindState(LPCTSTR *args, int numArgs, Window *&window) {
    TCHAR buffer[MAX_RCCOMMAND];
    int numTokens = LiteStep::CommandTokenize(*args, nullptr, 0, nullptr);

    if (numTokens == numArgs + 1 || numTokens == numArgs + 2) {
        LiteStep::GetToken(*args, buffer, args, 0);
        window = windowFinder(buffer);
        if (window) {
            if (numTokens == numArgs + 2) {
                LiteStep::GetToken(*args, buffer, args, 0);
                return window->GetState(buffer);
            }
            else {
                return window->GetState(L"");
            }
        }
    }

    return nullptr;
}


static void ApplyCornerRadiusAll(State *state, Window *window, LPCTSTR args) {
    TCHAR first[MAX_RCCOMMAND], second[MAX_RCCOMMAND];
    LPTSTR buffers[] = { first, second };
    int count = LiteStep::CommandTokenize(args, buffers, _countof(buffers), nullptr);
    if (count >= 1) {
        float radiusX = float(_wtof(first));
        float radiusY = count >= 2 ? float(_wtof(second)) : radiusX;
        state->SetCornerRadius(radiusX, radiusY);
        window->Repaint();
    }
}


static void ApplyCornerRadius(State *state, Window *window, State::Corner corner, LPCTSTR args) {
    TCHAR first[MAX_RCCOMMAND], second[MAX_RCCOMMAND];
    LPTSTR buffers[] = { first, second };
    int count = LiteStep::CommandTokenize(args, buffers, _countof(buffers), nullptr);
    if (count >= 1) {
        float radiusX = float(_wtof(first));
        float radiusY = count >= 2 ? float(_wtof(second)) : radiusX;
        state->SetCornerRadius(corner, radiusX, radiusY);
        window->Repaint();
    }
}


static bool ParseShadowPresetArgs(LPCTSTR args, std::vector<std::wstring> &names) {
    if (args == nullptr) {
        return false;
    }

    WCHAR buffer[MAX_LINE_LENGTH];
    StringCchCopy(buffer, _countof(buffer), args);

    WCHAR tokenBuffers[16][MAX_RCCOMMAND];
    LPTSTR tokenPtrs[16];
    for (int i = 0; i < 16; ++i) {
        tokenPtrs[i] = tokenBuffers[i];
    }

    int count = LiteStep::CommandTokenize(buffer, tokenPtrs, _countof(tokenPtrs), nullptr);
    names.clear();
    for (int i = 0; i < count; ++i) {
        if (tokenPtrs[i] != nullptr && tokenPtrs[i][0] != L'\0') {
            names.emplace_back(tokenPtrs[i]);
        }
    }
    return !names.empty();
}


static void ApplyShadowPreset(State *state, Window *window, LPCTSTR args) {
    std::vector<std::wstring> names;
    if (ParseShadowPresetArgs(args, names)) {
        if (state->SetShadowPreset(names)) {
            window->Repaint();
        }
    }
}


static struct BangItem {
    BangItem(LPCTSTR name, LiteStep::BANGCOMMANDPROC proc) {
        this->name = name;
        this->proc = proc;
    }

    LPCTSTR name;
    LiteStep::BANGCOMMANDPROC proc;
} BangMap [] = {
    BangItem(L"SetCornerRadius",            [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 2, window);
        if (state) {
            ApplyCornerRadiusAll(state, window, args);
        }
    }),
    BangItem(L"SetCornerRadiusX",           [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetCornerRadiusX(wcstof(arg, nullptr));
            window->Repaint();
        }
    }),
    BangItem(L"SetCornerRadiusY",           [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetCornerRadiusY(wcstof(arg, nullptr));
            window->Repaint();
        }
    }),
    BangItem(L"SetShadowPreset",            [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            ApplyShadowPreset(state, window, args);
        }
    }),
    BangItem(L"ClearShadowLayers",          [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 0, window);
        if (state) {
            state->ClearShadowLayers();
            window->Repaint();
        }
    }),
    BangItem(L"SetCornerRadiusTopLeft",     [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 2, window);
        if (state) {
            ApplyCornerRadius(state, window, State::Corner::TopLeft, args);
        }
    }),
    BangItem(L"SetCornerRadiusTopRight",    [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 2, window);
        if (state) {
            ApplyCornerRadius(state, window, State::Corner::TopRight, args);
        }
    }),
    BangItem(L"SetCornerRadiusBottomRight", [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 2, window);
        if (state) {
            ApplyCornerRadius(state, window, State::Corner::BottomRight, args);
        }
    }),
    BangItem(L"SetCornerRadiusBottomLeft",  [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 2, window);
        if (state) {
            ApplyCornerRadius(state, window, State::Corner::BottomLeft, args);
        }
    }),
    BangItem(L"SetOutlineWidth",            [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetOutlineWidth(wcstof(arg, nullptr));
            window->Repaint();
        }
    }),
    /*BangItem(L"SetFont",                    [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            //state->SetFont(arg);
        }
    }),
    BangItem(L"SetFontSize",                [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            //state->SetFontSize(strtof(arg, nullptr));
        }
    }),
    BangItem(L"SetFontStretch",             [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            //state->SetFontStretch(StateSettings::ParseFontStretch(arg));
        }
    }),
    BangItem(L"SetFontStyle",               [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            //state->SetFontStyle(StateSettings::ParseFontStyle(arg));
        }
    }),
    BangItem(L"SetFontWeight",              [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            //state->SetFontWeight(StateSettings::ParseFontWeight(arg));
        }
    }),*/
    BangItem(L"SetReadingDirection",        [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetReadingDirection(State::Settings::ParseReadingDirection(arg));
            window->Repaint();
        }
    }),
    BangItem(L"SetTextAlign",               [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetTextAlignment(State::Settings::ParseTextAlignment(arg));
            window->Repaint();
        }
    }),
    BangItem(L"SetTextOffset",              [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 4, window);
        if (state) {
            TCHAR left[MAX_RCCOMMAND], top[MAX_RCCOMMAND], right[MAX_RCCOMMAND], bottom[MAX_RCCOMMAND];
            LPTSTR buffers[] = { left, top, right, bottom };
            LiteStep::CommandTokenize(args, buffers, _countof(buffers), nullptr);
            state->SetTextOffsets(float(_wtof(left)), float(_wtof(top)), float(_wtof(right)), float(_wtof(bottom)));
            window->Repaint();
        }
    }),
    BangItem(L"SetTextRotation",            [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetTextRotation(float(wcstof(arg, nullptr)));
            window->Repaint();
        }
    }),
    BangItem(L"SetTextTrimmingGranularity", [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetTextTrimmingGranuality(State::Settings::ParseTrimmingGranularity(arg));
            window->Repaint();
        }
    }),
    BangItem(L"SetTextVerticalAlign",       [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetTextVerticalAlign(State::Settings::ParseParagraphAlignment(arg));
            window->Repaint();
        }
    }),
    BangItem(L"SetWordWrapping",            [] (HWND, LPCTSTR args) -> void {
        Window *window = nullptr;
        State *state = FindState(&args, 1, window);
        if (state) {
            TCHAR arg[MAX_RCCOMMAND];
            LiteStep::GetToken(args, arg, nullptr, 0);
            state->SetWordWrapping(State::Settings::ParseWordWrapping(arg));
            window->Repaint();
        }
    })
};


/// <summary>
/// Registers bangs.
/// </summary>
void StateBangs::Register(LPCTSTR prefix, std::function<Window* (LPCTSTR)> windowFinder) {
    TCHAR bangName[64];
    ::windowFinder = windowFinder;
    for (BangItem item : BangMap) {
        StringCchPrintf(bangName, _countof(bangName), L"!%s%s", prefix, item.name);
        LiteStep::AddBangCommand(bangName, item.proc);
    }
}


/// <summary>
/// Unregisters bangs.
/// </summary>
void StateBangs::UnRegister(LPCTSTR prefix) {
    TCHAR bangName[64];
    for (BangItem item : BangMap) {
        StringCchPrintf(bangName, _countof(bangName), L"!%s%s", prefix, item.name);
        LiteStep::RemoveBangCommand(bangName);
    }
    ::windowFinder = nullptr;
}




