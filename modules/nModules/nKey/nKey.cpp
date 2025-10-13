//-------------------------------------------------------------------------------------------------
// /nKey/nKey.cpp
// The nModules Project
//
// nKey entry points.
//-------------------------------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#include "Version.h"

#include "../nShared/ErrorHandler.h"
#include "../nShared/LiteStep.h"
#include "../nShared/LSModule.hpp"

#include "../Utilities/StringUtils.h"

#include <unordered_map>
#include <algorithm>
#include <cwctype>
#include <vector>
#include <Shlwapi.h>
#include <strsafe.h>

typedef std::unordered_map<int, std::wstring> HotkeyMap;
typedef StringKeyedMaps<std::wstring, UINT, CaseSensitive>::UnorderedMap VKMap;

static void LoadSettings();
static void LoadHotKeys();
static void LoadVKeyTable();
static std::pair<bool, LPCWSTR> AddHotkey(UINT mods, UINT key, LPCWSTR command);
static UINT ParseMods(LPCWSTR mods);
static UINT ParseKey(LPCWSTR key);
static UINT LookupDefaultVirtualKey(const std::wstring& keyName);

// The messages we want from the core
static UINT gLSMessages[] = { LM_GETREVID, LM_REFRESH, 0 };

// All hotkey mappings
static HotkeyMap gHotKeys;

// Definitions loaded from vk104.txt
static VKMap gVKCodes;

// Used for assigning hotkeys.
static int gId = 0;

// The LiteStep module class
static LSModule gLSModule(TEXT(MODULE_NAME), TEXT(MODULE_AUTHOR), MakeVersion(MODULE_VERSION));

struct WinHotkeyEntry
{
  UINT mods;
  UINT key;
  std::wstring command;
  bool active;
};

static std::vector<WinHotkeyEntry> gWinFallbackHotkeys;
static HHOOK gWinHotkeyHook = NULL;

static void EnsureWinHook();
static void ReleaseWinHook();
static bool AreModifiersSatisfied(UINT mods);
static bool IsModifierPressed(int leftVk, int rightVk);
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);


static void Load() {
  LoadVKeyTable();
  LoadHotKeys();
}


static void Unload() {
  for (auto &hotkey : gHotKeys) {
    UnregisterHotKey(gLSModule.GetMessageWindow(), hotkey.first);
  }
  gHotKeys.clear();
  gVKCodes.clear();
  ReleaseWinHook();
  gId = 0;
}


/// <summary>
/// Called by the LiteStep core when this module is loaded.
/// </summary>
/// <param name="parent"></param>
/// <param name="instance">Handle to this module's instance.</param>
/// <param name="path">Path to the LiteStep directory.</param>
/// <returns>0 on success, non-zero on error.</returns>
/// <remarks>
/// If this function returns non-zero, the module will be unloaded immediately, without
/// going through quitModule.
/// </remarks>
EXPORT_CDECL(int) initModuleW(HWND parent, HINSTANCE instance, LPCWSTR /* path */) {
  if (!gLSModule.Initialize(parent, instance)) {
    return 1;
  }
  Load();
  return 0;
}


/// <summary>
/// Called by the LiteStep core when this module is about to be unloaded.
/// </summary>
/// <param name="instance">Handle to this module's instance.</param>
EXPORT_CDECL(void) quitModule(HINSTANCE /* instance */) {
  Unload();
  gLSModule.DeInitalize();
}


