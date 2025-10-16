//-------------------------------------------------------------------------------------------------
// /Label/nLabel.hpp
// The nModules Project
//
// nLabel entry points.
//-------------------------------------------------------------------------------------------------
#include "Label.hpp"
#include "Version.h"

#include "../ModuleKit/LiteStep.h"
#include "../ModuleKit/LSModule.hpp"
#include "../ModuleKit/ErrorHandler.h"

#include "../Utilities/StringUtils.h"

#include <strsafe.h>
#include <unordered_map>

using std::wstring;

static void CreateLabel(LPCWSTR labelName);
static void DestroyLabels();
static void LoadSettings();

// The LSModule class.
LSModule gLSModule(TEXT(MODULE_NAME), TEXT(MODULE_AUTHOR), MakeVersion(MODULE_VERSION));

// The messages we want from the core.
static UINT gLSMessages[] = { LM_GETREVID, LM_REFRESH, LM_FULLSCREENACTIVATED,
    LM_FULLSCREENDEACTIVATED, 0 };

// All the top-level labels we currently have loaded.
// These do not include overlay labels.
static StringKeyedMaps<wstring, Label>::UnorderedMap gTopLevelLabels;

// All the labels we currently have loaded. Labels add and remove themselves from this list.
StringKeyedMaps<wstring, Label*>::UnorderedMap gAllLabels;


/// <summary>
/// Creates a new label.
/// </summary>
/// <param name="labelName">The RC settings prefix of the label to create.</param>
void CreateLabel(LPCWSTR labelName) {
  if (gAllLabels.find(labelName) == gAllLabels.end()) {
    gTopLevelLabels.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(labelName),
      std::forward_as_tuple(labelName)
    );
  } else {
    ErrorHandler::Error(
      ErrorHandler::Level::Critical,
      L"Attempt to (re)create the already existing label %s!",
      labelName
    );
  }
}


/// <summary>
/// Destroys all labels.
/// </summary>
void DestroyLabels() {
  gTopLevelLabels.clear();
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
EXPORT_CDECL(int) initModuleW(HWND parent, HINSTANCE instance, LPCWSTR path) {
  UNREFERENCED_PARAMETER(path);

  if (!gLSModule.Initialize(parent, instance)) {
    return 1;
  }

  if (!gLSModule.ConnectToCore(MakeVersion(CORE_VERSION))) {
    return 1;
  }

  // Load settings
  LoadSettings();

  return 0;
}


/// <summary>
/// Reads through the .rc files and creates labels.
/// </summary>
void LoadSettings() {
  LiteStep::IterateOverLineTokens(L"*nLabel", CreateLabel);
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

  switch (message) {
  case WM_CREATE:
    SendMessage(GetLitestepWnd() ,LM_REGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
    return 0;

  case WM_DESTROY:
    SendMessage(GetLitestepWnd(), LM_UNREGISTERMESSAGE, (WPARAM)window, (LPARAM)gLSMessages);
    return 0;

  case LM_FULLSCREENACTIVATED:
    for (auto &label : gTopLevelLabels) {
      label.second.GetWindow()->FullscreenActivated((HMONITOR)wParam, (HWND)lParam);
    }
    return 0;

  case LM_FULLSCREENDEACTIVATED:
    for (auto &label : gTopLevelLabels) {
      label.second.GetWindow()->FullscreenDeactivated((HMONITOR)wParam);
    }
    return 0;

  case LM_REFRESH:
    DestroyLabels();
    LoadSettings();
    return 0;
  }
  return DefWindowProc(window, message, wParam, lParam);
}


/// <summary>
/// Called by the LiteStep core when this module is about to be unloaded.
/// </summary>
/// <param name="instance">Handle to this module's instance.</param>
EXPORT_CDECL(void) quitModule(HINSTANCE /* instance */) {
  DestroyLabels();
  gLSModule.DeInitalize();
}

