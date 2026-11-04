import re

with open("src/ui/MainWindow.cpp", "r", encoding="utf-8") as f:
    code = f.read()

# 1. Update Create session tabs & initial command path
old_restore = """    // Restore sessions tabs
    for (const auto& tabPath : cfg.sessionTabs) {
        m_tabBar.AddTab(tabPath, false);
    }
    if (m_tabBar.GetTabCount() == 0) {
        m_tabBar.AddTab(L"This PC", true);
    } else {
        int act = std::clamp(cfg.activeTabIndex, 0, m_tabBar.GetTabCount() - 1);
        m_tabBar.SetActiveTab(act);
    }"""

new_restore = """    std::wstring cmdPath;
    if (__argc > 1 && __wargv && __wargv[1]) {
        std::wstring arg = __wargv[1];
        if (arg.length() >= 2 && arg.front() == L'\"' && arg.back() == L'\"') {
            arg = arg.substr(1, arg.length() - 2);
        }
        if (PathFileExistsW(arg.c_str())) {
            cmdPath = arg;
        }
    }

    if (!cmdPath.empty()) {
        m_tabBar.AddTab(cmdPath, true);
    } else {
        if (cfg.restoreTabsOnStartup) {
            for (const auto& tabPath : cfg.sessionTabs) {
                m_tabBar.AddTab(tabPath, false);
            }
        }
        if (m_tabBar.GetTabCount() == 0) {
            m_tabBar.AddTab(L"This PC", true);
        } else {
            int act = std::clamp(cfg.activeTabIndex, 0, m_tabBar.GetTabCount() - 1);
            m_tabBar.SetActiveTab(act);
        }
    }
    std::wstring activePath = m_tabBar.GetTabPath(m_tabBar.GetActiveIndex());
    UpdateWindowTitle(activePath);"""

if old_restore in code:
    code = code.replace(old_restore, new_restore, 1)
    print("MainWindow::Create restore tabs updated.")
else:
    print("Warning: old_restore not found!")

# 2. Add UpdateWindowTitle and ApplyTheme implementations
old_add_history = """void MainWindow::AddToHistory(const std::wstring& path) {"""
new_methods = """void MainWindow::UpdateWindowTitle(const std::wstring& path) {
    std::wstring name = ShellUtils::GetDisplayName(path);
    if (name.empty()) name = path;
    if (name.empty()) {
        SetWindowTextW(m_hwnd, L"NeoMDIE");
    } else {
        std::wstring fullTitle = name + L" - NeoMDIE";
        SetWindowTextW(m_hwnd, fullTitle.c_str());
    }
}

void MainWindow::ApplyTheme() {
    Theme::Get().ApplyToWindow(m_hwnd);
    for (int i = 0; i < 4; ++i) {
        m_multiPane.GetPane(i).ApplyTheme();
    }
    m_tree.ApplyTheme();
    m_preview.ApplyTheme();
    InvalidateRect(m_tabBar.GetHwnd(), nullptr, TRUE);
    InvalidateRect(m_linksBar.GetHwnd(), nullptr, TRUE);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void MainWindow::AddToHistory(const std::wstring& path) {"""

code = code.replace(old_add_history, new_methods, 1)

# 3. In BuildMenu, add Dark Mode menu item and remove Favorites menu
old_view_end = """    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_REFRESH, isZh ? L"刷新(&R)\\tF5" : L"Refresh(&R)\\tF5");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, isZh ? L"查看(&V)" : L"View(&V)");"""

new_view_end = """    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_REFRESH, isZh ? L"刷新(&R)\\tF5" : L"Refresh(&R)\\tF5");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING | (Theme::Get().IsDarkMode() ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_DARK_MODE, isZh ? L"深色主题模式(&D)" : L"Dark Theme Mode(&D)");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, isZh ? L"查看(&V)" : L"View(&V)");"""

code = code.replace(old_view_end, new_view_end, 1)

# Remove the Favorites menu block from BuildMenu
fav_pattern = r"    // =========================================================================\s+// 4\. 收藏 \(A\) Favorites Menu.*?(?=    // =========================================================================\s+// 5\. 工具)"
code = re.sub(fav_pattern, "", code, flags=re.DOTALL)
code = code.replace("// 5. 工具 (T) Tools Menu", "// 4. 工具 (T) Tools Menu")
code = code.replace("// 6. 帮助 (H) Help Menu", "// 5. 帮助 (H) Help Menu")

