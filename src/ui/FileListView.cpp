#include "..\core\Config.h"
#include "FileListView.h"
#include "resource.h"
#include <unordered_set>
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "msimg32.lib")
#include "Theme.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"

const UINT FileListView::WM_NOTIFY_DIR_CHANGED;
const UINT FileListView::WM_NOTIFY_FOLDER_SIZE;

FileListView::FileListView() {
    m_activeColumns = { ColumnId::Name, ColumnId::DateModified, ColumnId::Type, ColumnId::Size };
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
                    if (isMove) {
                        wchar_t sDir[MAX_PATH];
                        StringCchCopyW(sDir, MAX_PATH, src.c_str());
                        PathRemoveFileSpecW(sDir);
                        if (_wcsicmp(sDir, targetDir.c_str()) == 0) continue;
                    }
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
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, targetDir.c_str(), nullptr);
        if (isMove) {
            for (const auto& src : files) {
                wchar_t sDir[MAX_PATH];
                StringCchCopyW(sDir, MAX_PATH, src.c_str());
                PathRemoveFileSpecW(sDir);
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, sDir, nullptr);
            }
        }
        Refresh();
    };

    RegisterDragDrop(m_hwnd, m_pDropTarget);
    m_gesture.Attach(m_hwnd);

    return m_hwnd;
}

static std::wstring GetColumnTitle(ColumnId id) {
    bool isZh = I18n::Get().IsChinese();
    switch (id) {
    case ColumnId::Name:         return isZh ? L"名称" : L"Name";
    case ColumnId::DateModified: return isZh ? L"修改日期" : L"Date modified";
    case ColumnId::Type:         return isZh ? L"类型" : L"Type";
    case ColumnId::Size:         return isZh ? L"大小" : L"Size";
    case ColumnId::DateCreated:  return isZh ? L"创建日期" : L"Date created";
    case ColumnId::Attributes:   return isZh ? L"属性" : L"Attributes";
    case ColumnId::Authors:      return isZh ? L"作者" : L"Authors";
    case ColumnId::Title:        return isZh ? L"标题" : L"Title";
    case ColumnId::Tags:         return isZh ? L"标记" : L"Tags";
    case ColumnId::Path:         return isZh ? L"文件夹路径" : L"Folder path";
    default:                     return L"";
    }
}

static int GetDefaultColumnWidth(ColumnId id) {
    switch (id) {
    case ColumnId::Name:         return 300;
    case ColumnId::DateModified: return 160;
    case ColumnId::Type:         return 130;
    case ColumnId::Size:         return 120;
    case ColumnId::DateCreated:  return 160;
    case ColumnId::Attributes:   return 90;
    case ColumnId::Authors:      return 110;
    case ColumnId::Title:        return 130;
    case ColumnId::Tags:         return 110;
    case ColumnId::Path:         return 260;
    default:                     return 120;
    }
}

struct ChooseDetailsData {
    std::vector<ColumnId> activeCols;
    bool confirmed = false;
};

