#include "MainWindow.h"
#include "Theme.h"
#include "SettingsDialog.h"
#include "..\resource.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"
#include "..\core\ScriptEngine.h"

static const wchar_t* MAINWINDOW_CLASS = L"NeoMDIE_MainWindow";
static std::unordered_map<HWND, bool> s_btnHoverMap;

MainWindow::MainWindow() {
}

MainWindow::~MainWindow() {
}

HWND MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAINWINDOW_CLASS;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = Theme::Get().BrushBackground();
    RegisterClassExW(&wc);

    Config& cfg = Config::Get();
    cfg.Load();

    I18n::Get().Initialize();

    m_treeVisible = cfg.showTree;
    m_treeWidth = cfg.treeWidth;
    m_previewVisible = cfg.previewEnabled;
    m_previewWidth = cfg.previewWidth;

    m_hwnd = CreateWindowExW(
        0, MAINWINDOW_CLASS, L"NeoMDIE",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        cfg.window.x, cfg.window.y, cfg.window.width, cfg.window.height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return nullptr;

    Theme::Get().ApplyToWindow(m_hwnd);

    HICON hIconBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    CreateControls();
    BuildMenu();

    // Restore sessions tabs
    for (const auto& tabPath : cfg.sessionTabs) {
        m_tabBar.AddTab(tabPath, false);
    }
    if (m_tabBar.GetTabCount() == 0) {
        m_tabBar.AddTab(L"This PC", true);
    } else {
        int act = std::clamp(cfg.activeTabIndex, 0, m_tabBar.GetTabCount() - 1);
        m_tabBar.SetActiveTab(act);
    }

    if (cfg.paneLayout >= 0 && cfg.paneLayout <= 4) {
        m_multiPane.SetLayout((PaneLayout)cfg.paneLayout);
    } else if (cfg.splitEnabled) {
        m_multiPane.SetLayout(PaneLayout::DualVertical);
    }

    if (cfg.window.maximized) {
        ShowWindow(m_hwnd, SW_MAXIMIZE);
    } else {
        ShowWindow(m_hwnd, nCmdShow);
    }
    UpdateWindow(m_hwnd);

    // Initialize scripts
    ScriptEngine::Get().Initialize(m_hwnd);

    return m_hwnd;
}

void MainWindow::AddToHistory(const std::wstring& path) {
    if (path.empty() || path == L"此电脑" || path == L"This PC") return;
    auto it = std::find(m_historyPaths.begin(), m_historyPaths.end(), path);
    if (it != m_historyPaths.end()) {
        m_historyPaths.erase(it);
    }
    m_historyPaths.insert(m_historyPaths.begin(), path);
    if (m_historyPaths.size() > 15) {
        m_historyPaths.resize(15);
    }
}

