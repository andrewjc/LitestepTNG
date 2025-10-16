//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// This is a part of the Litestep Shell source code.
//
// Copyright (C) 1997-2015  LiteStep Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#if !defined(LSAPIINIT_H)
#define LSAPIINIT_H

#ifndef LSAPI_DATA
#  if defined(LSAPI_INTERNAL)
#    define LSAPI_DATA __declspec(dllexport)
#  else
#    define LSAPI_DATA __declspec(dllimport)
#  endif
#endif

#include "BangManager.h"
#ifndef LSAPI_API
#  if defined(LSAPI_INTERNAL)
#    define LSAPI_API __declspec(dllexport)
#  else
#    define LSAPI_API __declspec(dllimport)
#  endif
#endif

#include "SettingsManager.h"
#include "../utility/common.h"
#include <memory>
class TaskExecutor;

enum ErrorType
{
     LSAPI_ERROR_GENERAL
    ,LSAPI_ERROR_NOTINITIALIZED
    ,LSAPI_ERROR_RECURRENT
    ,LSAPI_ERROR_INVALIDTHREAD
    ,LSAPI_ERROR_SETTINGSINIT
    ,LSAPI_ERROR_SETTINGSMANAGER
    ,LSAPI_ERROR_BANGMANAGER
    ,LSAPI_ERROR_BANGINIT
};

class LSAPIException
{
public:
    explicit LSAPIException(ErrorType et) throw() :m_et(et)
    {
        // do nothing
    }
    ErrorType Type() const throw()
    {
        return m_et;
    }

private:
    ErrorType m_et;
};

class LSAPI_API LSAPIInit
{
private:
    bool setShellFolderVariable(LPCWSTR pwzVariable, int nFolder);
    void setLitestepVars();
    void getCompileTime(LPWSTR pwzValue, size_t cchValue);

public:
    LSAPIInit();
    ~LSAPIInit();

    BangManager* GetBangManager() const
    {
        if (!IsInitialized())
        {
            throw LSAPIException(LSAPI_ERROR_NOTINITIALIZED);
        }

        return m_bmBangManager;
    }

    SettingsManager* GetSettingsManager() const
    {
        if (!IsInitialized())
        {
            throw LSAPIException(LSAPI_ERROR_NOTINITIALIZED);
        }

        return m_smSettingsManager;
    }

    TaskExecutor* GetTaskExecutor() const
    {
        return m_taskExecutor.get();
    }

    void ProcessTaskCompletionPayload(void* payload);

    HWND GetLitestepWnd() const
    {
        return m_hLitestepWnd;
    }

    DWORD GetMainThreadID() const
    {
        return m_dwMainThreadID;
    }

    void Initialize(LPCWSTR pszLitestepPath, LPCWSTR pszRcPath);

    bool IsInitialized() const
    {
        return m_bIsInitialized;
    }

    void ReloadBangs();
    void ReloadSettings();

    void SetLitestepWindow(HWND hLitestepWnd)
    {
        m_hLitestepWnd = hLitestepWnd;
    }

    void SetCOMFactory(IClassFactory *pFactory)
    {
        m_pComFactory = pFactory;
    }

    IClassFactory *GetCOMFactory() const
    {
        return m_pComFactory;
    }

private:
    DWORD m_dwMainThreadID;

    BangManager* m_bmBangManager;
    SettingsManager* m_smSettingsManager;
    std::unique_ptr<TaskExecutor> m_taskExecutor;

    HWND m_hLitestepWnd;
    IClassFactory *m_pComFactory;
    wchar_t m_wzLitestepPath[MAX_PATH];
    wchar_t m_wzRcPath[MAX_PATH];

    bool m_bIsInitialized;
};

extern LSAPI_DATA LSAPIInit g_LSAPIManager;
extern void SetupBangs();

#endif // LSAPIINIT_H