static LRESULT CALLBACK ChooseDetailsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ChooseDetailsData* pData = (ChooseDetailsData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_CREATE: {
        bool isZh = I18n::Get().IsChinese();
        HFONT hFont = Theme::Get().GetUIFont(hwnd);

        HWND hLabel = CreateWindowExW(0, WC_STATICW,
            isZh ? L"选择要在该文件夹中为此窗口显示的详细信息(&S):" : L"Select the details you want to display for the items in this folder(&S):",
            WS_CHILD | WS_VISIBLE, 15, 15, 340, 20, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
            15, 40, 340, 320, hwnd, (HMENU)1001, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_WIDTH;
        lvc.cx = 310;
        ListView_InsertColumn(hList, 0, &lvc);

        for (int i = 0; i < (int)ColumnId::Count; ++i) {
            ColumnId cid = (ColumnId)i;
            std::wstring title = GetColumnTitle(cid);
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.pszText = (LPWSTR)title.c_str();
            ListView_InsertItem(hList, &lvi);

            bool checked = false;
            if (pData) {
                for (auto c : pData->activeCols) {
                    if (c == cid) { checked = true; break; }
                }
            }
            if (cid == ColumnId::Name) checked = true;
            ListView_SetCheckState(hList, i, checked ? TRUE : FALSE);
        }

        HWND hBtnOk = CreateWindowExW(0, WC_BUTTONW, isZh ? L"确定" : L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            185, 375, 80, 28, hwnd, (HMENU)IDOK, nullptr, nullptr);
        SendMessageW(hBtnOk, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hBtnCancel = CreateWindowExW(0, WC_BUTTONW, isZh ? L"取消" : L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            275, 375, 80, 28, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
        SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK) {
            HWND hList = GetDlgItem(hwnd, 1001);
            if (pData && hList) {
                pData->activeCols.clear();
                pData->activeCols.push_back(ColumnId::Name);
                for (int i = 1; i < (int)ColumnId::Count; ++i) {
                    if (ListView_GetCheckState(hList, i)) {
                        pData->activeCols.push_back((ColumnId)i);
                    }
                }
                pData->confirmed = true;
            }
            DestroyWindow(hwnd);
            return 0;
        } else if (id == IDCANCEL) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void FileListView::ShowChooseColumnsDialog() {
    bool isZh = I18n::Get().IsChinese();
    WNDCLASSEXW wc = { sizeof(wc) };
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    if (!GetClassInfoExW(hInst, L"NeoMDIE_ChooseDetails", &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ChooseDetailsWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"NeoMDIE_ChooseDetails";
        RegisterClassExW(&wc);
    }

    ChooseDetailsData data;
    data.activeCols = m_activeColumns;

    RECT rcParent = { 0 };
    GetWindowRect(m_hwndParent ? m_hwndParent : m_hwnd, &rcParent);
    int dlgW = 385;
    int dlgH = 455;
    int x = rcParent.left + ((rcParent.right - rcParent.left) - dlgW) / 2;
    int y = rcParent.top + ((rcParent.bottom - rcParent.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, L"NeoMDIE_ChooseDetails",
        isZh ? L"选择详细信息" : L"Choose Details",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH,
        m_hwndParent ? m_hwndParent : m_hwnd,
        nullptr, hInst, &data
    );

    if (!hDlg) return;

    HWND hTopParent = GetAncestor(m_hwnd, GA_ROOT);
    EnableWindow(hTopParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hTopParent, TRUE);
    SetForegroundWindow(hTopParent);

    if (data.confirmed) {
        m_activeColumns = std::move(data.activeCols);
        RebuildColumns();
    }
}

void FileListView::RebuildColumns() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        int cnt = Header_GetItemCount(hHeader);
        for (int i = cnt - 1; i >= 0; --i) {
            ListView_DeleteColumn(m_hwnd, i);
        }
    }

    for (size_t i = 0; i < m_activeColumns.size(); ++i) {
        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = (int)i;
        lvc.cx = Theme::Get().Scale(GetDefaultColumnWidth(m_activeColumns[i]), m_hwnd);
        std::wstring title = GetColumnTitle(m_activeColumns[i]);
        lvc.pszText = (LPWSTR)title.c_str();
        ListView_InsertColumn(m_hwnd, (int)i, &lvc);
    }

    UpdateHeaderSortArrow();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::ToggleColumn(ColumnId id) {
    if (id == ColumnId::Name) return;
    auto it = std::find(m_activeColumns.begin(), m_activeColumns.end(), id);
    if (it != m_activeColumns.end()) {
        m_activeColumns.erase(it);
    } else {
        m_activeColumns.push_back(id);
    }
    RebuildColumns();
}

void FileListView::InitColumns() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowSubclass(hHeader, FileListView::HeaderSubclassProc, 1, (DWORD_PTR)this);
    }
    RebuildColumns();
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
        if (i < (int)m_activeColumns.size()) {
            ColumnId cid = m_activeColumns[i];
            bool match = false;
            switch (cid) {
            case ColumnId::Name:         match = (m_sortCol == SortColumn::Name); break;
            case ColumnId::DateModified: match = (m_sortCol == SortColumn::DateModified); break;
            case ColumnId::Type:         match = (m_sortCol == SortColumn::Type); break;
            case ColumnId::Size:         match = (m_sortCol == SortColumn::Size); break;
            case ColumnId::DateCreated:  match = (m_sortCol == SortColumn::DateCreated); break;
            case ColumnId::Attributes:   match = (m_sortCol == SortColumn::Attributes); break;
            case ColumnId::Authors:      match = (m_sortCol == SortColumn::Authors); break;
            case ColumnId::Title:        match = (m_sortCol == SortColumn::Title); break;
            default:                     match = false; break;
            }
            if (match) {
                hdi.fmt |= (m_sortAsc ? HDF_SORTUP : HDF_SORTDOWN);
            }
        }
        Header_SetItem(hHeader, i, &hdi);
    }
}

void FileListView::ShowColumnHeaderMenu(POINT ptScreen) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();

    for (int i = 0; i < (int)ColumnId::Count; ++i) {
        ColumnId cid = (ColumnId)i;
        std::wstring title = GetColumnTitle(cid);
        UINT flags = MF_STRING;

        bool active = false;
        for (auto c : m_activeColumns) {
            if (c == cid) { active = true; break; }
        }
        if (active) flags |= MF_CHECKED;
        if (cid == ColumnId::Name) flags |= MF_GRAYED;

        AppendMenuW(hMenu, flags, 100 + i, title.c_str());
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 10, isZh ? L"自动调整所有列宽(&A)" : L"Size All Columns to Fit(&A)");
    AppendMenuW(hMenu, MF_STRING, 20, isZh ? L"其他(&M)..." : L"More(&M)...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd >= 100 && cmd < 100 + (int)ColumnId::Count) {
        ColumnId cid = (ColumnId)(cmd - 100);
        if (cid != ColumnId::Name) {
            ToggleColumn(cid);
        }
    } else if (cmd == 10) {
        HWND hHeader = ListView_GetHeader(m_hwnd);
        int cnt = Header_GetItemCount(hHeader);
        for (int i = 0; i < cnt; ++i) {
            ListView_SetColumnWidth(m_hwnd, i, LVSCW_AUTOSIZE_USEHEADER);
        }
    } else if (cmd == 20) {
        ShowChooseColumnsDialog();
    }
}

LRESULT CALLBACK FileListView::HeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;
    if (msg == WM_RBUTTONUP || msg == WM_CONTEXTMENU) {
        POINT pt;
        GetCursorPos(&pt);
        if (pThis) pThis->ShowColumnHeaderMenu(pt);
        return 0;
    }
    if (Theme::Get().IsDarkMode()) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HGDIOBJ oldBM = SelectObject(memDC, memBM);

            FillRect(memDC, &rcClient, Theme::Get().BrushSurface());

            HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
            if (!hFont) hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(memDC, hFont);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, Theme::Get().ClrTextPrimary());

            HPEN hPenBorder = CreatePen(PS_SOLID, 1, Theme::Get().ClrBorder());
            HGDIOBJ oldPen = SelectObject(memDC, hPenBorder);

            int count = Header_GetItemCount(hwnd);
            for (int i = 0; i < count; ++i) {
                RECT rcItem;
                if (!Header_GetItemRect(hwnd, i, &rcItem)) continue;

                // Column divider
                MoveToEx(memDC, rcItem.right - 1, rcItem.top + 3, nullptr);
                LineTo(memDC, rcItem.right - 1, rcItem.bottom - 3);

                wchar_t text[256] = { 0 };
                HDITEMW hdi = { sizeof(hdi) };
                hdi.mask = HDI_TEXT | HDI_FORMAT;
                hdi.pszText = text;
                hdi.cchTextMax = 256;
                if (Header_GetItem(hwnd, i, &hdi)) {
                    RECT rcText = rcItem;
                    rcText.left += Theme::Get().Scale(6, hwnd);
                    rcText.right -= Theme::Get().Scale(6, hwnd);

                    std::wstring displayText = text;
                    if (hdi.fmt & HDF_SORTUP) {
                        displayText += L" ▲";
                    } else if (hdi.fmt & HDF_SORTDOWN) {
                        displayText += L" ▼";
                    }

                    UINT format = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
                    if (hdi.fmt & HDF_RIGHT) format |= DT_RIGHT;
                    else if (hdi.fmt & HDF_CENTER) format |= DT_CENTER;
                    else format |= DT_LEFT;

                    DrawTextW(memDC, displayText.c_str(), -1, &rcText, format);
                }
            }

            // Bottom border
            MoveToEx(memDC, 0, rcClient.bottom - 1, nullptr);
            LineTo(memDC, rcClient.right, rcClient.bottom - 1);

            SelectObject(memDC, oldPen);
            DeleteObject(hPenBorder);
            SelectObject(memDC, oldFont);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void FileListView::ApplyTheme() {
    if (!m_hwnd) return;
    bool isDark = Theme::Get().IsDarkMode();
    Theme::Get().ApplyToWindow(m_hwnd);
    SetWindowTheme(m_hwnd, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    COLORREF bg = Theme::Get().ClrBackground();
    COLORREF text = Theme::Get().ClrTextPrimary();
    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, bg);
    ListView_SetTextColor(m_hwnd, text);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowTheme(hHeader, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::UpdateColumnsLanguage() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (!hHeader) return;
    for (size_t i = 0; i < m_activeColumns.size(); ++i) {
        std::wstring title = GetColumnTitle(m_activeColumns[i]);
        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT;
        lvc.pszText = (LPWSTR)title.c_str();
        ListView_SetColumn(m_hwnd, (int)i, &lvc);
    }
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
    ListView_SetItemCountEx(m_hwnd, 0, 0);
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ApplyFilter();

    m_watcher.Watch(normalized, m_hwnd, WM_NOTIFY_DIR_CHANGED);

    if (m_calcFolderSize && normalized != L"此电脑" && normalized != L"This PC") {
        for (const auto& item : m_allItems) {
            if (item.isFolder && !item.isDrive) {
                m_calcSize.RequestFolderSize(item.fullPath, m_hwnd, WM_NOTIFY_FOLDER_SIZE);
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
    m_calcSize.CancelAll();

    std::vector<NeoShellItem> items;
    if (!ShellUtils::EnumerateDirectory(m_currentPath, m_showHidden, items)) {
        if (m_currentPath != L"This PC" && m_currentPath != L"此电脑" && m_currentPath != L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
            std::wstring parent = ShellUtils::GetParentPath(m_currentPath);
            if (parent.empty() || !PathFileExistsW(parent.c_str())) {
                NavigateTo(L"This PC", true);
            } else {
                NavigateTo(parent, true);
            }
        }
        return;
    }

    m_allItems = std::move(items);
    SortItems();

    // Reset item count to 0 first to force SysListView32 to flush all cached row visuals
    ListView_SetItemCountEx(m_hwnd, 0, 0);
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

    ApplyFilter();

    m_watcher.Watch(m_currentPath, m_hwnd, WM_NOTIFY_DIR_CHANGED);

    if (m_calcFolderSize && m_currentPath != L"此电脑" && m_currentPath != L"This PC" && m_currentPath != L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
        for (const auto& item : m_allItems) {
            if (item.isFolder && !item.isDrive) {
                m_calcSize.RequestFolderSize(item.fullPath, m_hwnd, WM_NOTIFY_FOLDER_SIZE);
            }
        }
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
        if (query.empty() || m_allItems.empty()) {
            Refresh();
        } else {
            ApplyFilter();
        }
    }
}

void FileListView::SearchRecursive(const std::wstring& rootDir, const std::wstring& query, bool matchCase) {
    HWND hwndTarget = m_hwnd; // Post directly to this pane's ListView so ListSubclassProc handles it
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
                            PostMessageW(m_hwnd, WM_SEARCH_BATCH, (WPARAM)this, (LPARAM)pBatch);
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
            PostMessageW(m_hwnd, WM_SEARCH_BATCH, (WPARAM)this, (LPARAM)pBatch);
        }
        PostMessageW(m_hwnd, WM_SEARCH_COMPLETE, (WPARAM)this, (LPARAM)totalFound);
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

    ListView_SetItemCountEx(m_hwnd, (int)m_filteredIndices.size(), LVSICF_NOSCROLL);
    if (!m_filteredIndices.empty()) {
        ListView_RedrawItems(m_hwnd, 0, (int)m_filteredIndices.size() - 1);
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
    UpdateWindow(m_hwnd);

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

void FileListView::ClearSelection() {
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

void FileListView::SetSelectedPaths(const std::vector<std::wstring>& paths) {
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (paths.empty()) return;

    std::unordered_set<std::wstring> selSet;
    for (const auto& p : paths) {
        std::wstring lower = p;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        selSet.insert(lower);
    }

    bool focusedSet = false;
    for (int row = 0; row < (int)m_filteredIndices.size(); ++row) {
        int idx = m_filteredIndices[row];
        if (idx >= 0 && idx < (int)m_allItems.size()) {
            std::wstring lower = m_allItems[idx].fullPath;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (selSet.count(lower)) {
                UINT state = LVIS_SELECTED;
                if (!focusedSet) {
                    state |= LVIS_FOCUSED;
                    focusedSet = true;
                    ListView_EnsureVisible(m_hwnd, row, FALSE);
                }
                ListView_SetItemState(m_hwnd, row, state, state);
            }
        }
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
            // In case of async virtual stream drop (UU remote / RDP etc.), trigger delayed catch-up refreshes
            SetTimer(m_hwnd, 701, 300, nullptr);
            SetTimer(m_hwnd, 702, 1200, nullptr);
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
        int count = ListView_GetItemCount(m_hwnd);
        if (count > 0) {
            ListView_SetItemState(m_hwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwnd, 0, FALSE);
        }
        SendMessageW(m_hwnd, WM_VSCROLL, SB_TOP, 0);
    }
}

void FileListView::ScrollToBottom() {
    if (m_hwnd) {
        SetFocus(m_hwnd);
        int count = ListView_GetItemCount(m_hwnd);
        if (count > 0) {
            ListView_SetItemState(m_hwnd, count - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwnd, count - 1, FALSE);
        }
        SendMessageW(m_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
    }
}

void FileListView::ScrollPageUp() {
    if (m_hwnd) {
        SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEUP, 0);
    }
}

void FileListView::ScrollPageDown() {
    if (m_hwnd) {
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
        bool ctrl = (GetKeyState(VK_CONTROL) < 0);
        bool shift = (GetKeyState(VK_SHIFT) < 0);
        if (ctrl && shift) {
            if (onCustomCommand) onCustomCommand(CMD_NEOMDIE_OPEN_BG_TAB, item.fullPath);
        } else if (ctrl) {
            if (onCustomCommand) onCustomCommand(CMD_NEOMDIE_OPEN_NEW_TAB, item.fullPath);
        } else if (shift) {
            if (onCustomCommand) onCustomCommand(CMD_NEOMDIE_OPEN_SPLIT, item.fullPath);
        } else {
            NavigateTo(item.fullPath, true);
        }
    } else {
        ShellUtils::OpenItem(m_hwnd, item.fullPath);
    }
}

void FileListView::OnItemRightClicked(POINT ptScreen) {
    auto handleNeoCmd = [this](int cmd, const std::wstring& targetPath) {
        if (cmd >= CMD_BG_NEW_DYNAMIC_BASE && cmd <= CMD_BG_NEW_DYNAMIC_MAX) {
            size_t idx = (size_t)(cmd - CMD_BG_NEW_DYNAMIC_BASE);
            const auto& items = ShellUtils::GetShellNewItems();
            if (idx < items.size()) {
                std::wstring newFilePath;
                if (ShellUtils::CreateShellNewFile(items[idx], m_currentPath, newFilePath)) {
                    Refresh();
                    SelectItemByPath(newFilePath);
                    StartRename();
                }
            }
            return;
        }

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

        case CMD_BG_REFRESH: {
            Refresh();
            HWND hTop = GetAncestor(m_hwnd, GA_ROOT);
            SendMessageW(hTop, WM_COMMAND, ID_VIEW_REFRESH, 0);
            break;
        }
        case CMD_BG_PASTE: PasteToCurrent(); break;
        case CMD_BG_PASTE_SHORTCUT: PasteShortcut(); break;
        case CMD_BG_PROPERTIES: ShellUtils::ShowItemProperties(m_hwnd, targetPath); break;
        case CMD_BG_NEW_SHORTCUT: {
            std::wstring args = L"appwiz.cpl,NewLinkHere \"" + m_currentPath + L"\"";
            ShellExecuteW(nullptr, L"open", L"rundll32.exe", args.c_str(), m_currentPath.c_str(), SW_SHOWNORMAL);
            break;
        }
        case CMD_BG_NEW_FOLDER: {
            std::wstring newDir = Util::CombinePath(m_currentPath, I18n::Get().T(StringId::NewFolder));
            int idx = 2;
            while (PathFileExistsW(newDir.c_str())) {
                newDir = Util::CombinePath(m_currentPath, std::wstring(I18n::Get().T(StringId::NewFolder)) + L" (" + std::to_wstring(idx++) + L")");
            }
            if (CreateDirectoryW(newDir.c_str(), nullptr)) {
                Refresh();
                SelectItemByPath(newDir);
                StartRename();
            }
            break;
        }
        case CMD_BG_NEW_FILE: {
            bool isZh = I18n::Get().IsChinese();
            std::wstring baseName = isZh ? L"新建文本文档.txt" : L"New Text Document.txt";
            std::wstring newFile = Util::CombinePath(m_currentPath, baseName);
            int idx = 2;
            while (PathFileExistsW(newFile.c_str())) {
                newFile = Util::CombinePath(m_currentPath, (isZh ? L"新建文本文档 (" : L"New Text Document (") + std::to_wstring(idx++) + L").txt");
            }
            HANDLE hF = CreateFileW(newFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) {
                CloseHandle(hF);
                Refresh();
                SelectItemByPath(newFile);
                StartRename();
            }
            break;
        }
        case CMD_BG_NEW_MD: {
            bool isZh = I18n::Get().IsChinese();
            std::wstring baseName = isZh ? L"新建 Markdown 文档.md" : L"New Markdown Document.md";
            std::wstring newMd = Util::CombinePath(m_currentPath, baseName);
            int idx = 2;
            while (PathFileExistsW(newMd.c_str())) {
                newMd = Util::CombinePath(m_currentPath, (isZh ? L"新建 Markdown 文档 (" : L"New Markdown Document (") + std::to_wstring(idx++) + L").md");
            }
            HANDLE hF = CreateFileW(newMd.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) {
                const char* initContent = "# Title\n\n";
                DWORD written = 0;
                WriteFile(hF, initContent, (DWORD)strlen(initContent), &written, nullptr);
                CloseHandle(hF);
                Refresh();
                SelectItemByPath(newMd);
                StartRename();
            }
            break;
        }
        case CMD_BG_NEW_CSV: {
            bool isZh = I18n::Get().IsChinese();
            std::wstring baseName = isZh ? L"新建表格.csv" : L"New Spreadsheet.csv";
            std::wstring newCsv = Util::CombinePath(m_currentPath, baseName);
            int idx = 2;
            while (PathFileExistsW(newCsv.c_str())) {
                newCsv = Util::CombinePath(m_currentPath, (isZh ? L"新建表格 (" : L"New Spreadsheet (") + std::to_wstring(idx++) + L").csv");
            }
            HANDLE hF = CreateFileW(newCsv.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) {
                const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
                DWORD written = 0;
                WriteFile(hF, bom, sizeof(bom), &written, nullptr);
                std::string header = isZh ? "序号,项目名称,创建时间,备注\r\n" : "ID,Name,CreatedTime,Notes\r\n";
                WriteFile(hF, header.c_str(), (DWORD)header.length(), &written, nullptr);
                CloseHandle(hF);
                Refresh();
                SelectItemByPath(newCsv);
                StartRename();
            }
            break;
        }
        case CMD_BG_NEW_RTF: {
            bool isZh = I18n::Get().IsChinese();
            std::wstring baseName = isZh ? L"新建富文本文档.rtf" : L"New Rich Text.rtf";
            std::wstring newRtf = Util::CombinePath(m_currentPath, baseName);
            int idx = 2;
            while (PathFileExistsW(newRtf.c_str())) {
                newRtf = Util::CombinePath(m_currentPath, (isZh ? L"新建富文本文档 (" : L"New Rich Text (") + std::to_wstring(idx++) + L").rtf");
            }
            HANDLE hF = CreateFileW(newRtf.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) {
                const char* rtfHeader = "{\\rtf1\\ansi\\ansicpg936\\deff0\\nouicompat\\deflang1033\\deflangfe2052{\\fonttbl{\\f0\\fnil\\fcharset134 \\'cb\\'ce\\'cc\\'e5;}}\r\n{\\*\\generator NeoMDIE}\\viewkind4\\uc1 \r\n\\pard\\lang2052\\f0\\fs22\\par\r\n}\r\n";
                DWORD written = 0;
                WriteFile(hF, rtfHeader, (DWORD)strlen(rtfHeader), &written, nullptr);
                CloseHandle(hF);
                Refresh();
                SelectItemByPath(newRtf);
                StartRename();
            }
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

void FileListView::SelectItemByPath(const std::wstring& path) {
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    for (int row = 0; row < (int)m_filteredIndices.size(); ++row) {
        int idx = m_filteredIndices[row];
        if (idx >= 0 && idx < (int)m_allItems.size()) {
            if (_wcsicmp(m_allItems[idx].fullPath.c_str(), path.c_str()) == 0) {
                ListView_SetItemState(m_hwnd, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(m_hwnd, row, FALSE);
                break;
            }
        }
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
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;

    case WM_LBUTTONDOWN: {
        bool wasFocused = (GetFocus() == hwnd);
        if (!wasFocused) {
            SetFocus(hwnd);
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            LVHITTESTINFO lvhti = { 0 };
            lvhti.pt = { x, y };
            int hit = ListView_HitTest(hwnd, &lvhti);

            bool ctrl = (GetKeyState(VK_CONTROL) < 0);
            bool shift = (GetKeyState(VK_SHIFT) < 0);

            if (hit >= 0) {
                if (!ctrl && !shift) {
                    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                    ListView_SetItemState(hwnd, hit, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(hwnd, hit, FALSE);
                    if (pThis->onSelectionChanged && hit < (int)pThis->m_filteredIndices.size()) {
                        int idx = pThis->m_filteredIndices[hit];
                        pThis->onSelectionChanged(pThis->m_allItems[idx].fullPath);
                    }
                }
            } else {
                if (!ctrl && !shift) {
                    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                }
            }
            return 0; // Consume click on activation from another pane to prevent unintended label rename
        }
        break; // Already focused: let native SysListView32 handle selection/drag/F2
    }
    case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = { x, y };
        int hit = ListView_HitTest(hwnd, &lvhti);
        if (hit < 0) {
            pThis->GoUp();
            return 0;
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        if (GetFocus() != hwnd) {
            SetFocus(hwnd);
        }
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

        pThis->m_rBtnDownPoint = { x, y };
        pThis->m_rBtnDownOnItem = (hit >= 0);
        pThis->m_gesture.OnRButtonDown(hwnd, x, y);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if ((wParam & MK_RBUTTON) && pThis->m_rBtnDownOnItem) {
            int dx = abs(x - pThis->m_rBtnDownPoint.x);
            int dy = abs(y - pThis->m_rBtnDownPoint.y);
            if (dx > GetSystemMetrics(SM_CXDRAG) || dy > GetSystemMetrics(SM_CYDRAG)) {
                pThis->m_rBtnDownOnItem = false;
                pThis->m_gesture.Detach();
                pThis->StartDragDrop(-1, true);
                pThis->m_gesture.Attach(hwnd);
                return 0;
            }
        }

        bool rDown = (wParam & MK_RBUTTON) != 0;
        if (pThis->m_gesture.OnMouseMove(hwnd, x, y, rDown)) {
            return 0;
        }
        break;
    }
    case WM_MBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        LVHITTESTINFO hti = { 0 };
        hti.pt.x = x;
        hti.pt.y = y;
        int hitIndex = ListView_HitTest(hwnd, &hti);
        if (hitIndex >= 0 && (hti.flags & (LVHT_ONITEM | LVHT_ONITEMICON | LVHT_ONITEMLABEL))) {
            if (hitIndex < (int)pThis->m_filteredIndices.size()) {
                int idx = pThis->m_filteredIndices[hitIndex];
                const auto& item = pThis->m_allItems[idx];
                if (item.isFolder) {
                    bool ctrl = (GetKeyState(VK_CONTROL) < 0);
                    bool shift = (GetKeyState(VK_SHIFT) < 0);
                    if (shift || ctrl) {
                        if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_NEW_TAB, item.fullPath);
                    } else {
                        if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_BG_TAB, item.fullPath);
                    }
                    return 0;
                }
            }
        }
        break;
    }
    case WM_RBUTTONUP: {
        pThis->m_rBtnDownOnItem = false;
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        GestureAction action = GestureAction::None;
        if (pThis->m_gesture.OnRButtonUp(hwnd, x, y, action)) {
            if (action != GestureAction::None) {
                if (pThis->onGestureAction) {
                    pThis->onGestureAction(action);
                } else {
                    switch (action) {
                    case GestureAction::NavBack:      pThis->GoBack(); break;
                    case GestureAction::NavForward:   pThis->GoForward(); break;
                    case GestureAction::NavUp:        pThis->GoUp(); break;
                    case GestureAction::Refresh:      pThis->Refresh(); break;
                    case GestureAction::ScrollTop:    pThis->ScrollToTop(); break;
                    case GestureAction::ScrollBottom: pThis->ScrollToBottom(); break;
                    case GestureAction::PageUp:       pThis->ScrollPageUp(); break;
                    case GestureAction::PageDown:     pThis->ScrollPageDown(); break;
                    default: break;
                    }
                }
            }
            return 0; // Handled gesture movement, suppress context menu
        }

        POINT ptScreen = { x, y };
        ClientToScreen(hwnd, &ptScreen);
        pThis->OnItemRightClicked(ptScreen);
        return 0;
    }
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) < 0);
        bool shift = (GetKeyState(VK_SHIFT) < 0);
        bool alt = (GetKeyState(VK_MENU) < 0);

        if (wParam == VK_RETURN && alt) { // Alt + Enter
            auto sel = pThis->GetSelectedPaths();
            if (!sel.empty()) {
                ShellUtils::ShowItemProperties(hwnd, sel[0]);
            } else {
                ShellUtils::ShowItemProperties(hwnd, pThis->GetCurrentPath());
            }
            return 0;
        }

        if (wParam == VK_F5) {
            HWND hTop = GetAncestor(hwnd, GA_ROOT);
            SendMessageW(hTop, WM_COMMAND, ID_VIEW_REFRESH, 0);
            return 0;
        }

        if (wParam == VK_F11) {
            HWND hTop = GetAncestor(hwnd, GA_ROOT);
            SendMessageW(hTop, WM_COMMAND, ID_WINDOW_MAXIMIZE, 0);
            return 0;
        }

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
            int selected = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
            if (selected != -1) {
                pThis->OnItemDoubleClicked(selected);
            }
            return 0;
        } else if (wParam == VK_BACK) {
            pThis->GoUp();
            return 0;
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == 701 || wParam == 702) {
            KillTimer(hwnd, wParam);
            pThis->Refresh();
            return 0;
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

LRESULT CALLBACK FileListView::ParentSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->hwndFrom == pThis->m_hwnd) {
            switch (pnm->code) {
            case LVN_BEGINDRAG: {
                NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
                pThis->StartDragDrop(pnmv->iItem, false);
                return 0;
            }
            case LVN_BEGINRDRAG: {
                NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
                pThis->StartDragDrop(pnmv->iItem, true);
                return 0;
            }
            case LVN_GETDISPINFOW: {
                NMLVDISPINFOW* pDisp = (NMLVDISPINFOW*)lParam;
                int row = pDisp->item.iItem;
                if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[row];
                    const auto& item = pThis->m_allItems[idx];

                    if (pDisp->item.mask & LVIF_TEXT) {
                        static wchar_t s_buf[MAX_PATH * 2];
                        s_buf[0] = L'\0';
                        int colIdx = pDisp->item.iSubItem;
                        if (colIdx >= 0 && colIdx < (int)pThis->m_activeColumns.size()) {
                            ColumnId colId = pThis->m_activeColumns[colIdx];
                            switch (colId) {
                            case ColumnId::Name:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.name.c_str());
                                break;
                            case ColumnId::DateModified:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatFileTime(item.modifiedTime).c_str());
                                break;
                            case ColumnId::Type:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.typeName.c_str());
                                break;
                            case ColumnId::Size:
                                if (item.isDrive) {
                                    uint64_t freeBytes = 0, totalBytes = 0;
                                    if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                        StringCchPrintfW(s_buf, ARRAYSIZE(s_buf), L"%s 可用 / 共 %s",
                                                         Util::FormatBytes(freeBytes).c_str(),
                                                         Util::FormatBytes(totalBytes).c_str());
                                    }
                                } else if (item.isFolder && !item.isSizeCalculated) {
                                    s_buf[0] = L'\0';
                                } else {
                                    StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatBytes(item.size).c_str());
                                }
                                break;
                            case ColumnId::DateCreated:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatFileTime(item.createdTime).c_str());
                                break;
                            case ColumnId::Attributes:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), Util::FormatAttributes(item.attributes).c_str());
                                break;
                            case ColumnId::Authors:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.authors.c_str());
                                break;
                            case ColumnId::Title:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.title.c_str());
                                break;
                            case ColumnId::Tags:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.tags.c_str());
                                break;
                            case ColumnId::Path:
                                StringCchCopyW(s_buf, ARRAYSIZE(s_buf), item.fullPath.c_str());
                                break;
                            }
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
                    return CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
                }
                case CDDS_ITEMPOSTPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;



                    // 2. Grid lines
                    if (Config::Get().showGridLines && row >= 0) {
                        RECT rcItem;
                        ListView_GetItemRect(pThis->m_hwnd, row, &rcItem, LVIR_BOUNDS);
                        COLORREF gridClr = Theme::Get().IsDarkMode() ? RGB(58, 60, 65) : RGB(222, 226, 232);
                        HPEN hPen = CreatePen(PS_SOLID, 1, gridClr);
                        HPEN hOldPen = (HPEN)SelectObject(pcd->nmcd.hdc, hPen);
                        MoveToEx(pcd->nmcd.hdc, rcItem.left, rcItem.bottom - 1, nullptr);
                        LineTo(pcd->nmcd.hdc, rcItem.right, rcItem.bottom - 1);
                        int curX = rcItem.left;
                        for (size_t col = 0; col < pThis->m_activeColumns.size(); ++col) {
                            int w = ListView_GetColumnWidth(pThis->m_hwnd, (int)col);
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
                    return CDRF_DODEFAULT;
                }
                }
                return CDRF_DODEFAULT;
            }

            case NM_DBLCLK: {
                NMITEMACTIVATE* pia = (NMITEMACTIVATE*)lParam;
                if (pia->iItem < 0) {
                    pThis->GoUp();
                } else {
                    pThis->OnItemDoubleClicked(pia->iItem);
                }
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
                if (pnlv->iSubItem >= 0 && pnlv->iSubItem < (int)pThis->m_activeColumns.size()) {
                    ColumnId colId = pThis->m_activeColumns[pnlv->iSubItem];
                    SortColumn clickedCol = SortColumn::Name;
                    switch (colId) {
                    case ColumnId::Name:         clickedCol = SortColumn::Name; break;
                    case ColumnId::DateModified: clickedCol = SortColumn::DateModified; break;
                    case ColumnId::Type:         clickedCol = SortColumn::Type; break;
                    case ColumnId::Size:         clickedCol = SortColumn::Size; break;
                    case ColumnId::DateCreated:  clickedCol = SortColumn::DateCreated; break;
                    case ColumnId::Attributes:   clickedCol = SortColumn::Attributes; break;
                    case ColumnId::Authors:      clickedCol = SortColumn::Authors; break;
                    case ColumnId::Title:        clickedCol = SortColumn::Title; break;
                    default:                     clickedCol = SortColumn::Name; break;
                    }

                    if (pThis->m_sortCol == clickedCol) {
                        pThis->m_sortAsc = !pThis->m_sortAsc;
                    } else {
                        pThis->m_sortCol = clickedCol;
                        pThis->m_sortAsc = true;
                    }
                    pThis->SortItems();
                    pThis->ApplyFilter();
                    pThis->UpdateHeaderSortArrow();
                }
                return 0;
            }
            }
        }
        break;
    }
    case FileListView::WM_NOTIFY_DIR_CHANGED:
        pThis->Refresh();
        break;

    case FileListView::WM_NOTIFY_FOLDER_SIZE:
        pThis->OnFolderSizeReady((FolderSizeResult*)lParam);
        break;
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

