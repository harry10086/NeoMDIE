with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

# 1. Update LayoutAll
old_layout_toolbar = r"""    // 2. Toolbar
    int btnW = Theme::Get().Scale(36, m_hwnd);
    int btnH = Theme::Get().Scale(32, m_hwnd);
    int spacing = Theme::Get().Scale(3, m_hwnd);
    int barY = tabH + (toolbarH - btnH) / 2;

    int curX = Theme::Get().Scale(6, m_hwnd);
    MoveWindow(m_hwndBtnBack, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing;
    MoveWindow(m_hwndBtnForward, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing;
    MoveWindow(m_hwndBtnUp, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing;
    MoveWindow(m_hwndBtnRefresh, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing;
    MoveWindow(m_hwndBtnNewTab, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing * 2;

    MoveWindow(m_hwndBtnTree, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing * 2;

    int rightBtnsW = btnW * 2 + spacing * 3;
    int filterW = Theme::Get().Scale(Config::Get().filterBarWidth, m_hwnd);
    int splitBarToolW = Theme::Get().Scale(5, m_hwnd);
    int breadcrumbW = totalW - curX - filterW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
    if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) {
        breadcrumbW = Theme::Get().Scale(120, m_hwnd);
        filterW = totalW - curX - breadcrumbW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
        if (filterW < Theme::Get().Scale(100, m_hwnd)) filterW = Theme::Get().Scale(100, m_hwnd);
    }

    if (!Config::Get().searchBarFirst) {
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
    }

    MoveWindow(m_hwndBtnSplit, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing;

    MoveWindow(m_hwndBtnPreview, curX, barY, btnW, btnH, TRUE);"""

