#pragma once
#include "..\Common.h"
#include "FileListView.h"

enum class PaneLayout {
  Single = 0,         // 1x1
  DualVertical = 1,   // 1x2 Left-Right
  DualHorizontal = 2, // 2x1 Top-Bottom
  Quad = 3,           // 2x2 4-Grid
  ThreeLeft = 4       // 1 Left, 2 Right
};

class MultiPaneContainer {
public:
  MultiPaneContainer();
  ~MultiPaneContainer();

  HWND Create(HWND hwndParent, int id);
  HWND GetHwnd() const { return m_hwnd; }

  void SetLayout(PaneLayout layout);
  PaneLayout GetLayout() const { return m_layout; }

  FileListView *GetActivePane();
  int GetActiveIndex() const { return m_activePane; }
  void SetActivePane(int index, bool force = false);

  FileListView &GetPane(int index) { return m_panes[std::clamp(index, 0, 3)]; }
  int GetPaneCount() const;

  void CycleActivePane();
  void SwapPanes(int idxA, int idxB);
  void ClosePane(int index);
  void ToggleMaximizePane(int index);
  void MinimizePane(int index);

  void UpdateLayout(int x, int y, int width, int height);
  void UpdatePaneHeader(int index);
  void ApplyTheme();
  void LayoutChildren();
  void SetShowHidden(bool show);
  void SetCalcFolderSize(bool calc);
  void ReSortAllPanes();

  std::function<void(int activeIndex, const std::wstring &path)> onActivePaneChanged;
  std::function<void(PaneLayout newLayout)> onLayoutChanged;
  std::function<void(GestureAction action)> onGestureAction;
  std::function<void(int neoCmd, const std::wstring &path)> onCustomCommand;
  std::function<void(int paneIdx, POINT ptScreen)> onPaneDragMove;
  std::function<void(int paneIdx, POINT ptScreen)> onPaneDragEnd;

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK HeaderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK PaneSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

  LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT HandleHeaderMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, int paneIdx);

  HWND m_hwnd = nullptr;
  HWND m_hHeaders[4] = { nullptr, nullptr, nullptr, nullptr };
  FileListView m_panes[4];
  int m_activePane = 0;
  PaneLayout m_layout = PaneLayout::Single;
  PaneLayout m_prevSplitLayout = PaneLayout::DualVertical;
  int m_maximizedPane = -1;

  int m_splitH = 50; // percentage
  int m_splitV = 50; // percentage

  bool m_draggingSplitH = false;
  bool m_draggingSplitV = false;

  bool m_draggingPane = false;
  int m_dragPaneIdx = -1;
  POINT m_dragPaneStart = {0, 0};
};