void FileListView::StartDragDrop(int itemIndex, bool isRightClick) {
    if (m_currentPath.empty() || m_currentPath == L"This PC" || m_currentPath == L"此电脑") {
        return;
    }

    if (itemIndex >= 0 && itemIndex < (int)m_filteredIndices.size()) {
        UINT state = ListView_GetItemState(m_hwnd, itemIndex, LVIS_SELECTED);
        if (!(state & LVIS_SELECTED)) {
            ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(m_hwnd, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    std::vector<std::wstring> sel = GetSelectedPaths();
    if (sel.empty() && itemIndex >= 0 && itemIndex < (int)m_filteredIndices.size()) {
        int idx = m_filteredIndices[itemIndex];
        sel.push_back(m_allItems[idx].fullPath);
    }

    std::vector<std::wstring> validPaths;
    for (const auto& p : sel) {
        if (PathFileExistsW(p.c_str())) {
            validPaths.push_back(p);
        }
    }
    if (validPaths.empty()) return;

    DWORD dwEffect = 0;
    HRESULT hr = ShellUtils::DoFileDragDrop(m_hwnd, m_currentPath, validPaths, isRightClick, &dwEffect);
    if (SUCCEEDED(hr) && dwEffect == DROPEFFECT_MOVE) {
        Refresh();
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, m_currentPath.c_str(), nullptr);
    }
}