void MainWindow::BuildMenu() {
    HMENU hOldMenu = GetMenu(m_hwnd);
    HMENU hMenuBar = CreateMenu();
    bool isZh = I18n::Get().IsChinese();
    FileListView* activeView = m_multiPane.GetActivePane();

    // 1. 文件 (F) File Menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_NEW_TAB, I18n::Get().T(StringId::MenuNewTab));
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_DUPLICATE_TAB, I18n::Get().T(StringId::MenuDuplicateTab));
    bool isPinned = m_tabBar.IsTabPinned(m_tabBar.GetActiveIndex());
    AppendMenuW(hFileMenu, MF_STRING | (isPinned ? MF_CHECKED : MF_UNCHECKED),
                ID_FILE_PIN_TAB, isPinned ? I18n::Get().T(StringId::MenuUnpinTab) : I18n::Get().T(StringId::MenuPinTab));
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_REOPEN_CLOSED_TAB, I18n::Get().T(StringId::MenuReopenClosedTab));
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CLOSE_TAB, I18n::Get().T(StringId::MenuCloseTab));
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CLOSE_OTHER_TABS, I18n::Get().T(StringId::MenuCloseOtherTabs));
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CLOSE_ALL_TABS, I18n::Get().T(StringId::MenuCloseAllTabs));
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hNewSubmenu = CreatePopupMenu();
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FOLDER, I18n::Get().T(StringId::MenuNewFolder));
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FILE, I18n::Get().T(StringId::MenuNewFile));
    AppendMenuW(hFileMenu, MF_POPUP, (UINT_PTR)hNewSubmenu, isZh ? L"新建(&N)" : L"New(&N)");

    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_PROPERTIES, I18n::Get().T(StringId::MenuProperties));
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_EXIT, I18n::Get().T(StringId::MenuExit));
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, I18n::Get().T(StringId::MenuFile));

    // 2. 编辑 (E) Edit Menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_CUT, I18n::Get().T(StringId::MenuCut));
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_COPY, I18n::Get().T(StringId::MenuCopy));

    HMENU hCopyPathSubmenu = CreatePopupMenu();
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_PATH, I18n::Get().T(StringId::MenuCopyPath));
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_NAME, I18n::Get().T(StringId::MenuCopyName));
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_DIR, I18n::Get().T(StringId::MenuCopyDir));
    AppendMenuW(hEditMenu, MF_POPUP, (UINT_PTR)hCopyPathSubmenu, isZh ? L"复制路径(&Y)" : L"Copy Path(&Y)");

    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE, I18n::Get().T(StringId::MenuPaste));
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE_SHORTCUT, I18n::Get().T(StringId::MenuPasteShortcut));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_DELETE, I18n::Get().T(StringId::MenuDelete));
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_RENAME, I18n::Get().T(StringId::MenuRename));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_SELECT_ALL, I18n::Get().T(StringId::MenuSelectAll));
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_INVERT_SELECTION, I18n::Get().T(StringId::MenuInvertSelection));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_VIEW_FILTER_FOCUS, I18n::Get().T(StringId::MenuFilter));
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, I18n::Get().T(StringId::MenuEdit));

    // 3. 查看 (V) View Menu
    HMENU hViewMenu = CreatePopupMenu();
    HMENU hSubLayout = CreatePopupMenu();
    PaneLayout curLayout = m_multiPane.GetLayout();
    AppendMenuW(hSubLayout, MF_STRING | (curLayout == PaneLayout::Single ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_1X1, I18n::Get().T(StringId::Layout1x1));
    AppendMenuW(hSubLayout, MF_STRING | (curLayout == PaneLayout::DualVertical ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_1X2, I18n::Get().T(StringId::Layout1x2));
    AppendMenuW(hSubLayout, MF_STRING | (curLayout == PaneLayout::DualHorizontal ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_2X1, I18n::Get().T(StringId::Layout2x1));
    AppendMenuW(hSubLayout, MF_STRING | (curLayout == PaneLayout::Quad ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_2X2, I18n::Get().T(StringId::Layout2x2));
    AppendMenuW(hSubLayout, MF_STRING | (curLayout == PaneLayout::ThreeLeft ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_1P2, I18n::Get().T(StringId::Layout1p2));
    AppendMenuW(hSubLayout, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubLayout, MF_STRING | (Config::Get().syncNav ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SYNC_NAV, I18n::Get().T(StringId::LayoutSyncNav));
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hSubLayout, isZh ? L"窗格布局(&L)" : L"Pane Layout(&L)");

    HMENU hBarsSubmenu = CreatePopupMenu();
    AppendMenuW(hBarsSubmenu, MF_STRING | (m_linksVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TOGGLE_LINKS, isZh ? L"显示链接栏 / 收藏夹(&L)" : L"Favorites / Links Bar(&L)");
    AppendMenuW(hBarsSubmenu, MF_STRING | (m_treeVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TREE_TOGGLE, I18n::Get().T(StringId::MenuTree));
    AppendMenuW(hBarsSubmenu, MF_STRING | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_PREVIEW_TOGGLE, I18n::Get().T(StringId::MenuPreview));
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hBarsSubmenu, isZh ? L"工具栏与栏目(&T)" : L"Toolbars & Sidebars(&T)");

    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_REFRESH, I18n::Get().T(StringId::MenuRefresh));
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hViewStyleSubmenu = CreatePopupMenu();
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_XLARGE, isZh ? L"超大图标(&X)" : L"Extra large icons(&X)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_LARGE, isZh ? L"大图标(&R)" : L"Large icons(&R)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_MEDIUM, isZh ? L"中等图标(&M)" : L"Medium icons(&M)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_SMALL, isZh ? L"小图标(&N)" : L"Small icons(&N)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_LIST, isZh ? L"列表(&L)" : L"List(&L)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_DETAILS, isZh ? L"详细信息(&D)" : L"Details(&D)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_TILES, isZh ? L"平铺(&S)" : L"Tiles(&S)");

    int curVm = Config::Get().viewMode;
    UINT activeVmId = ID_VIEW_DETAILS;
    if (curVm == 0) activeVmId = ID_VIEW_DETAILS;
    else if (curVm == 1) activeVmId = ID_VIEW_ICONS_LARGE;
    else if (curVm == 2) activeVmId = ID_VIEW_ICONS_SMALL;
    else if (curVm == 3) activeVmId = ID_VIEW_LIST;
    else if (curVm == 4) activeVmId = ID_VIEW_TILES;
    CheckMenuRadioItem(hViewStyleSubmenu, ID_VIEW_DETAILS, ID_VIEW_ICONS_MEDIUM, activeVmId, MF_BYCOMMAND);
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hViewStyleSubmenu, isZh ? L"视图样式(&V)" : L"View Mode(&V)");

    HMENU hSortSubmenu = CreatePopupMenu();
    SortColumn curSortCol = activeView ? activeView->GetSortColumn() : SortColumn::Name;
    bool isAsc = activeView ? activeView->IsSortAscending() : true;

    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::Name ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_NAME, I18n::Get().T(StringId::MenuSortName));
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::DateModified ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_DATE, I18n::Get().T(StringId::MenuSortDate));
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::Type ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_TYPE, I18n::Get().T(StringId::MenuSortType));
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::Size ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_SIZE, I18n::Get().T(StringId::MenuSortSize));
    AppendMenuW(hSortSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSortSubmenu, MF_STRING | (isAsc ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_ASC, I18n::Get().T(StringId::MenuSortAsc));
    AppendMenuW(hSortSubmenu, MF_STRING | (!isAsc ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_DESC, I18n::Get().T(StringId::MenuSortDesc));
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hSortSubmenu, I18n::Get().T(StringId::MenuSortBy));

    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().showHidden ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SHOW_HIDDEN, I18n::Get().T(StringId::MenuShowHidden));
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().calcFolderSize ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_CALC_FOLDER_SIZE, I18n::Get().T(StringId::MenuCalcFolderSize));
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, I18n::Get().T(StringId::MenuView));

    // 4. 收藏 (A) Favorites Menu
    HMENU hFavMenu = CreatePopupMenu();
    AppendMenuW(hFavMenu, MF_STRING, ID_FAVORITES_ADD, I18n::Get().T(StringId::MenuAddFavorites));
    AppendMenuW(hFavMenu, MF_STRING, ID_FAVORITES_MANAGE, I18n::Get().T(StringId::MenuManageFavorites));
    AppendMenuW(hFavMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_THIS_PC, I18n::Get().T(StringId::ThisPC));
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_DESKTOP, I18n::Get().T(StringId::Desktop));
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_DOWNLOADS, I18n::Get().T(StringId::Downloads));
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_DOCUMENTS, I18n::Get().T(StringId::Documents));
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_PICTURES, I18n::Get().T(StringId::Pictures));
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_USER_PROFILE, isZh ? L"用户主目录(&U)" : L"User Profile(&U)");
    AppendMenuW(hFavMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hDrivesSubmenu = CreatePopupMenu();
    DWORD driveMask = GetLogicalDrives();
    m_menuDrives.clear();
    for (int i = 0; i < 26; ++i) {
        if (driveMask & (1 << i)) {
            wchar_t rootPath[4] = { (wchar_t)(L'A' + i), L':', L'\\', 0 };
            wchar_t volName[MAX_PATH] = { 0 };
            GetVolumeInformationW(rootPath, volName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
            std::wstring label = rootPath;
            if (volName[0]) {
                label = std::wstring(volName) + L" (" + rootPath[0] + L":)";
            } else {
                UINT type = GetDriveTypeW(rootPath);
                if (type == DRIVE_CDROM) label = isZh ? (L"光驱 (" + std::wstring(1, rootPath[0]) + L":)") : (L"CD/DVD Drive (" + std::wstring(1, rootPath[0]) + L":)");
                else if (type == DRIVE_REMOVABLE) label = isZh ? (L"可移动磁盘 (" + std::wstring(1, rootPath[0]) + L":)") : (L"Removable Disk (" + std::wstring(1, rootPath[0]) + L":)");
                else label = isZh ? (L"本地磁盘 (" + std::wstring(1, rootPath[0]) + L":)") : (L"Local Disk (" + std::wstring(1, rootPath[0]) + L":)");
            }
            UINT driveCmdId = ID_FAV_DRIVE_START + (UINT)m_menuDrives.size();
            m_menuDrives.push_back(rootPath);
            AppendMenuW(hDrivesSubmenu, MF_STRING, driveCmdId, label.c_str());
        }
    }
    AppendMenuW(hFavMenu, MF_POPUP, (UINT_PTR)hDrivesSubmenu, isZh ? L"本地磁盘与驱动器(&D)" : L"Drives & Volumes(&D)");

    m_menuFavLinks.clear();
    const auto& links = m_linksBar.GetLinks();
    if (!links.empty()) {
        AppendMenuW(hFavMenu, MF_SEPARATOR, 0, nullptr);
        for (const auto& lk : links) {
            UINT favCmdId = ID_FAV_ITEM_START + (UINT)m_menuFavLinks.size();
            m_menuFavLinks.push_back(lk.path);
            AppendMenuW(hFavMenu, MF_STRING, favCmdId, lk.name.c_str());
        }
    }
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFavMenu, I18n::Get().T(StringId::MenuFavorites));

    // 5. 转到 (G) Go Menu
    HMENU hGoMenu = CreatePopupMenu();
    AppendMenuW(hGoMenu, MF_STRING, ID_NAV_BACK, I18n::Get().T(StringId::MenuBack));
    AppendMenuW(hGoMenu, MF_STRING, ID_NAV_FORWARD, I18n::Get().T(StringId::MenuForward));
    AppendMenuW(hGoMenu, MF_STRING, ID_NAV_UP, I18n::Get().T(StringId::MenuUp));
    AppendMenuW(hGoMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hGoMenu, MF_STRING, ID_NAV_HOME, I18n::Get().T(StringId::MenuAddressBar));
    AppendMenuW(hGoMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hHistSubmenu = CreatePopupMenu();
    if (m_historyPaths.empty()) {
        AppendMenuW(hHistSubmenu, MF_STRING | MF_GRAYED, 0, isZh ? L"(无历史记录)" : L"(No History)");
    } else {
        for (size_t i = 0; i < m_historyPaths.size(); ++i) {
            UINT histCmdId = ID_HISTORY_START + (UINT)i;
            AppendMenuW(hHistSubmenu, MF_STRING, histCmdId, m_historyPaths[i].c_str());
        }
    }
    AppendMenuW(hGoMenu, MF_POPUP, (UINT_PTR)hHistSubmenu, isZh ? L"历史记录(&H)" : L"History(&H)");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hGoMenu, I18n::Get().T(StringId::MenuGo));

    // 6. 标签页 (B) Tabs Menu
    HMENU hTabsMenu = CreatePopupMenu();
    AppendMenuW(hTabsMenu, MF_STRING, ID_FILE_NEW_TAB, I18n::Get().T(StringId::MenuNewTab));
    AppendMenuW(hTabsMenu, MF_STRING, ID_FILE_DUPLICATE_TAB, I18n::Get().T(StringId::MenuDuplicateTab));
    AppendMenuW(hTabsMenu, MF_STRING | (isPinned ? MF_CHECKED : MF_UNCHECKED),
                ID_FILE_PIN_TAB, isPinned ? I18n::Get().T(StringId::MenuUnpinTab) : I18n::Get().T(StringId::MenuPinTab));
    AppendMenuW(hTabsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTabsMenu, MF_STRING, ID_FILE_REOPEN_CLOSED_TAB, I18n::Get().T(StringId::MenuReopenClosedTab));
    AppendMenuW(hTabsMenu, MF_STRING, ID_FILE_CLOSE_TAB, I18n::Get().T(StringId::MenuCloseTab));
    AppendMenuW(hTabsMenu, MF_STRING, ID_FILE_CLOSE_OTHER_TABS, I18n::Get().T(StringId::MenuCloseOtherTabs));
    AppendMenuW(hTabsMenu, MF_STRING, ID_FILE_CLOSE_ALL_TABS, I18n::Get().T(StringId::MenuCloseAllTabs));
    AppendMenuW(hTabsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTabsMenu, MF_STRING, ID_TAB_NEXT, I18n::Get().T(StringId::MenuNextTab));
    AppendMenuW(hTabsMenu, MF_STRING, ID_TAB_PREV, I18n::Get().T(StringId::MenuPrevTab));
    AppendMenuW(hTabsMenu, MF_SEPARATOR, 0, nullptr);

    const auto& allTabs = m_tabBar.GetTabs();
    int activeIdx = m_tabBar.GetActiveIndex();
    for (size_t i = 0; i < allTabs.size(); ++i) {
        UINT tabCmdId = ID_TAB_ITEM_START + (UINT)i;
        std::wstring title = allTabs[i].title.empty() ? allTabs[i].path : allTabs[i].title;
        std::wstring itemLabel = std::to_wstring(i + 1) + L" " + title;
        AppendMenuW(hTabsMenu, MF_STRING | (i == (size_t)activeIdx ? MF_CHECKED : MF_UNCHECKED), tabCmdId, itemLabel.c_str());
    }
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hTabsMenu, I18n::Get().T(StringId::MenuTabs));

    // 7. 工具 (T) Tools Menu
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_TERMINAL, I18n::Get().T(StringId::MenuTerminal));
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_POWERSHELL, I18n::Get().T(StringId::MenuPowerShell));
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, I18n::Get().T(StringId::MenuCmd));
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_VSCODE, I18n::Get().T(StringId::MenuVSCode));
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_RELOAD_SCRIPTS, I18n::Get().T(StringId::MenuReloadScripts));
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SCRIPT_CONSOLE, I18n::Get().T(StringId::MenuScriptConsole));
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"选项与设置(&S)..." : L"Options & Settings(&S)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu, I18n::Get().T(StringId::MenuTools));

    // 8. 帮助 (H) Help Menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_SHORTCUTS, I18n::Get().T(StringId::MenuHelpShortcuts));
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GESTURES, I18n::Get().T(StringId::MenuHelpGestures));
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_MDIE_HOMEPAGE, I18n::Get().T(StringId::MenuHelpMdieHome));
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GITHUB, I18n::Get().T(StringId::MenuHelpGitHub));
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_ABOUT, I18n::Get().T(StringId::MenuAbout));
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, I18n::Get().T(StringId::MenuHelp));

    SetMenu(m_hwnd, hMenuBar);
    if (hOldMenu) {
        DestroyMenu(hOldMenu);
    }
}

