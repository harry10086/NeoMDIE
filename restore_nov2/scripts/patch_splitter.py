with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    s = f.read()

# 1. Update resizingFilterBar logic
old_resize = """        if (m_resizingFilterBar && (wParam & MK_LBUTTON)) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int totalW = rcClient.right - rcClient.left;
            int btnW = Theme::Get().Scale(36, hwnd);
            int spacing = Theme::Get().Scale(3, hwnd);
            int rightBtnsW = btnW * 2 + spacing * 3;
            int newFilterW = totalW - pt.x - rightBtnsW - Theme::Get().Scale(16, hwnd);
            int unscaled = (int)(newFilterW / Theme::Get().GetScaleFactor(hwnd));
            if (unscaled < 100) unscaled = 100;
            if (unscaled > 600) unscaled = 600;
            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            return 0;
        }"""

new_resize = """        if (m_resizingFilterBar && (wParam & MK_LBUTTON)) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int totalW = rcClient.right - rcClient.left;
            int btnW = Theme::Get().Scale(36, hwnd);
            int spacing = Theme::Get().Scale(3, hwnd);
            int rightBtnsW = btnW * 2 + spacing * 3;
            int filterStartX = Theme::Get().Scale(6, hwnd) + (btnW + spacing) * 5 + btnW + spacing * 2;

            int newFilterW = 0;
            if (Config::Get().searchBarFirst) {
                newFilterW = pt.x - filterStartX;
            } else {
                newFilterW = (totalW - rightBtnsW - Theme::Get().Scale(16, hwnd)) - pt.x;
            }
            int unscaled = (int)(newFilterW / Theme::Get().GetScaleFactor(hwnd));
            if (unscaled < 80) unscaled = 80;
            if (unscaled > 700) unscaled = 700;
            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            return 0;
        }"""

if old_resize in s:
    s = s.replace(old_resize, new_resize, 1)
    print("Resizing filter bar updated.")
else:
    print("Error: old_resize not found!")

# 2. Update WM_PAINT to clear toolbar background before drawing splitter
old_paint = """    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hbr = Theme::Get().BrushBorder();
        if (m_treeVisible) {
            FillRect(hdc, &m_rcTreeSplitter, hbr);
        }
        if (m_previewVisible) {
            FillRect(hdc, &m_rcPreviewSplitter, hbr);
        }
        if (!Config::Get().lockToolbars) {
            FillRect(hdc, &m_rcFilterSplitter, hbr);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }"""

new_paint = """    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int tabH = Theme::Get().Scale(36, hwnd);
        int toolbarH = Theme::Get().Scale(38, hwnd);
        RECT rcToolbar = { 0, tabH, rcClient.right, tabH + toolbarH };
        FillRect(hdc, &rcToolbar, Theme::Get().BrushSurface());

        HBRUSH hbr = Theme::Get().BrushBorder();
        if (m_treeVisible) {
            FillRect(hdc, &m_rcTreeSplitter, hbr);
        }
        if (m_previewVisible) {
            FillRect(hdc, &m_rcPreviewSplitter, hbr);
        }
        if (!Config::Get().lockToolbars) {
            FillRect(hdc, &m_rcFilterSplitter, hbr);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }"""

if old_paint in s:
    s = s.replace(old_paint, new_paint, 1)
    print("WM_PAINT updated.")
else:
    print("Error: old_paint not found!")

# 3. In LayoutAll, invalidate toolbar area with TRUE
old_layout_end = """    InvalidateRect(m_hwnd, nullptr, FALSE);
}"""

new_layout_end = """    RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
    InvalidateRect(m_hwnd, &rcToolbar, TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}"""

if old_layout_end in s:
    s = s.replace(old_layout_end, new_layout_end, 1)
    print("LayoutAll invalidate updated.")
else:
    print("Error: old_layout_end not found!")

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(s)
print("MainWindow.cpp splitter updates finished!")
