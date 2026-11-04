import re

with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Insert DrawToolbarButton before HandleMessage
draw_button_func = '''
void MainWindow::DrawToolbarButton(DRAWITEMSTRUCT* pdis) {
    HDC hdc = pdis->hDC;
    RECT rc = pdis->rcItem;

    bool isHot = s_btnHoverMap[pdis->hwndItem];
    bool isSelected = (pdis->itemState & ODS_SELECTED) != 0;
    bool isDisabled = (pdis->itemState & ODS_DISABLED) != 0;

    // Surface background
    FillRect(hdc, &rc, Theme::Get().BrushSurface());

    // Hover or pressed round-rect
    if (isSelected) {
        HBRUSH hbrPressed = Theme::Get().BrushSelected();
        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldBrush = SelectObject(hdc, hbrPressed);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 6, 6);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    } else if (isHot) {
        HBRUSH hbrHover = Theme::Get().BrushHover();
        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldBrush = SelectObject(hdc, hbrHover);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 6, 6);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    COLORREF iconColor = isDisabled ? Theme::Get().ClrTextSecondary() : Theme::Get().ClrTextPrimary();
    HPEN hPen = CreatePen(PS_SOLID, 2, iconColor);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int s = Theme::Get().Scale(1, m_hwnd);
    if (s < 1) s = 1;

    UINT btnId = pdis->CtlID;
    switch (btnId) {
    case IDC_BTN_BACK: // Sleek arrow pointing left
        MoveToEx(hdc, cx + 5 * s, cy, nullptr);
        LineTo(hdc, cx - 5 * s, cy);
        MoveToEx(hdc, cx - 1 * s, cy - 4 * s, nullptr);
        LineTo(hdc, cx - 5 * s, cy);
        LineTo(hdc, cx - 1 * s, cy + 4 * s);
        break;

    case IDC_BTN_FORWARD: // Sleek arrow pointing right
        MoveToEx(hdc, cx - 5 * s, cy, nullptr);
        LineTo(hdc, cx + 5 * s, cy);
        MoveToEx(hdc, cx + 1 * s, cy - 4 * s, nullptr);
        LineTo(hdc, cx + 5 * s, cy);
        LineTo(hdc, cx + 1 * s, cy + 4 * s);
        break;

    case IDC_BTN_UP: // Sleek arrow pointing up
        MoveToEx(hdc, cx, cy + 5 * s, nullptr);
        LineTo(hdc, cx, cy - 5 * s);
        MoveToEx(hdc, cx - 4 * s, cy - 1 * s, nullptr);
        LineTo(hdc, cx, cy - 5 * s);
        LineTo(hdc, cx + 4 * s, cy - 1 * s);
        break;

    case IDC_BTN_REFRESH: // Crisp circular arrow
        Arc(hdc, cx - 5 * s, cy - 5 * s, cx + 5 * s, cy + 5 * s,
            cx, cy - 5 * s, cx + 5 * s, cy);
        MoveToEx(hdc, cx + 2 * s, cy - 7 * s, nullptr);
        LineTo(hdc, cx + 5 * s, cy - 4 * s);
        LineTo(hdc, cx + 2 * s, cy - 1 * s);
        break;

    case IDC_BTN_NEWTAB: // Crisp '+'
        MoveToEx(hdc, cx - 5 * s, cy, nullptr);
        LineTo(hdc, cx + 6 * s, cy);
        MoveToEx(hdc, cx, cy - 5 * s, nullptr);
        LineTo(hdc, cx, cy + 6 * s);
        break;

    case IDC_BTN_TREE: { // Sidebar layout icon
        RoundRect(hdc, cx - 6 * s, cy - 6 * s, cx + 7 * s, cy + 7 * s, 3, 3);
        HPEN hThin = CreatePen(PS_SOLID, 1, iconColor);
        HGDIOBJ oldThin = SelectObject(hdc, hThin);
        MoveToEx(hdc, cx - 2 * s, cy - 6 * s, nullptr);
        LineTo(hdc, cx - 2 * s, cy + 6 * s);
        SelectObject(hdc, oldThin);
        DeleteObject(hThin);
        break;
    }

    case IDC_BTN_SPLIT: { // 2-pane split icon
        RoundRect(hdc, cx - 6 * s, cy - 6 * s, cx + 7 * s, cy + 7 * s, 3, 3);
        HPEN hThin = CreatePen(PS_SOLID, 1, iconColor);
        HGDIOBJ oldThin = SelectObject(hdc, hThin);
        MoveToEx(hdc, cx, cy - 6 * s, nullptr);
        LineTo(hdc, cx, cy + 6 * s);
        SelectObject(hdc, oldThin);
        DeleteObject(hThin);
        break;
    }

    case IDC_BTN_PREVIEW: { // Preview pane inspector icon
        RoundRect(hdc, cx - 7 * s, cy - 6 * s, cx + 7 * s, cy + 7 * s, 3, 3);
        HPEN hThin = CreatePen(PS_SOLID, 1, iconColor);
        HGDIOBJ oldThin = SelectObject(hdc, hThin);
        MoveToEx(hdc, cx + 2 * s, cy - 6 * s, nullptr);
        LineTo(hdc, cx + 2 * s, cy + 6 * s);
        SelectObject(hdc, oldThin);
        DeleteObject(hThin);
        break;
    }
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}
'''

