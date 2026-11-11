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

struct FolderViewSettings {
    int sortCol = -1;      // -1: default / not set
    bool sortAsc = true;
    int viewMode = -1;     // -1: default / not set
    int foldersFirst = -1; // -1: not set / use global, 0: files first, 1: folders first
    std::unordered_map<int, int> columnWidths; // colId -> width in px
};

class Config {
public:
    static Config& Get();

    void Load();
    void Save();

    std::wstring GetConfigFilePath() const { return m_iniPath; }
    std::wstring GetIniPath() const { return m_iniPath; }

    static std::wstring NormalizeFolderKey(const std::wstring& path) {
        if (path.empty()) return L"";
        std::wstring k = path;
        for (auto& ch : k) ch = towlower(ch);
        while (k.length() > 3 && (k.back() == L'\\' || k.back() == L'/')) {
            k.pop_back();
        }
        return k;
    }

    WindowPlacementConfig window;
    int themeMode = -1; // -1: auto, 0: light, 1: dark
    int accentColorMode = 0; // 0: Neo Blue, 1: Emerald/Teal, 2: Purple, 3: Orange, 4: Red, 5: Slate, 6: Custom
    COLORREF customAccentColor = RGB(0, 120, 212);
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
    bool hideDotFiles = true;  // 默认隐藏 . 开头的点文件/点文件夹
    bool showItemCheckBoxes = false; // 项目复选框 (Item check boxes)
    int listDensity = 1;       // 间距调整: 0: 紧凑, 1: 适中 (默认), 2: 宽松
    int listScale = 1;         // 放大调整: 0: 偏小 (9pt/90%), 1: 标准 (10pt/100%, 默认), 2: 放大 (12pt/125%)
    bool foldersFirst = true;
    bool calcFolderSize = false;
    bool zebraStriping = false;
    bool showGridLines = false;
    bool singleInstance = true;
    bool gesturesEnabled = true;

    // Per-folder settings and global fallback column widths
    std::unordered_map<int, int> columnWidths;
    std::unordered_map<std::wstring, FolderViewSettings> folderViewMap;

    int GetColumnWidth(int colId, int defaultWidth) const {
        auto it = columnWidths.find(colId);
        if (it != columnWidths.end() && it->second > 20) {
            return it->second;
        }
        return defaultWidth;
    }
    void SetColumnWidth(int colId, int width) {
        if (width > 20) {
            columnWidths[colId] = width;
        }
    }

    bool HasFolderView(const std::wstring& path) const {
        std::wstring key = NormalizeFolderKey(path);
        return folderViewMap.find(key) != folderViewMap.end();
    }

    FolderViewSettings GetFolderView(const std::wstring& path) const {
        std::wstring key = NormalizeFolderKey(path);
        auto it = folderViewMap.find(key);
        if (it != folderViewMap.end()) {
            return it->second;
        }
        FolderViewSettings def;
        def.viewMode = viewMode;
        return def;
    }

    int GetFolderColumnWidth(const std::wstring& path, int colId, int defaultWidth) const {
        std::wstring key = NormalizeFolderKey(path);
        auto it = folderViewMap.find(key);
        if (it != folderViewMap.end()) {
            auto cit = it->second.columnWidths.find(colId);
            if (cit != it->second.columnWidths.end() && cit->second > 20) {
                return cit->second;
            }
        }
        return GetColumnWidth(colId, defaultWidth);
    }

    void SetFolderColumnWidth(const std::wstring& path, int colId, int width) {
        if (path.empty() || width <= 20) return;
        std::wstring key = NormalizeFolderKey(path);
        folderViewMap[key].columnWidths[colId] = width;
        SetColumnWidth(colId, width);
    }

    void SetFolderSort(const std::wstring& path, int sortCol, bool sortAsc) {
        if (path.empty()) return;
        std::wstring key = NormalizeFolderKey(path);
        folderViewMap[key].sortCol = sortCol;
        folderViewMap[key].sortAsc = sortAsc;
    }

    void SetFolderViewMode(const std::wstring& path, int mode) {
        if (path.empty() || mode < 0) return;
        std::wstring key = NormalizeFolderKey(path);
        folderViewMap[key].viewMode = mode;
    }

    void SetFolderFoldersFirst(const std::wstring& path, bool first) {
        if (path.empty()) return;
        std::wstring key = NormalizeFolderKey(path);
        folderViewMap[key].foldersFirst = first ? 1 : 0;
    }

    std::vector<GestureEntry> gestures;
    std::vector<ShortcutItemConfig> shortcuts;

    struct SessionTabInfo {
        std::wstring path;
        int layout = 0;
        int activePaneIndex = 0;
        std::vector<std::wstring> panePaths;
        std::vector<PaneViewState> paneViewStates;
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
    bool showMenuBar = false;
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

    // Quick Access (快速访问)
    std::vector<std::wstring> quickAccessFolders;
    void InitDefaultQuickAccess();
    bool AddQuickAccess(const std::wstring& path);
    bool RemoveQuickAccess(const std::wstring& path);
    void ResetQuickAccess();

private:
    Config();
    std::wstring m_iniPath;
};
