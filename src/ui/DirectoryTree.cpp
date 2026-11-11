#include "DirectoryTree.h"
#include "..\core\Config.h"
#include "..\core\DropTarget.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"
#include "Theme.h"
#include "resource.h"


DirectoryTree::DirectoryTree() {}

DirectoryTree::~DirectoryTree() {
  if (m_pDropTarget && m_hwnd) {
    RevokeDragDrop(m_hwnd);
    m_pDropTarget->Release();
    m_pDropTarget = nullptr;
  }
  if (m_hFontDensity) {
    DeleteObject(m_hFontDensity);
    m_hFontDensity = nullptr;
  }
  FreeAllNodeData();
}

void DirectoryTree::FreeAllNodeData() {
  for (auto *p : m_nodeDataList) {
    delete p;
  }
  m_nodeDataList.clear();
}

HWND DirectoryTree::Create(HWND hwndParent, int id) {
  m_hwndParent = hwndParent;
  m_controlId = id;
  HINSTANCE hInst = GetModuleHandleW(nullptr);

  DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                WS_HSCROLL | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;

  m_hwnd = CreateWindowExW(0, WC_TREEVIEWW, L"", style, 0, 0, 240, 400,
                           hwndParent, (HMENU)(INT_PTR)id, hInst, nullptr);

  TreeView_SetExtendedStyle(m_hwnd,
                            TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS | TVS_EX_AUTOHSCROLL,
                            TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS | TVS_EX_AUTOHSCROLL);
  SetWindowTheme(m_hwnd, L"Explorer", nullptr);

  HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(false);
  if (himlSmall) {
    TreeView_SetImageList(m_hwnd, himlSmall, TVSIL_NORMAL);
  }

  Theme::Get().ApplyToWindow(m_hwnd);
  ApplyTheme();
  ApplyViewMetrics();

  SetWindowSubclass(hwndParent, DirectoryTree::ParentSubclassProc,
                    (UINT_PTR)this, (DWORD_PTR)this);
  SetWindowSubclass(m_hwnd, DirectoryTree::TreeSubclassProc, (UINT_PTR)this,
                    (DWORD_PTR)this);

  // Register Drag & Drop
  m_pDropTarget = new DropTarget(
      m_hwnd, [this](POINT ptScreen, bool &outIsSubFolder) -> std::wstring {
        POINT ptClient = ptScreen;
        ScreenToClient(m_hwnd, &ptClient);
        TVHITTESTINFO tvhti = {0};
        tvhti.pt = ptClient;
        HTREEITEM hHit = TreeView_HitTest(m_hwnd, &tvhti);
        if (hHit) {
          TVITEMW item = {0};
          item.hItem = hHit;
          item.mask = TVIF_PARAM;
          if (TreeView_GetItem(m_hwnd, &item) && item.lParam) {
            TreeNodeData *pData = (TreeNodeData *)item.lParam;
            if (!pData->path.empty() && pData->path != L"This PC" &&
                pData->path != L"QuickAccess") {
              outIsSubFolder = true;
              return pData->path;
            }
          }
        }
        outIsSubFolder = false;
        return L"";
      });

  m_pDropTarget->onDragHover = [this](POINTL pt) {
    POINT ptClient = {pt.x, pt.y};
    ScreenToClient(m_hwnd, &ptClient);
    TVHITTESTINFO tvhti = {0};
    tvhti.pt = ptClient;
    HTREEITEM hHit = TreeView_HitTest(m_hwnd, &tvhti);
    if (m_hDropHighlight != hHit) {
      m_hDropHighlight = hHit;
      TreeView_SelectDropTarget(m_hwnd, m_hDropHighlight);
    }
  };

  m_pDropTarget->onDragLeave = [this]() {
    if (m_hDropHighlight) {
      m_hDropHighlight = nullptr;
      TreeView_SelectDropTarget(m_hwnd, nullptr);
    }
  };

  m_pDropTarget->onFilesDropped = [this](const std::wstring &targetDir,
                                         const std::vector<std::wstring> &files,
                                         bool isMove) {
    if (ShellUtils::CopyOrMoveFiles(m_hwnd, targetDir, files, isMove)) {
      Refresh();
    }
  };

RegisterDragDrop(m_hwnd, m_pDropTarget);

Populate();
return m_hwnd;
}

HTREEITEM DirectoryTree::FindOrLoadChildPath(HTREEITEM hParent,
                                             const std::wstring &targetPath) {
  if (!hParent)
    return nullptr;

  // Ensure children are loaded
  TVITEMW item = {0};
  item.hItem = hParent;
  item.mask = TVIF_PARAM;
  if (TreeView_GetItem(m_hwnd, &item) && item.lParam) {
    TreeNodeData *pData = (TreeNodeData *)item.lParam;
    if (!pData->childrenLoaded && !pData->path.empty() &&
        pData->path != L"This PC" && pData->path != L"QuickAccess") {
      PopulateChildren(hParent, pData->path);
      pData->childrenLoaded = true;
    }
  }

  std::wstring lowerTarget = Util::NormalizePath(targetPath);
  std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
                 ::towlower);
  if (lowerTarget.length() > 3 && lowerTarget.back() == L'\\') {
    lowerTarget.pop_back();
  }

  HTREEITEM hChild = TreeView_GetChild(m_hwnd, hParent);
  while (hChild) {
    item.hItem = hChild;
    item.mask = TVIF_PARAM;
    if (TreeView_GetItem(m_hwnd, &item) && item.lParam) {
      TreeNodeData *pChildData = (TreeNodeData *)item.lParam;
      std::wstring childPath = Util::NormalizePath(pChildData->path);
      std::transform(childPath.begin(), childPath.end(), childPath.begin(),
                     ::towlower);
      if (childPath.length() > 3 && childPath.back() == L'\\') {
        childPath.pop_back();
      }

      if (childPath == lowerTarget) {
        return hChild;
      }
    }
    hChild = TreeView_GetNextSibling(m_hwnd, hChild);
  }
  return nullptr;
}