new_layout_toolbar = r"""    Config& cfg = Config::Get();
    m_linksVisible = cfg.showToolbarLinks;

    bool showTB = cfg.showToolbarStandard || cfg.showToolbarAddress || cfg.showToolbarSearch;
    toolbarH = showTB ? Theme::Get().Scale(38, m_hwnd) : 0;
    linksH = m_linksVisible ? Theme::Get().Scale(28, m_hwnd) : 0;
    statusH = cfg.showStatusBar ? Theme::Get().Scale(24, m_hwnd) : 0;

    ShowWindow(m_hwndStatusBar, cfg.showStatusBar ? SW_SHOW : SW_HIDE);

    // 2. Toolbar
    int btnW = Theme::Get().Scale(36, m_hwnd);
    int btnH = Theme::Get().Scale(32, m_hwnd);
    int spacing = Theme::Get().Scale(3, m_hwnd);
    int barY = tabH + (toolbarH - btnH) / 2;

    int curX = Theme::Get().Scale(6, m_hwnd);

    int swStd = (cfg.showToolbarStandard && showTB) ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hwndBtnBack, swStd);
    ShowWindow(m_hwndBtnForward, swStd);
    ShowWindow(m_hwndBtnUp, swStd);
    ShowWindow(m_hwndBtnRefresh, swStd);
    ShowWindow(m_hwndBtnNewTab, swStd);
    ShowWindow(m_hwndBtnTree, swStd);

    if (cfg.showToolbarStandard && showTB) {
        MoveWindow(m_hwndBtnBack, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnForward, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnUp, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnRefresh, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnNewTab, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing * 2;

        MoveWindow(m_hwndBtnTree, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing * 2;
    }

    int rightBtnsW = btnW * 2 + spacing * 3;
    int filterW = Theme::Get().Scale(cfg.filterBarWidth, m_hwnd);
    int splitBarToolW = (cfg.showToolbarAddress && cfg.showToolbarSearch && !cfg.lockToolbars && showTB) ? Theme::Get().Scale(5, m_hwnd) : 0;
    int breadcrumbW = totalW - curX - filterW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
    if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) {
        breadcrumbW = Theme::Get().Scale(120, m_hwnd);
        filterW = totalW - curX - breadcrumbW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
        if (filterW < Theme::Get().Scale(100, m_hwnd)) filterW = Theme::Get().Scale(100, m_hwnd);
    }

    if (!cfg.showToolbarAddress) {
        ShowWindow(m_breadcrumb.GetHwnd(), SW_HIDE);
        filterW = totalW - curX - rightBtnsW - Theme::Get().Scale(16, m_hwnd);
        if (filterW < Theme::Get().Scale(100, m_hwnd)) filterW = Theme::Get().Scale(100, m_hwnd);
    } else {
        ShowWindow(m_breadcrumb.GetHwnd(), showTB ? SW_SHOW : SW_HIDE);
    }

    if (!cfg.showToolbarSearch) {
        ShowWindow(m_filterBar.GetHwnd(), SW_HIDE);
        breadcrumbW = totalW - curX - rightBtnsW - Theme::Get().Scale(16, m_hwnd);
        if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) breadcrumbW = Theme::Get().Scale(120, m_hwnd);
    } else {
        ShowWindow(m_filterBar.GetHwnd(), showTB ? SW_SHOW : SW_HIDE);
    }

    if (!cfg.searchBarFirst) {
        if (cfg.showToolbarAddress && showTB) {
            m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
            curX += breadcrumbW;
        }

        if (cfg.showToolbarAddress && cfg.showToolbarSearch && showTB) {
            m_rcFilterSplitter.left = curX;
            m_rcFilterSplitter.top = barY;
            m_rcFilterSplitter.right = curX + splitBarToolW;
            m_rcFilterSplitter.bottom = barY + btnH;
            curX += splitBarToolW;
        } else {
            SetRectEmpty(&m_rcFilterSplitter);
        }

        if (cfg.showToolbarSearch && showTB) {
            m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
            curX += filterW + spacing * 2;
        }
    } else {
        if (cfg.showToolbarSearch && showTB) {
            m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
            curX += filterW;
        }

        if (cfg.showToolbarAddress && cfg.showToolbarSearch && showTB) {
            m_rcFilterSplitter.left = curX;
            m_rcFilterSplitter.top = barY;
            m_rcFilterSplitter.right = curX + splitBarToolW;
            m_rcFilterSplitter.bottom = barY + btnH;
            curX += splitBarToolW;
        } else {
            SetRectEmpty(&m_rcFilterSplitter);
        }

        if (cfg.showToolbarAddress && showTB) {
            m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
            curX += breadcrumbW + spacing * 2;
        }
    }

    int swRight = showTB ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hwndBtnSplit, swRight);
    ShowWindow(m_hwndBtnPreview, swRight);
    if (showTB) {
        MoveWindow(m_hwndBtnSplit, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnPreview, curX, barY, btnW, btnH, TRUE);
    }"""

assert old_layout_toolbar in text, "old_layout_toolbar not found"
text = text.replace(old_layout_toolbar, new_layout_toolbar)

# 2. Update BuildMenu scripts enumeration
old_script_sub = r"""    HMENU hScriptSub = CreatePopupMenu();
    AppendMenuW(hScriptSub, MF_STRING, ID_TOOLS_RELOAD_SCRIPTS, isZh ? L"重新加载所有脚本(&R)" : L"Reload All Scripts(&R)");
    AppendMenuW(hScriptSub, MF_STRING, ID_TOOLS_SCRIPT_CONSOLE, isZh ? L"打开脚本控制台(&C)" : L"Script Console(&C)");
    AppendMenuW(hScriptSub, MF_STRING, ID_TOOLS_OPEN_SCRIPTS_DIR, isZh ? L"打开脚本目录(&D)" : L"Open Scripts Directory(&D)");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hScriptSub, isZh ? L"脚本扩展(&E)" : L"Scripts(&E)");"""

