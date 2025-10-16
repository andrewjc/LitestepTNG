//-------------------------------------------------------------------------------------------------
// /ModuleKit/LiteStep.h
// The nModules Project
//
// Confines LSAPI routines to a namespace, and provides extra parsing functions.
//-------------------------------------------------------------------------------------------------
#pragma once

#include "Color.h"
#include "Distance.hpp"

#include "../Utilities/Common.h"
#include <memory>
#include <utility>

#include <functional>
#include <ShlObj.h>

namespace LiteStep {
  // Wrap the core API in the LiteStep namespace
  #include "../../sdk/include/lsapi.h"

  // Fetching of prefixed data types
  bool GetPrefixedRCBool(LPCTSTR prefix, LPCTSTR keyName, bool defaultValue);
  IColorVal* GetPrefixedRCColor(LPCTSTR prefix, LPCTSTR keyName, const IColorVal* defaultValue);
  double GetPrefixedRCDouble(LPCTSTR prefix, LPCTSTR keyName, double defaultValue);
  float GetPrefixedRCFloat(LPCTSTR prefix, LPCTSTR keyName, float defaultValue);
  int GetPrefixedRCInt(LPCTSTR prefix, LPCTSTR keyName, int defaultValue);
  __int64 GetPrefixedRCInt64(LPCTSTR prefix, LPCTSTR keyName, __int64 defaultValue);
  bool GetPrefixedRCLine(LPCTSTR prefix, LPCTSTR keyName, LPTSTR buffer, LPCTSTR defaultValue, size_t cchBuffer);
  UINT GetPrefixedRCMonitor(LPCTSTR prefix, LPCTSTR keyName, UINT defaultValue);
  Distance GetPrefixedRCDistance(LPCTSTR prefix, LPCTSTR keyName, Distance defaultValue);
  bool GetPrefixedRCString(LPCTSTR prefix, LPCTSTR keyName, LPTSTR buffer, LPCTSTR defaultValue, size_t cchBuffer);

  // Utility functions
  void IterateOverLines(LPCTSTR keyName, std::function<void (LPCTSTR line)> callback);
  void IterateOverTokens(LPCTSTR line, std::function<void (LPCTSTR token)> callback);
  void IterateOverLineTokens(LPCTSTR keyName, std::function<void (LPCTSTR token)> callback);
  void IterateOverCommandLineTokens(LPCTSTR prefix, LPCTSTR keyName, std::function<void (LPCTSTR token)> callback);

  // Parsing functions
  bool ParseBool(LPCTSTR boolString);
  IColorVal* ParseColor(LPCTSTR colorString, const IColorVal* defaultValue);
  UINT ParseMonitor(LPCTSTR monitorString, UINT defaultValue);

  using TaskHandle = LSTASKHANDLE;

  namespace detail {
    struct TaskThunk {
      std::function<void()> work;
      std::function<void(bool)> completion;
    };

    inline void CALLBACK RunTaskThunk(LPVOID context) {
      auto thunk = static_cast<TaskThunk*>(context);
      if (thunk && thunk->work) {
        thunk->work();
      }
    }

    inline void CALLBACK CompleteTaskThunk(LPVOID context, BOOL cancelled) {
      std::unique_ptr<TaskThunk> thunk(static_cast<TaskThunk*>(context));
      if (thunk && thunk->completion) {
        thunk->completion(cancelled != FALSE);
      }
    }
  }

  inline TaskHandle PostTask(LSTASKEXECUTEPROC executeProc, void* executeContext,
      LSTASKCOMPLETIONPROC completionProc, void* completionContext) {
    return LSPostTask(executeProc, executeContext, completionProc, completionContext);
  }

  inline TaskHandle PostTask(std::function<void()> work, std::function<void(bool)> completion)
  {
    if (!work) {
      return 0;
    }
    auto thunk = new (std::nothrow) detail::TaskThunk{ std::move(work), std::move(completion) };
    if (!thunk) {
      return 0;
    }
    TaskHandle handle = LSPostTask(detail::RunTaskThunk, thunk, detail::CompleteTaskThunk, thunk);
    if (handle == 0) {
      delete thunk;
    }
    return handle;
  }

  inline TaskHandle PostTask(std::function<void()> work, std::function<void()> completion)
  {
    if (!completion) {
      return PostTask(std::move(work), std::function<void(bool)>());
    }
    return PostTask(std::move(work), [fn = std::move(completion)](bool cancelled) mutable {
      if (!cancelled) {
        fn();
      }
    });
  }

  inline bool CancelTask(TaskHandle handle) {
    return handle != 0 && LSCancelTask(handle) != FALSE;
  }

  inline bool WaitTask(TaskHandle handle, DWORD timeout = INFINITE) {
    return handle == 0 ? false : (LSWaitTask(handle, timeout) != FALSE);
  }
}

