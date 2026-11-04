# -*- coding: utf-8 -*-
import sys

with open(r'src\ui\FileListView.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Update constructor
old_ctor = 'FileListView::FileListView() {\n}'
new_ctor = '''FileListView::FileListView() {
    m_activeColumns = { ColumnId::Name, ColumnId::DateModified, ColumnId::Type, ColumnId::Size };
}'''
assert old_ctor in content, "old_ctor not found"
content = content.replace(old_ctor, new_ctor, 1)

# 2. Add column helpers, RebuildColumns, ToggleColumn, ShowChooseColumnsDialog
target_init_columns = 'void FileListView::InitColumns() {'

replacement_columns_code = '''static std::wstring GetColumnTitle(ColumnId id) {
    bool isZh = I18n::Get().IsChinese();
    switch (id) {
    case ColumnId::Name:         return isZh ? L"名称" : L"Name";
    case ColumnId::DateModified: return isZh ? L"修改日期" : L"Date modified";
    case ColumnId::Type:         return isZh ? L"类型" : L"Type";
    case ColumnId::Size:         return isZh ? L"大小" : L"Size";
    case ColumnId::DateCreated:  return isZh ? L"创建日期" : L"Date created";
    case ColumnId::Attributes:   return isZh ? L"属性" : L"Attributes";
    case ColumnId::Authors:      return isZh ? L"作者" : L"Authors";
    case ColumnId::Title:        return isZh ? L"标题" : L"Title";
    case ColumnId::Tags:         return isZh ? L"标记" : L"Tags";
    case ColumnId::Path:         return isZh ? L"文件夹路径" : L"Folder path";
    default:                     return L"";
    }
}

static int GetDefaultColumnWidth(ColumnId id) {
    switch (id) {
    case ColumnId::Name:         return 300;
    case ColumnId::DateModified: return 160;
    case ColumnId::Type:         return 130;
    case ColumnId::Size:         return 120;
    case ColumnId::DateCreated:  return 160;
    case ColumnId::Attributes:   return 90;
    case ColumnId::Authors:      return 110;
    case ColumnId::Title:        return 130;
    case ColumnId::Tags:         return 110;
    case ColumnId::Path:         return 260;
    default:                     return 120;
    }
}

struct ChooseDetailsData {
    std::vector<ColumnId> activeCols;
    bool confirmed = false;
};

static LRESULT CALLBACK ChooseDetailsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ChooseDetailsData* pData = (ChooseDetailsData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_CREATE: {
        bool isZh = I18n::Get().IsChinese();
        HFONT hFont = Theme::Get().GetUIFont(hwnd);

        HWND hLabel = CreateWindowExW(0, WC_STATICW,
            isZh ? L"选择要在该文件夹中为此窗口显示的详细信息(&S):" : L"Select the details you want to display for the items in this folder(&S):",
            WS_CHILD | WS_VISIBLE, 15, 15, 340, 20, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
            15, 40, 340, 320, hwnd, (HMENU)1001, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_WIDTH;
        lvc.cx = 310;
        ListView_InsertColumn(hList, 0, &lvc);

        for (int i = 0; i < (int)ColumnId::Count; ++i) {
            ColumnId cid = (ColumnId)i;
            std::wstring title = GetColumnTitle(cid);
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.pszText = (LPWSTR)title.c_str();
            ListView_InsertItem(hList, &lvi);

            bool checked = false;
            if (pData) {
                for (auto c : pData->activeCols) {
                    if (c == cid) { checked = true; break; }
                }
            }
            if (cid == ColumnId::Name) checked = true;
            ListView_SetCheckState(hList, i, checked ? TRUE : FALSE);
        }

        HWND hBtnOk = CreateWindowExW(0, WC_BUTTONW, isZh ? L"确定" : L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            185, 375, 80, 28, hwnd, (HMENU)IDOK, nullptr, nullptr);
        SendMessageW(hBtnOk, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hBtnCancel = CreateWindowExW(0, WC_BUTTONW, isZh ? L"取消" : L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            275, 375, 80, 28, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
        SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK) {
            HWND hList = GetDlgItem(hwnd, 1001);
            if (pData && hList) {
                pData->activeCols.clear();
                pData->activeCols.push_back(ColumnId::Name);
                for (int i = 1; i < (int)ColumnId::Count; ++i) {
                    if (ListView_GetCheckState(hList, i)) {
                        pData->activeCols.push_back((ColumnId)i);
                    }
                }
                pData->confirmed = true;
            }
            DestroyWindow(hwnd);
            return 0;
        } else if (id == IDCANCEL) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void FileListView::ShowChooseColumnsDialog() {
    bool isZh = I18n::Get().IsChinese();
    WNDCLASSEXW wc = { sizeof(wc) };
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    if (!GetClassInfoExW(hInst, L"NeoMDIE_ChooseDetails", &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ChooseDetailsWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"NeoMDIE_ChooseDetails";
        RegisterClassExW(&wc);
    }

    ChooseDetailsData data;
    data.activeCols = m_activeColumns;

    RECT rcParent = { 0 };
    GetWindowRect(m_hwndParent ? m_hwndParent : m_hwnd, &rcParent);
    int dlgW = 385;
    int dlgH = 455;
    int x = rcParent.left + ((rcParent.right - rcParent.left) - dlgW) / 2;
    int y = rcParent.top + ((rcParent.bottom - rcParent.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, L"NeoMDIE_ChooseDetails",
        isZh ? L"选择详细信息" : L"Choose Details",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH,
        m_hwndParent ? m_hwndParent : m_hwnd,
        nullptr, hInst, &data
    );

    if (!hDlg) return;

    HWND hTopParent = GetAncestor(m_hwnd, GA_ROOT);
    EnableWindow(hTopParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hTopParent, TRUE);
    SetForegroundWindow(hTopParent);

    if (data.confirmed) {
        m_activeColumns = std::move(data.activeCols);
        RebuildColumns();
    }
}

void FileListView::RebuildColumns() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        int cnt = Header_GetItemCount(hHeader);
        for (int i = cnt - 1; i >= 0; --i) {
            ListView_DeleteColumn(m_hwnd, i);
        }
    }

    for (size_t i = 0; i < m_activeColumns.size(); ++i) {
        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = (int)i;
        lvc.cx = Theme::Get().Scale(GetDefaultColumnWidth(m_activeColumns[i]), m_hwnd);
        std::wstring title = GetColumnTitle(m_activeColumns[i]);
        lvc.pszText = (LPWSTR)title.c_str();
        ListView_InsertColumn(m_hwnd, (int)i, &lvc);
    }

    UpdateHeaderSortArrow();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::ToggleColumn(ColumnId id) {
    if (id == ColumnId::Name) return;
    auto it = std::find(m_activeColumns.begin(), m_activeColumns.end(), id);
    if (it != m_activeColumns.end()) {
        m_activeColumns.erase(it);
    } else {
        m_activeColumns.push_back(id);
    }
    RebuildColumns();
}

void FileListView::InitColumns() {'''

assert target_init_columns in content, "target_init_columns not found"
content = content.replace(target_init_columns, replacement_columns_code, 1)

# Replace body of InitColumns up to UpdateHeaderSortArrow
old_init_body = '''void FileListView::InitColumns() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowSubclass(hHeader, FileListView::HeaderSubclassProc, 1, (DWORD_PTR)this);
    }

    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;

    lvc.iSubItem = 0;
    lvc.cx = Theme::Get().Scale(320, m_hwnd);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColName);
    ListView_InsertColumn(m_hwnd, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = Theme::Get().Scale(160, m_hwnd);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColDateModified);
    ListView_InsertColumn(m_hwnd, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.cx = Theme::Get().Scale(140, m_hwnd);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColType);
    ListView_InsertColumn(m_hwnd, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.cx = Theme::Get().Scale(180, m_hwnd);
    lvc.fmt = LVCFMT_LEFT;
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColSize);
    ListView_InsertColumn(m_hwnd, 3, &lvc);

    UpdateHeaderSortArrow();
}'''

new_init_body = '''void FileListView::InitColumns() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowSubclass(hHeader, FileListView::HeaderSubclassProc, 1, (DWORD_PTR)this);
    }
    RebuildColumns();
}'''

assert old_init_body in content, "old_init_body not found"
content = content.replace(old_init_body, new_init_body, 1)

# Replace UpdateHeaderSortArrow
old_arrow = '''void FileListView::UpdateHeaderSortArrow() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (!hHeader) return;
    int colCount = Header_GetItemCount(hHeader);
    for (int i = 0; i < colCount; ++i) {
        HDITEMW hdi = { sizeof(hdi) };
        hdi.mask = HDI_FORMAT;
        Header_GetItem(hHeader, i, &hdi);
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == (int)m_sortCol) {
            hdi.fmt |= (m_sortAsc ? HDF_SORTUP : HDF_SORTDOWN);
        }
        Header_SetItem(hHeader, i, &hdi);
    }
}'''

new_arrow = '''void FileListView::UpdateHeaderSortArrow() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (!hHeader) return;
    int colCount = Header_GetItemCount(hHeader);
    for (int i = 0; i < colCount; ++i) {
        HDITEMW hdi = { sizeof(hdi) };
        hdi.mask = HDI_FORMAT;
        Header_GetItem(hHeader, i, &hdi);
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i < (int)m_activeColumns.size()) {
            ColumnId cid = m_activeColumns[i];
            bool match = false;
            switch (cid) {
            case ColumnId::Name:         match = (m_sortCol == SortColumn::Name); break;
            case ColumnId::DateModified: match = (m_sortCol == SortColumn::DateModified); break;
            case ColumnId::Type:         match = (m_sortCol == SortColumn::Type); break;
            case ColumnId::Size:         match = (m_sortCol == SortColumn::Size); break;
            case ColumnId::DateCreated:  match = (m_sortCol == SortColumn::DateCreated); break;
            case ColumnId::Attributes:   match = (m_sortCol == SortColumn::Attributes); break;
            case ColumnId::Authors:      match = (m_sortCol == SortColumn::Authors); break;
            case ColumnId::Title:        match = (m_sortCol == SortColumn::Title); break;
            default:                     match = false; break;
            }
            if (match) {
                hdi.fmt |= (m_sortAsc ? HDF_SORTUP : HDF_SORTDOWN);
            }
        }
        Header_SetItem(hHeader, i, &hdi);
    }
}'''

assert old_arrow in content, "old_arrow not found"
content = content.replace(old_arrow, new_arrow, 1)

# Replace ShowColumnHeaderMenu
old_menu = '''void FileListView::ShowColumnHeaderMenu(POINT ptScreen) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED | MF_GRAYED, 1, isZh ? L"名称" : L"Name");
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED, 2, isZh ? L"修改日期" : L"Date modified");
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED, 3, isZh ? L"类型" : L"Type");
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED, 4, isZh ? L"大小" : L"Size");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 10, isZh ? L"自动调整所有列宽(&A)" : L"Size All Columns to Fit(&A)");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 10) {
        HWND hHeader = ListView_GetHeader(m_hwnd);
        int cnt = Header_GetItemCount(hHeader);
        for (int i = 0; i < cnt; ++i) {
            ListView_SetColumnWidth(m_hwnd, i, LVSCW_AUTOSIZE_USEHEADER);
        }
    }
}'''

new_menu = '''void FileListView::ShowColumnHeaderMenu(POINT ptScreen) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();

    for (int i = 0; i < (int)ColumnId::Count; ++i) {
        ColumnId cid = (ColumnId)i;
        std::wstring title = GetColumnTitle(cid);
        UINT flags = MF_STRING;

        bool active = false;
        for (auto c : m_activeColumns) {
            if (c == cid) { active = true; break; }
        }
        if (active) flags |= MF_CHECKED;
        if (cid == ColumnId::Name) flags |= MF_GRAYED;

        AppendMenuW(hMenu, flags, 100 + i, title.c_str());
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 10, isZh ? L"自动调整所有列宽(&A)" : L"Size All Columns to Fit(&A)");
    AppendMenuW(hMenu, MF_STRING, 20, isZh ? L"其他(&M)..." : L"More(&M)...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd >= 100 && cmd < 100 + (int)ColumnId::Count) {
        ColumnId cid = (ColumnId)(cmd - 100);
        if (cid != ColumnId::Name) {
            ToggleColumn(cid);
        }
    } else if (cmd == 10) {
        HWND hHeader = ListView_GetHeader(m_hwnd);
        int cnt = Header_GetItemCount(hHeader);
        for (int i = 0; i < cnt; ++i) {
            ListView_SetColumnWidth(m_hwnd, i, LVSCW_AUTOSIZE_USEHEADER);
        }
    } else if (cmd == 20) {
        ShowChooseColumnsDialog();
    }
}'''

assert old_menu in content, "old_menu not found"
content = content.replace(old_menu, new_menu, 1)

# Replace UpdateColumnsLanguage
old_lang = '''void FileListView::UpdateColumnsLanguage() {
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT;
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColName);
    ListView_SetColumn(m_hwnd, 0, &lvc);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColDateModified);
    ListView_SetColumn(m_hwnd, 1, &lvc);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColType);
    ListView_SetColumn(m_hwnd, 2, &lvc);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColSize);
    ListView_SetColumn(m_hwnd, 3, &lvc);
}'''

new_lang = '''void FileListView::UpdateColumnsLanguage() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (!hHeader) return;
    for (size_t i = 0; i < m_activeColumns.size(); ++i) {
        std::wstring title = GetColumnTitle(m_activeColumns[i]);
        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT;
        lvc.pszText = (LPWSTR)title.c_str();
        ListView_SetColumn(m_hwnd, (int)i, &lvc);
    }
}'''

assert old_lang in content, "old_lang not found"
content = content.replace(old_lang, new_lang, 1)

# Replace LVN_GETDISPINFOW
old_disp = '''                    if (pDisp->item.mask & LVIF_TEXT) {
                        static wchar_t s_buf[MAX_PATH];
                        s_buf[0] = L'\\0';
                        switch (pDisp->item.iSubItem) {
                        case 0: StringCchCopyW(s_buf, MAX_PATH, item.name.c_str()); break;
                        case 1: StringCchCopyW(s_buf, MAX_PATH, Util::FormatFileTime(item.modifiedTime).c_str()); break;
                        case 2: StringCchCopyW(s_buf, MAX_PATH, item.typeName.c_str()); break;
                        case 3:
                            if (item.isDrive) {
                                uint64_t freeBytes = 0, totalBytes = 0;
                                if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                    StringCchPrintfW(s_buf, MAX_PATH, L"%s 可用 / 共 %s",
                                                     Util::FormatBytes(freeBytes).c_str(),
                                                     Util::FormatBytes(totalBytes).c_str());
                                }
                            } else if (item.isFolder && !item.isSizeCalculated) {
                                s_buf[0] = L'\\0';
                            } else {
                                StringCchCopyW(s_buf, MAX_PATH, Util::FormatBytes(item.size).c_str());
                            }
                            break;
                        }
                        pDisp->item.pszText = s_buf;
                    }'''

new_disp = '''                    if (pDisp->item.mask & LVIF_TEXT) {
                        static wchar_t s_buf[MAX_PATH * 2];
                        s_buf[0] = L'\\0';
                        int colIdx = pDisp->item.iSubItem;
                        if (colIdx >= 0 && colIdx < (int)pThis->m_activeColumns.size()) {
                            ColumnId colId = pThis->m_activeColumns[colIdx];
                            switch (colId) {
                            case ColumnId::Name:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.name.c_str());
                                break;
                            case ColumnId::DateModified:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatFileTime(item.modifiedTime).c_str());
                                break;
                            case ColumnId::Type:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.typeName.c_str());
                                break;
                            case ColumnId::Size:
                                if (item.isDrive) {
                                    uint64_t freeBytes = 0, totalBytes = 0;
                                    if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                        StringCchPrintfW(s_buf, ARRAYSIZE(s_buf), L"%s 可用 / 共 %s",
                                                         Util::FormatBytes(freeBytes).c_str(),
                                                         Util::FormatBytes(totalBytes).c_str());
                                    }
                                } else if (item.isFolder && !item.isSizeCalculated) {
                                    s_buf[0] = L'\\0';
                                } else {
                                    StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatBytes(item.size).c_str());
                                }
                                break;
                            case ColumnId::DateCreated:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatFileTime(item.createdTime).c_str());
                                break;
                            case ColumnId::Attributes:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatAttributes(item.attributes).c_str());
                                break;
                            case ColumnId::Authors:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.authors.c_str());
                                break;
                            case ColumnId::Title:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.title.c_str());
                                break;
                            case ColumnId::Tags:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.tags.c_str());
                                break;
                            case ColumnId::Path:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.fullPath.c_str());
                                break;
                            }
                        }
                        pDisp->item.pszText = s_buf;
                    }'''

assert old_disp in content, "old_disp not found"
content = content.replace(old_disp, new_disp, 1)

# Replace LVN_COLUMNCLICK
old_colclick = '''            case LVN_COLUMNCLICK: {
                NMLISTVIEW* pnlv = (NMLISTVIEW*)lParam;
                SortColumn newCol = (SortColumn)pnlv->iSubItem;
                if (pThis->m_sortCol == newCol) {
                    pThis->m_sortAsc = !pThis->m_sortAsc;
                } else {
                    pThis->m_sortCol = newCol;
                    pThis->m_sortAsc = true;
                }
                pThis->SortItems();
                pThis->ApplyFilter();
                pThis->UpdateHeaderSortArrow();
                return 0;
            }'''

new_colclick = '''            case LVN_COLUMNCLICK: {
                NMLISTVIEW* pnlv = (NMLISTVIEW*)lParam;
                if (pnlv->iSubItem >= 0 && pnlv->iSubItem < (int)pThis->m_activeColumns.size()) {
                    ColumnId colId = pThis->m_activeColumns[pnlv->iSubItem];
                    SortColumn clickedCol = SortColumn::Name;
                    switch (colId) {
                    case ColumnId::Name:         clickedCol = SortColumn::Name; break;
                    case ColumnId::DateModified: clickedCol = SortColumn::DateModified; break;
                    case ColumnId::Type:         clickedCol = SortColumn::Type; break;
                    case ColumnId::Size:         clickedCol = SortColumn::Size; break;
                    case ColumnId::DateCreated:  clickedCol = SortColumn::DateCreated; break;
                    case ColumnId::Attributes:   clickedCol = SortColumn::Attributes; break;
                    case ColumnId::Authors:      clickedCol = SortColumn::Authors; break;
                    case ColumnId::Title:        clickedCol = SortColumn::Title; break;
                    default:                     clickedCol = SortColumn::Name; break;
                    }

                    if (pThis->m_sortCol == clickedCol) {
                        pThis->m_sortAsc = !pThis->m_sortAsc;
                    } else {
                        pThis->m_sortCol = clickedCol;
                        pThis->m_sortAsc = true;
                    }
                    pThis->SortItems();
                    pThis->ApplyFilter();
                    pThis->UpdateHeaderSortArrow();
                }
                return 0;
            }'''

assert old_colclick in content, "old_colclick not found"
content = content.replace(old_colclick, new_colclick, 1)

with open(r'src\ui\FileListView.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("FileListView.cpp updated successfully!")
