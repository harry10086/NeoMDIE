import re

# 1. Update MainWindow.h
with open('src/ui/MainWindow.h', 'r', encoding='utf-8') as f:
    hdr = f.read()

hdr_target = 'bool m_hasTrayIcon = false;'
hdr_repl = '''bool m_hasTrayIcon = false;
    bool m_wasMaximizedBeforeTray = false;'''
hdr = hdr.replace(hdr_target, hdr_repl)

with open('src/ui/MainWindow.h', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(hdr)
print('MainWindow.h updated')

# 2. Update MainWindow.cpp
with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

# Replace scripts dir ShellExecute with m_tabBar.AddTab
old_scripts = '''    case ID_TOOLS_OPEN_SCRIPTS_DIR:
        ShellExecuteW(nullptr, L"open", ScriptEngine::Get().GetScriptsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;'''
new_scripts = '''    case ID_TOOLS_OPEN_SCRIPTS_DIR: {
        std::wstring sDir = ScriptEngine::Get().GetScriptsDir();
        CreateDirectoryW(sDir.c_str(), nullptr);
        m_tabBar.AddTab(sDir, true);
        break;
    }'''
assert old_scripts in src
src = src.replace(old_scripts, new_scripts, 1)

# Replace ID_HELP_CHECK_UPDATES MessageBox with ShellUtils::CheckForUpdates
old_updates_start = src.find('case ID_HELP_CHECK_UPDATES:')
old_updates_end = src.find('case ID_HELP_GITHUB:', old_updates_start)
assert old_updates_start != -1 and old_updates_end != -1
new_updates = '''case ID_HELP_CHECK_UPDATES:
        ShellUtils::CheckForUpdates(m_hwnd);
        break;
    '''
src = src[:old_updates_start] + new_updates + src[old_updates_end:]

# Update SettingsDialog::Show callback in ID_TOOLS_SETTINGS
old_dlg_cb = '''            for (int i = 0; i < 4; ++i) {
                m_multiPane.GetPane(i).SetCalcFolderSize(cfg.calcFolderSize);
            }

            UpdateLanguageUI();'''
new_dlg_cb = '''            for (int i = 0; i < 4; ++i) {
                m_multiPane.GetPane(i).SetCalcFolderSize(cfg.calcFolderSize);
            }

            m_multiPane.ApplyTheme();
            ApplyTheme();
            DrawMenuBar(m_hwnd);
            UpdateLanguageUI();'''
assert old_dlg_cb in src
src = src.replace(old_dlg_cb, new_dlg_cb, 1)

# Update UAH menu drawing: rcBar.bottom += 1 and GetMenu(hwnd)
old_uah_1 = '''                RECT rcBar = mbi.rcBar;
                OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
                FillRect(pUah->hdc, &rcBar, Theme::Get().BrushSurface());'''
new_uah_1 = '''                RECT rcBar = mbi.rcBar;
                OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
                rcBar.bottom += 1;
                FillRect(pUah->hdc, &rcBar, Theme::Get().BrushSurface());'''
assert old_uah_1 in src
src = src.replace(old_uah_1, new_uah_1, 1)

old_uah_2 = '''            wchar_t text[128] = { 0 };
            GetMenuStringW(pItem->uahMenu.hmenu, pItem->iPosition, text, 128, MF_BYPOSITION);'''
new_uah_2 = '''            HMENU hMenu = GetMenu(hwnd);
            wchar_t text[128] = { 0 };
            GetMenuStringW(hMenu, pItem->iPosition, text, 128, MF_BYPOSITION);'''
assert old_uah_2 in src
src = src.replace(old_uah_2, new_uah_2, 1)

# Update WM_PAINT to draw m_rcFilterSplitter
old_paint_split = '''        if (!Config::Get().lockToolbars) {
        }'''
new_paint_split = '''        if (!Config::Get().lockToolbars) {
            FillRect(hdc, &m_rcFilterSplitter, hbr);
        }'''
assert old_paint_split in src
src = src.replace(old_paint_split, new_paint_split, 1)

# Add WM_ERASEBKGND to MainWindow::HandleMessage
erase_needle = '    case WM_PAINT: {'
erase_code = '''    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int tabH = Theme::Get().Scale(36, hwnd);
        int toolbarH = Theme::Get().Scale(38, hwnd);
        RECT rcToolbar = { 0, tabH, rcClient.right, tabH + toolbarH };
        FillRect(hdc, &rcToolbar, Theme::Get().BrushSurface());
        return 1;
    }
    case WM_PAINT: {'''
assert erase_needle in src
src = src.replace(erase_needle, erase_code, 1)

# Add WM_COPYDATA
copydata_needle = '    case WM_SIZE:'
copydata_code = '''    case WM_COPYDATA: {
        COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
        if (pcds && pcds->lpData) {
            std::wstring path = (const wchar_t*)pcds->lpData;
            if (!path.empty() && PathFileExistsW(path.c_str())) {
                m_tabBar.AddTab(path, true);
            }
        }
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, m_wasMaximizedBeforeTray ? SW_MAXIMIZE : SW_RESTORE);
        }
        SetForegroundWindow(hwnd);
        return TRUE;
    }
    case WM_SIZE:'''
assert copydata_needle in src
src = src.replace(copydata_needle, copydata_code, 1)

# Update WM_MOUSEMOVE m_resizingFilterBar
old_resize_fb = '''        if (m_resizingFilterBar && (wParam & MK_LBUTTON)) {
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
            int tabH = Theme::Get().Scale(36, hwnd);
            int toolbarH = Theme::Get().Scale(38, hwnd);
            RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
            RedrawWindow(hwnd, &rcToolbar, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            return 0;
        }'''

new_resize_fb = '''        if (m_resizingFilterBar && (wParam & MK_LBUTTON)) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int totalW = rcClient.right - rcClient.left;
            int btnW = Theme::Get().Scale(36, hwnd);
            int spacing = Theme::Get().Scale(3, hwnd);
            int rightBtnsW = btnW * 2 + spacing * 3;
            int curX_start = Theme::Get().Scale(6, hwnd) + (btnW + spacing) * 4 + (btnW + spacing * 2) * 2;
            int filterEndX = totalW - rightBtnsW - Theme::Get().Scale(16, hwnd);

            int newFilterW = 0;
            if (Config::Get().searchBarFirst) {
                newFilterW = pt.x - curX_start;
            } else {
                newFilterW = filterEndX - pt.x;
            }
            int unscaled = (int)(newFilterW / Theme::Get().GetScaleFactor(hwnd));
            if (unscaled < 80) unscaled = 80;
            if (unscaled > 700) unscaled = 700;
            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            int tabH = Theme::Get().Scale(36, hwnd);
            int toolbarH = Theme::Get().Scale(38, hwnd);
            RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
            InvalidateRect(hwnd, &rcToolbar, TRUE);
            InvalidateRect(m_breadcrumb.GetHwnd(), nullptr, TRUE);
            InvalidateRect(m_filterBar.GetHwnd(), nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;
        }'''
assert old_resize_fb in src
src = src.replace(old_resize_fb, new_resize_fb, 1)

# Update tray minimize / close and restore
old_syscommand = '''    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE && Config::Get().minimizeToTray) {
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;'''
new_syscommand = '''    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE && Config::Get().minimizeToTray) {
            m_wasMaximizedBeforeTray = (IsZoomed(hwnd) != FALSE);
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;'''
assert old_syscommand in src
src = src.replace(old_syscommand, new_syscommand, 1)

old_close_tray = '''    case WM_CLOSE: {
        if (Config::Get().closeToTray) {
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }'''
new_close_tray = '''    case WM_CLOSE: {
        if (Config::Get().closeToTray) {
            m_wasMaximizedBeforeTray = (IsZoomed(hwnd) != FALSE);
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }'''
assert old_close_tray in src
src = src.replace(old_close_tray, new_close_tray, 1)

old_restore_tray = '''        if (uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon();'''
new_restore_tray = '''        if (uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, m_wasMaximizedBeforeTray ? SW_MAXIMIZE : SW_RESTORE);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon();'''
assert old_restore_tray in src
src = src.replace(old_restore_tray, new_restore_tray, 1)

old_cmd_restore = '''            if (cmd == 5001) {
                ShowWindow(hwnd, SW_RESTORE);'''
new_cmd_restore = '''            if (cmd == 5001) {
                ShowWindow(hwnd, m_wasMaximizedBeforeTray ? SW_MAXIMIZE : SW_RESTORE);'''
assert old_cmd_restore in src
src = src.replace(old_cmd_restore, new_cmd_restore, 1)

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(src)
print('MainWindow.cpp updated')
