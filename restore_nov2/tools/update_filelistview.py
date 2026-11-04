import re

with open('src/ui/FileListView.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

# 1. SearchRecursive target hwnd
src = src.replace('HWND hwndTarget = m_hwndParent;', 'HWND hwndTarget = m_hwnd;')

# 2. Zebra and Gridlines in CustomDraw
old_subitem = '''                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;'''

new_subitem = '''                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;
                    if (Config::Get().zebraStriping && !isSelected && (row % 2 == 1)) {
                        pcd->clrTextBk = Theme::Get().IsDarkMode() ? RGB(38, 40, 44) : RGB(238, 243, 250);
                    }'''
assert old_subitem in src
src = src.replace(old_subitem, new_subitem, 1)

# 3. Update Scroll methods
old_scroll = '''void FileListView::ScrollToTop() {
    if (m_filteredIndices.empty()) return;
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(m_hwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(m_hwnd, 0, FALSE);
}

void FileListView::ScrollToBottom() {
    if (m_filteredIndices.empty()) return;
    int last = (int)m_filteredIndices.size() - 1;
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(m_hwnd, last, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(m_hwnd, last, FALSE);
}

void FileListView::ScrollPageUp() {
    if (m_hwnd) {
        SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEUP, 0);
    }
}

void FileListView::ScrollPageDown() {
    if (m_hwnd) {
        SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEDOWN, 0);
    }
}'''

new_scroll = '''void FileListView::ScrollToTop() {
    SendMessageW(m_hwnd, WM_KEYDOWN, VK_HOME, 0);
    SendMessageW(m_hwnd, WM_KEYUP, VK_HOME, 0);
}

void FileListView::ScrollToBottom() {
    SendMessageW(m_hwnd, WM_KEYDOWN, VK_END, 0);
    SendMessageW(m_hwnd, WM_KEYUP, VK_END, 0);
}

void FileListView::ScrollPageUp() {
    SendMessageW(m_hwnd, WM_KEYDOWN, VK_PRIOR, 0);
    SendMessageW(m_hwnd, WM_KEYUP, VK_PRIOR, 0);
}

void FileListView::ScrollPageDown() {
    SendMessageW(m_hwnd, WM_KEYDOWN, VK_NEXT, 0);
    SendMessageW(m_hwnd, WM_KEYUP, VK_NEXT, 0);
}'''
assert old_scroll in src
src = src.replace(old_scroll, new_scroll, 1)

# 4. Handle CMD_BG_NEW_MD
old_new_file = '''        case CMD_BG_NEW_FILE: {
            std::wstring newFile = Util::CombinePath(m_currentPath, I18n::Get().T(StringId::NewTextFile));
            int idx = 2;
            while (PathFileExistsW(newFile.c_str())) {
                newFile = Util::CombinePath(m_currentPath, L"New (" + std::to_wstring(idx++) + L").txt");
            }
            HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) CloseHandle(hF);
            Refresh();
            break;
        }'''

new_new_file = '''        case CMD_BG_NEW_FILE: {
            std::wstring newFile = Util::CombinePath(m_currentPath, I18n::Get().T(StringId::NewTextFile));
            int idx = 2;
            while (PathFileExistsW(newFile.c_str())) {
                newFile = Util::CombinePath(m_currentPath, L"New (" + std::to_wstring(idx++) + L").txt");
            }
            HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) CloseHandle(hF);
            Refresh();
            break;
        }
        case CMD_BG_NEW_MD: {
            bool isZh = I18n::Get().IsChinese();
            std::wstring newMd = Util::CombinePath(m_currentPath, isZh ? L"新建文档.md" : L"New Document.md");
            int idx = 2;
            while (PathFileExistsW(newMd.c_str())) {
                newMd = Util::CombinePath(m_currentPath, (isZh ? L"新建文档 (" : L"New Document (") + std::to_wstring(idx++) + L").md");
            }
            HANDLE hF = CreateFileW(newMd.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) CloseHandle(hF);
            Refresh();
            break;
        }'''
assert old_new_file in src
src = src.replace(old_new_file, new_new_file, 1)

with open('src/ui/FileListView.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(src)
print('FileListView.cpp updated')
