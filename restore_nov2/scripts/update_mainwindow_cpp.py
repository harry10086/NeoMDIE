with open("src/ui/MainWindow.cpp", "r", encoding="utf-8") as f:
    mw = f.read()

# 1. Add tray helper functions
tray_impl = """#define WM_TRAY_NOTIFY (WM_USER + 201)

void MainWindow::AddTrayIcon() {
    if (m_hasTrayIcon) return;
    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = m_hwnd;
    nid.uID = 1001;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_NOTIFY;
    nid.hIcon = (HICON)LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    StringCchCopyW(nid.szTip, 128, L"NeoMDIE");
    Shell_NotifyIconW(NIM_ADD, &nid);
    m_hasTrayIcon = true;
}

void MainWindow::RemoveTrayIcon() {
    if (!m_hasTrayIcon) return;
    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = m_hwnd;
    nid.uID = 1001;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_hasTrayIcon = false;
}
"""

if "void MainWindow::AddTrayIcon()" not in mw:
    mw = mw.replace("MainWindow::MainWindow()", tray_impl + "\nMainWindow::MainWindow()")

# 2. Update Tools Menu
old_tools_menu = """    // =========================================================================
    // 4. 工具 (T) Tools Menu
    // =========================================================================
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"NeoMDIE 选项(&O)..." : L"NeoMDIE Options(&O)...");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_GESTURES, isZh ? L"鼠标手势(&G)..." : L"Mouse Gestures(&G)...");

    HMENU hQuickOptSubmenu = CreatePopupMenu();
    AppendMenuW(hQuickOptSubmenu, MF_STRING | (Config::Get().allowDuplicateTabs ? MF_CHECKED : MF_UNCHECKED),
                ID_TOOLS_ALLOW_DUP_TABS, isZh ? L"允许重复的文件夹(&F)" : L"Allow Duplicate Tabs(&F)");
    AppendMenuW(hQuickOptSubmenu, MF_STRING | (Config::Get().showHidden ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SHOW_HIDDEN, isZh ? L"显示隐藏文件(&H)" : L"Show Hidden Files(&H)");
    AppendMenuW(hQuickOptSubmenu, MF_STRING | (Config::Get().autoExpandTree ? MF_CHECKED : MF_UNCHECKED),
                ID_TOOLS_AUTO_EXPAND_TREE, isZh ? L"侧边栏自动同步展开(&S)" : L"Auto Expand Sidebar Tree(&S)");
    AppendMenuW(hQuickOptSubmenu, MF_STRING | (Config::Get().syncNav ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SYNC_NAV, isZh ? L"多窗格同步浏览(&Y)" : L"Sync Multi-Pane Navigation(&Y)");
    AppendMenuW(hQuickOptSubmenu, MF_STRING | (Config::Get().minimizeToTray ? MF_CHECKED : MF_UNCHECKED),
                ID_TOOLS_MIN_TO_TRAY, isZh ? L"最小化到系统托盘(&T)" : L"Minimize to Tray(&T)");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hQuickOptSubmenu, isZh ? L"快速选项设置(&Q)" : L"Quick Options(&Q)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, isZh ? L"打开命令行 CMD(&C)" : L"Open Command Prompt CMD(&C)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SHORTCUTS_SETTINGS, isZh ? L"按键设置(&K)..." : L"Keyboard Shortcuts(&K)...");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"选项设置(&O)..." : L"Options(&O)...");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hScriptSubmenu = CreatePopupMenu();
    AppendMenuW(hScriptSubmenu, MF_STRING, ID_TOOLS_RELOAD_SCRIPTS, isZh ? L"重新加载所有脚本(&R)" : L"Reload Scripts(&R)");
    AppendMenuW(hScriptSubmenu, MF_STRING, ID_TOOLS_SCRIPT_CONSOLE, isZh ? L"打开脚本控制台(&C)" : L"Script Console(&C)");
    AppendMenuW(hScriptSubmenu, MF_STRING, ID_TOOLS_OPEN_SCRIPTS_DIR, isZh ? L"打开脚本目录(&F)" : L"Open Scripts Folder(&F)");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hScriptSubmenu, isZh ? L"扩展功能(&R)" : L"Extensions & Scripts(&R)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_FOLDER_OPTIONS, isZh ? L"系统文件夹选项(&F)..." : L"Folder Options(&F)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu, isZh ? L"工具(&T)" : L"Tools(&T)");"""