void MainWindow::CreateControls() {
    // 1. TabBar
    m_tabBar.Create(m_hwnd, 101);

    // 2. Toolbar buttons with bold icons & larger touch area
    auto createBtn = [this](const wchar_t* text, int id) {
        HWND hBtn = CreateWindowExW(
            0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, Theme::Get().Scale(36, m_hwnd), Theme::Get().Scale(32, m_hwnd),
            m_hwnd, (HMENU)(INT_PTR)id, m_hInstance, nullptr
        );
        SetWindowSubclass(hBtn, [](HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) -> LRESULT {
            if (m == WM_MOUSEMOVE) {
                if (!s_btnHoverMap[h]) {
                    s_btnHoverMap[h] = true;
                    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
                    TrackMouseEvent(&tme);
                    InvalidateRect(h, nullptr, FALSE);
                }
            } else if (m == WM_MOUSELEAVE) {
                s_btnHoverMap[h] = false;
                InvalidateRect(h, nullptr, FALSE);
            }
            return DefSubclassProc(h, m, w, l);
        }, 100, 0);
        return hBtn;
    };

    m_hwndBtnBack    = createBtn(L"\uE72B", IDC_BTN_BACK);
    m_hwndBtnForward = createBtn(L"\uE72A", IDC_BTN_FORWARD);
    m_hwndBtnUp      = createBtn(L"\uE74A", IDC_BTN_UP);
    m_hwndBtnRefresh = createBtn(L"\uE72C", IDC_BTN_REFRESH);
    m_hwndBtnNewTab  = createBtn(L"\uE710", IDC_BTN_NEWTAB);
    m_hwndBtnTree    = createBtn(L"\uE8A7", IDC_BTN_TREE);
    m_hwndBtnSplit   = createBtn(L"\uE740", IDC_BTN_SPLIT);
    m_hwndBtnPreview = createBtn(L"\uE896", IDC_BTN_PREVIEW);

    // Tooltips
    m_hwndTooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        m_hwnd, nullptr, m_hInstance, nullptr
    );
    SetWindowPos(m_hwndTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    auto addTip = [this](HWND hCtrl, const wchar_t* text) {
        TOOLINFOW ti = { sizeof(ti) };
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = m_hwnd;
        ti.uId = (UINT_PTR)hCtrl;
        ti.lpszText = (LPWSTR)text;
        SendMessageW(m_hwndTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    };

    bool isZh = I18n::Get().IsChinese();
    addTip(m_hwndBtnBack, isZh ? L"后退 (Alt+Left)" : L"Back (Alt+Left)");
    addTip(m_hwndBtnForward, isZh ? L"前进 (Alt+Right)" : L"Forward (Alt+Right)");
    addTip(m_hwndBtnUp, isZh ? L"向上一级 (Backspace / Alt+Up)" : L"Up (Backspace / Alt+Up)");
    addTip(m_hwndBtnRefresh, isZh ? L"刷新 (F5)" : L"Refresh (F5)");
    addTip(m_hwndBtnNewTab, isZh ? L"新建标签页 (Ctrl+T)" : L"New Tab (Ctrl+T)");
    addTip(m_hwndBtnTree, isZh ? L"文件夹目录树 (F8)" : L"Directory Tree (F8)");
    addTip(m_hwndBtnSplit, isZh ? L"多栏分屏 (F4)" : L"Split View (F4)");
    addTip(m_hwndBtnPreview, isZh ? L"快速预览 (Alt+P)" : L"Preview Pane (Alt+P)");

    // 3. Breadcrumb & FilterBar
    m_breadcrumb.Create(m_hwnd, 102);
    m_filterBar.Create(m_hwnd, 103);

    // 4. Links / Bookmarks Bar
    m_linksBar.Create(m_hwnd, 108);

    // 5. Directory Tree on Left
    m_tree.Create(m_hwnd, 107);
    ShowWindow(m_tree.GetHwnd(), m_treeVisible ? SW_SHOW : SW_HIDE);

    // 6. Multi-Pane Container in Center
    m_multiPane.Create(m_hwnd, 104);

    // 7. Preview Pane on Right
    m_preview.Create(m_hwnd, 105);
    ShowWindow(m_preview.GetHwnd(), m_previewVisible ? SW_SHOW : SW_HIDE);

    // 8. Status Bar
    m_hwndStatusBar = CreateStatusWindowW(
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        I18n::Get().T(StringId::StatusReady), m_hwnd, 106
    );
    Theme::Get().ApplyToWindow(m_hwndStatusBar);
    SendMessageW(m_hwndStatusBar, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    int parts[] = { 200, 450, -1 };
    SendMessageW(m_hwndStatusBar, SB_SETPARTS, 3, (LPARAM)parts);

    // Wire up links bar callbacks
    m_linksBar.onNavigate = [this](const std::wstring& path) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) activeView->NavigateTo(path);
    };
    m_linksBar.onOpenNewTab = [this](const std::wstring& path) {
        m_tabBar.AddTab(path, true);
    };
    m_linksBar.onOpenSplit = [this](const std::wstring& path) {
        m_multiPane.SetLayout(PaneLayout::DualVertical);
        m_multiPane.GetPane(1).NavigateTo(path);
        m_multiPane.SetActivePane(1);
    };

    // Wire up tab bar callbacks
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
    };

    m_filterBar.onFilterChanged = [this](const std::wstring& q, bool sub, bool matchCase) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            activeView->SetFilter(q, sub, matchCase);
        }
    };

    m_filterBar.onEscapePressed = [this]() {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            SetFocus(activeView->GetHwnd());
        }
    };

    // MultiPane gesture & custom command callbacks
    m_multiPane.onGestureAction = [this](GestureAction act) {
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
        case GestureAction::CloseOtherTabs: OnCommand(ID_FILE_CLOSE_OTHER_TABS); break;
        case GestureAction::ReopenClosedTab: OnCommand(ID_FILE_REOPEN_CLOSED_TAB); break;
        case GestureAction::LockTab: {
            int actIdx = m_tabBar.GetActiveIndex();
            m_tabBar.TogglePinTab(actIdx);
            break;
        }
        case GestureAction::NextTab: m_tabBar.NextTab(); break;
        case GestureAction::PrevTab: m_tabBar.PrevTab(); break;
        case GestureAction::ScrollTop: if (activeView) activeView->ScrollToTop(); break;
        case GestureAction::ScrollBottom: if (activeView) activeView->ScrollToBottom(); break;
        case GestureAction::PageUp: if (activeView) activeView->ScrollPageUp(); break;
        case GestureAction::PageDown: if (activeView) activeView->ScrollPageDown(); break;
        case GestureAction::Copy: OnCommand(ID_EDIT_COPY); break;
        case GestureAction::Cut: OnCommand(ID_EDIT_CUT); break;
        case GestureAction::Paste: OnCommand(ID_EDIT_PASTE); break;
        case GestureAction::SelectAll: OnCommand(ID_EDIT_SELECT_ALL); break;
        case GestureAction::NewFolder: OnCommand(ID_FILE_NEW_FOLDER); break;
        case GestureAction::Properties: OnCommand(ID_FILE_PROPERTIES); break;
        case GestureAction::LayoutSingle: OnCommand(ID_VIEW_LAYOUT_1X1); break;
        case GestureAction::Layout2H: OnCommand(ID_VIEW_LAYOUT_1X2); break;
        case GestureAction::Layout2V: OnCommand(ID_VIEW_LAYOUT_2X1); break;
        case GestureAction::Layout4Grid: OnCommand(ID_VIEW_LAYOUT_2X2); break;
        case GestureAction::CopyPath: OnCommand(ID_EDIT_COPY_PATH); break;
        case GestureAction::Delete: OnCommand(ID_EDIT_DELETE); break;
        case GestureAction::Rename: OnCommand(ID_EDIT_RENAME); break;
        case GestureAction::NewMD: OnCommand(ID_FILE_NEW_MD); break;
        case GestureAction::ToggleHidden: OnCommand(ID_VIEW_SHOW_HIDDEN); break;
        case GestureAction::OpenSettings: OnCommand(ID_TOOLS_SETTINGS); break;
        case GestureAction::InvertSelection: OnCommand(ID_EDIT_INVERT_SELECTION); break;
        default: break;
        }
    };

    m_multiPane.onCustomCommand = [this](int cmd, const std::wstring& path) {
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB) {
            m_tabBar.AddTab(path, true);
        } else if (cmd == CMD_NEOMDIE_OPEN_SPLIT) {
            m_multiPane.SetLayout(PaneLayout::DualVertical);
            m_multiPane.GetPane(1).NavigateTo(path);
            m_multiPane.SetActivePane(1);
        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };

    m_tree.onCustomCommand = [this](int cmd, const std::wstring& path) {
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB) {
            m_tabBar.AddTab(path, true);
        } else if (cmd == CMD_NEOMDIE_OPEN_SPLIT) {
            m_multiPane.SetLayout(PaneLayout::DualVertical);
            m_multiPane.GetPane(1).NavigateTo(path);
            m_multiPane.SetActivePane(1);
        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };

    m_tree.onCustomCommand = [this](int cmd, const std::wstring& path) {
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB) {
            m_tabBar.AddTab(path, true);
        } else if (cmd == CMD_NEOMDIE_OPEN_SPLIT) {
            m_multiPane.SetLayout(PaneLayout::DualVertical);
            m_multiPane.GetPane(1).NavigateTo(path);
            m_multiPane.SetActivePane(1);
        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };

    m_tree.onCustomCommand = [this](int cmd, const std::wstring& path) {
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB) {
            m_tabBar.AddTab(path, true);
        } else if (cmd == CMD_NEOMDIE_OPEN_SPLIT) {
            m_multiPane.SetLayout(PaneLayout::DualVertical);
            m_multiPane.GetPane(1).NavigateTo(path);
            m_multiPane.SetActivePane(1);
        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };

    auto setupViewCallbacks = [this](FileListView& view, int paneIdx) {
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
        };

        view.onSelectionChanged = [this, paneIdx](const std::wstring& sel) {
            if (m_previewVisible && m_multiPane.GetActiveIndex() == paneIdx) {
                m_preview.SetFile(sel);
            }
        };

        view.onStatusUpdated = [this, paneIdx](int count, uint64_t totalSize) {
            if (m_multiPane.GetActiveIndex() == paneIdx) {
                UpdateStatus(count, totalSize);
            }
        };
    };

    for (int i = 0; i < 4; ++i) {
        setupViewCallbacks(m_multiPane.GetPane(i), i);
    }
    m_multiPane.SetShowHidden(Config::Get().showHidden);
    m_multiPane.SetCalcFolderSize(Config::Get().calcFolderSize);

    m_multiPane.onActivePaneChanged = [this](int idx, const std::wstring& p) {
        m_breadcrumb.SetPath(p);
        m_tree.SelectPath(p);
        m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
        UpdateNavButtons();
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            auto sel = activeView->GetSelectedPaths();
            if (m_previewVisible && !sel.empty()) {
                m_preview.SetFile(sel[0]);
            }
        }
    };
}

