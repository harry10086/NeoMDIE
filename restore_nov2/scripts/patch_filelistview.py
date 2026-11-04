# -*- coding: utf-8 -*-
with open(r'src\ui\FileListView.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Update Refresh & SetFilter
old_block1 = """void FileListView::Refresh() {
    NavigateTo(m_currentPath, false);
}"""

new_block1 = """void FileListView::Refresh() {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }
    std::vector<NeoShellItem> items;
    if (ShellUtils::EnumerateDirectory(m_currentPath, m_showHidden, items)) {
        m_allItems = std::move(items);
        SortItems();
        ApplyFilter();
    }
}"""

old_block2 = """void FileListView::SetFilter(const std::wstring& query, bool subfolders, bool matchCase) {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }

    m_filterQuery = query;
    if (subfolders && !query.empty() && m_currentPath != L"This PC" && m_currentPath != L"此电脑") {
        m_searchCancelled = false;
        m_allItems.clear();
        m_filteredIndices.clear();
        ListView_SetItemCountEx(m_hwnd, 0, 0);
        InvalidateRect(m_hwnd, nullptr, TRUE);
        SearchRecursive(m_currentPath, query, matchCase);
    } else {
        if (m_allItems.empty() || m_allItems[0].fullPath.find(m_currentPath) == std::wstring::npos) {
            Refresh();
        } else {
            ApplyFilter();
        }
    }
}"""

new_block2 = """void FileListView::SetFilter(const std::wstring& query, bool subfolders, bool matchCase) {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }

    m_filterQuery = query;
    if (subfolders && !query.empty() && m_currentPath != L"This PC" && m_currentPath != L"此电脑") {
        m_searchCancelled = false;
        m_allItems.clear();
        m_filteredIndices.clear();
        ListView_SetItemCountEx(m_hwnd, 0, 0);
        InvalidateRect(m_hwnd, nullptr, TRUE);
        SearchRecursive(m_currentPath, query, matchCase);
    } else {
        if (query.empty() || m_allItems.empty()) {
            Refresh();
        } else {
            ApplyFilter();
        }
    }
}"""

# 2. Update double click on blank space in NM_DBLCLK
old_block3 = """            case NM_DBLCLK: {
                NMITEMACTIVATE* pia = (NMITEMACTIVATE*)lParam;
                pThis->OnItemDoubleClicked(pia->iItem);
                return 0;
            }"""

new_block3 = """            case NM_DBLCLK: {
                NMITEMACTIVATE* pia = (NMITEMACTIVATE*)lParam;
                if (pia->iItem < 0) {
                    pThis->GoUp();
                } else {
                    pThis->OnItemDoubleClicked(pia->iItem);
                }
                return 0;
            }"""

# 3. Update WM_LBUTTONDBLCLK in ListSubclassProc
old_block4 = """    switch (msg) {
    case WM_RBUTTONDOWN: {"""

new_block4 = """    switch (msg) {
    case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = { x, y };
        int hit = ListView_HitTest(hwnd, &lvhti);
        if (hit < 0) {
            pThis->GoUp();
            return 0;
        }
        break;
    }
    case WM_RBUTTONDOWN: {"""

for old_s, new_s in [(old_block1, new_block1), (old_block2, new_block2), (old_block3, new_block3), (old_block4, new_block4)]:
    old_crlf = old_s.replace('\n', '\r\n')
    new_crlf = new_s.replace('\n', '\r\n')
    if old_crlf in text:
        text = text.replace(old_crlf, new_crlf, 1)
    elif old_s in text:
        text = text.replace(old_s, new_s, 1)
    else:
        print("Block not found!")

with open(r'src\ui\FileListView.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("FileListView.cpp patched!")