target_handle = 'LRESULT MainWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {'
code = code.replace(target_handle, draw_button_func + '\n' + target_handle, 1)

# 2. Replace WM_DRAWITEM block
old_drawitem = '''    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlType == ODT_BUTTON) {
            HDC hdc = pdis->hDC;
            RECT rc = pdis->rcItem;

            bool isHot = s_btnHoverMap[pdis->hwndItem];
            bool isSelected = (pdis->itemState & ODS_SELECTED) != 0;
            bool isDisabled = (pdis->itemState & ODS_DISABLED) != 0;

            FillRect(hdc, &rc, Theme::Get().BrushSurface());

            if (isSelected) {
                HBRUSH hbrPressed = Theme::Get().BrushSelected();
                HPEN hPen = Theme::Get().PenBorder();
                HGDIOBJ oldBrush = SelectObject(hdc, hbrPressed);
                HGDIOBJ oldPen = SelectObject(hdc, hPen);
                RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 6, 6);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
            } else if (isHot) {
                HBRUSH hbrHover = Theme::Get().BrushHover();
                HPEN hPen = Theme::Get().PenBorder();
                HGDIOBJ oldBrush = SelectObject(hdc, hbrHover);
                HGDIOBJ oldPen = SelectObject(hdc, hPen);
                RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 6, 6);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
            }

            HFONT hIconFont = CreateFontW(-Theme::Get().Scale(16, m_hwnd), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                          L"Segoe MDL2 Assets");
            HGDIOBJ oldFont = SelectObject(hdc, hIconFont);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isDisabled ? Theme::Get().ClrTextSecondary() : Theme::Get().ClrTextPrimary());

            wchar_t text[64] = { 0 };
            GetWindowTextW(pdis->hwndItem, text, 64);
            DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            DeleteObject(hIconFont);
            return TRUE;
        }
        break;
    }'''

new_drawitem = '''    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlType == ODT_BUTTON) {
            DrawToolbarButton(pdis);
            return TRUE;
        }
        break;
    }'''
code = code.replace(old_drawitem, new_drawitem, 1)

# 3. Update LayoutAll to support searchBarFirst and clamp minListW
old_layout_mid = '''    m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
    curX += breadcrumbW;

    m_rcFilterSplitter.left = curX;
    m_rcFilterSplitter.top = barY;
    m_rcFilterSplitter.right = curX + splitBarToolW;
    m_rcFilterSplitter.bottom = barY + btnH;
    curX += splitBarToolW;

    m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
    curX += filterW + spacing * 2;'''

