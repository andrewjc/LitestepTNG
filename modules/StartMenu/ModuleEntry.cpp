#include "Version.h"
#include "../ModuleKit/LiteStep.h"
#include "../ModuleKit/LSModule.hpp"
#include "../ModuleKit/ErrorHandler.h"
#include "../Utilities/StringUtils.h"

#include "../Popup/Popup.hpp"
#include "../Popup/CommandItem.hpp"
#include "../Popup/SeparatorItem.hpp"
#include "../Popup/ContentPopup.hpp"
#include "../Popup/FolderItem.hpp"
#include "../Popup/InfoItem.hpp"

#include "../CoreCom/Core.h"
#include "../Core/FileSystemLoaderResponseHandler.hpp"
#include "../Core/FileSystemLoader.h"

#include <algorithm>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <list>
#include <memory>
#include <deque>
#include <unordered_map>
#include <cwctype>
#include <cwchar>
#include <cstdlib>
#include <string>
#include <vector>

using std::wstring;
using std::unique_ptr;
using std::vector;
using lsapi::StringUtils;


LSModule gLSModule(TEXT(MODULE_NAME), TEXT(MODULE_AUTHOR), MakeVersion(MODULE_VERSION));

namespace
{
class StartMenuPopup;

class PlaceholderItem final : public PopupItem
{
public:
    PlaceholderItem(Popup* owner, LPCWSTR text)
        : PopupItem(owner, L"StartMenuPlaceholder", PopupItem::Type::Info)
        , mOwner(owner)
    {
        mWindow->Initialize(owner->mPopupSettings.mInfoWindowSettings,
            &owner->mPopupSettings.mInfoStateRender);
        mWindow->SetText(text);
        mWindow->Show();
    }

    int GetDesiredWidth(int maxWidth) override
    {
        SIZE size = {};
        mWindow->GetDesiredSize(maxWidth, static_cast<int>(mWindow->GetSize().height), &size);
        return size.cx;
    }

    LRESULT HandleMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, LPVOID) override
    {
        switch (msg)
        {
        case WM_MOUSEMOVE:
            mOwner->mPopupSettings.mInfoStateRender.ActivateState(InfoItem::State::Hover, mWindow);
            return 0;

        case WM_MOUSELEAVE:
            mOwner->mPopupSettings.mInfoStateRender.ClearState(InfoItem::State::Hover, mWindow);
            return 0;
        }
        return DefWindowProc(window, msg, wParam, lParam);
    }

private:
    Popup* mOwner;
};

std::unique_ptr<StartMenuPopup> gStartMenuPopup;


wstring SanitizePrefixFragment(const wstring &title)
{
    wstring fragment;
    fragment.reserve(title.length());
    for (wchar_t ch : title)
    {
        if (iswalnum(ch))
        {
            fragment.push_back(ch);
        }
        else if (ch == L' ')
        {
            fragment.push_back(L'_');
        }
    }
    if (fragment.empty())
    {
        fragment = L"Entry";
    }
    return fragment;
}

struct MenuEntry
{
    enum class Type
    {
        Command,
        Separator,
        Content
    };

    Type type{ Type::Command };
    wstring title;
    wstring command;
    ContentPopup::ContentSource contentSource{ ContentPopup::ContentSource::PROGRAMS };
};

bool TryResolveContentSource(const wstring &command, ContentPopup::ContentSource &out)
{
    if (_wcsicmp(command.c_str(), L"!PopupPrograms") == 0)
    {
        out = ContentPopup::ContentSource::PROGRAMS;
        return true;
    }
    if (_wcsicmp(command.c_str(), L"!PopupRecentDocuments") == 0)
    {
        out = ContentPopup::ContentSource::RECENT_DOCUMENTS;
        return true;
    }
    if (_wcsicmp(command.c_str(), L"!PopupNetwork") == 0)
    {
        out = ContentPopup::ContentSource::NETWORK;
        return true;
    }
    if (_wcsicmp(command.c_str(), L"!PopupRecycleBin") == 0)
    {
        out = ContentPopup::ContentSource::RECYCLE_BIN;
        return true;
    }
    if (_wcsicmp(command.c_str(), L"!PopupControlPanel") == 0)
    {
        out = ContentPopup::ContentSource::CONTROL_PANEL;
        return true;
    }
    if (_wcsicmp(command.c_str(), L"!PopupMyComputer") == 0)
    {
        out = ContentPopup::ContentSource::MY_COMPUTER;
        return true;
    }
    return false;
}

