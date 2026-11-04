# -*- coding: utf-8 -*-
with open(r'src\core\ShellUtils.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# In ShellUtils.cpp: update ShowContextMenu for root drives & CMF_ASYNCVERBSTATE
old_su = """    bool shown = false;
    std::wstring firstPath = Util::NormalizePath(paths[0]);
    PIDLIST_ABSOLUTE pidlFirst = ILCreateFromPathW(firstPath.c_str());

    IShellFolder* pFolder = nullptr;
    PCUITEMID_CHILD pidlFirstChild = nullptr;

    if (pidlFirst) {
        if (SUCCEEDED(SHBindToParent(pidlFirst, IID_IShellFolder, (void**)&pFolder, &pidlFirstChild)) && pFolder) {"""

new_su = """    bool shown = false;
    std::wstring firstPath = Util::NormalizePath(paths[0]);
    PIDLIST_ABSOLUTE pidlFirst = nullptr;
    if (firstPath == L"This PC" || firstPath == L"此电脑" || firstPath == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
        SHParseDisplayName(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", nullptr, &pidlFirst, 0, nullptr);
    } else {
        pidlFirst = ILCreateFromPathW(firstPath.c_str());
        if (!pidlFirst) {
            SHParseDisplayName(firstPath.c_str(), nullptr, &pidlFirst, 0, nullptr);
        }
    }

    IShellFolder* pFolder = nullptr;
    PCUITEMID_CHILD pidlFirstChild = nullptr;

    if (pidlFirst) {
        if (FAILED(SHBindToParent(pidlFirst, IID_IShellFolder, (void**)&pFolder, &pidlFirstChild)) || !pFolder) {
            if (SUCCEEDED(SHGetDesktopFolder(&pFolder)) && pFolder) {
                pidlFirstChild = pidlFirst;
            }
        }
        if (pFolder && pidlFirstChild) {"""

old_cmf = """pContextMenu->QueryContextMenu(hMenu, topItemsCount, 1, 0x6FFF, CMF_NORMAL | CMF_CANRENAME);"""
new_cmf = """pContextMenu->QueryContextMenu(hMenu, topItemsCount, 1, 0x6FFF, CMF_ASYNCVERBSTATE | CMF_CANRENAME);"""

old_split_text = """AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_SPLIT, isZh ? L"在分屏中打开(&S)" : L"Open in Split Pane(&S)");"""
new_split_text = """AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_SPLIT, isZh ? L"在新栏中打开(&S)" : L"Open in New Pane(&S)");"""

for old_s, new_s in [(old_su, new_su), (old_cmf, new_cmf), (old_split_text, new_split_text)]:
    old_crlf = old_s.replace('\n', '\r\n')
    new_crlf = new_s.replace('\n', '\r\n')
    if old_crlf in text:
        text = text.replace(old_crlf, new_crlf, 1)
    elif old_s in text:
        text = text.replace(old_s, new_s, 1)
    else:
        print("ShellUtils block not found:", old_s[:40])

with open(r'src\core\ShellUtils.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print("ShellUtils.cpp patched!")

# In DirectoryTree.cpp: update TreeSubclassProc
with open(r'src\ui\DirectoryTree.cpp', 'r', encoding='utf-8') as f:
    dt_text = f.read()

old_dt = """LRESULT CALLBACK DirectoryTree::TreeSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    DirectoryTree* pThis = (DirectoryTree*)dwRefData;

    switch (msg) {
    case WM_RBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TVHITTESTINFO tvhti = { 0 };
        tvhti.pt = pt;
        HTREEITEM hHit = TreeView_HitTest(hwnd, &tvhti);
        if (hHit) {
            TreeView_SelectItem(hwnd, hHit);
        }
        break;
    }

    case WM_RBUTTONUP:
    case WM_CONTEXTMENU: {
        POINT ptScreen;
        if (msg == WM_CONTEXTMENU) {
            ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (ptScreen.x == -1 && ptScreen.y == -1) {
                HTREEITEM hSel = TreeView_GetSelection(hwnd);
                RECT rcItem = { 0 };
                *(HTREEITEM*)&rcItem = hSel;
                if (TreeView_GetItemRect(hwnd, hSel, &rcItem, TRUE)) {
                    ptScreen.x = rcItem.left;
                    ptScreen.y = rcItem.bottom;
                    ClientToScreen(hwnd, &ptScreen);
                } else {
                    GetCursorPos(&ptScreen);
                }
            }
        } else {
            GetCursorPos(&ptScreen);
        }

        HTREEITEM hSel = TreeView_GetSelection(hwnd);
        if (hSel) {
            TVITEMW item = { 0 };
            item.hItem = hSel;
            item.mask = TVIF_PARAM;
            if (TreeView_GetItem(hwnd, &item) && item.lParam) {
                TreeNodeData* pData = (TreeNodeData*)item.lParam;
                if (pData && !pData->path.empty() && pData->path != L"This PC" && pData->path != L"QuickAccess") {
                    ShellUtils::ShowContextMenu(pThis->m_hwndParent ? pThis->m_hwndParent : hwnd,
                                                { pData->path }, ptScreen,
                                                [pThis](int cmd, const std::wstring& targetPath) {
                                                    if (pThis->onCustomCommand) {
                                                        pThis->onCustomCommand(cmd, targetPath);
                                                    }
                                                });
                    return 0;
                }
            }
        }
        break;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}"""

new_dt = """LRESULT CALLBACK DirectoryTree::TreeSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    DirectoryTree* pThis = (DirectoryTree*)dwRefData;

    switch (msg) {
    case WM_RBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TVHITTESTINFO tvhti = { 0 };
        tvhti.pt = pt;
        HTREEITEM hHit = TreeView_HitTest(hwnd, &tvhti);
        if (!hHit) hHit = tvhti.hItem;
        if (hHit) {
            TreeView_SelectItem(hwnd, hHit);
        }
        return 0;
    }

    case WM_RBUTTONUP:
        return 0;

    case WM_CONTEXTMENU: {
        POINT ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (ptScreen.x == -1 && ptScreen.y == -1) {
            HTREEITEM hSel = TreeView_GetSelection(hwnd);
            RECT rcItem = { 0 };
            *(HTREEITEM*)&rcItem = hSel;
            if (TreeView_GetItemRect(hwnd, hSel, &rcItem, TRUE)) {
                ptScreen.x = rcItem.left;
                ptScreen.y = rcItem.bottom;
                ClientToScreen(hwnd, &ptScreen);
            } else {
                GetCursorPos(&ptScreen);
            }
        }

        HTREEITEM hSel = TreeView_GetSelection(hwnd);
        if (hSel) {
            TVITEMW item = { 0 };
            item.hItem = hSel;
            item.mask = TVIF_PARAM;
            if (TreeView_GetItem(hwnd, &item) && item.lParam) {
                TreeNodeData* pData = (TreeNodeData*)item.lParam;
                if (pData && !pData->path.empty() && pData->path != L"QuickAccess") {
                    std::wstring targetPath = (pData->path == L"This PC") ? L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}" : pData->path;
                    ShellUtils::ShowContextMenu(hwnd,
                                                { targetPath }, ptScreen,
                                                [pThis](int cmd, const std::wstring& targetPath) {
                                                    if (pThis->onCustomCommand) {
                                                        pThis->onCustomCommand(cmd, targetPath);
                                                    }
                                                });
                    return 0;
                }
            }
        }
        return 0;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}"""

old_crlf = old_dt.replace('\n', '\r\n')
new_crlf = new_dt.replace('\n', '\r\n')
if old_crlf in dt_text:
    dt_text = dt_text.replace(old_crlf, new_crlf, 1)
elif old_dt in dt_text:
    dt_text = dt_text.replace(old_dt, new_dt, 1)
else:
    print("DirectoryTree block not found!")

with open(r'src\ui\DirectoryTree.cpp', 'w', encoding='utf-8') as f:
    f.write(dt_text)
print("DirectoryTree.cpp patched!")
