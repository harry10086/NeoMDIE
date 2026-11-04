with open('src/ui/DirectoryTree.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

old_select = r"""void DirectoryTree::SelectPath(const std::wstring& path) {
    if (m_syncing || !m_hwnd) return;
    m_syncing = true;

    if (path.empty() || path == L"此电脑" || path == L"This PC") {
        TreeView_SelectItem(m_hwnd, m_hThisPC);
        m_syncing = false;
        return;
    }

    // Try finding drive under This PC
    wchar_t drivePrefix[4] = { path[0], L':', L'\\', L'\0' };
    HTREEITEM hDrive = FindOrLoadChildPath(m_hThisPC, drivePrefix);
    if (hDrive) {
        TreeView_Expand(m_hwnd, m_hThisPC, TVE_EXPAND);

        std::wstring currentSub = drivePrefix;
        HTREEITEM hCurrent = hDrive;

        std::wistringstream ss(path.substr(3)); // skip "C:\"
        std::wstring segment;
        while (std::getline(ss, segment, L'\\')) {
            if (segment.empty()) continue;
            currentSub += (currentSub.back() == L'\\' ? L"" : L"\\") + segment;
            HTREEITEM hNext = FindOrLoadChildPath(hCurrent, currentSub);
            if (hNext) {
                TreeView_Expand(m_hwnd, hCurrent, TVE_EXPAND);
                hCurrent = hNext;
            } else {
                break;
            }
        }
        TreeView_SelectItem(m_hwnd, hCurrent);
    }

    m_syncing = false;
}"""

new_select = r"""void DirectoryTree::SelectPath(const std::wstring& path) {
    if (m_syncing || !m_hwnd) return;
    m_syncing = true;
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);

    if (path.empty() || path == L"此电脑" || path == L"This PC") {
        TreeView_SelectItem(m_hwnd, m_hThisPC);
        SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwnd, nullptr, TRUE);
        m_syncing = false;
        return;
    }

    // Try finding drive under This PC
    wchar_t drivePrefix[4] = { path[0], L':', L'\\', L'\0' };
    HTREEITEM hDrive = FindOrLoadChildPath(m_hThisPC, drivePrefix);
    if (hDrive) {
        TreeView_Expand(m_hwnd, m_hThisPC, TVE_EXPAND);

        std::wstring currentSub = drivePrefix;
        HTREEITEM hCurrent = hDrive;

        std::wistringstream ss(path.substr(3)); // skip "C:\"
        std::wstring segment;
        while (std::getline(ss, segment, L'\\')) {
            if (segment.empty()) continue;
            currentSub += (currentSub.back() == L'\\' ? L"" : L"\\") + segment;
            HTREEITEM hNext = FindOrLoadChildPath(hCurrent, currentSub);
            if (hNext) {
                TreeView_Expand(m_hwnd, hCurrent, TVE_EXPAND);
                hCurrent = hNext;
            } else {
                break;
            }
        }
        TreeView_SelectItem(m_hwnd, hCurrent);
    }

    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwnd, nullptr, TRUE);
    m_syncing = false;
}"""

assert old_select in text, "old_select not found in DirectoryTree.cpp"
text = text.replace(old_select, new_select)

old_populate = r"""void DirectoryTree::PopulateChildren(HTREEITEM hParent, const std::wstring& folderPath) {
    // Delete existing child items (including dummy)
    HTREEITEM hChild = TreeView_GetChild(m_hwnd, hParent);
    while (hChild) {
        HTREEITEM hNext = TreeView_GetNextSibling(m_hwnd, hChild);
        TreeView_DeleteItem(m_hwnd, hChild);
        hChild = hNext;
    }

    std::wstring search = Util::NormalizePath(folderPath) + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(search.c_str(), FindExInfoBasic, &fd,
                                    FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::pair<std::wstring, std::wstring>> subdirs;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
            std::wstring subPath = Util::NormalizePath(folderPath) + L"\\" + fd.cFileName;
            subdirs.push_back({ fd.cFileName, subPath });
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    for (const auto& item : subdirs) {
        TVINSERTSTRUCTW tvis = { 0 };
        tvis.hParent = hParent;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)item.first.c_str();
        tvis.item.iImage = ShellUtils::GetItemIconIndex(item.second, FILE_ATTRIBUTE_DIRECTORY, true);
        tvis.item.iSelectedImage = tvis.item.iImage;
        tvis.item.cChildren = 1;

        TreeNodeData* pData = new TreeNodeData{ item.second, false, false };
        m_nodeDataList.push_back(pData);
        tvis.item.lParam = (LPARAM)pData;

        HTREEITEM hSub = TreeView_InsertItem(m_hwnd, &tvis);

        // Dummy item for lazy load
        TVINSERTSTRUCTW dummy = { 0 };
        dummy.hParent = hSub;
        dummy.item.mask = TVIF_TEXT;
        dummy.item.pszText = (LPWSTR)L"";
        TreeView_InsertItem(m_hwnd, &dummy);
    }
}"""

new_populate = r"""void DirectoryTree::PopulateChildren(HTREEITEM hParent, const std::wstring& folderPath) {
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);

    // Delete existing child items (including dummy)
    HTREEITEM hChild = TreeView_GetChild(m_hwnd, hParent);
    while (hChild) {
        HTREEITEM hNext = TreeView_GetNextSibling(m_hwnd, hChild);
        TreeView_DeleteItem(m_hwnd, hChild);
        hChild = hNext;
    }

    std::wstring search = Util::NormalizePath(folderPath) + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(search.c_str(), FindExInfoBasic, &fd,
                                    FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (hFind == INVALID_HANDLE_VALUE) {
        SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
        return;
    }

    std::vector<std::pair<std::wstring, std::wstring>> subdirs;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
            std::wstring subPath = Util::NormalizePath(folderPath) + L"\\" + fd.cFileName;
            subdirs.push_back({ fd.cFileName, subPath });
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    int defaultFolderIcon = ShellUtils::GetItemIconIndex(L"", FILE_ATTRIBUTE_DIRECTORY, true);

    for (const auto& item : subdirs) {
        TVINSERTSTRUCTW tvis = { 0 };
        tvis.hParent = hParent;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)item.first.c_str();
        tvis.item.iImage = defaultFolderIcon;
        tvis.item.iSelectedImage = defaultFolderIcon;
        tvis.item.cChildren = 1;

        TreeNodeData* pData = new TreeNodeData{ item.second, false, false };
        m_nodeDataList.push_back(pData);
        tvis.item.lParam = (LPARAM)pData;

        HTREEITEM hSub = TreeView_InsertItem(m_hwnd, &tvis);

        // Dummy item for lazy load
        TVINSERTSTRUCTW dummy = { 0 };
        dummy.hParent = hSub;
        dummy.item.mask = TVIF_TEXT;
        dummy.item.pszText = (LPWSTR)L"";
        TreeView_InsertItem(m_hwnd, &dummy);
    }

    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
}"""

assert old_populate in text, "old_populate not found in DirectoryTree.cpp"
text = text.replace(old_populate, new_populate)

with open('src/ui/DirectoryTree.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("DirectoryTree.cpp updated successfully.")
