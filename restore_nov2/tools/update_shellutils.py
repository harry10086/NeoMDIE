import re

# 1. Update ShellUtils.h
with open('src/core/ShellUtils.h', 'r', encoding='utf-8') as f:
    hdr = f.read()

hdr_target = 'CMD_BG_NEW_FILE          = 0x7136'
hdr_repl = '''CMD_BG_NEW_FILE          = 0x7136,
    CMD_BG_NEW_MD            = 0x7137'''
hdr = hdr.replace(hdr_target, hdr_repl)

hdr_func_target = 'bool SetExplorerReplaced(bool enable);'
hdr_func_repl = '''bool SetExplorerReplaced(bool enable);
    void CheckForUpdates(HWND hwndParent);'''
hdr = hdr.replace(hdr_func_target, hdr_func_repl)

with open('src/core/ShellUtils.h', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(hdr)
print('ShellUtils.h updated')

# 2. Update ShellUtils.cpp
with open('src/core/ShellUtils.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

# Replace CMF_ASYNCVERBSTATE in ShowContextMenu
src = src.replace('CMF_ASYNCVERBSTATE | CMF_CANRENAME', 'CMF_NORMAL | CMF_CANRENAME')

# Update ShowBackgroundContextMenu
bg_start = src.find('bool ShowBackgroundContextMenu(')
bg_end = src.find('\nbool CopyFilesToClipboard(', bg_start)
assert bg_start != -1 and bg_end != -1

new_bg_menu = '''bool ShowBackgroundContextMenu(HWND hwnd, const std::wstring& currentDir, POINT ptScreen,
                               int currentViewMode, int currentSortCol, bool sortAsc,
                               std::function<void(int neoCmd, const std::wstring& currentDir)> onCustomCommand) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();

    // 0. Top NeoMDIE shortcuts
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_CMD, isZh ? L"在此处打开命令提示符 CMD(&C)" : L"Open CMD Here(&C)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制当前路径(&A)" : L"Copy Current Path(&A)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_NEW_TAB, isZh ? L"新建标签页(&T)\\tCtrl+T" : L"New Tab(&T)\\tCtrl+T");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // 1. View submenu
    HMENU hSubView = CreatePopupMenu();
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_XLARGE, isZh ? L"超大图标(&X)" : L"Extra large icons(&X)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_LARGE, isZh ? L"大图标(&R)" : L"Large icons(&R)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_MEDIUM, isZh ? L"中等图标(&M)" : L"Medium icons(&M)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_SMALL, isZh ? L"小图标(&N)" : L"Small icons(&N)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_LIST, isZh ? L"列表(&L)" : L"List(&L)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_DETAILS, isZh ? L"详细信息(&D)" : L"Details(&D)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_TILES, isZh ? L"平铺(&S)" : L"Tiles(&S)");

    int checkViewCmd = CMD_BG_VIEW_DETAILS;
    if (currentViewMode == 0) checkViewCmd = CMD_BG_VIEW_DETAILS;
    else if (currentViewMode == 1) checkViewCmd = CMD_BG_VIEW_LARGE;
    else if (currentViewMode == 2) checkViewCmd = CMD_BG_VIEW_SMALL;
    else if (currentViewMode == 3) checkViewCmd = CMD_BG_VIEW_LIST;
    else if (currentViewMode == 4) checkViewCmd = CMD_BG_VIEW_TILES;
    CheckMenuRadioItem(hSubView, CMD_BG_VIEW_DETAILS, CMD_BG_VIEW_TILES, checkViewCmd, MF_BYCOMMAND);
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubView, isZh ? L"查看(&V)" : L"View(&V)");

    // 2. Sort by submenu
    HMENU hSubSort = CreatePopupMenu();
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_NAME, isZh ? L"名称" : L"Name");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_DATE, isZh ? L"修改日期" : L"Date modified");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_TYPE, isZh ? L"类型" : L"Type");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_SIZE, isZh ? L"大小" : L"Size");
    AppendMenuW(hSubSort, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_ASCENDING, isZh ? L"递增" : L"Ascending");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_DESCENDING, isZh ? L"递减" : L"Descending");

    int checkSortCmd = CMD_BG_SORT_NAME + std::clamp(currentSortCol, 0, 3);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_NAME, CMD_BG_SORT_SIZE, checkSortCmd, MF_BYCOMMAND);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_ASCENDING, CMD_BG_SORT_DESCENDING,
                       sortAsc ? CMD_BG_SORT_ASCENDING : CMD_BG_SORT_DESCENDING, MF_BYCOMMAND);
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubSort, isZh ? L"排序方式(&O)" : L"Sort by(&O)");

    // 3. Refresh
    AppendMenuW(hMenu, MF_STRING, CMD_BG_REFRESH, isZh ? L"刷新(&E)\\tF5" : L"Refresh(&E)\\tF5");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // 4. Paste / Paste Shortcut / Undo
    bool canPaste = CanPasteFromClipboard() && currentDir != L"This PC" && currentDir != L"此电脑";
    AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), CMD_BG_PASTE, isZh ? L"粘贴(&P)\\tCtrl+V" : L"Paste(&P)\\tCtrl+V");
    AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), CMD_BG_PASTE_SHORTCUT, isZh ? L"粘贴快捷方式(&S)" : L"Paste shortcut(&S)");
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, CMD_BG_UNDO, isZh ? L"撤消(&U)\\tCtrl+Z" : L"Undo(&U)\\tCtrl+Z");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // 5. Query Shell Background Context Menu
    IContextMenu* pContextMenu = nullptr;
    IShellFolder* pFolder = nullptr;
    PIDLIST_ABSOLUTE pidl = nullptr;

    if (!currentDir.empty() && currentDir != L"This PC" && currentDir != L"此电脑") {
        pidl = ILCreateFromPathW(Util::NormalizePath(currentDir).c_str());
        if (pidl) {
            SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void**)&pFolder);
            if (pFolder) {
                DEFCONTEXTMENU dcm = { 0 };
                dcm.hwnd = hwnd;
                dcm.pcmcb = nullptr;
                dcm.pidlFolder = pidl;
                dcm.psf = pFolder;
                dcm.cidl = 0;
                dcm.apidl = nullptr;
                if (FAILED(SHCreateDefaultContextMenu(&dcm, IID_IContextMenu, (void**)&pContextMenu)) || !pContextMenu) {
                    pFolder->CreateViewObject(hwnd, IID_IContextMenu, (void**)&pContextMenu);
                }
            }
        }
    }

    if (pContextMenu) {
        pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu2));
        pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu3));
        int shellPos = GetMenuItemCount(hMenu);
        pContextMenu->QueryContextMenu(hMenu, shellPos, 1, 0x6FFF, CMF_NORMAL);
    } else {
        // Fallback New submenu if shell COM not available
        HMENU hSubNew = CreatePopupMenu();
        AppendMenuW(hSubNew, MF_STRING, CMD_BG_NEW_FOLDER, I18n::Get().T(StringId::MenuNewFolder));
        AppendMenuW(hSubNew, MF_STRING, CMD_BG_NEW_FILE, I18n::Get().T(StringId::MenuNewFile));
        AppendMenuW(hSubNew, MF_STRING, CMD_BG_NEW_MD, isZh ? L"新建 Markdown 文档 (.md)" : L"New Markdown Document (.md)");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubNew, isZh ? L"新建(&W)" : L"New(&W)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, CMD_BG_PROPERTIES, I18n::Get().T(StringId::MenuProperties));
    }

    // Append NeoMDIE specific commands
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_SETTINGS, isZh ? L"选项与设置(&S)..." : L"Options & Settings(&S)...");

    SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

    RemoveWindowSubclass(hwnd, ContextMenuSubclassProc, 9999);

    if (g_pContextMenu3) { g_pContextMenu3->Release(); g_pContextMenu3 = nullptr; }
    if (g_pContextMenu2) { g_pContextMenu2->Release(); g_pContextMenu2 = nullptr; }

    DestroyMenu(hMenu);

    if (cmd >= 0x7100) {
        if (pContextMenu) { pContextMenu->Release(); pContextMenu = nullptr; }
        if (pFolder) { pFolder->Release(); pFolder = nullptr; }
        if (pidl) { ILFree(pidl); pidl = nullptr; }

        if (onCustomCommand) {
            onCustomCommand(cmd, currentDir);
        }
        return true;
    } else if (cmd > 0 && pContextMenu) {
        CMINVOKECOMMANDINFOEX cmi = { 0 };
        cmi.cbSize = sizeof(cmi);
        cmi.fMask = CMIC_MASK_UNICODE;
        cmi.hwnd = hwnd;
        cmi.lpVerb = MAKEINTRESOURCEA(cmd - 1);
        cmi.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
        cmi.nShow = SW_SHOWNORMAL;
        pContextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
    }

    if (pContextMenu) pContextMenu->Release();
    if (pFolder) pFolder->Release();
    if (pidl) ILFree(pidl);

    return true;
}
'''
src = src[:bg_start] + new_bg_menu + src[bg_end:]

# Update SetExplorerReplaced and add CheckForUpdates
repl_start = src.find('bool SetExplorerReplaced(bool enable) {')
assert repl_start != -1

new_replaced_and_update = '''bool SetExplorerReplaced(bool enable) {
    const wchar_t* shellKeys[] = {
        L"Software\\\\Classes\\\\Directory\\\\shell",
        L"Software\\\\Classes\\\\Folder\\\\shell",
        L"Software\\\\Classes\\\\Drive\\\\shell"
    };

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmdStr = L"\\\"" + std::wstring(exePath) + L"\\\" \\\"%1\\\"";

        for (const auto* k : shellKeys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                const wchar_t* defaultVerb = L"open";
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)defaultVerb, (DWORD)((wcslen(defaultVerb) + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }

            std::wstring cmdKeyPath = std::wstring(k) + L"\\\\open\\\\command";
            if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)cmdStr.c_str(), (DWORD)((cmdStr.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }
        }
    } else {
        for (const auto* k : shellKeys) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, k, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, nullptr);
                RegCloseKey(hKey);
            }
        }
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\\\Classes\\\\Directory\\\\shell\\\\open\\\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\\\Classes\\\\Directory\\\\shell\\\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\\\Classes\\\\Folder\\\\shell\\\\open\\\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\\\Classes\\\\Folder\\\\shell\\\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\\\Classes\\\\Drive\\\\shell\\\\open\\\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\\\Classes\\\\Drive\\\\shell\\\\open");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

void CheckForUpdates(HWND hwndParent) {
    bool isZh = I18n::Get().IsChinese();
    std::thread([hwndParent, isZh]() {
        HINTERNET hInternet = InternetOpenW(L"NeoMDIE-UpdateChecker", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!hInternet) {
            MessageBoxW(hwndParent,
                isZh ? L"无法初始化网络连接以检查更新。" : L"Failed to initialize internet connection.",
                isZh ? L"检查更新" : L"Check for Updates", MB_OK | MB_ICONWARNING);
            return;
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
        HINTERNET hUrl = InternetOpenUrlW(hInternet,
            L"https://api.github.com/repos/harry10086/NeoMDIE/releases/latest",
            L"User-Agent: NeoMDIE-UpdateChecker\\r\\nAccept: application/vnd.github.v3+json\\r\\n",
            (DWORD)-1, flags, 0);

        if (!hUrl) {
            InternetCloseHandle(hInternet);
            int res = MessageBoxW(hwndParent,
                isZh ? L"无法连接到 GitHub 检查更新。\\n\\n是否直接前往 GitHub 仓库 Releases 页面？"
                     : L"Unable to connect to GitHub.\\n\\nOpen Releases page in browser?",
                isZh ? L"检查更新" : L"Check for Updates", MB_YESNO | MB_ICONQUESTION);
            if (res == IDYES) {
                ShellExecuteW(nullptr, L"open", L"https://github.com/harry10086/NeoMDIE/releases", nullptr, nullptr, SW_SHOWNORMAL);
            }
            return;
        }

        std::string json;
        char buffer[2048];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\\0';
            json.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        std::string tagName;
        size_t tagPos = json.find("\\\"tag_name\\\":");
        if (tagPos != std::string::npos) {
            size_t start = json.find("\\\"", tagPos + 11);
            if (start != std::string::npos) {
                size_t end = json.find("\\\"", start + 1);
                if (end != std::string::npos) {
                    tagName = json.substr(start + 1, end - start - 1);
                }
            }
        }

        std::string htmlUrl = "https://github.com/harry10086/NeoMDIE/releases";
        size_t urlPos = json.find("\\\"html_url\\\":");
        if (urlPos != std::string::npos) {
            size_t start = json.find("\\\"", urlPos + 11);
            if (start != std::string::npos) {
                size_t end = json.find("\\\"", start + 1);
                if (end != std::string::npos) {
                    htmlUrl = json.substr(start + 1, end - start - 1);
                }
            }
        }

        std::wstring wTagName = Util::Utf8ToWide(tagName);
        std::wstring wHtmlUrl = Util::Utf8ToWide(htmlUrl);

        const std::wstring currentVer = L"v1.0.0";
        if (!wTagName.empty() && wTagName != currentVer) {
            std::wstring msg = isZh
                ? (L"发现 NeoMDIE 新版本: " + wTagName + L" (当前版本: " + currentVer + L")\\n\\n是否立即前往 GitHub 下载更新？")
                : (L"New version found: " + wTagName + L" (Current: " + currentVer + L")\\n\\nOpen GitHub release page to download?");
            int res = MessageBoxW(hwndParent, msg.c_str(), isZh ? L"发现新版本" : L"Update Available", MB_YESNO | MB_ICONINFORMATION);
            if (res == IDYES) {
                ShellExecuteW(nullptr, L"open", wHtmlUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else {
            std::wstring msg = isZh
                ? (L"您当前使用的是最新版本 (" + currentVer + L")。\\n\\n没有可用的更新。")
                : (L"You are using the latest version (" + currentVer + L").\\n\\nNo updates available.");
            MessageBoxW(hwndParent, msg.c_str(), isZh ? L"检查更新" : L"Check for Updates", MB_OK | MB_ICONINFORMATION);
        }
    }).detach();
}

} // namespace ShellUtils
'''
src = src[:repl_start] + new_replaced_and_update

with open('src/core/ShellUtils.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(src)
print('ShellUtils.cpp updated')