vector<MenuEntry> ParseMenuEntries()
{
    vector<MenuEntry> entries;
    LiteStep::IterateOverLines(L"*PopupStartMenu", [&](LPCTSTR rawLine) {
        wchar_t buffers[8][MAX_RCCOMMAND];
        LPTSTR tokens[8];
        for (int i = 0; i < 8; ++i)
        {
            tokens[i] = buffers[i];
        }

        int tokenCount = LiteStep::CommandTokenize(rawLine, tokens, 8, nullptr);
        if (tokenCount <= 0)
        {
            return;
        }

        wstring title = StringUtils::TrimQuotesCopy(tokens[0]);
        if (title.empty())
        {
            return;
        }

        if (_wcsicmp(title.c_str(), L"!Separator") == 0)
        {
            MenuEntry entry;
            entry.type = MenuEntry::Type::Separator;
            entries.emplace_back(std::move(entry));
            return;
        }

        wstring command;
        for (int i = 1; i < tokenCount; ++i)
        {
            if (i > 1)
            {
                command += L" ";
            }
            command += tokens[i];
        }
        command = StringUtils::TrimQuotesCopy(command);

        MenuEntry entry;
        entry.title = title;
        if (tokenCount >= 2)
        {
            wstring primaryCommand = tokens[1];
            if (_wcsicmp(primaryCommand.c_str(), L"!Separator") == 0)
            {
                entry.type = MenuEntry::Type::Separator;
            }
            else if (TryResolveContentSource(primaryCommand, entry.contentSource))
            {
                entry.type = MenuEntry::Type::Content;
                entry.command = primaryCommand;
            }
            else
            {
                entry.type = MenuEntry::Type::Command;
                entry.command = command.empty() ? primaryCommand : command;
            }
        }
        else
        {
            entry.type = MenuEntry::Type::Command;
            entry.command = command;
        }

        if (entry.type == MenuEntry::Type::Command && entry.command.empty())
        {
            // No command specified, skip this entry.
            return;
        }

        entries.emplace_back(std::move(entry));
    });

    return entries;
}

vector<MenuEntry> BuildDefaultEntries()
{
    vector<MenuEntry> defaults;

    MenuEntry programs;
    programs.type = MenuEntry::Type::Content;
    programs.title = L"Programs";
    programs.command = L"!PopupPrograms";
    programs.contentSource = ContentPopup::ContentSource::PROGRAMS;
    defaults.emplace_back(programs);

    MenuEntry recent;
    recent.type = MenuEntry::Type::Content;
    recent.title = L"Recent";
    recent.command = L"!PopupRecentDocuments";
    recent.contentSource = ContentPopup::ContentSource::RECENT_DOCUMENTS;
    defaults.emplace_back(recent);

    MenuEntry settings;
    settings.type = MenuEntry::Type::Command;
    settings.title = L"Settings";
    settings.command = L"!Execute [ms-settings:]";
    defaults.emplace_back(settings);

    MenuEntry network;
    network.type = MenuEntry::Type::Content;
    network.title = L"Network";
    network.command = L"!PopupNetwork";
    network.contentSource = ContentPopup::ContentSource::NETWORK;
    defaults.emplace_back(network);

    MenuEntry cmd;
    cmd.type = MenuEntry::Type::Command;
    cmd.title = L"Command Prompt";
    cmd.command = L"!Execute [$WINDIR$\\System32\\cmd.exe]";
    defaults.emplace_back(cmd);

    MenuEntry run;
    run.type = MenuEntry::Type::Command;
    run.title = L"Run";
    run.command = L"!PopupRun";
    defaults.emplace_back(run);

    MenuEntry shutdown;
    shutdown.type = MenuEntry::Type::Command;
    shutdown.title = L"Shutdown";
    shutdown.command = L"!PopupPower";
    defaults.emplace_back(shutdown);

    MenuEntry recycle;
    recycle.type = MenuEntry::Type::Content;
    recycle.title = L"Recycle";
    recycle.command = L"!PopupRecycleBin";
    recycle.contentSource = ContentPopup::ContentSource::RECYCLE_BIN;
    defaults.emplace_back(recycle);

    return defaults;
}

class AsyncShellFolderPopup : public Popup, private FileSystemLoaderResponseHandler
{
public:
    AsyncShellFolderPopup(LPCTSTR title, LPCTSTR bang, LPCTSTR prefix, std::wstring placeholder)
        : Popup(title, bang, prefix)
        , mPlaceholderText(std::move(placeholder))
    {
    }