void MainWindow::LayoutAll() {
    if (!m_hwnd) return;

    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    int totalW = rcClient.right - rcClient.left;
    int totalH = rcClient.bottom - rcClient.top;

    int tabH = Theme::Get().Scale(36, m_hwnd);
    int toolbarH = Theme::Get().Scale(38, m_hwnd);
    int linksH = m_linksVisible ? Theme::Get().Scale(28, m_hwnd) : 0;
    int statusH = Theme::Get().Scale(24, m_hwnd);

    // 1. TabBar
    m_tabBar.UpdateLayout(totalW, tabH);

    // 2. Toolbar
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

    Config& cfg = Config::Get();
    int rightBtnsW = btnW * 2 + spacing * 3;
    int filterW = Theme::Get().Scale(cfg.filterBarWidth, m_hwnd);
    int splitBarToolW = (!cfg.lockToolbars) ? Theme::Get().Scale(5, m_hwnd) : 0;
    int breadcrumbW = totalW - curX - filterW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
    if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) {
        breadcrumbW = Theme::Get().Scale(120, m_hwnd);
        filterW = totalW - curX - breadcrumbW - rightBtnsW - splitBarToolW - Theme::Get().Scale(16, m_hwnd);
        if (filterW < Theme::Get().Scale(80, m_hwnd)) filterW = Theme::Get().Scale(80, m_hwnd);
    }

    if (cfg.searchBarFirst) {
        m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
        curX += filterW;
        if (!cfg.lockToolbars) {
            m_rcFilterSplitter.left = curX;
            m_rcFilterSplitter.top = barY;
            m_rcFilterSplitter.right = curX + splitBarToolW;
            m_rcFilterSplitter.bottom = barY + btnH;
            curX += splitBarToolW;
        } else {
            SetRectEmpty(&m_rcFilterSplitter);
        }
        m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
        curX += breadcrumbW + spacing * 2;
    } else {
        m_breadcrumb.UpdateLayout(curX, barY, breadcrumbW, btnH);
        curX += breadcrumbW;
        if (!cfg.lockToolbars) {
            m_rcFilterSplitter.left = curX;
            m_rcFilterSplitter.top = barY;
            m_rcFilterSplitter.right = curX + splitBarToolW;
            m_rcFilterSplitter.bottom = barY + btnH;
            curX += splitBarToolW;
        } else {
            SetRectEmpty(&m_rcFilterSplitter);
        }
        m_filterBar.UpdateLayout(curX, barY, filterW, btnH);
        curX += filterW + spacing * 2;
    }

    MoveWindow(m_hwndBtnSplit, curX, barY, btnW, btnH, TRUE);
    curX += btnW + spacing;

    MoveWindow(m_hwndBtnPreview, curX, barY, btnW, btnH, TRUE);

    // Links Bar
    if (m_linksVisible) {
        ShowWindow(m_linksBar.GetHwnd(), SW_SHOW);
        m_linksBar.UpdateLayout(0, tabH + toolbarH, totalW, linksH);
    } else {
        ShowWindow(m_linksBar.GetHwnd(), SW_HIDE);
    }

    // 3. Center Content Area (Directory Tree + Split Container + Preview Pane)
    int contentY = tabH + toolbarH + linksH;
    int contentH = totalH - contentY - statusH;
    int splitBarW = Theme::Get().Scale(5, m_hwnd);

    int mainStartX = 0;
    int mainWidth = totalW;

    if (m_treeVisible) {
        if (m_treeWidth < Theme::Get().Scale(120, m_hwnd)) {
            m_treeWidth = Theme::Get().Scale(120, m_hwnd);
        }
        if (m_treeWidth > totalW / 2) {
            m_treeWidth = totalW / 2;
        }

        m_tree.UpdateLayout(0, contentY, m_treeWidth, contentH);

        m_rcTreeSplitter.left = m_treeWidth;
        m_rcTreeSplitter.top = contentY;
        m_rcTreeSplitter.right = m_treeWidth + splitBarW;
        m_rcTreeSplitter.bottom = contentY + contentH;

        mainStartX = m_treeWidth + splitBarW;
        mainWidth = totalW - mainStartX;
    }

    if (!m_previewVisible) {
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
    }

    // 4. Status Bar
    if (m_hwndStatusBar) {
        SendMessageW(m_hwndStatusBar, WM_SIZE, 0, 0);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::UpdateStatus(int itemCount, uint64_t totalSize) {
    if (!m_hwndStatusBar) return;

    wchar_t buf1[64];
    StringCchPrintfW(buf1, 64, I18n::Get().T(StringId::StatusItems), itemCount);
    SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)buf1);

    wchar_t buf2[64];
    StringCchPrintfW(buf2, 64, I18n::Get().T(StringId::StatusSize), Util::FormatBytes(totalSize).c_str());
    SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 1, (LPARAM)buf2);
}

