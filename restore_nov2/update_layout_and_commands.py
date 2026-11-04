with open(r'd:\github\NeoMDIE\src\ui\MainWindow.cpp', 'r', encoding='utf-8') as f:
    s = f.read()

# 1. Update LayoutAll
target_layout = '''    int rightBtnsW = btnW * 2 + spacing * 3;
    int filterW = Theme::Get().Scale(220, m_hwnd);
    int breadcrumbW = totalW - curX - filterW - rightBtnsW - Theme::Get().Scale(16, m_hwnd);
    if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) {
        breadcrumbW = Theme::Get().Scale(120, m_hwnd);
    }

    m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
    curX += breadcrumbW + spacing * 2;

    m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
    curX += filterW + spacing * 2;'''

replace_layout = '''    int rightBtnsW = btnW * 2 + spacing * 3;
    int filterW = Theme::Get().Scale(Config::Get().filterBarWidth, m_hwnd);
    int splitBarToolW = Theme::Get().Scale(5, m_hwnd);
    int breadcrumbW = totalW - curX - filterW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
    if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) {
        breadcrumbW = Theme::Get().Scale(120, m_hwnd);
        filterW = totalW - curX - breadcrumbW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
        if (filterW < Theme::Get().Scale(100, m_hwnd)) filterW = Theme::Get().Scale(100, m_hwnd);
    }

    m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
    curX += breadcrumbW;

    m_rcFilterSplitter.left = curX;
    m_rcFilterSplitter.top = barY;
    m_rcFilterSplitter.right = curX + splitBarToolW;
    m_rcFilterSplitter.bottom = barY + btnH;
    curX += splitBarToolW;

    m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
    curX += filterW + spacing * 2;'''

assert target_layout in s, "target_layout not found"
s = s.replace(target_layout, replace_layout, 1)

# 2. Update HandleMessage (WM_PAINT, WM_SETCURSOR, WM_LBUTTONDOWN, WM_MOUSEMOVE, WM_LBUTTONUP)
target_paint = '''        if (m_previewVisible) {
            FillRect(hdc, &m_rcPreviewSplitter, hbr);
        }
        EndPaint(hwnd, &ps);'''

replace_paint = '''        if (m_previewVisible) {
            FillRect(hdc, &m_rcPreviewSplitter, hbr);
        }
        if (!Config::Get().lockToolbars) {
            FillRect(hdc, &m_rcFilterSplitter, hbr);
        }
        EndPaint(hwnd, &ps);'''

assert target_paint in s, "target_paint not found"
s = s.replace(target_paint, replace_paint, 1)

target_cursor = '''        if ((m_previewVisible && PtInRect(&m_rcPreviewSplitter, pt)) || m_resizingPreview) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;'''

replace_cursor = '''        if ((m_previewVisible && PtInRect(&m_rcPreviewSplitter, pt)) || m_resizingPreview) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        if (!Config::Get().lockToolbars && (PtInRect(&m_rcFilterSplitter, pt) || m_resizingFilterBar)) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;'''

assert target_cursor in s, "target_cursor not found"
s = s.replace(target_cursor, replace_cursor, 1)

target_lbdown = '''        if (m_previewVisible && PtInRect(&m_rcPreviewSplitter, pt)) {
            m_resizingPreview = true;
            SetCapture(hwnd);
            return 0;
        }
        break;'''

replace_lbdown = '''        if (m_previewVisible && PtInRect(&m_rcPreviewSplitter, pt)) {
            m_resizingPreview = true;
            SetCapture(hwnd);
            return 0;
        }
        if (!Config::Get().lockToolbars && PtInRect(&m_rcFilterSplitter, pt)) {
            m_resizingFilterBar = true;
            SetCapture(hwnd);
            return 0;
        }
        break;'''

assert target_lbdown in s, "target_lbdown not found"
s = s.replace(target_lbdown, replace_lbdown, 1)

target_mousemove = '''        if (m_resizingPreview && (wParam & MK_LBUTTON)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            m_previewWidth = rc.right - pt.x;
            LayoutAll();
            return 0;
        }
        break;'''

replace_mousemove = '''        if (m_resizingPreview && (wParam & MK_LBUTTON)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            m_previewWidth = rc.right - pt.x;
            LayoutAll();
            return 0;
        }
        if (m_resizingFilterBar && (wParam & MK_LBUTTON)) {
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
        }
        break;'''

assert target_mousemove in s, "target_mousemove not found"
s = s.replace(target_mousemove, replace_mousemove, 1)

target_lbup = '''        if (m_previewVisible) {
            m_previewVisible = false;
            ReleaseCapture();
            return 0;
        }'''

# Note: in previous code it had m_resizingPreview
target_lbup2 = '''        if (m_resizingPreview) {
            m_resizingPreview = false;
            ReleaseCapture();
            return 0;
        }'''

assert target_lbup2 in s, "target_lbup2 not found"
replace_lbup = '''        if (m_resizingPreview) {
            m_resizingPreview = false;
            ReleaseCapture();
            return 0;
        }
        if (m_resizingFilterBar) {
            m_resizingFilterBar = false;
            ReleaseCapture();
            Config::Get().Save();
            return 0;
        }'''
s = s.replace(target_lbup2, replace_lbup, 1)

with open(r'd:\github\NeoMDIE\src\ui\MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(s)
print('Updated LayoutAll and HandleMessage successfully')