# 4. Wire up Window Actions, preview onCloseRequested, and fix callbacks
old_callbacks = """    // Wire up tab bar callbacks
    m_tabBar.onTabChanged = [this](int idx) {
        std::wstring path = m_tabBar.GetTabPath(idx);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            activeView->NavigateTo(path);
        }
        m_breadcrumb.SetPath(path);
        m_tree.SelectPath(path);
        UpdateNavButtons();
        AddToHistory(path);
        BuildMenu();
    };

    m_tabBar.onTabClosed = [this](int) {
        BuildMenu();
    };

    m_tabBar.onNewTabRequested = [this]() {
        FileListView* activeView = m_multiPane.GetActivePane();
        std::wstring cur = activeView ? activeView->GetCurrentPath() : L"This PC";
        m_tabBar.AddTab(cur, true);
        BuildMenu();
    };

    m_tabBar.onOpenTerminalRequested = [this](const std::wstring& path) {
        ShellUtils::LaunchTerminal(path);
    };

    m_breadcrumb.onPathNavigated = [this](const std::wstring& path) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            activeView->NavigateTo(path);
        }
    };

    m_tree.onPathSelected = [this](const std::wstring& path) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            activeView->NavigateTo(path);
        }
    };"""

new_callbacks = """    // Wire up preview close
    m_preview.onCloseRequested = [this]() {
        TogglePreview();
    };

    // Wire up tab bar callbacks
    m_tabBar.onTabChanged = [this](int idx) {
        std::wstring path = m_tabBar.GetTabPath(idx);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            activeView->NavigateTo(path);
        }
        m_breadcrumb.SetPath(path);
        if (m_treeVisible && Config::Get().autoExpandTree) {
            m_tree.SelectPath(path);
        }
        UpdateNavButtons();
        AddToHistory(path);
        UpdateWindowTitle(path);
    };

    m_tabBar.onTabClosed = [this](int) {
        int act = m_tabBar.GetActiveIndex();
        if (act >= 0) {
            UpdateWindowTitle(m_tabBar.GetTabPath(act));
        }
    };

    m_tabBar.onNewTabRequested = [this]() {
        FileListView* activeView = m_multiPane.GetActivePane();
        std::wstring cur = activeView ? activeView->GetCurrentPath() : L"This PC";
        m_tabBar.AddTab(cur, true);
    };

    m_tabBar.onOpenTerminalRequested = [this](const std::wstring& path) {
        ShellUtils::LaunchTerminal(path);
    };

    // Window Action: Breadcrumb / Address Bar
    m_breadcrumb.onPathNavigated = [this](const std::wstring& path) {
        int act = Config::Get().actionAddressOpen;
        if (act == 2) {
            m_tabBar.AddTab(path, true);
        } else if (act == 1 && m_multiPane.GetPaneCount() > 1) {
            int otherIdx = (m_multiPane.GetActiveIndex() + 1) % m_multiPane.GetPaneCount();
            m_multiPane.GetPane(otherIdx).NavigateTo(path);
            m_multiPane.SetActivePane(otherIdx);
        } else {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->NavigateTo(path);
        }
    };

    // Window Action: Tree
    m_tree.onPathSelected = [this](const std::wstring& path) {
        int act = Config::Get().actionTreeOpen;
        if (act == 2) {
            m_tabBar.AddTab(path, true);
        } else if (act == 1 && m_multiPane.GetPaneCount() > 1) {
            int otherIdx = (m_multiPane.GetActiveIndex() + 1) % m_multiPane.GetPaneCount();
            m_multiPane.GetPane(otherIdx).NavigateTo(path);
            m_multiPane.SetActivePane(otherIdx);
        } else {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->NavigateTo(path);
        }
    };"""

code = code.replace(old_callbacks, new_callbacks, 1)

# Window Action: Links Bar
old_links_nav = """    m_linksBar.onNavigate = [this](const std::wstring& path) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            activeView->NavigateTo(path);
        }
    };"""

new_links_nav = """    m_linksBar.onNavigate = [this](const std::wstring& path) {
        int act = Config::Get().actionLinksOpen;
        if (act == 2) {
            m_tabBar.AddTab(path, true);
        } else if (act == 1 && m_multiPane.GetPaneCount() > 1) {
            int otherIdx = (m_multiPane.GetActiveIndex() + 1) % m_multiPane.GetPaneCount();
            m_multiPane.GetPane(otherIdx).NavigateTo(path);
            m_multiPane.SetActivePane(otherIdx);
        } else {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->NavigateTo(path);
        }
    };"""

code = code.replace(old_links_nav, new_links_nav, 1)

