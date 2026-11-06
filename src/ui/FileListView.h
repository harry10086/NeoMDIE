#pragma once
#include "..\Common.h"
#include "..\core\DropTarget.h"
#include "..\core\FileSystemWatcher.h"
#include "..\core\FolderSizeCalculator.h"
#include "..\core\GestureEngine.h"
#include "..\core\ShellItem.h"


class FileListView {
public:
  FileListView();
  ~FileListView();

  HWND Create(HWND hwndParent, int id);
  HWND GetHwnd() const { return m_hwnd; }

  bool NavigateTo(const std::wstring &path, bool recordHistory = true);
  std::wstring GetCurrentPath() const { return m_currentPath; }

  void Refresh();
  void GoBack();
  void GoForward();
  void GoUp();

  bool CanGoBack() const { return m_historyIndex > 0; }
  bool CanGoForward() const {
    return m_historyIndex + 1 < (int)m_history.size();
  }
  const std::vector<std::wstring> &GetHistory() const { return m_history; }
  int GetHistoryIndex() const { return m_historyIndex; }
  void SetHistory(const std::vector<std::wstring> &history, int historyIndex) {
    m_history = history;
    m_historyIndex = historyIndex;
  }
  void ClearHistory(const std::wstring &initialPath) {
    m_history.clear();
    if (!initialPath.empty()) {
      m_history.push_back(initialPath);
      m_historyIndex = 0;
    } else {
      m_historyIndex = -1;
    }
  }
  void SelectItemByPath(const std::wstring &path);

  void SetFilter(const std::wstring &query, bool subfolders = false,
                 bool matchCase = false);
  const std::wstring &GetFilterQuery() const { return m_filterQuery; }
  void SearchRecursive(const std::wstring &rootDir, const std::wstring &query,
                       bool matchCase);
  void SetViewMode(int mode); // 0: Details, 1: Large Icons, 2: Small Icons, 3:
                              // List, 4: Tiles
  int GetViewMode() const { return m_viewMode; }
  void SetShowHidden(bool show);
  void SetCalcFolderSize(bool calc);
  void UpdateColumnsLanguage();
  void UpdateHeaderSortArrow();
  void ShowColumnHeaderMenu(POINT ptScreen);
  void RebuildColumns();
  void ToggleColumn(ColumnId id);
  void ShowChooseColumnsDialog();
  const std::vector<ColumnId> &GetActiveColumns() const {
    return m_activeColumns;
  }
  void ApplyTheme();

  // Clipboard and file actions
  void CopySelected(bool isCut);
  void PasteToCurrent();
  void PasteShortcut();
  void DeleteSelected(bool permanent);
  void StartRename();

  void SetSort(SortColumn col, bool ascending);
  SortColumn GetSortColumn() const { return m_sortCol; }
  bool IsSortAscending() const { return m_sortAsc; }

  std::vector<std::wstring> GetSelectedPaths() const;
  void SelectAll();
  void InvertSelection();
  void ClearSelection();
  void SetSelectedPaths(const std::vector<std::wstring> &paths);
  PaneViewState GetViewState() const;
  void RestoreViewState(const PaneViewState &state);
  void NotifyStatus();

  void ScrollToTop();
  void ScrollToBottom();
  void ScrollPageUp();
  void ScrollPageDown();

  void UpdateLayout(int x, int y, int width, int height);

  // Callbacks
  std::function<void(const std::wstring &newPath)> onPathChanged;
  std::function<void(const std::wstring &selectedFile)> onSelectionChanged;
  std::function<void(int count, uint64_t totalSize)> onStatusUpdated;
  std::function<void(GestureAction action)> onGestureAction;
  std::function<void(int neoCmd, const std::wstring &path)> onCustomCommand;

private:
  static LRESULT CALLBACK ParentSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam,
                                             UINT_PTR uIdSubclass,
                                             DWORD_PTR dwRefData);
  static LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam, UINT_PTR uIdSubclass,
                                           DWORD_PTR dwRefData);
  static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam, UINT_PTR uIdSubclass,
                                           DWORD_PTR dwRefData);
  static LRESULT CALLBACK HeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam,
                                             UINT_PTR uIdSubclass,
                                             DWORD_PTR dwRefData);

  void InitColumns();
  void ApplyFilter();
  void SortItems();
  void OnItemDoubleClicked(int itemIndex);
  void OnItemRightClicked(POINT ptScreen);
  void OnFolderSizeReady(FolderSizeResult *pRes);

  HWND m_hwndParent = nullptr;
  HWND m_hwnd = nullptr;
  int m_controlId = 0;

  std::wstring m_currentPath;
  std::vector<NeoShellItem> m_allItems;
  std::vector<int> m_filteredIndices;

  std::vector<std::wstring> m_history;
  int m_historyIndex = -1;

  std::wstring m_filterQuery;
  std::vector<ColumnId> m_activeColumns;
  SortColumn m_sortCol = SortColumn::Name;
  bool m_sortAsc = true;
  bool m_showHidden = false;
  bool m_calcFolderSize = false;

  int m_dropHighlightItem = -1;
  DropTarget *m_pDropTarget = nullptr;
  void StartDragDrop(int itemIndex);

  FileSystemWatcher m_watcher;
  FolderSizeCalculator m_calcSize;
  GestureEngine m_gesture;

  int m_viewMode = 0;
  std::atomic<bool> m_searchCancelled{false};
  std::thread m_searchThread;

  static const UINT WM_NOTIFY_DIR_CHANGED = WM_APP + 101;
  static const UINT WM_NOTIFY_FOLDER_SIZE = WM_APP + 102;
  static const UINT WM_SEARCH_BATCH = WM_APP + 103;
  static const UINT WM_SEARCH_COMPLETE = WM_APP + 104;
};