/// <summary>
/// Handles the main window's messages.
/// </summary>
/// <param name="window">The window the message is for.</param>
/// <param name="message">The type of message.</param>
/// <param name="wParam">wParam</param>
/// <param name="lParam">lParam</param>
LRESULT WINAPI LSMessageHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  using LiteStep::GetLitestepWnd;
  switch(message) {
  case WM_CREATE:
    SendMessage(GetLitestepWnd(), LM_REGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
    return 0;

  case WM_DESTROY:
    SendMessage(GetLitestepWnd(), LM_UNREGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
    return 0;

  case LM_REFRESH:
    Unload();
    Load();
    return 0;

  case WM_HOTKEY:
    HotkeyMap::iterator hotkey = gHotKeys.find((int)wParam);
    if (hotkey != gHotKeys.end()) {
      LiteStep::LSExecute(window, hotkey->second.c_str(), 0);
    }
    return 0;
  }
  return DefWindowProc(window, message, wParam, lParam);
}


/// <summary>
/// Adds a hotkey.
/// </summary>
static std::pair<bool, LPCWSTR> AddHotkey(UINT mods, UINT key, LPCWSTR command) {
  if (mods == UINT(-1) || key == UINT(-1)) {
    return std::make_pair(false, L"Invalid modifiers or key.");
  }

  // Register the hotkey
  if (RegisterHotKey(gLSModule.GetMessageWindow(), gId, mods, key) == FALSE) {
    DWORD error = GetLastError();

    if ((mods & MOD_WIN) != 0) {
      EnsureWinHook();
      if (gWinHotkeyHook != NULL) {
        WinHotkeyEntry entry;
        entry.mods = mods;
        entry.key = key;
        entry.command = command;
        entry.active = false;
        gWinFallbackHotkeys.push_back(std::move(entry));
        return std::make_pair(true, nullptr);
      }
    }

    if (error == ERROR_HOTKEY_ALREADY_REGISTERED) {
      return std::make_pair(false, L"Failed to register the hotkey. Probably already taken.");
    }

    return std::make_pair(false, L"Failed to register the hotkey.");
  }

  // Add the hotkey definition to the map
  gHotKeys[gId++] = command;

  return std::make_pair(true, nullptr);
}


/// <summary>
/// Loads VK definitions
/// </summary>
static void LoadVKeyTable() {
  FILE *file;
  WCHAR path[MAX_PATH], line[256], name[256], code[64];
  LPWSTR tokens[] = { name, code };
  LPWSTR endPtr;
  UINT vkey;

  if (LiteStep::GetRCLine(L"nKeyVKTable", path, _countof(path), L"") != 0) {
    PathUnquoteSpaces(path);
    errno_t result = _wfopen_s(&file, path, L"rt, ccs=UTF-8");
    if (result == 0) {
      while (fgetws(line, _countof(line), file) != nullptr) {
        LPCWSTR firstChar = &line[wcsspn(line, L" \t\n\r")];
        if (*firstChar == L';' || *firstChar == L'\0') {
          continue;
        }
        if (LiteStep::LCTokenize(line, tokens, 2, nullptr) == 2) {
          _wcsupr_s(name, _countof(name));
          vkey = wcstoul(code, &endPtr, 0);
          if (*code != L'\0' && (*endPtr == L'\0' || *endPtr == L';')) {
            gVKCodes[name] = vkey;
          } else {
            ErrorHandler::Error(ErrorHandler::Level::Warning,
              L"Invalid line in nKeyVKTable.\n%ls", line);
          }
        } else {
          ErrorHandler::Error(ErrorHandler::Level::Warning,
            L"Invalid line in nKeyVKTable.\n%ls", line);
        }
      }
      fclose(file);
    } else {
      ErrorHandler::Error(ErrorHandler::Level::Warning,
        L"Unable to open nKeyVKTable, %ls\n%ls", _wcserror(result), path);
    }
  }
}


/// <summary>
/// Reads through the .rc files and load *HotKeys
/// </summary>
static void LoadHotKeys() {
  WCHAR line[MAX_LINE_LENGTH], mods[128], key[128], command[MAX_LINE_LENGTH];
  LPWSTR tokens[] = { mods, key };
  LPVOID f = LiteStep::LCOpenW(nullptr);

  while (LiteStep::LCReadNextConfigW(f, L"*HotKey", line, _countof(line))) {
    LiteStep::LCTokenizeW(line + _countof("*HotKey"), tokens, 2, command);

    // ParseMods expects szMods to be all lowercase.
    _wcslwr_s(mods, _countof(mods));
    std::pair<bool, LPCWSTR> result = AddHotkey(ParseMods(mods), ParseKey(key), command);
    if (!result.first) {
      ErrorHandler::Error(ErrorHandler::Level::Warning,
        L"Error while registering hotkey %ls %ls.\n%ls", mods, key, result.second);
    }
  }

  LiteStep::LCClose(f);
}


/// <summary>
/// String -> Virtual Key Code
/// </summary>
static UINT ParseKey(LPCWSTR key) {
  if (key == nullptr || *key == L'\0') {
    return UINT(-1);
  }

  const size_t length = wcslen(key);

  if (length == 1) {
    SHORT scan = VkKeyScanW(key[0]);
    if (scan == -1) {
      return UINT(-1);
    }
    return static_cast<UINT>(scan & 0xFF);
  }

  std::wstring lookup(key);
  std::transform(lookup.begin(), lookup.end(), lookup.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(towupper(static_cast<unsigned short>(ch)));
  });

  VKMap::const_iterator vk = gVKCodes.find(lookup);
  if (vk != gVKCodes.end()) {
    return vk->second;
  }

  UINT fallback = LookupDefaultVirtualKey(lookup);
  if (fallback != UINT(-1)) {
    return fallback;
  }

  wchar_t* endPtr = nullptr;
  unsigned long numeric = wcstoul(lookup.c_str(), &endPtr, 0);
  if (endPtr != nullptr && *endPtr == L'\0' && numeric <= 0xFFFF) {
    return static_cast<UINT>(numeric);
  }

  return UINT(-1);
}

