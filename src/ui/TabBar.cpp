#include "TabBar.h"
#include "Theme.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

static const wchar_t* TABBAR_CLASS = L"NeoMDIE_TabBar";

TabBar::TabBar() {
}

TabBar::~TabBar() {
}

HWND TabBar::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, TABBAR_CLASS, &wc)) {
        wc.lpfnWndProc = TabBar::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = TABBAR_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, TABBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 100, 36,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    return m_hwnd;
}

static const COLORREF s_tabPalette[] = {
    RGB(0, 120, 215),   // Azure Blue
    RGB(16, 137, 62),   // Emerald Green
    RGB(232, 17, 35),   // Coral Red
    RGB(136, 23, 152),  // Purple
    RGB(255, 140, 0),   // Orange
    RGB(0, 153, 188),   // Cyan / Teal
    RGB(234, 0, 94),    // Magenta / Pink
    RGB(107, 105, 214), // Lavender
    RGB(16, 124, 65),   // Forest Green
    RGB(202, 80, 16),   // Amber
    RGB(0, 99, 177),    // Cobalt
    RGB(180, 0, 158)    // Violet
};
static size_t s_tabColorIndex = 0;

int TabBar::AddTab(const std::wstring& path, bool activate) {
    TabItem item;
    item.path = path;
    item.title = ShellUtils::GetDisplayName(path);
    item.iconIndex = ShellUtils::GetItemIconIndex(path, FILE_ATTRIBUTE_DIRECTORY, true);
    item.color = s_tabPalette[s_tabColorIndex % (sizeof(s_tabPalette) / sizeof(s_tabPalette[0]))];
    s_tabColorIndex++;

    int index = (int)m_tabs.size();
    m_tabs.push_back(item);

    RecalcLayout();
    if (activate || m_tabs.size() == 1) {
        SetActiveTab(index);
    } else {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    return index;
}

void TabBar::CloseTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    if (m_tabs[index].isPinned) return; // Cannot close pinned tab

    if (!m_tabs[index].path.empty()) {
        m_closedHistory.push_back(m_tabs[index].path);
    }
    m_tabs.erase(m_tabs.begin() + index);

    if (onTabClosed) {
        onTabClosed(index);
    }

    if (m_tabs.empty()) {
        AddTab(L"This PC", true);
        return;
    }

    if (m_activeIndex >= (int)m_tabs.size()) {
        m_activeIndex = (int)m_tabs.size() - 1;
    }

    RecalcLayout();
    if (onTabChanged) {
        onTabChanged(m_activeIndex);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseOtherTabs(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    TabItem keepTab = m_tabs[index];

    std::vector<TabItem> remaining;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if ((int)i == index || m_tabs[i].isPinned) {
            remaining.push_back(m_tabs[i]);
        } else if (!m_tabs[i].path.empty()) {
            m_closedHistory.push_back(m_tabs[i].path);
        }
    }

    m_tabs = std::move(remaining);
    m_activeIndex = 0;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].path == keepTab.path) {
            m_activeIndex = (int)i;
            break;
        }
    }

    RecalcLayout();
    if (onTabChanged) {
        onTabChanged(m_activeIndex);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseTabsToLeft(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;

    std::vector<TabItem> remaining;
    TabItem activeTab = m_tabs[m_activeIndex];
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if ((int)i >= index || m_tabs[i].isPinned) {
            remaining.push_back(m_tabs[i]);
        } else if (!m_tabs[i].path.empty()) {
            m_closedHistory.push_back(m_tabs[i].path);
        }
    }

    m_tabs = std::move(remaining);
    m_activeIndex = 0;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].path == activeTab.path) {
            m_activeIndex = (int)i;
            break;
        }
    }

    RecalcLayout();
    if (onTabChanged) {
        onTabChanged(m_activeIndex);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseTabsToRight(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;

    std::vector<TabItem> remaining;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if ((int)i <= index || m_tabs[i].isPinned) {
            remaining.push_back(m_tabs[i]);
        } else if (!m_tabs[i].path.empty()) {
            m_closedHistory.push_back(m_tabs[i].path);
        }
    }

    m_tabs = std::move(remaining);
    if (m_activeIndex >= (int)m_tabs.size()) {
        m_activeIndex = (int)m_tabs.size() - 1;
    }

    RecalcLayout();
    if (onTabChanged) {
        onTabChanged(m_activeIndex);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseAllTabs() {
    std::vector<TabItem> remaining;
    for (const auto& t : m_tabs) {
        if (t.isPinned) {
            remaining.push_back(t);
        } else if (!t.path.empty()) {
            m_closedHistory.push_back(t.path);
        }
    }

    if (remaining.empty()) {
        m_tabs.clear();
        AddTab(L"This PC", true);
        return;
    }

    m_tabs = std::move(remaining);
    m_activeIndex = 0;
    RecalcLayout();
    if (onTabChanged) {
        onTabChanged(m_activeIndex);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::ReopenClosedTab() {
    if (!m_closedHistory.empty()) {
        std::wstring lastPath = m_closedHistory.back();
        m_closedHistory.pop_back();
        AddTab(lastPath, true);
    }
}

void TabBar::NextTab() {
    if (m_tabs.empty()) return;
    int next = (m_activeIndex + 1) % (int)m_tabs.size();
    SetActiveTab(next);
}

void TabBar::PrevTab() {
    if (m_tabs.empty()) return;
    int prev = (m_activeIndex - 1 + (int)m_tabs.size()) % (int)m_tabs.size();
    SetActiveTab(prev);
}

void TabBar::TogglePinTab(int index) {
    if (index >= 0 && index < (int)m_tabs.size()) {
        m_tabs[index].isPinned = !m_tabs[index].isPinned;
        RecalcLayout();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

bool TabBar::IsTabPinned(int index) const {
    if (index >= 0 && index < (int)m_tabs.size()) {
        return m_tabs[index].isPinned;
    }
    return false;
}

void TabBar::SetActiveTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    if (m_activeIndex != index) {
        m_activeIndex = index;
        if (onTabChanged) {
            onTabChanged(m_activeIndex);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

std::wstring TabBar::GetTabPath(int index) const {
    if (index >= 0 && index < (int)m_tabs.size()) {
        return m_tabs[index].path;
    }
    return L"";
}

void TabBar::SetTabPath(int index, const std::wstring& path) {
    if (index >= 0 && index < (int)m_tabs.size()) {
        m_tabs[index].path = path;
        m_tabs[index].title = ShellUtils::GetDisplayName(path);
        m_tabs[index].iconIndex = ShellUtils::GetItemIconIndex(path, FILE_ATTRIBUTE_DIRECTORY, true);
        RecalcLayout();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TabBar::SetTabColor(int index, COLORREF color) {
    if (index >= 0 && index < (int)m_tabs.size()) {
        m_tabs[index].color = color;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

COLORREF TabBar::GetTabColor(int index) const {
    if (index >= 0 && index < (int)m_tabs.size()) {
        return m_tabs[index].color;
    }
    return 0;
}

void TabBar::UpdateLayout(int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, 0, 0, width, height, TRUE);
        RecalcLayout();
    }
}

void TabBar::ShowTabContextMenu(int index, POINT ptScreen) {
    if (index < 0 || index >= (int)m_tabs.size()) return;

    HMENU hMenu = CreatePopupMenu();
    bool pinned = m_tabs[index].isPinned;
    bool isZh = I18n::Get().IsChinese();

    AppendMenuW(hMenu, MF_STRING | (pinned ? MF_GRAYED : 0), 1, I18n::Get().T(StringId::TabMenuClose));
    AppendMenuW(hMenu, MF_STRING, 2, I18n::Get().T(StringId::TabMenuCloseOthers));
    AppendMenuW(hMenu, MF_STRING, 3, I18n::Get().T(StringId::TabMenuCloseRight));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 4, pinned ? I18n::Get().T(StringId::TabMenuUnpin) : I18n::Get().T(StringId::TabMenuPin));
    AppendMenuW(hMenu, MF_STRING, 5, I18n::Get().T(StringId::TabMenuDuplicate));

    // Tab Color submenu
    HMENU hColorSub = CreatePopupMenu();
    AppendMenuW(hColorSub, MF_STRING, 101, isZh ? L"天蓝色" : L"Azure Blue");
    AppendMenuW(hColorSub, MF_STRING, 102, isZh ? L"翡翠绿" : L"Emerald Green");
    AppendMenuW(hColorSub, MF_STRING, 103, isZh ? L"珊瑚红" : L"Coral Red");
    AppendMenuW(hColorSub, MF_STRING, 104, isZh ? L"高雅紫" : L"Purple");
    AppendMenuW(hColorSub, MF_STRING, 105, isZh ? L"活力橙" : L"Orange");
    AppendMenuW(hColorSub, MF_STRING, 106, isZh ? L"薄荷青" : L"Teal");
    AppendMenuW(hColorSub, MF_STRING, 107, isZh ? L"蔷薇粉" : L"Pink");
    AppendMenuW(hColorSub, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hColorSub, MF_STRING, 110, isZh ? L"随机新颜色(&R)" : L"Random Color(&R)");
    AppendMenuW(hColorSub, MF_STRING, 120, isZh ? L"自定义颜色(&O)..." : L"Choose Custom Color(&O)...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSub, isZh ? L"标签颜色(&C)" : L"Tab Color(&C)");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 6, I18n::Get().T(StringId::TabMenuCopyPath));
    AppendMenuW(hMenu, MF_STRING, 7, I18n::Get().T(StringId::TabMenuOpenTerminal));

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd >= 101 && cmd <= 107) {
        COLORREF colors[] = {
            RGB(0, 120, 215), RGB(16, 137, 62), RGB(232, 17, 35),
            RGB(136, 23, 152), RGB(255, 140, 0), RGB(0, 153, 188), RGB(234, 0, 94)
        };
        SetTabColor(index, colors[cmd - 101]);
        return;
    } else if (cmd == 110) {
        SetTabColor(index, s_tabPalette[s_tabColorIndex++ % (sizeof(s_tabPalette) / sizeof(s_tabPalette[0]))]);
        return;
    } else if (cmd == 120) {
        static COLORREF customColors[16] = { 0 };
        CHOOSECOLORW cc = { sizeof(cc) };
        cc.hwndOwner = m_hwnd;
        cc.rgbResult = m_tabs[index].color;
        cc.lpCustColors = customColors;
        cc.Flags = CC_RGBINIT | CC_FULLOPEN;
        if (ChooseColorW(&cc)) {
            SetTabColor(index, cc.rgbResult);
        }
        return;
    }

    switch (cmd) {
    case 1: CloseTab(index); break;
    case 2: CloseOtherTabs(index); break;
    case 3: CloseTabsToRight(index); break;
    case 4: TogglePinTab(index); break;
    case 5: AddTab(m_tabs[index].path, true); break;
    case 6: ShellUtils::CopyToClipboard(m_tabs[index].path); break;
    case 7:
        if (onOpenTerminalRequested) {
            onOpenTerminalRequested(m_tabs[index].path);
        } else {
            ShellUtils::LaunchTerminal(m_tabs[index].path);
        }
        break;
    }
}

void TabBar::RecalcLayout() {
    if (!m_hwnd || m_tabs.empty()) return;

    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    int totalWidth = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;

    int newTabBtnWidth = Theme::Get().Scale(32, m_hwnd);
    int availableWidth = totalWidth - newTabBtnWidth - Theme::Get().Scale(8, m_hwnd);

    int maxTabWidth = Theme::Get().Scale(200, m_hwnd);
    int minTabWidth = Theme::Get().Scale(90, m_hwnd);

    int tabWidth = maxTabWidth;
    if ((int)m_tabs.size() * tabWidth > availableWidth) {
        tabWidth = availableWidth / (int)m_tabs.size();
        if (tabWidth < minTabWidth) tabWidth = minTabWidth;
    }

    int curX = Theme::Get().Scale(4, m_hwnd);
    int tabHeight = h - Theme::Get().Scale(4, m_hwnd);
    int closeSize = Theme::Get().Scale(16, m_hwnd);

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        m_tabs[i].rc.left = curX;
        m_tabs[i].rc.top = Theme::Get().Scale(4, m_hwnd);
        m_tabs[i].rc.right = curX + tabWidth;
        m_tabs[i].rc.bottom = m_tabs[i].rc.top + tabHeight;

        m_tabs[i].rcClose.right = m_tabs[i].rc.right - Theme::Get().Scale(6, m_hwnd);
        m_tabs[i].rcClose.left = m_tabs[i].rcClose.right - closeSize;
        m_tabs[i].rcClose.top = m_tabs[i].rc.top + (tabHeight - closeSize) / 2;
        m_tabs[i].rcClose.bottom = m_tabs[i].rcClose.top + closeSize;

        curX += tabWidth + Theme::Get().Scale(2, m_hwnd);
    }

    m_rcNewTab.left = curX + Theme::Get().Scale(4, m_hwnd);
    m_rcNewTab.top = Theme::Get().Scale(6, m_hwnd);
    m_rcNewTab.right = m_rcNewTab.left + Theme::Get().Scale(24, m_hwnd);
    m_rcNewTab.bottom = m_rcNewTab.top + Theme::Get().Scale(24, m_hwnd);
}

void TabBar::Draw(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

    // Background
    HBRUSH hbrBg = Theme::Get().BrushBackground();
    FillRect(hdcMem, &rcClient, hbrBg);

    HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(false);
    HFONT hFont = Theme::Get().GetUIFont(m_hwnd);
    HGDIOBJ oldFont = SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);

    // Draw tabs
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        bool isActive = ((int)i == m_activeIndex);
        bool isHover = ((int)i == m_hoverIndex);
        const auto& tab = m_tabs[i];

        // Tab surface
        COLORREF tabBg = isActive ? Theme::Get().ClrSurface() : (isHover ? Theme::Get().ClrHover() : Theme::Get().ClrBackground());
        HBRUSH hbrTab = CreateSolidBrush(tabBg);
        FillRect(hdcMem, &tab.rc, hbrTab);
        DeleteObject(hbrTab);

        // Colored accent strip at top of tab
        if (tab.color != 0) {
            RECT rcColorStrip = tab.rc;
            rcColorStrip.bottom = rcColorStrip.top + Theme::Get().Scale(3, m_hwnd);
            HBRUSH hbrStrip = CreateSolidBrush(tab.color);
            FillRect(hdcMem, &rcColorStrip, hbrStrip);
            DeleteObject(hbrStrip);
        }

        // Active indicator line at bottom
        if (isActive) {
            RECT rcLine = tab.rc;
            rcLine.top = rcLine.bottom - Theme::Get().Scale(3, m_hwnd);
            HBRUSH hbrAccent = CreateSolidBrush(tab.color != 0 ? tab.color : Theme::Get().ClrAccent());
            FillRect(hdcMem, &rcLine, hbrAccent);
            DeleteObject(hbrAccent);
        }

        // Icon
        int iconY = tab.rc.top + (tab.rc.bottom - tab.rc.top - 16) / 2;
        int iconX = tab.rc.left + Theme::Get().Scale(8, m_hwnd);
        if (himlSmall) {
            ImageList_Draw(himlSmall, tab.iconIndex, hdcMem, iconX, iconY, ILD_TRANSPARENT);
        }

        // Title
        RECT rcText = tab.rc;
        rcText.left = iconX + 20;
        rcText.right = tab.isPinned ? tab.rc.right - Theme::Get().Scale(6, m_hwnd) : (tab.rcClose.left - Theme::Get().Scale(4, m_hwnd));
        SetTextColor(hdcMem, isActive ? Theme::Get().ClrTextPrimary() : Theme::Get().ClrTextSecondary());
        std::wstring displayTitle = tab.isPinned ? (L"📌 " + tab.title) : tab.title;
        DrawTextW(hdcMem, displayTitle.c_str(), -1, &rcText,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Close button '×' (only if not pinned)
        if (!tab.isPinned && (isHover || isActive)) {
            bool closeHover = (isHover && m_hoverClose);
            if (closeHover) {
                HBRUSH hbrCloseHover = CreateSolidBrush(RGB(232, 17, 35));
                FillRect(hdcMem, &tab.rcClose, hbrCloseHover);
                DeleteObject(hbrCloseHover);
                SetTextColor(hdcMem, RGB(255, 255, 255));
            } else {
                SetTextColor(hdcMem, Theme::Get().ClrTextSecondary());
            }
            HFONT hCloseFont = Theme::Get().GetIconFont(10, m_hwnd);
            HGDIOBJ oldCloseF = SelectObject(hdcMem, hCloseFont);
            wchar_t closeGlyph[2] = { 0xE8BB, 0 };
            RECT rcClose = tab.rcClose;
            DrawTextW(hdcMem, closeGlyph, 1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMem, oldCloseF);
            DeleteObject(hCloseFont);
        }
    }

    // Draw '+' new tab button
    if (m_hoverNewTab) {
        HBRUSH hbrNewTab = CreateSolidBrush(Theme::Get().ClrHover());
        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldBr = SelectObject(hdcMem, hbrNewTab);
        HGDIOBJ oldP = SelectObject(hdcMem, hPen);
        RoundRect(hdcMem, m_rcNewTab.left, m_rcNewTab.top, m_rcNewTab.right, m_rcNewTab.bottom, 4, 4);
        SelectObject(hdcMem, oldP);
        SelectObject(hdcMem, oldBr);
        DeleteObject(hbrNewTab);
    }
    // Fluent '+'
    HFONT hPlusFont = Theme::Get().GetIconFont(12, m_hwnd);
    HGDIOBJ oldPlusF = SelectObject(hdcMem, hPlusFont);
    SetTextColor(hdcMem, m_hoverNewTab ? Theme::Get().ClrAccent() : Theme::Get().ClrTextPrimary());
    wchar_t plusGlyph[2] = { 0xE710, 0 };
    DrawTextW(hdcMem, plusGlyph, 1, &m_rcNewTab, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdcMem, oldPlusF);
    DeleteObject(hPlusFont);

    // Bottom border separator line
    RECT rcBorder = { 0, h - 1, w, h };
    HBRUSH hbrBorder = Theme::Get().BrushBorder();
    FillRect(hdcMem, &rcBorder, hbrBorder);

    SelectObject(hdcMem, oldFont);
    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
}

LRESULT TabBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Draw(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);

        int oldHover = m_hoverIndex;
        bool oldClose = m_hoverClose;
        bool oldNewTab = m_hoverNewTab;

        m_hoverIndex = -1;
        m_hoverClose = false;
        m_hoverNewTab = PtInRect(&m_rcNewTab, pt);

        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                m_hoverIndex = (int)i;
                m_hoverClose = PtInRect(&m_tabs[i].rcClose, pt);
                break;
            }
        }

        if (m_dragging && wParam & MK_LBUTTON) {
            // Drag reorder
            if (m_hoverIndex != -1 && m_hoverIndex != m_dragIndex) {
                std::swap(m_tabs[m_dragIndex], m_tabs[m_hoverIndex]);
                if (m_activeIndex == m_dragIndex) {
                    m_activeIndex = m_hoverIndex;
                } else if (m_activeIndex == m_hoverIndex) {
                    m_activeIndex = m_dragIndex;
                }
                m_dragIndex = m_hoverIndex;
                RecalcLayout();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        if (oldHover != m_hoverIndex || oldClose != m_hoverClose || oldNewTab != m_hoverNewTab) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        m_hoverIndex = -1;
        m_hoverClose = false;
        m_hoverNewTab = false;
        m_dragging = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&m_rcNewTab, pt)) {
            if (onNewTabRequested) onNewTabRequested();
            return 0;
        }

        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                if (PtInRect(&m_tabs[i].rcClose, pt)) {
                    CloseTab((int)i);
                    return 0;
                } else {
                    SetActiveTab((int)i);
                    m_dragging = true;
                    m_dragIndex = (int)i;
                    m_dragStart = pt;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (m_dragging) {
            m_dragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                POINT ptScreen = pt;
                ClientToScreen(hwnd, &ptScreen);
                ShowTabContextMenu((int)i, ptScreen);
                return 0;
            }
        }
        // Clicked on empty space in TabBar: show TabBar background context menu!
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);
        HMENU hMenu = CreatePopupMenu();
        bool isZh = I18n::Get().IsChinese();
        AppendMenuW(hMenu, MF_STRING, 1, isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
        AppendMenuW(hMenu, MF_STRING | (m_closedHistory.empty() ? MF_GRAYED : 0), 2, isZh ? L"恢复关闭的标签页(&R)\tCtrl+Shift+T" : L"Reopen Closed Tab(&R)\tCtrl+Shift+T");
        AppendMenuW(hMenu, MF_STRING, 3, isZh ? L"关闭其他标签页(&O)" : L"Close Other Tabs(&O)");
        AppendMenuW(hMenu, MF_STRING, 4, isZh ? L"关闭当前标签页(&W)\tCtrl+W" : L"Close Current Tab(&W)\tCtrl+W");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 5, isZh ? L"在此处打开命令提示符 CMD(&C)" : L"Open CMD Here(&C)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 6, isZh ? L"NeoMDIE 设置(&S)..." : L"NeoMDIE Settings(&S)...");

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);

        switch (cmd) {
        case 1: AddTab(L"This PC", true); break;
        case 2: ReopenClosedTab(); break;
        case 3: CloseOtherTabs(m_activeIndex); break;
        case 4: CloseTab(m_activeIndex); break;
        case 5:
            if (m_activeIndex >= 0 && m_activeIndex < (int)m_tabs.size()) {
                ShellUtils::LaunchTerminal(m_tabs[m_activeIndex].path);
            }
            break;
        case 6:
            PostMessageW(GetAncestor(hwnd, GA_ROOT), WM_COMMAND, MAKEWPARAM(40056, 0), 0); // ID_TOOLS_SETTINGS
            break;
        }
        return 0;
    }

    case WM_MBUTTONUP: {
        // Middle click to close tab
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                CloseTab((int)i);
                return 0;
            }
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        // Double click empty space to open new tab
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool onTab = false;
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                onTab = true;
                break;
            }
        }
        if (!onTab && onNewTabRequested) {
            onNewTabRequested();
        }
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TabBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TabBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (TabBar*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (TabBar*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
