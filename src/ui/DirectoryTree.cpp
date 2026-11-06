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
                WS_HSCROLL | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                TVS_SHOWSELALWAYS;

  m_hwnd = CreateWindowExW(0, WC_TREEVIEWW, L"", style, 0, 0, 240, 400,
                           hwndParent, (HMENU)(INT_PTR)id, hInst, nullptr);

  TreeView_SetExtendedStyle(m_hwnd, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

  HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(false);
  if (himlSmall) {
    TreeView_SetImageList(m_hwnd, himlSmall, TVSIL_NORMAL);
  }

  Theme::Get().ApplyToWindow(m_hwnd);
  SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd),
               TRUE);
  ApplyTheme();

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
    IFileOperation *pfo = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&pfo)))) {
      pfo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR | FOF_RENAMEONCOLLISION);
      pfo->SetOwnerWindow(m_hwnd);

      IShellItem *psiDest = nullptr;
      if (SUCCEEDED(SHCreateItemFromParsingName(targetDir.c_str(), nullptr,
                                                IID_PPV_ARGS(&psiDest)))) {
        for (const auto &src : files) {
          if (isMove) {
            wchar_t sDir[MAX_PATH];
            StringCchCopyW(sDir, MAX_PATH, src.c_str());
            PathRemoveFileSpecW(sDir);
            if (_wcsicmp(sDir, targetDir.c_str()) == 0)
              continue;
          }
          IShellItem *psiSrc = nullptr;
          if (SUCCEEDED(SHCreateItemFromParsingName(src.c_str(), nullptr,
                                                    IID_PPV_ARGS(&psiSrc)))) {
            if (isMove) {
              pfo->MoveItem(psiSrc, psiDest, nullptr, nullptr);
            } else {
              pfo->CopyItem(psiSrc, psiDest, nullptr, nullptr);
            }
            psiSrc->Release();
          }
        }
        pfo->PerformOperations();
        psiDest->Release();
      }
      pfo->Release();
    }
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, targetDir.c_str(), nullptr);
    Refresh();
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
  }

  if (normPath.length() >= 2 && normPath[1] == L':') {
    wchar_t drivePrefix[4] = {(wchar_t)towupper(normPath[0]), L':', L'\\',
                              L'\0'};
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

void DirectoryTree::AddSpecialFolder(HTREEITEM hParent, const wchar_t *name,
                                     int csidl) {
  wchar_t path[MAX_PATH];
  if (SHGetFolderPathW(nullptr, csidl, nullptr, 0, path) == S_OK) {
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                     TVIF_CHILDREN | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)name;
    tvis.item.iImage =
        ShellUtils::GetItemIconIndex(path, FILE_ATTRIBUTE_DIRECTORY, true);
    tvis.item.iSelectedImage = tvis.item.iImage;
    tvis.item.cChildren = 1;

    TreeNodeData *pData = new TreeNodeData{path, false, false};
    m_nodeDataList.push_back(pData);
    tvis.item.lParam = (LPARAM)pData;

    HTREEITEM hItem = TreeView_InsertItem(m_hwnd, &tvis);

    // Dummy child item for lazy loading
    TVINSERTSTRUCTW dummy = {0};
    dummy.hParent = hItem;
    dummy.item.mask = TVIF_TEXT;
    dummy.item.pszText = (LPWSTR)L"";
    TreeView_InsertItem(m_hwnd, &dummy);
  }
}

void DirectoryTree::PopulateQuickAccess(HTREEITEM hParent) {
  AddSpecialFolder(hParent, I18n::Get().T(StringId::Desktop),
                   CSIDL_DESKTOPDIRECTORY);

  // Downloads
  wchar_t userProfile[MAX_PATH];
  if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
    std::wstring dl = std::wstring(userProfile) + L"\\Downloads";
    if (PathFileExistsW(dl.c_str())) {
      TVINSERTSTRUCTW tvis = {0};
      tvis.hParent = hParent;
      tvis.hInsertAfter = TVI_LAST;
      tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                       TVIF_CHILDREN | TVIF_PARAM;
      tvis.item.pszText = (LPWSTR)I18n::Get().T(StringId::Downloads);
      tvis.item.iImage =
          ShellUtils::GetItemIconIndex(dl, FILE_ATTRIBUTE_DIRECTORY, true);
      tvis.item.iSelectedImage = tvis.item.iImage;
      tvis.item.cChildren = 1;

      TreeNodeData *pData = new TreeNodeData{dl, false, false};
      m_nodeDataList.push_back(pData);
      tvis.item.lParam = (LPARAM)pData;
      HTREEITEM hItem = TreeView_InsertItem(m_hwnd, &tvis);

      TVINSERTSTRUCTW dummy = {0};
      dummy.hParent = hItem;
      dummy.item.mask = TVIF_TEXT;
      dummy.item.pszText = (LPWSTR)L"";
      TreeView_InsertItem(m_hwnd, &dummy);
    }
  }

  AddSpecialFolder(hParent, I18n::Get().T(StringId::Documents),
                   CSIDL_MYDOCUMENTS);
  AddSpecialFolder(hParent, I18n::Get().T(StringId::Pictures),
                   CSIDL_MYPICTURES);
  AddSpecialFolder(hParent, I18n::Get().T(StringId::Music), CSIDL_MYMUSIC);
  AddSpecialFolder(hParent, I18n::Get().T(StringId::Videos), CSIDL_MYVIDEO);
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
  }

  std::vector<std::pair<std::wstring, std::wstring>> subdirs;
  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
      continue;
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
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
    tvis.item.cChildren = 1;

    TreeNodeData *pData = new TreeNodeData{item.second, false, false};
    m_nodeDataList.push_back(pData);
    tvis.item.lParam = (LPARAM)pData;

    TreeView_InsertItem(m_hwnd, &tvis);
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
  COLORREF bg = Theme::Get().ClrBackground();
  COLORREF text = Theme::Get().ClrTextPrimary();
  TreeView_SetBkColor(m_hwnd, bg);
  TreeView_SetTextColor(m_hwnd, text);
  SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                   SWP_NOACTIVATE);
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
        if (pThis->m_syncing || (GetKeyState(VK_RBUTTON) < 0))
          break;
        NMTREEVIEWW *pnmtv = (NMTREEVIEWW *)lParam;
        TreeNodeData *pData = (TreeNodeData *)pnmtv->itemNew.lParam;
        if (pData) {
          std::wstring target =
              (pData->path == L"This PC" || pData->path == L"QuickAccess")
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
      item.mask = TVIF_PARAM;
      if (TreeView_GetItem(hwnd, &item) && item.lParam) {
        TreeNodeData *pData = (TreeNodeData *)item.lParam;
        if (pData && !pData->path.empty() && pData->path != L"QuickAccess") {
          std::wstring targetPath =
              (pData->path == L"This PC")
                  ? L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}"
                  : pData->path;
          ShellUtils::ShowContextMenu(
              hwnd, {targetPath}, ptScreen,
              [pThis](int cmd, const std::wstring &targetPath) {
                if (pThis->onCustomCommand) {
                  pThis->onCustomCommand(cmd, targetPath);
                }
              });
          return 0;
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