void DirectoryTree::SelectPath(const std::wstring &path) {
  if (m_syncing || !m_hwnd)
    return;
  m_syncing = true;
  SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);

  std::wstring normPath = Util::NormalizePath(path);
  if (normPath.empty() || normPath == L"此电脑" || normPath == L"This PC") {
    TreeView_SelectItem(m_hwnd, m_hThisPC);
    TreeView_EnsureVisible(m_hwnd, m_hThisPC);
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwnd, nullptr, TRUE);
    m_syncing = false;
    return;
  }

  // UNC network paths (e.g. \192.168.188.79\share): do not freeze UI thread with deep tree expansion
  if (normPath.length() >= 2 && normPath[0] == L'\\' && normPath[1] == L'\\') {
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    m_syncing = false;
    return;
  }

  if (normPath.length() >= 2 && normPath[1] == L':') {
    wchar_t drivePrefix[4] = {(wchar_t)towupper(normPath[0]), L':', L'\\',
                              L'\0'};
    UINT dt = GetDriveTypeW(drivePrefix);
    if (dt == DRIVE_REMOTE) {
      // Remote mapped network drive: do not freeze UI thread with deep recursive tree expansion
      HTREEITEM hDrive = FindOrLoadChildPath(m_hThisPC, drivePrefix);
      if (hDrive) {
        TreeView_SelectItem(m_hwnd, hDrive);
        TreeView_EnsureVisible(m_hwnd, hDrive);
      }
      SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
      m_syncing = false;
      return;
    }
    HTREEITEM hDrive = FindOrLoadChildPath(m_hThisPC, drivePrefix);
    if (hDrive) {
      TreeView_Expand(m_hwnd, m_hThisPC, TVE_EXPAND);

      std::wstring currentSub = drivePrefix;
      HTREEITEM hCurrent = hDrive;

      if (normPath.length() > 3) {
        std::wistringstream ss(normPath.substr(3)); // skip "C:\"
        std::wstring segment;
        while (std::getline(ss, segment, L'\\')) {
          if (segment.empty())
            continue;
          currentSub = Util::CombinePath(currentSub, segment);
          HTREEITEM hNext = FindOrLoadChildPath(hCurrent, currentSub);
          if (hNext) {
            TreeView_Expand(m_hwnd, hCurrent, TVE_EXPAND);
            hCurrent = hNext;
          } else {
            break;
          }
        }
      }
      TreeView_SelectItem(m_hwnd, hCurrent);
      TreeView_EnsureVisible(m_hwnd, hCurrent);
    }
  }

  SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(m_hwnd, nullptr, TRUE);
  m_syncing = false;
}

void DirectoryTree::Populate() {
  TreeView_DeleteAllItems(m_hwnd);
  FreeAllNodeData();

  // 1. Quick Access root
  TVINSERTSTRUCTW tvis = {0};
  tvis.hParent = TVI_ROOT;
  tvis.hInsertAfter = TVI_LAST;
  tvis.item.mask =
      TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;
  tvis.item.pszText = (LPWSTR)I18n::Get().T(StringId::QuickAccess);
  tvis.item.iImage =
      ShellUtils::GetItemIconIndex(L"", FILE_ATTRIBUTE_DIRECTORY, true);
  tvis.item.iSelectedImage = tvis.item.iImage;
  tvis.item.cChildren = 1;

  TreeNodeData *pQAData = new TreeNodeData{L"QuickAccess", false, true};
  m_nodeDataList.push_back(pQAData);
  tvis.item.lParam = (LPARAM)pQAData;

  m_hQuickAccess = TreeView_InsertItem(m_hwnd, &tvis);
  PopulateQuickAccess(m_hQuickAccess);

  // 2. This PC root
  tvis.item.pszText = (LPWSTR)I18n::Get().T(StringId::ThisPC);
  tvis.item.iImage =
      ShellUtils::GetItemIconIndex(L"", FILE_ATTRIBUTE_DIRECTORY, true);
  tvis.item.iSelectedImage = tvis.item.iImage;
  tvis.item.cChildren = 1;

  TreeNodeData *pPCData = new TreeNodeData{L"This PC", false, true};
  m_nodeDataList.push_back(pPCData);
  tvis.item.lParam = (LPARAM)pPCData;

  m_hThisPC = TreeView_InsertItem(m_hwnd, &tvis);
  PopulateThisPC(m_hThisPC);

  // Expand roots by default
  TreeView_Expand(m_hwnd, m_hQuickAccess, TVE_EXPAND);
  TreeView_Expand(m_hwnd, m_hThisPC, TVE_EXPAND);
}

