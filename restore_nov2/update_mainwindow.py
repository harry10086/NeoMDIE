import os

with open(r'd:\github\NeoMDIE\src\ui\MainWindow.cpp', 'r', encoding='utf-8') as f:
    s = f.read()

# 1. Replace BuildMenu()
start_marker = 'void MainWindow::BuildMenu() {'
end_marker = 'void MainWindow::CreateControls() {'

start_idx = s.find(start_marker)
end_idx = s.find(end_marker)

new_build_menu = '''void MainWindow::BuildMenu() {
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
                isZh ? L"打开最后关闭的文件夹(&L)\\tCtrl+Shift+T" : L"Reopen Last Closed Tab(&L)\\tCtrl+Shift+T");
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

    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_NEW_TAB, isZh ? L"新建标签页(&T)\\tCtrl+T" : L"New Tab(&T)\\tCtrl+T");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CLOSE_TAB, isZh ? L"关闭当前标签(&C)\\tCtrl+W" : L"Close Tab(&C)\\tCtrl+W");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hFileMenu, MF_STRING, ID_EDIT_DELETE, isZh ? L"删除(&D)\\tDelete" : L"Delete(&D)\\tDelete");
    AppendMenuW(hFileMenu, MF_STRING, ID_EDIT_RENAME, isZh ? L"重命名(&R)\\tF2" : L"Rename(&R)\\tF2");

    HMENU hNewSubmenu = CreatePopupMenu();
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FOLDER, isZh ? L"文件夹(&F)\\tCtrl+Shift+N" : L"Folder(&F)\\tCtrl+Shift+N");
    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FILE, isZh ? L"文本文档(&T)" : L"Text Document(&T)");
    AppendMenuW(hFileMenu, MF_POPUP, (UINT_PTR)hNewSubmenu, isZh ? L"新建(&W)" : L"New(&W)");

    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CREATE_SHORTCUT, isZh ? L"创建快捷方式(&S)" : L"Create Shortcut(&S)");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_OPEN_EXPLORER, isZh ? L"用系统资源管理器打开(&O)" : L"Open in Explorer(&O)");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_PROPERTIES, isZh ? L"属性(&P)\\tAlt+Enter" : L"Properties(&P)\\tAlt+Enter");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_EXIT, isZh ? L"退出(&X)\\tAlt+F4" : L"Exit(&X)\\tAlt+F4");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, isZh ? L"文件(&F)" : L"File(&F)");

    // =========================================================================
    // 2. 编辑 (E) Edit Menu
    // =========================================================================
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_UNDO, isZh ? L"撤销(&U)\\tCtrl+Z" : L"Undo(&U)\\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_CUT, isZh ? L"剪切(&T)\\tCtrl+X" : L"Cut(&T)\\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_COPY, isZh ? L"复制(&C)\\tCtrl+C" : L"Copy(&C)\\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE, isZh ? L"粘贴(&P)\\tCtrl+V" : L"Paste(&P)\\tCtrl+V");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE_SHORTCUT, isZh ? L"粘贴快捷方式(&S)" : L"Paste Shortcut(&S)");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_SELECT_ALL, isZh ? L"全选(&A)\\tCtrl+A" : L"Select All(&A)\\tCtrl+A");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_INVERT_SELECTION, isZh ? L"反向选择(&I)\\tCtrl+I" : L"Invert Selection(&I)\\tCtrl+I");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hCopyPathSubmenu = CreatePopupMenu();
    AppendMenuW(hCopyPathSubmenu, MF_STRING, ID_EDIT_COPY_PATH, isZh ? L"复制完整路径(&P)\\tCtrl+Shift+C" : L"Copy Full Path(&P)\\tCtrl+Shift+C");
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
                ID_VIEW_TREE_TOGGLE, isZh ? L"文件夹目录树(&O)\\tF8" : L"Folders Tree(&O)\\tF8");
    AppendMenuW(hSideSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSideSubmenu, MF_STRING | (m_previewVisible ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_PREVIEW_TOGGLE, isZh ? L"快速预览窗格(&P)\\tAlt+P" : L"Preview Pane(&P)\\tAlt+P");
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
    int curVm = Config::Get().viewMode;
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
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hSortSubmenu, isZh ? L"排列图标(&I)" : L"Sort By(&I)");

    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);

    // 转到 (M)
    HMENU hGoSubmenu = CreatePopupMenu();
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_BACK, isZh ? L"后退(&B)\\tAlt+Left" : L"Back(&B)\\tAlt+Left");
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_FORWARD, isZh ? L"前进(&F)\\tAlt+Right" : L"Forward(&F)\\tAlt+Right");
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_UP, isZh ? L"向上一级(&U)\\tAlt+Up" : L"Up(&U)\\tAlt+Up");
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_UP_NEW_TAB, isZh ? L"在新标签页中打开上一级文件夹(&N)" : L"Open Up in New Tab(&N)");
    AppendMenuW(hGoSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hGoSubmenu, MF_STRING, ID_NAV_HOME, isZh ? L"转到地址栏(&D)\\tAlt+D" : L"Go to Address Bar(&D)\\tAlt+D");
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

    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_MINIMIZE_TRAY, isZh ? L"最小化至系统托盘(&Y)" : L"Minimize to Tray(&Y)");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().showHidden ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_SHOW_HIDDEN, isZh ? L"显示隐藏文件(&H)" : L"Show Hidden Files(&H)");
    AppendMenuW(hViewMenu, MF_STRING | (Config::Get().calcFolderSize ? MF_CHECKED : MF_UNCHECKED),
                ID_VIEW_CALC_FOLDER_SIZE, isZh ? L"计算文件夹大小(&C)" : L"Calculate Folder Sizes(&C)");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_REFRESH, isZh ? L"刷新(&R)\\tF5" : L"Refresh(&R)\\tF5");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, isZh ? L"查看(&V)" : L"View(&V)");

    // =========================================================================
    // 4. 收藏 (A) Favorites Menu
    // =========================================================================
    HMENU hFavMenu = CreatePopupMenu();
    AppendMenuW(hFavMenu, MF_STRING, ID_FAVORITES_ADD, isZh ? L"添加到收藏夹(&A)..." : L"Add to Favorites(&A)...");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAVORITES_ADD_LINKS, isZh ? L"添加到链接(&L)..." : L"Add to Links(&L)...");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAVORITES_MANAGE, isZh ? L"管理收藏夹(&E)..." : L"Manage Favorites(&E)...");
    AppendMenuW(hFavMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_THIS_PC, isZh ? L"此电脑(&C)" : L"This PC(&C)");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_DESKTOP, isZh ? L"桌面(&D)" : L"Desktop(&D)");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_DOWNLOADS, isZh ? L"下载(&O)" : L"Downloads(&O)");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_DOCUMENTS, isZh ? L"文档(&O)" : L"Documents(&O)");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_PICTURES, isZh ? L"图片(&P)" : L"Pictures(&P)");
    AppendMenuW(hFavMenu, MF_STRING, ID_FAV_USER_PROFILE, isZh ? L"用户主目录(&U)" : L"User Profile(&U)");
    AppendMenuW(hFavMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hDrivesSubmenu = CreatePopupMenu();
    DWORD driveMask = GetLogicalDrives();
    m_menuDrives.clear();
    for (int i = 0; i < 26; ++i) {
        if (driveMask & (1 << i)) {
            wchar_t rootPath[4] = { (wchar_t)(L'A' + i), L':', L'\\\\', 0 };
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
        HMENU hLinksSub = CreatePopupMenu();
        for (const auto& lk : links) {
            UINT favCmdId = ID_FAV_ITEM_START + (UINT)m_menuFavLinks.size();
            m_menuFavLinks.push_back(lk.path);
            AppendMenuW(hLinksSub, MF_STRING, favCmdId, lk.name.c_str());
        }
        AppendMenuW(hFavMenu, MF_POPUP, (UINT_PTR)hLinksSub, isZh ? L"链接(&K)" : L"Links(&K)");
    }
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFavMenu, isZh ? L"收藏(&A)" : L"Favorites(&A)");

    // =========================================================================
    // 5. 工具 (T) Tools Menu
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

    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_TERMINAL, isZh ? L"打开 Windows Terminal(&T)" : L"Open Windows Terminal(&T)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_POWERSHELL, isZh ? L"打开 PowerShell(&P)" : L"Open PowerShell(&P)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, isZh ? L"打开 CMD 命令行(&C)" : L"Open CMD Command Prompt(&C)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_VSCODE, isZh ? L"在 VS Code 中打开(&V)" : L"Open in VS Code(&V)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hScriptSubmenu = CreatePopupMenu();
    AppendMenuW(hScriptSubmenu, MF_STRING, ID_TOOLS_RELOAD_SCRIPTS, isZh ? L"重新加载所有脚本(&R)" : L"Reload Scripts(&R)");
    AppendMenuW(hScriptSubmenu, MF_STRING, ID_TOOLS_SCRIPT_CONSOLE, isZh ? L"打开脚本控制台(&C)" : L"Script Console(&C)");
    AppendMenuW(hScriptSubmenu, MF_STRING, ID_TOOLS_OPEN_SCRIPTS_DIR, isZh ? L"打开脚本目录(&F)" : L"Open Scripts Folder(&F)");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hScriptSubmenu, isZh ? L"扩展功能(&R)" : L"Extensions & Scripts(&R)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_FOLDER_OPTIONS, isZh ? L"系统文件夹选项(&F)..." : L"Folder Options(&F)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu, isZh ? L"工具(&T)" : L"Tools(&T)");

    // =========================================================================
    // 6. 窗口 (W) Window Menu
    // =========================================================================
    HMENU hWindowMenu = CreatePopupMenu();

    // 分页操作 (T)
    HMENU hTabOpsSubmenu = CreatePopupMenu();
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_TAB_NEXT, isZh ? L"下一分页(&N)\\tCtrl+Tab" : L"Next Tab(&N)\\tCtrl+Tab");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_TAB_PREV, isZh ? L"上一分页(&B)\\tCtrl+Shift+Tab" : L"Previous Tab(&B)\\tCtrl+Shift+Tab");
    AppendMenuW(hTabOpsSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_TAB, isZh ? L"关闭当前分页(&C)\\tCtrl+W" : L"Close Current Tab(&C)\\tCtrl+W");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_LEFT_TABS, isZh ? L"关闭左侧分页(&L)" : L"Close Left Tabs(&L)");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_RIGHT_TABS, isZh ? L"关闭右侧分页(&R)" : L"Close Right Tabs(&R)");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_OTHER_TABS, isZh ? L"除当前分页外全部关闭(&O)" : L"Close Other Tabs(&O)");
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_CLOSE_ALL_TABS, isZh ? L"关闭全部分页(&A)" : L"Close All Tabs(&A)");
    AppendMenuW(hTabOpsSubmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTabOpsSubmenu, MF_STRING, ID_FILE_DUPLICATE_TAB, isZh ? L"复制当前分页(&D)" : L"Duplicate Tab(&D)");
    bool isPinned = m_tabBar.IsTabPinned(m_tabBar.GetActiveIndex());
    AppendMenuW(hTabOpsSubmenu, MF_STRING | (isPinned ? MF_CHECKED : MF_UNCHECKED),
                ID_FILE_PIN_TAB, isPinned ? (isZh ? L"取消固定分页(&P)" : L"Unpin Tab(&P)") : (isZh ? L"固定当前分页(&P)" : L"Pin Tab(&P)"));
    AppendMenuW(hWindowMenu, MF_POPUP, (UINT_PTR)hTabOpsSubmenu, isZh ? L"分页操作(&T)" : L"Tab Operations(&T)");

    // 窗口操作 (W)
    HMENU hWinOpsSubmenu = CreatePopupMenu();
    AppendMenuW(hWinOpsSubmenu, MF_STRING, ID_WINDOW_MAXIMIZE, isZh ? L"当前窗格最大化(&M)" : L"Maximize Pane(&M)");
    AppendMenuW(hWinOpsSubmenu, MF_STRING, ID_WINDOW_MINIMIZE, isZh ? L"当前窗格最小化(&I)" : L"Minimize Pane(&I)");
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
                ID_VIEW_LAYOUT_2X2, isZh ? L"四分屏(&Q)" : L"Quad 2x2(&Q)");
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
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_SHORTCUTS, isZh ? L"帮助与键盘快捷键(&K)" : L"Keyboard Shortcuts(&K)");
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GESTURES, isZh ? L"鼠标手势说明(&G)" : L"Mouse Gestures Cheatsheet(&G)");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_MDIE_HOMEPAGE, isZh ? L"MDIE 官方主页(&M)" : L"MDIE Homepage(&M)");
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GITHUB, isZh ? L"NeoMDIE GitHub 开源仓库(&G)" : L"NeoMDIE GitHub(&G)");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_ABOUT, isZh ? L"关于 NeoMDIE(&A)..." : L"About NeoMDIE(&A)...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, isZh ? L"帮助(&H)" : L"Help(&H)");

    SetMenu(m_hwnd, hMenuBar);
    if (hOldMenu) {
        DestroyMenu(hOldMenu);
    }
}

'''

s = s[:start_idx] + new_build_menu + s[end_idx:]

with open(r'd:\github\NeoMDIE\src\ui\MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(s)
print('Updated MainWindow.cpp successfully')
