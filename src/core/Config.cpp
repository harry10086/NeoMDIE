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
    calcFolderSize = GetPrivateProfileIntW(secGeneral, L"CalcFolderSize", 0, m_iniPath.c_str()) != 0;
    gesturesEnabled = GetPrivateProfileIntW(secGeneral, L"GesturesEnabled", 1, m_iniPath.c_str()) != 0;

    lockToolbars = GetPrivateProfileIntW(secGeneral, L"LockToolbars", 0, m_iniPath.c_str()) != 0;
    searchBarFirst = GetPrivateProfileIntW(secGeneral, L"SearchBarFirst", 0, m_iniPath.c_str()) != 0;
    filterBarWidth = GetPrivateProfileIntW(secGeneral, L"FilterBarWidth", 240, m_iniPath.c_str());
    restoreTabsOnStartup = GetPrivateProfileIntW(secGeneral, L"RestoreTabs", 1, m_iniPath.c_str()) != 0;
    minimizeToTray = GetPrivateProfileIntW(secGeneral, L"MinimizeToTray", 0, m_iniPath.c_str()) != 0;
    closeToTray = GetPrivateProfileIntW(secGeneral, L"CloseToTray", 0, m_iniPath.c_str()) != 0;
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
                if (codeStr == "LDR" && vAct == (int)GestureAction::ScrollTop) codeStr = "RU";
                else if (codeStr == "RDL" && vAct == (int)GestureAction::ScrollBottom) codeStr = "RD";
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
            { "UD", (int)GestureAction::NewTab, L"新建标签页" },
            { "DR", (int)GestureAction::CloseTab, L"关闭当前标签页" },
            { "UL", (int)GestureAction::ToggleTree, L"显示/隐藏侧边栏目录树" },
            { "UR", (int)GestureAction::TogglePreview, L"显示/隐藏快速预览窗格" },
            { "RU", (int)GestureAction::ScrollTop, L"滚动到列表顶部" },
            { "RD", (int)GestureAction::ScrollBottom, L"滚动到列表底部" },
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
            { L"复制选中项", L"C", false, true, false },
            { L"剪切选中项", L"X", false, true, false },
            { L"粘贴", L"V", false, true, false },
            { L"新建文件夹", L"N", true, true, false },
            { L"属性", L"Enter", false, false, true }
        };
    }
}