static UINT LookupDefaultVirtualKey(const std::wstring& keyName) {
  static const std::unordered_map<std::wstring, UINT> defaultKeys = []() {
    std::unordered_map<std::wstring, UINT> map;
    map.reserve(128);
    auto add = [&map](const wchar_t* name, UINT code) {
      map.emplace(name, code);
    };

    add(L"SPACE", VK_SPACE);
    add(L"VK_SPACE", VK_SPACE);
    add(L"TAB", VK_TAB);
    add(L"VK_TAB", VK_TAB);
    add(L"ENTER", VK_RETURN);
    add(L"RETURN", VK_RETURN);
    add(L"VK_RETURN", VK_RETURN);
    add(L"ESC", VK_ESCAPE);
    add(L"ESCAPE", VK_ESCAPE);
    add(L"VK_ESCAPE", VK_ESCAPE);
    add(L"BACKSPACE", VK_BACK);
    add(L"BACK", VK_BACK);
    add(L"VK_BACK", VK_BACK);
    add(L"DELETE", VK_DELETE);
    add(L"DEL", VK_DELETE);
    add(L"VK_DELETE", VK_DELETE);
    add(L"INSERT", VK_INSERT);
    add(L"INS", VK_INSERT);
    add(L"VK_INSERT", VK_INSERT);
    add(L"HOME", VK_HOME);
    add(L"END", VK_END);
    add(L"PGUP", VK_PRIOR);
    add(L"PAGEUP", VK_PRIOR);
    add(L"VK_PRIOR", VK_PRIOR);
    add(L"PGDN", VK_NEXT);
    add(L"PAGEDOWN", VK_NEXT);
    add(L"VK_NEXT", VK_NEXT);
    add(L"UP", VK_UP);
    add(L"DOWN", VK_DOWN);
    add(L"LEFT", VK_LEFT);
    add(L"RIGHT", VK_RIGHT);
    add(L"CAPSLOCK", VK_CAPITAL);
    add(L"CAPS", VK_CAPITAL);
    add(L"NUMLOCK", VK_NUMLOCK);
    add(L"SCROLLLOCK", VK_SCROLL);
    add(L"SCROLL", VK_SCROLL);
    add(L"PAUSE", VK_PAUSE);
    add(L"BREAK", VK_PAUSE);
    add(L"PRINTSCREEN", VK_SNAPSHOT);
    add(L"PRTSC", VK_SNAPSHOT);
    add(L"VK_SNAPSHOT", VK_SNAPSHOT);
    add(L"APPS", VK_APPS);
    add(L"MENU", VK_APPS);
    add(L"LWIN", VK_LWIN);
    add(L"RWIN", VK_RWIN);
    add(L"BROWSER_BACK", VK_BROWSER_BACK);
    add(L"BROWSER_FORWARD", VK_BROWSER_FORWARD);
    add(L"BROWSER_REFRESH", VK_BROWSER_REFRESH);
    add(L"BROWSER_STOP", VK_BROWSER_STOP);
    add(L"BROWSER_SEARCH", VK_BROWSER_SEARCH);
    add(L"BROWSER_FAVORITES", VK_BROWSER_FAVORITES);
    add(L"BROWSER_HOME", VK_BROWSER_HOME);
    add(L"VOLUME_MUTE", VK_VOLUME_MUTE);
    add(L"VOLUME_DOWN", VK_VOLUME_DOWN);
    add(L"VOLUME_UP", VK_VOLUME_UP);
    add(L"MEDIA_NEXT", VK_MEDIA_NEXT_TRACK);
    add(L"MEDIA_PREV", VK_MEDIA_PREV_TRACK);
    add(L"MEDIA_STOP", VK_MEDIA_STOP);
    add(L"MEDIA_PLAY", VK_MEDIA_PLAY_PAUSE);
    add(L"LAUNCH_MAIL", VK_LAUNCH_MAIL);
    add(L"LAUNCH_MEDIA", VK_LAUNCH_MEDIA_SELECT);
    add(L"LAUNCH_APP1", VK_LAUNCH_APP1);
    add(L"LAUNCH_APP2", VK_LAUNCH_APP2);
    add(L"OEM_PLUS", VK_OEM_PLUS);
    add(L"OEM_MINUS", VK_OEM_MINUS);
    add(L"OEM_COMMA", VK_OEM_COMMA);
    add(L"OEM_PERIOD", VK_OEM_PERIOD);
    add(L"OEM_1", VK_OEM_1);
    add(L"OEM_2", VK_OEM_2);
    add(L"OEM_3", VK_OEM_3);
    add(L"OEM_4", VK_OEM_4);
    add(L"OEM_5", VK_OEM_5);
    add(L"OEM_6", VK_OEM_6);
    add(L"OEM_7", VK_OEM_7);
    add(L"OEM_8", VK_OEM_8);
    add(L"OEM_102", VK_OEM_102);
    add(L"NUMPAD0", VK_NUMPAD0);
    add(L"NUMPAD1", VK_NUMPAD1);
    add(L"NUMPAD2", VK_NUMPAD2);
    add(L"NUMPAD3", VK_NUMPAD3);
    add(L"NUMPAD4", VK_NUMPAD4);
    add(L"NUMPAD5", VK_NUMPAD5);
    add(L"NUMPAD6", VK_NUMPAD6);
    add(L"NUMPAD7", VK_NUMPAD7);
    add(L"NUMPAD8", VK_NUMPAD8);
    add(L"NUMPAD9", VK_NUMPAD9);
    add(L"DECIMAL", VK_DECIMAL);
    add(L"NUMPAD_DECIMAL", VK_DECIMAL);
    add(L"DIVIDE", VK_DIVIDE);
    add(L"MULTIPLY", VK_MULTIPLY);
    add(L"SUBTRACT", VK_SUBTRACT);
    add(L"ADD", VK_ADD);

    for (UINT i = 1; i <= 24; ++i) {
      wchar_t name[8];
      StringCchPrintfW(name, _countof(name), L"F%u", i);
      UINT vk = VK_F1 + (i - 1);
      map.emplace(name, vk);
    }

    return map;
  }();

  auto it = defaultKeys.find(keyName);
  if (it != defaultKeys.end()) {
    return it->second;
  }

  return UINT(-1);
}