void MainWindow::UpdateNavButtons() {
    FileListView* activeView = m_multiPane.GetActivePane();
    if (activeView) {
        EnableWindow(m_hwndBtnBack, activeView->CanGoBack());
        EnableWindow(m_hwndBtnForward, activeView->CanGoForward());
    }
}

void MainWindow::UpdateLanguageUI() {
    BuildMenu();
    SetWindowTextW(m_hwndBtnTree, I18n::Get().T(StringId::BtnTree));
    SetWindowTextW(m_hwndBtnSplit, I18n::Get().T(StringId::BtnSplit));
    SetWindowTextW(m_hwndBtnPreview, I18n::Get().T(StringId::BtnPreview));

    m_filterBar.UpdateLanguage();
    for (int i = 0; i < 4; ++i) {
        m_multiPane.GetPane(i).UpdateColumnsLanguage();
    }
    m_tree.Refresh();

    FileListView* activeView = m_multiPane.GetActivePane();
    if (activeView) {
        activeView->Refresh();
    }
    LayoutAll();
}

void MainWindow::SetLayout(PaneLayout layout) {
    m_multiPane.SetLayout(layout);
    Config::Get().paneLayout = (int)layout;
    Config::Get().Save();
    BuildMenu();
    LayoutAll();
}

void MainWindow::ShowLayoutMenu(POINT ptScreen) {
    HMENU hMenu = CreatePopupMenu();
    PaneLayout cur = m_multiPane.GetLayout();

    AppendMenuW(hMenu, MF_STRING | (cur == PaneLayout::Single ? MF_CHECKED : 0), 1, I18n::Get().T(StringId::Layout1x1));
    AppendMenuW(hMenu, MF_STRING | (cur == PaneLayout::DualVertical ? MF_CHECKED : 0), 2, I18n::Get().T(StringId::Layout1x2));
    AppendMenuW(hMenu, MF_STRING | (cur == PaneLayout::DualHorizontal ? MF_CHECKED : 0), 3, I18n::Get().T(StringId::Layout2x1));
    AppendMenuW(hMenu, MF_STRING | (cur == PaneLayout::Quad ? MF_CHECKED : 0), 4, I18n::Get().T(StringId::Layout2x2));
    AppendMenuW(hMenu, MF_STRING | (cur == PaneLayout::ThreeLeft ? MF_CHECKED : 0), 5, I18n::Get().T(StringId::Layout1p2));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (Config::Get().syncNav ? MF_CHECKED : 0), 6, I18n::Get().T(StringId::LayoutSyncNav));

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case 1: SetLayout(PaneLayout::Single); break;
    case 2: SetLayout(PaneLayout::DualVertical); break;
    case 3: SetLayout(PaneLayout::DualHorizontal); break;
    case 4: SetLayout(PaneLayout::Quad); break;
    case 5: SetLayout(PaneLayout::ThreeLeft); break;
    case 6:
        Config::Get().syncNav = !Config::Get().syncNav;
        Config::Get().Save();
        BuildMenu();
        break;
    }
}