# 5. Fix setupViewCallbacks to update pane header and window title
old_view_cb = """    auto setupViewCallbacks = [this](FileListView& view, int paneIdx) {
        view.onPathChanged = [this, paneIdx](const std::wstring& p) {
            if (m_multiPane.GetActiveIndex() == paneIdx) {
                m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
                m_breadcrumb.SetPath(p);
                m_tree.SelectPath(p);
                UpdateNavButtons();
                AddToHistory(p);
            }
            if (Config::Get().syncNav) {
                for (int i = 0; i < m_multiPane.GetPaneCount(); ++i) {
                    if (i != paneIdx && m_multiPane.GetPane(i).GetCurrentPath() != p) {
                        m_multiPane.GetPane(i).NavigateTo(p, false);
                    }
                }
            }
        };"""

new_view_cb = """    auto setupViewCallbacks = [this](FileListView& view, int paneIdx) {
        view.onPathChanged = [this, paneIdx](const std::wstring& p) {
            m_multiPane.UpdatePaneHeader(paneIdx);

            if (m_multiPane.GetActiveIndex() == paneIdx) {
                m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
                m_breadcrumb.SetPath(p);
                if (m_treeVisible && Config::Get().autoExpandTree) {
                    m_tree.SelectPath(p);
                }
                UpdateNavButtons();
                AddToHistory(p);
                UpdateWindowTitle(p);
            }
            if (Config::Get().syncNav) {
                for (int i = 0; i < m_multiPane.GetPaneCount(); ++i) {
                    if (i != paneIdx && m_multiPane.GetPane(i).GetCurrentPath() != p) {
                        m_multiPane.GetPane(i).NavigateTo(p, false);
                    }
                }
            }
        };"""

code = code.replace(old_view_cb, new_view_cb, 1)

# 6. Update onActivePaneChanged to update window title
old_active_changed = """    m_multiPane.onActivePaneChanged = [this](int idx, const std::wstring& p) {
        m_breadcrumb.SetPath(p);
        m_tree.SelectPath(p);
        m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
        UpdateNavButtons();"""

new_active_changed = """    m_multiPane.onActivePaneChanged = [this](int idx, const std::wstring& p) {
        m_breadcrumb.SetPath(p);
        if (m_treeVisible && Config::Get().autoExpandTree) {
            m_tree.SelectPath(p);
        }
        m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
        UpdateNavButtons();
        UpdateWindowTitle(p);"""

code = code.replace(old_active_changed, new_active_changed, 1)

# 7. Redesign DrawToolbarButton with Segoe Fluent Icons
old_draw_btn_pattern = r"void MainWindow::DrawToolbarButton\(DRAWITEMSTRUCT\* pdis\) \{.*?(?=\nLRESULT MainWindow::HandleMessage)"
new_draw_btn = """void MainWindow::DrawToolbarButton(DRAWITEMSTRUCT* pdis) {
    HDC hdc = pdis->hDC;
    RECT rc = pdis->rcItem;

    bool isHot = s_btnHoverMap[pdis->hwndItem];
    bool isSelected = (pdis->itemState & ODS_SELECTED) != 0;
    bool isDisabled = (pdis->itemState & ODS_DISABLED) != 0;
    UINT btnId = pdis->CtlID;

    bool isActiveToggle = false;
    if (btnId == IDC_BTN_TREE && m_treeVisible) isActiveToggle = true;
    else if (btnId == IDC_BTN_SPLIT && (m_multiPane.GetLayout() != PaneLayout::Single)) isActiveToggle = true;
    else if (btnId == IDC_BTN_PREVIEW && m_previewVisible) isActiveToggle = true;

    // Background surface fill
    FillRect(hdc, &rc, Theme::Get().BrushSurface());

    // Rounded rectangle on hover / selected / active
    RECT rcBtn = { rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2 };
    if (isSelected) {
        HBRUSH hbrPressed = Theme::Get().BrushSelected();
        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldBrush = SelectObject(hdc, hbrPressed);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        RoundRect(hdc, rcBtn.left, rcBtn.top, rcBtn.right, rcBtn.bottom, 6, 6);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    } else if (isHot || isActiveToggle) {
        HBRUSH hbrHover = isActiveToggle ? Theme::Get().BrushSelected() : Theme::Get().BrushHover();
        HPEN hPen = isActiveToggle ? CreatePen(PS_SOLID, 1, Theme::Get().ClrAccent()) : Theme::Get().PenBorder();
        HGDIOBJ oldBrush = SelectObject(hdc, hbrHover);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        RoundRect(hdc, rcBtn.left, rcBtn.top, rcBtn.right, rcBtn.bottom, 6, 6);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        if (isActiveToggle) DeleteObject(hPen);
    }

    // Windows 11 Fluent Icons glyph
    wchar_t glyph[2] = { 0, 0 };
    switch (btnId) {
    case IDC_BTN_BACK:    glyph[0] = 0xE72B; break; // Back
    case IDC_BTN_FORWARD: glyph[0] = 0xE72A; break; // Forward
    case IDC_BTN_UP:      glyph[0] = 0xE74A; break; // Up
    case IDC_BTN_REFRESH: glyph[0] = 0xE72C; break; // Refresh
    case IDC_BTN_NEWTAB:  glyph[0] = 0xE710; break; // Add (+)
    case IDC_BTN_TREE:    glyph[0] = 0xE8A9; break; // Navigation / Tree
    case IDC_BTN_SPLIT:   glyph[0] = 0xE784; break; // Split View
    case IDC_BTN_PREVIEW: glyph[0] = 0xE8FF; break; // Preview Pane
    default: return;
    }

    COLORREF iconColor = Theme::Get().ClrTextPrimary();
    if (isDisabled) {
        iconColor = Theme::Get().ClrTextSecondary();
    } else if (isActiveToggle || isHot) {
        iconColor = Theme::Get().ClrAccent();
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, iconColor);

    int iconPixelHeight = (btnId == IDC_BTN_NEWTAB) ? 14 : 15;
    HFONT hIconFont = Theme::Get().GetIconFont(iconPixelHeight, m_hwnd);
    HGDIOBJ oldFont = SelectObject(hdc, hIconFont);

    DrawTextW(hdc, glyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hIconFont);
}"""

