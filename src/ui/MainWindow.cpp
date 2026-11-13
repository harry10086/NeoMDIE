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

#define WM_TRAY_NOTIFY (WM_USER + 201)

// Undocumented User32 / UxTheme messages and structures for native menu bar dark mode
#define WM_UAHDRAWMENU         0x0091
#define WM_UAHDRAWMENUITEM     0x0092
#define WM_UAHMEASUREMENUITEM  0x0094

typedef union tagUAHMENUITEMMETRICS {
    struct { DWORD cx; DWORD cy; } rgsizeBar[2];
    struct { DWORD cx; DWORD cy; } rgsizePopup[4];
} UAHMENUITEMMETRICS;

typedef struct tagUAHMENUPOPUPMETRICS {
    DWORD rgcx[4];
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS;

typedef struct tagUAHMENU {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct tagUAHMENUITEM {
    int iPosition;
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

typedef struct tagUAHDRAWMENUITEM {
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM;

typedef struct tagUAHMEASUREMENUITEM {
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHMEASUREMENUITEM;

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

void MainWindow::RestoreFromTrayOrMinimized() {
    bool isHidden = !IsWindowVisible(m_hwnd);
    bool isMin = (IsIconic(m_hwnd) != FALSE);
    if (isHidden || isMin) {
        if (m_wasMaximizedBeforeTray) {
            ShowWindow(m_hwnd, SW_MAXIMIZE);
        } else {
            ShowWindow(m_hwnd, SW_RESTORE);
        }
        if (m_hasTrayIcon) {
            RemoveTrayIcon();
        }
    }
    SetForegroundWindow(m_hwnd);
}

MainWindow::MainWindow() {
}

MainWindow::~MainWindow() {
    if (m_hwnd) UnregisterHotKey(m_hwnd, 9001);
    if (m_hAccel) DestroyAcceleratorTable(m_hAccel);
}

HWND MainWindow::Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialPath) {
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

    // Ensure restored window size is sane (not taking entire screen)
    HMONITOR hPrimaryMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO pmi = { sizeof(pmi) };
    if (GetMonitorInfoW(hPrimaryMon, &pmi)) {
        int workW = pmi.rcWork.right - pmi.rcWork.left;
        int workH = pmi.rcWork.bottom - pmi.rcWork.top;
        if (cfg.window.width >= workW || cfg.window.height >= workH || cfg.window.width <= 200 || cfg.window.height <= 200) {
            cfg.window.width = (workW * 4) / 5;
            cfg.window.height = (workH * 4) / 5;
            cfg.window.x = pmi.rcWork.left + (workW - cfg.window.width) / 2;
            cfg.window.y = pmi.rcWork.top + (workH - cfg.window.height) / 2;
        }
    }

    m_hwnd = CreateWindowExW(
        0, MAINWINDOW_CLASS, L"NeoMDIE",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        cfg.window.x, cfg.window.y, cfg.window.width, cfg.window.height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return nullptr;

    Theme::Get().ApplyToWindow(m_hwnd);
    MARGINS margins = cfg.showMenuBar ? MARGINS{ 0, 0, 0, 0 } : MARGINS{ 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    CreateControls();

    // Initialize scripts before BuildMenu so they are listed in Tools menu
    ScriptEngine::Get().Initialize(m_hwnd);

    BuildMenu();
    RebuildAccelerators();
    AddClipboardFormatListener(m_hwnd);
    if (ShellUtils::IsExplorerReplaced()) {
        RegisterHotKey(m_hwnd, 9001, MOD_WIN, 'E');
    }

// Shell COM initialized cleanly on main UI thread

    std::wstring cmdPath = initialPath;
    if (cmdPath.find(L"645FF040-5081-101B-9F08-00AA002F954E") != std::wstring::npos ||
        cmdPath.find(L"RecycleBinFolder") != std::wstring::npos) {
        ShellExecuteW(nullptr, L"open", L"explorer.exe", L"::{645FF040-5081-101B-9F08-00AA002F954E}", nullptr, SW_SHOWNORMAL);
        cmdPath.clear();
    }
    if (cmdPath.find(L"20D04FE0-3AEA-1069-A2D8-08002B30309D") != std::wstring::npos) {
        cmdPath = L"This PC";
    }
    if (cmdPath.empty() && __argc > 1 && __wargv && __wargv[1]) {
        std::wstring arg = __wargv[1];
        while (!arg.empty() && (arg.back() == L'"' || arg.back() == L' ')) arg.pop_back();
        while (!arg.empty() && arg.front() == L'"') arg.erase(arg.begin());
        if (arg.length() == 2 && arg[1] == L':') arg += L"\\";
        if (ShellUtils::IsValidDirectory(arg) || arg == L"此电脑" || arg == L"This PC") {
            cmdPath = arg;
        }
    }

    if (!cmdPath.empty()) {
        m_tabBar.AddTab(cmdPath, true);
    } else {
        if (cfg.restoreTabsOnStartup && !cfg.sessionTabsInfo.empty()) {
            for (size_t i = 0; i < cfg.sessionTabsInfo.size(); ++i) {
                const auto& info = cfg.sessionTabsInfo[i];
                int tabIdx = m_tabBar.AddTab(info.path, false);
                auto& item = m_tabBar.GetTabItemRef(tabIdx);
                item.layout = info.layout;
                item.activePaneIndex = info.activePaneIndex;
                item.panePaths = info.panePaths;
                item.paneViewStates = info.paneViewStates;
                if (item.panePaths.empty()) {
                    item.panePaths.push_back(item.path.empty() ? L"This PC" : item.path);
                }
            }
        } else if (cfg.restoreTabsOnStartup && !cfg.sessionTabs.empty()) {
            for (const auto& tabPath : cfg.sessionTabs) {
                m_tabBar.AddTab(tabPath, false);
            }
        }
        if (m_tabBar.GetTabCount() == 0) {
            m_tabBar.AddTab(L"This PC", true);
            if (cfg.paneLayout >= 0 && cfg.paneLayout <= 4) {
                SetLayout((PaneLayout)cfg.paneLayout);
            }
        } else {
            int act = std::clamp(cfg.activeTabIndex, 0, m_tabBar.GetTabCount() - 1);
            m_currentTabIndex = -1;
            m_tabBar.SetActiveTab(act, true);
        }
        // Guarantee active pane is never left blank on startup
        FileListView* activePane = m_multiPane.GetActivePane();
        if (activePane && activePane->GetCurrentPath().empty()) {
            std::wstring activePath = m_tabBar.GetTabPath(m_tabBar.GetActiveIndex());
            if (activePath.empty()) activePath = L"This PC";
            activePane->NavigateTo(activePath, false);
        }
    }
    m_currentTabIndex = m_tabBar.GetActiveIndex();
    std::wstring activePath = m_tabBar.GetTabPath(m_tabBar.GetActiveIndex());
    UpdateWindowTitle(activePath);
    FileListView* startupActivePane = m_multiPane.GetActivePane();
    if (startupActivePane) {
        startupActivePane->NotifyStatus();
    }

    if (cfg.window.maximized) {
        ShowWindow(m_hwnd, SW_MAXIMIZE);
    } else {
        ShowWindow(m_hwnd, nCmdShow);
    }
    UpdateWindow(m_hwnd);



    return m_hwnd;
}

void MainWindow::UpdateWindowTitle(const std::wstring& path) {
    std::wstring name = ShellUtils::GetDisplayName(path);
    if (name.empty()) name = path;
    if (name.empty() || _wcsicmp(name.c_str(), L"NeoMDIE") == 0) {
        SetWindowTextW(m_hwnd, L"NeoMDIE");
    } else {
        std::wstring fullTitle = name + L" - NeoMDIE";
        SetWindowTextW(m_hwnd, fullTitle.c_str());
    }
}

void MainWindow::ApplyTheme() {
    Theme::Get().ApplyToWindow(m_hwnd);
    m_multiPane.ApplyTheme();
    for (int p = 0; p < 4; ++p) {
        m_multiPane.GetPane(p).ApplyTheme();
    }
    m_tree.ApplyTheme();
    m_preview.ApplyTheme();
    InvalidateRect(m_tabBar.GetHwnd(), nullptr, TRUE);
    InvalidateRect(m_linksBar.GetHwnd(), nullptr, TRUE);
    InvalidateRect(m_filterBar.GetHwnd(), nullptr, TRUE);
    InvalidateRect(m_breadcrumb.GetHwnd(), nullptr, TRUE);
    InvalidateRect(m_hwnd, nullptr, TRUE);
    DrawMenuBar(m_hwnd);
}

void MainWindow::OnSettingsApplied() {
    Config& cfg = Config::Get();
    if (cfg.themeMode == 0) {
        Theme::Get().SetDarkMode(false);
    } else if (cfg.themeMode == 1) {
        Theme::Get().SetDarkMode(true);
    } else {
        Theme::Get().Initialize();
    }
    ApplyTheme();
    BuildMenu();
    MARGINS margins = cfg.showMenuBar ? MARGINS{ 0, 0, 0, 0 } : MARGINS{ 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    DrawMenuBar(m_hwnd);

    m_treeVisible = cfg.showTree;
    ShowWindow(m_tree.GetHwnd(), m_treeVisible ? SW_SHOW : SW_HIDE);

    m_previewVisible = cfg.previewEnabled;
    ShowWindow(m_preview.GetHwnd(), m_previewVisible ? SW_SHOW : SW_HIDE);
    if (m_previewVisible) {
        FileListView* active = m_multiPane.GetActivePane();
        if (active) {
            auto sel = active->GetSelectedPaths();
            if (!sel.empty()) {
                m_preview.SetFile(sel[0]);
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        m_multiPane.GetPane(i).SetCalcFolderSize(cfg.calcFolderSize);
        m_multiPane.GetPane(i).SetHideDotFiles(cfg.hideDotFiles);
        m_multiPane.GetPane(i).ApplyDensity(cfg.listDensity);
        m_multiPane.GetPane(i).ApplyScale(cfg.listScale);
        m_multiPane.GetPane(i).ApplyItemCheckBoxes();
    }
    m_tree.ApplyViewMetrics();
    m_tree.Refresh();

    Theme::Get().RecreateGdiObjects();
    m_tabBar.ApplyTheme();
    m_multiPane.ApplyTheme();
    m_tree.ApplyTheme();
    InvalidateRect(m_hwnd, nullptr, TRUE);

    RebuildAccelerators();
    if (ShellUtils::IsExplorerReplaced()) {
        RegisterHotKey(m_hwnd, 9001, MOD_WIN, 'E');
    }
    UpdateLanguageUI();
    LayoutAll();
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

    // =========================================================================
    // 1. 文件 (F) File Menu
    // =========================================================================
    HMENU hFileMenu = CreatePopupMenu();

    // 打开新窗口 (N)
    HMENU hNewWinSubmenu = CreatePopupMenu();
    AppendMenuW(hNewWinSubmenu, MF_STRING, ID_FILE_NEW_WINDOW_SPECIFY, isZh ? L"指定文件夹(&T)..." : L"Specify Folder(&T)...");
    AppendMenuW(hNewWinSubmenu, MF_STRING, ID_FILE_NEW_WINDOW_DESKTOP, isZh ? L"桌面(&D)" : L"Desktop(&D)");
    AppendMenuW(hNewWinSubmenu, MF_STRING, ID_FILE_NEW_WINDOW_CURRENT, isZh ? L"当前文件夹(&F)" : L"Current Folder(&F)");
    AppendMenuW(hNewWinSubmenu, MF_STRING, ID_FILE_NEW_WINDOW_CLIPBOARD, isZh ? L"剪贴板(&C)" : L"Clipboard(&C)");
    AppendMenuW(hFileMenu, MF_POPUP, (UINT_PTR)hNewWinSubmenu, isZh ? L"打开新窗口(&N)" : L"New Window(&N)");

    // 最近关闭的文件夹 (M)
    HMENU hRecentClosedSubmenu = CreatePopupMenu();
    AppendMenuW(hRecentClosedSubmenu, MF_STRING, ID_FILE_REOPEN_CLOSED_TAB,
                isZh ? L"打开最后关闭的文件夹(&L)\tCtrl+Shift+T" : L"Reopen Last Closed Tab(&L)\tCtrl+Shift+T");
    AppendMenuW(hRecentClosedSubmenu, MF_SEPARATOR, 0, nullptr);
    const auto& closedHist = m_tabBar.GetClosedHistory();
    if (closedHist.empty()) {
        AppendMenuW(hRecentClosedSubmenu, MF_STRING | MF_GRAYED, 0, isZh ? L"(无记录)" : L"(Empty)");
    } else {
        int count = std::min((int)closedHist.size(), 10);
        for (int i = 0; i < count; ++i) {
            UINT cmd = ID_RECENT_CLOSED_START + i;
            const std::wstring& p = closedHist[closedHist.size() - 1 - i];
            std::wstring label = std::to_wstring(i) + L" " + p;
            AppendMenuW(hRecentClosedSubmenu, MF_STRING, cmd, label.c_str());
        }
    }
    AppendMenuW(hFileMenu, MF_POPUP, (UINT_PTR)hRecentClosedSubmenu, isZh ? L"最近关闭的文件夹(&M)" : L"Recently Closed Folders(&M)");

    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_NEW_TAB, isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CLOSE_TAB, isZh ? L"关闭当前标签(&C)\tCtrl+W" : L"Close Tab(&C)\tCtrl+W");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hFileMenu, MF_STRING, ID_EDIT_DELETE, isZh ? L"删除(&D)\tDelete" : L"Delete(&D)\tDelete");
    AppendMenuW(hFileMenu, MF_STRING, ID_EDIT_RENAME, isZh ? L"重命名(&R)\tF2" : L"Rename(&R)\tF2");

    HMENU hNewSubmenu = CreatePopupMenu();
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FOLDER, isZh ? L"文件夹(&F)\tCtrl+Shift+N" : L"Folder(&F)\tCtrl+Shift+N");
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FILE, isZh ? L"TXT文档(&T)" : L"TXT Document(&T)");
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_MD, isZh ? L"Markdown 文档(&M)" : L"Markdown Document(&M)");
    AppendMenuW(hFileMenu, MF_POPUP, (UINT_PTR)hNewSubmenu, isZh ? L"新建(&W)" : L"New(&W)");

    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CREATE_SHORTCUT, isZh ? L"创建快捷方式(&S)" : L"Create Shortcut(&S)");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_OPEN_EXPLORER, isZh ? L"用系统资源管理器打开(&O)" : L"Open in Explorer(&O)");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_PROPERTIES, isZh ? L"属性(&P)\tAlt+Enter" : L"Properties(&P)\tAlt+Enter");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_EXIT, isZh ? L"退出(&X)\tAlt+F4" : L"Exit(&X)\tAlt+F4");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, isZh ? L"文件(&F)" : L"File(&F)");

    // =========================================================================
    // 2. 编辑 (E) Edit Menu
    // =========================================================================
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_UNDO, isZh ? L"撤销(&U)\tCtrl+Z" : L"Undo(&U)\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_CUT, isZh ? L"剪切(&T)\tCtrl+X" : L"Cut(&T)\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_COPY, isZh ? L"复制(&C)\tCtrl+C" : L"Copy(&C)\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE, isZh ? L"粘贴(&P)\tCtrl+V" : L"Paste(&P)\tCtrl+V");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE_SHORTCUT, isZh ? L"粘贴快捷方式(&S)" : L"Paste Shortcut(&S)");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_SELECT_ALL, isZh ? L"全选(&A)\tCtrl+A" : L"Select All(&A)\tCtrl+A");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_INVERT_SELECTION, isZh ? L"反向选择(&I)\tCtrl+I" : L"Invert Selection(&I)\tCtrl+I");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hCopyPathSubmenu = CreatePopupMenu();
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_PATH, isZh ? L"复制完整路径(&P)\tCtrl+Shift+C" : L"Copy Full Path(&P)\tCtrl+Shift+C");
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_NAME, isZh ? L"复制文件名(&N)" : L"Copy File Name(&N)");
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_DIR, isZh ? L"复制所在目录(&D)" : L"Copy Directory Path(&D)");
    AppendMenuW(hEditMenu, MF_POPUP, (UINT_PTR)hCopyPathSubmenu, isZh ? L"复制路径(&Y)" : L"Copy Path(&Y)");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, isZh ? L"编辑(&E)" : L"Edit(&E)");

    // =========================================================================
    // 3. 查看 (V) View Menu
    // =========================================================================
    HMENU hViewMenu = CreatePopupMenu();

    // 工具栏 (T)
    HMENU hBarsSubmenu = CreatePopupMenu();
    AppendMenuW(hBarsSubmenu, MF_STRING | (Config::Get().showToolbarStandard ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TB_STANDARD, isZh ? L"标准按钮(&S)" : L"Standard Buttons(&S)");
    AppendMenuW(hBarsSubmenu, MF_STRING | (Config::Get().showToolbarAddress ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TB_ADDRESS, isZh ? L"地址栏(&A)" : L"Address Bar(&A)");
    AppendMenuW(hBarsSubmenu, MF_STRING | (m_linksVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TOGGLE_LINKS, isZh ? L"链接栏(&L)" : L"Links Bar(&L)");
    AppendMenuW(hBarsSubmenu, MF_STRING | (Config::Get().showToolbarSearch ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TB_SEARCH, isZh ? L"搜索栏(&F)" : L"Search Bar(&F)");
    AppendMenuW(hBarsSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hBarsSubmenu, MF_STRING | (Config::Get().lockToolbars ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LOCK_TOOLBARS, isZh ? L"锁定工具栏" : L"Lock Toolbars");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hBarsSubmenu, isZh ? L"工具栏(&T)" : L"Toolbars(&T)");

    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().showStatusBar ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_STATUSBAR, isZh ? L"状态栏(&B)" : L"Status Bar(&B)");

    // 侧边栏 (E)
    HMENU hSideSubmenu = CreatePopupMenu();
    AppendMenuW(hSideSubmenu, MF_STRING | (m_treeVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TREE_TOGGLE, isZh ? L"文件夹目录树(&O)\tF8" : L"Folders Tree(&O)\tF8");
    AppendMenuW(hSideSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSideSubmenu, MF_STRING | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_PREVIEW_TOGGLE, isZh ? L"快速预览窗格(&P)\tAlt+P" : L"Preview Pane(&P)\tAlt+P");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hSideSubmenu, isZh ? L"侧边栏(&E)" : L"Sidebar(&E)");

    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);

    // 视图样式 (V)
    HMENU hViewStyleSubmenu = CreatePopupMenu();
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_XLARGE, isZh ? L"超大图标(&X)" : L"Extra large icons(&X)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_LARGE, isZh ? L"大图标(&R)" : L"Large icons(&R)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_MEDIUM, isZh ? L"中等图标(&M)" : L"Medium icons(&M)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_ICONS_SMALL, isZh ? L"小图标(&N)" : L"Small icons(&N)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_LIST, isZh ? L"列表(&L)" : L"List(&L)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_DETAILS, isZh ? L"详细信息(&D)" : L"Details(&D)");
    AppendMenuW(hViewStyleSubmenu, MF_STRING, ID_VIEW_TILES, isZh ? L"平铺(&S)" : L"Tiles(&S)");
    int curVm = activeView ? activeView->GetViewMode() : Config::Get().viewMode;
    UINT activeVmId = ID_VIEW_DETAILS;
    if (curVm == 0) activeVmId = ID_VIEW_DETAILS;
    else if (curVm == 1) activeVmId = ID_VIEW_ICONS_LARGE;
    else if (curVm == 2) activeVmId = ID_VIEW_ICONS_SMALL;
    else if (curVm == 3) activeVmId = ID_VIEW_LIST;
    else if (curVm == 4) activeVmId = ID_VIEW_TILES;
    CheckMenuRadioItem(hViewStyleSubmenu, ID_VIEW_DETAILS, ID_VIEW_ICONS_MEDIUM, activeVmId, MF_BYCOMMAND);
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hViewStyleSubmenu, isZh ? L"视图样式(&V)" : L"View Mode(&V)");

    // 排列图标 (I)
    HMENU hSortSubmenu = CreatePopupMenu();
    SortColumn curSortCol = activeView ? activeView->GetSortColumn() : SortColumn::Name;
    bool isAsc = activeView ? activeView->IsSortAscending() : true;
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::Name ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_NAME, isZh ? L"名称" : L"Name");
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::Size ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_SIZE, isZh ? L"大小" : L"Size");
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::Type ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_TYPE, isZh ? L"类型" : L"Type");
    AppendMenuW(hSortSubmenu, MF_STRING | (curSortCol == SortColumn::DateModified ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_DATE, isZh ? L"修改日期" : L"Date Modified");
    AppendMenuW(hSortSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSortSubmenu, MF_STRING | (isAsc ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_ASC, isZh ? L"递增排序(&A)" : L"Ascending(&A)");
    AppendMenuW(hSortSubmenu, MF_STRING | (!isAsc ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_DESC, isZh ? L"递减排序(&D)" : L"Descending(&D)");
    AppendMenuW(hSortSubmenu, MF_SEPARATOR, 0, nullptr);
    bool foldersFirst = activeView ? activeView->IsFoldersFirst() : Config::Get().foldersFirst;
    AppendMenuW(hSortSubmenu, MF_STRING | (foldersFirst ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_FOLDERS_FIRST, isZh ? L"文件夹在前(&F)" : L"Folders First(&F)");
    AppendMenuW(hSortSubmenu, MF_STRING | (!foldersFirst ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SORT_FILES_FIRST, isZh ? L"文件在前(&L)" : L"Files First(&L)");
    CheckMenuRadioItem(hSortSubmenu, ID_VIEW_SORT_FOLDERS_FIRST, ID_VIEW_SORT_FILES_FIRST,
                       foldersFirst ? ID_VIEW_SORT_FOLDERS_FIRST : ID_VIEW_SORT_FILES_FIRST, MF_BYCOMMAND);
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hSortSubmenu, isZh ? L"排列图标(&I)" : L"Sort By(&I)");

    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);

    // 转到 (M)
    HMENU hGoSubmenu = CreatePopupMenu();
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_BACK, isZh ? L"后退(&B)\tAlt+Left" : L"Back(&B)\tAlt+Left");
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_FORWARD, isZh ? L"前进(&F)\tAlt+Right" : L"Forward(&F)\tAlt+Right");
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_UP, isZh ? L"向上一级(&U)\tAlt+Up" : L"Up(&U)\tAlt+Up");
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_UP_NEW_TAB, isZh ? L"在新标签页中打开上一级文件夹(&N)" : L"Open Up in New Tab(&N)");
    AppendMenuW(hGoSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_HOME, isZh ? L"转到地址栏(&D)\tAlt+D" : L"Go to Address Bar(&D)\tAlt+D");
    HMENU hHistSubmenu = CreatePopupMenu();
    if (m_historyPaths.empty()) {
        AppendMenuW(hHistSubmenu, MF_STRING | MF_GRAYED, 0, isZh ? L"(无历史记录)" : L"(No History)");
    } else {
        for (size_t i = 0; i < m_historyPaths.size(); ++i) {
            UINT histCmdId = ID_HISTORY_START + (UINT)i;
            AppendMenuW(hHistSubmenu, MF_STRING, histCmdId, m_historyPaths[i].c_str());
        }
    }
    AppendMenuW(hGoSubmenu, MF_POPUP, (UINT_PTR)hHistSubmenu, isZh ? L"历史记录(&H)" : L"History(&H)");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hGoSubmenu, isZh ? L"转到(&M)" : L"Go(&M)");

    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().showHidden ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SHOW_HIDDEN, isZh ? L"显示隐藏文件(&H)" : L"Show Hidden Files(&H)");
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().hideDotFiles ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_HIDE_DOTFILES, isZh ? L"隐藏点文件(.开头文件夹)(&F)\tCtrl+H" : L"Hide Dotfiles(.xxx)(&F)\tCtrl+H");
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().showItemCheckBoxes ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_ITEM_CHECKBOXES, isZh ? L"项目复选框(&X)" : L"Item check boxes(&X)");

    HMENU hDensitySubmenu = CreatePopupMenu();
    AppendMenuW(hDensitySubmenu, MF_STRING | (Config::Get().listDensity == 0 ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_DENSITY_COMPACT, isZh ? L"紧凑 (Compact)" : L"Compact");
    AppendMenuW(hDensitySubmenu, MF_STRING | (Config::Get().listDensity == 1 ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_DENSITY_COMFORTABLE, isZh ? L"适中 (Normal)" : L"Normal");
    AppendMenuW(hDensitySubmenu, MF_STRING | (Config::Get().listDensity == 2 ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_DENSITY_SPACIOUS, isZh ? L"宽松 (Relaxed)" : L"Relaxed");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hDensitySubmenu, isZh ? L"间距调整(&D)" : L"Spacing Density(&D)");

    HMENU hScaleSubmenu = CreatePopupMenu();
    AppendMenuW(hScaleSubmenu, MF_STRING | (Config::Get().listScale == 0 ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SCALE_SMALL, isZh ? L"偏小 (90% / 9pt)" : L"Small (90% / 9pt)");
    AppendMenuW(hScaleSubmenu, MF_STRING | (Config::Get().listScale == 1 ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SCALE_MEDIUM, isZh ? L"标准 (100% / 10pt)" : L"Standard (100% / 10pt)");
    AppendMenuW(hScaleSubmenu, MF_STRING | (Config::Get().listScale == 2 ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SCALE_LARGE, isZh ? L"放大 (125% / 12pt)" : L"Large (125% / 12pt)");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hScaleSubmenu, isZh ? L"放大调整(&Z)" : L"Scale & Zoom(&Z)");
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().calcFolderSize ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_CALC_FOLDER_SIZE, isZh ? L"计算文件夹大小(&C)" : L"Calculate Folder Sizes(&C)");
        // 滚动 (X)
    HMENU hScrollSubmenu = CreatePopupMenu();
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_TOP, isZh ? L"列表顶部(&T)\tHome" : L"List Top(&T)\tHome");
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_BOTTOM, isZh ? L"列表底部(&B)\tEnd" : L"List Bottom(&B)\tEnd");
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_PAGEUP, isZh ? L"向上翻页(&U)\tPage Up" : L"Page Up(&U)\tPage Up");
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_PAGEDOWN, isZh ? L"向下翻页(&D)\tPage Down" : L"Page Down(&D)\tPage Down");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hScrollSubmenu, isZh ? L"滚动(&X)" : L"Scroll(&X)");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_REFRESH, isZh ? L"刷新(&R)\tF5" : L"Refresh(&R)\tF5");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING | (Theme::Get().IsDarkMode() ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_DARK_MODE, isZh ? L"深色主题模式(&D)" : L"Dark Theme Mode(&D)");
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().showMenuBar ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TOGGLE_MENUBAR, isZh ? L"显示传统明文菜单栏(&M)" : L"Show Classic Menu Bar(&M)");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, isZh ? L"查看(&V)" : L"View(&V)");

    // =========================================================================
    // 4. 工具 (T) Tools Menu
    // =========================================================================
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, isZh ? L"打开命令行 CMD(&C)" : L"Open Command Prompt CMD(&C)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    HMENU hScriptSub = CreatePopupMenu();
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
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hScriptSub, isZh ? L"脚本扩展(&E)" : L"Scripts(&E)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_FOLDER_OPTIONS, isZh ? L"系统文件夹选项(&O)..." : L"Folder Options(&O)...");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"NeoMDIE 设置(&S)..." : L"NeoMDIE Settings(&S)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu, isZh ? L"工具(&T)" : L"Tools(&T)");

    // =========================================================================
    // 6. 窗口 (W) Window Menu
    // =========================================================================
    HMENU hWindowMenu = CreatePopupMenu();

    // 标签页操作 (T)
    HMENU hTabOpsSubmenu = CreatePopupMenu();
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_TAB_NEXT, isZh ? L"下一个标签(&N)\tCtrl+Tab" : L"Next Tab(&N)\tCtrl+Tab");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_TAB_PREV, isZh ? L"上一个标签(&B)\tCtrl+Shift+Tab" : L"Previous Tab(&B)\tCtrl+Shift+Tab");
    AppendMenuW(hTabOpsSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_TAB, isZh ? L"关闭当前标签(&C)\tCtrl+W" : L"Close Current Tab(&C)\tCtrl+W");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_LEFT_TABS, isZh ? L"关闭左侧标签(&L)" : L"Close Left Tabs(&L)");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_RIGHT_TABS, isZh ? L"关闭右侧标签(&R)" : L"Close Right Tabs(&R)");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_OTHER_TABS, isZh ? L"除当前标签外全部关闭(&O)" : L"Close Other Tabs(&O)");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_ALL_TABS, isZh ? L"关闭全部标签(&A)" : L"Close All Tabs(&A)");
    AppendMenuW(hTabOpsSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_DUPLICATE_TAB, isZh ? L"复制当前标签(&D)" : L"Duplicate Tab(&D)");
    bool isPinned = m_tabBar.IsTabPinned(m_tabBar.GetActiveIndex());
    AppendMenuW(hTabOpsSubmenu, MF_STRING | (isPinned ? MF_CHECKED : MF_UNCHECKED),
                ID_FILE_PIN_TAB, isPinned ? (isZh ? L"取消固定标签(&P)" : L"Unpin Tab(&P)") : (isZh ? L"固定当前标签(&P)" : L"Pin Tab(&P)"));
    AppendMenuW(hWindowMenu, MF_POPUP, (UINT_PTR)hTabOpsSubmenu, isZh ? L"标签页操作(&T)" : L"Tab Operations(&T)");

    // 窗口操作 (W)
    HMENU hWinOpsSubmenu = CreatePopupMenu();
    AppendMenuW(hWinOpsSubmenu, MF_STRING, ID_WINDOW_MAXIMIZE, isZh ? L"当前窗格最大化(&M)" : L"Maximize Pane(&M)");
    AppendMenuW(hWinOpsSubmenu, MF_STRING, ID_WINDOW_CLOSE, isZh ? L"关闭当前窗格(&C)" : L"Close Pane(&C)");
    AppendMenuW(hWinOpsSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hWinOpsSubmenu, MF_STRING, ID_WINDOW_MAIN_MAXIMIZE, isZh ? L"主窗口最大化(&A)" : L"Maximize Main Window(&A)");
    AppendMenuW(hWinOpsSubmenu, MF_STRING, ID_WINDOW_MAIN_MINIMIZE, isZh ? L"主窗口最小化(&N)" : L"Minimize Main Window(&N)");
    AppendMenuW(hWindowMenu, MF_POPUP, (UINT_PTR)hWinOpsSubmenu, isZh ? L"窗口操作(&W)" : L"Window Operations(&W)");

    // 窗口模式 (M)
    HMENU hWinModeSubmenu = CreatePopupMenu();
    PaneLayout curLayout = m_multiPane.GetLayout();
    AppendMenuW(hWinModeSubmenu, MF_STRING | (curLayout == PaneLayout::Single ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_1X1, isZh ? L"普通/单屏(&N)" : L"Single Pane(&N)");
    AppendMenuW(hWinModeSubmenu, MF_STRING | (curLayout == PaneLayout::DualVertical ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_1X2, isZh ? L"垂直两个页面(&V)" : L"Dual Vertical(&V)");
    AppendMenuW(hWinModeSubmenu, MF_STRING | (curLayout == PaneLayout::DualHorizontal ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_2X1, isZh ? L"水平两个页面(&H)" : L"Dual Horizontal(&H)");
    AppendMenuW(hWinModeSubmenu, MF_STRING | (curLayout == PaneLayout::Quad ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_2X2, isZh ? L"四栏(&Q)" : L"Quad 2x2(&Q)");
    AppendMenuW(hWinModeSubmenu, MF_STRING | (curLayout == PaneLayout::ThreeLeft ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_LAYOUT_1P2, isZh ? L"左一右二(&T)" : L"Three Panes (1+2)(&T)");
    AppendMenuW(hWinModeSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hWinModeSubmenu, MF_STRING | (Config::Get().syncNav ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SYNC_NAV, isZh ? L"多窗格同步浏览(&S)" : L"Sync Navigation(&S)");
    AppendMenuW(hWindowMenu, MF_POPUP, (UINT_PTR)hWinModeSubmenu, isZh ? L"窗口模式(&M)" : L"Window Mode(&M)");

    AppendMenuW(hWindowMenu, MF_SEPARATOR, 0, nullptr);
    const auto& allTabs = m_tabBar.GetTabs();
    int activeIdx = m_tabBar.GetActiveIndex();
    for (size_t i = 0; i < allTabs.size(); ++i) {
        UINT tabCmdId = ID_TAB_ITEM_START + (UINT)i;
        std::wstring title = allTabs[i].title.empty() ? allTabs[i].path : allTabs[i].title;
        std::wstring itemLabel = std::to_wstring(i + 1) + L" " + title;
        AppendMenuW(hWindowMenu, MF_STRING | (i == (size_t)activeIdx ? MF_CHECKED : MF_UNCHECKED), tabCmdId, itemLabel.c_str());
    }
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hWindowMenu, isZh ? L"窗口(&W)" : L"Window(&W)");

    // =========================================================================
    // 7. 帮助 (H) Help Menu
    // =========================================================================
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GITHUB, isZh ? L"NeoMDIE 开源仓库(&G)" : L"NeoMDIE GitHub Repository(&G)");
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_CHECK_UPDATES, isZh ? L"检查更新(&U)..." : L"Check for Updates(&U)...");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_ABOUT, isZh ? L"关于 NeoMDIE(&A)..." : L"About NeoMDIE(&A)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, isZh ? L"帮助(&H)" : L"Help(&H)");

    SetMenu(m_hwnd, Config::Get().showMenuBar ? hMenuBar : nullptr);
    DrawMenuBar(m_hwnd);
    if (m_hMenuBar && m_hMenuBar != hMenuBar) {
        DestroyMenu(m_hMenuBar);
    }
    m_hMenuBar = hMenuBar;
    if (hOldMenu && hOldMenu != m_hMenuBar) {
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
    m_hwndBtnTree    = createBtn(L"\uE8A7", IDC_BTN_TREE);
    m_hwndBtnSplit   = createBtn(L"\uE740", IDC_BTN_SPLIT);
    m_hwndBtnPreview = createBtn(L"\uE896", IDC_BTN_PREVIEW);
    m_hwndBtnMenu    = createBtn(L"\uE700", IDC_BTN_MENU);

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
    addTip(m_hwndBtnTree, isZh ? L"文件夹目录树 (F8)" : L"Directory Tree (F8)");
    addTip(m_hwndBtnSplit, isZh ? L"多栏布局 (F4)" : L"Split View (F4)");
    addTip(m_hwndBtnPreview, isZh ? L"快速预览 (Alt+P)" : L"Preview Pane (Alt+P)");
    addTip(m_hwndBtnMenu, isZh ? L"主菜单 (≡ / Alt+M)" : L"Main Menu (≡ / Alt+M)");

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
    m_splitOverlay.Create(m_hwnd);

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

    int parts[] = { -1 };
    SendMessageW(m_hwndStatusBar, SB_SETPARTS, 1, (LPARAM)parts);

    // Wire up links bar callbacks
    m_linksBar.onNavigate = [this](const std::wstring& path) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) activeView->NavigateTo(path);
    };
    m_linksBar.onOpenNewTab = [this](const std::wstring& path) {
        m_tabBar.AddTab(path, true);
    };
    m_linksBar.onOpenBgTab = [this](const std::wstring& path) {
        m_tabBar.AddTab(path, false);
    };
    m_linksBar.onOpenSplit = [this](const std::wstring& path) {
        SetLayout(PaneLayout::DualVertical);
        m_multiPane.GetPane(1).NavigateTo(path);
        m_multiPane.SetActivePane(1);
    };

    // Wire up preview close
    m_preview.onCloseRequested = [this]() {
        TogglePreview();
    };

    // Wire up tab bar callbacks
    m_tabBar.onTabChanged = [this](int idx) {
        if (idx < 0 || idx >= m_tabBar.GetTabCount()) return;

        if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabBar.GetTabCount() && m_currentTabIndex != idx) {
            auto& oldTab = m_tabBar.GetTabItemRef(m_currentTabIndex);
            oldTab.layout = (int)m_multiPane.GetLayout();
            oldTab.activePaneIndex = m_multiPane.GetActiveIndex();
            oldTab.panePaths.clear();
            oldTab.paneHistories.clear();
            oldTab.paneSelectedPaths.clear();
            oldTab.paneViewStates.clear();
            int pCount = m_multiPane.GetPaneCount();
            for (int p = 0; p < pCount; ++p) {
                std::wstring pPath = m_multiPane.GetPane(p).GetCurrentPath();
                if (pPath.empty()) pPath = (p == 0 && !oldTab.path.empty()) ? oldTab.path : L"This PC";
                oldTab.panePaths.push_back(pPath);
                PaneHistory ph;
                ph.history = m_multiPane.GetPane(p).GetHistory();
                ph.historyIndex = m_multiPane.GetPane(p).GetHistoryIndex();
                oldTab.paneHistories.push_back(ph);
                oldTab.paneSelectedPaths.push_back(m_multiPane.GetPane(p).GetSelectedPaths());
                oldTab.paneViewStates.push_back(m_multiPane.GetPane(p).GetViewState());
            }
            if (!oldTab.panePaths.empty()) {
                int aIdx = std::clamp(oldTab.activePaneIndex, 0, (int)oldTab.panePaths.size() - 1);
                oldTab.path = oldTab.panePaths[aIdx];
            }
        }

        m_currentTabIndex = idx;

        const auto& itemRef = m_tabBar.GetTabItem(idx);
        int targetLayout = itemRef.layout;
        int targetActivePane = itemRef.activePaneIndex;
        std::vector<std::wstring> targetPanePaths = itemRef.panePaths;
        std::vector<PaneHistory> targetHistories = itemRef.paneHistories;
        std::vector<std::vector<std::wstring>> targetSelections = itemRef.paneSelectedPaths;
        std::vector<PaneViewState> targetViewStates = itemRef.paneViewStates;
        std::wstring fallbackPath = itemRef.path.empty() ? L"This PC" : itemRef.path;

        m_isSwitchingTab = true;

        m_multiPane.SetLayout((PaneLayout)targetLayout);
        int targetPaneCount = m_multiPane.GetPaneCount();

        if (targetPanePaths.empty()) {
            m_multiPane.GetPane(0).NavigateTo(fallbackPath, false);
            m_multiPane.GetPane(0).ClearHistory(fallbackPath);
            m_multiPane.GetPane(0).ClearSelection();
        } else {
            for (int p = 0; p < targetPaneCount && p < 4; ++p) {
                std::wstring targetPath = (p < (int)targetPanePaths.size() && !targetPanePaths[p].empty())
                                          ? targetPanePaths[p] : fallbackPath;
                if (_wcsicmp(m_multiPane.GetPane(p).GetCurrentPath().c_str(), targetPath.c_str()) != 0 ||
                    m_multiPane.GetPane(p).GetCurrentPath().empty()) {
                    m_multiPane.GetPane(p).NavigateTo(targetPath, false);
                }
                if (p < (int)targetHistories.size() && !targetHistories[p].history.empty()) {
                    m_multiPane.GetPane(p).SetHistory(targetHistories[p].history, targetHistories[p].historyIndex);
                } else {
                    m_multiPane.GetPane(p).ClearHistory(targetPath);
                }
            }
        }

        int safeActivePane = std::clamp(targetActivePane, 0, targetPaneCount - 1);
        m_multiPane.SetActivePane(safeActivePane, true);

        FileListView* activeView = m_multiPane.GetActivePane();
        std::wstring activePath = activeView ? activeView->GetCurrentPath() : fallbackPath;

        m_tabBar.SetTabPath(idx, activePath);
        m_breadcrumb.SetPath(activePath);
        m_filterBar.SetText(activeView ? activeView->GetFilterQuery() : L"");
        if (m_treeVisible && Config::Get().autoExpandTree) {
            m_pendingTreePath = activePath;
            PostMessageW(m_hwnd, WM_SYNC_TREE_SELECTION, 0, 0);
        }
        UpdateNavButtons();
        AddToHistory(activePath);
        UpdateWindowTitle(activePath);
        LayoutAll();

        // Restore view states for all panes after layout and sizing are complete
        for (int p = 0; p < targetPaneCount && p < 4; ++p) {
            if (p < (int)targetViewStates.size()) {
                m_multiPane.GetPane(p).RestoreViewState(targetViewStates[p]);
            } else if (p < (int)targetSelections.size() && !targetSelections[p].empty()) {
                m_multiPane.GetPane(p).SetSelectedPaths(targetSelections[p]);
            } else {
                m_multiPane.GetPane(p).ClearSelection();
            }
        }

        m_isSwitchingTab = false;
        BuildMenu();

        if (activeView) {
            activeView->NotifyStatus();
        }

        SetFocus(m_multiPane.GetPane(safeActivePane).GetHwnd());
        if (m_previewVisible && activeView) {
            auto sel = activeView->GetSelectedPaths();
            if (!sel.empty()) {
                m_preview.SetFile(sel[0]);
            } else {
                m_preview.Clear();
            }
        }
    };

    m_tabBar.onTabClosed = [this](int idx) {
        if (m_currentTabIndex == idx) {
            m_currentTabIndex = -1;
        } else if (m_currentTabIndex > idx) {
            m_currentTabIndex--;
        }
        int act = m_tabBar.GetActiveIndex();
        if (act >= 0) {
            UpdateWindowTitle(m_tabBar.GetTabPath(act));
        }
    };

    m_tabBar.onTabOrderChanged = [this]() {
        m_currentTabIndex = m_tabBar.GetActiveIndex();
    };

    m_tabBar.onNewTabRequested = [this]() {
        FileListView* activeView = m_multiPane.GetActivePane();
        std::wstring cur = activeView ? activeView->GetCurrentPath() : L"This PC";
        m_tabBar.AddTab(cur, true);
    };

    m_tabBar.onOpenTerminalRequested = [this](const std::wstring& path) {
        ShellUtils::LaunchTerminal(path);
    };

    m_tabBar.onTabDragMove = [this](int dragIndex, int hostTabIndex, POINT ptScreen) {
        HandleTabDragMove(dragIndex, hostTabIndex, ptScreen);
    };

    m_tabBar.onTabDragEnd = [this](int dragIndex, int hostTabIndex, POINT ptScreen) {
        HandleTabDragEnd(dragIndex, hostTabIndex, ptScreen);
    };

    m_multiPane.onPaneDragMove = [this](int paneIdx, POINT ptScreen) {
        HandlePaneDragMove(paneIdx, ptScreen);
    };

    m_multiPane.onPaneDragEnd = [this](int paneIdx, POINT ptScreen) {
        HandlePaneDragEnd(paneIdx, ptScreen);
    };

    // Window Action: Breadcrumb / Address Bar
    m_breadcrumb.onPathNavigated = [this](const std::wstring& path) {
        ActionTrigger trigger = GetCurrentTrigger();
        WindowAction act = Config::Get().actionAddress.ResolveAction(trigger);
        if (act == WindowAction::NewTab) {
            m_tabBar.AddTab(path, true);
        } else if (act == WindowAction::BgTab) {
            m_tabBar.AddTab(path, false);
        } else if (act == WindowAction::OtherPane && m_multiPane.GetPaneCount() > 1) {
            int otherIdx = (m_multiPane.GetActiveIndex() + 1) % m_multiPane.GetPaneCount();
            m_multiPane.GetPane(otherIdx).NavigateTo(path);
            m_multiPane.SetActivePane(otherIdx);
        } else if (act == WindowAction::Explorer) {
            ShellExecuteW(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (act == WindowAction::CurrentPane) {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->NavigateTo(path);
        }
    };

    // Window Action: Tree
    m_tree.onPathSelected = [this](const std::wstring& path) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) activeView->NavigateTo(path);
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

    // MultiPane layout change callback
    m_multiPane.onLayoutChanged = [this](PaneLayout newLayout) {
        if (m_isSwitchingTab) return;

        int curTab = m_tabBar.GetActiveIndex();
        if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
            auto& tab = m_tabBar.GetTabItemRef(curTab);
            tab.layout = (int)newLayout;
            int pCount = m_multiPane.GetPaneCount();
            tab.panePaths.resize(pCount);
            for (int p = 0; p < pCount; ++p) {
                tab.panePaths[p] = m_multiPane.GetPane(p).GetCurrentPath();
            }
            tab.activePaneIndex = m_multiPane.GetActiveIndex();
        }
        BuildMenu();
        LayoutAll();
        InvalidateRect(m_hwnd, nullptr, TRUE);
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
            m_tabBar.TogglePinTab(actIdx);
            break;
        }
        case GestureAction::ReopenClosedTab: OnCommand(ID_FILE_REOPEN_CLOSED_TAB); break;
        case GestureAction::NextTab: m_tabBar.NextTab(); break;
        case GestureAction::PrevTab: m_tabBar.PrevTab(); break;
        case GestureAction::CloseOtherTabs: OnCommand(ID_FILE_CLOSE_OTHER_TABS); break;
        case GestureAction::LayoutSingle: SetLayout(PaneLayout::Single); break;
        case GestureAction::Layout2H:     SetLayout(PaneLayout::DualHorizontal); break;
        case GestureAction::Layout2V:     SetLayout(PaneLayout::DualVertical); break;
        case GestureAction::Layout4Grid:  SetLayout(PaneLayout::Quad); break;
        case GestureAction::CopyPath:     if (activeView) ShellUtils::CopyToClipboard(activeView->GetCurrentPath()); break;
        case GestureAction::Delete:       OnCommand(ID_EDIT_DELETE); break;
        case GestureAction::Rename:       OnCommand(ID_EDIT_RENAME); break;
        case GestureAction::NewMD:        OnCommand(ID_FILE_NEW_MD); break;
        case GestureAction::ToggleHidden: OnCommand(ID_VIEW_SHOW_HIDDEN); break;
        case GestureAction::OpenSettings: OnCommand(ID_TOOLS_SETTINGS); break;
        case GestureAction::InvertSelection: OnCommand(ID_EDIT_INVERT_SELECTION); break;
        case GestureAction::Undo:            OnCommand(ID_EDIT_UNDO); break;
        case GestureAction::ToggleCheckBoxes:    OnCommand(ID_VIEW_ITEM_CHECKBOXES); break;
        case GestureAction::ToggleDotFiles:      OnCommand(ID_VIEW_HIDE_DOTFILES); break;
        case GestureAction::ToggleCalcFolderSize: OnCommand(ID_VIEW_CALC_FOLDER_SIZE); break;
        case GestureAction::FocusAddress:        OnCommand(ID_VIEW_ADDRESS_FOCUS); break;
        case GestureAction::FocusFilter:         OnCommand(ID_VIEW_FILTER_FOCUS); break;
        case GestureAction::ToggleDarkMode:      OnCommand(ID_VIEW_DARK_MODE); break;
        case GestureAction::ToggleMenuBar:       OnCommand(ID_VIEW_TOGGLE_MENUBAR); break;
        default: {
            int intAct = static_cast<int>(act);
            if (intAct >= ID_SCRIPT_COMMAND_START && intAct <= ID_SCRIPT_COMMAND_END) {
                OnCommand((UINT)intAct);
            }
            break;
        }
        }
    };

    auto handleCustomCommand = [this](int cmd, const std::wstring& path) {
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB) {
            m_tabBar.AddTab(path, true);
        } else if (cmd == CMD_NEOMDIE_OPEN_BG_TAB) {
            m_tabBar.AddTab(path, false);
        } else if (cmd == CMD_NEOMDIE_OPEN_SPLIT) {
            int count = m_multiPane.GetPaneCount();
            if (count == 1) {
                SetLayout(PaneLayout::DualVertical, path);
                m_multiPane.SetActivePane(1);
            } else if (count == 2) {
                SetLayout(PaneLayout::ThreeLeft, path);
                m_multiPane.SetActivePane(2);
            } else if (count == 3) {
                SetLayout(PaneLayout::Quad, path);
                m_multiPane.SetActivePane(3);
            } else {
                auto* active = m_multiPane.GetActivePane();
                if (active) active->NavigateTo(path);
            }
        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        } else if (cmd == CMD_NEOMDIE_PIN_QUICKACCESS) {
            if (Config::Get().AddQuickAccess(path)) {
                m_tree.RefreshQuickAccess();
            }
        } else if (cmd == CMD_NEOMDIE_UNPIN_QUICKACCESS) {
            if (Config::Get().RemoveQuickAccess(path)) {
                m_tree.RefreshQuickAccess();
            }
        } else if (cmd == CMD_NEOMDIE_RESET_QUICKACCESS) {
            Config::Get().ResetQuickAccess();
            m_tree.RefreshQuickAccess();
        } else if (cmd == CMD_NEOMDIE_SETTINGS) {
            OnCommand(ID_TOOLS_SETTINGS);
        } else if (cmd == CMD_NEOMDIE_OPEN_EXPLORER) {
            ShellUtils::OpenInExplorer(path);
        } else if (cmd == CMD_BG_VIEW_ITEM_CHECKBOXES) {
            OnCommand(ID_VIEW_ITEM_CHECKBOXES);
        }
    };

    m_multiPane.onCustomCommand = handleCustomCommand;
    m_tree.onCustomCommand = handleCustomCommand;

    auto setupViewCallbacks = [this](FileListView& view, int paneIdx) {
        view.onPathChanged = [this, paneIdx](const std::wstring& p) {
            m_multiPane.UpdatePaneHeader(paneIdx);
            if (m_isSwitchingTab) return;

            int curTab = m_tabBar.GetActiveIndex();
            if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
                auto& tab = m_tabBar.GetTabItemRef(curTab);
                if (paneIdx >= (int)tab.panePaths.size()) {
                    tab.panePaths.resize(paneIdx + 1);
                }
                tab.panePaths[paneIdx] = p;
            }

            if (m_multiPane.GetActiveIndex() == paneIdx) {
                m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
                m_breadcrumb.SetPath(p);
                if (m_treeVisible && Config::Get().autoExpandTree) {
                    m_pendingTreePath = p;
                    PostMessageW(m_hwnd, WM_SYNC_TREE_SELECTION, 0, 0);
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
        };

        view.onSelectionChanged = [this, paneIdx](const std::wstring& sel) {
            if (m_isSwitchingTab) return;
            if (m_previewVisible && m_multiPane.GetActiveIndex() == paneIdx) {
                m_preview.SetFile(sel);
            }
        };

        view.onStatusUpdated = [this, paneIdx](int count, uint64_t totalSize) {
            if (m_isSwitchingTab) return;
            if (m_multiPane.GetActiveIndex() == paneIdx) {
                UpdateStatus(count, totalSize);
            }
        };

        view.onStatusText = [this, paneIdx](const std::wstring& text) {
            if (m_multiPane.GetActiveIndex() == paneIdx && m_hwndStatusBar) {
                SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)text.c_str());
            }
        };

        view.onCustomCommand = [this](int cmd, const std::wstring& path) {
            if (m_multiPane.onCustomCommand) {
                m_multiPane.onCustomCommand(cmd, path);
            }
        };
    };

    for (int i = 0; i < 4; ++i) {
        setupViewCallbacks(m_multiPane.GetPane(i), i);
    }
    m_multiPane.SetShowHidden(Config::Get().showHidden);
    m_multiPane.SetCalcFolderSize(Config::Get().calcFolderSize);

    m_multiPane.onActivePaneChanged = [this](int idx, const std::wstring& p) {
        if (m_isSwitchingTab) return;

        int curTab = m_tabBar.GetActiveIndex();
        if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
            auto& tab = m_tabBar.GetTabItemRef(curTab);
            tab.activePaneIndex = idx;
            if (idx >= 0) {
                if (idx >= (int)tab.panePaths.size()) {
                    tab.panePaths.resize(idx + 1);
                }
                tab.panePaths[idx] = p;
            }
        }

        m_breadcrumb.SetPath(p);
        if (m_treeVisible && Config::Get().autoExpandTree) {
            m_pendingTreePath = p;
            PostMessageW(m_hwnd, WM_SYNC_TREE_SELECTION, 0, 0);
        }
        m_tabBar.SetTabPath(m_tabBar.GetActiveIndex(), p);
        UpdateNavButtons();
        UpdateWindowTitle(p);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            m_filterBar.SetText(activeView->GetFilterQuery());
            activeView->NotifyStatus();
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

    Config& cfg = Config::Get();

    // 1. Menu Button & TabBar
    int menuBtnW = Theme::Get().Scale(34, m_hwnd);
    int menuBtnH = Theme::Get().Scale(28, m_hwnd);
    int menuX = Theme::Get().Scale(6, m_hwnd);
    int menuY = (tabH - menuBtnH) / 2 + Theme::Get().Scale(1, m_hwnd);
    MoveWindow(m_hwndBtnMenu, menuX, menuY, menuBtnW, menuBtnH, TRUE);

    if (cfg.showMenuBar) {
        ShowWindow(m_hwndBtnMenu, SW_HIDE);
        m_tabBar.SetLeftOffset(Theme::Get().Scale(6, m_hwnd));
    } else {
        SetWindowPos(m_hwndBtnMenu, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ShowWindow(m_hwndBtnMenu, SW_SHOW);
        int leftOffset = menuX + menuBtnW + Theme::Get().Scale(8, m_hwnd);
        m_tabBar.SetLeftOffset(leftOffset);
    }
    m_tabBar.UpdateLayout(totalW, tabH);
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
    ShowWindow(m_hwndBtnTree, swStd);
    ShowWindow(m_hwndBtnSplit, swStd);
    ShowWindow(m_hwndBtnPreview, swStd);

    if (cfg.showToolbarStandard && showTB) {
        MoveWindow(m_hwndBtnBack, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnForward, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnUp, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnRefresh, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing * 2;

        MoveWindow(m_hwndBtnTree, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnSplit, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing;
        MoveWindow(m_hwndBtnPreview, curX, barY, btnW, btnH, TRUE);
        curX += btnW + spacing * 2;
    }

    int rightMargin = Theme::Get().Scale(8, m_hwnd);
    int filterW = Theme::Get().Scale(cfg.filterBarWidth, m_hwnd);
    int splitBarToolW = (cfg.showToolbarAddress && cfg.showToolbarSearch && !cfg.lockToolbars && showTB) ? Theme::Get().Scale(5, m_hwnd) : 0;
    int breadcrumbW = totalW - curX - filterW - splitBarToolW - rightMargin;
    if (breadcrumbW < Theme::Get().Scale(120, m_hwnd)) {
        breadcrumbW = Theme::Get().Scale(120, m_hwnd);
        filterW = totalW - curX - breadcrumbW - splitBarToolW - rightMargin;
        if (filterW < Theme::Get().Scale(100, m_hwnd)) filterW = Theme::Get().Scale(100, m_hwnd);
    }

    if (!cfg.showToolbarAddress) {
        ShowWindow(m_breadcrumb.GetHwnd(), SW_HIDE);
        filterW = totalW - curX - rightMargin;
        if (filterW < Theme::Get().Scale(100, m_hwnd)) filterW = Theme::Get().Scale(100, m_hwnd);
    } else {
        ShowWindow(m_breadcrumb.GetHwnd(), showTB ? SW_SHOW : SW_HIDE);
    }

    if (!cfg.showToolbarSearch) {
        ShowWindow(m_filterBar.GetHwnd(), SW_HIDE);
        breadcrumbW = totalW - curX - rightMargin;
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
            curX += filterW;
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
            curX += breadcrumbW;
        }
    }

    // Links Bar
    if (m_linksVisible) {
        ShowWindow(m_linksBar.GetHwnd(), SW_SHOW);
        m_linksBar.UpdateLayout(0, tabH + toolbarH, totalW, linksH);
    } else {
        ShowWindow(m_linksBar.GetHwnd(), SW_HIDE);
    }

    // 3. Center Content Area (Directory Tree + Split Container + Preview Pane)
    LayoutCenterArea();

    // 4. Status Bar
    if (m_hwndStatusBar) {
        SendMessageW(m_hwndStatusBar, WM_SIZE, 0, 0);
    }

    RECT rcToolbar = { 0, tabH, totalW, tabH + toolbarH };
    InvalidateRect(m_hwnd, &rcToolbar, TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::LayoutCenterArea() {
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    int totalW = rcClient.right - rcClient.left;
    int totalH = rcClient.bottom - rcClient.top;

    Config& cfg = Config::Get();
    int tabH = Theme::Get().Scale(36, m_hwnd);
    bool showTB = cfg.showToolbarStandard || cfg.showToolbarAddress || cfg.showToolbarSearch;
    int toolbarH = showTB ? Theme::Get().Scale(38, m_hwnd) : 0;
    int linksH = (m_linksVisible && cfg.showToolbarLinks) ? Theme::Get().Scale(28, m_hwnd) : 0;
    int statusH = (m_hwndStatusBar && cfg.showStatusBar) ? Theme::Get().Scale(24, m_hwnd) : 0;

    int contentY = tabH + toolbarH + linksH;
    int contentH = totalH - contentY - statusH;
    int splitBarW = Theme::Get().Scale(5, m_hwnd);

    int mainStartX = 0;
    int mainWidth = totalW;

    if (m_treeVisible) {
        int minTreeW = Theme::Get().Scale(180, m_hwnd);
        if (m_treeWidth < minTreeW) {
            m_treeWidth = minTreeW;
        }
        int effectiveTreeW = m_treeWidth;
        if (totalW > Theme::Get().Scale(400, m_hwnd) && effectiveTreeW > totalW / 2) {
            effectiveTreeW = totalW / 2;
        }

        m_tree.UpdateLayout(0, contentY, effectiveTreeW, contentH);

        m_rcTreeSplitter.left = effectiveTreeW;
        m_rcTreeSplitter.top = contentY;
        m_rcTreeSplitter.right = effectiveTreeW + splitBarW;
        m_rcTreeSplitter.bottom = contentY + contentH;

        mainStartX = effectiveTreeW + splitBarW;
        mainWidth = totalW - mainStartX;
    }

    ShowWindow(m_preview.GetHwnd(), m_previewVisible ? SW_SHOW : SW_HIDE);
    if (!m_previewVisible) {
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

        RECT rcOldSplitter = m_rcPreviewSplitter;
        m_rcPreviewSplitter.left = mainStartX + listW;
        m_rcPreviewSplitter.top = contentY;
        m_rcPreviewSplitter.right = mainStartX + listW + splitBarW;
        m_rcPreviewSplitter.bottom = contentY + contentH;

        m_preview.UpdateLayout(mainStartX + listW + splitBarW, contentY, m_previewWidth, contentH);

        // Smooth splitter redraw without ghosting
        RECT rcUnion;
        UnionRect(&rcUnion, &rcOldSplitter, &m_rcPreviewSplitter);
        rcUnion.left -= 2;
        rcUnion.right += 2;
        InvalidateRect(m_hwnd, &rcUnion, TRUE);
        UpdateWindow(m_hwnd);
    }
}

void MainWindow::UpdateStatus(int itemCount, uint64_t totalSize) {
    if (!m_hwndStatusBar) return;

    FileListView* activeView = m_multiPane.GetActivePane();
    int selCount = (activeView && activeView->GetHwnd()) ? (int)ListView_GetSelectedCount(activeView->GetHwnd()) : 0;
    bool isZh = I18n::Get().IsChinese();

    wchar_t buf[128];
    if (selCount > 0) {
        if (isZh) {
            StringCchPrintfW(buf, 128, L"已选择 %d 个项目    大小: %s", selCount, Util::FormatBytes(totalSize).c_str());
        } else {
            StringCchPrintfW(buf, 128, L"%d item(s) selected    Size: %s", selCount, Util::FormatBytes(totalSize).c_str());
        }
    } else {
        if (isZh) {
            StringCchPrintfW(buf, 128, L"%d 个项目    大小: %s", itemCount, Util::FormatBytes(totalSize).c_str());
        } else {
            StringCchPrintfW(buf, 128, L"%d items    Size: %s", itemCount, Util::FormatBytes(totalSize).c_str());
        }
    }
    SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)buf);
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

void MainWindow::SetLayout(PaneLayout layout, const std::wstring& targetNewPath) {
    int prevCount = m_multiPane.GetPaneCount();
    m_multiPane.SetLayout(layout);
    int newCount = m_multiPane.GetPaneCount();

    int curTab = m_tabBar.GetActiveIndex();
    if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
        auto& tab = m_tabBar.GetTabItemRef(curTab);
        tab.layout = (int)layout;
        tab.panePaths.resize(newCount);

        std::wstring initPath = m_multiPane.GetPane(m_multiPane.GetActiveIndex()).GetCurrentPath();
        if (initPath.empty() && !tab.path.empty()) initPath = tab.path;
        if (initPath.empty()) initPath = L"This PC";

        for (int p = 0; p < newCount; ++p) {
            if (m_multiPane.GetPane(p).GetCurrentPath().empty()) {
                std::wstring pPath = (!targetNewPath.empty() && p == newCount - 1) ? targetNewPath : initPath;
                m_multiPane.GetPane(p).NavigateTo(pPath, false);
            }
            tab.panePaths[p] = m_multiPane.GetPane(p).GetCurrentPath();
        }
        tab.activePaneIndex = m_multiPane.GetActiveIndex();
    }
    BuildMenu();
    LayoutAll();
    UpdateWindow(m_hwnd);
    if (m_hwndBtnSplit) {
        InvalidateRect(m_hwndBtnSplit, nullptr, TRUE);
        UpdateWindow(m_hwndBtnSplit);
    }
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

void MainWindow::HandleTabDragMove(int dragIndex, int hostTabIndex, POINT ptScreen) {
    if (dragIndex < 0 || dragIndex >= m_tabBar.GetTabCount()) {
        m_splitOverlay.Hide();
        return;
    }

    HWND hMulti = m_multiPane.GetHwnd();
    if (!hMulti || !IsWindow(hMulti)) {
        m_splitOverlay.Hide();
        return;
    }

    RECT rcPane;
    GetWindowRect(hMulti, &rcPane);
    if (!PtInRect(&rcPane, ptScreen)) {
        m_splitOverlay.Hide();
        return;
    }

    SplitDropAction action = CalculateDropAction(dragIndex, hostTabIndex, ptScreen);
    if (action.type != SplitDropAction::None) {
        m_splitOverlay.Show(action.overlayRect, action.overlayText);
    } else {
        m_splitOverlay.Hide();
    }
}

MainWindow::SplitDropAction MainWindow::CalculateDropAction(int dragIndex, int hostTabIndex, POINT ptScreen) {
    SplitDropAction act;
    act.type = SplitDropAction::None;

    HWND hMulti = m_multiPane.GetHwnd();
    if (!hMulti) return act;

    RECT rc;
    GetWindowRect(hMulti, &rc);
    int totalW = rc.right - rc.left;
    int totalH = rc.bottom - rc.top;
    if (totalW <= 50 || totalH <= 50) return act;

    int rx = ptScreen.x - rc.left;
    int ry = ptScreen.y - rc.top;

    PaneLayout curLayout = m_multiPane.GetLayout();
    bool isZh = I18n::Get().IsChinese();

    std::wstring dragTitle = (dragIndex >= 0 && dragIndex < m_tabBar.GetTabCount())
        ? m_tabBar.GetTabItem(dragIndex).title : L"";
    std::wstring hostTitle = (hostTabIndex >= 0 && hostTabIndex < m_tabBar.GetTabCount())
        ? m_tabBar.GetTabItem(hostTabIndex).title : L"";
    if (dragTitle.empty()) dragTitle = isZh ? L"标签页" : L"Tab";
    if (hostTitle.empty()) hostTitle = isZh ? L"当前标签页" : L"Current Tab";

    auto fmtMerge = [&](const std::wstring& posZh, const std::wstring& posEn) -> std::wstring {
        if (isZh) {
            return L"将【" + dragTitle + L"】合并到【" + hostTitle + L"】(" + posZh + L")";
        } else {
            return L"Merge [" + dragTitle + L"] into [" + hostTitle + L"] (" + posEn + L")";
        }
    };

    if (curLayout == PaneLayout::Single) {
        if (rx > totalW * 0.5) {
            if (ry > totalH * 0.75) {
                act.type = SplitDropAction::SplitDualHorizontalBottom;
                act.overlayRect = { rc.left, rc.top + totalH / 2, rc.right, rc.bottom };
                act.overlayText = fmtMerge(L"下方双栏", L"Bottom Dual");
            } else {
                act.type = SplitDropAction::SplitDualVerticalRight;
                act.overlayRect = { rc.left + totalW / 2, rc.top, rc.right, rc.bottom };
                act.overlayText = fmtMerge(L"右侧双栏", L"Right Dual");
            }
        } else {
            if (ry > totalH * 0.75) {
                act.type = SplitDropAction::SplitDualHorizontalBottom;
                act.overlayRect = { rc.left, rc.top + totalH / 2, rc.right, rc.bottom };
                act.overlayText = fmtMerge(L"下方双栏", L"Bottom Dual");
            } else if (ry < totalH * 0.25) {
                act.type = SplitDropAction::SplitDualHorizontalTop;
                act.overlayRect = { rc.left, rc.top, rc.right, rc.top + totalH / 2 };
                act.overlayText = fmtMerge(L"上方双栏", L"Top Dual");
            } else {
                act.type = SplitDropAction::SplitDualVerticalLeft;
                act.overlayRect = { rc.left, rc.top, rc.left + totalW / 2, rc.bottom };
                act.overlayText = fmtMerge(L"左侧双栏", L"Left Dual");
            }
        }
    } else if (curLayout == PaneLayout::DualVertical) {
        int midX = rc.left + totalW / 2;
        if (ptScreen.x >= midX) {
            if (ry > totalH * 0.5) {
                act.type = SplitDropAction::SplitThreeRightBottom;
                act.overlayRect = { midX, rc.top + totalH / 2, rc.right, rc.bottom };
                act.overlayText = fmtMerge(L"三分栏右下", L"3-Pane Bottom-Right");
            } else {
                act.type = SplitDropAction::SplitThreeRightTop;
                act.overlayRect = { midX, rc.top, rc.right, rc.top + totalH / 2 };
                act.overlayText = fmtMerge(L"三分栏右上", L"3-Pane Top-Right");
            }
        } else {
            if (ry > totalH * 0.5) {
                act.type = SplitDropAction::SplitQuadBottomLeft;
                act.overlayRect = { rc.left, rc.top + totalH / 2, midX, rc.bottom };
                act.overlayText = fmtMerge(L"四分栏左下", L"4-Pane Bottom-Left");
            } else {
                act.type = SplitDropAction::MergeIntoPane;
                act.targetPaneIndex = 0;
                act.overlayRect = { rc.left, rc.top, midX, rc.bottom };
                act.overlayText = fmtMerge(L"覆盖左侧分栏", L"Cover Left Pane");
            }
        }
    } else if (curLayout == PaneLayout::DualHorizontal) {
        int midY = rc.top + totalH / 2;
        if (rx > totalW * 0.5) {
            act.type = SplitDropAction::SplitQuad;
            act.overlayRect = { rc.left + totalW / 2, rc.top, rc.right, rc.bottom };
            act.overlayText = fmtMerge(L"四分栏网格", L"4-Pane Grid");
        } else {
            if (ptScreen.y < midY) {
                act.type = SplitDropAction::MergeIntoPane;
                act.targetPaneIndex = 0;
                act.overlayRect = { rc.left, rc.top, rc.left + totalW / 2, midY };
                act.overlayText = fmtMerge(L"覆盖上方分栏", L"Cover Top Pane");
            } else {
                act.type = SplitDropAction::MergeIntoPane;
                act.targetPaneIndex = 1;
                act.overlayRect = { rc.left, midY, rc.left + totalW / 2, rc.bottom };
                act.overlayText = fmtMerge(L"覆盖下方分栏", L"Cover Bottom Pane");
            }
        }
    } else if (curLayout == PaneLayout::ThreeLeft) {
        int midX = rc.left + totalW / 2;
        if (ptScreen.x < midX) {
            act.type = SplitDropAction::SplitQuadBottomLeft;
            act.overlayRect = { rc.left, rc.top + totalH / 2, midX, rc.bottom };
            act.overlayText = fmtMerge(L"扩展四分栏(左下)", L"Expand 4-Pane Bottom-Left");
        } else {
            int midY = rc.top + totalH / 2;
            if (ptScreen.y < midY) {
                act.type = SplitDropAction::MergeIntoPane;
                act.targetPaneIndex = 1;
                act.overlayRect = { midX, rc.top, rc.right, midY };
                act.overlayText = fmtMerge(L"覆盖右上分栏", L"Cover Top-Right Pane");
            } else {
                act.type = SplitDropAction::MergeIntoPane;
                act.targetPaneIndex = 2;
                act.overlayRect = { midX, midY, rc.right, rc.bottom };
                act.overlayText = fmtMerge(L"覆盖右下分栏", L"Cover Bottom-Right Pane");
            }
        }
    } else if (curLayout == PaneLayout::Quad) {
        int midX = rc.left + totalW / 2;
        int midY = rc.top + totalH / 2;
        if (ptScreen.x < midX && ptScreen.y < midY) {
            act.type = SplitDropAction::MergeIntoPane;
            act.targetPaneIndex = 0;
            act.overlayRect = { rc.left, rc.top, midX, midY };
            act.overlayText = fmtMerge(L"覆盖左上分栏", L"Cover Top-Left Pane");
        } else if (ptScreen.x >= midX && ptScreen.y < midY) {
            act.type = SplitDropAction::MergeIntoPane;
            act.targetPaneIndex = 1;
            act.overlayRect = { midX, rc.top, rc.right, midY };
            act.overlayText = fmtMerge(L"覆盖右上分栏", L"Cover Top-Right Pane");
        } else if (ptScreen.x < midX && ptScreen.y >= midY) {
            act.type = SplitDropAction::MergeIntoPane;
            act.targetPaneIndex = 2;
            act.overlayRect = { rc.left, midY, midX, rc.bottom };
            act.overlayText = fmtMerge(L"覆盖左下分栏", L"Cover Bottom-Left Pane");
        } else {
            act.type = SplitDropAction::MergeIntoPane;
            act.targetPaneIndex = 3;
            act.overlayRect = { midX, midY, rc.right, rc.bottom };
            act.overlayText = fmtMerge(L"覆盖右下分栏", L"Cover Bottom-Right Pane");
        }
    }

    return act;
}

void MainWindow::HandleTabDragEnd(int dragIndex, int hostTabIndex, POINT ptScreen) {
    m_splitOverlay.Hide();

    if (dragIndex < 0 || dragIndex >= m_tabBar.GetTabCount()) return;

    SplitDropAction action = CalculateDropAction(dragIndex, hostTabIndex, ptScreen);
    if (action.type == SplitDropAction::None) return;

    std::wstring draggedPath = m_tabBar.GetTabPath(dragIndex);
    if (draggedPath.empty()) draggedPath = L"This PC";

    bool shouldCloseDragged = (m_tabBar.GetTabCount() > 1);

    // If hostTab is different and valid, make sure hostTab is activated
    if (hostTabIndex >= 0 && hostTabIndex < m_tabBar.GetTabCount() && hostTabIndex != dragIndex) {
        m_tabBar.SetActiveTab(hostTabIndex, true);
    } else if (shouldCloseDragged && m_tabBar.GetActiveIndex() == dragIndex) {
        int prev = m_tabBar.GetPrevActiveIndex();
        int newHost = (prev >= 0 && prev < m_tabBar.GetTabCount() && prev != dragIndex)
            ? prev
            : ((dragIndex > 0) ? (dragIndex - 1) : 1);
        m_tabBar.SetActiveTab(newHost, true);
    }

    std::wstring p0 = m_multiPane.GetPane(0).GetCurrentPath();
    std::wstring p1 = m_multiPane.GetPane(1).GetCurrentPath();
    std::wstring p2 = m_multiPane.GetPane(2).GetCurrentPath();
    std::wstring p3 = m_multiPane.GetPane(3).GetCurrentPath();

    if (p0.empty()) p0 = draggedPath;
    if (p1.empty()) p1 = draggedPath;
    if (p2.empty()) p2 = draggedPath;
    if (p3.empty()) p3 = draggedPath;

    switch (action.type) {
    case SplitDropAction::SplitDualVerticalRight:
        SetLayout(PaneLayout::DualVertical);
        m_multiPane.GetPane(0).NavigateTo(p0, false);
        m_multiPane.GetPane(1).NavigateTo(draggedPath, false);
        m_multiPane.SetActivePane(1);
        break;

    case SplitDropAction::SplitDualVerticalLeft:
        SetLayout(PaneLayout::DualVertical);
        m_multiPane.GetPane(0).NavigateTo(draggedPath, false);
        m_multiPane.GetPane(1).NavigateTo(p0, false);
        m_multiPane.SetActivePane(0);
        break;

    case SplitDropAction::SplitDualHorizontalBottom:
        SetLayout(PaneLayout::DualHorizontal);
        m_multiPane.GetPane(0).NavigateTo(p0, false);
        m_multiPane.GetPane(1).NavigateTo(draggedPath, false);
        m_multiPane.SetActivePane(1);
        break;

    case SplitDropAction::SplitDualHorizontalTop:
        SetLayout(PaneLayout::DualHorizontal);
        m_multiPane.GetPane(0).NavigateTo(draggedPath, false);
        m_multiPane.GetPane(1).NavigateTo(p0, false);
        m_multiPane.SetActivePane(0);
        break;

    case SplitDropAction::SplitThreeRightBottom:
        SetLayout(PaneLayout::ThreeLeft);
        m_multiPane.GetPane(0).NavigateTo(p0, false);
        m_multiPane.GetPane(1).NavigateTo(p1, false);
        m_multiPane.GetPane(2).NavigateTo(draggedPath, false);
        m_multiPane.SetActivePane(2);
        break;

    case SplitDropAction::SplitThreeRightTop:
        SetLayout(PaneLayout::ThreeLeft);
        m_multiPane.GetPane(0).NavigateTo(p0, false);
        m_multiPane.GetPane(1).NavigateTo(draggedPath, false);
        m_multiPane.GetPane(2).NavigateTo(p1, false);
        m_multiPane.SetActivePane(1);
        break;

    case SplitDropAction::SplitQuadBottomLeft:
        SetLayout(PaneLayout::Quad);
        m_multiPane.GetPane(0).NavigateTo(p0, false);
        m_multiPane.GetPane(1).NavigateTo(p1, false);
        m_multiPane.GetPane(2).NavigateTo(draggedPath, false);
        m_multiPane.GetPane(3).NavigateTo(!p2.empty() ? p2 : p1, false);
        m_multiPane.SetActivePane(2);
        break;

    case SplitDropAction::SplitQuad:
        SetLayout(PaneLayout::Quad);
        m_multiPane.GetPane(0).NavigateTo(p0, false);
        m_multiPane.GetPane(1).NavigateTo(draggedPath, false);
        m_multiPane.GetPane(2).NavigateTo(p1, false);
        m_multiPane.GetPane(3).NavigateTo(draggedPath, false);
        m_multiPane.SetActivePane(1);
        break;

    case SplitDropAction::MergeIntoPane:
        m_multiPane.GetPane(action.targetPaneIndex).NavigateTo(draggedPath, false);
        m_multiPane.SetActivePane(action.targetPaneIndex);
        break;

    default:
        break;
    }

    if (shouldCloseDragged) {
        m_tabBar.CloseTab(dragIndex);
    }

    int curTab = m_tabBar.GetActiveIndex();
    if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
        auto& tab = m_tabBar.GetTabItemRef(curTab);
        tab.layout = (int)m_multiPane.GetLayout();
        tab.activePaneIndex = m_multiPane.GetActiveIndex();
        int pCount = m_multiPane.GetPaneCount();
        tab.panePaths.resize(pCount);
        for (int p = 0; p < pCount; ++p) {
            tab.panePaths[p] = m_multiPane.GetPane(p).GetCurrentPath();
        }
    }
    LayoutAll();
    BuildMenu();
}

MainWindow::PaneDragAction MainWindow::CalculatePaneDropAction(int paneIdx, POINT ptScreen) {
    PaneDragAction act;
    act.type = PaneDragActionType::None;

    if (paneIdx < 0 || paneIdx >= m_multiPane.GetPaneCount()) return act;

    HWND hTabBar = m_tabBar.GetHwnd();
    HWND hMulti = m_multiPane.GetHwnd();
    if (!hTabBar || !hMulti) return act;

    RECT rcTab;
    GetWindowRect(hTabBar, &rcTab);
    RECT rcMulti;
    GetWindowRect(hMulti, &rcMulti);

    bool isZh = I18n::Get().IsChinese();
    std::wstring curPanePath = m_multiPane.GetPane(paneIdx).GetCurrentPath();
    std::wstring curPaneTitle = ShellUtils::GetDisplayName(curPanePath);
    if (curPaneTitle.empty()) curPaneTitle = curPanePath;

    // 1. Check if dragging UP towards TabBar to detach as tab
    if (ptScreen.y <= rcTab.bottom + 20) {
        act.type = PaneDragActionType::DetachToTab;
        act.overlayRect = rcTab;
        act.overlayText = isZh ? (L"还原为独立标签页: 【" + curPaneTitle + L"】")
                               : (L"Detach as Tab: [" + curPaneTitle + L"]");
        return act;
    }

    // 2. Check if dragging inside MultiPaneContainer
    if (!PtInRect(&rcMulti, ptScreen)) {
        return act;
    }

    int totalW = rcMulti.right - rcMulti.left;
    int totalH = rcMulti.bottom - rcMulti.top;
    if (totalW <= 50 || totalH <= 50) return act;

    int rx = ptScreen.x - rcMulti.left;
    int ry = ptScreen.y - rcMulti.top;
    PaneLayout curLayout = m_multiPane.GetLayout();

    if (curLayout == PaneLayout::DualVertical) {
        int midX = rcMulti.left + totalW / 2;
        RECT rcLeft = { rcMulti.left, rcMulti.top, midX, rcMulti.bottom };
        RECT rcRight = { midX, rcMulti.top, rcMulti.right, rcMulti.bottom };

        std::wstring title0 = ShellUtils::GetDisplayName(m_multiPane.GetPane(0).GetCurrentPath());
        std::wstring title1 = ShellUtils::GetDisplayName(m_multiPane.GetPane(1).GetCurrentPath());
        if (title0.empty()) title0 = isZh ? L"左侧分栏" : L"Left Pane";
        if (title1.empty()) title1 = isZh ? L"右侧分栏" : L"Right Pane";

        if (paneIdx == 0) {
            // Dragging left pane
            if (ptScreen.x >= midX) {
                if (ry > totalH * 0.75) {
                    act.type = PaneDragActionType::DualVertical_To_DualHorizontal;
                    act.overlayRect = { rcMulti.left, rcMulti.top + totalH / 2, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"转换为上下双栏 (下方)" : L"Convert to Dual Horizontal (Bottom)";
                } else {
                    act.type = PaneDragActionType::SwapPanes;
                    act.targetPaneIdx = 1;
                    act.overlayRect = rcRight;
                    act.overlayText = isZh ? (L"互换左右分栏: 【" + title0 + L"】⇄【" + title1 + L"】")
                                           : (L"Swap Left/Right: [" + title0 + L"] <-> [" + title1 + L"]");
                }
            } else {
                if (ry > totalH * 0.75) {
                    act.type = PaneDragActionType::DualVertical_To_DualHorizontal;
                    act.overlayRect = { rcMulti.left, rcMulti.top + totalH / 2, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"转换为上下双栏 (下方)" : L"Convert to Dual Horizontal (Bottom)";
                }
            }
        } else if (paneIdx == 1) {
            // Dragging right pane
            if (ptScreen.x < midX) {
                if (ry > totalH * 0.75) {
                    act.type = PaneDragActionType::DualVertical_To_DualHorizontal;
                    act.overlayRect = { rcMulti.left, rcMulti.top + totalH / 2, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"转换为上下双栏 (下方)" : L"Convert to Dual Horizontal (Bottom)";
                } else {
                    act.type = PaneDragActionType::SwapPanes;
                    act.targetPaneIdx = 0;
                    act.overlayRect = rcLeft;
                    act.overlayText = isZh ? (L"互换左右分栏: 【" + title1 + L"】⇄【" + title0 + L"】")
                                           : (L"Swap Left/Right: [" + title1 + L"] <-> [" + title0 + L"]");
                }
            } else {
                if (ry > totalH * 0.75) {
                    act.type = PaneDragActionType::SplitThreeRightBottom;
                    act.overlayRect = { midX, rcMulti.top + totalH / 2, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"拆分为三分栏 (右下)" : L"Split to 3 Panes (Bottom-Right)";
                }
            }
        }
    } else if (curLayout == PaneLayout::DualHorizontal) {
        int midY = rcMulti.top + totalH / 2;
        RECT rcTop = { rcMulti.left, rcMulti.top, rcMulti.right, midY };
        RECT rcBottom = { rcMulti.left, midY, rcMulti.right, rcMulti.bottom };

        std::wstring title0 = ShellUtils::GetDisplayName(m_multiPane.GetPane(0).GetCurrentPath());
        std::wstring title1 = ShellUtils::GetDisplayName(m_multiPane.GetPane(1).GetCurrentPath());
        if (title0.empty()) title0 = isZh ? L"上方分栏" : L"Top Pane";
        if (title1.empty()) title1 = isZh ? L"下方分栏" : L"Bottom Pane";

        if (paneIdx == 0) {
            // Dragging top pane
            if (ptScreen.y >= midY) {
                if (rx > totalW * 0.75) {
                    act.type = PaneDragActionType::DualHorizontal_To_DualVertical;
                    act.overlayRect = { rcMulti.left + totalW / 2, rcMulti.top, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"转换为左右双栏 (右侧)" : L"Convert to Dual Vertical (Right)";
                } else {
                    act.type = PaneDragActionType::SwapPanes;
                    act.targetPaneIdx = 1;
                    act.overlayRect = rcBottom;
                    act.overlayText = isZh ? (L"互换上下分栏: 【" + title0 + L"】⇄【" + title1 + L"】")
                                           : (L"Swap Top/Bottom: [" + title0 + L"] <-> [" + title1 + L"]");
                }
            } else {
                if (rx > totalW * 0.75) {
                    act.type = PaneDragActionType::DualHorizontal_To_DualVertical;
                    act.overlayRect = { rcMulti.left + totalW / 2, rcMulti.top, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"转换为左右双栏 (右侧)" : L"Convert to Dual Vertical (Right)";
                }
            }
        } else if (paneIdx == 1) {
            // Dragging bottom pane
            if (ptScreen.y < midY) {
                if (rx > totalW * 0.75) {
                    act.type = PaneDragActionType::DualHorizontal_To_DualVertical;
                    act.overlayRect = { rcMulti.left + totalW / 2, rcMulti.top, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"转换为左右双栏 (右侧)" : L"Convert to Dual Vertical (Right)";
                } else {
                    act.type = PaneDragActionType::SwapPanes;
                    act.targetPaneIdx = 0;
                    act.overlayRect = rcTop;
                    act.overlayText = isZh ? (L"互换上下分栏: 【" + title1 + L"】⇄【" + title0 + L"】")
                                           : (L"Swap Top/Bottom: [" + title1 + L"] <-> [" + title0 + L"]");
                }
            } else {
                if (rx > totalW * 0.75) {
                    act.type = PaneDragActionType::SplitQuad;
                    act.overlayRect = { rcMulti.left + totalW / 2, rcMulti.top, rcMulti.right, rcMulti.bottom };
                    act.overlayText = isZh ? L"拆分为四分栏网格" : L"Split to 4 Panes Grid";
                }
            }
        }
    } else if (curLayout == PaneLayout::ThreeLeft) {
        int midX = rcMulti.left + totalW / 2;
        int midY = rcMulti.top + totalH / 2;
        if (paneIdx == 0) {
            if (ptScreen.x >= midX) {
                act.type = PaneDragActionType::SwapPanes;
                act.targetPaneIdx = (ptScreen.y < midY) ? 1 : 2;
                act.overlayRect = (ptScreen.y < midY)
                    ? RECT{ midX, rcMulti.top, rcMulti.right, midY }
                    : RECT{ midX, midY, rcMulti.right, rcMulti.bottom };
                act.overlayText = isZh ? L"交换左侧分栏与右侧分栏" : L"Swap Left Pane with Right Pane";
            } else if (ry > totalH * 0.75) {
                act.type = PaneDragActionType::SplitQuad;
                act.overlayRect = { rcMulti.left, rcMulti.top + totalH / 2, midX, rcMulti.bottom };
                act.overlayText = isZh ? L"扩展为四分栏 (左下)" : L"Expand to 4 Panes (Bottom-Left)";
            }
        } else if (paneIdx == 1) {
            if (ptScreen.x < midX) {
                act.type = PaneDragActionType::SwapPanes;
                act.targetPaneIdx = 0;
                act.overlayRect = { rcMulti.left, rcMulti.top, midX, rcMulti.bottom };
                act.overlayText = isZh ? L"交换右上分栏与左侧主分栏" : L"Swap Top-Right with Main Pane";
            } else if (ptScreen.y >= midY) {
                act.type = PaneDragActionType::SwapPanes;
                act.targetPaneIdx = 2;
                act.overlayRect = { midX, midY, rcMulti.right, rcMulti.bottom };
                act.overlayText = isZh ? L"互换右上与右下分栏" : L"Swap Top-Right and Bottom-Right";
            }
        } else if (paneIdx == 2) {
            if (ptScreen.x < midX) {
                act.type = PaneDragActionType::SwapPanes;
                act.targetPaneIdx = 0;
                act.overlayRect = { rcMulti.left, rcMulti.top, midX, rcMulti.bottom };
                act.overlayText = isZh ? L"交换右下分栏与左侧主分栏" : L"Swap Bottom-Right with Main Pane";
            } else if (ptScreen.y < midY) {
                act.type = PaneDragActionType::SwapPanes;
                act.targetPaneIdx = 1;
                act.overlayRect = { midX, rcMulti.top, rcMulti.right, midY };
                act.overlayText = isZh ? L"互换右上与右下分栏" : L"Swap Top-Right and Bottom-Right";
            }
        }
    } else if (curLayout == PaneLayout::Quad) {
        int midX = rcMulti.left + totalW / 2;
        int midY = rcMulti.top + totalH / 2;
        int hoverQ = -1;
        RECT rcQ = { 0 };
        if (ptScreen.x < midX && ptScreen.y < midY) {
            hoverQ = 0;
            rcQ = { rcMulti.left, rcMulti.top, midX, midY };
        } else if (ptScreen.x >= midX && ptScreen.y < midY) {
            hoverQ = 1;
            rcQ = { midX, rcMulti.top, rcMulti.right, midY };
        } else if (ptScreen.x < midX && ptScreen.y >= midY) {
            hoverQ = 2;
            rcQ = { rcMulti.left, midY, midX, rcMulti.bottom };
        } else {
            hoverQ = 3;
            rcQ = { midX, midY, rcMulti.right, rcMulti.bottom };
        }

        if (hoverQ >= 0 && hoverQ != paneIdx) {
            act.type = PaneDragActionType::SwapPanes;
            act.targetPaneIdx = hoverQ;
            act.overlayRect = rcQ;
            std::wstring tA = ShellUtils::GetDisplayName(m_multiPane.GetPane(paneIdx).GetCurrentPath());
            std::wstring tB = ShellUtils::GetDisplayName(m_multiPane.GetPane(hoverQ).GetCurrentPath());
            act.overlayText = isZh ? (L"互换分栏位置: 【" + tA + L"】⇄【" + tB + L"】")
                                   : (L"Swap Panes: [" + tA + L"] <-> [" + tB + L"]");
        }
    }

    return act;
}

void MainWindow::HandlePaneDragMove(int paneIdx, POINT ptScreen) {
    if (paneIdx < 0 || paneIdx >= m_multiPane.GetPaneCount()) {
        m_splitOverlay.Hide();
        return;
    }

    PaneDragAction act = CalculatePaneDropAction(paneIdx, ptScreen);
    if (act.type != PaneDragActionType::None) {
        m_splitOverlay.Show(act.overlayRect, act.overlayText);
    } else {
        m_splitOverlay.Hide();
    }
}

void MainWindow::HandlePaneDragEnd(int paneIdx, POINT ptScreen) {
    m_splitOverlay.Hide();

    if (paneIdx < 0 || paneIdx >= m_multiPane.GetPaneCount()) return;

    PaneDragAction act = CalculatePaneDropAction(paneIdx, ptScreen);
    if (act.type == PaneDragActionType::None) return;

    switch (act.type) {
    case PaneDragActionType::DetachToTab: {
        FileListView& pane = m_multiPane.GetPane(paneIdx);
        std::wstring panePath = pane.GetCurrentPath();
        if (panePath.empty()) panePath = L"This PC";
        auto hist = pane.GetHistory();
        int histIdx = pane.GetHistoryIndex();
        auto vstate = pane.GetViewState();

        // 1. Close this split pane from container
        m_multiPane.ClosePane(paneIdx);

        // 2. Add as a new tab and activate it
        int newTabIdx = m_tabBar.AddTab(panePath, true);
        if (newTabIdx >= 0) {
            FileListView* curActive = m_multiPane.GetActivePane();
            if (curActive) {
                curActive->SetHistory(hist, histIdx);
                curActive->RestoreViewState(vstate);
            }
        }
        break;
    }

    case PaneDragActionType::SwapPanes: {
        if (act.targetPaneIdx >= 0 && act.targetPaneIdx < m_multiPane.GetPaneCount()) {
            m_multiPane.SwapPanes(paneIdx, act.targetPaneIdx);
        }
        break;
    }

    case PaneDragActionType::DualVertical_To_DualHorizontal: {
        SetLayout(PaneLayout::DualHorizontal);
        break;
    }

    case PaneDragActionType::DualHorizontal_To_DualVertical: {
        SetLayout(PaneLayout::DualVertical);
        break;
    }

    case PaneDragActionType::SplitThreeRightBottom: {
        SetLayout(PaneLayout::ThreeLeft);
        break;
    }

    case PaneDragActionType::SplitQuad: {
        SetLayout(PaneLayout::Quad);
        break;
    }

    default:
        break;
    }

    int curTab = m_tabBar.GetActiveIndex();
    if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
        auto& tab = m_tabBar.GetTabItemRef(curTab);
        tab.layout = (int)m_multiPane.GetLayout();
        tab.activePaneIndex = m_multiPane.GetActiveIndex();
        int pCount = m_multiPane.GetPaneCount();
        tab.panePaths.resize(pCount);
        for (int p = 0; p < pCount; ++p) {
            tab.panePaths[p] = m_multiPane.GetPane(p).GetCurrentPath();
        }
    }
    LayoutAll();
    BuildMenu();
}
void MainWindow::ToggleTree() {
    m_treeVisible = !m_treeVisible;
    Config::Get().showTree = m_treeVisible;
    Config::Get().Save();
    ShowWindow(m_tree.GetHwnd(), m_treeVisible ? SW_SHOW : SW_HIDE);
    BuildMenu();
    LayoutAll();
    if (m_hwndBtnTree) {
        InvalidateRect(m_hwndBtnTree, nullptr, TRUE);
        UpdateWindow(m_hwndBtnTree);
    }
}

void MainWindow::ToggleLinksBar() {
    m_linksVisible = !m_linksVisible;
    Config::Get().showToolbarLinks = m_linksVisible;
    Config::Get().Save();
    ShowWindow(m_linksBar.GetHwnd(), m_linksVisible ? SW_SHOW : SW_HIDE);
    BuildMenu();
    LayoutAll();
}

void MainWindow::ShowToolbarContextMenu(POINT ptScreen) {
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
                1001, isZh ? L"显示链接栏(&K)" : L"Show Links Bar(&K)");
    AppendMenuW(hMenu, MF_STRING | (m_treeVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_TREE_TOGGLE, isZh ? L"显示文件夹目录树(&T)\tF8" : L"Show Directory Tree(&T)\tF8");
    AppendMenuW(hMenu, MF_STRING | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_PREVIEW_TOGGLE, isZh ? L"显示快速预览窗格(&P)\tAlt+P" : L"Show Preview Pane(&P)\tAlt+P");
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
}

void MainWindow::TogglePreview() {
    m_previewVisible = !m_previewVisible;
    Config::Get().previewEnabled = m_previewVisible;
    WritePrivateProfileStringW(L"General", L"PreviewEnabled", m_previewVisible ? L"1" : L"0", Config::Get().GetIniPath().c_str());

    HMENU hMenu = GetMenu(m_hwnd);
    if (hMenu) {
        CheckMenuItem(hMenu, ID_VIEW_PREVIEW_TOGGLE, MF_BYCOMMAND | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED));
    }

    if (m_hwndBtnPreview) {
        InvalidateRect(m_hwndBtnPreview, nullptr, TRUE);
        UpdateWindow(m_hwndBtnPreview);
    }

    if (m_previewVisible) {
        LayoutCenterArea();
        ShowWindow(m_preview.GetHwnd(), SW_SHOW);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            auto sel = activeView->GetSelectedPaths();
            if (!sel.empty()) {
                m_preview.SetFile(sel[0]);
            }
        }
    } else {
        ShowWindow(m_preview.GetHwnd(), SW_HIDE);
        LayoutCenterArea();
        m_preview.Clear();
    }
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
    case ID_VIEW_REFRESH: {
        HCURSOR hOldCur = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        std::wstring curFilter = m_filterBar.GetText();
        for (int i = 0; i < m_multiPane.GetPaneCount(); ++i) {
            FileListView& pane = m_multiPane.GetPane(i);
            if (curFilter.empty() && !pane.GetFilterQuery().empty()) {
                pane.SetFilter(L"", false, false);
            } else if (!curFilter.empty() && m_filterBar.IsSearchSubfolders()) {
                pane.SetFilter(curFilter, true, false);
            }
            pane.Refresh();
        }
        m_tree.Refresh();
        SetCursor(hOldCur);
        break;
    }
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
        break;

    case IDC_BTN_MENU: {
        RECT rcBtn;
        GetWindowRect(m_hwndBtnMenu, &rcBtn);
        HMENU hPopup = CreatePopupMenu();
        bool isZh = I18n::Get().IsChinese();

        if (m_hMenuBar) {
            int count = GetMenuItemCount(m_hMenuBar);
            for (int i = 0; i < count; ++i) {
                wchar_t menuText[128] = { 0 };
                GetMenuStringW(m_hMenuBar, i, menuText, 128, MF_BYPOSITION);
                HMENU hSub = GetSubMenu(m_hMenuBar, i);
                if (hSub) {
                    AppendMenuW(hPopup, MF_POPUP, (UINT_PTR)hSub, menuText);
                }
            }
        }
        AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPopup, MF_STRING | (Config::Get().showMenuBar ? MF_CHECKED : MF_UNCHECKED),
                    ID_VIEW_TOGGLE_MENUBAR, isZh ? L"显示传统明文菜单栏(&M)" : L"Show Classic Menu Bar(&M)");

        TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_TOPALIGN, rcBtn.left, rcBtn.bottom, 0, m_hwnd, nullptr);

        int count = GetMenuItemCount(hPopup);
        for (int i = count - 1; i >= 0; --i) {
            RemoveMenu(hPopup, i, MF_BYPOSITION);
        }
        DestroyMenu(hPopup);
        break;
    }

    case ID_VIEW_TOGGLE_MENUBAR: {
        Config::Get().showMenuBar = !Config::Get().showMenuBar;
        Config::Get().Save();
        BuildMenu();
        MARGINS margins = Config::Get().showMenuBar ? MARGINS{ 0, 0, 0, 0 } : MARGINS{ 0, 0, 1, 0 };
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        DrawMenuBar(m_hwnd);
        LayoutAll();
        break;
    }

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
    case ID_FILE_CLOSE_LEFT_TABS:
        m_tabBar.CloseTabsToLeft(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_CLOSE_RIGHT_TABS:
        m_tabBar.CloseTabsToRight(m_tabBar.GetActiveIndex());
        BuildMenu();
        break;
    case ID_FILE_NEW_WINDOW_SPECIFY: {
        IFileDialog* pfd = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
            DWORD dwOptions = 0;
            pfd->GetOptions(&dwOptions);
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
            if (SUCCEEDED(pfd->Show(m_hwnd))) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(pfd->GetResult(&psi))) {
                    PWSTR pszPath = nullptr;
                    if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                        wchar_t exePath[MAX_PATH];
                        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                        ShellExecuteW(nullptr, L"open", exePath, pszPath, nullptr, SW_SHOWNORMAL);
                        CoTaskMemFree(pszPath);
                    }
                    psi->Release();
                }
            }
            pfd->Release();
        }
        break;
    }
    case ID_FILE_NEW_WINDOW_DESKTOP: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &p)) && p) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            ShellExecuteW(nullptr, L"open", exePath, p, nullptr, SW_SHOWNORMAL);
            CoTaskMemFree(p);
        }
        break;
    }
    case ID_FILE_NEW_WINDOW_CURRENT: {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        ShellExecuteW(nullptr, L"open", exePath, activeView->GetCurrentPath().c_str(), nullptr, SW_SHOWNORMAL);
        break;
    }
    case ID_FILE_NEW_WINDOW_CLIPBOARD: {
        if (OpenClipboard(m_hwnd)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pText = (wchar_t*)GlobalLock(hData);
                if (pText && PathFileExistsW(pText)) {
                    wchar_t exePath[MAX_PATH];
                    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                    ShellExecuteW(nullptr, L"open", exePath, pText, nullptr, SW_SHOWNORMAL);
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
        break;
    }
    case ID_FILE_OPEN_EXPLORER:
        ShellExecuteW(nullptr, L"open", L"explorer.exe", activeView->GetCurrentPath().c_str(), nullptr, SW_SHOWNORMAL);
        break;
    case ID_FILE_CREATE_SHORTCUT: {
        auto sel = activeView->GetSelectedPaths();
        std::wstring target = !sel.empty() ? sel[0] : activeView->GetCurrentPath();
        PWSTR pDesk = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &pDesk)) && pDesk) {
            std::wstring lnkPath = std::wstring(pDesk) + L"\\" + PathFindFileNameW(target.c_str()) + L".lnk";
            ShellUtils::CreateShortcut(lnkPath, target, L"", L"");
            CoTaskMemFree(pDesk);
        }
        break;
    }
    case ID_EDIT_UNDO: {
        ShellUtils::ExecuteUndo(m_hwnd);
        if (activeView) {
            activeView->Refresh();
        }
        break;
    }
    case ID_VIEW_LOCK_TOOLBARS:
        Config::Get().lockToolbars = !Config::Get().lockToolbars;
        Config::Get().Save();
        BuildMenu();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        break;
    case ID_VIEW_TB_STANDARD:
        Config::Get().showToolbarStandard = !Config::Get().showToolbarStandard;
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_TB_ADDRESS:
        Config::Get().showToolbarAddress = !Config::Get().showToolbarAddress;
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_TB_SEARCH:
        Config::Get().showToolbarSearch = !Config::Get().showToolbarSearch;
        Config::Get().Save();
        BuildMenu();
        LayoutAll();
        break;
    case ID_VIEW_STATUSBAR:
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
    case ID_VIEW_HIDE_DOTFILES: {
        Config::Get().hideDotFiles = !Config::Get().hideDotFiles;
        Config::Get().Save();
        m_multiPane.SetHideDotFiles(Config::Get().hideDotFiles);
        m_tree.Refresh();
        BuildMenu();
        break;
    }
    case ID_VIEW_ITEM_CHECKBOXES: {
        Config::Get().showItemCheckBoxes = !Config::Get().showItemCheckBoxes;
        Config::Get().Save();
        m_multiPane.ApplyItemCheckBoxes();
        BuildMenu();
        break;
    }
    case ID_VIEW_DENSITY_COMPACT:
    case ID_VIEW_DENSITY_COMFORTABLE:
    case ID_VIEW_DENSITY_SPACIOUS: {
        int d = (cmdId == ID_VIEW_DENSITY_COMPACT) ? 0 :
                (cmdId == ID_VIEW_DENSITY_COMFORTABLE) ? 1 : 2;
        Config::Get().listDensity = d;
        Config::Get().Save();
        m_multiPane.ApplyDensity(d);
        m_tree.ApplyViewMetrics();
        BuildMenu();
        break;
    }
    case ID_VIEW_SCALE_SMALL:
    case ID_VIEW_SCALE_MEDIUM:
    case ID_VIEW_SCALE_LARGE: {
        int s = (cmdId == ID_VIEW_SCALE_SMALL) ? 0 :
                (cmdId == ID_VIEW_SCALE_MEDIUM) ? 1 : 2;
        Config::Get().listScale = s;
        Config::Get().Save();
        m_multiPane.ApplyScale(s);
        m_tree.ApplyViewMetrics();
        BuildMenu();
        break;
    }
    case ID_VIEW_CALC_FOLDER_SIZE: {
        Config::Get().calcFolderSize = !Config::Get().calcFolderSize;
        Config::Get().Save();
        m_multiPane.SetCalcFolderSize(Config::Get().calcFolderSize);
        BuildMenu();
        break;
    }
    case ID_VIEW_MINIMIZE_TRAY:
        ShowWindow(m_hwnd, SW_MINIMIZE);
        break;
    case ID_NAV_UP_NEW_TAB: {
        wchar_t parent[MAX_PATH];
        StringCchCopyW(parent, MAX_PATH, activeView->GetCurrentPath().c_str());
        PathRemoveFileSpecW(parent);
        if (parent[0]) {
            m_tabBar.AddTab(parent, true);
            BuildMenu();
        }
        break;
    }
    case ID_WINDOW_MAXIMIZE:
        m_multiPane.ToggleMaximizePane(m_multiPane.GetActiveIndex());
        break;
    case ID_WINDOW_MINIMIZE:
        m_multiPane.MinimizePane(m_multiPane.GetActiveIndex());
        break;
    case ID_WINDOW_CLOSE:
        m_multiPane.ClosePane(m_multiPane.GetActiveIndex());
        break;
    case ID_WINDOW_MAIN_MAXIMIZE:
        ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        break;
    case ID_WINDOW_MAIN_MINIMIZE:
        ShowWindow(m_hwnd, SW_MINIMIZE);
        break;
    case ID_TOOLS_GESTURES:
        SettingsDialog::Show(m_hwnd, [this]() {
            OnSettingsApplied();
        }, 8);
        break;
    case ID_TOOLS_ALLOW_DUP_TABS:
        Config::Get().allowDuplicateTabs = !Config::Get().allowDuplicateTabs;
        Config::Get().Save();
        BuildMenu();
        break;
    case ID_TOOLS_MIN_TO_TRAY:
        Config::Get().minimizeToTray = !Config::Get().minimizeToTray;
        Config::Get().Save();
        BuildMenu();
        break;
    case ID_TOOLS_AUTO_EXPAND_TREE:
        Config::Get().autoExpandTree = !Config::Get().autoExpandTree;
        Config::Get().Save();
        BuildMenu();
        break;
    case ID_TOOLS_OPEN_SCRIPTS_DIR:
        ShellExecuteW(nullptr, L"open", ScriptEngine::Get().GetScriptsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_TOOLS_FOLDER_OPTIONS:
        ShellExecuteW(nullptr, L"open", L"control.exe", L"folders", nullptr, SW_SHOWNORMAL);
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
        if (CreateDirectoryW(newDir.c_str(), nullptr)) {
            ShellUtils::RecordCreate(newDir);
            activeView->Refresh();
            activeView->SelectItemByPath(newDir);
            activeView->StartRename();
        }
        break;
    }
    case ID_FILE_NEW_MD: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring baseName = isZh ? L"新建 Markdown 文档.md" : L"New Markdown Document.md";
        std::wstring newFile = activeView->GetCurrentPath() + L"\\" + baseName;
        int idx = 2;
        while (PathFileExistsW(newFile.c_str())) {
            newFile = activeView->GetCurrentPath() + (isZh ? (L"\\新建 Markdown 文档 (" + std::to_wstring(idx++) + L").md") : (L"\\New Markdown (" + std::to_wstring(idx++) + L").md"));
        }
        HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hF != INVALID_HANDLE_VALUE) {
            const char* initContent = "# Title\n\n";
            DWORD written = 0;
            WriteFile(hF, initContent, (DWORD)strlen(initContent), &written, nullptr);
            CloseHandle(hF);
            ShellUtils::RecordCreate(newFile);
            activeView->Refresh();
            activeView->SelectItemByPath(newFile);
            activeView->StartRename();
        }
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
    case ID_FILE_NEW_FILE: {
        std::wstring newFile = activeView->GetCurrentPath() + L"\\" + I18n::Get().T(StringId::NewTextFile);
        int idx = 2;
        while (PathFileExistsW(newFile.c_str())) {
            newFile = activeView->GetCurrentPath() + L"\\New (" + std::to_wstring(idx++) + L").txt";
        }
        HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hF != INVALID_HANDLE_VALUE) {
            CloseHandle(hF);
            ShellUtils::RecordCreate(newFile);
            activeView->Refresh();
            activeView->SelectItemByPath(newFile);
            activeView->StartRename();
        }
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
        m_forceClose = true;
        SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
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

    case ID_VIEW_SORT_FOLDERS_FIRST:
        if (activeView) {
            activeView->SetFoldersFirst(true);
        } else {
            Config::Get().foldersFirst = true;
            Config::Get().Save();
        }
        BuildMenu();
        break;

    case ID_VIEW_SORT_FILES_FIRST:
        if (activeView) {
            activeView->SetFoldersFirst(false);
        } else {
            Config::Get().foldersFirst = false;
            Config::Get().Save();
        }
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
        BuildMenu();
        DrawMenuBar(m_hwnd);
        break;
    case ID_TOOLS_SCRIPT_CONSOLE:
        ScriptEngine::Get().ShowConsole();
        break;
    case ID_TOOLS_RESTORE_EXPLORER: {
        ShellUtils::SetExplorerReplaced(false);
        bool isZh = I18n::Get().IsChinese();
        MessageBoxW(m_hwnd,
            isZh ? L"已成功恢复系统默认资源管理器关联。\nWindows 将恢复使用原生资源管理器打开文件夹。"
                 : L"Windows Explorer default associations restored successfully.",
            isZh ? L"恢复默认关联" : L"Restore Explorer Defaults",
            MB_OK | MB_ICONINFORMATION);
        break;
    }
    case ID_TOOLS_SETTINGS:
        SettingsDialog::Show(m_hwnd, [this]() {
            OnSettingsApplied();
        });
        break;

    case ID_TOOLS_SHORTCUTS_SETTINGS:
    case ID_HELP_SHORTCUTS:
        SettingsDialog::Show(m_hwnd, [this]() {
            OnSettingsApplied();
        }, 7);
        break;
    case ID_HELP_GESTURES:
        SettingsDialog::Show(m_hwnd, [this]() {
            OnSettingsApplied();
        }, 8);
        break;
    case ID_HELP_CHECK_UPDATES: {
        ShellUtils::CheckForUpdates(m_hwnd);
        break;
    }
    case ID_HELP_GITHUB:
        ShellExecuteW(nullptr, L"open", L"https://github.com/harry10086/NeoMDIE", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_VIEW_DARK_MODE: {
        Theme::Get().ToggleTheme();
        Config::Get().themeMode = Theme::Get().IsDarkMode() ? 1 : 0;
        DrawMenuBar(m_hwnd);
        Config::Get().Save();
        ApplyTheme();
        BuildMenu();
        break;
    }

    case ID_HELP_ABOUT: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring aboutText = isZh ?
            L"NeoMDIE - 现代化多标签多栏文件管理器\n"
            L"版本: v1.1.1 (x64 Release)\n\n"
            L"作者: Harry (https://mianao.info)\n"
            L"开源项目仓库: https://github.com/harry10086/NeoMDIE\n"
            L"致敬经典: MDIE(Multiple Document Interface Explorer)\n\n"
            L"基于 GNU General Public License v3 (GPLv3) 开源协议发布。" :
            L"NeoMDIE - Modern Native Win32 Tabbed & Multi-Pane File Manager\n"
            L"Version: v1.1.1 (x64 Release)\n\n"
            L"Author: Harry (https://mianao.info)\n"
            L"Repository: https://github.com/harry10086/NeoMDIE\n"
            L"Inspired by: MDIE(Multiple Document Interface Explorer)\n\n"
            L"Pure C++17 / Native Win32 / Shell COM. Zero dependencies.\n"
            L"Licensed under the GNU General Public License v3 (GPLv3).";
        MessageBoxW(m_hwnd, aboutText.c_str(), isZh ? L"关于 NeoMDIE" : L"About NeoMDIE", MB_OK | MB_ICONINFORMATION);
        break;
    }

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
        } else if (cmdId >= ID_RECENT_CLOSED_START && cmdId <= ID_RECENT_CLOSED_END) {
            int cIdx = (int)(cmdId - ID_RECENT_CLOSED_START);
            const auto& cHist = m_tabBar.GetClosedHistory();
            if (cIdx >= 0 && cIdx < (int)cHist.size()) {
                m_tabBar.AddTab(cHist[cHist.size() - 1 - cIdx], true);
                BuildMenu();
            }
        } else if (cmdId >= ID_SCRIPT_COMMAND_START && cmdId <= ID_SCRIPT_COMMAND_END) {
            bool ok = ScriptEngine::Get().ExecuteCommand(cmdId, activeView ? activeView->GetCurrentPath() : L"", activeView ? activeView->GetSelectedPaths() : std::vector<std::wstring>());
            if (ok && activeView) {
                activeView->Refresh();
            }
        }
        break;
    }
}


