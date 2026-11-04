with open("src/ui/FileListView.cpp", "r", encoding="utf-8") as f:
    flv = f.read()

# 1. Update ApplyTheme to handle Gridlines and proper dark background
old_apply_theme = """void FileListView::ApplyTheme() {
    if (!m_hwnd) return;
    bool isDark = Theme::Get().IsDarkMode();
    Theme::Get().ApplyToWindow(m_hwnd);
    COLORREF bg = Theme::Get().ClrBackground();
    COLORREF text = Theme::Get().ClrTextPrimary();
    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, bg);
    ListView_SetTextColor(m_hwnd, text);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowTheme(hHeader, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
}"""

new_apply_theme = """void FileListView::ApplyTheme() {
    if (!m_hwnd) return;
    bool isDark = Theme::Get().IsDarkMode();
    Theme::Get().ApplyToWindow(m_hwnd);
    COLORREF bg = Theme::Get().ClrBackground();
    COLORREF text = Theme::Get().ClrTextPrimary();
    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, bg);
    ListView_SetTextColor(m_hwnd, text);

    DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_SUBITEMIMAGES;
    if (Config::Get().showGridLines) {
        exStyle |= LVS_EX_GRIDLINES;
    }
    ListView_SetExtendedListViewStyle(m_hwnd, exStyle);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowTheme(hHeader, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
}"""
flv = flv.replace(old_apply_theme, new_apply_theme)

# 2. Update NM_CUSTOMDRAW for zebra striping
old_custom_draw = """                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                    return CDRF_NOTIFYSUBITEMDRAW;
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {"""

new_custom_draw = """                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    if (Config::Get().zebraStriping && (pcd->nmcd.dwItemSpec % 2 == 1)) {
                        COLORREF altBg = Theme::Get().IsDarkMode() ? RGB(45, 45, 45) : RGB(242, 245, 250);
                        pcd->clrTextBk = altBg;
                    } else {
                        pcd->clrTextBk = Theme::Get().ClrBackground();
                    }
                    pcd->clrText = Theme::Get().ClrTextPrimary();
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {"""
flv = flv.replace(old_custom_draw, new_custom_draw)

# 3. Add WM_XBUTTONUP to ListSubclassProc
old_list_sub = """LRESULT CALLBACK FileListView::ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {"""

new_list_sub = """LRESULT CALLBACK FileListView::ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {
    case WM_XBUTTONUP: {
        WORD btn = HIWORD(wParam);
        if (btn == XBUTTON1) {
            pThis->GoBack();
            return TRUE;
        } else if (btn == XBUTTON2) {
            pThis->GoForward();
            return TRUE;
        }
        break;
    }"""
flv = flv.replace(old_list_sub, new_list_sub)

with open("src/ui/FileListView.cpp", "w", encoding="utf-8") as f:
    f.write(flv)
print("FileListView.cpp updated successfully!")