static bool HasSubdirectories(const std::wstring &folderPath, bool showHidden, bool hideDot) {
  std::wstring search = Util::CombinePath(folderPath, L"*");
  WIN32_FIND_DATAW fd;
  HANDLE hFind = FindFirstFileExW(search.c_str(), FindExInfoBasic, &fd,
                                  FindExSearchLimitToDirectories, nullptr, 0);
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }
  bool hasSub = false;
  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
      continue;
    if (hideDot && fd.cFileName[0] == L'.')
      continue;
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        (showHidden || !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))) {
      hasSub = true;
      break;
    }
  } while (FindNextFileW(hFind, &fd));
  FindClose(hFind);
  return hasSub;
}

void DirectoryTree::AddSpecialFolder(HTREEITEM hParent, const wchar_t *name,
                                     int csidl) {
  wchar_t path[MAX_PATH];
  if (SHGetFolderPathW(nullptr, csidl, nullptr, 0, path) == S_OK) {
    bool hideDot = Config::Get().hideDotFiles && !Config::Get().showHidden;
    bool hasSub = HasSubdirectories(path, Config::Get().showHidden, hideDot);

    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                     TVIF_CHILDREN | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)name;
    tvis.item.iImage =
        ShellUtils::GetItemIconIndex(path, FILE_ATTRIBUTE_DIRECTORY, true);
    tvis.item.iSelectedImage = tvis.item.iImage;
    tvis.item.cChildren = hasSub ? 1 : 0;

    TreeNodeData *pData = new TreeNodeData{path, false, false};
    m_nodeDataList.push_back(pData);
    tvis.item.lParam = (LPARAM)pData;

    HTREEITEM hItem = TreeView_InsertItem(m_hwnd, &tvis);

    if (hasSub) {
      // Dummy child item for lazy loading
      TVINSERTSTRUCTW dummy = {0};
      dummy.hParent = hItem;
      dummy.item.mask = TVIF_TEXT;
      dummy.item.pszText = (LPWSTR)L"";
      TreeView_InsertItem(m_hwnd, &dummy);
    }
  }
}

void DirectoryTree::PopulateQuickAccess(HTREEITEM hParent) {
  // Delete existing child items
  HTREEITEM hChild = TreeView_GetChild(m_hwnd, hParent);
  while (hChild) {
    HTREEITEM hNext = TreeView_GetNextSibling(m_hwnd, hChild);
    TreeView_DeleteItem(m_hwnd, hChild);
    hChild = hNext;
  }

  const auto &folders = Config::Get().quickAccessFolders;
  for (const auto &folderPath : folders) {
    if (folderPath.empty() || !PathFileExistsW(folderPath.c_str()))
      continue;

    std::wstring displayName = ShellUtils::GetDisplayName(folderPath);
    bool hideDot = Config::Get().hideDotFiles && !Config::Get().showHidden;
    bool hasSub = HasSubdirectories(folderPath, Config::Get().showHidden, hideDot);
    int iconIdx = ShellUtils::GetItemIconIndex(folderPath, FILE_ATTRIBUTE_DIRECTORY, true);

    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                     TVIF_CHILDREN | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)displayName.c_str();
    tvis.item.iImage = iconIdx;
    tvis.item.iSelectedImage = iconIdx;
    tvis.item.cChildren = hasSub ? 1 : 0;

    TreeNodeData *pData = new TreeNodeData{folderPath, false, false};
    m_nodeDataList.push_back(pData);
    tvis.item.lParam = (LPARAM)pData;

    HTREEITEM hItem = TreeView_InsertItem(m_hwnd, &tvis);

    if (hasSub) {
      TVINSERTSTRUCTW dummy = {0};
      dummy.hParent = hItem;
      dummy.item.mask = TVIF_TEXT;
      dummy.item.pszText = (LPWSTR)L"";
      TreeView_InsertItem(m_hwnd, &dummy);
    }
  }

  TVITEMW tvParent = {0};
  tvParent.hItem = hParent;
  tvParent.mask = TVIF_CHILDREN;
  tvParent.cChildren = TreeView_GetChild(m_hwnd, hParent) != nullptr ? 1 : 0;
  TreeView_SetItem(m_hwnd, &tvParent);
}

