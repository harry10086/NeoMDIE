#include "Config.h"
#include "GestureEngine.h"

Config& Config::Get() {
    static Config instance;
    return instance;
}

Config::Config() {
    std::wstring exeDir = Util::GetExecutableDir();
    m_iniPath = exeDir + L"\\NeoMDIE.ini";

    // Test writability in exeDir
    HANDLE hFile = CreateFileW(m_iniPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        // Fallback to %APPDATA%\NeoMDIE
        wchar_t appData[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData) == S_OK) {
            std::wstring dir = std::wstring(appData) + L"\\NeoMDIE";
            CreateDirectoryW(dir.c_str(), nullptr);
            m_iniPath = dir + L"\\NeoMDIE.ini";
        }
    } else {
        LARGE_INTEGER size;
        if (GetFileSizeEx(hFile, &size) && size.QuadPart == 0) {
            unsigned char bom[2] = { 0xFF, 0xFE };
            DWORD written = 0;
            WriteFile(hFile, bom, 2, &written, nullptr);
        }
        CloseHandle(hFile);
    }
}

void Config::Load() {
    const wchar_t* secGeneral = L"General";
    const wchar_t* secWindow = L"Window";
    const wchar_t* secTabs = L"Tabs";

    // Window
    window.x = GetPrivateProfileIntW(secWindow, L"X", CW_USEDEFAULT, m_iniPath.c_str());
    window.y = GetPrivateProfileIntW(secWindow, L"Y", CW_USEDEFAULT, m_iniPath.c_str());
    window.width = GetPrivateProfileIntW(secWindow, L"Width", 1100, m_iniPath.c_str());
    window.height = GetPrivateProfileIntW(secWindow, L"Height", 700, m_iniPath.c_str());
    window.maximized = GetPrivateProfileIntW(secWindow, L"Maximized", 1, m_iniPath.c_str()) != 0;

    // General
    themeMode = GetPrivateProfileIntW(secGeneral, L"ThemeMode", -1, m_iniPath.c_str());
    accentColorMode = GetPrivateProfileIntW(secGeneral, L"AccentColorMode", 0, m_iniPath.c_str());
    int clrR = GetPrivateProfileIntW(secGeneral, L"CustomAccentR", 0, m_iniPath.c_str());
    int clrG = GetPrivateProfileIntW(secGeneral, L"CustomAccentG", 120, m_iniPath.c_str());
    int clrB = GetPrivateProfileIntW(secGeneral, L"CustomAccentB", 212, m_iniPath.c_str());
    customAccentColor = RGB(clrR, clrG, clrB);
    language = GetPrivateProfileIntW(secGeneral, L"Language", -1, m_iniPath.c_str());
    showTree = GetPrivateProfileIntW(secGeneral, L"ShowTree", 1, m_iniPath.c_str()) != 0;
    treeWidth = GetPrivateProfileIntW(secGeneral, L"TreeWidth", 240, m_iniPath.c_str());
    paneLayout = GetPrivateProfileIntW(secGeneral, L"PaneLayout", 0, m_iniPath.c_str());
    syncNav = GetPrivateProfileIntW(secGeneral, L"SyncNav", 0, m_iniPath.c_str()) != 0;
    splitEnabled = GetPrivateProfileIntW(secGeneral, L"SplitEnabled", 0, m_iniPath.c_str()) != 0;
    splitRatio = GetPrivateProfileIntW(secGeneral, L"SplitRatio", 50, m_iniPath.c_str());
    previewEnabled = GetPrivateProfileIntW(secGeneral, L"PreviewEnabled", 0, m_iniPath.c_str()) != 0;
    previewWidth = GetPrivateProfileIntW(secGeneral, L"PreviewWidth", 320, m_iniPath.c_str());
    viewMode = GetPrivateProfileIntW(secGeneral, L"ViewMode", 0, m_iniPath.c_str());
    showHidden = GetPrivateProfileIntW(secGeneral, L"ShowHidden", 0, m_iniPath.c_str()) != 0;
    hideDotFiles = GetPrivateProfileIntW(secGeneral, L"HideDotFiles", 1, m_iniPath.c_str()) != 0;
    showItemCheckBoxes = GetPrivateProfileIntW(secGeneral, L"ShowItemCheckBoxes", 0, m_iniPath.c_str()) != 0;
    listDensity = GetPrivateProfileIntW(secGeneral, L"ListDensity", 1, m_iniPath.c_str());
    listScale = GetPrivateProfileIntW(secGeneral, L"ListScale", 1, m_iniPath.c_str());
    foldersFirst = GetPrivateProfileIntW(secGeneral, L"FoldersFirst", 1, m_iniPath.c_str()) != 0;
    calcFolderSize = GetPrivateProfileIntW(secGeneral, L"CalcFolderSize", 0, m_iniPath.c_str()) != 0;
    gesturesEnabled = GetPrivateProfileIntW(secGeneral, L"GesturesEnabled", 1, m_iniPath.c_str()) != 0;

    lockToolbars = GetPrivateProfileIntW(secGeneral, L"LockToolbars", 0, m_iniPath.c_str()) != 0;
    searchBarFirst = GetPrivateProfileIntW(secGeneral, L"SearchBarFirst", 0, m_iniPath.c_str()) != 0;
    filterBarWidth = GetPrivateProfileIntW(secGeneral, L"FilterBarWidth", 240, m_iniPath.c_str());
    restoreTabsOnStartup = GetPrivateProfileIntW(secGeneral, L"RestoreTabs", 1, m_iniPath.c_str()) != 0;
    minimizeToTray = GetPrivateProfileIntW(secGeneral, L"MinimizeToTray", 0, m_iniPath.c_str()) != 0;
    closeToTray = GetPrivateProfileIntW(secGeneral, L"CloseToTray", 0, m_iniPath.c_str()) != 0;
    showMenuBar = GetPrivateProfileIntW(secGeneral, L"ShowMenuBar", 0, m_iniPath.c_str()) != 0;
    showToolbarStandard = GetPrivateProfileIntW(secGeneral, L"ShowToolbarStandard", 1, m_iniPath.c_str()) != 0;
    showToolbarAddress = GetPrivateProfileIntW(secGeneral, L"ShowToolbarAddress", 1, m_iniPath.c_str()) != 0;
    showToolbarLinks = GetPrivateProfileIntW(secGeneral, L"ShowToolbarLinks", 1, m_iniPath.c_str()) != 0;
    showToolbarSearch = GetPrivateProfileIntW(secGeneral, L"ShowToolbarSearch", 1, m_iniPath.c_str()) != 0;
    showStatusBar = GetPrivateProfileIntW(secGeneral, L"ShowStatusBar", 1, m_iniPath.c_str()) != 0;
    allowDuplicateTabs = GetPrivateProfileIntW(secGeneral, L"AllowDuplicateTabs", 1, m_iniPath.c_str()) != 0;
    tabDoubleClickAction = GetPrivateProfileIntW(secGeneral, L"TabDblClickAction", 0, m_iniPath.c_str());
    autoExpandTree = GetPrivateProfileIntW(secGeneral, L"AutoExpandTree", 1, m_iniPath.c_str()) != 0;
    searchSubfoldersDefault = GetPrivateProfileIntW(secGeneral, L"SearchSubfolders", 0, m_iniPath.c_str()) != 0;
    singleInstance = GetPrivateProfileIntW(secGeneral, L"SingleInstance", 1, m_iniPath.c_str()) != 0;
    zebraStriping = GetPrivateProfileIntW(secGeneral, L"ZebraStriping", 0, m_iniPath.c_str()) != 0;
    showGridLines = GetPrivateProfileIntW(secGeneral, L"ShowGridLines", 0, m_iniPath.c_str()) != 0;
    auto loadAction = [&](const wchar_t* prefix, TargetActionConfig& tac) {
        wchar_t k[64];
        StringCchPrintfW(k, 64, L"%s_OpenCurrent", prefix); tac.openCurrentPane = GetPrivateProfileIntW(secGeneral, k, tac.openCurrentPane, m_iniPath.c_str());
        StringCchPrintfW(k, 64, L"%s_OpenNewTab", prefix); tac.openNewTab = GetPrivateProfileIntW(secGeneral, k, tac.openNewTab, m_iniPath.c_str());
        StringCchPrintfW(k, 64, L"%s_OpenBgTab", prefix); tac.openBgTab = GetPrivateProfileIntW(secGeneral, k, tac.openBgTab, m_iniPath.c_str());
        StringCchPrintfW(k, 64, L"%s_OpenOtherPane", prefix); tac.openOtherPane = GetPrivateProfileIntW(secGeneral, k, tac.openOtherPane, m_iniPath.c_str());
        StringCchPrintfW(k, 64, L"%s_OpenExplorer", prefix); tac.openExplorer = GetPrivateProfileIntW(secGeneral, k, tac.openExplorer, m_iniPath.c_str());
    };
    loadAction(L"ActionTree", actionTree);
    loadAction(L"ActionLinks", actionLinks);
    loadAction(L"ActionAddress", actionAddress);

    // Quick Access
    const wchar_t* secQA = L"QuickAccess";
    int qaCount = GetPrivateProfileIntW(secQA, L"Count", -1, m_iniPath.c_str());
    if (qaCount == -1) {
        InitDefaultQuickAccess();
    } else {
        quickAccessFolders.clear();
        for (int i = 0; i < qaCount; ++i) {
            wchar_t k[32], val[MAX_PATH];
            StringCchPrintfW(k, 32, L"Folder%d", i);
            GetPrivateProfileStringW(secQA, k, L"", val, MAX_PATH, m_iniPath.c_str());
            if (val[0] != L'\0' && PathFileExistsW(val)) {
                quickAccessFolders.push_back(Util::NormalizePath(val));
            }
        }
        if (quickAccessFolders.empty()) {
            InitDefaultQuickAccess();
        }
    }

    // Tabs
    sessionTabs.clear();
    sessionTabsInfo.clear();
    int tabCount = GetPrivateProfileIntW(secTabs, L"Count", 0, m_iniPath.c_str());
    activeTabIndex = GetPrivateProfileIntW(secTabs, L"Active", 0, m_iniPath.c_str());

    for (int i = 0; i < tabCount; ++i) {
        wchar_t key[64];
        StringCchPrintfW(key, 64, L"Tab%d", i);
        wchar_t val[MAX_PATH];
        GetPrivateProfileStringW(secTabs, key, L"", val, MAX_PATH, m_iniPath.c_str());
        if (wcslen(val) == 0) continue;

        sessionTabs.push_back(val);

        SessionTabInfo tabInfo;
        tabInfo.path = val;

        StringCchPrintfW(key, 64, L"Tab%d_Layout", i);
        tabInfo.layout = GetPrivateProfileIntW(secTabs, key, 0, m_iniPath.c_str());

        StringCchPrintfW(key, 64, L"Tab%d_ActivePane", i);
        tabInfo.activePaneIndex = GetPrivateProfileIntW(secTabs, key, 0, m_iniPath.c_str());

        StringCchPrintfW(key, 64, L"Tab%d_PaneCount", i);
        int pCount = GetPrivateProfileIntW(secTabs, key, 0, m_iniPath.c_str());
        if (pCount > 0) {
            for (int p = 0; p < pCount; ++p) {
                wchar_t pKey[64];
                StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d", i, p);
                wchar_t pVal[MAX_PATH];
                GetPrivateProfileStringW(secTabs, pKey, L"", pVal, MAX_PATH, m_iniPath.c_str());
                if (wcslen(pVal) > 0) {
                    tabInfo.panePaths.push_back(pVal);
                } else {
                    tabInfo.panePaths.push_back(tabInfo.path);
                }

                PaneViewState vs;
                StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_SortCol", i, p);
                vs.sortCol = GetPrivateProfileIntW(secTabs, pKey, 0, m_iniPath.c_str());
                StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_SortAsc", i, p);
                vs.sortAsc = GetPrivateProfileIntW(secTabs, pKey, 1, m_iniPath.c_str()) != 0;
                StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_ViewMode", i, p);
                vs.viewMode = GetPrivateProfileIntW(secTabs, pKey, -1, m_iniPath.c_str());
                StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_FoldersFirst", i, p);
                vs.foldersFirst = GetPrivateProfileIntW(secTabs, pKey, -1, m_iniPath.c_str());
                tabInfo.paneViewStates.push_back(vs);
            }
        }
        if (tabInfo.panePaths.empty()) {
            tabInfo.panePaths.push_back(tabInfo.path);
            PaneViewState vs;
            tabInfo.paneViewStates.push_back(vs);
        }
        sessionTabsInfo.push_back(tabInfo);
    }

    wchar_t userProfile[MAX_PATH] = { 0 };
    GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);

    if (sessionTabs.empty() || (sessionTabs.size() == 1 && userProfile[0] && _wcsicmp(sessionTabs[0].c_str(), userProfile) == 0)) {
        sessionTabs.clear();
        sessionTabs.push_back(L"This PC");
        sessionTabsInfo.clear();
        SessionTabInfo defInfo;
        defInfo.path = L"This PC";
        defInfo.layout = 0;
        defInfo.activePaneIndex = 0;
        defInfo.panePaths = { L"This PC" };
        defInfo.paneViewStates.push_back(PaneViewState{});
        sessionTabsInfo.push_back(defInfo);
    }

    // Gestures
    gestures.clear();
    const wchar_t* secGestures = L"Gestures";
    int gestureCount = GetPrivateProfileIntW(secGestures, L"Count", -1, m_iniPath.c_str());
    if (gestureCount > 0) {
        for (int i = 0; i < gestureCount; ++i) {
            wchar_t kCode[32], kAct[32], kName[32];
            StringCchPrintfW(kCode, 32, L"Code%d", i);
            StringCchPrintfW(kAct, 32, L"Action%d", i);
            StringCchPrintfW(kName, 32, L"Name%d", i);

            wchar_t vCode[64], vName[64];
            GetPrivateProfileStringW(secGestures, kCode, L"", vCode, 64, m_iniPath.c_str());
            GetPrivateProfileStringW(secGestures, kName, L"", vName, 64, m_iniPath.c_str());
            int vAct = GetPrivateProfileIntW(secGestures, kAct, 0, m_iniPath.c_str());

            if (wcslen(vCode) > 0) {
                std::wstring nameStr = vName;
                if (nameStr == L"滚动到列表首端") nameStr = L"滚动到列表顶部";
                else if (nameStr == L"滚动到列表末端") nameStr = L"滚动到列表底部";
                std::string codeStr = Util::WideToUtf8(vCode);
                if (codeStr == "UD" && vAct == (int)GestureAction::NewTab) codeStr = "RL";
                else if ((codeStr == "RU" || codeStr == "LDR") && vAct == (int)GestureAction::ScrollTop) codeStr = "DU";
                else if ((codeStr == "RD" || codeStr == "RDL") && vAct == (int)GestureAction::ScrollBottom) codeStr = "UD";
                else if (codeStr == "LUR" && vAct == (int)GestureAction::PageUp) codeStr = "LU";
                else if (codeStr == "RUL" && vAct == (int)GestureAction::PageDown) codeStr = "LD";
                gestures.push_back({ codeStr, vAct, nameStr });
            }
        }
    }
    bool gestCorrupted = false;
    for (const auto& g : gestures) {
        if (g.name.length() <= 1) { gestCorrupted = true; break; }
    }
    if (gestures.empty() || gestCorrupted) {
        gestures = {
            { "L", (int)GestureAction::NavBack, L"后退" },
            { "R", (int)GestureAction::NavForward, L"前进" },
            { "U", (int)GestureAction::NavUp, L"向上一级" },
            { "D", (int)GestureAction::Refresh, L"刷新" },
            { "RL", (int)GestureAction::NewTab, L"新建标签页" },
            { "DR", (int)GestureAction::CloseTab, L"关闭当前标签页" },
            { "UL", (int)GestureAction::ToggleTree, L"显示/隐藏侧边栏目录树" },
            { "UR", (int)GestureAction::TogglePreview, L"显示/隐藏快速预览窗格" },
            { "DU", (int)GestureAction::ScrollTop, L"滚动到列表顶部" },
            { "UD", (int)GestureAction::ScrollBottom, L"滚动到列表底部" },
            { "LU", (int)GestureAction::PageUp, L"向上翻页" },
            { "LD", (int)GestureAction::PageDown, L"向下翻页" }
        };
    }

    // Shortcuts
    shortcuts.clear();
    const wchar_t* secShortcuts = L"Shortcuts";
    int scCount = GetPrivateProfileIntW(secShortcuts, L"Count", -1, m_iniPath.c_str());
    if (scCount > 0) {
        for (int i = 0; i < scCount; ++i) {
            wchar_t kName[32], kKey[32], kShift[32], kCtrl[32], kAlt[32];
            StringCchPrintfW(kName, 32, L"Name%d", i);
            StringCchPrintfW(kKey, 32, L"Key%d", i);
            StringCchPrintfW(kShift, 32, L"Shift%d", i);
            StringCchPrintfW(kCtrl, 32, L"Ctrl%d", i);
            StringCchPrintfW(kAlt, 32, L"Alt%d", i);

            wchar_t vName[128], vKey[32];
            GetPrivateProfileStringW(secShortcuts, kName, L"", vName, 128, m_iniPath.c_str());
            GetPrivateProfileStringW(secShortcuts, kKey, L"", vKey, 32, m_iniPath.c_str());
            bool vShift = GetPrivateProfileIntW(secShortcuts, kShift, 0, m_iniPath.c_str()) != 0;
            bool vCtrl = GetPrivateProfileIntW(secShortcuts, kCtrl, 0, m_iniPath.c_str()) != 0;
            bool vAlt = GetPrivateProfileIntW(secShortcuts, kAlt, 0, m_iniPath.c_str()) != 0;

            if (wcslen(vName) > 0 && wcslen(vKey) > 0) {
                shortcuts.push_back({ vName, vKey, vShift, vCtrl, vAlt });
            }
        }
        bool hasNewMD = false;
        for (const auto& sc : shortcuts) {
            if (sc.name == L"新建Markdown文档" || sc.name == L"新建Markdown文件" || sc.name == L"新建markdown文件" || sc.name == L"New Markdown Document") {
                hasNewMD = true;
                break;
            }
        }
        if (!hasNewMD) {
            shortcuts.push_back({ L"新建Markdown文档", L"M", true, true, true });
        }
    }
    bool scCorrupted = false;
    for (const auto& sc : shortcuts) {
        if (sc.name.length() <= 1) { scCorrupted = true; break; }
    }
    if (shortcuts.empty() || scCorrupted) {
        shortcuts = {
            { L"新建标签页", L"T", false, true, false },
            { L"关闭当前标签页", L"W", false, true, false },
            { L"切换到下一个标签页", L"Tab", false, true, false },
            { L"切换到上一个标签页", L"Tab", true, true, false },
            { L"新建窗口", L"N", false, true, false },
            { L"后退", L"Left", false, false, true },
            { L"前进", L"Right", false, false, true },
            { L"向上一级", L"Up", false, false, true },
            { L"刷新", L"F5", false, false, false },
            { L"聚焦到地址栏", L"D", false, false, true },
            { L"聚焦到搜索过滤栏", L"F", false, true, false },
            { L"最大化/还原当前栏", L"F11", false, false, false },
            { L"单栏模式", L"1", true, true, false },
            { L"左右双栏", L"2", true, true, false },
            { L"上下双栏", L"3", true, true, false },
            { L"四分栏模式", L"4", true, true, false },
            { L"滚动到列表顶部", L"Home", false, false, false },
            { L"滚动到列表底部", L"End", false, false, false },
            { L"向上翻页", L"PageUp", false, false, false },
            { L"向下翻页", L"PageDown", false, false, false },
            { L"全选", L"A", false, true, false },
            { L"反向选择", L"I", false, true, false },
            { L"撤销", L"Z", false, true, false },
            { L"复制选中项", L"C", false, true, false },
            { L"剪切选中项", L"X", false, true, false },
            { L"粘贴", L"V", false, true, false },
            { L"新建文件夹", L"N", true, true, false },
            { L"新建Markdown文档", L"M", true, true, true },
            { L"属性", L"Enter", false, false, true }
        };
    }

    // Column widths
    const wchar_t* secColumns = L"Columns";
    static const struct { int id; const wchar_t* key; int defW; } s_colDefs[] = {
        { 0, L"Col_Name", 300 },
        { 1, L"Col_DateModified", 160 },
        { 2, L"Col_Type", 130 },
        { 3, L"Col_Size", 120 },
        { 4, L"Col_DateCreated", 160 },
        { 5, L"Col_Attributes", 100 },
        { 6, L"Col_Authors", 150 },
        { 7, L"Col_Title", 180 },
        { 8, L"Col_Tags", 150 },
        { 9, L"Col_Path", 300 }
    };
    for (const auto& cd : s_colDefs) {
        int w = GetPrivateProfileIntW(secColumns, cd.key, cd.defW, m_iniPath.c_str());
        if (w > 20) {
            columnWidths[cd.id] = w;
        }
    }

    // FolderViews (per-folder view mode, sort column/direction, and column widths)
    folderViewMap.clear();
    const wchar_t* secFolderViews = L"FolderViews";
    int fvCount = GetPrivateProfileIntW(secFolderViews, L"Count", 0, m_iniPath.c_str());
    for (int i = 0; i < fvCount; ++i) {
        wchar_t kPath[32], kVal[32];
        StringCchPrintfW(kPath, 32, L"P%d", i);
        StringCchPrintfW(kVal, 32, L"V%d", i);
        wchar_t pathBuf[MAX_PATH] = { 0 };
        GetPrivateProfileStringW(secFolderViews, kPath, L"", pathBuf, MAX_PATH, m_iniPath.c_str());
        if (pathBuf[0] == L'\0') continue;

        wchar_t valBuf[512] = { 0 };
        GetPrivateProfileStringW(secFolderViews, kVal, L"", valBuf, 512, m_iniPath.c_str());

        // val format: "sortCol,sortAsc,viewMode,foldersFirst;0:300,1:160,2:130,..."
        FolderViewSettings fvs;
        std::wstring sVal(valBuf);
        size_t semi = sVal.find(L';');
        std::wstring headerPart = (semi != std::wstring::npos) ? sVal.substr(0, semi) : sVal;
        int sc = -1, sa = 1, vm = -1, ff = -1;
        int parsed = swscanf_s(headerPart.c_str(), L"%d,%d,%d,%d", &sc, &sa, &vm, &ff);
        if (parsed >= 2) {
            fvs.sortCol = sc;
            fvs.sortAsc = (sa != 0);
            fvs.viewMode = vm;
            if (parsed >= 4) {
                fvs.foldersFirst = ff;
            }
        }
        if (semi != std::wstring::npos) {
            std::wstring colsPart = sVal.substr(semi + 1);
            size_t start = 0;
            while (start < colsPart.length()) {
                size_t comma = colsPart.find(L',', start);
                std::wstring token = (comma != std::wstring::npos) ? colsPart.substr(start, comma - start) : colsPart.substr(start);
                size_t colon = token.find(L':');
                if (colon != std::wstring::npos) {
                    int cId = _wtoi(token.substr(0, colon).c_str());
                    int cW = _wtoi(token.substr(colon + 1).c_str());
                    if (cW > 20) {
                        fvs.columnWidths[cId] = cW;
                    }
                }
                if (comma == std::wstring::npos) break;
                start = comma + 1;
            }
        }
        folderViewMap[NormalizeFolderKey(pathBuf)] = fvs;
    }
}