    ~AsyncShellFolderPopup() override
    {
        for (auto &request : mActiveRequests)
        {
            nCore::CancelLoad(request.first);
        }
        mActiveRequests.clear();
        mPendingFolders.clear();
        RemovePlaceholder();
    }

protected:
    void PreShow() override
    {
        if (!mInitialized)
        {
            BuildInitialQueue();
            mInitialized = true;
        }

        if (!mLoaded && !mLoading)
        {
            EnsurePlaceholder();
            StartNextRequest();
        }
        else if (!mLoaded)
        {
            EnsurePlaceholder();
        }
    }

    void PostClose() override {}

    virtual void BuildInitialQueue() = 0;

    void EnqueueKnownFolder(REFKNOWNFOLDERID folderId)
    {
        mPendingFolders.push_back(folderId);
    }

    void SetPlaceholderText(const std::wstring &text)
    {
        mPlaceholderText = text;
        if (mPlaceholderItem)
        {
            mPlaceholderItem->GetWindow()->SetText(mPlaceholderText.c_str());
        }
    }

    bool HasPendingFolders() const
    {
        return !mPendingFolders.empty();
    }

    LPARAM FolderLoaded(UINT64 id, LoadFolderResponse *response) override
    {
        auto active = mActiveRequests.find(id);
        if (active == mActiveRequests.end())
        {
            return 0;
        }

        auto folder = active->second->folder;
        mActiveRequests.erase(active);

        if (folder && response)
        {
            for (auto &entry : response->items)
            {
                AppendEntry(folder, entry);
            }
        }

        StartNextRequest();
        return 0;
    }

    LPARAM ItemLoaded(UINT64, LoadItemResponse*) override
    {
        return 0;
    }

private:
    struct FolderRequest
    {
        explicit FolderRequest(IShellFolder2 *ptr) : folder(ptr) {}
        ~FolderRequest()
        {
            if (folder)
            {
                folder->Release();
            }
        }
        IShellFolder2 *folder;
    };

    void EnsurePlaceholder()
    {
        if (!mPlaceholderItem)
        {
            mPlaceholderItem = new PlaceholderItem(static_cast<Popup*>(this), mPlaceholderText.c_str());
            AddItem(mPlaceholderItem);
        }
    }

    void RemovePlaceholder()
    {
        if (!mPlaceholderItem)
        {
            return;
        }

        auto it = std::find(items.begin(), items.end(), mPlaceholderItem);
        if (it != items.end())
        {
            delete *it;
            items.erase(it);
        }
        mPlaceholderItem = nullptr;
    }

    static IShellFolder2* BindToKnownFolder(REFKNOWNFOLDERID folderId)
    {
        IShellFolder *desktop = nullptr;
        if (FAILED(SHGetDesktopFolder(&desktop)))
        {
            return nullptr;
        }

        PIDLIST_ABSOLUTE idList = nullptr;
        IShellFolder2 *result = nullptr;
        if (SUCCEEDED(SHGetKnownFolderIDList(folderId, 0, nullptr, &idList)))
        {
            IShellFolder *folder = nullptr;
            if (SUCCEEDED(desktop->BindToObject(idList, nullptr, IID_IShellFolder, reinterpret_cast<void**>(&folder))))
            {
                folder->QueryInterface(IID_PPV_ARGS(&result));
                folder->Release();
            }
            CoTaskMemFree(idList);
        }

        desktop->Release();
        return result;
    }

    void StartNextRequest()
    {
        while (!mPendingFolders.empty())
        {
            KNOWNFOLDERID folderId = mPendingFolders.front();
            mPendingFolders.pop_front();
            IShellFolder2 *folder = BindToKnownFolder(folderId);
            if (!folder)
            {
                continue;
            }

            LoadFolderRequest request{};
            request.folder = folder;
            request.targetIconWidth = 32;

            UINT64 requestId = nCore::LoadFolder(request, this);
            if (requestId == 0)
            {
                folder->Release();
                continue;
            }

            mLoading = true;
            mActiveRequests.emplace(requestId, std::make_unique<FolderRequest>(folder));
            return;
        }

        if (mActiveRequests.empty())
        {
            OnRequestsFinished();
        }
    }

