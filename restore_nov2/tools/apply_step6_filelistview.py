with open('src/ui/FileListView.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

old_theme = r"""    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, bg);
    ListView_SetTextColor(m_hwnd, text);"""

new_theme = r"""    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, CLR_NONE);
    ListView_SetTextColor(m_hwnd, text);"""

assert old_theme in text, "old_theme not found in FileListView.cpp"
text = text.replace(old_theme, new_theme)

old_scroll = r"""void FileListView::ScrollPageUp() {
    SendMessageW(m_hwnd, WM_KEYDOWN, VK_PRIOR, 0);
    SendMessageW(m_hwnd, WM_KEYUP, VK_PRIOR, 0);
}

void FileListView::ScrollPageDown() {
    SendMessageW(m_hwnd, WM_KEYDOWN, VK_NEXT, 0);
    SendMessageW(m_hwnd, WM_KEYUP, VK_NEXT, 0);
}"""

new_scroll = r"""void FileListView::ScrollPageUp() {
    int pageCount = ListView_GetCountPerPage(m_hwnd);
    if (pageCount <= 0) pageCount = 10;
    RECT rcItem = { 0 };
    ListView_GetItemRect(m_hwnd, 0, &rcItem, LVIR_BOUNDS);
    int itemH = (rcItem.bottom > rcItem.top) ? (rcItem.bottom - rcItem.top) : 22;
    ListView_Scroll(m_hwnd, 0, -pageCount * itemH);
}

void FileListView::ScrollPageDown() {
    int pageCount = ListView_GetCountPerPage(m_hwnd);
    if (pageCount <= 0) pageCount = 10;
    RECT rcItem = { 0 };
    ListView_GetItemRect(m_hwnd, 0, &rcItem, LVIR_BOUNDS);
    int itemH = (rcItem.bottom > rcItem.top) ? (rcItem.bottom - rcItem.top) : 22;
    ListView_Scroll(m_hwnd, 0, pageCount * itemH);
}"""

assert old_scroll in text, "old_scroll not found in FileListView.cpp"
text = text.replace(old_scroll, new_scroll)

old_search_target = r"""void FileListView::SearchRecursive(const std::wstring& rootDir, const std::wstring& query, bool matchCase) {
    HWND hwndTarget = m_hwnd;"""

new_search_target = r"""void FileListView::SearchRecursive(const std::wstring& rootDir, const std::wstring& query, bool matchCase) {
    HWND hwndTarget = m_hwndParent ? m_hwndParent : m_hwnd;"""

assert old_search_target in text, "old_search_target not found in FileListView.cpp"
text = text.replace(old_search_target, new_search_target)

old_list_subclass = r"""        } else if (wParam == VK_BACK) {
            pThis->GoUp();
            return 0;
        }
        break;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}"""

new_list_subclass = r"""        } else if (wParam == VK_BACK) {
            pThis->GoUp();
            return 0;
        }
        break;
    }
    case FileListView::WM_SEARCH_BATCH: {
        auto* pBatch = (std::vector<NeoShellItem>*)lParam;
        if (pBatch) {
            for (auto& item : *pBatch) {
                pThis->m_allItems.push_back(std::move(item));
            }
            delete pBatch;
            pThis->ApplyFilter();
        }
        return 0;
    }
    case FileListView::WM_SEARCH_COMPLETE: {
        pThis->SortItems();
        pThis->ApplyFilter();
        return 0;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}"""

assert old_list_subclass in text, "old_list_subclass not found in FileListView.cpp"
text = text.replace(old_list_subclass, new_list_subclass)

with open('src/ui/FileListView.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("FileListView.cpp updated successfully.")