void Config::Save() {
    std::wstringstream out;

    // Window
    out << L"[Window]\r\n";
    out << L"X=" << window.x << L"\r\n";
    out << L"Y=" << window.y << L"\r\n";
    out << L"Width=" << window.width << L"\r\n";
    out << L"Height=" << window.height << L"\r\n";
    out << L"Maximized=" << (window.maximized ? 1 : 0) << L"\r\n\r\n";

    // General
    out << L"[General]\r\n";
    out << L"ThemeMode=" << themeMode << L"\r\n";
    out << L"AccentColorMode=" << accentColorMode << L"\r\n";
    out << L"CustomAccentR=" << (int)GetRValue(customAccentColor) << L"\r\n";
    out << L"CustomAccentG=" << (int)GetGValue(customAccentColor) << L"\r\n";
    out << L"CustomAccentB=" << (int)GetBValue(customAccentColor) << L"\r\n";
    out << L"Language=" << language << L"\r\n";
    out << L"ShowTree=" << (showTree ? 1 : 0) << L"\r\n";
    out << L"TreeWidth=" << treeWidth << L"\r\n";
    out << L"PaneLayout=" << paneLayout << L"\r\n";
    out << L"SyncNav=" << (syncNav ? 1 : 0) << L"\r\n";
    out << L"SplitEnabled=" << (splitEnabled ? 1 : 0) << L"\r\n";
    out << L"SplitRatio=" << splitRatio << L"\r\n";
    out << L"PreviewEnabled=" << (previewEnabled ? 1 : 0) << L"\r\n";
    out << L"PreviewWidth=" << previewWidth << L"\r\n";
    out << L"ViewMode=" << viewMode << L"\r\n";
    out << L"ShowHidden=" << (showHidden ? 1 : 0) << L"\r\n";
    out << L"HideDotFiles=" << (hideDotFiles ? 1 : 0) << L"\r\n";
    out << L"ShowItemCheckBoxes=" << (showItemCheckBoxes ? 1 : 0) << L"\r\n";
    out << L"ListDensity=" << listDensity << L"\r\n";
    out << L"ListScale=" << listScale << L"\r\n";
    out << L"FoldersFirst=" << (foldersFirst ? 1 : 0) << L"\r\n";
    out << L"CalcFolderSize=" << (calcFolderSize ? 1 : 0) << L"\r\n";
    out << L"GesturesEnabled=" << (gesturesEnabled ? 1 : 0) << L"\r\n";
    out << L"SingleInstance=" << (singleInstance ? 1 : 0) << L"\r\n";
    out << L"ZebraStriping=" << (zebraStriping ? 1 : 0) << L"\r\n";
    out << L"ShowGridLines=" << (showGridLines ? 1 : 0) << L"\r\n";
    out << L"LockToolbars=" << (lockToolbars ? 1 : 0) << L"\r\n";
    out << L"SearchBarFirst=" << (searchBarFirst ? 1 : 0) << L"\r\n";
    out << L"FilterBarWidth=" << filterBarWidth << L"\r\n";
    out << L"RestoreTabs=" << (restoreTabsOnStartup ? 1 : 0) << L"\r\n";
    out << L"MinimizeToTray=" << (minimizeToTray ? 1 : 0) << L"\r\n";
    out << L"CloseToTray=" << (closeToTray ? 1 : 0) << L"\r\n";
    out << L"ShowMenuBar=" << (showMenuBar ? 1 : 0) << L"\r\n";
    out << L"ShowToolbarStandard=" << (showToolbarStandard ? 1 : 0) << L"\r\n";
    out << L"ShowToolbarAddress=" << (showToolbarAddress ? 1 : 0) << L"\r\n";
    out << L"ShowToolbarLinks=" << (showToolbarLinks ? 1 : 0) << L"\r\n";
    out << L"ShowToolbarSearch=" << (showToolbarSearch ? 1 : 0) << L"\r\n";
    out << L"ShowStatusBar=" << (showStatusBar ? 1 : 0) << L"\r\n";
    out << L"AllowDuplicateTabs=" << (allowDuplicateTabs ? 1 : 0) << L"\r\n";
    out << L"TabDblClickAction=" << tabDoubleClickAction << L"\r\n";
    out << L"AutoExpandTree=" << (autoExpandTree ? 1 : 0) << L"\r\n";
    out << L"SearchSubfolders=" << (searchSubfoldersDefault ? 1 : 0) << L"\r\n";

    auto saveAction = [&](const wchar_t* prefix, const TargetActionConfig& tac) {
        out << prefix << L"_OpenCurrent=" << tac.openCurrentPane << L"\r\n";
        out << prefix << L"_OpenNewTab=" << tac.openNewTab << L"\r\n";
        out << prefix << L"_OpenBgTab=" << tac.openBgTab << L"\r\n";
        out << prefix << L"_OpenOtherPane=" << tac.openOtherPane << L"\r\n";
        out << prefix << L"_OpenExplorer=" << tac.openExplorer << L"\r\n";
    };
    saveAction(L"ActionTree", actionTree);
    saveAction(L"ActionLinks", actionLinks);
    saveAction(L"ActionAddress", actionAddress);
    out << L"\r\n";

    // Quick Access
    out << L"[QuickAccess]\r\n";
    out << L"Count=" << quickAccessFolders.size() << L"\r\n";
    for (size_t i = 0; i < quickAccessFolders.size(); ++i) {
        out << L"Folder" << i << L"=" << quickAccessFolders[i] << L"\r\n";
    }
    out << L"\r\n";

    // Tabs
    out << L"[Tabs]\r\n";
    int saveCount = !sessionTabsInfo.empty() ? (int)sessionTabsInfo.size() : (int)sessionTabs.size();
    out << L"Count=" << saveCount << L"\r\n";
    out << L"Active=" << activeTabIndex << L"\r\n";
    if (!sessionTabsInfo.empty()) {
        for (size_t i = 0; i < sessionTabsInfo.size(); ++i) {
            out << L"Tab" << i << L"=" << sessionTabsInfo[i].path << L"\r\n";
            out << L"Tab" << i << L"_Layout=" << sessionTabsInfo[i].layout << L"\r\n";
            out << L"Tab" << i << L"_ActivePane=" << sessionTabsInfo[i].activePaneIndex << L"\r\n";
            out << L"Tab" << i << L"_PaneCount=" << (int)sessionTabsInfo[i].panePaths.size() << L"\r\n";

            for (size_t p = 0; p < sessionTabsInfo[i].panePaths.size(); ++p) {
                out << L"Tab" << i << L"_Pane" << p << L"=" << sessionTabsInfo[i].panePaths[p] << L"\r\n";
                if (p < sessionTabsInfo[i].paneViewStates.size()) {
                    const auto& vs = sessionTabsInfo[i].paneViewStates[p];
                    out << L"Tab" << i << L"_Pane" << p << L"_SortCol=" << vs.sortCol << L"\r\n";
                    out << L"Tab" << i << L"_Pane" << p << L"_SortAsc=" << (vs.sortAsc ? 1 : 0) << L"\r\n";
                    out << L"Tab" << i << L"_Pane" << p << L"_ViewMode=" << vs.viewMode << L"\r\n";
                    out << L"Tab" << i << L"_Pane" << p << L"_FoldersFirst=" << vs.foldersFirst << L"\r\n";
                }
            }
        }
    } else {
        for (size_t i = 0; i < sessionTabs.size(); ++i) {
            out << L"Tab" << i << L"=" << sessionTabs[i] << L"\r\n";
        }
    }
    out << L"\r\n";

    // Gestures
    out << L"[Gestures]\r\n";
    out << L"Count=" << (int)gestures.size() << L"\r\n";
    for (size_t i = 0; i < gestures.size(); ++i) {
        std::wstring wCode = Util::Utf8ToWide(gestures[i].code);
        out << L"Code" << i << L"=" << wCode << L"\r\n";
        out << L"Action" << i << L"=" << gestures[i].action << L"\r\n";
        out << L"Name" << i << L"=" << gestures[i].name << L"\r\n";
    }
    out << L"\r\n";

    // Shortcuts
    out << L"[Shortcuts]\r\n";
    out << L"Count=" << (int)shortcuts.size() << L"\r\n";
    for (size_t i = 0; i < shortcuts.size(); ++i) {
        out << L"Name" << i << L"=" << shortcuts[i].name << L"\r\n";
        out << L"Key" << i << L"=" << shortcuts[i].key << L"\r\n";
        out << L"Shift" << i << L"=" << (shortcuts[i].shift ? 1 : 0) << L"\r\n";
        out << L"Ctrl" << i << L"=" << (shortcuts[i].ctrl ? 1 : 0) << L"\r\n";
        out << L"Alt" << i << L"=" << (shortcuts[i].alt ? 1 : 0) << L"\r\n";
    }
    out << L"\r\n";

    // Column widths
    out << L"[Columns]\r\n";
    static const struct { int id; const wchar_t* key; int defW; } s_colDefsSave[] = {
        { 0, L"Col_Name", 300 },
        { 1, L"Col_DateModified", 160 },
        { 2, L"Col_Type", 130 },
        { 3, L"Col_Size", 120 },
        { 4, L"Col_DateCreated", 160 },
        { 5, L"Col_Attributes", 100 },
        { 6, L"Col_Authors", 150 },
        { 7, L"Col_Title", 180 },
        { 8, L"Col_Tags", 150 },
        { 9, L"Col_Path", 300 }
    };
    for (const auto& cd : s_colDefsSave) {
        auto it = columnWidths.find(cd.id);
        int w = (it != columnWidths.end() && it->second > 20) ? it->second : cd.defW;
        out << cd.key << L"=" << w << L"\r\n";
    }
    out << L"\r\n";

    // FolderViews (cap at 200 recent folders to keep INI clean)
    out << L"[FolderViews]\r\n";
    int fvIdx = 0;
    for (const auto& kv : folderViewMap) {
        if (fvIdx >= 200) break;
        if (kv.first.empty()) continue;

        out << L"P" << fvIdx << L"=" << kv.first << L"\r\n";
        out << L"V" << fvIdx << L"=" << kv.second.sortCol << L"," << (kv.second.sortAsc ? 1 : 0)
            << L"," << kv.second.viewMode << L"," << kv.second.foldersFirst << L";";
        bool firstCol = true;
        for (const auto& cw : kv.second.columnWidths) {
            if (!firstCol) out << L",";
            firstCol = false;
            out << cw.first << L":" << cw.second;
        }
        out << L"\r\n";
        fvIdx++;
    }
    out << L"Count=" << fvIdx << L"\r\n";

    std::wstring content = out.str();
    HANDLE hFile = CreateFileW(m_iniPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        unsigned char bom[2] = { 0xFF, 0xFE }; // UTF-16LE BOM
        DWORD written = 0;
        WriteFile(hFile, bom, 2, &written, nullptr);
        WriteFile(hFile, content.data(), (DWORD)(content.size() * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(hFile);
    }
    // Flush Windows internal INI cache
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, m_iniPath.c_str());
}

void Config::InitDefaultQuickAccess() {
    quickAccessFolders.clear();
    wchar_t path[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path) == S_OK) {
        quickAccessFolders.push_back(Util::NormalizePath(path));
    }
    wchar_t userProfile[MAX_PATH];
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        std::wstring dl = std::wstring(userProfile) + L"\\Downloads";
        if (PathFileExistsW(dl.c_str())) {
            quickAccessFolders.push_back(Util::NormalizePath(dl));
        }
    }
    if (SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, path) == S_OK) {
        quickAccessFolders.push_back(Util::NormalizePath(path));
    }
    if (SHGetFolderPathW(nullptr, CSIDL_MYPICTURES, nullptr, 0, path) == S_OK) {
        quickAccessFolders.push_back(Util::NormalizePath(path));
    }
    if (SHGetFolderPathW(nullptr, CSIDL_MYMUSIC, nullptr, 0, path) == S_OK) {
        quickAccessFolders.push_back(Util::NormalizePath(path));
    }
    if (SHGetFolderPathW(nullptr, CSIDL_MYVIDEO, nullptr, 0, path) == S_OK) {
        quickAccessFolders.push_back(Util::NormalizePath(path));
    }
}

bool Config::AddQuickAccess(const std::wstring& path) {
    std::wstring norm = Util::NormalizePath(path);
    if (norm.empty() || !PathFileExistsW(norm.c_str())) return false;
    for (const auto& f : quickAccessFolders) {
        if (_wcsicmp(f.c_str(), norm.c_str()) == 0) return false;
    }
    quickAccessFolders.push_back(norm);
    Save();
    return true;
}

bool Config::RemoveQuickAccess(const std::wstring& path) {
    std::wstring norm = Util::NormalizePath(path);
    for (auto it = quickAccessFolders.begin(); it != quickAccessFolders.end(); ++it) {
        if (_wcsicmp(it->c_str(), norm.c_str()) == 0) {
            quickAccessFolders.erase(it);
            Save();
            return true;
        }
    }
    return false;
}

void Config::ResetQuickAccess() {
    InitDefaultQuickAccess();
    Save();
}