void DirectoryTree::RefreshQuickAccess() {
  if (m_hQuickAccess) {
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    PopulateQuickAccess(m_hQuickAccess);
    TreeView_Expand(m_hwnd, m_hQuickAccess, TVE_EXPAND);
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

void DirectoryTree::PopulateThisPC(HTREEITEM hParent) {
  std::vector<NeoShellItem> drives;
  ShellUtils::EnumerateDrives(drives);

  for (const auto &d : drives) {
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                     TVIF_CHILDREN | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)d.name.c_str();
    tvis.item.iImage = d.iconIndex;
    tvis.item.iSelectedImage = d.iconIndex;
    tvis.item.cChildren = 1;

    TreeNodeData *pData = new TreeNodeData{d.fullPath, true, false};
    m_nodeDataList.push_back(pData);
    tvis.item.lParam = (LPARAM)pData;

    HTREEITEM hDrive = TreeView_InsertItem(m_hwnd, &tvis);

    TVINSERTSTRUCTW dummy = {0};
    dummy.hParent = hDrive;
    dummy.item.mask = TVIF_TEXT;
    dummy.item.pszText = (LPWSTR)L"";
    TreeView_InsertItem(m_hwnd, &dummy);
  }
}

void DirectoryTree::PopulateChildren(HTREEITEM hParent,
                                     const std::wstring &folderPath) {
  SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);

  // Delete existing child items (including dummy)
  HTREEITEM hChild = TreeView_GetChild(m_hwnd, hParent);
  while (hChild) {
    HTREEITEM hNext = TreeView_GetNextSibling(m_hwnd, hChild);
    TreeView_DeleteItem(m_hwnd, hChild);
    hChild = hNext;
  }

  std::wstring search = Util::CombinePath(folderPath, L"*");
  WIN32_FIND_DATAW fd;
  HANDLE hFind = FindFirstFileExW(search.c_str(), FindExInfoBasic, &fd,
                                  FindExSearchNameMatch, nullptr,
                                  FIND_FIRST_EX_LARGE_FETCH);

  if (hFind == INVALID_HANDLE_VALUE) {
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    return;
  }

  bool hideDot = Config::Get().hideDotFiles && !Config::Get().showHidden;
  std::vector<std::pair<std::wstring, std::wstring>> subdirs;
  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
      continue;
    if (hideDot && fd.cFileName[0] == L'.')
      continue;
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        (Config::Get().showHidden || !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))) {
      std::wstring subPath = Util::CombinePath(folderPath, fd.cFileName);
      subdirs.push_back({fd.cFileName, subPath});
    }
  } while (FindNextFileW(hFind, &fd));
  FindClose(hFind);

  int defaultFolderIcon =
      ShellUtils::GetItemIconIndex(L"", FILE_ATTRIBUTE_DIRECTORY, true);

  for (const auto &item : subdirs) {
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                     TVIF_CHILDREN | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)item.first.c_str();
    tvis.item.iImage = defaultFolderIcon;
    tvis.item.iSelectedImage = defaultFolderIcon;

    bool hasSub = HasSubdirectories(item.second, Config::Get().showHidden, hideDot);
    tvis.item.cChildren = hasSub ? 1 : 0;

    TreeNodeData *pData = new TreeNodeData{item.second, false, false};
    m_nodeDataList.push_back(pData);
    tvis.item.lParam = (LPARAM)pData;

    HTREEITEM hItem = TreeView_InsertItem(m_hwnd, &tvis);

    if (hasSub) {
      TVINSERTSTRUCTW dummy = {0};
      dummy.hParent = hItem;
      dummy.item.mask = TVIF_TEXT;
      dummy.item.pszText = (LPWSTR)L"";
      TreeView_InsertItem(m_hwnd, &dummy);
    }
  }

  if (subdirs.empty()) {
    TVITEMW tvi = {0};
    tvi.hItem = hParent;
    tvi.mask = TVIF_CHILDREN;
    tvi.cChildren = 0;
    TreeView_SetItem(m_hwnd, &tvi);
  }

  SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
}

void DirectoryTree::Refresh() {
  std::wstring curPath;
  HTREEITEM hSel = TreeView_GetSelection(m_hwnd);
  if (hSel) {
    TVITEMW tvi = {0};
    tvi.hItem = hSel;
    tvi.mask = TVIF_PARAM;
    if (TreeView_GetItem(m_hwnd, &tvi) && tvi.lParam) {
      TreeNodeData *pData = (TreeNodeData *)tvi.lParam;
      if (pData)
        curPath = pData->path;
    }
  }
  Populate();
  if (!curPath.empty()) {
    SelectPath(curPath);
  }
}

void DirectoryTree::ApplyTheme() {
  if (!m_hwnd)
    return;
  bool isDark = Theme::Get().IsDarkMode();
  Theme::Get().ApplyToWindow(m_hwnd);
  SetWindowTheme(m_hwnd, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
  COLORREF bg = Theme::Get().ClrBackground();
  COLORREF text = Theme::Get().ClrTextPrimary();
  TreeView_SetBkColor(m_hwnd, bg);
  TreeView_SetTextColor(m_hwnd, text);
  SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                   SWP_NOACTIVATE);
  InvalidateRect(m_hwnd, nullptr, TRUE);
}

void DirectoryTree::ApplyViewMetrics() {
  if (!m_hwnd)
    return;

  int scale = Config::Get().listScale;
  int density = Config::Get().listDensity;

  int fontSizePt = 10;
  if (scale == 0) fontSizePt = 9;       // 偏小
  else if (scale == 2) fontSizePt = 12; // 放大

  int currentDpi = (int)Theme::Get().GetDpi(m_hwnd);
  int fontHeight = -MulDiv(fontSizePt, currentDpi, 72);

  LOGFONTW lf = { 0 };
  if (!SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &lf, 0)) {
    StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
  }
  lf.lfHeight = fontHeight;
  lf.lfWeight = FW_NORMAL;
  lf.lfQuality = CLEARTYPE_QUALITY;

  HFONT hNewFont = CreateFontIndirectW(&lf);
  SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)hNewFont, TRUE);

  if (m_hFontDensity) {
    DeleteObject(m_hFontDensity);
  }
  m_hFontDensity = hNewFont;

  // Apply item height for TreeView (间距调整)
  // 紧凑: ~20-22px
  // 适中: ~28-30px
  // 宽松: ~38-42px
  int baseH = (scale == 0) ? 20 : ((scale == 2) ? 26 : 22);
  int pad = (density == 0) ? 0 : ((density == 2) ? 14 : 6);
  int itemH = MulDiv(baseH + pad, currentDpi, 96);
  TreeView_SetItemHeight(m_hwnd, itemH);

  InvalidateRect(m_hwnd, nullptr, TRUE);
}