new_script_sub = r"""    HMENU hScriptSub = CreatePopupMenu();
    AppendMenuW(hScriptSub, MF_STRING, ID_TOOLS_RELOAD_SCRIPTS, isZh ? L"重新加载所有脚本(&R)" : L"Reload All Scripts(&R)");
    AppendMenuW(hScriptSub, MF_STRING, ID_TOOLS_SCRIPT_CONSOLE, isZh ? L"打开脚本控制台(&C)" : L"Script Console(&C)");
    AppendMenuW(hScriptSub, MF_STRING, ID_TOOLS_OPEN_SCRIPTS_DIR, isZh ? L"打开脚本目录(&D)" : L"Open Scripts Directory(&D)");

    const auto& scripts = ScriptEngine::Get().GetScripts();
    if (!scripts.empty()) {
        AppendMenuW(hScriptSub, MF_SEPARATOR, 0, nullptr);
        for (const auto& s : scripts) {
            std::wstring itemText = s.name;
            if (!s.hotkey.empty()) itemText += L"\t" + s.hotkey;
            AppendMenuW(hScriptSub, MF_STRING, s.commandId, itemText.c_str());
        }
    }
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hScriptSub, isZh ? L"脚本扩展(&E)" : L"Scripts(&E)");"""

assert old_script_sub in text, "old_script_sub not found"
text = text.replace(old_script_sub, new_script_sub)

# 3. Update Gesture actions in onGestureAction
old_gest_act = r"""        case GestureAction::ReopenClosedTab: OnCommand(ID_FILE_REOPEN_CLOSED_TAB); break;
        case GestureAction::NextTab: m_tabBar.NextTab(); break;
        case GestureAction::PrevTab: m_tabBar.PrevTab(); break;
        case GestureAction::CloseOtherTabs: OnCommand(ID_FILE_CLOSE_OTHER_TABS); break;
        default: break;"""

new_gest_act = r"""        case GestureAction::ReopenClosedTab: OnCommand(ID_FILE_REOPEN_CLOSED_TAB); break;
        case GestureAction::NextTab: m_tabBar.NextTab(); break;
        case GestureAction::PrevTab: m_tabBar.PrevTab(); break;
        case GestureAction::CloseOtherTabs: OnCommand(ID_FILE_CLOSE_OTHER_TABS); break;
        case GestureAction::LayoutSingle: m_multiPane.SetLayout(PaneLayout::Single); break;
        case GestureAction::Layout2H:     m_multiPane.SetLayout(PaneLayout::DualHorizontal); break;
        case GestureAction::Layout2V:     m_multiPane.SetLayout(PaneLayout::DualVertical); break;
        case GestureAction::Layout4Grid:  m_multiPane.SetLayout(PaneLayout::Quad); break;
        case GestureAction::CopyPath:     if (activeView) ShellUtils::CopyToClipboard(activeView->GetCurrentPath()); break;
        case GestureAction::Delete:       OnCommand(ID_EDIT_DELETE); break;
        case GestureAction::Rename:       OnCommand(ID_EDIT_RENAME); break;
        case GestureAction::NewMD:        OnCommand(ID_FILE_NEW_MD); break;
        case GestureAction::ToggleHidden: OnCommand(ID_VIEW_SHOW_HIDDEN); break;
        case GestureAction::OpenSettings: OnCommand(ID_TOOLS_SETTINGS); break;
        case GestureAction::InvertSelection: OnCommand(ID_EDIT_INVERT_SEL); break;
        default: break;"""

assert old_gest_act in text, "old_gest_act not found"
text = text.replace(old_gest_act, new_gest_act)

# 4. Handle ID_VIEW_SHOW_HIDDEN, ID_VIEW_CALC_FOLDER_SIZE, and ID_SCRIPT_COMMAND in OnCommand
old_cmd_dispatch = r"""    case ID_VIEW_STATUSBAR:
        Config::Get().showStatusBar = !Config::Get().showStatusBar;
        ShowWindow(m_hwndStatusBar, Config::Get().showStatusBar ? SW_SHOW : SW_HIDE);
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;"""