new_tools_menu = """    // =========================================================================
    // 4. 工具 (T) Tools Menu
    // =========================================================================
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, isZh ? L"打开命令行 CMD(&C)" : L"Open Command Prompt CMD(&C)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"NeoMDIE 设置(&S)..." : L"NeoMDIE Settings(&S)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu, isZh ? L"工具(&T)" : L"Tools(&T)");"""

mw = mw.replace(old_tools_menu, new_tools_menu)

# 3. Update Gesture dispatching
old_gesture_cb = """    m_multiPane.onGestureAction = [this](GestureAction act) {
        if (act == GestureAction::CloseTab) {
            m_tabBar.CloseTab(m_tabBar.GetActiveIndex());
        } else if (act == GestureAction::NewTab) {
            FileListView* activeView = m_multiPane.GetActivePane();
            std::wstring cur = activeView ? activeView->GetCurrentPath() : L"This PC";
            m_tabBar.AddTab(cur, true);
        }
    };"""

new_gesture_cb = """    m_multiPane.onGestureAction = [this](GestureAction act) {
        FileListView* activeView = m_multiPane.GetActivePane();
        switch (act) {
        case GestureAction::NavBack: if (activeView) activeView->GoBack(); break;
        case GestureAction::NavForward: if (activeView) activeView->GoForward(); break;
        case GestureAction::NavUp: if (activeView) activeView->GoUp(); break;
        case GestureAction::Refresh: if (activeView) activeView->Refresh(); m_tree.Refresh(); break;
        case GestureAction::CloseTab: m_tabBar.CloseTab(m_tabBar.GetActiveIndex()); break;
        case GestureAction::NewTab: {
            std::wstring cur = activeView ? activeView->GetCurrentPath() : L"This PC";
            m_tabBar.AddTab(cur, true);
            break;
        }
        case GestureAction::DuplicateTab: OnCommand(ID_FILE_DUPLICATE_TAB); break;
        case GestureAction::ToggleTree: OnCommand(ID_VIEW_TREE_TOGGLE); break;
        case GestureAction::TogglePreview: OnCommand(ID_VIEW_PREVIEW_TOGGLE); break;
        case GestureAction::OpenTerminal: OnCommand(ID_TOOLS_OPEN_CMD); break;
        case GestureAction::MaximizePane: m_multiPane.ToggleMaximizePane(m_multiPane.GetActiveIndex()); break;
        case GestureAction::ClosePane: m_multiPane.ClosePane(m_multiPane.GetActiveIndex()); break;
        case GestureAction::ScrollTop: if (activeView) activeView->ScrollToTop(); break;
        case GestureAction::ScrollBottom: if (activeView) activeView->ScrollToBottom(); break;
        case GestureAction::PageUp: if (activeView) activeView->ScrollPageUp(); break;
        case GestureAction::PageDown: if (activeView) activeView->ScrollPageDown(); break;
        case GestureAction::NewFolder: OnCommand(ID_FILE_NEW_FOLDER); break;
        case GestureAction::Copy: OnCommand(ID_EDIT_COPY); break;
        case GestureAction::Cut: OnCommand(ID_EDIT_CUT); break;
        case GestureAction::Paste: OnCommand(ID_EDIT_PASTE); break;
        case GestureAction::SelectAll: OnCommand(ID_EDIT_SELECT_ALL); break;
        case GestureAction::Properties: OnCommand(ID_FILE_PROPERTIES); break;
        case GestureAction::LockTab: {
            int actIdx = m_tabBar.GetActiveIndex();
            m_tabBar.SetTabPinned(actIdx, !m_tabBar.IsTabPinned(actIdx));
            break;
        }
        case GestureAction::ReopenClosedTab: OnCommand(ID_FILE_REOPEN_CLOSED_TAB); break;
        case GestureAction::NextTab: m_tabBar.SelectNextTab(); break;
        case GestureAction::PrevTab: m_tabBar.SelectPrevTab(); break;
        case GestureAction::CloseOtherTabs: OnCommand(ID_FILE_CLOSE_OTHER_TABS); break;
        default: break;
        }
    };"""

mw = mw.replace(old_gesture_cb, new_gesture_cb)

# 4. Handle CMD_NEOMDIE_SETTINGS and connect view.onCustomCommand
old_custom_cmd = """        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };"""

new_custom_cmd = """        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        } else if (cmd == CMD_NEOMDIE_SETTINGS) {
            OnCommand(ID_TOOLS_SETTINGS);
        }
    };"""

mw = mw.replace(old_custom_cmd, new_custom_cmd)

old_setup_view = """        view.onStatusUpdated = [this, paneIdx](int count, uint64_t totalSize) {
            if (m_multiPane.GetActiveIndex() == paneIdx) {
                UpdateStatus(count, totalSize);
            }
        };
    };"""