void MainWindow::ToggleTree() {
    m_treeVisible = !m_treeVisible;
    ShowWindow(m_tree.GetHwnd(), m_treeVisible ? SW_SHOW : SW_HIDE);
    BuildMenu();
    LayoutAll();
}

void MainWindow::ToggleLinksBar() {
    m_linksVisible = !m_linksVisible;
    ShowWindow(m_linksBar.GetHwnd(), m_linksVisible ? SW_SHOW : SW_HIDE);
    BuildMenu();
    LayoutAll();
}

void MainWindow::ShowToolbarContextMenu(POINT ptScreen) {
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
}

void MainWindow::TogglePreview() {
    m_previewVisible = !m_previewVisible;
    ShowWindow(m_preview.GetHwnd(), m_previewVisible ? SW_SHOW : SW_HIDE);
    if (m_previewVisible) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            auto sel = activeView->GetSelectedPaths();
            if (!sel.empty()) {
                m_preview.SetFile(sel[0]);
            }
        }
    } else {
        m_preview.Clear();
    }
    BuildMenu();
    LayoutAll();
}

void MainWindow::OnCommand(UINT cmdId) {
    FileListView* activeView = m_multiPane.GetActivePane();
    if (!activeView) return;

    switch (cmdId) {
    case IDC_BTN_BACK:
    case ID_NAV_BACK:
        activeView->GoBack();
        break;
    case IDC_BTN_FORWARD:
    case ID_NAV_FORWARD:
        activeView->GoForward();
        break;
    case IDC_BTN_UP:
    case ID_NAV_UP:
        activeView->GoUp();
        break;
    case IDC_BTN_REFRESH:
    case ID_VIEW_REFRESH:
        activeView->Refresh();
        m_tree.Refresh();
        break;
    case IDC_BTN_NEWTAB:
        m_tabBar.AddTab(activeView->GetCurrentPath(), true);
        break;
    case ID_VIEW_TOGGLE_LINKS:
    case 1001: // Toggle Links Bar
        ToggleLinksBar();
        BuildMenu();
        break;
    case 9901: // Add current to links
        m_linksBar.AddLink(L"", activeView->GetCurrentPath());
        BuildMenu();
        break;
    case IDC_BTN_TREE:
    case ID_VIEW_TREE_TOGGLE:
        ToggleTree();
        BuildMenu();
        break;
    case IDC_BTN_SPLIT: {
        RECT rcBtn;
        GetWindowRect(m_hwndBtnSplit, &rcBtn);
        POINT pt = { rcBtn.left, rcBtn.bottom };
        ShowLayoutMenu(pt);
        break;
    }
    case ID_VIEW_LAYOUT_1X1:
        SetLayout(PaneLayout::Single);
        break;
    case ID_VIEW_LAYOUT_1X2:
        SetLayout(PaneLayout::DualVertical);
        break;
    case ID_VIEW_LAYOUT_2X1:
        SetLayout(PaneLayout::DualHorizontal);
        break;
    case ID_VIEW_LAYOUT_2X2:
        SetLayout(PaneLayout::Quad);
        break;
    case ID_VIEW_LAYOUT_1P2:
        SetLayout(PaneLayout::ThreeLeft);
        break;
    case ID_VIEW_SYNC_NAV:
        Config::Get().syncNav = !Config::Get().syncNav;
        Config::Get().Save();
        BuildMenu();
        break;

    case IDC_BTN_PREVIEW:
    case ID_VIEW_PREVIEW_TOGGLE:
        TogglePreview();
        BuildMenu();
        break;

    case ID_LANG_ZH:
        I18n::Get().SetLanguage(Language::Chinese);
        UpdateLanguageUI();
        break;
    case ID_LANG_EN:
        I18n::Get().SetLanguage(Language::English);
        UpdateLanguageUI();
        break;

    case ID_FILE_NEW_TAB:
        m_tabBar.AddTab(activeView->GetCurrentPath(), true);
        BuildMenu();
        break;
    case ID_FILE_CLOSE_TAB:
        m_tabBar.CloseTab(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_CLOSE_OTHER_TABS:
        m_tabBar.CloseOtherTabs(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_CLOSE_ALL_TABS:
        m_tabBar.CloseAllTabs();
        BuildMenu();
        break;
    case ID_FILE_REOPEN_CLOSED_TAB:
        m_tabBar.ReopenClosedTab();
        BuildMenu();
        break;
    case ID_FILE_DUPLICATE_TAB:
        m_tabBar.AddTab(activeView->GetCurrentPath(), true);
        BuildMenu();
        break;
    case ID_FILE_PIN_TAB:
        m_tabBar.TogglePinTab(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_NEW_FOLDER: {
        std::wstring newDir = activeView->GetCurrentPath() + L"\\" + I18n::Get().T(StringId::NewFolder);
        int idx = 2;
        while (PathFileExistsW(newDir.c_str())) {
            newDir = activeView->GetCurrentPath() + L"\\" + I18n::Get().T(StringId::NewFolder) + L" (" + std::to_wstring(idx++) + L")";
        }
        CreateDirectoryW(newDir.c_str(), nullptr);
        activeView->Refresh();
        break;
    }
    case ID_FILE_NEW_FILE: {
        std::wstring newFile = activeView->GetCurrentPath() + L"\\" + I18n::Get().T(StringId::NewTextFile);
        int idx = 2;
        while (PathFileExistsW(newFile.c_str())) {
            newFile = activeView->GetCurrentPath() + L"\\New (" + std::to_wstring(idx++) + L").txt";
        }
        HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hF != INVALID_HANDLE_VALUE) CloseHandle(hF);
        activeView->Refresh();
        break;
    }
    case ID_FILE_PROPERTIES: {
        auto sel = activeView->GetSelectedPaths();
        if (!sel.empty()) {
            ShellUtils::ShowItemProperties(m_hwnd, sel[0]);
        } else {
            ShellUtils::ShowItemProperties(m_hwnd, activeView->GetCurrentPath());
        }
        break;
    }
    case ID_FILE_EXIT:
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
        break;

    case ID_EDIT_CUT:
        activeView->CopySelected(true);
        break;
    case ID_EDIT_COPY:
        activeView->CopySelected(false);
        break;
    case ID_EDIT_COPY_NAME: {
        auto sel = activeView->GetSelectedPaths();
        if (!sel.empty()) {
            std::wstring names;
            for (const auto& p : sel) {
                if (!names.empty()) names += L"\r\n";
                names += PathFindFileNameW(p.c_str());
            }
            ShellUtils::CopyToClipboard(names);
        } else {
            ShellUtils::CopyToClipboard(PathFindFileNameW(activeView->GetCurrentPath().c_str()));
        }
        break;
    }
    case ID_EDIT_COPY_DIR:
        ShellUtils::CopyToClipboard(activeView->GetCurrentPath());
        break;
    case ID_EDIT_PASTE:
        activeView->PasteToCurrent();
        break;
    case ID_EDIT_PASTE_SHORTCUT:
        activeView->PasteShortcut();
        break;
    case ID_EDIT_DELETE:
        activeView->DeleteSelected(GetKeyState(VK_SHIFT) < 0);
        break;
    case ID_EDIT_RENAME:
        activeView->StartRename();
        break;
    case ID_EDIT_SELECT_ALL:
        activeView->SelectAll();
        break;
    case ID_EDIT_INVERT_SELECTION:
        activeView->InvertSelection();
        break;
    case ID_EDIT_COPY_PATH: {
        auto sel = activeView->GetSelectedPaths();
        if (!sel.empty()) {
            ShellUtils::CopyToClipboard(sel[0]);
        } else {
            ShellUtils::CopyToClipboard(activeView->GetCurrentPath());
        }
        break;
    }

    case ID_VIEW_FILTER_FOCUS:
        m_filterBar.SetFocus();
        break;
    case ID_VIEW_ADDRESS_FOCUS:
    case ID_NAV_HOME:
        SendMessageW(m_breadcrumb.GetHwnd(), WM_LBUTTONDOWN, 0, MAKELPARAM(10, 10));
        break;

    case ID_VIEW_DETAILS:
        Config::Get().viewMode = 0;
        activeView->SetViewMode(0);
        BuildMenu();
        break;
    case ID_VIEW_ICONS_LARGE:
    case ID_VIEW_ICONS_XLARGE:
        Config::Get().viewMode = 1;
        activeView->SetViewMode(1);
        BuildMenu();
        break;
    case ID_VIEW_ICONS_MEDIUM:
        Config::Get().viewMode = 0;
        activeView->SetViewMode(0);
        BuildMenu();
        break;
    case ID_VIEW_ICONS_SMALL:
        Config::Get().viewMode = 2;
        activeView->SetViewMode(2);
        BuildMenu();
        break;
    case ID_VIEW_LIST:
        Config::Get().viewMode = 3;
        activeView->SetViewMode(3);
        BuildMenu();
        break;
    case ID_VIEW_TILES:
        Config::Get().viewMode = 4;
        activeView->SetViewMode(4);
        BuildMenu();
        break;

    case ID_VIEW_SORT_NAME:
        activeView->SetSort(SortColumn::Name, true);
        BuildMenu();
        break;
    case ID_VIEW_SORT_DATE:
        activeView->SetSort(SortColumn::DateModified, false);
        BuildMenu();
        break;
    case ID_VIEW_SORT_TYPE:
        activeView->SetSort(SortColumn::Type, true);
        BuildMenu();
        break;
    case ID_VIEW_SORT_SIZE:
        activeView->SetSort(SortColumn::Size, false);
        BuildMenu();
        break;
    case ID_VIEW_SORT_ASC:
        activeView->SetSort(activeView->GetSortColumn(), true);
        BuildMenu();
        break;
    case ID_VIEW_SORT_DESC:
        activeView->SetSort(activeView->GetSortColumn(), false);
        BuildMenu();
        break;

    case ID_FAVORITES_ADD:
        m_linksBar.AddLink(L"", activeView->GetCurrentPath());
        BuildMenu();
        break;
    case ID_FAVORITES_MANAGE:
        ToggleLinksBar();
        break;
    case ID_FAV_THIS_PC:
        activeView->NavigateTo(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}");
        break;
    case ID_FAV_DESKTOP: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &p)) && p) {
            activeView->NavigateTo(p);
            CoTaskMemFree(p);
        }
        break;
    }
    case ID_FAV_DOWNLOADS: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &p)) && p) {
            activeView->NavigateTo(p);
            CoTaskMemFree(p);
        }
        break;
    }
    case ID_FAV_DOCUMENTS: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &p)) && p) {
            activeView->NavigateTo(p);
            CoTaskMemFree(p);
        }
        break;
    }
    case ID_FAV_PICTURES: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &p)) && p) {
            activeView->NavigateTo(p);
            CoTaskMemFree(p);
        }
        break;
    }
    case ID_FAV_USER_PROFILE: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &p)) && p) {
            activeView->NavigateTo(p);
            CoTaskMemFree(p);
        }
        break;
    }

    case ID_TAB_NEXT:
        m_tabBar.NextTab();
        BuildMenu();
        break;
    case ID_TAB_PREV:
        m_tabBar.PrevTab();
        BuildMenu();
        break;

    case ID_TOOLS_OPEN_TERMINAL:
        ShellUtils::LaunchTerminal(activeView->GetCurrentPath());
        break;
    case ID_TOOLS_OPEN_POWERSHELL:
        ShellUtils::LaunchPowerShell(activeView->GetCurrentPath());
        break;
    case ID_TOOLS_OPEN_CMD:
        ShellUtils::LaunchCmd(activeView->GetCurrentPath());
        break;
    case ID_TOOLS_OPEN_VSCODE:
        ShellUtils::LaunchVSCode(activeView->GetCurrentPath());
        break;
    case ID_TOOLS_RELOAD_SCRIPTS:
        ScriptEngine::Get().ReloadScripts();
        break;
    case ID_TOOLS_SCRIPT_CONSOLE:
        ScriptEngine::Get().ShowConsole();
        break;
    case ID_TOOLS_SETTINGS:
        SettingsDialog::Show(m_hwnd, [this]() {
            Config& cfg = Config::Get();
            if (cfg.themeMode == 0) {
                Theme::Get().SetDarkMode(false);
            } else if (cfg.themeMode == 1) {
                Theme::Get().SetDarkMode(true);
            } else {
                Theme::Get().Initialize();
            }
            Theme::Get().ApplyToWindow(m_hwnd);

            m_treeVisible = cfg.showTree;
            ShowWindow(m_tree.GetHwnd(), m_treeVisible ? SW_SHOW : SW_HIDE);

            for (int i = 0; i < 4; ++i) {
                m_multiPane.GetPane(i).SetCalcFolderSize(cfg.calcFolderSize);
            }

            UpdateLanguageUI();
        });
        break;

    case ID_HELP_SHORTCUTS:
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
        break;

    default:
        if (cmdId >= ID_HISTORY_START && cmdId <= ID_HISTORY_END) {
            int hIdx = (int)(cmdId - ID_HISTORY_START);
            if (hIdx >= 0 && hIdx < (int)m_historyPaths.size()) {
                activeView->NavigateTo(m_historyPaths[hIdx]);
            }
        } else if (cmdId >= ID_TAB_ITEM_START && cmdId <= ID_TAB_ITEM_END) {
            int tIdx = (int)(cmdId - ID_TAB_ITEM_START);
            if (tIdx >= 0 && tIdx < m_tabBar.GetTabCount()) {
                m_tabBar.SetActiveTab(tIdx);
                BuildMenu();
            }
        } else if (cmdId >= ID_SCRIPT_COMMAND_START && cmdId <= ID_SCRIPT_COMMAND_END) {
            ScriptEngine::Get().ExecuteCommand(cmdId, activeView->GetCurrentPath(), activeView->GetSelectedPaths());
        }
        break;
    }
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hbr = Theme::Get().BrushBorder();
        if (m_treeVisible) {
            FillRect(hdc, &m_rcTreeSplitter, hbr);
        }
        if (m_previewVisible) {
            FillRect(hdc, &m_rcPreviewSplitter, hbr);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        if ((m_treeVisible && PtInRect(&m_rcTreeSplitter, pt)) || m_resizingTree) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        if ((m_previewVisible && PtInRect(&m_rcPreviewSplitter, pt)) || m_resizingPreview) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (m_treeVisible && PtInRect(&m_rcTreeSplitter, pt)) {
            m_resizingTree = true;
            SetCapture(hwnd);
            return 0;
        }
        if (m_previewVisible && PtInRect(&m_rcPreviewSplitter, pt)) {
            m_resizingPreview = true;
            SetCapture(hwnd);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (m_resizingTree && (wParam & MK_LBUTTON)) {
            m_treeWidth = pt.x;
            LayoutAll();
            return 0;
        }
        if (m_resizingPreview && (wParam & MK_LBUTTON)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            m_previewWidth = rc.right - pt.x;
            LayoutAll();
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (m_resizingTree) {
            m_resizingTree = false;
            ReleaseCapture();
            return 0;
        }
        if (m_resizingPreview) {
            m_resizingPreview = false;
            ReleaseCapture();
            return 0;
        }
        break;
    }

    case WM_SIZE:
        LayoutAll();
        return 0;

    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;

    case WM_DRAWITEM: {
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
    }

    case WM_CONTEXTMENU: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT ptClient = pt;
        ScreenToClient(hwnd, &ptClient);
        int tabH = Theme::Get().Scale(36, hwnd);
        int toolbarH = Theme::Get().Scale(38, hwnd);
        if (ptClient.y >= tabH && ptClient.y <= tabH + toolbarH) {
            ShowToolbarContextMenu(pt);
            return 0;
        }
        break;
    }

    case WM_SETTINGCHANGE: {
        if (lParam && wcscmp((const wchar_t*)lParam, L"ImmersiveColorSet") == 0) {
            Theme::Get().Initialize();
            Theme::Get().ApplyToWindow(m_hwnd);
            InvalidateRect(m_hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        RECT* prc = (RECT*)lParam;
        SetWindowPos(hwnd, nullptr, prc->left, prc->top,
                     prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutAll();
        return 0;
    }

    case WM_CLOSE: {
        Config& cfg = Config::Get();
        RECT rc;
        GetWindowRect(hwnd, &rc);
        cfg.window.x = rc.left;
        cfg.window.y = rc.top;
        cfg.window.width = rc.right - rc.left;
        cfg.window.height = rc.bottom - rc.top;
        cfg.window.maximized = (IsZoomed(hwnd) != FALSE);

        cfg.showTree = m_treeVisible;
        cfg.treeWidth = m_treeWidth;
        cfg.paneLayout = (int)m_multiPane.GetLayout();
        cfg.previewEnabled = m_previewVisible;
        cfg.previewWidth = m_previewWidth;

        cfg.sessionTabs.clear();
        for (int i = 0; i < m_tabBar.GetTabCount(); ++i) {
            cfg.sessionTabs.push_back(m_tabBar.GetTabPath(i));
        }
        cfg.activeTabIndex = m_tabBar.GetActiveIndex();
        cfg.Save();

        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