    void OnRequestsFinished()
    {
        mLoading = false;
        if (items.empty())
        {
            SetPlaceholderText(L"No items found");
            EnsurePlaceholder();
        }
        else
        {
            RemovePlaceholder();
            std::sort(items.begin(), items.end(), [](PopupItem *a, PopupItem *b) { return a->CompareTo(b); });
        }

        mLoaded = true;

        if (mWindow->IsVisible())
        {
            MonitorInfo &monInfo = nCore::FetchMonitorInfo();
            RECT limits = monInfo.GetVirtualDesktop().rect;
            Size(&limits);
            mWindow->Repaint();
        }
    }

    bool ResolveEntry(IShellFolder2 *folder, PITEMID_CHILD child, std::wstring &name, std::wstring &command)
    {
        STRRET ret;
        if (FAILED(folder->GetDisplayNameOf(child, SHGDN_NORMAL, &ret)))
        {
            return false;
        }
        WCHAR rawName[MAX_PATH];
        if (FAILED(StrRetToBufW(&ret, child, rawName, ARRAYSIZE(rawName))))
        {
            return false;
        }
        name.assign(rawName);

        if (FAILED(folder->GetDisplayNameOf(child, SHGDN_FORPARSING, &ret)))
        {
            return false;
        }
        WCHAR rawCommand[MAX_PATH];
        if (FAILED(StrRetToBufW(&ret, child, rawCommand, ARRAYSIZE(rawCommand))))
        {
            return false;
        }
        command.assign(rawCommand);
        return true;
    }

    void AppendEntry(IShellFolder2 *folder, LoadItemResponse &entry)
    {
        std::wstring name;
        std::wstring command;
        if (!ResolveEntry(folder, entry.id, name, command))
        {
            return;
        }

        SFGAOF attributes = SFGAO_BROWSABLE | SFGAO_FOLDER;
        bool openable = SUCCEEDED(folder->GetAttributesOf(1, (LPCITEMIDLIST*)&entry.id, &attributes)) &&
            (((attributes & SFGAO_FOLDER) == SFGAO_FOLDER) || ((attributes & SFGAO_BROWSABLE) == SFGAO_BROWSABLE));

        PopupItem *item = nullptr;

        if (openable)
        {
            auto existing = std::find_if(items.begin(), items.end(), [&](PopupItem *candidate) {
                return candidate->CheckMerge(name.c_str());
            });

            if (existing != items.end())
            {
                static_cast<nPopup::FolderItem*>(*existing)->AddPath(command.c_str());
                return;
            }

            item = new nPopup::FolderItem(
                this,
                name.c_str(),
                [] (nPopup::FolderItem::CreationData *data) -> Popup*
                {
                    ContentPopup *popup = new ContentPopup(data->command, true, data->name, nullptr, data->prefix);
                    for (auto path : data->paths)
                    {
                        popup->AddPath(path);
                    }
                    return popup;
                },
                new nPopup::FolderItem::CreationData(command.c_str(), name.c_str(), mSettings->GetPrefix()));
        }
        else
        {
            wchar_t quotedCommand[MAX_LINE_LENGTH];
            StringCchCopyW(quotedCommand, _countof(quotedCommand), L"\"");
            StringCchCatW(quotedCommand, _countof(quotedCommand), command.c_str());
            StringCchCatW(quotedCommand, _countof(quotedCommand), L"\"");
            item = new CommandItem(this, name.c_str(), quotedCommand);
        }

        if (item)
        {
            IExtractIconW *extractIcon = nullptr;
            if (SUCCEEDED(folder->GetUIObjectOf(nullptr, 1, (LPCITEMIDLIST*)&entry.id, IID_IExtractIconW, nullptr, reinterpret_cast<void**>(&extractIcon))))
            {
                item->SetIcon(extractIcon);
                extractIcon->Release();
            }
            AddItem(item);
        }
    }

    PlaceholderItem *mPlaceholderItem = nullptr;
    std::wstring mPlaceholderText;
    bool mInitialized = false;
    bool mLoaded = false;
    bool mLoading = false;
    std::deque<KNOWNFOLDERID> mPendingFolders;
    std::unordered_map<UINT64, std::unique_ptr<FolderRequest>> mActiveRequests;
};

class NetworkContentPopup final : public AsyncShellFolderPopup
{
public:
    NetworkContentPopup(LPCTSTR title, LPCTSTR bang, LPCTSTR prefix)
        : AsyncShellFolderPopup(title, bang, prefix, L"Discovering network...")
    {
    }

protected:
    void BuildInitialQueue() override
    {
        EnqueueKnownFolder(FOLDERID_NetworkFolder);
        if (!HasPendingFolders())
        {
            SetPlaceholderText(L"Network discovery unavailable");
        }
    }
};

