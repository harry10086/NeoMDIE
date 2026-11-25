#include "..\core\Config.h"
#include "FileListView.h"
#include "resource.h"
#include <unordered_set>
#include <propkey.h>
#include <propsys.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "propsys.lib")
#include "Theme.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"

const UINT FileListView::WM_NOTIFY_DIR_CHANGED;
const UINT FileListView::WM_NOTIFY_FOLDER_SIZE;

static void DrawThumbnail(HDC hdc, const RECT& rcIcon, HBITMAP hbmp, bool isSelected, bool isHot, HWND hwnd) {
    BITMAP bm;
    if (!GetObject(hbmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) return;

    int iconW = rcIcon.right - rcIcon.left;
    int iconH = rcIcon.bottom - rcIcon.top;
    if (iconW <= 0 || iconH <= 0) return;

    // Aspect ratio fit inside rcIcon
    int drawW = iconW;
    int drawH = iconH;
    if ((int64_t)bm.bmWidth * iconH > (int64_t)bm.bmHeight * iconW) {
        drawW = iconW;
        drawH = (int)((int64_t)bm.bmHeight * iconW / bm.bmWidth);
    } else {
        drawH = iconH;
        drawW = (int)((int64_t)bm.bmWidth * iconH / bm.bmHeight);
    }
    if (drawW < 1) drawW = 1;
    if (drawH < 1) drawH = 1;

    int posX = rcIcon.left + (iconW - drawW) / 2;
    int posY = rcIcon.top + (iconH - drawH) / 2;

    // Erase default system icon behind thumbnail
    HBRUSH hbrBg = isSelected ? Theme::Get().BrushSelected() : (isHot ? Theme::Get().BrushHover() : Theme::Get().BrushBackground());
    FillRect(hdc, &rcIcon, hbrBg);

    // High quality bicubic rendering via GDI+
    Gdiplus::Graphics g(hdc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

    DIBSECTION ds = { 0 };
    if (GetObject(hbmp, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBits && ds.dsBm.bmBitsPixel == 32) {
        uint32_t* pPixels = (uint32_t*)ds.dsBm.bmBits;
        int totalPixels = ds.dsBm.bmWidth * ds.dsBm.bmHeight;
        bool hasNonZeroAlpha = false;
        int step = (totalPixels > 1024) ? (totalPixels / 512) : 1;
        for (int i = 0; i < totalPixels; i += step) {
            BYTE a = (pPixels[i] >> 24) & 0xFF;
            if (a > 0) {
                hasNonZeroAlpha = true;
                break;
            }
        }

        if (hasNonZeroAlpha) {
            Gdiplus::Bitmap gpBmp(ds.dsBm.bmWidth, ds.dsBm.bmHeight, ds.dsBm.bmWidthBytes, PixelFormat32bppPARGB, (BYTE*)ds.dsBm.bmBits);
            g.DrawImage(&gpBmp, posX, posY, drawW, drawH);
            return;
        }
    }

    Gdiplus::Bitmap gpBmp(hbmp, nullptr);
    g.DrawImage(&gpBmp, posX, posY, drawW, drawH);
}

static void DrawSmoothCheckmark(HDC hdc, const RECT& rcBox, HWND hwnd) {
    int iconH = Theme::Get().Scale(10, hwnd);
    LOGFONTW lf = { 0 };
    lf.lfHeight = -iconH;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    StringCchCopyW(lf.lfFaceName, LF_FACESIZE, Theme::Get().GetIconFontFace());
    HFONT hFont = CreateFontIndirectW(&lf);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    wchar_t glyph[2] = { (wchar_t)0xE73E, 0 };
    RECT rcDraw = rcBox;
    DrawTextW(hdc, glyph, 1, &rcDraw, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

FileListView::FileListView() {
    m_foldersFirst = Config::Get().foldersFirst;
    m_hideDotFiles = Config::Get().hideDotFiles;
    m_activeColumns = { ColumnId::Name, ColumnId::DateModified, ColumnId::Type, ColumnId::Size };
}

FileListView::~FileListView() {
    SaveColumnWidths();
    ClearThumbnails();
    if (m_hFontText) {
        DeleteObject(m_hFontText);
        m_hFontText = nullptr;
    }
    if (m_hFontRowMetric) {
        DeleteObject(m_hFontRowMetric);
        m_hFontRowMetric = nullptr;
    }
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
    CancelCustomIconLoader();
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

    DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_SUBITEMIMAGES | LVS_EX_INFOTIP;
    if (Config::Get().showItemCheckBoxes) {
        exStyle |= LVS_EX_CHECKBOXES;
    }
    ListView_SetExtendedListViewStyle(m_hwnd, exStyle);
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_LABELTIP, 0);
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_INFOTIP, LVS_EX_INFOTIP);

    HWND hTip = ListView_GetToolTips(m_hwnd);
    if (hTip) {
        SendMessageW(hTip, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);
        SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 0, 500);
    }

    HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(false);
    HIMAGELIST himlLarge = ShellUtils::GetSystemImageList(true);
    ListView_SetImageList(m_hwnd, himlSmall, LVSIL_SMALL);
    ListView_SetImageList(m_hwnd, himlLarge, LVSIL_NORMAL);

    Theme::Get().ApplyToWindow(m_hwnd);
    SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    InitColumns();
    ApplyTheme();
    ApplyDensity(Config::Get().listDensity);
    UpdateHeaderCheckBox();

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
        if (ShellUtils::CopyOrMoveFiles(m_hwnd, targetDir, files, isMove)) {
            Refresh();
        }
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
        int defW = GetDefaultColumnWidth(m_activeColumns[i]);
        int savedW = !m_currentPath.empty() 
            ? Config::Get().GetFolderColumnWidth(m_currentPath, (int)m_activeColumns[i], defW)
            : Config::Get().GetColumnWidth((int)m_activeColumns[i], defW);
        lvc.cx = Theme::Get().Scale(savedW, m_hwnd);
        std::wstring title = GetColumnTitle(m_activeColumns[i]);
        lvc.pszText = (LPWSTR)title.c_str();
        ListView_InsertColumn(m_hwnd, (int)i, &lvc);
    }

    if (m_viewMode == 5) {
        ListView_EnsureVisible(m_hwnd, 0, FALSE);
        int hPos = GetScrollPos(m_hwnd, SB_HORZ);
        if (hPos > 0) {
            ListView_Scroll(m_hwnd, -hPos, 0);
        }
    }

    UpdateHeaderSortArrow();
    UpdateHeaderCheckBox();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::SaveColumnWidths() {
    if (!m_hwnd || m_viewMode != 0) return;
    for (size_t i = 0; i < m_activeColumns.size(); ++i) {
        int w = ListView_GetColumnWidth(m_hwnd, (int)i);
        if (w > 20) {
            int unscaled = Theme::Get().Unscale(w, m_hwnd);
            if (!m_currentPath.empty()) {
                Config::Get().SetFolderColumnWidth(m_currentPath, (int)m_activeColumns[i], unscaled);
            } else {
                Config::Get().SetColumnWidth((int)m_activeColumns[i], unscaled);
            }
        }
    }
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

void FileListView::UpdateHeaderCheckBox() {
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (!hHeader) return;
    HDITEMW hdi = { sizeof(hdi) };
    hdi.mask = HDI_FORMAT;
    if (Header_GetItem(hHeader, 0, &hdi)) {
        if (Config::Get().showItemCheckBoxes) {
            hdi.fmt |= HDF_CHECKBOX;
            int selCount = ListView_GetSelectedCount(m_hwnd);
            int totalCount = (int)m_filteredIndices.size();
            if (totalCount > 0 && selCount == totalCount) {
                hdi.fmt |= HDF_CHECKED;
            } else {
                hdi.fmt &= ~HDF_CHECKED;
            }
        } else {
            hdi.fmt &= ~(HDF_CHECKBOX | HDF_CHECKED);
        }
        Header_SetItem(hHeader, 0, &hdi);
    }
    InvalidateRect(hHeader, nullptr, TRUE);
}

void FileListView::ApplyItemCheckBoxes() {
    if (!m_hwnd) return;
    bool enable = Config::Get().showItemCheckBoxes;
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_CHECKBOXES, enable ? LVS_EX_CHECKBOXES : 0);
    UpdateHeaderCheckBox();
    InvalidateRect(m_hwnd, nullptr, TRUE);
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
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
        if (Config::Get().showItemCheckBoxes && pThis) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rcItem0;
            if (Header_GetItemRect(hwnd, 0, &rcItem0)) {
                RECT rcChkZone = rcItem0;
                rcChkZone.right = rcItem0.left + Theme::Get().Scale(26, hwnd);
                if (PtInRect(&rcChkZone, pt)) {
                    if (msg == WM_LBUTTONUP) {
                        int selCount = ListView_GetSelectedCount(pThis->m_hwnd);
                        int totalCount = (int)pThis->m_filteredIndices.size();
                        if (totalCount > 0 && selCount == totalCount) {
                            ListView_SetItemState(pThis->m_hwnd, -1, 0, LVIS_SELECTED);
                        } else {
                            pThis->SelectAll();
                        }
                        pThis->UpdateHeaderCheckBox();
                        pThis->NotifyStatus();
                        InvalidateRect(hwnd, nullptr, TRUE);
                    }
                    return 0; // Handled checkbox click, avoid sorting
                }
            }
        }
    }
    if (msg == WM_LBUTTONUP) {
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (pThis) pThis->SaveColumnWidths();
        return res;
    }
    if (msg == WM_SETFONT) {
        HFONT hFontArg = (HFONT)wParam;
        if (pThis && (hFontArg == pThis->m_hFontRowMetric || hFontArg == nullptr)) {
            wParam = (WPARAM)Theme::Get().GetUIFont(hwnd);
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_RBUTTONUP || msg == WM_CONTEXTMENU) {
        POINT pt;
        GetCursorPos(&pt);
        if (pThis) pThis->ShowColumnHeaderMenu(pt);
        return 0;
    }

    bool customHeader = true; // Always custom-draw header for consistent fonts, alignment, and borders
    if (customHeader) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HGDIOBJ oldBM = SelectObject(memDC, memBM);

            bool isDark = Theme::Get().IsDarkMode();
            HBRUSH hbrBg = isDark ? Theme::Get().BrushSurface() : CreateSolidBrush(RGB(246, 246, 248));
            FillRect(memDC, &rcClient, hbrBg);
            if (!isDark) DeleteObject(hbrBg);

            HFONT hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(memDC, hFont);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, isDark ? Theme::Get().ClrTextPrimary() : RGB(30, 30, 30));

            COLORREF borderClr = isDark ? Theme::Get().ClrBorder() : RGB(218, 220, 224);
            HPEN hPenBorder = CreatePen(PS_SOLID, 1, borderClr);
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

                    // Master checkbox for Column 0
                    if (i == 0 && Config::Get().showItemCheckBoxes) {
                        int boxSize = Theme::Get().Scale(14, hwnd);
                        int boxY = rcItem.top + (rcItem.bottom - rcItem.top - boxSize) / 2;
                        int rad = Theme::Get().Scale(3, hwnd);

                        // Align header checkbox with row checkbox
                        int boxLeft = rcItem.left + Theme::Get().Scale(3, hwnd);
                        if (pThis && pThis->m_hwnd && ListView_GetItemCount(pThis->m_hwnd) > 0) {
                            RECT rcIcon = {0};
                            if (ListView_GetItemRect(pThis->m_hwnd, 0, &rcIcon, LVIR_ICON)) {
                                int slotW = rcIcon.left - rcItem.left;
                                if (slotW > boxSize) {
                                    boxLeft = rcItem.left + (slotW - boxSize) / 2;
                                }
                            }
                        }

                        RECT rcBox = { boxLeft, boxY, boxLeft + boxSize, boxY + boxSize };

                        bool isChecked = (hdi.fmt & HDF_CHECKED) != 0;
                        COLORREF boxBgUnchecked = isDark ? RGB(45, 45, 45) : RGB(255, 255, 255);
                        COLORREF boxBorderUnchecked = isDark ? RGB(110, 110, 110) : RGB(160, 160, 160);
                        COLORREF accentClr = Theme::Get().ClrAccent();

                        HBRUSH hbrBox = CreateSolidBrush(isChecked ? accentClr : boxBgUnchecked);
                        HPEN hPenBox = CreatePen(PS_SOLID, 1, isChecked ? accentClr : boxBorderUnchecked);
                        HGDIOBJ oldBrush = SelectObject(memDC, hbrBox);
                        HGDIOBJ oldPen = SelectObject(memDC, hPenBox);

                        RoundRect(memDC, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, rad, rad);

                        SelectObject(memDC, oldBrush);
                        SelectObject(memDC, oldPen);
                        DeleteObject(hbrBox);
                        DeleteObject(hPenBox);

                        if (isChecked) {
                            DrawSmoothCheckmark(memDC, rcBox, hwnd);
                        }
                        rcText.left = rcBox.right + Theme::Get().Scale(6, hwnd);
                    }

                    std::wstring displayText = text;

                    bool hasSort = (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) != 0;
                    int gap = Theme::Get().Scale(11, hwnd);  // Clear breathing room between column title and sort chevron
                    int halfW = Theme::Get().Scale(4, hwnd); // Chevron half width
                    int halfH = Theme::Get().Scale(3, hwnd); // Chevron half height
                    int sortArrowSpace = hasSort ? (gap + halfW * 2 + Theme::Get().Scale(4, hwnd)) : 0;

                    RECT rcDrawText = rcText;
                    if (hasSort && !(hdi.fmt & HDF_RIGHT)) {
                        rcDrawText.right -= sortArrowSpace;
                    }

                    UINT format = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
                    if (hdi.fmt & HDF_RIGHT) format |= DT_RIGHT;
                    else if (hdi.fmt & HDF_CENTER) format |= DT_CENTER;
                    else format |= DT_LEFT;

                    DrawTextW(memDC, displayText.c_str(), -1, &rcDrawText, format);

                    // Draw clean modern Fluent/Explorer line chevron for sort direction
                    if (hasSort) {
                        SIZE szText = { 0 };
                        GetTextExtentPoint32W(memDC, displayText.c_str(), (int)displayText.length(), &szText);

                        int arrowCenterX = 0;
                        if (hdi.fmt & HDF_RIGHT) {
                            arrowCenterX = rcText.right - szText.cx - gap - halfW;
                        } else if (hdi.fmt & HDF_CENTER) {
                            int textLeft = rcDrawText.left + (rcDrawText.right - rcDrawText.left - szText.cx) / 2;
                            arrowCenterX = textLeft + szText.cx + gap + halfW;
                        } else {
                            int textRight = (std::min)(rcDrawText.left + szText.cx, rcDrawText.right);
                            arrowCenterX = textRight + gap + halfW;
                        }

                        int arrowCenterY = rcItem.top + (rcItem.bottom - rcItem.top) / 2;

                        if (arrowCenterX - halfW >= rcItem.left && arrowCenterX + halfW <= rcItem.right - 2) {
                            Gdiplus::Graphics g(memDC);
                            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

                            COLORREF arrowClr = isDark ? RGB(170, 170, 170) : RGB(100, 100, 100);
                            Gdiplus::Color gdiClr(255, GetRValue(arrowClr), GetGValue(arrowClr), GetBValue(arrowClr));
                            float penWidth = (float)Theme::Get().Scale(13, hwnd) / 10.0f;
                            if (penWidth < 1.2f) penWidth = 1.2f;

                            Gdiplus::Pen pen(gdiClr, penWidth);
                            pen.SetStartCap(Gdiplus::LineCapRound);
                            pen.SetEndCap(Gdiplus::LineCapRound);
                            pen.SetLineJoin(Gdiplus::LineJoinRound);

                            Gdiplus::PointF pts[3];
                            if (hdi.fmt & HDF_SORTUP) {
                                // Upward chevron (线形折线 ^)
                                pts[0] = Gdiplus::PointF((float)(arrowCenterX - halfW), (float)(arrowCenterY + halfH));
                                pts[1] = Gdiplus::PointF((float)arrowCenterX, (float)(arrowCenterY - halfH));
                                pts[2] = Gdiplus::PointF((float)(arrowCenterX + halfW), (float)(arrowCenterY + halfH));
                            } else {
                                // Downward chevron (线形折线 v)
                                pts[0] = Gdiplus::PointF((float)(arrowCenterX - halfW), (float)(arrowCenterY - halfH));
                                pts[1] = Gdiplus::PointF((float)arrowCenterX, (float)(arrowCenterY + halfH));
                                pts[2] = Gdiplus::PointF((float)(arrowCenterX + halfW), (float)(arrowCenterY - halfH));
                            }
                            g.DrawLines(&pen, pts, 3);
                        }
                    }
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
    ListView_SetTextBkColor(m_hwnd, m_viewMode == 0 ? bg : CLR_NONE);
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

bool FileListView::NavigateTo(const std::wstring& path, bool recordHistory, const PaneViewState* pRestoreState) {
    if (path.find(L"645FF040-5081-101B-9F08-00AA002F954E") != std::wstring::npos ||
        path.find(L"RecycleBinFolder") != std::wstring::npos) {
        ShellExecuteW(nullptr, L"open", L"explorer.exe", L"::{645FF040-5081-101B-9F08-00AA002F954E}", nullptr, SW_SHOWNORMAL);
        return false;
    }
    std::wstring normalized = Util::NormalizePath(path);
    if (!m_currentPath.empty()) {
        SaveColumnWidths();
        // Save current view state before leaving this folder
        if (m_historyIndex >= 0 && m_historyIndex < (int)m_historyStates.size()) {
            m_historyStates[m_historyIndex] = GetViewState();
        }
    }

    m_calcSize.CancelAll();
    CancelCustomIconLoader();
    ClearThumbnails();
    m_currentPath = normalized;
    m_hoverItem = -1;

    if (pRestoreState) {
        m_pendingRestoreState = *pRestoreState;
        m_hasPendingRestore = true;
    } else {
        m_hasPendingRestore = false;
    }

    if (recordHistory) {
        if (m_historyIndex >= 0 && m_historyIndex + 1 < (int)m_history.size()) {
            m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
            m_historyStates.erase(m_historyStates.begin() + m_historyIndex + 1, m_historyStates.end());
        }
        m_history.push_back(normalized);
        m_historyStates.push_back(PaneViewState{});
        m_historyIndex = (int)m_history.size() - 1;
    }

    // Restore per-folder view settings (Sort column, sort direction, view mode, foldersFirst, column widths)
    FolderViewSettings fvs = Config::Get().GetFolderView(normalized);
    if (fvs.foldersFirst >= 0) {
        m_foldersFirst = (fvs.foldersFirst != 0);
    } else {
        m_foldersFirst = Config::Get().foldersFirst;
    }
    if (fvs.sortCol >= 0) {
        m_sortCol = (SortColumn)fvs.sortCol;
        m_sortAsc = fvs.sortAsc;
    } else {
        m_sortCol = SortColumn::Name;
        m_sortAsc = true;
    }
    if (fvs.viewMode >= 0 && fvs.viewMode != m_viewMode) {
        SetViewMode(fvs.viewMode);
    } else if (fvs.viewMode < 0 && m_viewMode != Config::Get().viewMode) {
        SetViewMode(Config::Get().viewMode);
    } else if (m_viewMode == 0) {
        RebuildColumns();
    }

    bool isNetworkPath = (normalized.length() >= 2 && normalized[0] == L'\\' && normalized[1] == L'\\');
    if (!isNetworkPath && normalized.length() >= 2 && normalized[1] == L':') {
        wchar_t dRoot[4] = { normalized[0], L':', L'\\', L'\0' };
        if (GetDriveTypeW(dRoot) == DRIVE_REMOTE) {
            isNetworkPath = true;
        }
    }
    bool isVirtual = ShellUtils::IsVirtualShellPath(normalized);
    bool isSlowPath = isNetworkPath || isVirtual;

    if (isSlowPath) {
        std::vector<NeoShellItem> cachedItems;
        if (isVirtual && ShellUtils::GetCachedShellNamespace(normalized, m_showHidden, cachedItems, m_hideDotFiles)) {
            // Instant cache hit! No UI freeze, no repeated USB MTP queries
            m_isNavLoading = false;
            m_allItems = std::move(cachedItems);
            SortItems();
            UpdateHeaderSortArrow();
            ListView_SetItemCountEx(m_hwnd, 0, 0);
            ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ApplyFilter();
            if (m_hasPendingRestore) {
                RestoreViewState(m_pendingRestoreState);
                m_hasPendingRestore = false;
            }
            if (!m_pendingSelectPath.empty()) {
                std::wstring target = m_pendingSelectPath;
                m_pendingSelectPath.clear();
                SelectItemByPath(target);
            }
            StartCustomIconLoader();
            StartThumbnailLoader();
            if (onPathChanged) {
                onPathChanged(m_currentPath);
            }
            return true;
        }

        m_isNavLoading = true;
        m_allItems.clear();
        m_filteredIndices.clear();
        ListView_SetItemCountEx(m_hwnd, 0, 0);
        ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

        if (onStatusText) {
            onStatusText(isVirtual
                ? (I18n::Get().IsChinese() ? L"正在读取便携设备..." : L"Reading portable device...")
                : (I18n::Get().IsChinese() ? L"正在连接网络共享目录..." : L"Connecting to network share..."));
        }

        if (onPathChanged) {
            onPathChanged(m_currentPath);
        }

        InvalidateRect(m_hwnd, nullptr, TRUE);

        int gen = ++m_navGeneration;
        std::thread([hwnd = m_hwnd, path = normalized, showHidden = m_showHidden, hideDot = m_hideDotFiles, gen]() {
            HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            std::vector<NeoShellItem> items;
            bool ok = ShellUtils::EnumerateDirectory(path, showHidden, items, hideDot);
            auto* pRes = new AsyncNavResult{ gen, path, ok, std::move(items) };
            PostMessageW(hwnd, WM_ASYNC_NAV_COMPLETE, 0, (LPARAM)pRes);
            if (SUCCEEDED(hrCom)) {
                CoUninitialize();
            }
        }).detach();

        return true;
    }

    m_isNavLoading = false;
    std::vector<NeoShellItem> items;
    if (!ShellUtils::EnumerateDirectory(normalized, m_showHidden, items, m_hideDotFiles)) {
        return false;
    }

    m_allItems = std::move(items);

    SortItems();
    UpdateHeaderSortArrow();
    ListView_SetItemCountEx(m_hwnd, 0, 0);
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ApplyFilter();
    if (m_hasPendingRestore) {
        RestoreViewState(m_pendingRestoreState);
        m_hasPendingRestore = false;
    }
    if (!m_pendingSelectPath.empty()) {
        std::wstring target = m_pendingSelectPath;
        m_pendingSelectPath.clear();
        SelectItemByPath(target);
    }

    if (!ShellUtils::IsVirtualShellPath(normalized)) {
        m_watcher.Watch(normalized, m_hwnd, WM_NOTIFY_DIR_CHANGED);

        if (m_calcFolderSize && normalized != L"此电脑" && normalized != L"This PC") {
            for (const auto& item : m_allItems) {
                if (item.isFolder && !item.isDrive) {
                    m_calcSize.RequestFolderSize(item.fullPath, m_hwnd, WM_NOTIFY_FOLDER_SIZE);
                }
            }
        }
    }

    StartCustomIconLoader();
    StartThumbnailLoader();

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
    CancelCustomIconLoader();
    ClearThumbnails();

    bool isNetworkPath = (m_currentPath.length() >= 2 && m_currentPath[0] == L'\\' && m_currentPath[1] == L'\\');
    if (!isNetworkPath && m_currentPath.length() >= 2 && m_currentPath[1] == L':') {
        wchar_t dRoot[4] = { m_currentPath[0], L':', L'\\', L'\0' };
        if (GetDriveTypeW(dRoot) == DRIVE_REMOTE) {
            isNetworkPath = true;
        }
    }
    bool isVirtual = ShellUtils::IsVirtualShellPath(m_currentPath);
    bool isSlowPath = isNetworkPath || isVirtual;
    if (isSlowPath) {
        if (isVirtual) {
            ShellUtils::InvalidateVirtualShellCache(m_currentPath);
        }
        m_isNavLoading = true;
        if (onStatusText) {
            onStatusText(isVirtual
                ? (I18n::Get().IsChinese() ? L"正在刷新便携设备..." : L"Refreshing portable device...")
                : (I18n::Get().IsChinese() ? L"正在刷新网络共享目录..." : L"Refreshing network share..."));
        }
        InvalidateRect(m_hwnd, nullptr, TRUE);
        int gen = ++m_navGeneration;
        std::thread([hwnd = m_hwnd, path = m_currentPath, showHidden = m_showHidden, hideDot = m_hideDotFiles, gen]() {
            HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            std::vector<NeoShellItem> items;
            bool ok = ShellUtils::EnumerateDirectory(path, showHidden, items, hideDot);
            auto* pRes = new AsyncNavResult{ gen, path, ok, std::move(items) };
            PostMessageW(hwnd, WM_ASYNC_NAV_COMPLETE, 0, (LPARAM)pRes);
            if (SUCCEEDED(hrCom)) {
                CoUninitialize();
            }
        }).detach();
        return;
    }

    std::vector<NeoShellItem> items;
    if (!ShellUtils::EnumerateDirectory(m_currentPath, m_showHidden, items, m_hideDotFiles)) {
        if (m_currentPath != L"This PC" && m_currentPath != L"此电脑" && m_currentPath != L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
            std::wstring parent = ShellUtils::GetParentPath(m_currentPath);
            if (parent.empty() || !ShellUtils::IsValidDirectory(parent)) {
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

    StartCustomIconLoader();
    StartThumbnailLoader();
}

void FileListView::GoBack() {
    if (CanGoBack()) {
        if (m_historyIndex >= 0 && m_historyIndex < (int)m_historyStates.size()) {
            m_historyStates[m_historyIndex] = GetViewState();
        }
        m_historyIndex--;
        PaneViewState targetState = m_historyStates[m_historyIndex];
        NavigateTo(m_history[m_historyIndex], false, &targetState);
    }
}

void FileListView::GoForward() {
    if (CanGoForward()) {
        if (m_historyIndex >= 0 && m_historyIndex < (int)m_historyStates.size()) {
            m_historyStates[m_historyIndex] = GetViewState();
        }
        m_historyIndex++;
        PaneViewState targetState = m_historyStates[m_historyIndex];
        NavigateTo(m_history[m_historyIndex], false, &targetState);
    }
}

void FileListView::GoUp() {
    std::wstring prevFolder = m_currentPath;
    std::wstring parent = ShellUtils::GetParentPath(m_currentPath);
    if (parent.empty() || parent == m_currentPath) return;

    if (m_historyIndex >= 0 && m_historyIndex < (int)m_historyStates.size()) {
        m_historyStates[m_historyIndex] = GetViewState();
    }

    // Check if parent path was recorded previously in history
    std::wstring normParent = Util::NormalizePath(parent);
    const PaneViewState* pFoundState = nullptr;
    PaneViewState foundState;
    for (int i = m_historyIndex - 1; i >= 0; --i) {
        if (i < (int)m_history.size() && Util::NormalizePath(m_history[i]) == normParent) {
            foundState = m_historyStates[i];
            pFoundState = &foundState;
            break;
        }
    }

    m_pendingSelectPath = prevFolder;
    NavigateTo(parent, true, pFoundState);
}

void FileListView::SetFilter(const std::wstring& query, bool subfolders, bool matchCase) {
    m_searchCancelled = true;
    if (m_searchThread.joinable()) {
        m_searchThread.detach();
    }

    if (m_filterQuery == query && !subfolders) {
        return;
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
        if (m_allItems.empty()) {
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
                        item.iconIndex = ShellUtils::GetItemIconIndex(full, fd.dwFileAttributes, isFolder, true);
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
            return CompareShellItems(a, b, m_sortCol, m_sortAsc, m_foldersFirst);
        });
}

void FileListView::ReSort() {
    SortItems();
    ApplyFilter();
    UpdateHeaderSortArrow();
}

void FileListView::SetViewMode(int mode) {
    m_viewMode = std::clamp(mode, 0, 7);
    if (!m_currentPath.empty()) {
        Config::Get().SetFolderViewMode(m_currentPath, m_viewMode);
    }
    DWORD style = GetWindowLongW(m_hwnd, GWL_STYLE) & ~LVS_TYPEMASK;
    DWORD lvView = LV_VIEW_DETAILS;
    HIMAGELIST himlNormal = ShellUtils::GetSystemImageList(SHIL_LARGE);
    HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(SHIL_SMALL);

    switch (m_viewMode) {
    case 0: // Details (详细信息)
        style |= LVS_REPORT;
        lvView = LV_VIEW_DETAILS;
        break;
    case 1: // Extra Large Icons (超大图标 256x256)
        style |= LVS_ICON | LVS_AUTOARRANGE;
        lvView = LV_VIEW_ICON;
        himlNormal = ShellUtils::GetSystemImageList(SHIL_JUMBO);
        break;
    case 2: // Large Icons (大图标 96x96)
        style |= LVS_ICON | LVS_AUTOARRANGE;
        lvView = LV_VIEW_ICON;
        himlNormal = ShellUtils::GetSystemImageList(SHIL_EXTRALARGE);
        break;
    case 3: // Medium Icons (中等图标 48x48)
        style |= LVS_ICON | LVS_AUTOARRANGE;
        lvView = LV_VIEW_ICON;
        himlNormal = ShellUtils::GetSystemImageList(SHIL_EXTRALARGE);
        break;
    case 4: // Small Icons (小图标 16x16)
        style |= LVS_ICON;
        lvView = LV_VIEW_TILE;
        himlNormal = ShellUtils::GetSystemImageList(SHIL_SMALL);
        break;
    case 5: // List (列表)
        style |= LVS_LIST;
        lvView = LV_VIEW_LIST;
        break;
    case 6: // Tiles (平铺)
        style |= LVS_ICON;
        lvView = LV_VIEW_TILE;
        himlNormal = ShellUtils::GetSystemImageList(SHIL_EXTRALARGE);
        break;
    case 7: // Content (内容)
        style |= LVS_ICON;
        lvView = LV_VIEW_TILE;
        himlNormal = ShellUtils::GetSystemImageList(SHIL_LARGE);
        break;
    default:
        style |= LVS_REPORT;
        lvView = LV_VIEW_DETAILS;
        break;
    }

    ListView_SetImageList(m_hwnd, himlNormal, LVSIL_NORMAL);
    ListView_SetImageList(m_hwnd, himlSmall, LVSIL_SMALL);
    SetWindowLongW(m_hwnd, GWL_STYLE, style);
    SendMessageW(m_hwnd, LVM_SETVIEW, lvView, 0);

    if (m_viewMode == 0) {
        ListView_SetTextBkColor(m_hwnd, Theme::Get().ClrBackground());
    } else {
        ListView_SetTextBkColor(m_hwnd, CLR_NONE);
    }

    ApplyViewMetrics();

    m_hoverItem = -1;
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_LABELTIP, 0);
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_INFOTIP, LVS_EX_INFOTIP);
    HWND hTip = ListView_GetToolTips(m_hwnd);
    if (hTip) {
        SendMessageW(hTip, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);
        SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 0, 500);
    }

    if (m_viewMode == 1 || m_viewMode == 2 || m_viewMode == 3 || m_viewMode == 6 || m_viewMode == 7) {
        StartThumbnailLoader();
    } else {
        CancelThumbnailLoader();
    }

    if (m_viewMode == 5) {
        ListView_EnsureVisible(m_hwnd, 0, FALSE);
        int hPos = GetScrollPos(m_hwnd, SB_HORZ);
        if (hPos > 0) {
            ListView_Scroll(m_hwnd, -hPos, 0);
        }
    }

    UpdateHeaderSortArrow();
    UpdateHeaderCheckBox();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileListView::SetShowHidden(bool show) {
    if (m_showHidden != show) {
        m_showHidden = show;
        Refresh();
    }
}

void FileListView::SetHideDotFiles(bool hide) {
    if (m_hideDotFiles != hide) {
        m_hideDotFiles = hide;
        Refresh();
    }
}

int FileListView::GetDesiredRowHeight() const {
    int dpi = (int)Theme::Get().GetDpi(m_hwnd);
    int density = Config::Get().listDensity; // 0: 紧凑, 1: 适中, 2: 宽松
    int scale = Config::Get().listScale;     // 0: 偏小, 1: 标准, 2: 放大

    int baseFontPt = (scale == 0) ? 9 : ((scale == 2) ? 12 : 10);
    int fontPx = MulDiv(baseFontPt * 4 / 3, dpi, 96);

    int padPx = (density == 0) ? 4 : ((density == 2) ? 16 : 9);
    int desiredH = fontPx + MulDiv(padPx, dpi, 96);
    int minH = MulDiv(18, dpi, 96);
    if (desiredH < minH) desiredH = minH;
    return desiredH;
}

void FileListView::ApplyDensity(int density) {
    Config::Get().listDensity = density;
    ApplyViewMetrics();

    m_hoverItem = -1;
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_LABELTIP, 0);
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_INFOTIP, LVS_EX_INFOTIP);
    HWND hTip = ListView_GetToolTips(m_hwnd);
    if (hTip) {
        SendMessageW(hTip, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);
        SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 0, 500);
    }
}