new_layout_mid = '''    if (!Config::Get().searchBarFirst) {
        m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
        curX += breadcrumbW;

        m_rcFilterSplitter.left = curX;
        m_rcFilterSplitter.top = barY;
        m_rcFilterSplitter.right = curX + splitBarToolW;
        m_rcFilterSplitter.bottom = barY + btnH;
        curX += splitBarToolW;

        m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
        curX += filterW + spacing * 2;
    } else {
        m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
        curX += filterW;

        m_rcFilterSplitter.left = curX;
        m_rcFilterSplitter.top = barY;
        m_rcFilterSplitter.right = curX + splitBarToolW;
        m_rcFilterSplitter.bottom = barY + btnH;
        curX += splitBarToolW;

        m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
        curX += breadcrumbW + spacing * 2;
    }'''
code = code.replace(old_layout_mid, new_layout_mid, 1)

old_layout_preview = '''    if (!m_previewVisible) {
        m_multiPane.UpdateLayout(mainStartX, contentY, mainWidth, contentH);
    } else {
        int listW = mainWidth - m_previewWidth - splitBarW;
        if (listW < Theme::Get().Scale(250, m_hwnd)) {
            listW = Theme::Get().Scale(250, m_hwnd);
            m_previewWidth = mainWidth - listW - splitBarW;
        }

        m_multiPane.UpdateLayout(mainStartX, contentY, listW, contentH);

        m_rcPreviewSplitter.left = mainStartX + listW;
        m_rcPreviewSplitter.top = contentY;
        m_rcPreviewSplitter.right = mainStartX + listW + splitBarW;
        m_rcPreviewSplitter.bottom = contentY + contentH;

        m_preview.UpdateLayout(mainStartX + listW + splitBarW, contentY, m_previewWidth, contentH);
    }'''

new_layout_preview = '''    if (!m_previewVisible) {
        m_multiPane.UpdateLayout(mainStartX, contentY, mainWidth, contentH);
        m_multiPane.LayoutChildren();
    } else {
        int minListW = (m_multiPane.GetPaneCount() > 1) ? Theme::Get().Scale(350, m_hwnd) : Theme::Get().Scale(250, m_hwnd);
        int listW = mainWidth - m_previewWidth - splitBarW;
        if (listW < minListW) {
            listW = minListW;
            m_previewWidth = mainWidth - listW - splitBarW;
            if (m_previewWidth < Theme::Get().Scale(120, m_hwnd)) {
                m_previewWidth = Theme::Get().Scale(120, m_hwnd);
                listW = mainWidth - m_previewWidth - splitBarW;
            }
        }

        m_multiPane.UpdateLayout(mainStartX, contentY, listW, contentH);
        m_multiPane.LayoutChildren();

        m_rcPreviewSplitter.left = mainStartX + listW;
        m_rcPreviewSplitter.top = contentY;
        m_rcPreviewSplitter.right = mainStartX + listW + splitBarW;
        m_rcPreviewSplitter.bottom = contentY + contentH;

        m_preview.UpdateLayout(mainStartX + listW + splitBarW, contentY, m_previewWidth, contentH);
    }'''
code = code.replace(old_layout_preview, new_layout_preview, 1)

# 4. Update ShowToolbarContextMenu
old_tb_ctx = '''void MainWindow::ShowToolbarContextMenu(POINT ptScreen) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (m_linksVisible ? MF_CHECKED : MF_UNCHECKED),
                1001, isZh ? L"显示链接栏 / 收藏夹" : L"Show Favorites / Links Bar");
    AppendMenuW(hMenu, MF_STRING | (m_treeVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TREE_TOGGLE, isZh ? L"显示文件夹目录树 (F8)" : L"Show Directory Tree (F8)");
    AppendMenuW(hMenu, MF_STRING | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_PREVIEW_TOGGLE, isZh ? L"显示快速预览窗格 (Alt+P)" : L"Show Preview Pane (Alt+P)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"选项与设置(&S)..." : L"Options & Settings(&S)...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd > 0) {
        OnCommand((UINT)cmd);
    }
}'''

