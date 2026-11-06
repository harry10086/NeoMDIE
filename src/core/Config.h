#pragma once
#include "..\Common.h"

struct WindowPlacementConfig {
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = 1100;
    int height = 700;
    bool maximized = true;
};

struct GestureEntry {
    std::string code;    // e.g. "L", "R", "U", "D", "UD", "DR", "UL", etc.
    int action;          // GestureAction integer
    std::wstring name;   // Display name
};

struct ShortcutItemConfig {
    std::wstring name;
    std::wstring key;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

enum class ActionTrigger {
    None = 0,
    LeftClick = 1,
    DoubleClick = 2,
    MiddleClick = 3,
    ShiftClick = 4,
    CtrlClick = 5,
    AltClick = 6,
    ShiftCtrlClick = 7,
    ShiftAltClick = 8,
    CtrlAltClick = 9,
    ShiftCtrlAltClick = 10,
    ShiftMiddleClick = 11,
    CtrlMiddleClick = 12
};

enum class WindowAction {
    None = 0,
    CurrentPane = 1,
    NewTab = 2,
    BgTab = 3,
    OtherPane = 4,
    Explorer = 5
};

struct TargetActionConfig {
    int openCurrentPane = (int)ActionTrigger::LeftClick;      // 在当前栏打开
    int openNewTab = (int)ActionTrigger::CtrlClick;           // 在新标签页打开
    int openBgTab = (int)ActionTrigger::ShiftCtrlClick;       // 在后台新标签页打开
    int openOtherPane = (int)ActionTrigger::ShiftClick;       // 在其他栏打开
    int openExplorer = (int)ActionTrigger::None;              // 在系统资源管理器中打开

    WindowAction ResolveAction(ActionTrigger trigger) const {
        int t = (int)trigger;
        if (t == 0) return WindowAction::None;
        if (openBgTab == t) return WindowAction::BgTab;
        if (openNewTab == t) return WindowAction::NewTab;
        if (openOtherPane == t) return WindowAction::OtherPane;
        if (openExplorer == t) return WindowAction::Explorer;
        if (openCurrentPane == t) return WindowAction::CurrentPane;
        return WindowAction::None;
    }
};

inline ActionTrigger GetCurrentTrigger(bool isMiddle = false, bool isDouble = false) {
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;
    if (isMiddle) {
        if (shift) return ActionTrigger::ShiftMiddleClick;
        if (ctrl) return ActionTrigger::CtrlMiddleClick;
        return ActionTrigger::MiddleClick;
    }
    if (isDouble) {
        return ActionTrigger::DoubleClick;
    }
    if (shift && ctrl && alt) return ActionTrigger::ShiftCtrlAltClick;
    if (shift && ctrl) return ActionTrigger::ShiftCtrlClick;
    if (shift && alt) return ActionTrigger::ShiftAltClick;
    if (ctrl && alt) return ActionTrigger::CtrlAltClick;
    if (shift) return ActionTrigger::ShiftClick;
    if (ctrl) return ActionTrigger::CtrlClick;
    if (alt) return ActionTrigger::AltClick;
    return ActionTrigger::LeftClick;
}


class Config {
public:
    static Config& Get();

    void Load();
    void Save();

    std::wstring GetConfigFilePath() const { return m_iniPath; }
    std::wstring GetIniPath() const { return m_iniPath; }

    WindowPlacementConfig window;
    int themeMode = -1; // -1: auto, 0: light, 1: dark
    int language = -1;  // -1: auto, 0: Chinese, 1: English
    bool showTree = true;
    int treeWidth = 280;
    int paneLayout = 0; // 0: Single, 1: DualVertical, 2: DualHorizontal, 3: Quad, 4: ThreeLeft
    bool syncNav = false;
    bool splitEnabled = false;
    int splitRatio = 50; // percentage
    bool previewEnabled = false;
    int previewWidth = 320;
    int viewMode = 0; // 0: Details, 1: Large Icons, 2: Small Icons, 3: List, 4: Tiles
    bool showHidden = false;
    bool calcFolderSize = false;
    bool zebraStriping = false;
    bool showGridLines = false;
    bool singleInstance = true;
    bool gesturesEnabled = true;
    std::vector<GestureEntry> gestures;
    std::vector<ShortcutItemConfig> shortcuts;

    struct SessionTabInfo {
        std::wstring path;
        int layout = 0;
        int activePaneIndex = 0;
        std::vector<std::wstring> panePaths;
    };
    std::vector<SessionTabInfo> sessionTabsInfo;
    std::vector<std::wstring> sessionTabs;
    int activeTabIndex = 0;

    // Advanced MDIE settings
    bool lockToolbars = false;
    bool searchBarFirst = false;
    int filterBarWidth = 240;
    bool restoreTabsOnStartup = true;
    bool minimizeToTray = false;
    bool closeToTray = false;
    bool showToolbarStandard = true;
    bool showToolbarAddress = true;
    bool showToolbarLinks = true;
    bool showToolbarSearch = true;
    bool showStatusBar = true;
    bool allowDuplicateTabs = true;
    int tabDoubleClickAction = 0; // 0: Close, 1: Lock, 2: Refresh, 3: Up
    bool autoExpandTree = true;
    bool searchSubfoldersDefault = false;

    // Window Actions (窗口动作)
    TargetActionConfig actionTree;
    TargetActionConfig actionLinks;
    TargetActionConfig actionAddress;

private:
    Config();
    std::wstring m_iniPath;
};