new_cmd_dispatch = r"""    case ID_VIEW_STATUSBAR:
        Config::Get().showStatusBar = !Config::Get().showStatusBar;
        ShowWindow(m_hwndStatusBar, Config::Get().showStatusBar ? SW_SHOW : SW_HIDE);
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_SHOW_HIDDEN: {
        Config::Get().showHidden = !Config::Get().showHidden;
        Config::Get().Save();
        m_multiPane.SetShowHidden(Config::Get().showHidden);
        BuildMenu();
        break;
    }
    case ID_VIEW_CALC_FOLDER_SIZE: {
        Config::Get().calcFolderSize = !Config::Get().calcFolderSize;
        Config::Get().Save();
        m_multiPane.SetCalcFolderSize(Config::Get().calcFolderSize);
        BuildMenu();
        break;
    }"""

assert old_cmd_dispatch in text, "old_cmd_dispatch not found"
text = text.replace(old_cmd_dispatch, new_cmd_dispatch)

# 5. Handle script commands in OnCommand default
old_cmd_default = r"""    case ID_TOOLS_SCRIPT_CONSOLE:
        ScriptEngine::Get().ShowConsole();
        break;"""

new_cmd_default = r"""    case ID_TOOLS_SCRIPT_CONSOLE:
        ScriptEngine::Get().ShowConsole();
        break;
    default:
        if (wmId >= ID_SCRIPT_COMMAND_START && wmId <= ID_SCRIPT_COMMAND_END) {
            FileListView* pView = m_multiPane.GetActivePane();
            std::wstring curPath = pView ? pView->GetCurrentPath() : L"";
            std::vector<std::wstring> sel = pView ? pView->GetSelectedPaths() : std::vector<std::wstring>();
            ScriptEngine::Get().ExecuteCommand(wmId, curPath, sel);
            break;
        }
        break;"""

assert old_cmd_default in text, "old_cmd_default not found"
text = text.replace(old_cmd_default, new_cmd_default)

# 6. SettingsDialog callback update
old_settings_cb = r"""            for (int i = 0; i < 4; ++i) {
                m_multiPane.GetPane(i).SetCalcFolderSize(cfg.calcFolderSize);
            }

            m_multiPane.ApplyTheme();
            ApplyTheme();
            DrawMenuBar(m_hwnd);
            UpdateLanguageUI();"""

new_settings_cb = r"""            m_linksVisible = cfg.showToolbarLinks;
            m_multiPane.SetShowHidden(cfg.showHidden);
            m_multiPane.SetCalcFolderSize(cfg.calcFolderSize);
            ShowWindow(m_hwndStatusBar, cfg.showStatusBar ? SW_SHOW : SW_HIDE);

            m_multiPane.ApplyTheme();
            ApplyTheme();
            LayoutAll();
            BuildMenu();
            DrawMenuBar(m_hwnd);
            UpdateLanguageUI();"""

assert old_settings_cb in text, "old_settings_cb not found"
text = text.replace(old_settings_cb, new_settings_cb)

