with open(r'd:\github\NeoMDIE\src\ui\MainWindow.cpp', 'r', encoding='utf-8') as f:
    s = f.read()

target = '''    case ID_FILE_CLOSE_ALL_TABS:
        m_tabBar.CloseAllTabs();
        BuildMenu();
        break;'''

replace = '''    case ID_FILE_CLOSE_ALL_TABS:
        m_tabBar.CloseAllTabs();
        BuildMenu();
        break;
    case ID_FILE_CLOSE_LEFT_TABS:
        m_tabBar.CloseTabsToLeft(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_CLOSE_RIGHT_TABS:
        m_tabBar.CloseTabsToRight(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_NEW_WINDOW_SPECIFY: {
        IFileDialog* pfd = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
            DWORD dwOptions = 0;
            pfd->GetOptions(&dwOptions);
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
            if (SUCCEEDED(pfd->Show(m_hwnd))) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(pfd->GetResult(&psi))) {
                    PWSTR pszPath = nullptr;
                    if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                        wchar_t exePath[MAX_PATH];
                        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                        ShellExecuteW(nullptr, L"open", exePath, pszPath, nullptr, SW_SHOWNORMAL);
                        CoTaskMemFree(pszPath);
                    }
                    psi->Release();
                }
            }
            pfd->Release();
        }
        break;
    }
    case ID_FILE_NEW_WINDOW_DESKTOP: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &p)) && p) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            ShellExecuteW(nullptr, L"open", exePath, p, nullptr, SW_SHOWNORMAL);
            CoTaskMemFree(p);
        }
        break;
    }
    case ID_FILE_NEW_WINDOW_CURRENT: {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        ShellExecuteW(nullptr, L"open", exePath, activeView->GetCurrentPath().c_str(), nullptr, SW_SHOWNORMAL);
        break;
    }
    case ID_FILE_NEW_WINDOW_CLIPBOARD: {
        if (OpenClipboard(m_hwnd)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pText = (wchar_t*)GlobalLock(hData);
                if (pText && PathFileExistsW(pText)) {
                    wchar_t exePath[MAX_PATH];
                    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                    ShellExecuteW(nullptr, L"open", exePath, pText, nullptr, SW_SHOWNORMAL);
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
        break;
    }
    case ID_FILE_OPEN_EXPLORER:
        ShellExecuteW(nullptr, L"open", L"explorer.exe", activeView->GetCurrentPath().c_str(), nullptr, SW_SHOWNORMAL);
        break;
    case ID_FILE_CREATE_SHORTCUT: {
        auto sel = activeView->GetSelectedPaths();
        std::wstring target = !sel.empty() ? sel[0] : activeView->GetCurrentPath();
        PWSTR pDesk = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &pDesk)) && pDesk) {
            std::wstring lnkPath = std::wstring(pDesk) + L"\\\\" + PathFindFileNameW(target.c_str()) + L".lnk";
            ShellUtils::CreateShortcut(lnkPath, target, L"", L"");
            CoTaskMemFree(pDesk);
        }
        break;
    }
    case ID_EDIT_UNDO: {
        HWND hList = activeView->GetListHwnd();
        if (hList) {
            SetFocus(hList);
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('Z', 0, 0, 0);
            keybd_event('Z', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        }
        break;
    }
    case ID_VIEW_LOCK_TOOLBARS:
        Config::Get().lockToolbars = !Config::Get().lockToolbars;
        Config::Get().Save();
        BuildMenu();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        break;
    case ID_VIEW_TB_STANDARD:
        Config::Get().showToolbarStandard = !Config::Get().showToolbarStandard;
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_TB_ADDRESS:
        Config::Get().showToolbarAddress = !Config::Get().showToolbarAddress;
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_TB_SEARCH:
        Config::Get().showToolbarSearch = !Config::Get().showToolbarSearch;
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_STATUSBAR:
        Config::Get().showStatusBar = !Config::Get().showStatusBar;
        ShowWindow(m_hwndStatusBar, Config::Get().showStatusBar ? SW_SHOW : SW_HIDE);
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_MINIMIZE_TRAY:
        ShowWindow(m_hwnd, SW_MINIMIZE);
        break;
    case ID_NAV_UP_NEW_TAB: {
        wchar_t parent[MAX_PATH];
        StringCchCopyW(parent, MAX_PATH, activeView->GetCurrentPath().c_str());
        PathRemoveFileSpecW(parent);
        if (parent[0]) {
            m_tabBar.AddTab(parent, true);
            BuildMenu();
        }
        break;
    }
    case ID_WINDOW_MAXIMIZE:
        m_multiPane.ToggleMaximizePane(m_multiPane.GetActiveIndex());
        break;
    case ID_WINDOW_MINIMIZE:
        m_multiPane.MinimizePane(m_multiPane.GetActiveIndex());
        break;
    case ID_WINDOW_CLOSE:
        m_multiPane.ClosePane(m_multiPane.GetActiveIndex());
        break;
    case ID_WINDOW_MAIN_MAXIMIZE:
        ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        break;
    case ID_WINDOW_MAIN_MINIMIZE:
        ShowWindow(m_hwnd, SW_MINIMIZE);
        break;
    case ID_TOOLS_GESTURES:
        SettingsDialog::Show(m_hwnd, [this]() {
            UpdateLanguageUI();
        });
        break;
    case ID_TOOLS_ALLOW_DUP_TABS:
        Config::Get().allowDuplicateTabs = !Config::Get().allowDuplicateTabs;
        Config::Get().Save();
        BuildMenu();
        break;
    case ID_TOOLS_MIN_TO_TRAY:
        Config::Get().minimizeToTray = !Config::Get().minimizeToTray;
        Config::Get().Save();
        BuildMenu();
        break;
    case ID_TOOLS_AUTO_EXPAND_TREE:
        Config::Get().autoExpandTree = !Config::Get().autoExpandTree;
        Config::Get().Save();
        BuildMenu();
        break;
    case ID_TOOLS_OPEN_SCRIPTS_DIR:
        ShellExecuteW(nullptr, L"open", ScriptEngine::Get().GetScriptsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_TOOLS_FOLDER_OPTIONS:
        ShellExecuteW(nullptr, L"open", L"control.exe", L"folders", nullptr, SW_SHOWNORMAL);
        break;'''

assert target in s, "target not found"
s = s.replace(target, replace, 1)

# Add recent closed to default:
target_def = '''        } else if (cmdId >= ID_SCRIPT_COMMAND_START && cmdId <= ID_SCRIPT_COMMAND_END) {
            ScriptEngine::Get().ExecuteCommand(cmdId, activeView->GetCurrentPath(), activeView->GetSelectedPaths());
        }'''

replace_def = '''        } else if (cmdId >= ID_RECENT_CLOSED_START && cmdId <= ID_RECENT_CLOSED_END) {
            int cIdx = (int)(cmdId - ID_RECENT_CLOSED_START);
            const auto& cHist = m_tabBar.GetClosedHistory();
            if (cIdx >= 0 && cIdx < (int)cHist.size()) {
                m_tabBar.AddTab(cHist[cHist.size() - 1 - cIdx], true);
                BuildMenu();
            }
        } else if (cmdId >= ID_SCRIPT_COMMAND_START && cmdId <= ID_SCRIPT_COMMAND_END) {
            ScriptEngine::Get().ExecuteCommand(cmdId, activeView->GetCurrentPath(), activeView->GetSelectedPaths());
        }'''

assert target_def in s, "target_def not found"
s = s.replace(target_def, replace_def, 1)

with open(r'd:\github\NeoMDIE\src\ui\MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(s)
print('Updated OnCommand successfully')