/// <summary>
/// String -> Mod code
/// </summary>
static UINT ParseMods(LPCWSTR modsStr) {
  UINT mods = 0;
  if (wcsstr(modsStr, L"win") != nullptr) mods |= MOD_WIN;
  if (wcsstr(modsStr, L"alt") != nullptr) mods |= MOD_ALT;
  if (wcsstr(modsStr, L"ctrl") != nullptr) mods |= MOD_CONTROL;
  if (wcsstr(modsStr, L"shift") != nullptr) mods |= MOD_SHIFT;
  if (wcsstr(modsStr, L"norepeat") != nullptr) mods |= MOD_NOREPEAT;
  return mods;
}

static bool IsModifierPressed(int leftVk, int rightVk) {
  return ((GetAsyncKeyState(leftVk) & 0x8000) != 0) || ((GetAsyncKeyState(rightVk) & 0x8000) != 0);
}

static bool AreModifiersSatisfied(UINT mods) {
  UINT normalized = mods & ~MOD_NOREPEAT;

  auto check = [&](UINT flag, int leftVk, int rightVk) {
    bool pressed = IsModifierPressed(leftVk, rightVk);
    if ((normalized & flag) != 0) {
      return pressed;
    }
    return !pressed;
  };

  if (!check(MOD_WIN, VK_LWIN, VK_RWIN)) {
    return false;
  }
  if (!check(MOD_CONTROL, VK_LCONTROL, VK_RCONTROL)) {
    return false;
  }
  if (!check(MOD_ALT, VK_LMENU, VK_RMENU)) {
    return false;
  }
  if (!check(MOD_SHIFT, VK_LSHIFT, VK_RSHIFT)) {
    return false;
  }

  return true;
}