new_setup_view = """        view.onStatusUpdated = [this, paneIdx](int count, uint64_t totalSize) {
            if (m_multiPane.GetActiveIndex() == paneIdx) {
                UpdateStatus(count, totalSize);
            }
        };

        view.onCustomCommand = [this](int cmd, const std::wstring& path) {
            if (m_multiPane.onCustomCommand) {
                m_multiPane.onCustomCommand(cmd, path);
            }
        };
    };"""

mw = mw.replace(old_setup_view, new_setup_view)

# 5. Sync filter query on onActivePaneChanged
old_active_pane = """        UpdateWindowTitle(p);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {"""

new_active_pane = """        UpdateWindowTitle(p);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            m_filterBar.SetText(activeView->GetFilterQuery());"""

mw = mw.replace(old_active_pane, new_active_pane)

# 6. ID_VIEW_REFRESH clears filter if filterbar is empty
old_refresh = """    case IDC_BTN_REFRESH:
    case ID_VIEW_REFRESH:
        activeView->Refresh();
        m_tree.Refresh();
        break;"""

new_refresh = """    case IDC_BTN_REFRESH:
    case ID_VIEW_REFRESH: {
        std::wstring curFilter = m_filterBar.GetText();
        if (curFilter.empty() && !activeView->GetFilterQuery().empty()) {
            activeView->SetFilter(L"", false, false);
        } else {
            activeView->Refresh();
        }
        m_tree.Refresh();
        break;
    }"""

mw = mw.replace(old_refresh, new_refresh)

# 7. WM_PAINT erases background to fix splitter trails
old_paint = """    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hbr = CreateSolidBrush(Theme::Get().ClrSplitter());"""

new_paint = """    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, Theme::Get().BrushBackground());
        HBRUSH hbr = CreateSolidBrush(Theme::Get().ClrSplitter());"""

mw = mw.replace(old_paint, new_paint)

# 8. Filter splitter resize invalidates toolbar
old_filter_resize = """            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            return 0;
        }"""

new_filter_resize = """            Config::Get().filterBarWidth = unscaled;
            LayoutAll();
            int tabH = Theme::Get().Scale(36, hwnd);
            int toolbarH = Theme::Get().Scale(38, hwnd);
            RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
            InvalidateRect(hwnd, &rcToolbar, TRUE);
            UpdateWindow(hwnd);
            return 0;
        }"""

mw = mw.replace(old_filter_resize, new_filter_resize)

# 9. Handle WM_SYSCOMMAND, WM_CLOSE, WM_TRAY_NOTIFY, and DrawMenuBar on dark mode toggle
old_syscommand = """    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;"""

new_syscommand = """    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE && Config::Get().minimizeToTray) {
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_TRAY_NOTIFY: {
        UINT uMsg = LOWORD(lParam);
        if (uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon();
        } else if (uMsg == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            bool isZh = I18n::Get().IsChinese();
            AppendMenuW(hMenu, MF_STRING, 5001, isZh ? L"显示主界面(&O)" : L"Show NeoMDIE(&O)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 5002, isZh ? L"退出(&X)" : L"Exit(&X)");
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
            if (cmd == 5001) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                RemoveTrayIcon();
            } else if (cmd == 5002) {
                RemoveTrayIcon();
                Config::Get().closeToTray = false;
                DestroyWindow(hwnd);
            }
        }
        return 0;
    }

    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;"""

mw = mw.replace(old_syscommand, new_syscommand)

old_close = """    case WM_CLOSE: {
        Config& cfg = Config::Get();"""

new_close = """    case WM_CLOSE: {
        if (Config::Get().closeToTray) {
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        RemoveTrayIcon();
        Config& cfg = Config::Get();"""

mw = mw.replace(old_close, new_close)

# Ensure DrawMenuBar on theme toggle
old_toggle_theme = """    case ID_VIEW_DARK_MODE: {
        Theme::Get().ToggleTheme();
        Config::Get().themeMode = Theme::Get().IsDarkMode() ? 1 : 0;"""

new_toggle_theme = """    case ID_VIEW_DARK_MODE: {
        Theme::Get().ToggleTheme();
        Config::Get().themeMode = Theme::Get().IsDarkMode() ? 1 : 0;
        DrawMenuBar(m_hwnd);"""

mw = mw.replace(old_toggle_theme, new_toggle_theme)

with open("src/ui/MainWindow.cpp", "w", encoding="utf-8") as f:
    f.write(mw)
print("MainWindow.cpp updated successfully!")