void FileListView::ApplyScale(int scale) {
    Config::Get().listScale = scale;
    ApplyViewMetrics();

    m_hoverItem = -1;
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_LABELTIP, 0);
    ListView_SetExtendedListViewStyleEx(m_hwnd, LVS_EX_INFOTIP, LVS_EX_INFOTIP);
    HWND hTip = ListView_GetToolTips(m_hwnd);
    if (hTip) {
        SendMessageW(hTip, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);
        SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 0, 500);
    }
}

void FileListView::ApplyViewMetrics() {
    if (!m_hwnd) return;

    int scale = Config::Get().listScale;
    int density = Config::Get().listDensity;

    int currentDpi = (int)Theme::Get().GetDpi(m_hwnd);

    // 1. Text display font (controlled by listScale: 偏小 9pt, 标准 10pt, 放大 12pt)
    int fontSizePt = 10;
    if (scale == 0) fontSizePt = 9;       // 偏小
    else if (scale == 2) fontSizePt = 12; // 放大

    int fontHeight = -MulDiv(fontSizePt, currentDpi, 72);

    LOGFONTW lf = { 0 };
    if (!SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &lf, 0)) {
        StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
    }
    lf.lfHeight = fontHeight;
    lf.lfWeight = FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;

    HFONT hNewTextFont = CreateFontIndirectW(&lf);
    if (m_hFontText) {
        DeleteObject(m_hFontText);
    }
    m_hFontText = hNewTextFont;

    // 2. Row metric font (controlled by listDensity: 紧凑 ~20-22px, 适中 ~28-30px, 宽松 ~38-42px)
    // SysListView32 uses the font assigned via WM_SETFONT to determine line height in Report/Details view
    int baseMetric = (density == 0) ? 14 : ((density == 2) ? 27 : 20);
    if (scale == 0) baseMetric -= 1;
    else if (scale == 2) baseMetric += 2;
    int metricH = MulDiv(baseMetric, currentDpi, 96);

    LOGFONTW lfMetric = lf;
    lfMetric.lfHeight = -metricH;
    HFONT hNewMetricFont = CreateFontIndirectW(&lfMetric);

    // Set font on ListView control:
    // IMPORTANT: Details view (0) uses m_hFontRowMetric to control row height,
    // while all other views (Extra Large, Large, Medium, Small, List, Tiles, Content)
    // MUST use m_hFontText so labels and filenames match Windows Explorer standard size!
    if (m_viewMode == 0) {
        SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)hNewMetricFont, TRUE);
    } else if (m_viewMode == 1 || m_viewMode == 2 || m_viewMode == 3) {
        // Reserve enough vertical metric height for 2-3 wrapped label lines + bottom padding
        // so SysListView32 scroll range does not clip the bottom row in Medium/Large/Extra Large views
        int iconMetricH = MulDiv(34, currentDpi, 96);
        LOGFONTW lfIconMetric = lf;
        lfIconMetric.lfHeight = -iconMetricH;
        HFONT hIconMetric = CreateFontIndirectW(&lfIconMetric);
        SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)hIconMetric, TRUE);
        DeleteObject(hNewMetricFont);
        hNewMetricFont = hIconMetric;
    } else {
        SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)hNewTextFont, TRUE);
    }

    if (m_hFontRowMetric) {
        DeleteObject(m_hFontRowMetric);
    }
    m_hFontRowMetric = hNewMetricFont;

    // Keep header font at clean standard size
    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        SendMessageW(hHeader, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);
    }

    // 3. Spacing & layout metrics based on view mode (matching Windows Explorer)
    if (m_viewMode == 1) { // Extra Large Icons (256x256 Jumbo)
        int spX = 280;
        int spY = 345;
        if (scale == 2) { spX = spX * 5 / 4; spY = spY * 5 / 4; }
        else if (scale == 0) { spX = spX * 9 / 10; spY = spY * 9 / 10; }
        ListView_SetIconSpacing(m_hwnd, Theme::Get().Scale(spX, m_hwnd), Theme::Get().Scale(spY, m_hwnd));
        SendMessageW(m_hwnd, LVM_ARRANGE, LVA_DEFAULT, 0);
    } else if (m_viewMode == 2) { // Large Icons (96x96)
        int spX = (density == 0) ? 130 : ((density == 2) ? 165 : 145);
        int spY = (density == 0) ? 150 : ((density == 2) ? 190 : 170);
        if (scale == 2) { spX = spX * 5 / 4; spY = spY * 5 / 4; }
        else if (scale == 0) { spX = spX * 9 / 10; spY = spY * 9 / 10; }
        ListView_SetIconSpacing(m_hwnd, Theme::Get().Scale(spX, m_hwnd), Theme::Get().Scale(spY, m_hwnd));
        SendMessageW(m_hwnd, LVM_ARRANGE, LVA_DEFAULT, 0);
    } else if (m_viewMode == 3) { // Medium Icons (48x48)
        int spX = (density == 0) ? 110 : ((density == 2) ? 140 : 124);
        int spY = (density == 0) ? 115 : ((density == 2) ? 145 : 130);
        if (scale == 2) { spX = spX * 5 / 4; spY = spY * 5 / 4; }
        else if (scale == 0) { spX = spX * 9 / 10; spY = spY * 9 / 10; }
        ListView_SetIconSpacing(m_hwnd, Theme::Get().Scale(spX, m_hwnd), Theme::Get().Scale(spY, m_hwnd));
        SendMessageW(m_hwnd, LVM_ARRANGE, LVA_DEFAULT, 0);
    } else if (m_viewMode == 4) { // Small Icons (小图标 16x16)
        int tileW = Theme::Get().Scale((density == 0) ? 220 : ((density == 2) ? 320 : 260), m_hwnd);
        int tileH = Theme::Get().Scale((density == 0) ? 26 : ((density == 2) ? 32 : 28), m_hwnd);
        if (scale == 2) { tileW = tileW * 5 / 4; tileH = tileH * 5 / 4; }
        else if (scale == 0) { tileW = tileW * 9 / 10; tileH = tileH * 9 / 10; }
        LVTILEVIEWINFO tvi = { 0 };
        tvi.cbSize = sizeof(LVTILEVIEWINFO);
        tvi.dwMask = LVTVIM_TILESIZE | LVTVIM_COLUMNS | LVTVIM_LABELMARGIN;
        tvi.dwFlags = LVTVIF_FIXEDSIZE;
        tvi.sizeTile.cx = tileW;
        tvi.sizeTile.cy = tileH;
        tvi.cLines = 0; // Single line, filename only without truncation!
        tvi.rcLabelMargin.left = Theme::Get().Scale(4, m_hwnd);
        tvi.rcLabelMargin.top = Theme::Get().Scale(2, m_hwnd);
        tvi.rcLabelMargin.right = Theme::Get().Scale(4, m_hwnd);
        tvi.rcLabelMargin.bottom = Theme::Get().Scale(5, m_hwnd);
        SendMessageW(m_hwnd, LVM_SETTILEVIEWINFO, 0, (LPARAM)&tvi);
    } else if (m_viewMode == 5) { // List (列表 16x16)
        int colW = Theme::Get().Scale((density == 0) ? 220 : ((density == 2) ? 320 : 260), m_hwnd);
        if (scale == 2) { colW = colW * 5 / 4; }
        else if (scale == 0) { colW = colW * 9 / 10; }
        ListView_SetColumnWidth(m_hwnd, 0, colW);
    } else if (m_viewMode == 6) { // Tiles (平铺)
        int tileW = Theme::Get().Scale((density == 0) ? 240 : ((density == 2) ? 320 : 280), m_hwnd);
        int tileH = Theme::Get().Scale((density == 0) ? 50 : ((density == 2) ? 64 : 56), m_hwnd);
        if (scale == 2) { tileW = tileW * 5 / 4; tileH = tileH * 5 / 4; }
        else if (scale == 0) { tileW = tileW * 9 / 10; tileH = tileH * 9 / 10; }
        LVTILEVIEWINFO tvi = { 0 };
        tvi.cbSize = sizeof(LVTILEVIEWINFO);
        tvi.dwMask = LVTVIM_TILESIZE | LVTVIM_COLUMNS | LVTVIM_LABELMARGIN;
        tvi.dwFlags = LVTVIF_FIXEDSIZE;
        tvi.sizeTile.cx = tileW;
        tvi.sizeTile.cy = tileH;
        tvi.cLines = 2; // Show 2 extra lines under title (Type and Size)
        tvi.rcLabelMargin.left = Theme::Get().Scale(6, m_hwnd);
        tvi.rcLabelMargin.top = Theme::Get().Scale(2, m_hwnd);
        tvi.rcLabelMargin.right = Theme::Get().Scale(6, m_hwnd);
        tvi.rcLabelMargin.bottom = Theme::Get().Scale(2, m_hwnd);
        SendMessageW(m_hwnd, LVM_SETTILEVIEWINFO, 0, (LPARAM)&tvi);
    } else if (m_viewMode == 7) { // Content (内容)
        RECT rcClient = { 0 };
        GetClientRect(m_hwnd, &rcClient);
        int clientW = rcClient.right - rcClient.left;
        int tileW = clientW - GetSystemMetrics(SM_CXVSCROLL) - Theme::Get().Scale(16, m_hwnd);
        if (tileW < Theme::Get().Scale(300, m_hwnd)) tileW = Theme::Get().Scale(300, m_hwnd);
        int tileH = Theme::Get().Scale((density == 0) ? 46 : ((density == 2) ? 58 : 52), m_hwnd);
        if (scale == 2) { tileH = tileH * 5 / 4; }
        else if (scale == 0) { tileH = tileH * 9 / 10; }

        LVTILEVIEWINFO tvi = { 0 };
        tvi.cbSize = sizeof(LVTILEVIEWINFO);
        tvi.dwMask = LVTVIM_TILESIZE;
        tvi.dwFlags = LVTVIF_FIXEDSIZE;
        tvi.sizeTile.cx = tileW;
        tvi.sizeTile.cy = tileH;
        SendMessageW(m_hwnd, LVM_SETTILEVIEWINFO, 0, (LPARAM)&tvi);
    }

    InvalidateRect(m_hwnd, nullptr, TRUE);
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
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