new_tb_ctx = '''void MainWindow::ShowToolbarContextMenu(POINT ptScreen) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (Config::Get().lockToolbars ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LOCK_TOOLBARS, isZh ? L"锁定工具栏(&L)" : L"Lock Toolbars(&L)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Toolbar arrangement
    HMENU hArrSub = CreatePopupMenu();
    AppendMenuW(hArrSub, MF_STRING | (!Config::Get().searchBarFirst ? MF_CHECKED : MF_UNCHECKED),
                1101, isZh ? L"地址栏在前，搜索栏在后" : L"Address Bar First, Search Bar Second");
    AppendMenuW(hArrSub, MF_STRING | (Config::Get().searchBarFirst ? MF_CHECKED : MF_UNCHECKED),
                1102, isZh ? L"搜索栏在前，地址栏在后" : L"Search Bar First, Address Bar Second");
    AppendMenuW(hArrSub, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hArrSub, MF_STRING, 1103, isZh ? L"重置工具栏默认位置与宽度(&R)" : L"Reset Toolbar Layout(&R)");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hArrSub, isZh ? L"排列方式(&A)" : L"Arrangement(&A)");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (m_linksVisible ? MF_CHECKED : MF_UNCHECKED),
                1001, isZh ? L"显示链接栏 / 收藏夹(&K)" : L"Show Favorites / Links Bar(&K)");
    AppendMenuW(hMenu, MF_STRING | (m_treeVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TREE_TOGGLE, isZh ? L"显示文件夹目录树(&T)\\tF8" : L"Show Directory Tree(&T)\\tF8");
    AppendMenuW(hMenu, MF_STRING | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_PREVIEW_TOGGLE, isZh ? L"显示快速预览窗格(&P)\\tAlt+P" : L"Show Preview Pane(&P)\\tAlt+P");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"选项设置(&O)..." : L"Options(&O)...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 1101) {
        Config::Get().searchBarFirst = false;
        Config::Get().Save();
        LayoutAll();
    } else if (cmd == 1102) {
        Config::Get().searchBarFirst = true;
        Config::Get().Save();
        LayoutAll();
    } else if (cmd == 1103) {
        Config::Get().searchBarFirst = false;
        Config::Get().filterBarWidth = 240;
        Config::Get().Save();
        LayoutAll();
    } else if (cmd > 0) {
        OnCommand((UINT)cmd);
    }
}'''
code = code.replace(old_tb_ctx, new_tb_ctx, 1)

# 5. Add handlers to OnCommand
target_cmd = 'case ID_FILE_NEW_FILE: {'
replacement_cmd = '''case ID_FILE_NEW_MD: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring baseName = isZh ? L"新建 Markdown 文档.md" : L"New Markdown Document.md";
        std::wstring newFile = activeView->GetCurrentPath() + L"\\\\" + baseName;
        int idx = 2;
        while (PathFileExistsW(newFile.c_str())) {
            newFile = activeView->GetCurrentPath() + (isZh ? (L"\\\\新建 Markdown 文档 (" + std::to_wstring(idx++) + L").md") : (L"\\\\New Markdown (" + std::to_wstring(idx++) + L").md"));
        }
        HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hF != INVALID_HANDLE_VALUE) {
            const char* initContent = "# Title\\n\\n";
            DWORD written = 0;
            WriteFile(hF, initContent, (DWORD)strlen(initContent), &written, nullptr);
            CloseHandle(hF);
        }
        activeView->Refresh();
        break;
    }
    case ID_SCROLL_TOP:
        activeView->ScrollToTop();
        break;
    case ID_SCROLL_BOTTOM:
        activeView->ScrollToBottom();
        break;
    case ID_SCROLL_PAGEUP:
        activeView->ScrollPageUp();
        break;
    case ID_SCROLL_PAGEDOWN:
        activeView->ScrollPageDown();
        break;
    ''' + target_cmd
