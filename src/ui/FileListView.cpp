#include "FileListView.h"
#include "Theme.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"

const UINT FileListView::WM_NOTIFY_DIR_CHANGED;
const UINT FileListView::WM_NOTIFY_FOLDER_SIZE;

FileListView::FileListView() {
}

FileListView::~FileListView() {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }
    if (m_pDropTarget && m_hwnd) {
        RevokeDragDrop(m_hwnd);
        m_pDropTarget->Release();
        m_pDropTarget = nullptr;
    }
    m_calcSize.CancelAll();
    m_watcher.Stop();
    m_gesture.Detach();
}

HWND FileListView::Create(HWND hwndParent, int id) {
    m_hwndParent = hwndParent;
    m_controlId = id;
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                  LVS_REPORT | LVS_OWNERDATA | LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS | LVS_EDITLABELS;

    m_hwnd = CreateWindowExW(
        0, WC_LISTVIEWW, L"", style,
        0, 0, 400, 300,
        hwndParent, (HMENU)(INT_PTR)id, hInst, nullptr
    );

    ListView_SetExtendedListViewStyle(m_hwnd,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_SUBITEMIMAGES);

    HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(false);
    HIMAGELIST himlLarge = ShellUtils::GetSystemImageList(true);
    ListView_SetImageList(m_hwnd, himlSmall, LVSIL_SMALL);
    ListView_SetImageList(m_hwnd, himlLarge, LVSIL_NORMAL);

    Theme::Get().ApplyToWindow(m_hwnd);
    SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    InitColumns();
    ApplyTheme();

    SetWindowSubclass(hwndParent, FileListView::ParentSubclassProc, (UINT_PTR)this, (DWORD_PTR)this);
    SetWindowSubclass(m_hwnd, FileListView::ListSubclassProc, (UINT_PTR)this, (DWORD_PTR)this);

    // Register OLE Drag & Drop target
    m_pDropTarget = new DropTarget(m_hwnd, [this](POINT ptScreen, bool& outIsSubFolder) -> std::wstring {
        POINT ptClient = ptScreen;
        ScreenToClient(m_hwnd, &ptClient);
        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = ptClient;
        int hitIndex = ListView_HitTest(m_hwnd, &lvhti);
        if (hitIndex >= 0 && hitIndex < (int)m_filteredIndices.size()) {
            int idx = m_filteredIndices[hitIndex];
            if (m_allItems[idx].isFolder && !m_allItems[idx].isDrive) {
                outIsSubFolder = true;
                return m_allItems[idx].fullPath;
            }
        }
        outIsSubFolder = false;
        return m_currentPath;
    });

    m_pDropTarget->onDragHover = [this](POINTL pt) {
        POINT ptClient = { pt.x, pt.y };
        ScreenToClient(m_hwnd, &ptClient);
        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = ptClient;
        int hitIndex = ListView_HitTest(m_hwnd, &lvhti);
        int newHighlight = -1;
        if (hitIndex >= 0 && hitIndex < (int)m_filteredIndices.size()) {
            int idx = m_filteredIndices[hitIndex];
            if (m_allItems[idx].isFolder && !m_allItems[idx].isDrive) {
                newHighlight = hitIndex;
            }
        }
        if (m_dropHighlightItem != newHighlight) {
            if (m_dropHighlightItem != -1) {
                ListView_SetItemState(m_hwnd, m_dropHighlightItem, 0, LVIS_DROPHILITED);
            }
            m_dropHighlightItem = newHighlight;
            if (m_dropHighlightItem != -1) {
                ListView_SetItemState(m_hwnd, m_dropHighlightItem, LVIS_DROPHILITED, LVIS_DROPHILITED);
            }
        }
    };

    m_pDropTarget->onDragLeave = [this]() {
        if (m_dropHighlightItem != -1) {
            ListView_SetItemState(m_hwnd, m_dropHighlightItem, 0, LVIS_DROPHILITED);
            m_dropHighlightItem = -1;
        }
    };

    m_pDropTarget->onFilesDropped = [this](const std::wstring& targetDir, const std::vector<std::wstring>& files, bool isMove) {
        IFileOperation* pfo = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo)))) {
            pfo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR);
            pfo->SetOwnerWindow(m_hwnd);

            IShellItem* psiDest = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(targetDir.c_str(), nullptr, IID_PPV_ARGS(&psiDest)))) {
                for (const auto& src : files) {
                    IShellItem* psiSrc = nullptr;
                    if (SUCCEEDED(SHCreateItemFromParsingName(src.c_str(), nullptr, IID_PPV_ARGS(&psiSrc)))) {
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
        Refresh();
    };

    RegisterDragDrop(m_hwnd, m_pDropTarget);
    m_gesture.Attach(m_hwnd);

    return m_hwnd;
}

void FileListView::InitColumns() {
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;

    lvc.iSubItem = 0;
    lvc.cx = Theme::Get().Scale(320, m_hwnd);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColName);
    ListView_InsertColumn(m_hwnd, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = Theme::Get().Scale(160, m_hwnd);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColDateModified);
    ListView_InsertColumn(m_hwnd, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.cx = Theme::Get().Scale(140, m_hwnd);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColType);
    ListView_InsertColumn(m_hwnd, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.cx = Theme::Get().Scale(180, m_hwnd);
    lvc.fmt = LVCFMT_LEFT;
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColSize);
    ListView_InsertColumn(m_hwnd, 3, &lvc);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowSubclass(hHeader, FileListView::HeaderSubclassProc, 1, (DWORD_PTR)this);
    }

    UpdateHeaderSortArrow();
}