PaneViewState FileListView::GetViewState() const {
    PaneViewState state;
    if (!m_hwnd) return state;

    state.selectedPaths = GetSelectedPaths();
    state.sortCol = (int)m_sortCol;
    state.sortAsc = m_sortAsc;
    state.viewMode = m_viewMode;
    state.foldersFirst = m_foldersFirst ? 1 : 0;

    int focusedRow = ListView_GetNextItem(m_hwnd, -1, LVNI_FOCUSED);
    if (focusedRow >= 0 && focusedRow < (int)m_filteredIndices.size()) {
        int idx = m_filteredIndices[focusedRow];
        if (idx >= 0 && idx < (int)m_allItems.size()) {
            state.focusedPath = m_allItems[idx].fullPath;
        }
    }

    state.topIndex = ListView_GetTopIndex(m_hwnd);
    SCROLLINFO siV = { sizeof(SCROLLINFO), SIF_POS };
    if (GetScrollInfo(m_hwnd, SB_VERT, &siV)) {
        state.scrollPosY = siV.nPos;
    }
    if (state.topIndex >= 0 && state.topIndex < (int)m_filteredIndices.size()) {
        int idx = m_filteredIndices[state.topIndex];
        if (idx >= 0 && idx < (int)m_allItems.size()) {
            state.topItemPath = m_allItems[idx].fullPath;
        }
    }
    return state;
}