class ProgramsContentPopup final : public AsyncShellFolderPopup
{
public:
    ProgramsContentPopup(LPCTSTR title, LPCTSTR bang, LPCTSTR prefix)
        : AsyncShellFolderPopup(title, bang, prefix, L"Loading applications...")
    {
    }

protected:
    void BuildInitialQueue() override
    {
        EnqueueKnownFolder(FOLDERID_Programs);
        EnqueueKnownFolder(FOLDERID_CommonPrograms);
        if (!HasPendingFolders())
        {
            SetPlaceholderText(L"No applications found");
        }
    }
};

class StartMenuPopup : public Popup
{
public:
    StartMenuPopup() : Popup(L"Start Menu", L"!PopupStartMenu", L"PopupStartMenu") {}

    void Build(const vector<MenuEntry> &entries)
    {
        mOwnedPopups.clear();

        for (const auto &entry : entries)
        {
            switch (entry.type)
            {
            case MenuEntry::Type::Separator:
                AddItem(new SeparatorItem(this));
                break;
            case MenuEntry::Type::Command:
                AddItem(new CommandItem(this, entry.title.c_str(), entry.command.c_str()));
                break;
            case MenuEntry::Type::Content:
            {
                wstring childPrefix = L"PopupStartMenu";
                childPrefix += SanitizePrefixFragment(entry.title);
                std::unique_ptr<Popup> popup;
                if (entry.contentSource == ContentPopup::ContentSource::NETWORK)
                {
                    popup = std::make_unique<NetworkContentPopup>(entry.title.c_str(), entry.command.c_str(), childPrefix.c_str());
                }
                else
                {
                    popup = std::make_unique<ContentPopup>(entry.contentSource, entry.title.c_str(), entry.command.c_str(), childPrefix.c_str());
                }
                AddItem(new nPopup::FolderItem(this, entry.title.c_str(), popup.get()));
                mOwnedPopups.emplace_back(std::move(popup));
                break;
            }
            }
        }
    }

protected:
    void PreShow() override {}
    void PostClose() override {}

private:
    vector<std::unique_ptr<Popup>> mOwnedPopups;
};

void WINAPI StartMenuBang(HWND, LPCTSTR, LPCTSTR args)
{
    if (!gStartMenuPopup)
    {
        return;
    }

    if (args != nullptr && *args != L'\0')
    {
        wchar_t buffers[2][MAX_RCCOMMAND];
        LPTSTR tokens[2] = { buffers[0], buffers[1] };
        if (LiteStep::CommandTokenize(args, tokens, 2, nullptr) == 2)
        {
            int x = _wtoi(tokens[0]);
            int y = _wtoi(tokens[1]);
            gStartMenuPopup->Show(x, y);
            return;
        }
    }

    gStartMenuPopup->Show();
}

void RegisterBang()
{
    LiteStep::AddBangCommandEx(L"!PopupStartMenu", StartMenuBang);
}

void UnregisterBang()
{
    LiteStep::RemoveBangCommand(L"!PopupStartMenu");
}

void LoadSettings()
{
    UnregisterBang();
    gStartMenuPopup.reset();

    auto entries = ParseMenuEntries();
    if (entries.empty())
    {
        entries = BuildDefaultEntries();
    }

    gStartMenuPopup = std::make_unique<StartMenuPopup>();
    gStartMenuPopup->Build(entries);

    RegisterBang();
}

} // namespace

int initModuleW(HWND parent, HINSTANCE instance, LPCWSTR path)
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

    LoadSettings();
    return 0;
}

void quitModule(HINSTANCE)
{
    UnregisterBang();
    gStartMenuPopup.reset();
    gLSModule.DeInitalize();
}

LRESULT WINAPI LSMessageHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        SendMessage(LiteStep::GetLitestepWnd(), LM_REGISTERMESSAGE, (WPARAM)window, 0);
        return 0;
    case WM_DESTROY:
        SendMessage(LiteStep::GetLitestepWnd(), LM_UNREGISTERMESSAGE, (WPARAM)window, 0);
        return 0;
    case LM_REFRESH:
        LoadSettings();
        return 0;
    }
    return DefWindowProc(window, message, wParam, lParam);
}

