void FileListView::UpdateHeaderSortArrow() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (!hHeader) return;
    int colCount = Header_GetItemCount(hHeader);
    for (int i = 0; i < colCount; ++i) {
        HDITEMW hdi = { sizeof(hdi) };
        hdi.mask = HDI_FORMAT;
        Header_GetItem(hHeader, i, &hdi);
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == (int)m_sortCol) {
            hdi.fmt |= (m_sortAsc ? HDF_SORTUP : HDF_SORTDOWN);
        }
        Header_SetItem(hHeader, i, &hdi);
    }
}

void FileListView::ShowColumnHeaderMenu(POINT ptScreen) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED | MF_GRAYED, 1, isZh ? L"名称" : L"Name");
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED, 2, isZh ? L"修改日期" : L"Date modified");
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED, 3, isZh ? L"类型" : L"Type");
    AppendMenuW(hMenu, MF_STRING | MF_CHECKED, 4, isZh ? L"大小" : L"Size");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 10, isZh ? L"自动调整所有列宽(&A)" : L"Size All Columns to Fit(&A)");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 10) {
        HWND hHeader = ListView_GetHeader(m_hwnd);
        int cnt = Header_GetItemCount(hHeader);
        for (int i = 0; i < cnt; ++i) {
            ListView_SetColumnWidth(m_hwnd, i, LVSCW_AUTOSIZE_USEHEADER);
        }
    }
}

LRESULT CALLBACK FileListView::HeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;
    if (msg == WM_RBUTTONUP || msg == WM_CONTEXTMENU) {
        if (pThis) {
            POINT pt;
            GetCursorPos(&pt);
            pThis->ShowColumnHeaderMenu(pt);
            return 0;
        }
    }
    if (msg == WM_ERASEBKGND && Theme::Get().IsDarkMode()) {
        return 1;
    }
    if (msg == WM_PAINT && Theme::Get().IsDarkMode()) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
        HGDIOBJ oldBm = SelectObject(hdcMem, hbmMem);

        FillRect(hdcMem, &rcClient, Theme::Get().BrushSurface());
        
        int count = Header_GetItemCount(hwnd);
        HFONT hFont = Theme::Get().GetUIFont(hwnd);
        HGDIOBJ oldFont = SelectObject(hdcMem, hFont);
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
        HPEN hPenBorder = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdcMem, hPenBorder);
        
        for (int i = 0; i < count; ++i) {
            RECT rcItem;
            Header_GetItemRect(hwnd, i, &rcItem);
            
            wchar_t text[256] = { 0 };
            HDITEMW hdi = { sizeof(hdi) };
            hdi.mask = HDI_TEXT | HDI_FORMAT;
            hdi.pszText = text;
            hdi.cchTextMax = 256;
            Header_GetItem(hwnd, i, &hdi);
            
            RECT rcText = rcItem;
            rcText.left += Theme::Get().Scale(8, hwnd);
            rcText.right -= Theme::Get().Scale(8, hwnd);
            
            if (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) {
                rcText.right -= Theme::Get().Scale(16, hwnd);
                int arrowX = rcItem.right - Theme::Get().Scale(14, hwnd);
                int arrowY = rcItem.top + (rcItem.bottom - rcItem.top) / 2;
                POINT pts[3];
                int aSize = Theme::Get().Scale(4, hwnd);
                if (hdi.fmt & HDF_SORTUP) {
                    pts[0] = { arrowX - aSize, arrowY + aSize / 2 };
                    pts[1] = { arrowX + aSize, arrowY + aSize / 2 };
                    pts[2] = { arrowX, arrowY - aSize / 2 };
                } else {
                    pts[0] = { arrowX - aSize, arrowY - aSize / 2 };
                    pts[1] = { arrowX + aSize, arrowY - aSize / 2 };
                    pts[2] = { arrowX, arrowY + aSize / 2 };
                }
                HBRUSH hArrowBr = CreateSolidBrush(Theme::Get().ClrTextSecondary());
                HGDIOBJ oldBr = SelectObject(hdcMem, hArrowBr);
                HGDIOBJ oldP = SelectObject(hdcMem, GetStockObject(NULL_PEN));
                Polygon(hdcMem, pts, 3);
                SelectObject(hdcMem, oldP);
                SelectObject(hdcMem, oldBr);
                DeleteObject(hArrowBr);
            }
            
            UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
            if (hdi.fmt & HDF_RIGHT) fmt = DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
            else if (hdi.fmt & HDF_CENTER) fmt = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
            DrawTextW(hdcMem, text, -1, &rcText, fmt);
            
            // Subtle column divider line
            MoveToEx(hdcMem, rcItem.right - 1, rcItem.top + 3, nullptr);
            LineTo(hdcMem, rcItem.right - 1, rcItem.bottom - 3);
        }
        
        // Bottom border line of header
        MoveToEx(hdcMem, 0, rcClient.bottom - 1, nullptr);
        LineTo(hdcMem, rcClient.right, rcClient.bottom - 1);
        
        BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);
        
        SelectObject(hdcMem, oldPen);
        SelectObject(hdcMem, oldFont);
        SelectObject(hdcMem, oldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void FileListView::ApplyTheme() {
    if (!m_hwnd) return;
    bool isDark = Theme::Get().IsDarkMode();
    Theme::Get().ApplyToWindow(m_hwnd);
    COLORREF bg = Theme::Get().ClrBackground();
    COLORREF text = Theme::Get().ClrTextPrimary();
    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, Config::Get().zebraStriping ? CLR_NONE : bg);
    ListView_SetTextColor(m_hwnd, text);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowSubclass(hHeader, FileListView::HeaderSubclassProc, 1, (DWORD_PTR)this);
        SetWindowTheme(hHeader, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
        InvalidateRect(hHeader, nullptr, TRUE);
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::UpdateColumnsLanguage() {
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT;
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColName);
    ListView_SetColumn(m_hwnd, 0, &lvc);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColDateModified);
    ListView_SetColumn(m_hwnd, 1, &lvc);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColType);
    ListView_SetColumn(m_hwnd, 2, &lvc);
    lvc.pszText = (LPWSTR)I18n::Get().T(StringId::ColSize);
    ListView_SetColumn(m_hwnd, 3, &lvc);
}

