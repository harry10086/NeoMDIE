#include "Config.h"

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
    int tabCount = GetPrivateProfileIntW(secTabs, L"Count", 0, m_iniPath.c_str());
    activeTabIndex = GetPrivateProfileIntW(secTabs, L"Active", 0, m_iniPath.c_str());

    for (int i = 0; i < tabCount; ++i) {
        wchar_t key[32];
        StringCchPrintfW(key, 32, L"Tab%d", i);
        wchar_t val[MAX_PATH];
        GetPrivateProfileStringW(secTabs, key, L"", val, MAX_PATH, m_iniPath.c_str());
        if (wcslen(val) > 0) {
            sessionTabs.push_back(val);
        }
    }

    if (sessionTabs.empty()) {
        // Default to Computer / User profile
        wchar_t userProfile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            sessionTabs.push_back(userProfile);
        } else {
            sessionTabs.push_back(L"C:\\");
        }
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
                gestures.push_back({ Util::WideToUtf8(vCode), vAct, vName });
            }
        }
    }
    if (gestures.empty()) {
        gestures = {
            { "L", 1, L"后退" },
            { "R", 2, L"前进" },
            { "U", 3, L"向上一级" },
            { "D", 4, L"关闭标签页" },
            { "UD", 6, L"刷新" },
            { "DR", 5, L"新建标签页" },
            { "UL", 7, L"复制标签页" },
            { "UR", 8, L"切换目录树" },
            { "LDR", 13, L"滚动到列表首端" },
            { "RDL", 14, L"滚动到列表末端" },
            { "LUR", 15, L"向上翻页" },
            { "RUL", 16, L"向下翻页" }
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
    writeInt(secTabs, L"Count", (int)sessionTabs.size());
    writeInt(secTabs, L"Active", activeTabIndex);
    for (size_t i = 0; i < sessionTabs.size(); ++i) {
        wchar_t key[32];
        StringCchPrintfW(key, 32, L"Tab%d", (int)i);
        WritePrivateProfileStringW(secTabs, key, sessionTabs[i].c_str(), m_iniPath.c_str());
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