void DirectoryTree::UpdateLayout(int x, int y, int width, int height) {
  if (m_hwnd) {
    MoveWindow(m_hwnd, x, y, width, height, TRUE);
  }
}

LRESULT CALLBACK DirectoryTree::ParentSubclassProc(HWND hwnd, UINT msg,
                                                   WPARAM wParam, LPARAM lParam,
                                                   UINT_PTR uIdSubclass,
                                                   DWORD_PTR dwRefData) {
  DirectoryTree *pThis = (DirectoryTree *)dwRefData;

  switch (msg) {
  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->hwndFrom == pThis->m_hwnd) {
      switch (pnm->code) {
      case NM_CUSTOMDRAW: {
        NMTVCUSTOMDRAW *ptvcd = (NMTVCUSTOMDRAW *)lParam;
        switch (ptvcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
          return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
          HTREEITEM hItem = (HTREEITEM)ptvcd->nmcd.dwItemSpec;
          bool isSelected = (ptvcd->nmcd.uItemState & CDIS_SELECTED) ||
                            (TreeView_GetSelection(pThis->m_hwnd) == hItem);

          if (isSelected) {
            ptvcd->nmcd.uItemState &= ~(CDIS_FOCUS | CDIS_SELECTED);

            RECT rcText = {0};
            RECT rcRow = {0};
            if (TreeView_GetItemRect(pThis->m_hwnd, hItem, &rcText, TRUE) &&
                TreeView_GetItemRect(pThis->m_hwnd, hItem, &rcRow, FALSE)) {
              HDC hdc = ptvcd->nmcd.hdc;
              HIMAGELIST himl = TreeView_GetImageList(pThis->m_hwnd, TVSIL_NORMAL);
              int cxIcon = 16, cyIcon = 16;
              if (himl) ImageList_GetIconSize(himl, &cxIcon, &cyIcon);

              RECT rcIcon;
              rcIcon.left = rcText.left - cxIcon - Theme::Get().Scale(3, pThis->m_hwnd);
              rcIcon.top = rcRow.top + (rcRow.bottom - rcRow.top - cyIcon) / 2;
              rcIcon.right = rcIcon.left + cxIcon;
              rcIcon.bottom = rcIcon.top + cyIcon;

              int padY = Theme::Get().Scale(2, pThis->m_hwnd);
              RECT rcSel;
              rcSel.left = rcIcon.left - Theme::Get().Scale(4, pThis->m_hwnd);
              rcSel.right = rcText.right + Theme::Get().Scale(6, pThis->m_hwnd);
              rcSel.top = rcRow.top + padY;
              rcSel.bottom = rcRow.bottom - padY;

              bool isDark = Theme::Get().IsDarkMode();
              HBRUSH hbr = Theme::Get().BrushSelected();
              COLORREF clrBorder = isDark ? RGB(75, 75, 75) : RGB(180, 215, 255);
              HPEN hPen = CreatePen(PS_SOLID, 1, clrBorder);
              HGDIOBJ oldBrush = SelectObject(hdc, hbr);
              HGDIOBJ oldPen = SelectObject(hdc, hPen);
              int rad = Theme::Get().Scale(4, pThis->m_hwnd);
              RoundRect(hdc, rcSel.left, rcSel.top, rcSel.right, rcSel.bottom, rad, rad);
              SelectObject(hdc, oldBrush);
              SelectObject(hdc, oldPen);
              DeleteObject(hPen);

              // Get full item info (image, text, children, expanded state)
              TVITEMW tvi = {0};
              tvi.hItem = hItem;
              tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_CHILDREN | TVIF_STATE;
              tvi.stateMask = TVIS_EXPANDED;
              wchar_t text[MAX_PATH] = {0};
              tvi.pszText = text;
              tvi.cchTextMax = MAX_PATH;
              TreeView_GetItem(pThis->m_hwnd, &tvi);

              // 1. Draw Expander Triangle Glyph if item has children
              if (tvi.cChildren != 0) {
                int glyphW = Theme::Get().Scale(16, pThis->m_hwnd);
                int glyphH = Theme::Get().Scale(16, pThis->m_hwnd);
                RECT rcGlyph;
                rcGlyph.right = rcSel.left - Theme::Get().Scale(2, pThis->m_hwnd);
                rcGlyph.left = rcGlyph.right - glyphW;
                rcGlyph.top = rcRow.top + (rcRow.bottom - rcRow.top - glyphH) / 2;
                rcGlyph.bottom = rcGlyph.top + glyphH;

                bool isExp = (tvi.state & TVIS_EXPANDED) != 0;
                HTHEME hTheme = OpenThemeData(pThis->m_hwnd, L"TreeView");
                if (hTheme) {
                  DrawThemeBackground(hTheme, hdc, 2 /* TVP_GLYPH */,
                                      isExp ? 2 /* GLPS_OPENED */ : 1 /* GLPS_CLOSED */,
                                      &rcGlyph, nullptr);
                  CloseThemeData(hTheme);
                } else {
                  POINT pts[3];
                  if (isExp) {
                    pts[0] = { rcGlyph.left + glyphW / 4, rcGlyph.top + glyphH * 3 / 8 };
                    pts[1] = { rcGlyph.right - glyphW / 4, rcGlyph.top + glyphH * 3 / 8 };
                    pts[2] = { rcGlyph.left + glyphW / 2, rcGlyph.top + glyphH * 5 / 8 };
                  } else {
                    pts[0] = { rcGlyph.left + glyphW * 3 / 8, rcGlyph.top + glyphH / 4 };
                    pts[1] = { rcGlyph.left + glyphW * 3 / 8, rcGlyph.bottom - glyphH / 4 };
                    pts[2] = { rcGlyph.left + glyphW * 5 / 8, rcGlyph.top + glyphH / 2 };
                  }
                  HBRUSH hGlyphBr = CreateSolidBrush(isDark ? RGB(180, 180, 180) : RGB(100, 100, 100));
                  HGDIOBJ oldBr = SelectObject(hdc, hGlyphBr);
                  HGDIOBJ oldPn = SelectObject(hdc, GetStockObject(NULL_PEN));
                  Polygon(hdc, pts, 3);
                  SelectObject(hdc, oldBr);
                  SelectObject(hdc, oldPn);
                  DeleteObject(hGlyphBr);
                }
              }

              // 2. Draw Icon transparently
              int imgIdx = (tvi.state & TVIS_SELECTED) ? tvi.iSelectedImage : tvi.iImage;
              if (himl) {
                ImageList_Draw(himl, imgIdx, hdc, rcIcon.left, rcIcon.top, ILD_TRANSPARENT);
              }

              // 3. Draw Text transparently
              SetBkMode(hdc, TRANSPARENT);
              SetTextColor(hdc, Theme::Get().ClrTextPrimary());
              HFONT hFont = (HFONT)SendMessageW(pThis->m_hwnd, WM_GETFONT, 0, 0);
              HGDIOBJ oldFont = SelectObject(hdc, hFont ? hFont : Theme::Get().GetUIFont(pThis->m_hwnd));
              DrawTextW(hdc, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
              SelectObject(hdc, oldFont);

              return CDRF_SKIPDEFAULT;
            }
          }
          return CDRF_DODEFAULT;
        }
        }
        return CDRF_DODEFAULT;
      }
      case TVN_ITEMEXPANDINGW: {
        NMTREEVIEWW *pnmtv = (NMTREEVIEWW *)lParam;
        if (pnmtv->action == TVE_EXPAND) {
          TreeNodeData *pData = (TreeNodeData *)pnmtv->itemNew.lParam;
          if (pData && !pData->childrenLoaded && !pData->path.empty() &&
              pData->path != L"This PC" && pData->path != L"QuickAccess") {
            pThis->PopulateChildren(pnmtv->itemNew.hItem, pData->path);
            pData->childrenLoaded = true;
          }
        }
        break;
      }
      case TVN_BEGINDRAGW: {
        NMTREEVIEWW *pnmtv = (NMTREEVIEWW *)lParam;
        pThis->StartDragDrop(pnmtv->itemNew.hItem);
        break;
      }
      case TVN_SELCHANGEDW: {
        InvalidateRect(pThis->m_hwnd, nullptr, FALSE);
        if (pThis->m_syncing || (GetKeyState(VK_RBUTTON) < 0))
          break;
        NMTREEVIEWW *pnmtv = (NMTREEVIEWW *)lParam;
        TreeNodeData *pData = (TreeNodeData *)pnmtv->itemNew.lParam;
        if (pData) {
          if (pData->path == L"QuickAccess") {
            // Group root header: do not navigate
            break;
          }
          std::wstring target =
              (pData->path == L"This PC")
                  ? L"This PC"
                  : pData->path;
          if (!target.empty()) {
            ActionTrigger trigger = GetCurrentTrigger();
            WindowAction act = Config::Get().actionTree.ResolveAction(trigger);

            if (act == WindowAction::CurrentPane) {
              if (pThis->onPathSelected)
                pThis->onPathSelected(target);
            } else if (act == WindowAction::OtherPane) {
              if (pThis->onCustomCommand)
                pThis->onCustomCommand(CMD_NEOMDIE_OPEN_SPLIT, target);
            } else if (act == WindowAction::NewTab) {
              if (pThis->onCustomCommand)
                pThis->onCustomCommand(CMD_NEOMDIE_OPEN_NEW_TAB, target);
            } else if (act == WindowAction::BgTab) {
              if (pThis->onCustomCommand)
                pThis->onCustomCommand(CMD_NEOMDIE_OPEN_BG_TAB, target);
            } else if (act == WindowAction::Explorer) {
              ShellExecuteW(nullptr, L"explore", target.c_str(), nullptr,
                            nullptr, SW_SHOWNORMAL);
            }
          }
        }
        break;
      }
      }
    }
    break;
  }
  }

  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK DirectoryTree::TreeSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR uIdSubclass,
                                                 DWORD_PTR dwRefData) {
  DirectoryTree *pThis = (DirectoryTree *)dwRefData;

  switch (msg) {
  case WM_KEYDOWN: {
    if (wParam == VK_F5) {
      HWND hTop = GetAncestor(hwnd, GA_ROOT);
      SendMessageW(hTop, WM_COMMAND, ID_VIEW_REFRESH, 0);
      return 0;
    }
    break;
  }
  case WM_MBUTTONUP: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    TVHITTESTINFO tvhti = {0};
    tvhti.pt = pt;
    HTREEITEM hHit = TreeView_HitTest(hwnd, &tvhti);
    if (!hHit)
      hHit = tvhti.hItem;
    if (hHit) {
      TVITEMW item = {0};
      item.hItem = hHit;
      item.mask = TVIF_PARAM;
      if (TreeView_GetItem(hwnd, &item) && item.lParam) {
        TreeNodeData *pData = (TreeNodeData *)item.lParam;
        if (pData && !pData->path.empty() && pData->path != L"QuickAccess") {
          std::wstring target =
              (pData->path == L"This PC") ? L"This PC" : pData->path;
          ActionTrigger trigger = GetCurrentTrigger(true, false);
          WindowAction act = Config::Get().actionTree.ResolveAction(trigger);
          if (act == WindowAction::CurrentPane) {
            if (pThis->onPathSelected)
              pThis->onPathSelected(target);
          } else if (act == WindowAction::OtherPane) {
            if (pThis->onCustomCommand)
              pThis->onCustomCommand(CMD_NEOMDIE_OPEN_SPLIT, target);
          } else if (act == WindowAction::NewTab) {
            if (pThis->onCustomCommand)
              pThis->onCustomCommand(CMD_NEOMDIE_OPEN_NEW_TAB, target);
          } else if (act == WindowAction::BgTab) {
            if (pThis->onCustomCommand)
              pThis->onCustomCommand(CMD_NEOMDIE_OPEN_BG_TAB, target);
          } else if (act == WindowAction::Explorer) {
            ShellExecuteW(nullptr, L"explore", target.c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL);
          }
          return 0;
        }
      }
    }
    break;
  }
  case WM_RBUTTONDOWN: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    TVHITTESTINFO tvhti = {0};
    tvhti.pt = pt;
    HTREEITEM hHit = TreeView_HitTest(hwnd, &tvhti);
    if (!hHit)
      hHit = tvhti.hItem;
    if (hHit) {
      bool oldSync = pThis->m_syncing;
      pThis->m_syncing =
          true; // Prevent right-click selection from navigating active pane
      TreeView_SelectItem(hwnd, hHit);
      pThis->m_syncing = oldSync;
    }
    return 0;
  }

  case WM_RBUTTONUP: {
    if (GetCapture() == hwnd) ReleaseCapture();
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    POINT ptScreen = pt;
    ClientToScreen(hwnd, &ptScreen);
    SendMessageW(hwnd, WM_CONTEXTMENU, (WPARAM)hwnd,
                 MAKELPARAM(ptScreen.x, ptScreen.y));
    return 0;
  }

  case WM_CONTEXTMENU: {
    POINT ptScreen = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (ptScreen.x == -1 && ptScreen.y == -1) {
      HTREEITEM hSel = TreeView_GetSelection(hwnd);
      RECT rcItem = {0};
      *(HTREEITEM *)&rcItem = hSel;
      if (TreeView_GetItemRect(hwnd, hSel, &rcItem, TRUE)) {
        ptScreen.x = rcItem.left;
        ptScreen.y = rcItem.bottom;
        ClientToScreen(hwnd, &ptScreen);
      } else {
        GetCursorPos(&ptScreen);
      }
    }

    HTREEITEM hSel = TreeView_GetSelection(hwnd);
    if (hSel) {
      TVITEMW item = {0};
      item.hItem = hSel;
      item.mask = TVIF_PARAM | TVIF_STATE;
      item.stateMask = TVIS_EXPANDED;
      if (TreeView_GetItem(hwnd, &item) && item.lParam) {
        TreeNodeData *pData = (TreeNodeData *)item.lParam;
        if (pData) {
          if (pData->path == L"QuickAccess") {
            bool isZh = I18n::Get().IsChinese();
            bool isExpanded = (item.state & TVIS_EXPANDED) != 0;
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 201, isExpanded ? (isZh ? L"折叠(&C)" : L"展开(&O)") : (isZh ? L"展开(&O)" : L"Expand(&O)"));
            AppendMenuW(hMenu, MF_STRING, 202, isZh ? L"刷新快速访问(&R)" : L"Refresh Quick Access(&R)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 203, isZh ? L"恢复默认快速访问列表(&D)" : L"Restore Default Quick Access(&D)");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == 201) {
              TreeView_Expand(hwnd, hSel, TVE_TOGGLE);
            } else if (cmd == 202) {
              pThis->RefreshQuickAccess();
            } else if (cmd == 203) {
              if (pThis->onCustomCommand) {
                pThis->onCustomCommand(CMD_NEOMDIE_RESET_QUICKACCESS, L"");
              }
            }
            return 0;
          }

          if (!pData->path.empty()) {
            std::wstring targetPath =
                (pData->path == L"This PC")
                    ? L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}"
                    : pData->path;

            bool isZh = I18n::Get().IsChinese();
            bool isExpanded = (item.state & TVIS_EXPANDED) != 0;
            HTREEITEM hParent = TreeView_GetParent(hwnd, hSel);
            bool isQuickAccessChild = (hParent == pThis->m_hQuickAccess);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 101, isZh ? L"在新标签页中打开(&T)" : L"Open in New Tab(&T)");
            AppendMenuW(hMenu, MF_STRING, 102, isZh ? L"在后台新标签页中打开(&B)" : L"Open in Background Tab(&B)");
            AppendMenuW(hMenu, MF_STRING, 103, isZh ? L"在新栏中打开(&S)" : L"Open in New Pane(&S)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            if (isQuickAccessChild) {
              AppendMenuW(hMenu, MF_STRING, 114, isZh ? L"从快速访问中取消固定(&U)" : L"Unpin from Quick Access(&U)");
            } else if (pData->path != L"This PC") {
              AppendMenuW(hMenu, MF_STRING, 115, isZh ? L"固定到快速访问(&P)" : L"Pin to Quick Access(&P)");
            }
            AppendMenuW(hMenu, MF_STRING, 108, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 104, isExpanded ? (isZh ? L"折叠(&C)" : L"Collapse(&C)") : (isZh ? L"展开(&O)" : L"Expand(&O)"));
            AppendMenuW(hMenu, MF_STRING, 105, isZh ? L"刷新该节点(&R)" : L"Refresh Node(&R)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 106, isZh ? L"在资源管理器中打开(&E)" : L"Open in File Explorer(&E)");
            AppendMenuW(hMenu, MF_STRING, 107, isZh ? L"复制完整路径(&A)" : L"Copy Full Path(&A)");
            AppendMenuW(hMenu, MF_STRING, 109, isZh ? L"打开命令CMD(&D)" : L"Open CMD(&D)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 110, isZh ? L"显示系统右键菜单(&M)..." : L"Show System Context Menu(&M)...");
            AppendMenuW(hMenu, MF_STRING, 111, isZh ? L"属性(&P)" : L"Properties(&P)");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == 101) {
              if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_NEW_TAB, targetPath);
            } else if (cmd == 102) {
              if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_BG_TAB, targetPath);
            } else if (cmd == 103) {
              if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_SPLIT, targetPath);
            } else if (cmd == 104) {
              TreeView_Expand(hwnd, hSel, TVE_TOGGLE);
            } else if (cmd == 105) {
              HTREEITEM hChild = TreeView_GetChild(hwnd, hSel);
              while (hChild) {
                HTREEITEM hNext = TreeView_GetNextSibling(hwnd, hChild);
                TreeView_DeleteItem(hwnd, hChild);
                hChild = hNext;
              }
              pThis->PopulateChildren(hSel, targetPath);
              TreeView_Expand(hwnd, hSel, TVE_EXPAND);
            } else if (cmd == 106) {
              ShellUtils::OpenInExplorer(targetPath);
            } else if (cmd == 107) {
              ShellUtils::CopyToClipboard(targetPath);
            } else if (cmd == 108) {
              if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_ADD_LINKS, targetPath);
            } else if (cmd == 109) {
              ShellUtils::LaunchTerminal(targetPath);
            } else if (cmd == 110) {
              ShellUtils::ShowContextMenu(
                  hwnd, {targetPath}, ptScreen,
                  [pThis](int c, const std::wstring &tp) {
                    if (pThis->onCustomCommand) {
                      pThis->onCustomCommand(c, tp);
                    }
                  });
            } else if (cmd == 111) {
              SHELLEXECUTEINFOW sei = { sizeof(sei) };
              sei.fMask = SEE_MASK_INVOKEIDLIST;
              sei.hwnd = hwnd;
              sei.lpVerb = L"properties";
              sei.lpFile = targetPath.c_str();
              sei.nShow = SW_SHOWNORMAL;
              ShellExecuteExW(&sei);
            } else if (cmd == 114) {
              if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_UNPIN_QUICKACCESS, targetPath);
            } else if (cmd == 115) {
              if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_PIN_QUICKACCESS, targetPath);
            }
            return 0;
          }
        }
      }
    }
    return 0;
  }
  }

  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void DirectoryTree::StartDragDrop(HTREEITEM hItem) {
  if (!hItem)
    return;
  TVITEMW item = {0};
  item.hItem = hItem;
  item.mask = TVIF_PARAM;
  if (TreeView_GetItem(m_hwnd, &item) && item.lParam) {
    TreeNodeData *pData = (TreeNodeData *)item.lParam;
    if (pData && !pData->path.empty() && PathFileExistsW(pData->path.c_str())) {
      DWORD dwEffect = 0;
      HRESULT hr = ShellUtils::DoFileDragDrop(
          m_hwnd, pData->path, {pData->path}, &dwEffect);
      if (SUCCEEDED(hr) && dwEffect == DROPEFFECT_MOVE) {
        Refresh();
      }
    }
  }
}