bool FileListView::NavigateTo(const std::wstring& path, bool recordHistory) {
    std::wstring normalized = Util::NormalizePath(path);

    std::vector<NeoShellItem> items;
    if (!ShellUtils::EnumerateDirectory(normalized, m_showHidden, items)) {
        return false;
    }

    m_calcSize.CancelAll();
    m_currentPath = normalized;
    m_allItems = std::move(items);

    if (recordHistory) {
        if (m_historyIndex >= 0 && m_historyIndex + 1 < (int)m_history.size()) {
            m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
        }
        m_history.push_back(normalized);
        m_historyIndex = (int)m_history.size() - 1;
    }

    SortItems();
    ApplyFilter();

    m_watcher.Watch(normalized, m_hwndParent, WM_NOTIFY_DIR_CHANGED);

    if (m_calcFolderSize && normalized != L"此电脑" && normalized != L"This PC") {
        for (const auto& item : m_allItems) {
            if (item.isFolder && !item.isDrive) {
                m_calcSize.RequestFolderSize(item.fullPath, m_hwndParent, WM_NOTIFY_FOLDER_SIZE);
            }
        }
    }

    if (onPathChanged) {
        onPathChanged(m_currentPath);
    }
    return true;
}

void FileListView::Refresh() {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }
    std::vector<NeoShellItem> items;
    if (ShellUtils::EnumerateDirectory(m_currentPath, m_showHidden, items)) {
        m_allItems = std::move(items);
        SortItems();
        ApplyFilter();
    }
}

void FileListView::GoBack() {
    if (CanGoBack()) {
        m_historyIndex--;
        NavigateTo(m_history[m_historyIndex], false);
    }
}

void FileListView::GoForward() {
    if (CanGoForward()) {
        m_historyIndex++;
        NavigateTo(m_history[m_historyIndex], false);
    }
}

void FileListView::GoUp() {
    std::wstring parent = ShellUtils::GetParentPath(m_currentPath);
    NavigateTo(parent, true);
}

void FileListView::SetFilter(const std::wstring& query, bool subfolders, bool matchCase) {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }

    m_filterQuery = query;
    if (subfolders && !query.empty() && m_currentPath != L"This PC" && m_currentPath != L"此电脑") {
        m_searchCancelled = false;
        m_allItems.clear();
        m_filteredIndices.clear();
        ListView_SetItemCountEx(m_hwnd, 0, 0);
        InvalidateRect(m_hwnd, nullptr, TRUE);
        SearchRecursive(m_currentPath, query, matchCase);
    } else {
        if (m_allItems.empty() || m_allItems[0].fullPath.find(m_currentPath) == std::wstring::npos) {
            Refresh();
        } else {
            ApplyFilter();
        }
    }
}

