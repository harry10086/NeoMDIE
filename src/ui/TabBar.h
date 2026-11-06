#pragma once
#include "..\Common.h"

struct PaneHistory {
  std::vector<std::wstring> history;
  int historyIndex = -1;
};

struct TabItem {
  std::wstring title;
  std::wstring path;
  int iconIndex = 0;
  bool isPinned = false;
  COLORREF color = 0;
  RECT rc = {0};
  RECT rcClose = {0};

  // Multi-pane state per tab
  int layout = 0; // 0: Single
  std::vector<std::wstring> panePaths;
  int activePaneIndex = 0;
  std::vector<PaneHistory>
      paneHistories; // Independent navigation history per pane in this tab
  std::vector<std::vector<std::wstring>>
      paneSelectedPaths; // Selected paths per pane in this tab
  std::vector<PaneViewState> paneViewStates;
};

class TabBar {
public:
  TabBar();
  ~TabBar();

  HWND Create(HWND hwndParent, int id);
  HWND GetHwnd() const { return m_hwnd; }

  int AddTab(const std::wstring &path, bool activate = true);
  void CloseTab(int index);
  void CloseOtherTabs(int index);
  void CloseTabsToLeft(int index);
  void CloseTabsToRight(int index);
  void CloseAllTabs();
  void ReopenClosedTab();
  const std::vector<std::wstring> &GetClosedHistory() const {
    return m_closedHistory;
  }
  void NextTab();
  void PrevTab();
  void TogglePinTab(int index);
  bool IsTabPinned(int index) const;
  void SetActiveTab(int index, bool force = false);
  int GetActiveIndex() const { return m_activeIndex; }
  int GetTabCount() const { return (int)m_tabs.size(); }
  const std::vector<TabItem> &GetTabs() const { return m_tabs; }
  TabItem &GetTabItemRef(int index) {
    return m_tabs[std::clamp(index, 0, (int)m_tabs.size() - 1)];
  }
  const TabItem &GetTabItem(int index) const {
    return m_tabs[std::clamp(index, 0, (int)m_tabs.size() - 1)];
  }

  std::wstring GetTabPath(int index) const;
  void SetTabPath(int index, const std::wstring &path);

  void SetTabColor(int index, COLORREF color);
  COLORREF GetTabColor(int index) const;

  void UpdateLayout(int width, int height);
  void EnsureTabVisible(int index);
  void ScrollBy(int delta);

  // Callbacks
  std::function<void(int index)> onTabChanged;
  std::function<void(int index)> onTabClosed;
  std::function<void()> onTabOrderChanged;
  std::function<void()> onNewTabRequested;
  std::function<void(const std::wstring &path)> onOpenTerminalRequested;

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
  LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void Draw(HDC hdc);
  void RecalcLayout();
  void ShowTabContextMenu(int index, POINT ptScreen);

  HWND m_hwnd = nullptr;
  std::vector<TabItem> m_tabs;
  std::vector<std::wstring> m_closedHistory;
  int m_activeIndex = -1;
  int m_hoverIndex = -1;
  bool m_hoverClose = false;
  bool m_hoverNewTab = false;
  RECT m_rcNewTab = {0};

  bool m_dragging = false;
  int m_dragIndex = -1;
  class DropTarget *m_pDropTarget = nullptr;
  DWORD m_hoverTabTick = 0;
  int m_hoverTabCandidate = -1;
  POINT m_dragStart = {0, 0};

  // Tooltip
  HWND m_hwndTooltip = nullptr;
  std::wstring m_tooltipText;
  void UpdateTooltip(int index);

  // Overflow scroll
  bool m_showScrollButtons = false;
  int m_scrollOffset = 0;
  RECT m_rcScrollLeft = {0};
  RECT m_rcScrollRight = {0};
  bool m_hoverScrollLeft = false;
  bool m_hoverScrollRight = false;
};
