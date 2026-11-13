#pragma once
#include "..\Common.h"
#include "TabBar.h"
#include "BreadcrumbBar.h"
#include "FilterBar.h"
#include "LinksBar.h"
#include "MultiPaneContainer.h"
#include "DropIndicatorOverlay.h"
#include "PreviewPane.h"
#include "DirectoryTree.h"

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    HWND Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialPath = L"");
    HWND GetHwnd() const { return m_hwnd; }
    HACCEL GetAcceleratorTable() const { return m_hAccel; }
    void RebuildAccelerators();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void BuildMenu();
    void LayoutAll();
    void OnCommand(UINT cmdId);
    void UpdateStatus(int itemCount, uint64_t totalSize);
    void UpdateNavButtons();
    void UpdateLanguageUI();
    void TogglePreview();
    void ToggleTree();
    void ToggleLinksBar();
    void SetLayout(PaneLayout layout, const std::wstring& targetNewPath = L"");
    void ShowLayoutMenu(POINT ptScreen);
    void ShowToolbarContextMenu(POINT ptScreen);
    void AddToHistory(const std::wstring& path);
    void DrawToolbarButton(DRAWITEMSTRUCT* pdis);

    struct SplitDropAction {
        enum Type {
            None,
            SplitDualVerticalRight,
            SplitDualVerticalLeft,
            SplitDualHorizontalBottom,
            SplitDualHorizontalTop,
            SplitThreeRightBottom,
            SplitThreeRightTop,
            SplitQuadBottomLeft,
            SplitQuad,
            MergeIntoPane
        } type = None;
        RECT overlayRect = { 0, 0, 0, 0 };
        std::wstring overlayText;
        int targetPaneIndex = 0;
    };

    void HandleTabDragMove(int dragIndex, int hostTabIndex, POINT ptScreen);
    void HandleTabDragEnd(int dragIndex, int hostTabIndex, POINT ptScreen);
    SplitDropAction CalculateDropAction(int dragIndex, int hostTabIndex, POINT ptScreen);
    enum class PaneDragActionType {
        None,
        DetachToTab,
        SwapPanes,
        DualVertical_To_DualHorizontal,
        DualHorizontal_To_DualVertical,
        SplitThreeRightBottom,
        SplitQuad
    };

    struct PaneDragAction {
        PaneDragActionType type = PaneDragActionType::None;
        RECT overlayRect = { 0, 0, 0, 0 };
        std::wstring overlayText;
        int targetPaneIdx = -1;
    };

    PaneDragAction CalculatePaneDropAction(int paneIdx, POINT ptScreen);
    void HandlePaneDragMove(int paneIdx, POINT ptScreen);
    void HandlePaneDragEnd(int paneIdx, POINT ptScreen);
    void UpdateWindowTitle(const std::wstring& path);
    void LayoutCenterArea();
    void ApplyTheme();
    void OnSettingsApplied();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void RestoreFromTrayOrMinimized();
    bool m_hasTrayIcon = false;
    bool m_forceClose = false;
    bool m_wasMaximizedBeforeTray = false;
    HACCEL m_hAccel = nullptr;

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    // Navigation buttons
    HWND m_hwndBtnBack = nullptr;
    HWND m_hwndBtnForward = nullptr;
    HWND m_hwndBtnUp = nullptr;
    HWND m_hwndBtnRefresh = nullptr;
    HWND m_hwndBtnTree = nullptr;
    HWND m_hwndBtnSplit = nullptr;
    HWND m_hwndBtnPreview = nullptr;
    HWND m_hwndBtnMenu = nullptr;
    HWND m_hwndTooltip = nullptr;
    HMENU m_hMenuBar = nullptr;

    TabBar m_tabBar;
    int m_currentTabIndex = -1;
    bool m_isSwitchingTab = false;
    BreadcrumbBar m_breadcrumb;
    FilterBar m_filterBar;
    LinksBar m_linksBar;
    DirectoryTree m_tree;
    MultiPaneContainer m_multiPane;
    DropIndicatorOverlay m_splitOverlay;
    PreviewPane m_preview;
    HWND m_hwndStatusBar = nullptr;

    bool m_linksVisible = true;
    bool m_treeVisible = true;
    int  m_treeWidth = 240;
    bool m_resizingTree = false;
    RECT m_rcTreeSplitter = { 0 };

    bool m_previewVisible = false;
    int  m_previewWidth = 320;
    bool m_resizingPreview = false;
    RECT m_rcPreviewSplitter = { 0 };

    bool m_resizingFilterBar = false;
    RECT m_rcFilterSplitter = { 0 };

    static const UINT WM_SYNC_TREE_SELECTION = WM_APP + 150;
    std::wstring m_pendingTreePath;

    std::vector<std::wstring> m_historyPaths;

    static const UINT IDC_BTN_BACK = 2001;
    static const UINT IDC_BTN_FORWARD = 2002;
    static const UINT IDC_BTN_UP = 2003;
    static const UINT IDC_BTN_REFRESH = 2004;
    static const UINT IDC_BTN_TREE = 2005;
    static const UINT IDC_BTN_SPLIT = 2006;
    static const UINT IDC_BTN_PREVIEW = 2007;
    static const UINT IDC_BTN_NEWTAB = 2008;
    static const UINT IDC_BTN_MENU = 2009;
};