void FileListView::SearchRecursive(const std::wstring& rootDir, const std::wstring& query, bool matchCase) {
    HWND hwndTarget = m_hwndParent;
    m_searchThread = std::thread([this, rootDir, query, matchCase, hwndTarget]() {
        std::vector<std::wstring> dirsToSearch = { rootDir };
        std::wstring lowerQuery = query;
        if (!matchCase) {
            std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::towlower);
        }

        std::vector<NeoShellItem>* pBatch = new std::vector<NeoShellItem>();
        int totalFound = 0;

        while (!dirsToSearch.empty() && !m_searchCancelled && totalFound < 2000) {
            std::wstring curDir = dirsToSearch.back();
            dirsToSearch.pop_back();

            std::wstring searchPattern = Util::CombinePath(curDir, L"*");
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileExW(searchPattern.c_str(), FindExInfoBasic, &fd,
                                            FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (m_searchCancelled) break;
                    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                    std::wstring name = fd.cFileName;
                    std::wstring full = Util::CombinePath(curDir, name);
                    bool isFolder = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                    std::wstring checkName = name;
                    if (!matchCase) {
                        std::transform(checkName.begin(), checkName.end(), checkName.begin(), ::towlower);
                    }

                    if (checkName.find(lowerQuery) != std::wstring::npos) {
                        NeoShellItem item;
                        item.name = name;
                        item.fullPath = full;
                        item.attributes = fd.dwFileAttributes;
                        item.isFolder = isFolder;
                        item.modifiedTime = fd.ftLastWriteTime;
                        ULARGE_INTEGER uli;
                        uli.LowPart = fd.nFileSizeLow;
                        uli.HighPart = fd.nFileSizeHigh;
                        item.size = uli.QuadPart;
                        item.isSizeCalculated = true;
                        item.iconIndex = ShellUtils::GetItemIconIndex(full, fd.dwFileAttributes, isFolder);
                        item.typeName = isFolder ? I18n::Get().T(StringId::Folder) : ShellUtils::GetItemTypeName(full, fd.dwFileAttributes);
                        pBatch->push_back(item);
                        totalFound++;

                        if (pBatch->size() >= 30) {
                            PostMessageW(hwndTarget, WM_SEARCH_BATCH, (WPARAM)this, (LPARAM)pBatch);
                            pBatch = new std::vector<NeoShellItem>();
                            Sleep(10);
                        }
                    }

                    if (isFolder && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                        dirsToSearch.push_back(full);
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }

        if (pBatch->empty()) {
            delete pBatch;
        } else {
            PostMessageW(hwndTarget, WM_SEARCH_BATCH, (WPARAM)this, (LPARAM)pBatch);
        }
        PostMessageW(hwndTarget, WM_SEARCH_COMPLETE, (WPARAM)this, (LPARAM)totalFound);
    });
}

void FileListView::ApplyFilter() {
    m_filteredIndices.clear();
    uint64_t totalSize = 0;

    for (size_t i = 0; i < m_allItems.size(); ++i) {
        if (m_allItems[i].MatchesFilter(m_filterQuery)) {
            m_filteredIndices.push_back((int)i);
            totalSize += m_allItems[i].size;
        }
    }

    ListView_SetItemCountEx(m_hwnd, (int)m_filteredIndices.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    InvalidateRect(m_hwnd, nullptr, TRUE);

    if (onStatusUpdated) {
        onStatusUpdated((int)m_filteredIndices.size(), totalSize);
    }
}

void FileListView::SortItems() {
    std::sort(m_allItems.begin(), m_allItems.end(),
        [this](const NeoShellItem& a, const NeoShellItem& b) {
            return CompareShellItems(a, b, m_sortCol, m_sortAsc);
        });
}

void FileListView::SetViewMode(int mode) {
    m_viewMode = mode;
    DWORD style = GetWindowLongW(m_hwnd, GWL_STYLE) & ~LVS_TYPEMASK;
    switch (mode) {
    case 0: style |= LVS_REPORT; break;
    case 1: style |= LVS_ICON; break;
    case 2: style |= LVS_SMALLICON; break;
    case 3: style |= LVS_LIST; break;
    case 4: style |= LVS_ICON; break; // Tiles
    default: style |= LVS_REPORT; break;
    }
    SetWindowLongW(m_hwnd, GWL_STYLE, style);
    if (mode == 4) {
        SendMessageW(m_hwnd, LVM_SETVIEW, LV_VIEW_TILE, 0);
    }
    UpdateHeaderSortArrow();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::SetShowHidden(bool show) {
    if (m_showHidden != show) {
        m_showHidden = show;
        Refresh();
    }
}

void FileListView::SetCalcFolderSize(bool calc) {
    if (m_calcFolderSize != calc) {
        m_calcFolderSize = calc;
        Refresh();
    }
}

std::vector<std::wstring> FileListView::GetSelectedPaths() const {
    std::vector<std::wstring> paths;
    int index = -1;
    while ((index = ListView_GetNextItem(m_hwnd, index, LVNI_SELECTED)) != -1) {
        if (index >= 0 && index < (int)m_filteredIndices.size()) {
            int itemIdx = m_filteredIndices[index];
            paths.push_back(m_allItems[itemIdx].fullPath);
        }
    }
    return paths;
}

void FileListView::SelectAll() {
    ListView_SetItemState(m_hwnd, -1, LVIS_SELECTED, LVIS_SELECTED);
}

void FileListView::InvertSelection() {
    int count = (int)m_filteredIndices.size();
    for (int i = 0; i < count; ++i) {
        UINT state = ListView_GetItemState(m_hwnd, i, LVIS_SELECTED);
        ListView_SetItemState(m_hwnd, i, (state & LVIS_SELECTED) ? 0 : LVIS_SELECTED, LVIS_SELECTED);
    }
}

void FileListView::CopySelected(bool isCut) {
    auto sel = GetSelectedPaths();
    if (!sel.empty()) {
        ShellUtils::CopyFilesToClipboard(m_hwnd, sel, isCut);
    }
}

void FileListView::PasteToCurrent() {
    if (m_currentPath != L"此电脑" && m_currentPath != L"This PC") {
        if (ShellUtils::PasteFilesFromClipboard(m_hwnd, m_currentPath)) {
            Refresh();
        }
    }
}

void FileListView::PasteShortcut() {
    if (m_currentPath != L"此电脑" && m_currentPath != L"This PC") {
        if (ShellUtils::PasteShortcut(m_hwnd, m_currentPath)) {
            Refresh();
        }
    }
}

void FileListView::SetSort(SortColumn col, bool ascending) {
    m_sortCol = col;
    m_sortAsc = ascending;
    SortItems();
    ApplyFilter();
    UpdateHeaderSortArrow();
}

void FileListView::DeleteSelected(bool permanent) {
    auto sel = GetSelectedPaths();
    if (!sel.empty()) {
        if (ShellUtils::DeleteFiles(m_hwnd, sel, permanent)) {
            Refresh();
        }
    }
}

void FileListView::StartRename() {
    int selected = ListView_GetNextItem(m_hwnd, -1, LVNI_SELECTED);
    if (selected != -1) {
        SetFocus(m_hwnd);
        ListView_EditLabel(m_hwnd, selected);
    }
}

void FileListView::ScrollToTop() {
    if (m_hwnd) {
        SetFocus(m_hwnd);
        SendMessageW(m_hwnd, WM_VSCROLL, SB_TOP, 0);
        if (!m_filteredIndices.empty()) {
            ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(m_hwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwnd, 0, FALSE);
        }
    }
}

void FileListView::ScrollToBottom() {
    if (m_hwnd) {
        SetFocus(m_hwnd);
        SendMessageW(m_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
        if (!m_filteredIndices.empty()) {
            int last = (int)m_filteredIndices.size() - 1;
            ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(m_hwnd, last, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwnd, last, FALSE);
        }
    }
}

void FileListView::ScrollPageUp() {
    if (m_hwnd) {
        SetFocus(m_hwnd);
        SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEUP, 0);
    }
}

void FileListView::ScrollPageDown() {
    if (m_hwnd) {
        SetFocus(m_hwnd);
        SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEDOWN, 0);
    }
}

void FileListView::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void FileListView::OnItemDoubleClicked(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= (int)m_filteredIndices.size()) return;
    int idx = m_filteredIndices[itemIndex];
    const auto& item = m_allItems[idx];

    if (item.isFolder) {
        NavigateTo(item.fullPath, true);
    } else {
        ShellUtils::OpenItem(m_hwnd, item.fullPath);
    }
}

void FileListView::OnItemRightClicked(POINT ptScreen) {
    auto handleNeoCmd = [this](int cmd, const std::wstring& targetPath) {
        switch (cmd) {
        case CMD_NEOMDIE_COPY_PATH:
            ShellUtils::CopyToClipboard(targetPath);
            break;
        case CMD_NEOMDIE_TERMINAL:
            ShellUtils::LaunchTerminal(targetPath);
            break;
        case CMD_NEOMDIE_POWERSHELL:
            ShellUtils::LaunchPowerShell(targetPath);
            break;
        case CMD_NEOMDIE_CMD:
            ShellUtils::LaunchCmd(targetPath);
            break;

        case CMD_BG_VIEW_DETAILS: SetViewMode(0); break;
        case CMD_BG_VIEW_LARGE:   SetViewMode(1); break;
        case CMD_BG_VIEW_SMALL:   SetViewMode(2); break;
        case CMD_BG_VIEW_LIST:    SetViewMode(3); break;
        case CMD_BG_VIEW_TILES:   SetViewMode(4); break;
        case CMD_BG_VIEW_XLARGE:  SetViewMode(1); break;
        case CMD_BG_VIEW_MEDIUM:  SetViewMode(0); break;

        case CMD_BG_SORT_NAME:
            m_sortCol = SortColumn::Name;
            SortItems(); ApplyFilter(); UpdateHeaderSortArrow();
            break;
        case CMD_BG_SORT_DATE:
            m_sortCol = SortColumn::DateModified;
            SortItems(); ApplyFilter(); UpdateHeaderSortArrow();
            break;
        case CMD_BG_SORT_TYPE:
            m_sortCol = SortColumn::Type;
            SortItems(); ApplyFilter(); UpdateHeaderSortArrow();
            break;
        case CMD_BG_SORT_SIZE:
            m_sortCol = SortColumn::Size;
            SortItems(); ApplyFilter(); UpdateHeaderSortArrow();
            break;
        case CMD_BG_SORT_ASCENDING:
            m_sortAsc = true;
            SortItems(); ApplyFilter(); UpdateHeaderSortArrow();
            break;
        case CMD_BG_SORT_DESCENDING:
            m_sortAsc = false;
            SortItems(); ApplyFilter(); UpdateHeaderSortArrow();
            break;

        case CMD_BG_REFRESH: Refresh(); break;
        case CMD_BG_PASTE: PasteToCurrent(); break;
        case CMD_BG_PASTE_SHORTCUT: PasteShortcut(); break;
        case CMD_BG_PROPERTIES: ShellUtils::ShowItemProperties(m_hwnd, targetPath); break;
        case CMD_BG_NEW_FOLDER: {
            std::wstring newDir = Util::CombinePath(m_currentPath, I18n::Get().T(StringId::NewFolder));
            int idx = 2;
            while (PathFileExistsW(newDir.c_str())) {
                newDir = Util::CombinePath(m_currentPath, std::wstring(I18n::Get().T(StringId::NewFolder)) + L" (" + std::to_wstring(idx++) + L")");
            }
            CreateDirectoryW(newDir.c_str(), nullptr);
            Refresh();
            break;
        }
        case CMD_BG_NEW_FILE: {
            std::wstring newFile = Util::CombinePath(m_currentPath, I18n::Get().T(StringId::NewTextFile));
            int idx = 2;
            while (PathFileExistsW(newFile.c_str())) {
                newFile = Util::CombinePath(m_currentPath, L"New (" + std::to_wstring(idx++) + L").txt");
            }
            HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) CloseHandle(hF);
            Refresh();
            break;
        }
        default:
            if (onCustomCommand) {
                onCustomCommand(cmd, targetPath);
            }
            break;
        }
    };

    std::vector<std::wstring> paths = GetSelectedPaths();
    if (!paths.empty()) {
        ShellUtils::ShowContextMenu(m_hwnd, paths, ptScreen, handleNeoCmd);
    } else {
        ShellUtils::ShowBackgroundContextMenu(m_hwnd, m_currentPath, ptScreen,
                                              m_viewMode, (int)m_sortCol, m_sortAsc, handleNeoCmd);
    }
}