code = code.replace(target_cmd, replacement_cmd, 1)

# 6. Update Help & Tools command handlers
old_help_handlers = '''    case ID_HELP_SHORTCUTS:
        MessageBoxW(m_hwnd, I18n::Get().T(StringId::ShortcutsCheatsheet),
                    I18n::Get().T(StringId::MenuHelpShortcuts), MB_OK | MB_ICONINFORMATION);
        break;
    case ID_HELP_GESTURES:
        MessageBoxW(m_hwnd, I18n::Get().T(StringId::GesturesCheatsheet),
                    I18n::Get().T(StringId::MenuHelpGestures), MB_OK | MB_ICONINFORMATION);
        break;
    case ID_HELP_MDIE_HOMEPAGE:
        ShellExecuteW(nullptr, L"open", L"https://cres.s28.xrea.com/soft/mdie.html", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_HELP_GITHUB:
        ShellExecuteW(nullptr, L"open", L"https://github.com/mianao/NeoMDIE", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_HELP_ABOUT:
        MessageBoxW(m_hwnd, I18n::Get().T(StringId::AboutDialogText),
                    I18n::Get().T(StringId::AppName), MB_OK | MB_ICONINFORMATION);
        break;'''

new_help_handlers = '''    case ID_TOOLS_SHORTCUTS_SETTINGS:
    case ID_HELP_SHORTCUTS:
    case ID_HELP_GESTURES:
        OnCommand(ID_TOOLS_SETTINGS);
        break;
    case ID_HELP_CHECK_UPDATES: {
        bool isZh = I18n::Get().IsChinese();
        MessageBoxW(m_hwnd,
            isZh ? L"当前已是最新版本 NeoMDIE v2.1.0\\n\\n已集成 MDIE 原版核心体验、现代化 Win32 / Shell COM 引擎、Tab 颜色标记与多栏分屏设计。"
                 : L"You are using the latest version of NeoMDIE v2.1.0\\n\\nAll features and modern shell extensions are up to date.",
            isZh ? L"检查更新" : L"Check for Updates",
            MB_OK | MB_ICONINFORMATION);
        break;
    }
    case ID_HELP_GITHUB:
        ShellExecuteW(nullptr, L"open", L"https://github.com/mianao/NeoMDIE", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_HELP_ABOUT: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring aboutText = isZh ?
            L"NeoMDIE - 现代化纯原生 Win32 多标签/多分屏文件管理器\\n"
            L"版本: v2.1.0 (x64 Release)\\n\\n"
            L"作者: Harry (mianao)\\n"
            L"开源项目仓库: https://github.com/mianao/NeoMDIE\\n"
            L"致敬经典: 抹茶 MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"技术特色:\\n"
            L"• 纯 C++17 / Native Win32 API / Windows Shell COM 引擎\\n"
            L"• 静态编译 (/MT)，零额外运行时依赖，轻量疾速 (<1MB)\\n"
            L"• 支持多栏分屏、鼠标手势、多标签彩色标记与即时搜索过滤\\n\\n"
            L"基于 MIT 开源协议发布。" :
            L"NeoMDIE - Modern Native Win32 Tabbed & Multi-Pane File Manager\\n"
            L"Version: v2.1.0 (x64 Release)\\n\\n"
            L"Author: Harry (mianao)\\n"
            L"Repository: https://github.com/mianao/NeoMDIE\\n"
            L"Inspired by: MDIE by Matcha\\n\\n"
            L"Pure C++17 / Native Win32 / Shell COM. Zero dependencies.\\n"
            L"Licensed under the MIT License.";
        MessageBoxW(m_hwnd, aboutText.c_str(), isZh ? L"关于 NeoMDIE" : L"About NeoMDIE", MB_OK | MB_ICONINFORMATION);
        break;
    }'''
code = code.replace(old_help_handlers, new_help_handlers, 1)

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("MainWindow part 2 patched successfully.")