code = re.sub(old_draw_btn_pattern, new_draw_btn, code, flags=re.DOTALL)

# 8. Update About dialog & add Dark Mode handler in OnCommand
old_about = """    case ID_HELP_ABOUT: {
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
    }"""

new_about = """    case ID_VIEW_DARK_MODE: {
        Theme::Get().ToggleTheme();
        Config::Get().themeMode = Theme::Get().IsDarkMode() ? 1 : 0;
        Config::Get().Save();
        ApplyTheme();
        BuildMenu();
        break;
    }

    case ID_HELP_ABOUT: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring aboutText = isZh ?
            L"NeoMDIE - 现代化纯原生 Win32 多标签/多分屏文件管理器\\n"
            L"版本: v2.1.0 (x64 Release)\\n\\n"
            L"作者: Harry (mianao)\\n"
            L"开源项目仓库: https://github.com/mianao/NeoMDIE\\n"
            L"致敬经典: MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"技术特色:\\n"
            L"• 纯 C++17 / Native Win32 API / Windows Shell COM 引擎\\n"
            L"• 静态编译 (/MT)，零额外运行时依赖，轻量疾速 (<1MB)\\n"
            L"• 支持多栏分屏、鼠标手势、多标签彩色标记与即时搜索过滤\\n\\n"
            L"基于 GNU General Public License v3 (GPLv3) 开源协议发布。" :
            L"NeoMDIE - Modern Native Win32 Tabbed & Multi-Pane File Manager\\n"
            L"Version: v2.1.0 (x64 Release)\\n\\n"
            L"Author: Harry (mianao)\\n"
            L"Repository: https://github.com/mianao/NeoMDIE\\n"
            L"Inspired by: MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"Pure C++17 / Native Win32 / Shell COM. Zero dependencies.\\n"
            L"Licensed under the GNU General Public License v3 (GPLv3).";
        MessageBoxW(m_hwnd, aboutText.c_str(), isZh ? L"关于 NeoMDIE" : L"About NeoMDIE", MB_OK | MB_ICONINFORMATION);
        break;
    }"""

code = code.replace(old_about, new_about, 1)

# 9. In HandleMessage, handle WM_CTLCOLOR* for dark mode edit & static controls
old_ctl_block = """    case WM_SETTINGCHANGE: {"""
new_ctl_block = """    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, Theme::Get().ClrSurface());
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());
        return (LRESULT)Theme::Get().BrushSurface();
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());
        return (LRESULT)Theme::Get().BrushSurface();
    }

    case WM_SETTINGCHANGE: {"""

code = code.replace(old_ctl_block, new_ctl_block, 1)

# In Settings Dialog return callback, apply theme
old_opt_call = """        SettingsDialog::Show(m_hwnd, [this]() {
            ApplyConfigChanges();
            BuildMenu();
            LayoutAll();
        });"""

new_opt_call = """        SettingsDialog::Show(m_hwnd, [this]() {
            ApplyConfigChanges();
            ApplyTheme();
            BuildMenu();
            LayoutAll();
        });"""

code = code.replace(old_opt_call, new_opt_call, 1)

with open("src/ui/MainWindow.cpp", "w", encoding="utf-8") as f:
    f.write(code)

print("MainWindow.cpp updated successfully!")