# 7. Dark mode menu bar drawing (0x0091 and 0x0092)
old_uah = r"""    case 0x0091: { // WM_UAHDRAWMENU (Dark mode menu bar background)
        if (Theme::Get().IsDarkMode()) {
            struct UAHMENU { HMENU hmenu; HDC hdc; DWORD dwFlags; };
            UAHMENU* pUah = (UAHMENU*)lParam;
            MENUBARINFO mbi = { sizeof(mbi) };
            if (GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) {
                RECT rcWindow;
                GetWindowRect(hwnd, &rcWindow);
                RECT rcBar = mbi.rcBar;
                OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
                rcBar.bottom += 1;
                FillRect(pUah->hdc, &rcBar, Theme::Get().BrushSurface());
                return TRUE;
            }
        }
        break;
    }
    case 0x0092: { // WM_UAHDRAWMENUITEM (Dark mode menu bar items)
        if (Theme::Get().IsDarkMode()) {
            struct UAHMENU { HMENU hmenu; HDC hdc; DWORD dwFlags; };
            struct UAHMENUITEM { int iPosition; UAHMENU uahMenu; DRAWITEMSTRUCT dis; };
            UAHMENUITEM* pItem = (UAHMENUITEM*)lParam;
            HDC hdc = pItem->dis.hDC;
            RECT rc = pItem->dis.rcItem;
            bool isHot = (pItem->dis.itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
            HBRUSH hbr = isHot ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface();
            FillRect(hdc, &rc, hbr);

            HMENU hMenu = GetMenu(hwnd);
            wchar_t text[128] = { 0 };
            GetMenuStringW(hMenu, pItem->iPosition, text, 128, MF_BYPOSITION);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isHot ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary());
            HFONT hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);
            DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFont);
            return TRUE;
        }
        break;
    }"""

new_uah = r"""    case 0x0091: { // WM_UAHDRAWMENU (Dark mode menu bar background)
        if (Theme::Get().IsDarkMode()) {
            struct UAHMENU { HMENU hmenu; HDC hdc; DWORD dwFlags; };
            UAHMENU* pUah = (UAHMENU*)lParam;
            MENUBARINFO mbi = { sizeof(mbi) };
            if (GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) {
                RECT rcWindow;
                GetWindowRect(hwnd, &rcWindow);
                RECT rcBar = mbi.rcBar;
                OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
                rcBar.bottom += 2;
                FillRect(pUah->hdc, &rcBar, Theme::Get().BrushSurface());
                return TRUE;
            }
        }
        break;
    }
    case 0x0092: { // WM_UAHDRAWMENUITEM (Dark mode menu bar items)
        if (Theme::Get().IsDarkMode()) {
            struct UAHMENU { HMENU hmenu; HDC hdc; DWORD dwFlags; };
            struct UAHMENUITEM { int iPosition; };
            struct UAHDRAWMENUITEM {
                DRAWITEMSTRUCT dis;
                UAHMENU um;
                UAHMENUITEM umi;
            };
            UAHDRAWMENUITEM* pItem = (UAHDRAWMENUITEM*)lParam;
            HDC hdc = pItem->dis.hDC ? pItem->dis.hDC : pItem->um.hdc;
            RECT rc = pItem->dis.rcItem;
            bool isHot = (pItem->dis.itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
            HBRUSH hbr = isHot ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface();
            FillRect(hdc, &rc, hbr);

            HMENU hMenu = GetMenu(hwnd);
            wchar_t text[128] = { 0 };
            GetMenuStringW(hMenu, pItem->umi.iPosition, text, 128, MF_BYPOSITION);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isHot ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary());
            HFONT hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);
            DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFont);
            return TRUE;
        }
        break;
    }"""

assert old_uah in text, "old_uah not found"
text = text.replace(old_uah, new_uah)

# 8. Splitter ghosting fix in WM_MOUSEMOVE
old_split_drag = r"""            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            int tabH = Theme::Get().Scale(36, hwnd);
            int toolbarH = Theme::Get().Scale(38, hwnd);
            RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
            InvalidateRect(hwnd, &rcToolbar, TRUE);
            InvalidateRect(m_breadcrumb.GetHwnd(), nullptr, TRUE);
            InvalidateRect(m_filterBar.GetHwnd(), nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;"""

new_split_drag = r"""            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            int tabH = Theme::Get().Scale(36, hwnd);
            int toolbarH = Theme::Get().Scale(38, hwnd);
            RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
            RedrawWindow(hwnd, &rcToolbar, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            return 0;"""

assert old_split_drag in text, "old_split_drag not found"
text = text.replace(old_split_drag, new_split_drag)

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("MainWindow.cpp updated successfully.")