void Config::Save() {
    const wchar_t* secGeneral = L"General";
    const wchar_t* secWindow = L"Window";
    const wchar_t* secTabs = L"Tabs";

    auto writeInt = [this](const wchar_t* sec, const wchar_t* key, int val) {
        wchar_t buf[32];
        StringCchPrintfW(buf, 32, L"%d", val);
        WritePrivateProfileStringW(sec, key, buf, m_iniPath.c_str());
    };

    // Window
    writeInt(secWindow, L"X", window.x);
    writeInt(secWindow, L"Y", window.y);
    writeInt(secWindow, L"Width", window.width);
    writeInt(secWindow, L"Height", window.height);
    writeInt(secWindow, L"Maximized", window.maximized ? 1 : 0);

    // General
    writeInt(secGeneral, L"ThemeMode", themeMode);
    writeInt(secGeneral, L"Language", language);
    writeInt(secGeneral, L"ShowTree", showTree ? 1 : 0);
    writeInt(secGeneral, L"TreeWidth", treeWidth);
    writeInt(secGeneral, L"PaneLayout", paneLayout);
    writeInt(secGeneral, L"SyncNav", syncNav ? 1 : 0);
    writeInt(secGeneral, L"SplitEnabled", splitEnabled ? 1 : 0);
    writeInt(secGeneral, L"SplitRatio", splitRatio);
    writeInt(secGeneral, L"PreviewEnabled", previewEnabled ? 1 : 0);
    writeInt(secGeneral, L"PreviewWidth", previewWidth);
    writeInt(secGeneral, L"ViewMode", viewMode);
    writeInt(secGeneral, L"ShowHidden", showHidden ? 1 : 0);
    writeInt(secGeneral, L"CalcFolderSize", calcFolderSize ? 1 : 0);
    writeInt(secGeneral, L"GesturesEnabled", gesturesEnabled ? 1 : 0);
    writeInt(secGeneral, L"SingleInstance", singleInstance ? 1 : 0);
    writeInt(secGeneral, L"ZebraStriping", zebraStriping ? 1 : 0);
    writeInt(secGeneral, L"ShowGridLines", showGridLines ? 1 : 0);
    writeInt(secGeneral, L"LockToolbars", lockToolbars ? 1 : 0);
    writeInt(secGeneral, L"SearchBarFirst", searchBarFirst ? 1 : 0);
    writeInt(secGeneral, L"FilterBarWidth", filterBarWidth);
    writeInt(secGeneral, L"RestoreTabs", restoreTabsOnStartup ? 1 : 0);
    writeInt(secGeneral, L"MinimizeToTray", minimizeToTray ? 1 : 0);
    writeInt(secGeneral, L"CloseToTray", closeToTray ? 1 : 0);
    writeInt(secGeneral, L"ShowToolbarStandard", showToolbarStandard ? 1 : 0);
    writeInt(secGeneral, L"ShowToolbarAddress", showToolbarAddress ? 1 : 0);
    writeInt(secGeneral, L"ShowToolbarLinks", showToolbarLinks ? 1 : 0);
    writeInt(secGeneral, L"ShowToolbarSearch", showToolbarSearch ? 1 : 0);
    writeInt(secGeneral, L"ShowStatusBar", showStatusBar ? 1 : 0);
    writeInt(secGeneral, L"AllowDuplicateTabs", allowDuplicateTabs ? 1 : 0);
    writeInt(secGeneral, L"TabDblClickAction", tabDoubleClickAction);
    writeInt(secGeneral, L"AutoExpandTree", autoExpandTree ? 1 : 0);
    writeInt(secGeneral, L"SearchSubfolders", searchSubfoldersDefault ? 1 : 0);
    auto saveAction = [&](const wchar_t* prefix, const TargetActionConfig& tac) {
        wchar_t k[64];
        StringCchPrintfW(k, 64, L"%s_OpenCurrent", prefix); writeInt(secGeneral, k, tac.openCurrentPane);
        StringCchPrintfW(k, 64, L"%s_OpenNewTab", prefix); writeInt(secGeneral, k, tac.openNewTab);
        StringCchPrintfW(k, 64, L"%s_OpenBgTab", prefix); writeInt(secGeneral, k, tac.openBgTab);
        StringCchPrintfW(k, 64, L"%s_OpenOtherPane", prefix); writeInt(secGeneral, k, tac.openOtherPane);
        StringCchPrintfW(k, 64, L"%s_OpenExplorer", prefix); writeInt(secGeneral, k, tac.openExplorer);
    };
    saveAction(L"ActionTree", actionTree);
    saveAction(L"ActionLinks", actionLinks);
    saveAction(L"ActionAddress", actionAddress);

    // Tabs
    WritePrivateProfileSectionW(secTabs, L"", m_iniPath.c_str());
    int saveCount = !sessionTabsInfo.empty() ? (int)sessionTabsInfo.size() : (int)sessionTabs.size();
    writeInt(secTabs, L"Count", saveCount);
    writeInt(secTabs, L"Active", activeTabIndex);
    if (!sessionTabsInfo.empty()) {
        for (size_t i = 0; i < sessionTabsInfo.size(); ++i) {
            wchar_t key[64];
            StringCchPrintfW(key, 64, L"Tab%d", (int)i);
            WritePrivateProfileStringW(secTabs, key, sessionTabsInfo[i].path.c_str(), m_iniPath.c_str());

            StringCchPrintfW(key, 64, L"Tab%d_Layout", (int)i);
            writeInt(secTabs, key, sessionTabsInfo[i].layout);

            StringCchPrintfW(key, 64, L"Tab%d_ActivePane", (int)i);
            writeInt(secTabs, key, sessionTabsInfo[i].activePaneIndex);

            StringCchPrintfW(key, 64, L"Tab%d_PaneCount", (int)i);
            writeInt(secTabs, key, (int)sessionTabsInfo[i].panePaths.size());

            for (size_t p = 0; p < sessionTabsInfo[i].panePaths.size(); ++p) {
                wchar_t pKey[64];
                StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d", (int)i, (int)p);
                WritePrivateProfileStringW(secTabs, pKey, sessionTabsInfo[i].panePaths[p].c_str(), m_iniPath.c_str());

                if (p < sessionTabsInfo[i].paneViewStates.size()) {
                    const auto& vs = sessionTabsInfo[i].paneViewStates[p];
                    StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_SortCol", (int)i, (int)p);
                    writeInt(secTabs, pKey, vs.sortCol);
                    StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_SortAsc", (int)i, (int)p);
                    writeInt(secTabs, pKey, vs.sortAsc ? 1 : 0);
                    StringCchPrintfW(pKey, 64, L"Tab%d_Pane%d_ViewMode", (int)i, (int)p);
                    writeInt(secTabs, pKey, vs.viewMode);
                }
            }
        }
    } else {
        for (size_t i = 0; i < sessionTabs.size(); ++i) {
            wchar_t key[32];
            StringCchPrintfW(key, 32, L"Tab%d", (int)i);
            WritePrivateProfileStringW(secTabs, key, sessionTabs[i].c_str(), m_iniPath.c_str());
        }
    }

    // Gestures
    const wchar_t* secGestures = L"Gestures";
    WritePrivateProfileSectionW(secGestures, L"", m_iniPath.c_str());
    writeInt(secGestures, L"Count", (int)gestures.size());
    for (size_t i = 0; i < gestures.size(); ++i) {
        wchar_t kCode[32], kAct[32], kName[32];
        StringCchPrintfW(kCode, 32, L"Code%d", (int)i);
        StringCchPrintfW(kAct, 32, L"Action%d", (int)i);
        StringCchPrintfW(kName, 32, L"Name%d", (int)i);

        std::wstring wCode = Util::Utf8ToWide(gestures[i].code);
        WritePrivateProfileStringW(secGestures, kCode, wCode.c_str(), m_iniPath.c_str());
        writeInt(secGestures, kAct, gestures[i].action);
        WritePrivateProfileStringW(secGestures, kName, gestures[i].name.c_str(), m_iniPath.c_str());
    }

    // Shortcuts
    const wchar_t* secShortcuts = L"Shortcuts";
    WritePrivateProfileSectionW(secShortcuts, L"", m_iniPath.c_str());
    writeInt(secShortcuts, L"Count", (int)shortcuts.size());
    for (size_t i = 0; i < shortcuts.size(); ++i) {
        wchar_t kName[32], kKey[32], kShift[32], kCtrl[32], kAlt[32];
        StringCchPrintfW(kName, 32, L"Name%d", (int)i);
        StringCchPrintfW(kKey, 32, L"Key%d", (int)i);
        StringCchPrintfW(kShift, 32, L"Shift%d", (int)i);
        StringCchPrintfW(kCtrl, 32, L"Ctrl%d", (int)i);
        StringCchPrintfW(kAlt, 32, L"Alt%d", (int)i);

        WritePrivateProfileStringW(secShortcuts, kName, shortcuts[i].name.c_str(), m_iniPath.c_str());
        WritePrivateProfileStringW(secShortcuts, kKey, shortcuts[i].key.c_str(), m_iniPath.c_str());
        writeInt(secShortcuts, kShift, shortcuts[i].shift ? 1 : 0);
        writeInt(secShortcuts, kCtrl, shortcuts[i].ctrl ? 1 : 0);
        writeInt(secShortcuts, kAlt, shortcuts[i].alt ? 1 : 0);
    }
}