static void ReleaseWinHook() {
  if (gWinHotkeyHook != NULL) {
    UnhookWindowsHookEx(gWinHotkeyHook);
    gWinHotkeyHook = NULL;
  }
  gWinFallbackHotkeys.clear();
}

static void EnsureWinHook() {
  if (gWinHotkeyHook == NULL) {
    gWinHotkeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, gLSModule.GetInstance(), 0);
    if (gWinHotkeyHook == NULL) {
      ErrorHandler::Error(ErrorHandler::Level::Warning,
        L"Failed to install WIN-key hotkey hook.\nError %lu", GetLastError());
    }
  }
}

static WinHotkeyEntry* FindFallbackEntry(UINT key, UINT mods) {
  UINT normalized = mods & ~MOD_NOREPEAT;
  for (auto &entry : gWinFallbackHotkeys) {
    if (entry.key == key && ((entry.mods & ~MOD_NOREPEAT) == normalized)) {
      return &entry;
    }
  }
  return nullptr;
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION && !gWinFallbackHotkeys.empty()) {
    const KBDLLHOOKSTRUCT* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

    if ((info->flags & LLKHF_INJECTED) != 0) {
      return CallNextHookEx(gWinHotkeyHook, nCode, wParam, lParam);
    }

    UINT vkCode = info->vkCode;

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      for (auto &entry : gWinFallbackHotkeys) {
        UINT normalized = entry.mods & ~MOD_NOREPEAT;
        if ((normalized & MOD_WIN) == 0) {
          continue;
        }

        if (vkCode == entry.key && AreModifiersSatisfied(entry.mods)) {
          if (!entry.active) {
            entry.active = true;
            LiteStep::LSExecute(gLSModule.GetMessageWindow(), entry.command.c_str(), 0);
          }
          return 1;
        }
      }
    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
      for (auto &entry : gWinFallbackHotkeys) {
        if (vkCode == entry.key) {
          entry.active = false;
        }

        UINT normalized = entry.mods & ~MOD_NOREPEAT;
        if ((normalized & MOD_CONTROL) && (vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)) {
          entry.active = false;
        }
        if ((normalized & MOD_ALT) && (vkCode == VK_LMENU || vkCode == VK_RMENU)) {
          entry.active = false;
        }
        if ((normalized & MOD_SHIFT) && (vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)) {
          entry.active = false;
        }
      }

      if (vkCode == VK_LWIN || vkCode == VK_RWIN) {
        for (auto &entry : gWinFallbackHotkeys) {
          entry.active = false;
        }
      }
    }
  }

  return CallNextHookEx(gWinHotkeyHook, nCode, wParam, lParam);
}











