with open('src/ui/FilterBar.cpp', 'r', encoding='utf-8') as f:
    fb = f.read()

# 1. WS_CLIPCHILDREN
fb = fb.replace('WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,\r\n        0, 0, 220, 30,',
                'WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,\r\n        0, 0, 220, 30,')

# 2. Invalidate on UpdateLayout
old_layout = '''        if (m_hwndOptBtn) {
            MoveWindow(m_hwndOptBtn, width - optW - Theme::Get().Scale(4, m_hwnd),
                       Theme::Get().Scale(3, m_hwnd), optW, height - Theme::Get().Scale(6, m_hwnd), TRUE);
        }
    }'''
new_layout = '''        if (m_hwndOptBtn) {
            MoveWindow(m_hwndOptBtn, width - optW - Theme::Get().Scale(4, m_hwnd),
                       Theme::Get().Scale(3, m_hwnd), optW, height - Theme::Get().Scale(6, m_hwnd), TRUE);
        }
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }'''
fb = fb.replace(old_layout, new_layout)

# 3. WM_ERASEBKGND
old_erase = '''    case WM_ERASEBKGND:
        return 1;'''
new_erase = '''    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushSurface());
        return 1;
    }'''
fb = fb.replace(old_erase, new_erase)

with open('src/ui/FilterBar.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(fb)
print('FilterBar.cpp updated')

# Update BreadcrumbBar.cpp
with open('src/ui/BreadcrumbBar.cpp', 'r', encoding='utf-8') as f:
    bb = f.read()

bb = bb.replace('WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,\r\n        0, 0, 300, 32,',
                'WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,\r\n        0, 0, 300, 32,')

old_bb_erase = '''    case WM_ERASEBKGND:
        return 1;'''
new_bb_erase = '''    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushSurface());
        return 1;
    }'''
bb = bb.replace(old_bb_erase, new_bb_erase)

with open('src/ui/BreadcrumbBar.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(bb)
print('BreadcrumbBar.cpp updated')