void FileListView::RestoreViewState(const PaneViewState& state) {
    if (!m_hwnd) return;

    // 0. Restore view mode & sort column first (before matching rows and scroll)
    if (state.viewMode >= 0 && m_viewMode != state.viewMode) {
        SetViewMode(state.viewMode);
    }
    if (state.foldersFirst >= 0 && m_foldersFirst != (state.foldersFirst != 0)) {
        m_foldersFirst = (state.foldersFirst != 0);
        SortItems();
        ApplyFilter();
    }
    if (state.sortCol >= 0 && ((int)m_sortCol != state.sortCol || m_sortAsc != state.sortAsc)) {
        m_sortCol = (SortColumn)state.sortCol;
        m_sortAsc = state.sortAsc;
        SortItems();
        ApplyFilter();
        UpdateHeaderSortArrow();
    }

    // 1. Clear previous selection & focus
    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

    // 2. Restore selected items
    std::unordered_set<std::wstring> selSet;
    for (const auto& p : state.selectedPaths) {
        std::wstring lower = p;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        selSet.insert(lower);
    }

    int focusedRow = -1;
    std::wstring lowerFocused = state.focusedPath;
    std::transform(lowerFocused.begin(), lowerFocused.end(), lowerFocused.begin(), ::towlower);

    int targetTopRow = -1;
    std::wstring lowerTop = state.topItemPath;
    std::transform(lowerTop.begin(), lowerTop.end(), lowerTop.begin(), ::towlower);

    for (int row = 0; row < (int)m_filteredIndices.size(); ++row) {
        int idx = m_filteredIndices[row];
        if (idx >= 0 && idx < (int)m_allItems.size()) {
            std::wstring lower = m_allItems[idx].fullPath;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

            if (selSet.count(lower)) {
                ListView_SetItemState(m_hwnd, row, LVIS_SELECTED, LVIS_SELECTED);
            }
            if (!lowerFocused.empty() && lower == lowerFocused) {
                focusedRow = row;
            }
            if (!lowerTop.empty() && lower == lowerTop) {
                targetTopRow = row;
            }
        }
    }

    // If focusedRow wasn't matched by path, but we have selected items, focus the first selected
    if (focusedRow == -1 && !state.selectedPaths.empty()) {
        focusedRow = ListView_GetNextItem(m_hwnd, -1, LVNI_SELECTED);
    }
    if (focusedRow >= 0 && focusedRow < (int)m_filteredIndices.size()) {
        ListView_SetItemState(m_hwnd, focusedRow, LVIS_FOCUSED, LVIS_FOCUSED);
    }

    // 3. Restore scroll position / top visible item
    if (targetTopRow == -1) {
        targetTopRow = state.topIndex;
    }
    int totalCount = (int)m_filteredIndices.size();
    if (totalCount > 0) {
        targetTopRow = std::clamp(targetTopRow, 0, totalCount - 1);
        if (m_viewMode == 0) {
            // Details view (vertical report scrolling)
            if (targetTopRow > 0) {
                ListView_EnsureVisible(m_hwnd, totalCount - 1, FALSE);
                ListView_EnsureVisible(m_hwnd, targetTopRow, FALSE);

                int currentTop = ListView_GetTopIndex(m_hwnd);
                int delta = targetTopRow - currentTop;
                if (delta != 0) {
                    RECT rcItem = { 0 };
                    if (ListView_GetItemRect(m_hwnd, 0, &rcItem, LVIR_BOUNDS)) {
                        int rowH = rcItem.bottom - rcItem.top;
                        if (rowH > 0) {
                            ListView_Scroll(m_hwnd, 0, delta * rowH);
                        }
                    }
                }
            } else if (focusedRow >= 0) {
                ListView_EnsureVisible(m_hwnd, focusedRow, FALSE);
            } else {
                ListView_EnsureVisible(m_hwnd, 0, FALSE);
            }
        } else if (m_viewMode == 5) {
            // List view (horizontal multi-column scrolling):
            // Reset horizontal scroll to origin, then ensure target/focused row is visible without overshooting
            int hPos = GetScrollPos(m_hwnd, SB_HORZ);
            if (hPos > 0) {
                ListView_Scroll(m_hwnd, -hPos, 0);
            }
            int rowToEnsure = (focusedRow >= 0) ? focusedRow : ((targetTopRow > 0) ? targetTopRow : 0);
            ListView_EnsureVisible(m_hwnd, rowToEnsure, FALSE);
        } else {
            // Icon / Tile / Content views: restore exact pixel scroll position if recorded
            if (state.scrollPosY >= 0) {
                int curY = GetScrollPos(m_hwnd, SB_VERT);
                int dy = state.scrollPosY - curY;
                if (dy != 0) {
                    ListView_Scroll(m_hwnd, 0, dy);
                }
            } else if (focusedRow >= 0) {
                ListView_EnsureVisible(m_hwnd, focusedRow, FALSE);
            } else if (targetTopRow > 0) {
                ListView_EnsureVisible(m_hwnd, targetTopRow, FALSE);
            } else {
                ListView_EnsureVisible(m_hwnd, 0, FALSE);
            }
        }
    }

    if (!m_filteredIndices.empty()) {
        ListView_RedrawItems(m_hwnd, 0, (int)m_filteredIndices.size() - 1);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
    UpdateWindow(m_hwnd);
}

void FileListView::NotifyStatus() {
    if (m_isNavLoading) {
        if (onStatusText) {
            onStatusText(I18n::Get().IsChinese() ? L"正在连接网络共享目录..." : L"Connecting to network share...");
        }
        return;
    }
    if (onStatusUpdated) {
        int selCount = (m_hwnd) ? (int)ListView_GetSelectedCount(m_hwnd) : 0;
        uint64_t totalSize = 0;
        if (selCount > 0) {
            int item = -1;
            while ((item = ListView_GetNextItem(m_hwnd, item, LVNI_SELECTED)) != -1) {
                if (item >= 0 && item < (int)m_filteredIndices.size()) {
                    int idx = m_filteredIndices[item];
                    if (idx >= 0 && idx < (int)m_allItems.size()) {
                        totalSize += m_allItems[idx].size;
                    }
                }
            }
            onStatusUpdated(selCount, totalSize);
        } else {
            for (int idx : m_filteredIndices) {
                if (idx >= 0 && idx < (int)m_allItems.size()) {
                    totalSize += m_allItems[idx].size;
                }
            }
            onStatusUpdated((int)m_filteredIndices.size(), totalSize);
        }
    }
    UpdateHeaderCheckBox();
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
    if (!m_currentPath.empty()) {
        Config::Get().SetFolderSort(m_currentPath, (int)m_sortCol, m_sortAsc);
    }
    SortItems();
    ApplyFilter();
    UpdateHeaderSortArrow();
}

void FileListView::SetFoldersFirst(bool foldersFirst) {
    m_foldersFirst = foldersFirst;
    Config::Get().foldersFirst = foldersFirst;
    if (!m_currentPath.empty()) {
        Config::Get().SetFolderFoldersFirst(m_currentPath, foldersFirst);
    }
    Config::Get().Save();
    SortItems();
    ApplyFilter();
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
        case CMD_NEOMDIE_OPEN_EXPLORER:
            ShellUtils::OpenInExplorer(targetPath);
            break;
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
        case CMD_BG_VIEW_XLARGE:  SetViewMode(1); break;
        case CMD_BG_VIEW_LARGE:   SetViewMode(2); break;
        case CMD_BG_VIEW_MEDIUM:  SetViewMode(3); break;
        case CMD_BG_VIEW_SMALL:   SetViewMode(4); break;
        case CMD_BG_VIEW_LIST:    SetViewMode(5); break;
        case CMD_BG_VIEW_TILES:   SetViewMode(6); break;
        case CMD_BG_VIEW_CONTENT: SetViewMode(7); break;
        case CMD_BG_VIEW_ITEM_CHECKBOXES:
            if (onCustomCommand) {
                onCustomCommand(CMD_BG_VIEW_ITEM_CHECKBOXES, L"");
            }
            break;

        case CMD_BG_SORT_NAME:
            SetSort(SortColumn::Name, m_sortAsc);
            break;
        case CMD_BG_SORT_DATE:
            SetSort(SortColumn::DateModified, m_sortAsc);
            break;
        case CMD_BG_SORT_TYPE:
            SetSort(SortColumn::Type, m_sortAsc);
            break;
        case CMD_BG_SORT_SIZE:
            SetSort(SortColumn::Size, m_sortAsc);
            break;
        case CMD_BG_SORT_ASCENDING:
            SetSort(m_sortCol, true);
            break;
        case CMD_BG_SORT_DESCENDING:
            SetSort(m_sortCol, false);
            break;
        case CMD_BG_SORT_FOLDERS_FIRST:
            SetFoldersFirst(true);
            break;
        case CMD_BG_SORT_FILES_FIRST:
            SetFoldersFirst(false);
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
            if (m_currentPath.empty() || m_currentPath == L"This PC" || m_currentPath == L"此电脑") {
                break;
            }
            std::wstring targetDir = m_currentPath;
            while (targetDir.length() > 3 && (targetDir.back() == L'\\' || targetDir.back() == L'/')) {
                targetDir.pop_back();
            }
            std::wstring args = L"appwiz.cpl,NewLinkHere " + targetDir;
            ShellExecuteW(nullptr, L"open", L"rundll32.exe", args.c_str(), targetDir.c_str(), SW_SHOWNORMAL);
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
            std::wstring baseName = isZh ? L"新建 TXT 文档.txt" : L"New TXT Document.txt";
            std::wstring newFile = Util::CombinePath(m_currentPath, baseName);
            int idx = 2;
            while (PathFileExistsW(newFile.c_str())) {
                newFile = Util::CombinePath(m_currentPath, (isZh ? L"新建 TXT 文档 (" : L"New TXT Document (") + std::to_wstring(idx++) + L").txt");
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
                                              m_viewMode, (int)m_sortCol, m_sortAsc, m_foldersFirst, handleNeoCmd);
    }
}

void FileListView::SelectItemByPath(const std::wstring& path) {
    if (path.empty() || m_filteredIndices.empty()) return;

    ListView_SetItemState(m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

    std::wstring normTarget = Util::NormalizePath(path);
    while (!normTarget.empty() && (normTarget.back() == L'\\' || normTarget.back() == L'/')) {
        normTarget.pop_back();
    }
    std::wstring targetName = PathFindFileNameW(normTarget.c_str());

    int targetRow = -1;
    for (int row = 0; row < (int)m_filteredIndices.size(); ++row) {
        int idx = m_filteredIndices[row];
        if (idx >= 0 && idx < (int)m_allItems.size()) {
            const auto& item = m_allItems[idx];
            std::wstring normItem = Util::NormalizePath(item.fullPath);
            while (!normItem.empty() && (normItem.back() == L'\\' || normItem.back() == L'/')) {
                normItem.pop_back();
            }

            if (_wcsicmp(item.fullPath.c_str(), path.c_str()) == 0 ||
                _wcsicmp(normItem.c_str(), normTarget.c_str()) == 0 ||
                (!targetName.empty() && _wcsicmp(item.name.c_str(), targetName.c_str()) == 0)) {
                targetRow = row;
                break;
            }
        }
    }

    if (targetRow >= 0) {
        ListView_SetItemState(m_hwnd, targetRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        int totalCount = (int)m_filteredIndices.size();
        if (totalCount > 0) {
            ListView_EnsureVisible(m_hwnd, targetRow, FALSE);
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
            NotifyStatus();
            break;
        }
    }
    delete pRes;
}

void FileListView::CancelCustomIconLoader() {
    m_iconNavGeneration++;
    if (m_iconThread.joinable()) {
        m_iconThread.detach();
    }
}

void FileListView::StartCustomIconLoader() {
    CancelCustomIconLoader();

    struct CustomIconTask {
        std::wstring fullPath;
        DWORD attributes;
    };
    std::vector<CustomIconTask> tasks;
    for (const auto& item : m_allItems) {
        if (!item.isFolder) {
            const std::wstring& ext = item.ext;
            if (ext == L".exe" || ext == L".lnk" || ext == L".ico" || ext == L".cur" || ext == L".scr") {
                if (ShellUtils::GetCustomPathIcon(item.fullPath) < 0) {
                    tasks.push_back({ item.fullPath, item.attributes });
                }
            }
        }
    }

    if (tasks.empty()) return;

    int curGen = m_iconNavGeneration.load();
    HWND hwnd = m_hwnd;

    m_iconThread = std::thread([hwnd, curGen, tasks = std::move(tasks), this]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool updatedAny = false;
        DWORD lastNotify = GetTickCount();

        for (size_t i = 0; i < tasks.size(); ++i) {
            if (m_iconNavGeneration.load() != curGen) {
                CoUninitialize();
                return;
            }

            int icon = ShellUtils::GetItemIconIndex(tasks[i].fullPath, tasks[i].attributes, false, false);
            if (icon >= 0) {
                updatedAny = true;
            }

            DWORD now = GetTickCount();
            if (updatedAny && (now - lastNotify >= 60 || i == tasks.size() - 1)) {
                PostMessageW(hwnd, WM_REFRESH_ICONS, (WPARAM)curGen, 0);
                lastNotify = now;
                updatedAny = false;
            }
        }
        CoUninitialize();
    });
}

void FileListView::CancelThumbnailLoader() {
    m_thumbNavGeneration++;
    if (m_thumbThread.joinable()) {
        m_thumbThread.detach();
    }
}

void FileListView::ClearThumbnails() {
    CancelThumbnailLoader();
    std::lock_guard<std::mutex> lock(m_thumbMutex);
    for (auto& pair : m_thumbnailMap) {
        if (pair.second) {
            DeleteObject(pair.second);
        }
    }
    m_thumbnailMap.clear();
    m_dimensionsMap.clear();
}

HBITMAP FileListView::GetThumbnail(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_thumbMutex);
    auto it = m_thumbnailMap.find(path);
    if (it != m_thumbnailMap.end()) {
        return it->second;
    }
    return nullptr;
}

std::wstring FileListView::GetDimensions(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_thumbMutex);
    auto it = m_dimensionsMap.find(path);
    if (it != m_dimensionsMap.end()) {
        return it->second;
    }
    return L"";
}

void FileListView::StartThumbnailLoader() {
    CancelThumbnailLoader();

    // Only load thumbnails for icon, tiles, and content view modes
    if (m_viewMode != 1 && m_viewMode != 2 && m_viewMode != 3 && m_viewMode != 6 && m_viewMode != 7) {
        return;
    }

    struct ThumbTask {
        std::wstring fullPath;
        bool isFolder;
    };
    std::vector<ThumbTask> tasks;
    for (const auto& item : m_allItems) {
        if (item.isDrive || item.isFolder) continue; // Folders must always use system folder icons, never load thumbnails!
        if (ShellUtils::IsVirtualShellPath(item.fullPath)) continue;
        if (ShellUtils::CanHaveThumbnail(item.ext)) {
            // Check if already in cache
            {
                std::lock_guard<std::mutex> lock(m_thumbMutex);
                if (m_thumbnailMap.count(item.fullPath) > 0) continue;
            }
            tasks.push_back({ item.fullPath, false });
        }
    }

    if (tasks.empty()) return;

    int curGen = m_thumbNavGeneration.load();
    HWND hwnd = m_hwnd;
    int targetSize = 256;
    bool isContentView = (m_viewMode == 7);

    m_thumbThread = std::thread([hwnd, curGen, tasks = std::move(tasks), targetSize, isContentView, this]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool updatedAny = false;
        DWORD lastNotify = GetTickCount();

        for (size_t i = 0; i < tasks.size(); ++i) {
            if (m_thumbNavGeneration.load() != curGen) {
                CoUninitialize();
                return;
            }

            HBITMAP hbmp = ShellUtils::GetThumbnail(tasks[i].fullPath, targetSize);
            if (hbmp) {
                std::lock_guard<std::mutex> lock(m_thumbMutex);
                m_thumbnailMap[tasks[i].fullPath] = hbmp;
                updatedAny = true;
            }

            if (isContentView && !tasks[i].isFolder && !ShellUtils::IsVirtualShellPath(tasks[i].fullPath)) {
                IShellItem2* pItem2 = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(tasks[i].fullPath.c_str(), nullptr, IID_PPV_ARGS(&pItem2)))) {
                    LPWSTR pszDim = nullptr;
                    if (SUCCEEDED(pItem2->GetString(PKEY_Image_Dimensions, &pszDim)) && pszDim) {
                        std::wstring cleanDim;
                        for (int k = 0; pszDim[k]; ++k) {
                            if (pszDim[k] >= 0x20 && pszDim[k] < 0x2000) {
                                cleanDim += pszDim[k];
                            }
                        }
                        CoTaskMemFree(pszDim);
                        if (!cleanDim.empty()) {
                            std::lock_guard<std::mutex> lock(m_thumbMutex);
                            m_dimensionsMap[tasks[i].fullPath] = cleanDim;
                            updatedAny = true;
                        }
                    }
                    pItem2->Release();
                }
            }

            DWORD now = GetTickCount();
            if (updatedAny && (now - lastNotify >= 80 || i == tasks.size() - 1)) {
                PostMessageW(hwnd, WM_NOTIFY_THUMBNAIL_BATCH, (WPARAM)curGen, 0);
                lastNotify = now;
                updatedAny = false;
            }
        }
        CoUninitialize();
    });
}

