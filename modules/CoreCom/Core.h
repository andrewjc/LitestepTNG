//-------------------------------------------------------------------------------------------------
// /CoreCom/Core.h
// The nModules Project
//
// Functions declarations for the CoreCom library. All functions exported by nCore and some
// initialization functions.
//-------------------------------------------------------------------------------------------------
#pragma once

#include "../Core/CoreMessages.h"
#include "../Core/FileSystemLoader.h"
#include "../Core/IParsedText.hpp"

#include "../ModuleKit/MonitorInfo.hpp"

#include "../Utilities/Versioning.h"

class Window;

namespace nCore {
  HRESULT Connect(VERSION minVersion);
  void Disconnect();
  bool Initialized();

  VERSION GetCoreVersion();
  MonitorInfo &FetchMonitorInfo();

  void RegisterForCoreMessages(HWND hwnd, const UINT messages[]);
  void UnregisterForCoreMessages(HWND hwnd, const UINT messages[]);

  // FileSystemLoader
  UINT64 LoadFolder(LoadFolderRequest&, FileSystemLoaderResponseHandler*);
  UINT64 LoadFolderItem(LoadItemRequest&, FileSystemLoaderResponseHandler*);
  void CancelLoad(UINT64 id);

  namespace System {
    // Dynamic Text Service
    IParsedText *ParseText(LPCWSTR text);
    BOOL RegisterDynamicTextFunction(LPCWSTR name, UCHAR numArgs, FORMATTINGPROC, bool dynamic);
    BOOL UnRegisterDynamicTextFunction(LPCWSTR name, UCHAR numArgs);
    BOOL DynamicTextChangeNotification(LPCWSTR name, UCHAR numArgs);

    // Window Registrar
    void RegisterWindow(LPCWSTR, Window*);
    void UnRegisterWindow(LPCWSTR);
    Window *FindRegisteredWindow(LPCWSTR);
    void AddWindowRegistrationListener(LPCWSTR, Window*);
    void RemoveWindowRegistrationListener(LPCWSTR, Window*);
  }
}