void FileListView::OnFolderSizeReady(FolderSizeResult* pRes) {
    if (!pRes) return;
    for (size_t i = 0; i < m_allItems.size(); ++i) {
        if (m_allItems[i].fullPath == pRes->folderPath) {
            m_allItems[i].size = pRes->totalSize;
            m_allItems[i].isSizeCalculated = true;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            break;
        }
    }
    delete pRes;
}

LRESULT CALLBACK FileListView::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK FileListView::ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {
    case WM_RBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Select item under cursor if not selected
        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = { x, y };
        int hit = ListView_HitTest(hwnd, &lvhti);
        if (hit >= 0) {
            UINT state = ListView_GetItemState(hwnd, hit, LVIS_SELECTED);
            if (!(state & LVIS_SELECTED)) {
                ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                ListView_SetItemState(hwnd, hit, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
        } else {
            ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
        }

        pThis->m_gesture.OnRButtonDown(hwnd, x, y);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        bool rDown = (wParam & MK_RBUTTON) != 0;
        if (pThis->m_gesture.OnMouseMove(hwnd, x, y, rDown)) {
            return 0;
        }
        break;
    }
    case WM_RBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        GestureAction action = GestureAction::None;
        if (pThis->m_gesture.OnRButtonUp(hwnd, x, y, action)) {
            if (action != GestureAction::None) {
                if (pThis->onGestureAction) {
                    pThis->onGestureAction(action);
                }
            }
            return 0; // Handled gesture movement, suppress context menu
        }

        POINT ptScreen = { x, y };
        ClientToScreen(hwnd, &ptScreen);
        pThis->OnItemRightClicked(ptScreen);
        return 0;
    }
    case WM_XBUTTONUP: {
        WORD btn = GET_XBUTTON_WPARAM(wParam);
        if (btn == XBUTTON1) {
            pThis->GoBack();
            return TRUE;
        } else if (btn == XBUTTON2) {
            pThis->GoForward();
            return TRUE;
        }
        break;
    }
    case WM_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) < 0);
        bool shift = (GetKeyState(VK_SHIFT) < 0);

        if (ctrl && wParam == 'C') {
            pThis->CopySelected(false);
            return 0;
        } else if (ctrl && wParam == 'X') {
            pThis->CopySelected(true);
            return 0;
        } else if (ctrl && wParam == 'V') {
            pThis->PasteToCurrent();
            return 0;
        } else if (wParam == VK_DELETE) {
            pThis->DeleteSelected(shift);
            return 0;
        } else if (wParam == VK_F2) {
            pThis->StartRename();
            return 0;
        } else if (wParam == VK_RETURN) {
            if (GetKeyState(VK_MENU) < 0) { // Alt + Enter
                auto sel = pThis->GetSelectedPaths();
                if (!sel.empty()) {
                    ShellUtils::ShowItemProperties(hwnd, sel[0]);
                }
            } else {
                int selected = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
                if (selected != -1) {
                    pThis->OnItemDoubleClicked(selected);
                }
            }
            return 0;
        } else if (wParam == VK_BACK) {
            pThis->GoUp();
            return 0;
        }
        break;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK FileListView::ParentSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->hwndFrom == pThis->m_hwnd) {
            switch (pnm->code) {
            case LVN_GETDISPINFOW: {
                NMLVDISPINFOW* pDisp = (NMLVDISPINFOW*)lParam;
                int row = pDisp->item.iItem;
                if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[row];
                    const auto& item = pThis->m_allItems[idx];

                    if (pDisp->item.mask & LVIF_TEXT) {
                        static wchar_t s_buf[MAX_PATH];
                        s_buf[0] = L'\0';
                        switch (pDisp->item.iSubItem) {
                        case 0: StringCchCopyW(s_buf, MAX_PATH, item.name.c_str()); break;
                        case 1: StringCchCopyW(s_buf, MAX_PATH, Util::FormatFileTime(item.modifiedTime).c_str()); break;
                        case 2: StringCchCopyW(s_buf, MAX_PATH, item.typeName.c_str()); break;
                        case 3:
                            if (item.isDrive) {
                                uint64_t freeBytes = 0, totalBytes = 0;
                                if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                    StringCchPrintfW(s_buf, MAX_PATH, L"%s 可用 / 共 %s",
                                                     Util::FormatBytes(freeBytes).c_str(),
                                                     Util::FormatBytes(totalBytes).c_str());
                                }
                            } else if (item.isFolder && !item.isSizeCalculated) {
                                s_buf[0] = L'\0';
                            } else {
                                StringCchCopyW(s_buf, MAX_PATH, Util::FormatBytes(item.size).c_str());
                            }
                            break;
                        }
                        pDisp->item.pszText = s_buf;
                    }

                    if (pDisp->item.mask & LVIF_IMAGE) {
                        pDisp->item.iImage = (pDisp->item.iSubItem == 0) ? item.iconIndex : -1;
                    }
                }
                return 0;
            }

            case LVN_BEGINLABELEDITW: {
                HWND hEdit = ListView_GetEditControl(pThis->m_hwnd);
                if (hEdit) {
                    wchar_t text[MAX_PATH];
                    GetWindowTextW(hEdit, text, MAX_PATH);
                    const wchar_t* pExt = PathFindExtensionW(text);
                    int extPos = pExt ? (int)(pExt - text) : (int)wcslen(text);
                    // Select filename only, without extension
                    SendMessageW(hEdit, EM_SETSEL, 0, extPos);
                }
                return FALSE; // Allow edit
            }

            case LVN_ENDLABELEDITW: {
                NMLVDISPINFOW* pDisp = (NMLVDISPINFOW*)lParam;
                if (pDisp->item.pszText && wcslen(pDisp->item.pszText) > 0) {
                    int row = pDisp->item.iItem;
                    if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                        int idx = pThis->m_filteredIndices[row];
                        const auto& item = pThis->m_allItems[idx];
                        if (ShellUtils::RenameItem(pThis->m_hwnd, item.fullPath, pDisp->item.pszText)) {
                            pThis->Refresh();
                        }
                    }
                }
                return FALSE;
            }

            case NM_CUSTOMDRAW: {
                NMLVCUSTOMDRAW* pcd = (NMLVCUSTOMDRAW*)lParam;
                switch (pcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;
                    if (Config::Get().zebraStriping && !isSelected && (row % 2 == 1) && row >= 0) {
                        RECT rcItem;
                        ListView_GetItemRect(pThis->m_hwnd, row, &rcItem, LVIR_BOUNDS);
                        RECT rcClient;
                        GetClientRect(pThis->m_hwnd, &rcClient);
                        if (rcItem.right < rcClient.right) rcItem.right = rcClient.right;
                        COLORREF zebraClr = Theme::Get().IsDarkMode() ? RGB(45, 50, 58) : RGB(236, 241, 248);
                        HBRUSH hbrZebra = CreateSolidBrush(zebraClr);
                        FillRect(pcd->nmcd.hdc, &rcItem, hbrZebra);
                        DeleteObject(hbrZebra);
                    }
                    return CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
                }
                case CDDS_ITEMPOSTPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;

                    // Grid lines
                    if (Config::Get().showGridLines && row >= 0) {
                        RECT rcItem;
                        ListView_GetItemRect(pThis->m_hwnd, row, &rcItem, LVIR_BOUNDS);
                        COLORREF gridClr = Theme::Get().IsDarkMode() ? RGB(58, 60, 65) : RGB(222, 226, 232);
                        HPEN hPen = CreatePen(PS_SOLID, 1, gridClr);
                        HPEN hOldPen = (HPEN)SelectObject(pcd->nmcd.hdc, hPen);
                        MoveToEx(pcd->nmcd.hdc, rcItem.left, rcItem.bottom - 1, nullptr);
                        LineTo(pcd->nmcd.hdc, rcItem.right, rcItem.bottom - 1);
                        int curX = rcItem.left;
                        for (int col = 0; col < 4; ++col) {
                            int w = ListView_GetColumnWidth(pThis->m_hwnd, col);
                            curX += w;
                            MoveToEx(pcd->nmcd.hdc, curX - 1, rcItem.top, nullptr);
                            LineTo(pcd->nmcd.hdc, curX - 1, rcItem.bottom);
                        }
                        SelectObject(pcd->nmcd.hdc, hOldPen);
                        DeleteObject(hPen);
                    }
                    return CDRF_DODEFAULT;
                }
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;
                    LRESULT ret = CDRF_DODEFAULT;
                    if (Config::Get().zebraStriping && !isSelected && (row % 2 == 1)) {
                        COLORREF zebraClr = Theme::Get().IsDarkMode() ? RGB(45, 50, 58) : RGB(236, 241, 248);
                        pcd->clrTextBk = zebraClr;
                        pcd->clrText = Theme::Get().ClrTextPrimary();
                        ret = CDRF_NEWFONT;
                    }
                    if (pcd->iSubItem == 3 && row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                        int idx = pThis->m_filteredIndices[row];
                        const auto& item = pThis->m_allItems[idx];
                        if (item.isDrive) {
                            uint64_t freeBytes = 0, totalBytes = 0;
                            if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                HDC hdc = pcd->nmcd.hdc;
                                RECT rcSub;
                                ListView_GetSubItemRect(pThis->m_hwnd, row, 3, LVIR_BOUNDS, &rcSub);

                                // Draw drive space text
                                wchar_t spaceText[128];
                                StringCchPrintfW(spaceText, 128, L"%s 可用 / 共 %s",
                                                 Util::FormatBytes(freeBytes).c_str(),
                                                 Util::FormatBytes(totalBytes).c_str());
                                SetBkMode(hdc, TRANSPARENT);
                                SetTextColor(hdc, Theme::Get().ClrTextSecondary());
                                HFONT hFont = Theme::Get().GetUIFont(pThis->m_hwnd);
                                HGDIOBJ oldFont = SelectObject(hdc, hFont);

                                RECT textRc = rcSub;
                                textRc.bottom -= Theme::Get().Scale(7, pThis->m_hwnd);
                                DrawTextW(hdc, spaceText, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                                // Draw mini capacity progress bar
                                RECT barRc = rcSub;
                                barRc.top = rcSub.bottom - Theme::Get().Scale(6, pThis->m_hwnd);
                                barRc.bottom = rcSub.bottom - Theme::Get().Scale(2, pThis->m_hwnd);
                                barRc.right -= Theme::Get().Scale(16, pThis->m_hwnd);

                                HBRUSH hbrBg = Theme::Get().BrushHover();
                                FillRect(hdc, &barRc, hbrBg);

                                uint64_t used = totalBytes - freeBytes;
                                int barW = barRc.right - barRc.left;
                                int fillW = (int)((used * barW) / totalBytes);

                                RECT fillRc = barRc;
                                fillRc.right = fillRc.left + fillW;

                                bool warning = (used * 100 / totalBytes) >= 90;
                                COLORREF barClr = warning ? RGB(232, 17, 35) : Theme::Get().ClrAccent();
                                HBRUSH hbrFill = CreateSolidBrush(barClr);
                                FillRect(hdc, &fillRc, hbrFill);
                                DeleteObject(hbrFill);

                                SelectObject(hdc, oldFont);
                                return CDRF_SKIPDEFAULT;
                            }
                        }
                    }
                    return ret;
                }
                }
                return CDRF_DODEFAULT;
            }

            case NM_DBLCLK: {
                NMITEMACTIVATE* pia = (NMITEMACTIVATE*)lParam;
                pThis->OnItemDoubleClicked(pia->iItem);
                return 0;
            }

            case LVN_ITEMCHANGED: {
                NMLISTVIEW* pnlv = (NMLISTVIEW*)lParam;
                if ((pnlv->uChanged & LVIF_STATE) && (pnlv->uNewState & LVIS_SELECTED)) {
                    if (pThis->onSelectionChanged) {
                        int row = pnlv->iItem;
                        if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                            int idx = pThis->m_filteredIndices[row];
                            pThis->onSelectionChanged(pThis->m_allItems[idx].fullPath);
                        }
                    }
                }
                break;
            }

            case LVN_COLUMNCLICK: {
                NMLISTVIEW* pnlv = (NMLISTVIEW*)lParam;
                SortColumn newCol = (SortColumn)pnlv->iSubItem;
                if (pThis->m_sortCol == newCol) {
                    pThis->m_sortAsc = !pThis->m_sortAsc;
                } else {
                    pThis->m_sortCol = newCol;
                    pThis->m_sortAsc = true;
                }
                pThis->SortItems();
                pThis->ApplyFilter();
                pThis->UpdateHeaderSortArrow();
                return 0;
            }
            }
        }
        break;
    }
    case FileListView::WM_NOTIFY_DIR_CHANGED:
        pThis->Refresh();
        return 0;

    case FileListView::WM_NOTIFY_FOLDER_SIZE:
        pThis->OnFolderSizeReady((FolderSizeResult*)lParam);
        return 0;

    case FileListView::WM_SEARCH_BATCH: {
        auto* pBatch = (std::vector<NeoShellItem>*)lParam;
        if (pBatch) {
            for (auto& item : *pBatch) {
                pThis->m_allItems.push_back(std::move(item));
            }
            delete pBatch;
            pThis->ApplyFilter();
        }
        return 0;
    }

    case FileListView::WM_SEARCH_COMPLETE: {
        pThis->SortItems();
        pThis->ApplyFilter();
        return 0;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