LRESULT CALLBACK FileListView::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;
    if (msg == WM_WINDOWPOSCHANGING && pThis) {
        if (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3) {
            WINDOWPOS* pwp = (WINDOWPOS*)lParam;
            int sel = ListView_GetNextItem(pThis->m_hwnd, -1, LVNI_FOCUSED);
            if (sel >= 0) {
                RECT rcB = { 0 };
                if (ListView_GetItemRect(pThis->m_hwnd, sel, &rcB, LVIR_BOUNDS)) {
                    DWORD sp = (DWORD)SendMessageW(pThis->m_hwnd, LVM_GETITEMSPACING, FALSE, 0);
                    int spX = LOWORD(sp);
                    int cellW = (spX > 0) ? spX : (rcB.right - rcB.left);
                    int iconDim = (pThis->m_viewMode == 1) ? Theme::Get().Scale(256, pThis->m_hwnd)
                                : ((pThis->m_viewMode == 2) ? Theme::Get().Scale(96, pThis->m_hwnd)
                                : Theme::Get().Scale(48, pThis->m_hwnd));
                    int editY = rcB.top + Theme::Get().Scale(6, pThis->m_hwnd) + iconDim + Theme::Get().Scale(4, pThis->m_hwnd);
                    int editX = rcB.left + Theme::Get().Scale(4, pThis->m_hwnd);
                    int editW = cellW - Theme::Get().Scale(8, pThis->m_hwnd);

                    wchar_t text[MAX_PATH] = { 0 };
                    GetWindowTextW(hwnd, text, MAX_PATH);
                    int editH = Theme::Get().Scale(24, pThis->m_hwnd);
                    if (text[0]) {
                        HDC hdc = GetDC(hwnd);
                        HGDIOBJ oldFont = SelectObject(hdc, pThis->m_hFontText);
                        RECT rcCalc = { 0, 0, editW - Theme::Get().Scale(6, pThis->m_hwnd), 0 };
                        DrawTextW(hdc, text, -1, &rcCalc, DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
                        SelectObject(hdc, oldFont);
                        ReleaseDC(hwnd, hdc);
                        int neededH = (rcCalc.bottom - rcCalc.top) + Theme::Get().Scale(10, pThis->m_hwnd);
                        if (neededH > editH) editH = neededH;
                    }

                    pwp->x = editX;
                    pwp->y = editY;
                    pwp->cx = editW;
                    pwp->cy = editH;
                    pwp->flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
                }
            }
        }
    } else if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, FileListView::EditSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

int FileListView::HitTestItem(int x, int y) {
    if (!m_hwnd) return -1;

    if (m_viewMode == 1 || m_viewMode == 2 || m_viewMode == 3) {
        DWORD sp = (DWORD)SendMessageW(m_hwnd, LVM_GETITEMSPACING, FALSE, 0);
        int spX = LOWORD(sp);
        int spY = HIWORD(sp);
        if (spX <= 0 || spY <= 0) return -1;

        RECT rcClient = { 0 };
        GetClientRect(m_hwnd, &rcClient);
        int total = (int)m_filteredIndices.size();
        for (int i = 0; i < total; ++i) {
            RECT rcB = { 0 };
            if (!ListView_GetItemRect(m_hwnd, i, &rcB, LVIR_BOUNDS)) continue;
            if (rcB.top + spY < rcClient.top) continue;
            if (rcB.top > rcClient.bottom) break;
            if (y >= rcB.top && y < rcB.top + spY &&
                x >= rcB.left && x < rcB.left + spX) {
                // (x, y) is inside cell i's tile. Check if it hit the actual icon or label card.
                int iconDim = (m_viewMode == 1) ? Theme::Get().Scale(256, m_hwnd)
                            : ((m_viewMode == 2) ? Theme::Get().Scale(96, m_hwnd)
                            : Theme::Get().Scale(48, m_hwnd));
                int iconX = rcB.left + (spX - iconDim) / 2;
                int iconY = rcB.top + Theme::Get().Scale(6, m_hwnd);
                int margin = Theme::Get().Scale(8, m_hwnd);
                int labelTop = iconY + iconDim + Theme::Get().Scale(4, m_hwnd);

                // 1. Icon area hit test
                if (x >= iconX - margin && x <= iconX + iconDim + margin &&
                    y >= rcB.top + 2 && y <= labelTop) {
                    return i;
                }

                // 2. Label area hit test
                int maxTextHeight = Theme::Get().Scale(50, m_hwnd);
                int textBottom = labelTop + maxTextHeight;
                if (textBottom > rcB.top + spY - 2) textBottom = rcB.top + spY - 2;

                if (y >= labelTop && y <= textBottom) {
                    if (x >= rcB.left + Theme::Get().Scale(2, m_hwnd) &&
                        x <= rcB.left + spX - Theme::Get().Scale(2, m_hwnd)) {
                        return i;
                    }
                }

                // Clicked within cell grid boundaries, but in the blank gap between icons or below label!
                return -1;
            }
        }
        return -1;
    }

    LVHITTESTINFO lvhti = { 0 };
    lvhti.pt = { x, y };
    return ListView_HitTest(m_hwnd, &lvhti);
}

void FileListView::InvalidateItemCell(int itemIndex) {
    if (itemIndex < 0 || !m_hwnd) return;
    RECT rcB = { 0 };
    if (ListView_GetItemRect(m_hwnd, itemIndex, &rcB, LVIR_BOUNDS)) {
        DWORD sp = (DWORD)SendMessageW(m_hwnd, LVM_GETITEMSPACING, FALSE, 0);
        int spX = LOWORD(sp);
        int spY = HIWORD(sp);
        if (spX > 0 && spY > 0) {
            RECT rcCell = { rcB.left - 2, rcB.top - 2, rcB.left + spX + 2, rcB.top + spY + 2 };
            InvalidateRect(m_hwnd, &rcCell, TRUE);
        } else {
            InvalidateRect(m_hwnd, &rcB, TRUE);
        }
    }
}

void FileListView::KillRenameTimer() {
    if (m_hwnd && m_renameTargetItem >= 0) {
        KillTimer(m_hwnd, 705);
        m_renameTargetItem = -1;
    }
}

LRESULT CALLBACK FileListView::ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {
    case WM_SIZE: {
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (pThis && pThis->m_viewMode == 7) {
            RECT rcClient = { 0 };
            GetClientRect(hwnd, &rcClient);
            int clientW = rcClient.right - rcClient.left;
            int tileW = clientW - GetSystemMetrics(SM_CXVSCROLL) - Theme::Get().Scale(16, hwnd);
            if (tileW < Theme::Get().Scale(300, hwnd)) tileW = Theme::Get().Scale(300, hwnd);
            int tileH = Theme::Get().Scale(52, hwnd);
            LVTILEVIEWINFO tvi = { 0 };
            tvi.cbSize = sizeof(LVTILEVIEWINFO);
            tvi.dwMask = LVTVIM_TILESIZE;
            tvi.dwFlags = LVTVIF_FIXEDSIZE;
            tvi.sizeTile.cx = tileW;
            tvi.sizeTile.cy = tileH;
            SendMessageW(hwnd, LVM_SETTILEVIEWINFO, 0, (LPARAM)&tvi);
        }
        return res;
    }
    case WM_PAINT: {
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (pThis && pThis->m_isNavLoading) {
            HDC hdc = GetDC(hwnd);
            if (hdc) {
                RECT rcClient;
                GetClientRect(hwnd, &rcClient);
                HWND hHeader = ListView_GetHeader(hwnd);
                if (hHeader && IsWindowVisible(hHeader)) {
                    RECT rcHdr;
                    GetWindowRect(hHeader, &rcHdr);
                    rcClient.top += (rcHdr.bottom - rcHdr.top);
                }

                SetBkMode(hdc, TRANSPARENT);

                int cardW = Theme::Get().Scale(320, hwnd);
                int cardH = Theme::Get().Scale(80, hwnd);
                int cardX = rcClient.left + (rcClient.right - rcClient.left - cardW) / 2;
                int cardY = rcClient.top + (rcClient.bottom - rcClient.top - cardH) / 2;
                RECT rcCard = { cardX, cardY, cardX + cardW, cardY + cardH };

                // Draw soft rounded card background
                int rad = Theme::Get().Scale(10, hwnd);
                HBRUSH hbrCard = Theme::Get().BrushSurface();
                HPEN hPenBorder = Theme::Get().PenBorder();
                HGDIOBJ oldBrush = SelectObject(hdc, hbrCard);
                HGDIOBJ oldPen = SelectObject(hdc, hPenBorder);
                RoundRect(hdc, rcCard.left, rcCard.top, rcCard.right, rcCard.bottom, rad, rad);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);

                // Draw network sync icon
                RECT rcIcon = rcCard;
                rcIcon.top += Theme::Get().Scale(12, hwnd);
                rcIcon.bottom = rcIcon.top + Theme::Get().Scale(24, hwnd);
                HFONT hIconFont = Theme::Get().GetIconFont(18, hwnd);
                HGDIOBJ oldIconFont = SelectObject(hdc, hIconFont);
                SetTextColor(hdc, Theme::Get().ClrAccent());
                wchar_t iconGlyph[2] = { (wchar_t)0xE898, 0 }; // Fluent Cloud/Network sync glyph
                DrawTextW(hdc, iconGlyph, 1, &rcIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldIconFont);
                DeleteObject(hIconFont);

                // Draw loading prompt text
                RECT rcText = rcCard;
                rcText.top = rcIcon.bottom + Theme::Get().Scale(4, hwnd);
                rcText.bottom = rcCard.bottom - Theme::Get().Scale(8, hwnd);
                HFONT hFont = Theme::Get().GetUIFont(hwnd);
                HGDIOBJ oldFont = SelectObject(hdc, hFont);
                SetTextColor(hdc, Theme::Get().ClrTextPrimary());
                std::wstring prompt = I18n::Get().IsChinese()
                    ? L"正在连接网络共享目录，请稍候..."
                    : L"Connecting to network share, please wait...";
                DrawTextW(hdc, prompt.c_str(), -1, &rcText, DT_CENTER | DT_TOP | DT_SINGLELINE);
                SelectObject(hdc, oldFont);

                ReleaseDC(hwnd, hdc);
            }
        }
        return res;
    }

    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (pnm && (pnm->code == HDN_ENDTRACKW || pnm->code == HDN_ENDTRACKA ||
                    pnm->code == HDN_ITEMCHANGEDW || pnm->code == HDN_ITEMCHANGEDA)) {
            if (pThis) {
                pThis->SaveColumnWidths();
            }
        }
        return res;
    }

    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;

    case WM_SETFOCUS: {
        int topIdx = ListView_GetTopIndex(hwnd);
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        int newTopIdx = ListView_GetTopIndex(hwnd);
        if (newTopIdx != topIdx && topIdx > 0) {
            RECT rcItem = { 0 };
            int itemH = 0;
            if (ListView_GetItemRect(hwnd, 0, &rcItem, LVIR_BOUNDS)) {
                itemH = rcItem.bottom - rcItem.top;
            }
            if (itemH <= 0) {
                itemH = Theme::Get().Scale(22, hwnd);
            }
            int dy = (topIdx - newTopIdx) * itemH;
            ListView_Scroll(hwnd, 0, dy);
        }
        return res;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (pThis && (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3)) {
            pThis->KillRenameTimer();

            int hit = pThis->HitTestItem(x, y);
            if (hit < 0) {
                bool ctrl = (GetKeyState(VK_CONTROL) < 0);
                bool shift = (GetKeyState(VK_SHIFT) < 0);
                if (!ctrl && !shift) {
                    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                SetFocus(hwnd);
                return 0;
            }

            SetFocus(hwnd);
            bool ctrl = (GetKeyState(VK_CONTROL) < 0);
            bool shift = (GetKeyState(VK_SHIFT) < 0);

            if (!ctrl && !shift) {
                bool isAlreadySel = (ListView_GetItemState(hwnd, hit, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                bool isAlreadyFoc = (ListView_GetItemState(hwnd, hit, LVIS_FOCUSED) & LVIS_FOCUSED) != 0;
                int selCount = ListView_GetSelectedCount(hwnd);

                if (isAlreadySel && isAlreadyFoc && selCount == 1) {
                    pThis->m_renameTargetItem = hit;
                    SetTimer(hwnd, 705, GetDoubleClickTime() + 60, nullptr);
                    return 0;
                }

                int curSel = -1;
                while ((curSel = ListView_GetNextItem(hwnd, curSel, LVNI_SELECTED)) != -1) {
                    if (curSel != hit) {
                        ListView_SetItemState(hwnd, curSel, 0, LVIS_SELECTED | LVIS_FOCUSED);
                        pThis->InvalidateItemCell(curSel);
                    }
                }
                ListView_SetItemState(hwnd, hit, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                pThis->InvalidateItemCell(hit);

                if (pThis->onSelectionChanged && hit < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[hit];
                    pThis->onSelectionChanged(pThis->m_allItems[idx].fullPath);
                }
                pThis->NotifyStatus();
                return 0;
            } else if (ctrl) {
                bool isSel = (ListView_GetItemState(hwnd, hit, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                ListView_SetItemState(hwnd, hit, isSel ? 0 : LVIS_SELECTED, LVIS_SELECTED);
                ListView_SetItemState(hwnd, hit, LVIS_FOCUSED, LVIS_FOCUSED);
                pThis->InvalidateItemCell(hit);
                pThis->NotifyStatus();
                return 0;
            } else if (shift) {
                int curFoc = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
                if (curFoc < 0) curFoc = 0;
                int start = (std::min)(curFoc, hit);
                int end = (std::max)(curFoc, hit);
                ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                for (int k = start; k <= end; ++k) {
                    ListView_SetItemState(hwnd, k, LVIS_SELECTED, LVIS_SELECTED);
                    pThis->InvalidateItemCell(k);
                }
                ListView_SetItemState(hwnd, hit, LVIS_FOCUSED, LVIS_FOCUSED);
                pThis->NotifyStatus();
                return 0;
            }
        }

        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = { x, y };
        int hit = ListView_HitTest(hwnd, &lvhti);

        auto isPointInBlankArea = [&](int px, int itemHit) -> bool {
            if (itemHit < 0 || !pThis) return true;
            if (pThis->m_viewMode == 7) { // Content view (内容)
                RECT rcItem = { 0 };
                ListView_GetItemRect(hwnd, itemHit, &rcItem, LVIR_BOUNDS);
                if (itemHit >= 0 && itemHit < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[itemHit];
                    if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                        const auto& item = pThis->m_allItems[idx];
                        int itemW = rcItem.right - rcItem.left;
                        int iconX = rcItem.left + Theme::Get().Scale(16, hwnd);
                        int col1Left = iconX + Theme::Get().Scale(32 + 12, hwnd);
                        int col2Left = rcItem.left + itemW * 40 / 100;
                        if (col2Left < col1Left + Theme::Get().Scale(140, hwnd)) {
                            col2Left = col1Left + Theme::Get().Scale(140, hwnd);
                        }
                        int col3Left = rcItem.left + itemW * 75 / 100;
                        if (col3Left < col2Left + Theme::Get().Scale(160, hwnd)) {
                            col3Left = col2Left + Theme::Get().Scale(160, hwnd);
                        }

                        HDC hdc = GetDC(hwnd);
                        HGDIOBJ oldFont = SelectObject(hdc, pThis->m_hFontText);
                        SIZE szName = { 0 }, szCol2 = { 0 }, szCol3 = { 0 };
                        GetTextExtentPoint32W(hdc, item.name.c_str(), (int)item.name.length(), &szName);
                        std::wstring dateStr = Util::FormatFileTime(item.modifiedTime);
                        std::wstring sizeStr = item.isFolder ? L"" : Util::FormatBytes(item.size);
                        std::wstring dimStr = item.isFolder ? L"" : pThis->GetDimensions(item.fullPath);
                        std::wstring typeStr = item.typeName.empty() ? (item.ext.empty() ? L"文件" : item.ext + L" 文件") : item.typeName;
                        std::wstring col2Text = item.isFolder ? L"" : (L"类型: " + typeStr);
                        GetTextExtentPoint32W(hdc, col2Text.c_str(), (int)col2Text.length(), &szCol2);
                        if (!dimStr.empty()) {
                            SIZE szDim = { 0 };
                            std::wstring dimText = L"分辨率: " + dimStr;
                            GetTextExtentPoint32W(hdc, dimText.c_str(), (int)dimText.length(), &szDim);
                            if (szDim.cx > szCol2.cx) szCol2.cx = szDim.cx;
                        }
                        std::wstring col3Line1 = (item.isFolder || dimStr.empty()) ? (dateStr.empty() ? L"" : (L"修改日期: " + dateStr)) : L"";
                        std::wstring col3Line2 = sizeStr.empty() ? L"" : (L"大小: " + sizeStr);
                        GetTextExtentPoint32W(hdc, col3Line1.c_str(), (int)col3Line1.length(), &szCol3);
                        SIZE szCol3L2 = { 0 };
                        GetTextExtentPoint32W(hdc, col3Line2.c_str(), (int)col3Line2.length(), &szCol3L2);
                        if (szCol3L2.cx > szCol3.cx) szCol3.cx = szCol3L2.cx;
                        SelectObject(hdc, oldFont);
                        ReleaseDC(hwnd, hdc);

                        int col1TextRight = col1Left + szName.cx;
                        int col2TextRight = col2Left + szCol2.cx;
                        int col3TextRight = col3Left + szCol3.cx;

                        if (px > col3TextRight + Theme::Get().Scale(16, hwnd)) return true;
                        if (item.isFolder) {
                            if (px > col1TextRight + Theme::Get().Scale(16, hwnd) && px < col3Left) return true;
                        } else {
                            if (px > col1TextRight + Theme::Get().Scale(16, hwnd) && px < col2Left) return true;
                            if (px > col2TextRight + Theme::Get().Scale(16, hwnd) && px < col3Left) return true;
                        }
                        return false;
                    }
                }
                return true;
            } else if (pThis->m_viewMode == 0) { // Details view (详细信息)
                HWND hHeader = ListView_GetHeader(hwnd);
                if (hHeader) {
                    int totalW = 0;
                    int colCnt = Header_GetItemCount(hHeader);
                    for (int i = 0; i < colCnt; ++i) {
                        RECT rcCol = { 0 };
                        Header_GetItemRect(hHeader, i, &rcCol);
                        if (rcCol.right > totalW) totalW = rcCol.right;
                    }
                    if (px > totalW) {
                        return true;
                    }
                }
            }
            return false;
        };

        bool isBlank = (hit < 0) || isPointInBlankArea(x, hit);
        if (isBlank) {
            bool ctrl = (GetKeyState(VK_CONTROL) < 0);
            bool shift = (GetKeyState(VK_SHIFT) < 0);
            if (!ctrl && !shift) {
                ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
            }
            SetFocus(hwnd);
            return 0;
        }

        if (Config::Get().showItemCheckBoxes && (lvhti.flags & LVHT_ONITEMSTATEICON) && hit >= 0) {
            SetFocus(hwnd);
            bool isSel = (ListView_GetItemState(hwnd, hit, LVIS_SELECTED) & LVIS_SELECTED) != 0;
            ListView_SetItemState(hwnd, hit, isSel ? 0 : (LVIS_SELECTED | LVIS_FOCUSED), LVIS_SELECTED | LVIS_FOCUSED);
            ListView_RedrawItems(hwnd, hit, hit);
            if (pThis) {
                pThis->NotifyStatus();
                pThis->UpdateHeaderCheckBox();
                if (pThis->onSelectionChanged && hit < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[hit];
                    pThis->onSelectionChanged(pThis->m_allItems[idx].fullPath);
                }
            }
            return 0;
        }

        bool wasFocused = (GetFocus() == hwnd);
        if (!wasFocused) {
            SetFocus(hwnd);
            pThis->m_preventRenameOnActivate = true;

            bool ctrl = (GetKeyState(VK_CONTROL) < 0);
            bool shift = (GetKeyState(VK_SHIFT) < 0);

            if (hit >= 0) {
                if (!ctrl && !shift) {
                    int curFocus = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
                    if (curFocus >= 0 && curFocus != hit) {
                        ListView_SetItemState(hwnd, curFocus, 0, LVIS_FOCUSED);
                    }
                    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                    ListView_SetItemState(hwnd, hit, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                }
                if (pThis->onSelectionChanged && hit < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[hit];
                    pThis->onSelectionChanged(pThis->m_allItems[idx].fullPath);
                }
            } else {
                if (!ctrl && !shift) {
                    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                }
            }
        }
        break; // Let native SysListView32 handle selection/drag/F2
    }
    case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (pThis) {
            pThis->KillRenameTimer();
            int hit = pThis->HitTestItem(x, y);
            if (hit >= 0) {
                pThis->OnItemDoubleClicked(hit);
                return 0;
            } else {
                pThis->GoUp();
                return 0;
            }
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        if (pThis) pThis->KillRenameTimer();
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Select item under cursor if not selected
        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = { x, y };
        int hit = pThis ? pThis->HitTestItem(x, y) : ListView_HitTest(hwnd, &lvhti);

        auto isPointInBlankArea = [&](int px, int itemHit) -> bool {
            if (itemHit < 0 || !pThis) return true;
            if (pThis->m_viewMode == 7) { // Content view (内容)
                RECT rcItem = { 0 };
                ListView_GetItemRect(hwnd, itemHit, &rcItem, LVIR_BOUNDS);
                if (itemHit >= 0 && itemHit < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[itemHit];
                    if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                        const auto& item = pThis->m_allItems[idx];
                        int itemW = rcItem.right - rcItem.left;
                        int iconX = rcItem.left + Theme::Get().Scale(16, hwnd);
                        int col1Left = iconX + Theme::Get().Scale(32 + 12, hwnd);
                        int col2Left = rcItem.left + itemW * 40 / 100;
                        if (col2Left < col1Left + Theme::Get().Scale(140, hwnd)) {
                            col2Left = col1Left + Theme::Get().Scale(140, hwnd);
                        }
                        int col3Left = rcItem.left + itemW * 75 / 100;
                        if (col3Left < col2Left + Theme::Get().Scale(160, hwnd)) {
                            col3Left = col2Left + Theme::Get().Scale(160, hwnd);
                        }

                        HDC hdc = GetDC(hwnd);
                        HGDIOBJ oldFont = SelectObject(hdc, pThis->m_hFontText);
                        SIZE szName = { 0 }, szCol2 = { 0 }, szCol3 = { 0 };
                        GetTextExtentPoint32W(hdc, item.name.c_str(), (int)item.name.length(), &szName);
                        std::wstring dateStr = Util::FormatFileTime(item.modifiedTime);
                        std::wstring sizeStr = item.isFolder ? L"" : Util::FormatBytes(item.size);
                        std::wstring dimStr = item.isFolder ? L"" : pThis->GetDimensions(item.fullPath);
                        std::wstring typeStr = item.typeName.empty() ? (item.ext.empty() ? L"文件" : item.ext + L" 文件") : item.typeName;
                        std::wstring col2Text = item.isFolder ? L"" : (L"类型: " + typeStr);
                        GetTextExtentPoint32W(hdc, col2Text.c_str(), (int)col2Text.length(), &szCol2);
                        if (!dimStr.empty()) {
                            SIZE szDim = { 0 };
                            std::wstring dimText = L"分辨率: " + dimStr;
                            GetTextExtentPoint32W(hdc, dimText.c_str(), (int)dimText.length(), &szDim);
                            if (szDim.cx > szCol2.cx) szCol2.cx = szDim.cx;
                        }
                        std::wstring col3Line1 = (item.isFolder || dimStr.empty()) ? (dateStr.empty() ? L"" : (L"修改日期: " + dateStr)) : L"";
                        std::wstring col3Line2 = sizeStr.empty() ? L"" : (L"大小: " + sizeStr);
                        GetTextExtentPoint32W(hdc, col3Line1.c_str(), (int)col3Line1.length(), &szCol3);
                        SIZE szCol3L2 = { 0 };
                        GetTextExtentPoint32W(hdc, col3Line2.c_str(), (int)col3Line2.length(), &szCol3L2);
                        if (szCol3L2.cx > szCol3.cx) szCol3.cx = szCol3L2.cx;
                        SelectObject(hdc, oldFont);
                        ReleaseDC(hwnd, hdc);

                        int col1TextRight = col1Left + szName.cx;
                        int col2TextRight = col2Left + szCol2.cx;
                        int col3TextRight = col3Left + szCol3.cx;

                        if (px > col3TextRight + Theme::Get().Scale(16, hwnd)) return true;
                        if (item.isFolder) {
                            if (px > col1TextRight + Theme::Get().Scale(16, hwnd) && px < col3Left) return true;
                        } else {
                            if (px > col1TextRight + Theme::Get().Scale(16, hwnd) && px < col2Left) return true;
                            if (px > col2TextRight + Theme::Get().Scale(16, hwnd) && px < col3Left) return true;
                        }
                        return false;
                    }
                }
                return true;
            } else if (pThis->m_viewMode == 0) { // Details view (详细信息)
                HWND hHeader = ListView_GetHeader(hwnd);
                if (hHeader) {
                    int totalW = 0;
                    int colCnt = Header_GetItemCount(hHeader);
                    for (int i = 0; i < colCnt; ++i) {
                        RECT rcCol = { 0 };
                        Header_GetItemRect(hHeader, i, &rcCol);
                        if (rcCol.right > totalW) totalW = rcCol.right;
                    }
                    if (px > totalW) {
                        return true;
                    }
                }
            }
            return false;
        };

        bool isBlank = (hit < 0) || isPointInBlankArea(x, hit);

        if (hit >= 0 && !isBlank) {
            UINT state = ListView_GetItemState(hwnd, hit, LVIS_SELECTED);
            if (!(state & LVIS_SELECTED)) {
                int curFocus = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
                if (curFocus >= 0 && curFocus != hit) {
                    ListView_SetItemState(hwnd, curFocus, 0, LVIS_FOCUSED);
                }
                ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
                ListView_SetItemState(hwnd, hit, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
        } else {
            ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED);
        }

        if (GetFocus() != hwnd) {
            SetFocus(hwnd);
        }

        pThis->m_gesture.OnRButtonDown(hwnd, x, y);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (pThis && pThis->m_renameTargetItem >= 0 && (wParam & MK_LBUTTON)) {
            pThis->KillRenameTimer();
        }

        bool rDown = (wParam & MK_RBUTTON) != 0;
        if (pThis && pThis->m_gesture.OnMouseMove(hwnd, x, y, rDown)) {
            return 0;
        }

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);

        if (pThis && (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3)) {
            LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
            int hit = pThis->HitTestItem(x, y);
            if (hit != pThis->m_hoverItem) {
                int oldHover = pThis->m_hoverItem;
                pThis->m_hoverItem = hit;
                if (oldHover >= 0) pThis->InvalidateItemCell(oldHover);
                if (hit >= 0) pThis->InvalidateItemCell(hit);
            }
            return res;
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (pThis && (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3)) {
            if (pThis->m_hoverItem >= 0) {
                int oldHover = pThis->m_hoverItem;
                pThis->m_hoverItem = -1;
                pThis->InvalidateItemCell(oldHover);
            }
        }
        break;
    }
    case WM_MBUTTONUP: {
        if (pThis) pThis->KillRenameTimer();
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int hitIndex = pThis ? pThis->HitTestItem(x, y) : -1;
        if (hitIndex >= 0) {
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

        if (wParam == VK_SPACE && Config::Get().showItemCheckBoxes && !ctrl && !shift && !alt) {
            int focus = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
            if (focus >= 0) {
                bool isSel = (ListView_GetItemState(hwnd, focus, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                ListView_SetItemState(hwnd, focus, isSel ? 0 : LVIS_SELECTED, LVIS_SELECTED);
                ListView_RedrawItems(hwnd, focus, focus);
                if (pThis) {
                    pThis->NotifyStatus();
                    pThis->UpdateHeaderCheckBox();
                }
                return 0;
            }
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

        if (wParam == VK_F7 && !ctrl && !shift && !alt) {
            HWND hTop = GetAncestor(hwnd, GA_ROOT);
            SendMessageW(hTop, msg, wParam, lParam);
            return 0;
        }

        if ((wParam == 'E' || wParam == 'e') && ctrl && alt && !shift) {
            HWND hTop = GetAncestor(hwnd, GA_ROOT);
            SendMessageW(hTop, msg, wParam, lParam);
            return 0;
        }

        if (ctrl && (wParam == 'A' || wParam == 'a')) {
            pThis->SelectAll();
            return 0;
        } else if (ctrl && (wParam == 'I' || wParam == 'i')) {
            pThis->InvertSelection();
            return 0;
        } else if (ctrl && (wParam == 'Z' || wParam == 'z')) {
            HWND hTop = GetAncestor(hwnd, GA_ROOT);
            SendMessageW(hTop, WM_COMMAND, ID_EDIT_UNDO, 0);
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
        } else if (wParam == VK_ESCAPE) {
            if (ShellUtils::HasCutFiles()) {
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    CloseClipboard();
                }
                return 0;
            }
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
        if (pThis && (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3)) {
            if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT ||
                wParam == VK_HOME || wParam == VK_END || wParam == VK_PRIOR || wParam == VK_NEXT) {
                int oldFocus = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
                LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
                int newFocus = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
                if (oldFocus >= 0 && oldFocus != newFocus) pThis->InvalidateItemCell(oldFocus);
                if (newFocus >= 0) pThis->InvalidateItemCell(newFocus);
                return res;
            }
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == 705) {
            KillTimer(hwnd, 705);
            if (pThis && pThis->m_renameTargetItem >= 0) {
                int itemToRename = pThis->m_renameTargetItem;
                pThis->m_renameTargetItem = -1;
                ListView_EditLabel(pThis->m_hwnd, itemToRename);
            }
            return 0;
        }
        if (wParam == 701 || wParam == 702 || wParam == 703) {
            KillTimer(hwnd, wParam);
            pThis->Refresh();
            return 0;
        }
        break;
    }
    case FileListView::WM_NOTIFY_DIR_CHANGED:
        // Debounce directory change notifications so rapid file operations (copying/extracting)
        // do not trigger repeated Refresh() storm
        KillTimer(hwnd, 703);
        SetTimer(hwnd, 703, 200, nullptr);
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
    case FileListView::WM_ASYNC_NAV_COMPLETE: {
        auto* pResult = (FileListView::AsyncNavResult*)lParam;
        if (pResult) {
            if (pResult->generation == pThis->m_navGeneration && pResult->path == pThis->m_currentPath) {
                pThis->m_isNavLoading = false;
                InvalidateRect(pThis->m_hwnd, nullptr, TRUE);
                if (pResult->success) {
                    pThis->m_allItems = std::move(pResult->items);
                    pThis->SortItems();
                    pThis->UpdateHeaderSortArrow();
                    ListView_SetItemCountEx(pThis->m_hwnd, 0, 0);
                    ListView_SetItemState(pThis->m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    pThis->ApplyFilter();
                    if (pThis->m_hasPendingRestore) {
                        pThis->RestoreViewState(pThis->m_pendingRestoreState);
                        pThis->m_hasPendingRestore = false;
                    }
                    if (!pThis->m_pendingSelectPath.empty()) {
                        std::wstring target = pThis->m_pendingSelectPath;
                        pThis->m_pendingSelectPath.clear();
                        pThis->SelectItemByPath(target);
                    }
                    pThis->NotifyStatus();
                    pThis->m_watcher.Watch(pThis->m_currentPath, pThis->m_hwnd, WM_NOTIFY_DIR_CHANGED);
                    if (pThis->m_calcFolderSize && pThis->m_currentPath != L"此电脑" && pThis->m_currentPath != L"This PC") {
                        for (const auto& item : pThis->m_allItems) {
                            if (item.isFolder && !item.isDrive) {
                                pThis->m_calcSize.RequestFolderSize(item.fullPath, pThis->m_hwnd, WM_NOTIFY_FOLDER_SIZE);
                            }
                        }
                    }
                    pThis->StartCustomIconLoader();
                    pThis->StartThumbnailLoader();
                } else {
                    pThis->m_allItems.clear();
                    pThis->ApplyFilter();
                    if (pThis->onStatusText) {
                        pThis->onStatusText(I18n::Get().IsChinese() ? L"无法访问网络共享路径" : L"Cannot access network share");
                    }
                }
            }
            delete pResult;
        }
        return 0;
    }
    case FileListView::WM_REFRESH_ICONS: {
        int gen = (int)wParam;
        if (gen == pThis->m_iconNavGeneration.load()) {
            InvalidateRect(pThis->m_hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case FileListView::WM_NOTIFY_THUMBNAIL_BATCH: {
        int gen = (int)wParam;
        if (gen == pThis->m_thumbNavGeneration.load()) {
            InvalidateRect(pThis->m_hwnd, nullptr, FALSE);
        }
        return 0;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK FileListView::ParentSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FileListView* pThis = (FileListView*)dwRefData;

    switch (msg) {
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
        if (pmis && pmis->CtlType == ODT_LISTVIEW && pmis->CtlID == (UINT)pThis->m_controlId) {
            pmis->itemHeight = pThis->GetDesiredRowHeight();
            return TRUE;
        }
        break;
    }
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->hwndFrom == pThis->m_hwnd) {
            switch (pnm->code) {
            case LVN_BEGINDRAG: {
                pThis->m_preventRenameOnActivate = false;
                NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
                pThis->StartDragDrop(pnmv->iItem);
                return 0;
            }
            case LVN_GETDISPINFOW: {
                NMLVDISPINFOW* pDisp = (NMLVDISPINFOW*)lParam;
                int row = pDisp->item.iItem;
                if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[row];
                    const auto& item = pThis->m_allItems[idx];

                    if (pDisp->item.mask & LVIF_TEXT) {
                        static wchar_t s_buf[8][MAX_PATH * 2];
                        static size_t s_bufIdx = 0;
                        wchar_t* buf = s_buf[(s_bufIdx++) & 7];
                        buf[0] = L'\0';

                        if (pThis->m_viewMode == 7) { // Content view (内容)
                            if (pDisp->item.iSubItem == 0) {
                                StringCchCopyW(buf, MAX_PATH * 2, item.name.c_str());
                            } else {
                                buf[0] = L'\0';
                            }
                        } else if (pThis->m_viewMode == 6) { // Tiles view (平铺)
                            if (pDisp->item.iSubItem == 0) {
                                StringCchCopyW(buf, MAX_PATH * 2, item.name.c_str());
                            } else if (pDisp->item.iSubItem == 1) {
                                // Line 2: Type
                                StringCchCopyW(buf, MAX_PATH * 2, item.typeName.c_str());
                            } else if (pDisp->item.iSubItem == 2) {
                                // Line 3: Size
                                if (item.isDrive) {
                                    uint64_t freeBytes = 0, totalBytes = 0;
                                    if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                        StringCchPrintfW(buf, MAX_PATH * 2, L"%s 可用 / 共 %s",
                                                         Util::FormatBytes(freeBytes).c_str(),
                                                         Util::FormatBytes(totalBytes).c_str());
                                    }
                                } else if (item.isFolder && !item.isSizeCalculated) {
                                    buf[0] = L'\0';
                                } else {
                                    StringCchCopyW(buf, MAX_PATH * 2, Util::FormatBytes(item.size).c_str());
                                }
                            } else {
                                StringCchCopyW(buf, MAX_PATH * 2, item.name.c_str());
                            }
                        } else {
                            int colIdx = pDisp->item.iSubItem;
                            if (colIdx >= 0 && colIdx < (int)pThis->m_activeColumns.size()) {
                                ColumnId colId = pThis->m_activeColumns[colIdx];
                                switch (colId) {
                                case ColumnId::Name:
                                    StringCchCopyW(buf, MAX_PATH * 2, item.name.c_str());
                                    break;
                                case ColumnId::DateModified:
                                    StringCchCopyW(buf, MAX_PATH * 2, Util::FormatFileTime(item.modifiedTime).c_str());
                                    break;
                                case ColumnId::Type:
                                    StringCchCopyW(buf, MAX_PATH * 2, item.typeName.c_str());
                                    break;
                                case ColumnId::Size:
                                    if (item.isDrive) {
                                        uint64_t freeBytes = 0, totalBytes = 0;
                                        if (ShellUtils::GetDriveSpace(item.fullPath, freeBytes, totalBytes) && totalBytes > 0) {
                                            StringCchPrintfW(buf, MAX_PATH * 2, L"%s 可用 / 共 %s",
                                                             Util::FormatBytes(freeBytes).c_str(),
                                                             Util::FormatBytes(totalBytes).c_str());
                                        }
                                    } else if (item.isFolder && !item.isSizeCalculated) {
                                        buf[0] = L'\0';
                                    } else {
                                        StringCchCopyW(buf, MAX_PATH * 2, Util::FormatBytes(item.size).c_str());
                                    }
                                    break;
                                case ColumnId::DateCreated:
                                    StringCchCopyW(buf, MAX_PATH * 2, Util::FormatFileTime(item.createdTime).c_str());
                                    break;
                                case ColumnId::Attributes:
                                    StringCchCopyW(buf, MAX_PATH * 2, Util::FormatAttributes(item.attributes).c_str());
                                    break;
                                case ColumnId::Authors:
                                    StringCchCopyW(buf, MAX_PATH * 2, item.authors.c_str());
                                    break;
                                case ColumnId::Title:
                                    StringCchCopyW(buf, MAX_PATH * 2, item.title.c_str());
                                    break;
                                case ColumnId::Tags:
                                    StringCchCopyW(buf, MAX_PATH * 2, item.tags.c_str());
                                    break;
                                case ColumnId::Path:
                                    StringCchCopyW(buf, MAX_PATH * 2, item.fullPath.c_str());
                                    break;
                                }
                            }
                        }
                        pDisp->item.pszText = buf;
                    }

                    if (pDisp->item.mask & LVIF_IMAGE) {
                        int icon = -1;
                        if (pDisp->item.iSubItem == 0) {
                            icon = ShellUtils::GetCustomPathIcon(item.fullPath);
                            if (icon < 0) {
                                icon = item.iconIndex;
                            }
                        }
                        pDisp->item.iImage = icon;
                    }
                    if (pDisp->item.mask & LVIF_COLUMNS) {
                        if (pDisp->item.puColumns) {
                            if (pThis->m_viewMode == 7) {
                                pDisp->item.cColumns = 0;
                            } else if (pThis->m_viewMode == 6) {
                                if (pDisp->item.cColumns >= 2) {
                                    pDisp->item.puColumns[0] = 1;
                                    pDisp->item.puColumns[1] = 2;
                                    pDisp->item.cColumns = 2;
                                } else if (pDisp->item.cColumns >= 1) {
                                    pDisp->item.puColumns[0] = 1;
                                    pDisp->item.cColumns = 1;
                                }
                            } else if (pThis->m_viewMode == 4) {
                                pDisp->item.cColumns = 0;
                            }
                        }
                    }
                    if (Config::Get().showItemCheckBoxes && (pDisp->item.mask & LVIF_STATE)) {
                        pDisp->item.stateMask |= LVIS_STATEIMAGEMASK;
                        bool isSel = (ListView_GetItemState(pThis->m_hwnd, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                        pDisp->item.state = INDEXTOSTATEIMAGEMASK(isSel ? 2 : 1);
                    }
                }
                return 0;
            }

            case LVN_BEGINLABELEDITW: {
                if (pThis->m_preventRenameOnActivate) {
                    pThis->m_preventRenameOnActivate = false;
                    return TRUE; // Cancel label edit
                }
                HWND hEdit = ListView_GetEditControl(pThis->m_hwnd);
                if (hEdit) {
                    SetWindowSubclass(hEdit, FileListView::EditSubclassProc, 1, (DWORD_PTR)pThis);
                    SendMessageW(hEdit, WM_SETFONT, (WPARAM)pThis->m_hFontText, TRUE);

                    if (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3) {
                        int sel = ListView_GetNextItem(pThis->m_hwnd, -1, LVNI_FOCUSED);
                        if (sel >= 0) {
                            RECT rcB = { 0 };
                            if (ListView_GetItemRect(pThis->m_hwnd, sel, &rcB, LVIR_BOUNDS)) {
                                DWORD sp = (DWORD)SendMessageW(pThis->m_hwnd, LVM_GETITEMSPACING, FALSE, 0);
                                int spX = LOWORD(sp);
                                int cellW = (spX > 0) ? spX : (rcB.right - rcB.left);
                                int iconDim = (pThis->m_viewMode == 1) ? Theme::Get().Scale(256, pThis->m_hwnd)
                                            : ((pThis->m_viewMode == 2) ? Theme::Get().Scale(96, pThis->m_hwnd)
                                            : Theme::Get().Scale(48, pThis->m_hwnd));
                                int editY = rcB.top + Theme::Get().Scale(6, pThis->m_hwnd) + iconDim + Theme::Get().Scale(4, pThis->m_hwnd);
                                int editX = rcB.left + Theme::Get().Scale(4, pThis->m_hwnd);
                                int editW = cellW - Theme::Get().Scale(8, pThis->m_hwnd);
                                int editH = Theme::Get().Scale(24, pThis->m_hwnd);
                                SetWindowPos(hEdit, nullptr, editX, editY, editW, editH, SWP_NOZORDER | SWP_SHOWWINDOW);
                            }
                        }
                    }

                    int selRow = ListView_GetNextItem(pThis->m_hwnd, -1, LVNI_FOCUSED);
                    bool isFolder = false;
                    if (selRow >= 0 && selRow < (int)pThis->m_filteredIndices.size()) {
                        int idx = pThis->m_filteredIndices[selRow];
                        if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                            isFolder = pThis->m_allItems[idx].isFolder;
                        }
                    }
                    if (isFolder) {
                        SendMessageW(hEdit, EM_SETSEL, 0, -1);
                    } else {
                        wchar_t text[MAX_PATH];
                        GetWindowTextW(hEdit, text, MAX_PATH);
                        const wchar_t* pExt = PathFindExtensionW(text);
                        int extPos = pExt ? (int)(pExt - text) : (int)wcslen(text);
                        // Select filename only, without extension
                        SendMessageW(hEdit, EM_SETSEL, 0, extPos);
                    }
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

            case LVN_GETINFOTIPW: {
                NMLVGETINFOTIPW* pGit = (NMLVGETINFOTIPW*)lParam;
                if (pGit && pGit->iItem >= 0 && pGit->iItem < (int)pThis->m_filteredIndices.size()) {
                    int idx = pThis->m_filteredIndices[pGit->iItem];
                    if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                        const auto& item = pThis->m_allItems[idx];
                        std::wstring tip = ShellUtils::GetItemInfoTip(item.fullPath, item.isFolder, item.size, item.modifiedTime, item.typeName, item.isSizeCalculated);
                        if (!tip.empty()) {
                            wcsncpy_s(pGit->pszText, pGit->cchTextMax, tip.c_str(), _TRUNCATE);
                        }
                    }
                }
                return 0;
            }

            case NM_CUSTOMDRAW: {
                NMLVCUSTOMDRAW* pcd = (NMLVCUSTOMDRAW*)lParam;
                switch (pcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    if (pThis->m_viewMode == 7) { // Content view (内容) - complete custom drawing matching Explorer
                        int row = (int)pcd->nmcd.dwItemSpec;
                        if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                            int idx = pThis->m_filteredIndices[row];
                            if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                                const auto& item = pThis->m_allItems[idx];
                                HDC hdc = pcd->nmcd.hdc;
                                RECT rcItem = { 0 };
                                ListView_GetItemRect(pThis->m_hwnd, row, &rcItem, LVIR_BOUNDS);
                                bool isSelected = (ListView_GetItemState(pThis->m_hwnd, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                                bool isHot = (row == pThis->m_hoverItem) || (pcd->nmcd.uItemState & CDIS_HOT) != 0 || (ListView_GetItemState(pThis->m_hwnd, row, LVIS_DROPHILITED) & LVIS_DROPHILITED) != 0;

                                // 1. Item background (hover, selected, or default background)
                                HBRUSH hbrBg = isSelected ? Theme::Get().BrushSelected() : (isHot ? Theme::Get().BrushHover() : Theme::Get().BrushBackground());
                                FillRect(hdc, &rcItem, hbrBg);

                                // 2. Subtle horizontal divider line across the entire row
                                COLORREF gridClr = Theme::Get().IsDarkMode() ? RGB(45, 47, 52) : RGB(235, 238, 242);
                                HPEN hPen = CreatePen(PS_SOLID, 1, gridClr);
                                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                                MoveToEx(hdc, rcItem.left, rcItem.bottom - 1, nullptr);
                                LineTo(hdc, rcItem.right, rcItem.bottom - 1);
                                SelectObject(hdc, hOldPen);
                                DeleteObject(hPen);

                                // 3. Icon / Thumbnail (32x32)
                                int iconSize = Theme::Get().Scale(32, pThis->m_hwnd);
                                int iconX = rcItem.left + Theme::Get().Scale(16, pThis->m_hwnd);
                                int iconY = rcItem.top + (rcItem.bottom - rcItem.top - iconSize) / 2;
                                RECT rcIcon = { iconX, iconY, iconX + iconSize, iconY + iconSize };

                                HBITMAP hThumb = pThis->GetThumbnail(item.fullPath);
                                if (hThumb) {
                                    DrawThumbnail(hdc, rcIcon, hThumb, isSelected, isHot, pThis->m_hwnd);
                                } else {
                                    int customIcon = ShellUtils::GetCustomPathIcon(item.fullPath);
                                    int iconIdx = (customIcon >= 0) ? customIcon : item.iconIndex;
                                    HIMAGELIST himl = ShellUtils::GetSystemImageList(SHIL_LARGE);
                                    ImageList_DrawEx(himl, iconIdx, hdc, iconX, iconY, iconSize, iconSize, CLR_NONE, CLR_NONE, ILD_TRANSPARENT);
                                }

                                // Ghosting if cut or hidden
                                bool isCut = ShellUtils::IsPathCut(item.fullPath);
                                bool isHidden = (item.attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
                                if (isCut || isHidden) {
                                    COLORREF bgClr = isSelected ? Theme::Get().ClrSelected() : Theme::Get().ClrBackground();
                                    BYTE alpha = isCut ? 140 : 120;
                                    ShellUtils::BlendRectWithColor(hdc, rcIcon, bgClr, alpha);
                                }

                                // 4. Column positions strictly matching Explorer standards:
                                // Column 1 starts after icon; Column 2 starts at ~40%; Column 3 starts at ~75%
                                int itemW = rcItem.right - rcItem.left;
                                int col1Left = iconX + iconSize + Theme::Get().Scale(12, pThis->m_hwnd);
                                int col2Left = rcItem.left + itemW * 40 / 100;
                                if (col2Left < col1Left + Theme::Get().Scale(140, pThis->m_hwnd)) {
                                    col2Left = col1Left + Theme::Get().Scale(140, pThis->m_hwnd);
                                }
                                int col3Left = rcItem.left + itemW * 75 / 100;
                                if (col3Left < col2Left + Theme::Get().Scale(160, pThis->m_hwnd)) {
                                    col3Left = col2Left + Theme::Get().Scale(160, pThis->m_hwnd);
                                }

                                int col1Right = col2Left - Theme::Get().Scale(16, pThis->m_hwnd);
                                int col2Right = col3Left - Theme::Get().Scale(16, pThis->m_hwnd);
                                int col3Right = rcItem.right - Theme::Get().Scale(16, pThis->m_hwnd);

                                // Two-line text grid: Line 1 (top) and Line 2 (bottom)
                                int lineH = Theme::Get().Scale(16, pThis->m_hwnd);
                                int gap = Theme::Get().Scale(2, pThis->m_hwnd);
                                int totalTextH = lineH * 2 + gap;
                                int textTop = rcItem.top + (rcItem.bottom - rcItem.top - totalTextH) / 2;
                                int line1Top = textTop;
                                int line1Bottom = textTop + lineH;
                                int line2Top = textTop + lineH + gap;
                                int line2Bottom = textTop + totalTextH;

                                SetBkMode(hdc, TRANSPARENT);
                                HGDIOBJ oldFont = SelectObject(hdc, pThis->m_hFontText);

                                // Column 1: Folder name or File name (strictly on Line 1, aligned with Col 2 & Col 3)
                                RECT rcName = { col1Left, line1Top, col1Right, line1Bottom };
                                SetTextColor(hdc, Theme::Get().ClrTextPrimary());
                                DrawTextW(hdc, item.name.c_str(), -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                                // Column 2 & Column 3 data strings
                                std::wstring dateStr = Util::FormatFileTime(item.modifiedTime);
                                std::wstring sizeStr = item.isFolder ? L"" : Util::FormatBytes(item.size);
                                std::wstring dimStr = item.isFolder ? L"" : pThis->GetDimensions(item.fullPath);

                                SetTextColor(hdc, Theme::Get().ClrTextSecondary());
                                if (item.isFolder) {
                                    // Folder:
                                    // Col 2: blank
                                    // Col 3: Line 1: 修改日期 (vertically aligned with folder name on Line 1)
                                    if (!dateStr.empty()) {
                                        std::wstring col3Line1 = L"修改日期: " + dateStr;
                                        RECT rcCol3L1 = { col3Left, line1Top, col3Right, line1Bottom };
                                        DrawTextW(hdc, col3Line1.c_str(), -1, &rcCol3L1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                                    }
                                } else {
                                    // File:
                                    // Col 2 Line 1: 类型: ...
                                    std::wstring typeStr = item.typeName.empty() ? (item.ext.empty() ? L"文件" : item.ext + L" 文件") : item.typeName;
                                    std::wstring col2Line1 = L"类型: " + typeStr;
                                    RECT rcCol2L1 = { col2Left, line1Top, col2Right, line1Bottom };
                                    DrawTextW(hdc, col2Line1.c_str(), -1, &rcCol2L1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                                    // Col 2 Line 2: 分辨率: ... (if image with dimensions)
                                    if (!dimStr.empty()) {
                                        std::wstring col2Line2 = L"分辨率: " + dimStr;
                                        RECT rcCol2L2 = { col2Left, line2Top, col2Right, line2Bottom };
                                        DrawTextW(hdc, col2Line2.c_str(), -1, &rcCol2L2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                                    }

                                    // Col 3:
                                    // Line 1: 修改日期: ... (for regular files without image dimensions)
                                    std::wstring col3Line1 = dimStr.empty() ? (dateStr.empty() ? L"" : (L"修改日期: " + dateStr)) : L"";
                                    if (!col3Line1.empty()) {
                                        RECT rcCol3L1 = { col3Left, line1Top, col3Right, line1Bottom };
                                        DrawTextW(hdc, col3Line1.c_str(), -1, &rcCol3L1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                                    }

                                    // Line 2: 大小: ... (strictly on Line 2, aligned with Col 2 Line 2)
                                    if (!sizeStr.empty()) {
                                        std::wstring col3Line2 = L"大小: " + sizeStr;
                                        RECT rcCol3L2 = { col3Left, line2Top, col3Right, line2Bottom };
                                        DrawTextW(hdc, col3Line2.c_str(), -1, &rcCol3L2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                                    }
                                }

                                SelectObject(hdc, oldFont);
                                return CDRF_SKIPDEFAULT;
                            }
                        }
                    }

                    if (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3) {
                        // Extra Large, Large, Medium icon views: custom draw item with multi-line wrapped text
                        int row = (int)pcd->nmcd.dwItemSpec;
                        if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                            int idx = pThis->m_filteredIndices[row];
                            if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                                const auto& item = pThis->m_allItems[idx];
                                HDC hdc = pcd->nmcd.hdc;
                                RECT rcBounds = { 0 };
                                ListView_GetItemRect(pThis->m_hwnd, row, &rcBounds, LVIR_BOUNDS);
                                bool isSelected = (ListView_GetItemState(pThis->m_hwnd, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                                bool isHot = (row == pThis->m_hoverItem) || (ListView_GetItemState(pThis->m_hwnd, row, LVIS_DROPHILITED) & LVIS_DROPHILITED) != 0;

                                // Full grid cell rect from icon spacing (LVIR_BOUNDS in SysListView32 has hardcoded height)
                                DWORD sp = (DWORD)SendMessageW(pThis->m_hwnd, LVM_GETITEMSPACING, FALSE, 0);
                                int spX = LOWORD(sp);
                                int spY = HIWORD(sp);
                                int cellW = (spX > 0) ? spX : (rcBounds.right - rcBounds.left);
                                int cellH = (spY > 0) ? spY : (rcBounds.bottom - rcBounds.top);
                                RECT rcCell = { rcBounds.left, rcBounds.top, rcBounds.left + cellW, rcBounds.top + cellH };

                                // Always paint clean background across entire cell to prevent hover/selection residue
                                FillRect(hdc, &rcCell, Theme::Get().BrushBackground());

                                // 1. Icon layout
                                int iconDim = (pThis->m_viewMode == 1) ? Theme::Get().Scale(256, pThis->m_hwnd)
                                            : ((pThis->m_viewMode == 2) ? Theme::Get().Scale(96, pThis->m_hwnd)
                                            : Theme::Get().Scale(48, pThis->m_hwnd));
                                int iconX = rcCell.left + (cellW - iconDim) / 2;
                                int iconY = rcCell.top + Theme::Get().Scale(6, pThis->m_hwnd);
                                RECT rcIcon = { iconX, iconY, iconX + iconDim, iconY + iconDim };

                                // 2. Label rect spanning below icon down to bottom of cell
                                RECT rcLabel;
                                rcLabel.left = rcCell.left + Theme::Get().Scale(4, pThis->m_hwnd);
                                rcLabel.right = rcCell.right - Theme::Get().Scale(4, pThis->m_hwnd);
                                rcLabel.top = rcIcon.bottom + Theme::Get().Scale(4, pThis->m_hwnd);
                                rcLabel.bottom = rcCell.bottom - Theme::Get().Scale(4, pThis->m_hwnd);

                                // 3. Selection / hover background snugly enclosing icon and text
                                if (isSelected || isHot) {
                                    RECT rcCalc = rcLabel;
                                    HGDIOBJ tmpFont = SelectObject(hdc, pThis->m_hFontText);
                                    DrawTextW(hdc, item.name.c_str(), -1, &rcCalc, DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_CALCRECT);
                                    SelectObject(hdc, tmpFont);
                                    int textBottom = rcLabel.top + (rcCalc.bottom - rcCalc.top) + Theme::Get().Scale(4, pThis->m_hwnd);
                                    if (textBottom > rcCell.bottom) textBottom = rcCell.bottom;
                                    RECT rcHl = { rcCell.left + 2, rcCell.top + 2, rcCell.right - 2, textBottom };
                                    HBRUSH hbr = isSelected ? Theme::Get().BrushSelected() : Theme::Get().BrushHover();
                                    FillRect(hdc, &rcHl, hbr);
                                }

                                // 4. Draw thumbnail or system icon
                                HBITMAP hThumb = pThis->GetThumbnail(item.fullPath);
                                if (hThumb) {
                                    DrawThumbnail(hdc, rcIcon, hThumb, isSelected, isHot, pThis->m_hwnd);
                                } else {
                                    int customIcon = ShellUtils::GetCustomPathIcon(item.fullPath);
                                    int iconIdx = (customIcon >= 0) ? customIcon : item.iconIndex;
                                    HIMAGELIST himl = (pThis->m_viewMode == 1 || pThis->m_viewMode == 2)
                                                    ? ShellUtils::GetSystemImageList(SHIL_JUMBO)
                                                    : ShellUtils::GetSystemImageList(SHIL_EXTRALARGE);
                                    int drawDim = (pThis->m_viewMode == 1 || pThis->m_viewMode == 2)
                                                ? iconDim : Theme::Get().Scale(48, pThis->m_hwnd);
                                    int drawX = rcIcon.left + (iconDim - drawDim) / 2;
                                    int drawY = rcIcon.top + (iconDim - drawDim) / 2;
                                    ImageList_DrawEx(himl, iconIdx, hdc, drawX, drawY, drawDim, drawDim, CLR_NONE, CLR_NONE, ILD_TRANSPARENT);
                                }

                                // Ghosting if cut or hidden
                                bool isCut = ShellUtils::IsPathCut(item.fullPath);
                                bool isHidden = (item.attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
                                if (isCut || isHidden) {
                                    COLORREF bgClr = isSelected ? Theme::Get().ClrSelected() : Theme::Get().ClrBackground();
                                    BYTE alpha = isCut ? 140 : 120;
                                    ShellUtils::BlendRectWithColor(hdc, rcIcon, bgClr, alpha);
                                }

                                // 5. Multi-line auto-wrapped filename
                                SetBkMode(hdc, TRANSPARENT);
                                SetTextColor(hdc, Theme::Get().ClrTextPrimary());
                                HGDIOBJ oldFont = SelectObject(hdc, pThis->m_hFontText);
                                DrawTextW(hdc, item.name.c_str(), -1, &rcLabel,
                                          DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS);
                                SelectObject(hdc, oldFont);

                                return CDRF_SKIPDEFAULT;
                            }
                        }
                    }

                    if (pThis->m_viewMode != 0) {
                        pcd->clrTextBk = CLR_NONE;
                        pcd->clrText = Theme::Get().ClrTextPrimary();
                        return CDRF_NOTIFYPOSTPAINT;
                    }
                    return CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
                }
                case CDDS_ITEMPOSTPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (row >= 0 && (ListView_GetItemState(pThis->m_hwnd, row, LVIS_SELECTED) & LVIS_SELECTED) != 0);
                    bool isHot = (pcd->nmcd.uItemState & CDIS_HOT) != 0 || (row >= 0 && (ListView_GetItemState(pThis->m_hwnd, row, LVIS_DROPHILITED) & LVIS_DROPHILITED) != 0);

                    // Thumbnail custom rendering in tiles view
                    if (pThis->m_viewMode == 6) {
                        if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                            int idx = pThis->m_filteredIndices[row];
                            if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                                const auto& item = pThis->m_allItems[idx];
                                HBITMAP hThumb = pThis->GetThumbnail(item.fullPath);
                                if (hThumb) {
                                    RECT rcIcon = { 0 };
                                    if (ListView_GetItemRect(pThis->m_hwnd, row, &rcIcon, LVIR_ICON)) {
                                        DrawThumbnail(pcd->nmcd.hdc, rcIcon, hThumb, isSelected, isHot, pThis->m_hwnd);
                                    }
                                }
                            }
                        }
                    }

                    // 1. Cut items and Hidden items ghosting effect (icon虚化淡化)
                    // Windows Explorer standard: only items with FILE_ATTRIBUTE_HIDDEN are dimmed; dotfiles without hidden attr remain normal
                    if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                        int idx = pThis->m_filteredIndices[row];
                        if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                            bool isCut = ShellUtils::IsPathCut(pThis->m_allItems[idx].fullPath);
                            bool isHidden = (pThis->m_allItems[idx].attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
                            if (isCut || isHidden) {
                                RECT rcIcon;
                                if (ListView_GetItemRect(pThis->m_hwnd, row, &rcIcon, LVIR_ICON)) {
                                    COLORREF bgClr = isSelected ? Theme::Get().ClrSelected() : Theme::Get().ClrBackground();
                                    BYTE alpha = isCut ? 140 : 120;
                                    ShellUtils::BlendRectWithColor(pcd->nmcd.hdc, rcIcon, bgClr, alpha);
                                }
                            }
                        }
                    }

                    // 2. Grid lines - subtle horizontal dividers only (Details view only)
                    if (Config::Get().showGridLines && pThis->m_viewMode == 0 && row >= 0) {
                        RECT rcItem;
                        ListView_GetItemRect(pThis->m_hwnd, row, &rcItem, LVIR_BOUNDS);
                        COLORREF gridClr = Theme::Get().IsDarkMode() ? RGB(45, 47, 52) : RGB(235, 238, 242);
                        HPEN hPen = CreatePen(PS_SOLID, 1, gridClr);
                        HPEN hOldPen = (HPEN)SelectObject(pcd->nmcd.hdc, hPen);
                        MoveToEx(pcd->nmcd.hdc, rcItem.left, rcItem.bottom - 1, nullptr);
                        LineTo(pcd->nmcd.hdc, rcItem.right, rcItem.bottom - 1);
                        SelectObject(pcd->nmcd.hdc, hOldPen);
                        DeleteObject(hPen);
                    }

                    // 3. Item checkbox custom rendering (Details view only)
                    if (Config::Get().showItemCheckBoxes && pThis->m_viewMode == 0 && row >= 0) {
                        RECT rcIcon = { 0 };
                        RECT rcItem = { 0 };
                        if (ListView_GetItemRect(pThis->m_hwnd, row, &rcIcon, LVIR_ICON) &&
                            ListView_GetItemRect(pThis->m_hwnd, row, &rcItem, LVIR_BOUNDS)) {
                            int slotW = rcIcon.left - rcItem.left;
                            if (slotW > 0) {
                                HDC hdc = pcd->nmcd.hdc;
                                HBRUSH hbrRowBg = isSelected ? Theme::Get().BrushSelected() : Theme::Get().BrushBackground();
                                RECT rcSlot = { rcItem.left, rcItem.top, rcIcon.left, rcItem.bottom };
                                FillRect(hdc, &rcSlot, hbrRowBg);

                                int boxSize = Theme::Get().Scale(14, pThis->m_hwnd);
                                int rad = Theme::Get().Scale(3, pThis->m_hwnd);
                                int boxX = rcItem.left + (slotW - boxSize) / 2;
                                int boxY = rcItem.top + (rcItem.bottom - rcItem.top - boxSize) / 2;
                                RECT rcBox = { boxX, boxY, boxX + boxSize, boxY + boxSize };

                                bool isChecked = (ListView_GetItemState(pThis->m_hwnd, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                                bool isDark = Theme::Get().IsDarkMode();
                                COLORREF boxBg = isChecked ? Theme::Get().ClrAccent() : (isDark ? RGB(45, 45, 45) : RGB(255, 255, 255));
                                COLORREF boxBorder = isChecked ? Theme::Get().ClrAccent() : (isDark ? RGB(110, 110, 110) : RGB(160, 160, 160));

                                HBRUSH hbrBox = CreateSolidBrush(boxBg);
                                HPEN hpenBox = CreatePen(PS_SOLID, 1, boxBorder);
                                HGDIOBJ oldBrush = SelectObject(hdc, hbrBox);
                                HGDIOBJ oldPen = SelectObject(hdc, hpenBox);
                                RoundRect(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, rad, rad);
                                SelectObject(hdc, oldBrush);
                                SelectObject(hdc, oldPen);
                                DeleteObject(hbrBox);
                                DeleteObject(hpenBox);

                                if (isChecked) {
                                    DrawSmoothCheckmark(hdc, rcBox, pThis->m_hwnd);
                                }
                            }
                        }
                    }
                    return CDRF_DODEFAULT;
                }
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
                    int row = (int)pcd->nmcd.dwItemSpec;
                    bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;

                    pcd->clrTextBk = CLR_NONE;

                    bool isCut = false;
                    bool isHidden = false;
                    if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                        int idx = pThis->m_filteredIndices[row];
                        if (idx >= 0 && idx < (int)pThis->m_allItems.size()) {
                            isCut = ShellUtils::IsPathCut(pThis->m_allItems[idx].fullPath);
                            isHidden = (pThis->m_allItems[idx].attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
                        }
                    }

                    if (!isSelected) {
                        if (isCut) {
                            pcd->clrText = Theme::Get().IsDarkMode() ? RGB(140, 140, 140) : RGB(155, 155, 155);
                        } else if (isHidden) {
                            pcd->clrText = Theme::Get().IsDarkMode() ? RGB(150, 150, 150) : RGB(140, 140, 140);
                        } else if (pcd->iSubItem == 0) {
                            pcd->clrText = Theme::Get().ClrTextPrimary();
                        } else {
                            pcd->clrText = Theme::Get().ClrTextSecondary();
                        }
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
                    if (pThis->m_hFontText) {
                        SelectObject(pcd->nmcd.hdc, pThis->m_hFontText);
                    }
                    return CDRF_NEWFONT;
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
                if (pnlv->uChanged & LVIF_STATE) {
                    if ((pnlv->uNewState ^ pnlv->uOldState) & LVIS_SELECTED) {
                        pThis->NotifyStatus();
                    }
                    if (pThis->m_viewMode == 1 || pThis->m_viewMode == 2 || pThis->m_viewMode == 3) {
                        if ((pnlv->uNewState ^ pnlv->uOldState) & (LVIS_SELECTED | LVIS_FOCUSED)) {
                            pThis->InvalidateItemCell(pnlv->iItem);
                        }
                    }
                    if (pnlv->uNewState & LVIS_SELECTED) {
                        if (pThis->onSelectionChanged) {
                            int row = pnlv->iItem;
                            if (row >= 0 && row < (int)pThis->m_filteredIndices.size()) {
                                int idx = pThis->m_filteredIndices[row];
                                pThis->onSelectionChanged(pThis->m_allItems[idx].fullPath);
                            }
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
                        pThis->SetSort(clickedCol, !pThis->m_sortAsc);
                    } else {
                        pThis->SetSort(clickedCol, true);
                    }
                }
                return 0;
            }
            }
        }
        break;
    }
    case FileListView::WM_NOTIFY_DIR_CHANGED:
        KillTimer(pThis->m_hwnd, 703);
        SetTimer(pThis->m_hwnd, 703, 200, nullptr);
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
    case FileListView::WM_ASYNC_NAV_COMPLETE: {
        auto* pResult = (FileListView::AsyncNavResult*)lParam;
        if (pResult) {
            if (pResult->generation == pThis->m_navGeneration && pResult->path == pThis->m_currentPath) {
                pThis->m_isNavLoading = false;
                InvalidateRect(pThis->m_hwnd, nullptr, TRUE);
                if (pResult->success) {
                    pThis->m_allItems = std::move(pResult->items);
                    pThis->SortItems();
                    pThis->UpdateHeaderSortArrow();
                    ListView_SetItemCountEx(pThis->m_hwnd, 0, 0);
                    ListView_SetItemState(pThis->m_hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    pThis->ApplyFilter();
                    if (pThis->m_hasPendingRestore) {
                        pThis->RestoreViewState(pThis->m_pendingRestoreState);
                        pThis->m_hasPendingRestore = false;
                    }
                    if (!pThis->m_pendingSelectPath.empty()) {
                        std::wstring target = pThis->m_pendingSelectPath;
                        pThis->m_pendingSelectPath.clear();
                        pThis->SelectItemByPath(target);
                    }
                    pThis->NotifyStatus();
                    pThis->m_watcher.Watch(pThis->m_currentPath, pThis->m_hwnd, WM_NOTIFY_DIR_CHANGED);
                    if (pThis->m_calcFolderSize && pThis->m_currentPath != L"此电脑" && pThis->m_currentPath != L"This PC") {
                        for (const auto& item : pThis->m_allItems) {
                            if (item.isFolder && !item.isDrive) {
                                pThis->m_calcSize.RequestFolderSize(item.fullPath, pThis->m_hwnd, WM_NOTIFY_FOLDER_SIZE);
                            }
                        }
                    }
                    pThis->StartCustomIconLoader();
                    pThis->StartThumbnailLoader();
                } else {
                    pThis->m_allItems.clear();
                    pThis->ApplyFilter();
                    if (pThis->onStatusText) {
                        pThis->onStatusText(I18n::Get().IsChinese() ? L"无法访问网络共享路径" : L"Cannot access network share");
                    }
                }
            }
            delete pResult;
        }
        return 0;
    }
    case FileListView::WM_REFRESH_ICONS: {
        int gen = (int)wParam;
        if (gen == pThis->m_iconNavGeneration.load()) {
            InvalidateRect(pThis->m_hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case FileListView::WM_NOTIFY_THUMBNAIL_BATCH: {
        int gen = (int)wParam;
        if (gen == pThis->m_thumbNavGeneration.load()) {
            InvalidateRect(pThis->m_hwnd, nullptr, FALSE);
        }
        return 0;
    }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void FileListView::StartDragDrop(int itemIndex) {
    if (m_currentPath.empty() || m_currentPath == L"This PC" || m_currentPath == L"此电脑") {
        return;
    }

    ReleaseCapture();

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
        if (PathFileExistsW(p.c_str()) || ShellUtils::IsVirtualShellPath(p)) {
            validPaths.push_back(p);
        }
    }
    if (validPaths.empty()) return;

    DWORD dwEffect = 0;
    HRESULT hr = ShellUtils::DoFileDragDrop(m_hwnd, m_currentPath, validPaths, &dwEffect);
    if (SUCCEEDED(hr) && dwEffect == DROPEFFECT_MOVE) {
        Refresh();
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, m_currentPath.c_str(), nullptr);
    }
}
