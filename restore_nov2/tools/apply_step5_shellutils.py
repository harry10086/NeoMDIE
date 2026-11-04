with open('src/core/ShellUtils.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

old_proc = r"""static LRESULT CALLBACK ContextMenuSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
    case WM_INITMENUPOPUP:
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:"""

new_proc = r"""static LRESULT CALLBACK ContextMenuSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    case WM_INITMENUPOPUP:
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:"""

assert old_proc in text, "old_proc not found in ShellUtils.cpp"
text = text.replace(old_proc, new_proc)

old_showctx = r"""                int topItemsCount = GetMenuItemCount(hMenu);
                pContextMenu->QueryContextMenu(hMenu, topItemsCount, 1, 0x6FFF, CMF_NORMAL | CMF_CANRENAME);

                SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);

                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                         ptScreen.x, ptScreen.y, 0, hwnd, nullptr);"""

new_showctx = r"""                int topItemsCount = GetMenuItemCount(hMenu);
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                pContextMenu->QueryContextMenu(hMenu, topItemsCount, 1, 0x6FFF, CMF_NORMAL | CMF_CANRENAME);
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));

                SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);

                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                         ptScreen.x, ptScreen.y, 0, hwnd, nullptr);"""

assert old_showctx in text, "old_showctx not found in ShellUtils.cpp"
text = text.replace(old_showctx, new_showctx)

old_bgcom = r"""    // 5. Query Shell Background Context Menu
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
        pContextMenu->QueryContextMenu(hMenu, shellPos, 1, 0x6FFF, CMF_NORMAL);"""

new_bgcom = r"""    // 5. Query Shell Background Context Menu (Modern IShellItem pipeline)
    IContextMenu* pContextMenu = nullptr;
    IShellFolder* pFolder = nullptr;

    if (!currentDir.empty() && currentDir != L"This PC" && currentDir != L"此电脑") {
        IShellItem* pItem = nullptr;
        HRESULT hr = SHCreateItemFromParsingName(Util::NormalizePath(currentDir).c_str(), nullptr, IID_PPV_ARGS(&pItem));
        if (SUCCEEDED(hr) && pItem) {
            hr = pItem->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&pFolder));
            if (SUCCEEDED(hr) && pFolder) {
                pFolder->CreateViewObject(hwnd, IID_PPV_ARGS(&pContextMenu));
            }
            pItem->Release();
        }
    }

    if (pContextMenu) {
        pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu2));
        pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu3));
        int shellPos = GetMenuItemCount(hMenu);
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        pContextMenu->QueryContextMenu(hMenu, shellPos, 1, 0x6FFF, CMF_NORMAL);
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));"""

assert old_bgcom in text, "old_bgcom not found in ShellUtils.cpp"
text = text.replace(old_bgcom, new_bgcom)

old_replace = r"""bool IsExplorerReplaced() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH * 2] = { 0 };
        DWORD size = sizeof(val);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, nullptr, nullptr, &type, (LPBYTE)val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::wstring s(val);
            if (s.find(L"NeoMDIE") != std::wstring::npos) {
                return true;
            }
        } else {
            RegCloseKey(hKey);
        }
    }
    return false;
}

bool SetExplorerReplaced(bool enable) {
    const wchar_t* shellKeys[] = {
        L"Software\\Classes\\Directory\\shell",
        L"Software\\Classes\\Folder\\shell",
        L"Software\\Classes\\Drive\\shell"
    };

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmdStr = L"\"" + std::wstring(exePath) + L"\" \"%1\"";

        for (const auto* k : shellKeys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                const wchar_t* defaultVerb = L"open";
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)defaultVerb, (DWORD)((wcslen(defaultVerb) + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }

            std::wstring cmdKeyPath = std::wstring(k) + L"\\open\\command";
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
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}"""

new_replace = r"""bool IsExplorerReplaced() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH] = { 0 };
        DWORD size = sizeof(val);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, nullptr, nullptr, &type, (LPBYTE)val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (wcscmp(val, L"open_NeoMDIE") == 0) {
                return true;
            }
        } else {
            RegCloseKey(hKey);
        }
    }
    return false;
}

bool SetExplorerReplaced(bool enable) {
    const wchar_t* shellKeys[] = {
        L"Software\\Classes\\Directory\\shell",
        L"Software\\Classes\\Folder\\shell",
        L"Software\\Classes\\Drive\\shell"
    };

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmdStr = L"\"" + std::wstring(exePath) + L"\" \"%1\"";

        for (const auto* k : shellKeys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                const wchar_t* defaultVerb = L"open_NeoMDIE";
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)defaultVerb, (DWORD)((wcslen(defaultVerb) + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }

            std::wstring cmdKeyPath = std::wstring(k) + L"\\open_NeoMDIE\\command";
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
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}"""

assert old_replace in text, "old_replace not found in ShellUtils.cpp"
text = text.replace(old_replace, new_replace)

with open('src/core/ShellUtils.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("ShellUtils.cpp updated successfully.")