void MainWindow::DrawToolbarButton(DRAWITEMSTRUCT* pdis) {
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

    // Background surface fill: TabBar background for menu button, toolbar surface for others
    HBRUSH hbrBg = (btnId == IDC_BTN_MENU) ? Theme::Get().BrushBackground() : Theme::Get().BrushSurface();
    FillRect(hdc, &rc, hbrBg);

    // Modern antialiased rounded rectangle on hover / selected / active
    int rad = Theme::Get().Scale(6, pdis->hwndItem);
    RECT rcBtn = { rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2 };
    if (btnId == IDC_BTN_MENU) {
        if (isSelected) {
            Theme::Get().FillSmoothRoundRect(hdc, rcBtn, rad, Theme::Get().ClrSelected());
        } else if (isHot) {
            Theme::Get().FillSmoothRoundRect(hdc, rcBtn, rad, Theme::Get().ClrHover());
        }
    } else {
        if (isSelected) {
            Theme::Get().DrawSmoothRoundRect(hdc, rcBtn, rad, Theme::Get().ClrSelected(), Theme::Get().ClrBorder(), 1.0f);
        } else if (isHot || isActiveToggle) {
            COLORREF bg = isActiveToggle ? Theme::Get().ClrSelected() : Theme::Get().ClrHover();
            COLORREF border = isActiveToggle ? Theme::Get().ClrAccent() : Theme::Get().ClrBorder();
            Theme::Get().DrawSmoothRoundRect(hdc, rcBtn, rad, bg, border, 1.0f);
        }
    }

    // Windows 11 Fluent Icons glyph
    wchar_t glyph[2] = { 0, 0 };
    switch (btnId) {
    case IDC_BTN_BACK:    glyph[0] = 0xE72B; break; // Back
    case IDC_BTN_FORWARD: glyph[0] = 0xE72A; break; // Forward
    case IDC_BTN_UP:      glyph[0] = 0xE74A; break; // Up
    case IDC_BTN_REFRESH: glyph[0] = 0xE72C; break; // Refresh
    case IDC_BTN_TREE:    glyph[0] = 0xE8A9; break; // Navigation / Tree
    case IDC_BTN_SPLIT:   glyph[0] = 0xE784; break; // Split View
    case IDC_BTN_PREVIEW: glyph[0] = 0xE8FF; break; // Preview Pane
    case IDC_BTN_MENU:    glyph[0] = 0xE700; break; // Hamburger Menu (≡)
    default: return;
    }

    COLORREF iconColor = Theme::Get().ClrTextPrimary();
    if (isDisabled) {
        iconColor = Theme::Get().ClrTextSecondary();
    } else if (btnId != IDC_BTN_MENU && (isActiveToggle || isHot)) {
        iconColor = Theme::Get().ClrAccent();
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, iconColor);

    int iconPixelHeight = 15;
    HFONT hIconFont = Theme::Get().GetIconFont(iconPixelHeight, m_hwnd);
    HGDIOBJ oldFont = SelectObject(hdc, hIconFont);

    RECT rcGlyph = rc;
    if (btnId == IDC_BTN_MENU) {
        rcGlyph.top += Theme::Get().Scale(2, m_hwnd);
    }
    DrawTextW(hdc, glyph, 1, &rcGlyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hIconFont);
}
LRESULT MainWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DEVICECHANGE: {
        // 0x8000: DBT_DEVICEARRIVAL, 0x8004: DBT_DEVICEREMOVECOMPLETE
        if (wParam == 0x8000 || wParam == 0x8004) {
            ShellUtils::ClearDriveIconCache();
            m_tree.Refresh();
            for (int i = 0; i < m_multiPane.GetPaneCount(); ++i) {
                FileListView& pane = m_multiPane.GetPane(i);
                std::wstring p = pane.GetCurrentPath();
                if (p == L"This PC" || p == L"此电脑" || p == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
                    pane.Refresh();
                }
            }
        }
        break;
    }
    case WM_COPYDATA: {
        PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
        if (pcds && pcds->lpData && pcds->cbData > 0) {
            std::wstring path((const wchar_t*)pcds->lpData);
            while (!path.empty() && (path.back() == L'"' || path.back() == L' ')) path.pop_back();
            while (!path.empty() && path.front() == L'"') path.erase(path.begin());
            if (path.length() == 2 && path[1] == L':') path += L"\\";
            if (path.find(L"645FF040-5081-101B-9F08-00AA002F954E") != std::wstring::npos ||
                path.find(L"RecycleBinFolder") != std::wstring::npos) {
                ShellExecuteW(nullptr, L"open", L"explorer.exe", L"::{645FF040-5081-101B-9F08-00AA002F954E}", nullptr, SW_SHOWNORMAL);
                return 0;
            }
            if (path.find(L"20D04FE0-3AEA-1069-A2D8-08002B30309D") != std::wstring::npos) {
                path = L"This PC";
            }
            if (ShellUtils::IsValidDirectory(path) || path == L"此电脑" || path == L"This PC") {
                m_tabBar.AddTab(path, true);
            }
            RestoreFromTrayOrMinimized();
            return TRUE;
        }
        return FALSE;
    }
    case WM_ACTIVATE_INSTANCE: {
        RestoreFromTrayOrMinimized();
        return 0;
    }
    case WM_NCCALCSIZE: {
        if (wParam == TRUE && !Config::Get().showMenuBar) {
            if (IsZoomed(hwnd)) {
                NCCALCSIZE_PARAMS* pParams = (NCCALCSIZE_PARAMS*)lParam;
                int borderX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                int borderY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                pParams->rgrc[0].left += borderX;
                pParams->rgrc[0].top += borderY;
                pParams->rgrc[0].right -= borderX;
                pParams->rgrc[0].bottom -= borderY;
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_NCHITTEST: {
        if (!Config::Get().showMenuBar && !IsZoomed(hwnd)) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            int border = GetSystemMetrics(SM_CXPADDEDBORDER) + GetSystemMetrics(SM_CXSIZEFRAME);
            if (pt.y < rcWindow.top + border) {
                if (pt.x < rcWindow.left + border * 2) return HTTOPLEFT;
                if (pt.x > rcWindow.right - border * 2) return HTTOPRIGHT;
                return HTTOP;
            }
            if (pt.y > rcWindow.bottom - border) {
                if (pt.x < rcWindow.left + border * 2) return HTBOTTOMLEFT;
                if (pt.x > rcWindow.right - border * 2) return HTBOTTOMRIGHT;
                return HTBOTTOM;
            }
            if (pt.x < rcWindow.left + border) return HTLEFT;
            if (pt.x > rcWindow.right - border) return HTRIGHT;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_NCPAINT:
    case WM_NCACTIVATE: {
        LRESULT res = DefWindowProcW(hwnd, msg, wParam, lParam);
        if (Theme::Get().IsDarkMode() && Config::Get().showMenuBar) {
            MENUBARINFO mbi = { sizeof(mbi) };
            if (GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) {
                RECT rcClient = { 0 };
                GetClientRect(hwnd, &rcClient);
                MapWindowPoints(hwnd, nullptr, (POINT*)&rcClient, 2);

                RECT rcWindow = { 0 };
                GetWindowRect(hwnd, &rcWindow);

                OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

                // 1-pixel separator line right above client area to cover legacy light border
                RECT rcSepLine = rcClient;
                rcSepLine.bottom = rcSepLine.top;
                rcSepLine.top--;

                HDC hdc = GetWindowDC(hwnd);
                if (hdc) {
                    FillRect(hdc, &rcSepLine, Theme::Get().BrushBackground());
                    ReleaseDC(hwnd, hdc);
                }
            }
        }
        return res;
    }
    case WM_UAHDRAWMENU: { // 0x0091: Dark mode menu bar background
        if (Theme::Get().IsDarkMode()) {
            UAHMENU* pUDM = (UAHMENU*)lParam;
            MENUBARINFO mbi = { sizeof(mbi) };
            if (GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) {
                RECT rcWindow;
                GetWindowRect(hwnd, &rcWindow);
                RECT rcBar = mbi.rcBar;
                OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
                RECT rcFill = rcBar;
                rcFill.left = 0;
                rcFill.right = rcWindow.right - rcWindow.left;
                FillRect(pUDM->hdc, &rcFill, Theme::Get().BrushBackground());
                return 0;
            }
        }
        break;
    }
    case WM_UAHDRAWMENUITEM: { // 0x0092: Dark mode menu bar items
        if (Theme::Get().IsDarkMode()) {
            UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)lParam;
            HDC hdc = pUDMI->um.hdc;
            RECT rc = pUDMI->dis.rcItem;

            bool isHot = (pUDMI->dis.itemState & (ODS_HOTLIGHT | ODS_SELECTED)) != 0;
            bool isSelected = (pUDMI->dis.itemState & ODS_SELECTED) != 0;
            bool isDisabled = (pUDMI->dis.itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;

            HBRUSH hbrItem = isSelected ? Theme::Get().BrushSelected() :
                             (isHot ? Theme::Get().BrushHover() : Theme::Get().BrushBackground());
            FillRect(hdc, &rc, hbrItem);

            wchar_t menuString[256] = { 0 };
            MENUITEMINFO mii = { sizeof(mii), MIIM_STRING };
            mii.dwTypeData = menuString;
            mii.cch = 255;
            GetMenuItemInfoW(pUDMI->um.hmenu, pUDMI->umi.iPosition, TRUE, &mii);

            DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
            if (pUDMI->dis.itemState & ODS_NOACCEL) {
                dwFlags |= DT_HIDEPREFIX;
            }

            COLORREF textColor = isDisabled ? Theme::Get().ClrTextSecondary() :
                                 (isSelected ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary());
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textColor);
            HFONT hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);
            DrawTextW(hdc, menuString, -1, &rc, dwFlags);
            SelectObject(hdc, oldFont);
            return 0;
        }
        break;
    }
    case WM_UAHMEASUREMENUITEM: { // 0x0094
        if (Theme::Get().IsDarkMode()) {
            UAHMEASUREMENUITEM* pMmi = (UAHMEASUREMENUITEM*)lParam;
            LRESULT lr = DefWindowProcW(hwnd, msg, wParam, lParam);
            pMmi->mis.itemWidth = (pMmi->mis.itemWidth * 5) / 4;
            return lr;
        }
        break;
    }
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) < 0);
        bool shift = (GetKeyState(VK_SHIFT) < 0);
        bool alt = (GetKeyState(VK_MENU) < 0);

        if (wParam == VK_F7 && !ctrl && !shift && !alt) {
            for (const auto& s : ScriptEngine::Get().GetScripts()) {
                if (s.hotkey == L"F7") {
                    OnCommand(s.commandId);
                    return 0;
                }
            }
        }
        if ((wParam == 'E' || wParam == 'e') && ctrl && alt && !shift) {
            for (const auto& s : ScriptEngine::Get().GetScripts()) {
                if (s.hotkey == L"Ctrl+Alt+E") {
                    OnCommand(s.commandId);
                    return 0;
                }
            }
        }
        break;
    }

    case WM_PAINT: {
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
        if (!Config::Get().lockToolbars && (PtInRect(&m_rcFilterSplitter, pt) || m_resizingFilterBar)) {
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
        if (!Config::Get().lockToolbars && PtInRect(&m_rcFilterSplitter, pt)) {
            m_resizingFilterBar = true;
            SetCapture(hwnd);
            return 0;
        }
        break;
    }

    case WM_XBUTTONUP: {
        WORD btn = GET_XBUTTON_WPARAM(wParam);
        if (btn == XBUTTON1) {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->GoBack();
            return TRUE;
        } else if (btn == XBUTTON2) {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->GoForward();
            return TRUE;
        }
        break;
    }

    case WM_APPCOMMAND: {
        int cmd = GET_APPCOMMAND_LPARAM(lParam);
        if (cmd == APPCOMMAND_BROWSER_BACKWARD) {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->GoBack();
            return TRUE;
        } else if (cmd == APPCOMMAND_BROWSER_FORWARD) {
            FileListView* activeView = m_multiPane.GetActivePane();
            if (activeView) activeView->GoForward();
            return TRUE;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (m_resizingTree && (wParam & MK_LBUTTON)) {
            int minTreeW = Theme::Get().Scale(180, hwnd);
            int newW = (pt.x < minTreeW) ? minTreeW : pt.x;
            if (newW != m_treeWidth) {
                m_treeWidth = newW;
                LayoutCenterArea();
            }
            return 0;
        }
        if (m_resizingPreview && (wParam & MK_LBUTTON)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int minListW = (m_multiPane.GetPaneCount() > 1) ? Theme::Get().Scale(350, hwnd) : Theme::Get().Scale(250, hwnd);
            int newW = rc.right - pt.x;
            int maxPreviewW = (rc.right - rc.left) - minListW - Theme::Get().Scale(20, hwnd);
            if (newW > maxPreviewW) newW = maxPreviewW;
            if (newW < Theme::Get().Scale(120, hwnd)) newW = Theme::Get().Scale(120, hwnd);
            if (newW != m_previewWidth) {
                m_previewWidth = newW;
                LayoutCenterArea();
            }
            return 0;
        }
        if (m_resizingFilterBar && (wParam & MK_LBUTTON)) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int totalW = rcClient.right - rcClient.left;
            int rightMargin = Theme::Get().Scale(8, hwnd);

            int newFilterW = 0;
            if (Config::Get().searchBarFirst) {
                int btnW = Theme::Get().Scale(36, hwnd);
                int spacing = Theme::Get().Scale(3, hwnd);
                int filterStartX = Theme::Get().Scale(6, hwnd) + (btnW + spacing) * 7 + spacing;
                newFilterW = pt.x - filterStartX;
            } else {
                newFilterW = (totalW - rightMargin) - pt.x;
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
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (m_resizingTree) {
            m_resizingTree = false;
            ReleaseCapture();
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return 0;
        }
        if (m_resizingPreview) {
            m_resizingPreview = false;
            ReleaseCapture();
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            Config::Get().previewWidth = m_previewWidth;
            Config::Get().Save();
            return 0;
        }
        if (m_resizingFilterBar) {
            m_resizingFilterBar = false;
            ReleaseCapture();
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            Config::Get().Save();
            return 0;
        }
        break;
    }

    case WM_SIZE:
        LayoutAll();
        return 0;

    case WM_SYSCOMMAND: {
        UINT cmd = (wParam & 0xFFF0);
        if (cmd == SC_MINIMIZE) {
            m_wasMaximizedBeforeTray = (IsZoomed(hwnd) != FALSE);
            if (Config::Get().minimizeToTray) {
                AddTrayIcon();
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
        }
        if (cmd == SC_RESTORE && IsZoomed(hwnd)) {
            WINDOWPLACEMENT wp = { sizeof(wp) };
            if (GetWindowPlacement(hwnd, &wp)) {
                HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(mi) };
                if (GetMonitorInfoW(hMon, &mi)) {
                    int monW = mi.rcWork.right - mi.rcWork.left;
                    int monH = mi.rcWork.bottom - mi.rcWork.top;
                    int normW = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                    int normH = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
                    if (normW >= monW || normH >= monH || normW <= 100 || normH <= 100) {
                        int targetW = (monW * 4) / 5;
                        int targetH = (monH * 4) / 5;
                        wp.rcNormalPosition.left = mi.rcWork.left + (monW - targetW) / 2;
                        wp.rcNormalPosition.top = mi.rcWork.top + (monH - targetH) / 2;
                        wp.rcNormalPosition.right = wp.rcNormalPosition.left + targetW;
                        wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + targetH;
                        SetWindowPlacement(hwnd, &wp);
                    }
                }
            }
        }
        break;
    }

    case WM_TRAY_NOTIFY: {
        UINT uMsg = LOWORD(lParam);
        if (uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK) {
            RestoreFromTrayOrMinimized();
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
                RestoreFromTrayOrMinimized();
            } else if (cmd == 5002) {
                m_forceClose = true;
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
            }
        }
        return 0;
    }

    case WM_SYNC_TREE_SELECTION: {
        if (m_treeVisible && Config::Get().autoExpandTree && !m_pendingTreePath.empty()) {
            std::wstring syncPath = m_pendingTreePath;
            m_pendingTreePath.clear();
            m_tree.SelectPath(syncPath);
        }
        return 0;
    }

    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlType == ODT_BUTTON) {
            DrawToolbarButton(pdis);
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

    case WM_CTLCOLOREDIT:
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
        if (!m_forceClose && Config::Get().closeToTray) {
            m_wasMaximizedBeforeTray = (IsZoomed(hwnd) != FALSE);
            AddTrayIcon();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        bool wasHiddenInTray = m_hasTrayIcon || !IsWindowVisible(hwnd);
        bool wasMaximized = wasHiddenInTray ? m_wasMaximizedBeforeTray : (IsZoomed(hwnd) != FALSE);

        Config& cfg = Config::Get();
        WINDOWPLACEMENT wp = { sizeof(wp) };
        if (GetWindowPlacement(hwnd, &wp)) {
            cfg.window.x = wp.rcNormalPosition.left;
            cfg.window.y = wp.rcNormalPosition.top;
            cfg.window.width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            cfg.window.height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            cfg.window.maximized = wasMaximized;
        }

        ShowWindow(hwnd, SW_HIDE);
        RemoveTrayIcon();

        cfg.showTree = m_treeVisible;
        cfg.treeWidth = m_treeWidth;
        cfg.previewEnabled = m_previewVisible;
        cfg.previewWidth = m_previewWidth;

        // Save column widths from all panes
        for (int p = 0; p < m_multiPane.GetPaneCount(); ++p) {
            m_multiPane.GetPane(p).SaveColumnWidths();
        }

        // Sync active tab multiPane state into TabItem before saving
        int curTab = m_tabBar.GetActiveIndex();
        if (curTab >= 0 && curTab < m_tabBar.GetTabCount()) {
            auto& activeTab = m_tabBar.GetTabItemRef(curTab);
            activeTab.layout = (int)m_multiPane.GetLayout();
            activeTab.activePaneIndex = m_multiPane.GetActiveIndex();
            activeTab.panePaths.clear();
            activeTab.paneViewStates.clear();
            int pCount = m_multiPane.GetPaneCount();
            for (int p = 0; p < pCount; ++p) {
                std::wstring pPath = m_multiPane.GetPane(p).GetCurrentPath();
                if (pPath.empty()) pPath = (p == 0 && !activeTab.path.empty()) ? activeTab.path : L"This PC";
                activeTab.panePaths.push_back(pPath);
                activeTab.paneViewStates.push_back(m_multiPane.GetPane(p).GetViewState());
            }
            if (!activeTab.panePaths.empty()) {
                int aIdx = std::clamp(activeTab.activePaneIndex, 0, (int)activeTab.panePaths.size() - 1);
                activeTab.path = activeTab.panePaths[aIdx];
            }
        }

        cfg.sessionTabsInfo.clear();
        cfg.sessionTabs.clear();
        for (int i = 0; i < m_tabBar.GetTabCount(); ++i) {
            const auto& item = m_tabBar.GetTabItem(i);
            Config::SessionTabInfo info;
            info.path = item.path;
            info.layout = item.layout;
            info.activePaneIndex = item.activePaneIndex;
            info.panePaths = item.panePaths;
            info.paneViewStates = item.paneViewStates;
            if (info.panePaths.empty() && !info.path.empty()) {
                info.panePaths.push_back(info.path);
            }
            cfg.sessionTabsInfo.push_back(info);
            cfg.sessionTabs.push_back(item.path);
        }
        cfg.activeTabIndex = m_tabBar.GetActiveIndex();
        cfg.Save();

        DestroyWindow(hwnd);
        return 0;
    }

    case WM_CLIPBOARDUPDATE: {
        if (m_multiPane.GetHwnd()) {
            for (int i = 0; i < 4; ++i) {
                HWND hPane = m_multiPane.GetPane(i).GetHwnd();
                if (hPane && IsWindow(hPane)) {
                    InvalidateRect(hPane, nullptr, TRUE);
                }
            }
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveClipboardFormatListener(m_hwnd);
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

void MainWindow::RebuildAccelerators() {
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }

    std::vector<ACCEL> accels;

    // Load from Config::Get().shortcuts
    const auto& shortcuts = Config::Get().shortcuts;

    auto mapKey = [](const std::wstring& k) -> WORD {
        if (k.length() == 1) {
            wchar_t ch = k[0];
            if (ch >= L'a' && ch <= L'z') return (WORD)(ch - L'a' + 'A');
            if (ch >= L'A' && ch <= L'Z') return (WORD)ch;
            if (ch >= L'0' && ch <= L'9') return (WORD)ch;
        }
        if (k == L"Tab") return VK_TAB;
        if (k == L"Space") return VK_SPACE;
        if (k == L"Enter") return VK_RETURN;
        if (k == L"Esc") return VK_ESCAPE;
        if (k == L"Backspace") return VK_BACK;
        if (k == L"Delete") return VK_DELETE;
        if (k == L"Insert") return VK_INSERT;
        if (k == L"Home") return VK_HOME;
        if (k == L"End") return VK_END;
        if (k == L"PageUp") return VK_PRIOR;
        if (k == L"PageDown") return VK_NEXT;
        if (k == L"Left") return VK_LEFT;
        if (k == L"Right") return VK_RIGHT;
        if (k == L"Up") return VK_UP;
        if (k == L"Down") return VK_DOWN;
        if (k == L"F1") return VK_F1;
        if (k == L"F2") return VK_F2;
        if (k == L"F3") return VK_F3;
        if (k == L"F4") return VK_F4;
        if (k == L"F5") return VK_F5;
        if (k == L"F6") return VK_F6;
        if (k == L"F7") return VK_F7;
        if (k == L"F8") return VK_F8;
        if (k == L"F9") return VK_F9;
        if (k == L"F10") return VK_F10;
        if (k == L"F11") return VK_F11;
        if (k == L"F12") return VK_F12;
        return 0;
    };

    auto mapAction = [](const std::wstring& name) -> UINT {
        if (name == L"新建标签页" || name == L"New Tab") return ID_FILE_NEW_TAB;
        if (name == L"关闭当前标签页" || name == L"Close Current Tab") return ID_FILE_CLOSE_TAB;
        if (name == L"切换到下一个标签页" || name == L"Next Tab") return ID_TAB_NEXT;
        if (name == L"切换到上一个标签页" || name == L"Previous Tab") return ID_TAB_PREV;
        if (name == L"新建窗口" || name == L"New Window") return ID_FILE_NEW_WINDOW_CURRENT;
        if (name == L"后退" || name == L"Back") return ID_NAV_BACK;
        if (name == L"前进" || name == L"Forward") return ID_NAV_FORWARD;
        if (name == L"向上一级" || name == L"Up to Parent") return ID_NAV_UP;
        if (name == L"刷新" || name == L"Refresh") return ID_VIEW_REFRESH;
        if (name == L"聚焦到地址栏" || name == L"Focus Address Bar") return ID_VIEW_ADDRESS_FOCUS;
        if (name == L"聚焦到搜索过滤栏" || name == L"Focus Filter Bar") return ID_VIEW_FILTER_FOCUS;
        if (name == L"最大化/还原当前栏" || name == L"Maximize/Restore Pane") return ID_WINDOW_MAXIMIZE;
        if (name == L"单栏视图" || name == L"单栏模式" || name == L"Single Pane") return ID_VIEW_LAYOUT_1X1;
        if (name == L"垂直双栏" || name == L"左右双栏" || name == L"Dual Vertical Panes") return ID_VIEW_LAYOUT_1X2;
        if (name == L"水平双栏" || name == L"上下双栏" || name == L"Dual Horizontal Panes") return ID_VIEW_LAYOUT_2X1;
        if (name == L"四分栏网格" || name == L"四分栏模式" || name == L"Quad Panes") return ID_VIEW_LAYOUT_2X2;
        if (name == L"滚动到列表顶部" || name == L"Scroll to Top") return ID_SCROLL_TOP;
        if (name == L"滚动到列表底部" || name == L"Scroll to Bottom") return ID_SCROLL_BOTTOM;
        if (name == L"向上翻页" || name == L"Page Up") return ID_SCROLL_PAGEUP;
        if (name == L"向下翻页" || name == L"Page Down") return ID_SCROLL_PAGEDOWN;
        if (name == L"全选" || name == L"Select All") return ID_EDIT_SELECT_ALL;
        if (name == L"复制选中项" || name == L"复制" || name == L"Copy") return ID_EDIT_COPY;
        if (name == L"剪切选中项" || name == L"剪切" || name == L"Cut") return ID_EDIT_CUT;
        if (name == L"粘贴" || name == L"Paste") return ID_EDIT_PASTE;
        if (name == L"新建文件夹" || name == L"New Folder") return ID_FILE_NEW_FOLDER;
        if (name == L"属性" || name == L"Properties") return ID_FILE_PROPERTIES;
        if (name == L"新建Markdown文档" || name == L"新建Markdown文件" || name == L"新建markdown文件" || name == L"New Markdown Document" || name == L"New Markdown File") return ID_FILE_NEW_MD;
        if (name == L"显示/隐藏隐藏文件" || name == L"Toggle Hidden Files") return ID_VIEW_SHOW_HIDDEN;
        if (name == L"隐藏/显示点文件" || name == L"Toggle Dotfiles" || name == L"隐藏点文件") return ID_VIEW_HIDE_DOTFILES;
        if (name == L"显示/隐藏项目复选框" || name == L"项目复选框" || name == L"Toggle Item Checkboxes") return ID_VIEW_ITEM_CHECKBOXES;
        if (name == L"计算文件夹大小" || name == L"自动计算文件夹大小" || name == L"Calculate Folder Size") return ID_VIEW_CALC_FOLDER_SIZE;
        if (name == L"切换深色/浅色模式" || name == L"切换主题模式" || name == L"Toggle Dark Mode") return ID_VIEW_DARK_MODE;
        if (name == L"显示/隐藏传统菜单栏" || name == L"显示传统明文菜单栏" || name == L"菜单栏" || name == L"Toggle Menu Bar") return ID_VIEW_TOGGLE_MENUBAR;
        if (name == L"选项与设置" || name == L"Options & Settings") return ID_TOOLS_SETTINGS;
        if (name == L"反向选择" || name == L"Invert Selection") return ID_EDIT_INVERT_SELECTION;
        if (name == L"撤销" || name == L"Undo") return ID_EDIT_UNDO;
        if (name == L"删除选中项" || name == L"删除" || name == L"Delete") return ID_EDIT_DELETE;
        if (name == L"重命名" || name == L"Rename") return ID_EDIT_RENAME;
        if (name == L"复制当前路径" || name == L"Copy Path") return ID_EDIT_COPY_PATH;
        if (name == L"显示/隐藏侧边栏目录树" || name == L"Toggle Tree Sidebar") return ID_VIEW_TREE_TOGGLE;
        if (name == L"显示/隐藏快速预览窗格" || name == L"Toggle Preview Pane") return ID_VIEW_PREVIEW_TOGGLE;
        if (name == L"打开命令行 CMD" || name == L"Open Command Prompt CMD") return ID_TOOLS_OPEN_CMD;
        for (const auto& s : ScriptEngine::Get().GetScripts()) {
            std::wstring sZh = L"【脚本】" + s.name;
            std::wstring sEn = L"[Script] " + s.name;
            if (name == sZh || name == sEn || name == s.name) {
                return s.commandId;
            }
        }
        return 0;
    };

    // 1. First register script hotkeys directly from script metadata (always up-to-date)
    for (const auto& s : ScriptEngine::Get().GetScripts()) {
        if (!s.hotkey.empty()) {
            std::wstring hk = s.hotkey;
            bool ctrl = false, shift = false, alt = false;
            size_t plusPos;
            while ((plusPos = hk.find(L"+")) != std::wstring::npos) {
                std::wstring mod = hk.substr(0, plusPos);
                while (!mod.empty() && mod.front() == L' ') mod.erase(mod.begin());
                while (!mod.empty() && mod.back() == L' ') mod.pop_back();
                if (_wcsicmp(mod.c_str(), L"Ctrl") == 0) ctrl = true;
                else if (_wcsicmp(mod.c_str(), L"Shift") == 0) shift = true;
                else if (_wcsicmp(mod.c_str(), L"Alt") == 0) alt = true;
                hk = hk.substr(plusPos + 1);
            }
            while (!hk.empty() && hk.front() == L' ') hk.erase(hk.begin());
            while (!hk.empty() && hk.back() == L' ') hk.pop_back();

            WORD vk = mapKey(hk);
            if (vk != 0) {
                ACCEL acc = { 0 };
                acc.fVirt = FVIRTKEY;
                if (ctrl) acc.fVirt |= FCONTROL;
                if (shift) acc.fVirt |= FSHIFT;
                if (alt) acc.fVirt |= FALT;
                acc.key = vk;
                acc.cmd = (WORD)s.commandId;
                accels.push_back(acc);
            }
        }
    }

    auto hasCommand = [&](UINT cmd) {
        for (const auto& a : accels) {
            if (a.cmd == (WORD)cmd) return true;
        }
        return false;
    };

    // 2. Register other shortcuts from Config
    for (const auto& item : shortcuts) {
        WORD vk = mapKey(item.key);
        UINT cmd = mapAction(item.name);
        if (vk != 0 && cmd != 0 && !hasCommand(cmd)) {
            ACCEL acc = { 0 };
            acc.fVirt = FVIRTKEY;
            if (item.ctrl) acc.fVirt |= FCONTROL;
            if (item.shift) acc.fVirt |= FSHIFT;
            if (item.alt) acc.fVirt |= FALT;
            acc.key = vk;
            acc.cmd = (WORD)cmd;
            accels.push_back(acc);
        }
    }

    // Guarantee critical default hotkeys (F5, F11, Alt+Enter, Ctrl+A, Ctrl+I, Ctrl+Z) are always registered
    if (!hasCommand(ID_VIEW_REFRESH)) {
        accels.push_back({ FVIRTKEY, (WORD)VK_F5, (WORD)ID_VIEW_REFRESH });
    }
    if (!hasCommand(ID_WINDOW_MAXIMIZE)) {
        accels.push_back({ FVIRTKEY, (WORD)VK_F11, (WORD)ID_WINDOW_MAXIMIZE });
    }
    if (!hasCommand(ID_FILE_PROPERTIES)) {
        accels.push_back({ FVIRTKEY | FALT, (WORD)VK_RETURN, (WORD)ID_FILE_PROPERTIES });
    }
    if (!hasCommand(ID_EDIT_SELECT_ALL)) {
        accels.push_back({ FVIRTKEY | FCONTROL, (WORD)'A', (WORD)ID_EDIT_SELECT_ALL });
    }
    if (!hasCommand(ID_EDIT_INVERT_SELECTION)) {
        accels.push_back({ FVIRTKEY | FCONTROL, (WORD)'I', (WORD)ID_EDIT_INVERT_SELECTION });
    }
    if (!hasCommand(ID_EDIT_UNDO)) {
        accels.push_back({ FVIRTKEY | FCONTROL, (WORD)'Z', (WORD)ID_EDIT_UNDO });
    }
    if (!hasCommand(ID_VIEW_HIDE_DOTFILES)) {
        accels.push_back({ FVIRTKEY | FCONTROL, (WORD)'H', (WORD)ID_VIEW_HIDE_DOTFILES });
    }
    if (!hasCommand(IDC_BTN_MENU)) {
        accels.push_back({ FVIRTKEY | FALT, (WORD)'M', (WORD)IDC_BTN_MENU });
    }

    if (!accels.empty()) {
        m_hAccel = CreateAcceleratorTableW(accels.data(), (int)accels.size());
    } else {
        m_hAccel = LoadAcceleratorsW(m_hInstance, MAKEINTRESOURCEW(IDR_MAIN_ACCEL));
    }
}
