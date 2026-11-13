#include "LinksBar.h"
#include <commdlg.h>
#include "Theme.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"

static const wchar_t* LINKSBAR_CLASS = L"NeoMDIE_LinksBar";

LinksBar::LinksBar() {
}

LinksBar::~LinksBar() {
    if (m_hwndTooltip && IsWindow(m_hwndTooltip)) {
        DestroyWindow(m_hwndTooltip);
        m_hwndTooltip = nullptr;
    }
}

HWND LinksBar::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, LINKSBAR_CLASS, &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = LinksBar::WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = Theme::Get().BrushSurface();
        wc.lpszClassName = LINKSBAR_CLASS;
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, LINKSBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 100, 28,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    // Tooltip control for displaying full path on hover
    m_hwndTooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        m_hwnd, nullptr, hInst, nullptr
    );
    SetWindowPos(m_hwndTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    LoadLinks();
    return m_hwnd;
}

void LinksBar::LoadLinks() {
    m_links = Config::Get().links;
}

void LinksBar::SaveLinks() {
    Config::Get().links = m_links;
    Config::Get().Save();
}

void LinksBar::AddLink(const std::wstring& name, const std::wstring& path) {
    if (path.empty()) return;
    std::wstring displayName = name;
    if (displayName.empty()) {
        displayName = ShellUtils::GetDisplayName(path);
    }
    m_links.push_back({ displayName, path, 0 });
    SaveLinks();
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

void LinksBar::RemoveLink(int index) {
    if (index >= 0 && index < (int)m_links.size()) {
        m_links.erase(m_links.begin() + index);
        SaveLinks();
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void LinksBar::SetLinkColor(int index, COLORREF color) {
    if (index >= 0 && index < (int)m_links.size()) {
        m_links[index].color = color;
        SaveLinks();
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void LinksBar::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void LinksBar::ScrollBy(int delta) {
    int oldOffset = m_scrollOffset;
    m_scrollOffset = std::clamp(m_scrollOffset + delta, 0, m_maxScroll);
    if (m_scrollOffset != oldOffset && m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void LinksBar::UpdateTooltip(int index) {
    if (!m_hwndTooltip) return;

    TOOLINFOW ti = { sizeof(ti) };
    ti.hwnd = m_hwnd;
    ti.uId = 1;

    if (index == -2) {
        ti.uFlags = TTF_SUBCLASS;
        std::wstring tip = I18n::Get().IsChinese() ? L"快捷链接" : L"Quick Links";
        ti.lpszText = (LPWSTR)tip.c_str();
        ti.rect = m_rcIcon;

        SendMessageW(m_hwndTooltip, TTM_DELTOOLW, 0, (LPARAM)&ti);
        SendMessageW(m_hwndTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    } else if (index >= 0 && index < (int)m_links.size()) {
        ti.uFlags = TTF_SUBCLASS;
        ti.lpszText = (LPWSTR)m_links[index].path.c_str();
        ti.rect = m_itemRects[index];

        SendMessageW(m_hwndTooltip, TTM_DELTOOLW, 0, (LPARAM)&ti);
        SendMessageW(m_hwndTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    } else {
        SendMessageW(m_hwndTooltip, TTM_DELTOOLW, 0, (LPARAM)&ti);
    }
}

LRESULT LinksBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

        HBRUSH hbrBg = Theme::Get().BrushSurface();
        FillRect(hdcMem, &rc, hbrBg);

        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdcMem, hPen);
        MoveToEx(hdcMem, 0, rc.bottom - 1, nullptr);
        LineTo(hdcMem, rc.right, rc.bottom - 1);
        SelectObject(hdcMem, oldPen);

        SetBkMode(hdcMem, TRANSPARENT);
        HFONT hFont = Theme::Get().GetUIFont(hwnd);
        HGDIOBJ oldFont = SelectObject(hdcMem, hFont);

        // Draw Shortcut Links Icon instead of text " 链接: "
        int iconW = Theme::Get().Scale(28, hwnd);
        m_rcIcon = { Theme::Get().Scale(6, hwnd), 0, Theme::Get().Scale(6, hwnd) + iconW, rc.bottom - 1 };

        if (m_hoverIcon) {
            RECT rcHover = { m_rcIcon.left, Theme::Get().Scale(2, hwnd), m_rcIcon.right, m_rcIcon.bottom - Theme::Get().Scale(2, hwnd) };
            int rad = Theme::Get().Scale(6, hwnd);
            Theme::Get().FillSmoothRoundRect(hdcMem, rcHover, rad, Theme::Get().ClrHover());
        }

        HFONT hIconFont = Theme::Get().GetIconFont(16, hwnd);
        HGDIOBJ oldIconFont = SelectObject(hdcMem, hIconFont);
        SetTextColor(hdcMem, m_hoverIcon ? Theme::Get().ClrAccent() : Theme::Get().ClrTextSecondary());
        wchar_t linkIcon[2] = { (wchar_t)0xE71B, 0 }; // Fluent Link icon
        DrawTextW(hdcMem, linkIcon, 1, &m_rcIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdcMem, oldIconFont);
        DeleteObject(hIconFont);

        int startX = m_rcIcon.right + Theme::Get().Scale(4, hwnd);
        int padX = Theme::Get().Scale(10, hwnd);
        int spacing = Theme::Get().Scale(4, hwnd);

        // Pre-measure all link pill widths
        int totalContentW = 0;
        std::vector<int> itemWidths(m_links.size(), 0);
        for (size_t i = 0; i < m_links.size(); ++i) {
            SIZE sz;
            GetTextExtentPoint32W(hdcMem, m_links[i].name.c_str(), (int)m_links[i].name.size(), &sz);
            itemWidths[i] = sz.cx + padX * 2;
            totalContentW += itemWidths[i] + spacing;
        }

        int btnW = Theme::Get().Scale(20, hwnd);
        int btnH = h - Theme::Get().Scale(6, hwnd);
        int btnY = Theme::Get().Scale(3, hwnd);

        int rightReservedW = 0;
        int availableW = w - startX;

        if (totalContentW > availableW) {
            m_showScrollButtons = true;
            rightReservedW = btnW * 2 + Theme::Get().Scale(10, hwnd);
            int visibleRight = w - rightReservedW;
            m_maxScroll = (std::max)(0, totalContentW - (visibleRight - startX));
            m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

            m_rcScrollRight = { w - btnW - Theme::Get().Scale(4, hwnd), btnY, w - Theme::Get().Scale(4, hwnd), btnY + btnH };
            m_rcScrollLeft = { m_rcScrollRight.left - btnW - Theme::Get().Scale(2, hwnd), btnY, m_rcScrollRight.left - Theme::Get().Scale(2, hwnd), btnY + btnH };
        } else {
            m_showScrollButtons = false;
            m_scrollOffset = 0;
            m_maxScroll = 0;
            SetRectEmpty(&m_rcScrollLeft);
            SetRectEmpty(&m_rcScrollRight);
        }

        int visibleRight = w - rightReservedW;

        // Clip region for links
        HRGN hClip = CreateRectRgn(startX, 0, visibleRight, h);
        SelectClipRgn(hdcMem, hClip);
        DeleteObject(hClip);

        int curX = startX - m_scrollOffset;
        m_itemRects.clear();

        for (size_t i = 0; i < m_links.size(); ++i) {
            int itemW = itemWidths[i];
            RECT rcItem = { curX, Theme::Get().Scale(2, hwnd), curX + itemW, rc.bottom - Theme::Get().Scale(3, hwnd) };
            m_itemRects.push_back(rcItem);

            int rad = Theme::Get().Scale(8, hwnd);
            if ((m_dragging && (int)i == m_dragIndex) || ((int)i == m_pressedIndex)) {
                Theme::Get().DrawSmoothRoundRect(hdcMem, rcItem, rad, Theme::Get().ClrSelected(), Theme::Get().ClrBorder(), 1.0f);
            } else if ((int)i == m_hoverIndex) {
                Theme::Get().DrawSmoothRoundRect(hdcMem, rcItem, rad, Theme::Get().ClrHover(), Theme::Get().ClrBorder(), 1.0f);
            }

            COLORREF textColor = (m_links[i].color != 0) ? m_links[i].color : Theme::Get().ClrTextPrimary();
            SetTextColor(hdcMem, textColor);
            DrawTextW(hdcMem, m_links[i].name.c_str(), -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            curX += itemW + spacing;
        }

        // Restore clip region before drawing buttons
        SelectClipRgn(hdcMem, nullptr);

        // Draw scroll buttons if overflowing
        if (m_showScrollButtons) {
            RECT rcScrollBg = { visibleRight, 0, w, h - 1 };
            HBRUSH hbrBgRight = Theme::Get().BrushSurface();
            FillRect(hdcMem, &rcScrollBg, hbrBgRight);

            // Subtle separator line before buttons
            HPEN hPenSep = Theme::Get().PenBorder();
            HGDIOBJ oldPSep = SelectObject(hdcMem, hPenSep);
            MoveToEx(hdcMem, rcScrollBg.left, Theme::Get().Scale(4, hwnd), nullptr);
            LineTo(hdcMem, rcScrollBg.left, h - Theme::Get().Scale(4, hwnd));
            SelectObject(hdcMem, oldPSep);

            HFONT hIconFont = Theme::Get().GetIconFont(11, hwnd);
            HGDIOBJ oldIconF = SelectObject(hdcMem, hIconFont);

            // Left button '<'
            bool leftCanScroll = (m_scrollOffset > 0);
            if (m_hoverScrollLeft && leftCanScroll) {
                HBRUSH hbrHover = CreateSolidBrush(Theme::Get().ClrHover());
                FillRect(hdcMem, &m_rcScrollLeft, hbrHover);
                DeleteObject(hbrHover);
            }
            SetTextColor(hdcMem, leftCanScroll ? (m_hoverScrollLeft ? Theme::Get().ClrAccent() : Theme::Get().ClrTextPrimary())
                                               : Theme::Get().ClrTextSecondary());
            wchar_t leftGlyph[2] = { 0xE76B, 0 }; // ChevronLeft
            DrawTextW(hdcMem, leftGlyph, 1, &m_rcScrollLeft, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Right button '>'
            bool rightCanScroll = (m_scrollOffset < m_maxScroll);
            if (m_hoverScrollRight && rightCanScroll) {
                HBRUSH hbrHover = CreateSolidBrush(Theme::Get().ClrHover());
                FillRect(hdcMem, &m_rcScrollRight, hbrHover);
                DeleteObject(hbrHover);
            }
            SetTextColor(hdcMem, rightCanScroll ? (m_hoverScrollRight ? Theme::Get().ClrAccent() : Theme::Get().ClrTextPrimary())
                                                 : Theme::Get().ClrTextSecondary());
            wchar_t rightGlyph[2] = { 0xE76C, 0 }; // ChevronRight
            DrawTextW(hdcMem, rightGlyph, 1, &m_rcScrollRight, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdcMem, oldIconF);
            DeleteObject(hIconFont);
        }

        BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, oldFont);
        SelectObject(hdcMem, oldBm);
        DeleteObject(hbm);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (!m_showScrollButtons) return 0;
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int step = Theme::Get().Scale(60, hwnd);
        ScrollBy(delta < 0 ? step : -step);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT pt = { x, y };

        if (m_showScrollButtons) {
            if (PtInRect(&m_rcScrollLeft, pt)) {
                ScrollBy(-Theme::Get().Scale(80, hwnd));
                return 0;
            }
            if (PtInRect(&m_rcScrollRight, pt)) {
                ScrollBy(Theme::Get().Scale(80, hwnd));
                return 0;
            }
        }

        int visibleRight = m_showScrollButtons ? m_rcScrollLeft.left - Theme::Get().Scale(4, hwnd) : 100000;
        if (x < visibleRight) {
            for (size_t i = 0; i < m_itemRects.size(); ++i) {
                if (PtInRect(&m_itemRects[i], pt)) {
                    m_pressedIndex = (int)i;
                    m_dragPotential = true;
                    m_dragging = false;
                    m_dragIndex = (int)i;
                    m_dragStart = pt;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &m_itemRects[i], TRUE);
                    return 0;
                }
            }
        }
        break;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT pt = { x, y };

        if (m_showScrollButtons) {
            bool oldHoverL = m_hoverScrollLeft;
            bool oldHoverR = m_hoverScrollRight;
            m_hoverScrollLeft = PtInRect(&m_rcScrollLeft, pt);
            m_hoverScrollRight = PtInRect(&m_rcScrollRight, pt);
            if (oldHoverL != m_hoverScrollLeft || oldHoverR != m_hoverScrollRight) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        if (m_dragPotential && (wParam & MK_LBUTTON)) {
            int dx = abs(x - m_dragStart.x);
            int dy = abs(y - m_dragStart.y);
            if (dx > 4 || dy > 4) {
                m_dragging = true;
                m_dragPotential = false;
            }
        }

        int visibleRight = m_showScrollButtons ? m_rcScrollLeft.left - Theme::Get().Scale(4, hwnd) : 100000;

        if (m_dragging && (wParam & MK_LBUTTON)) {
            int targetIndex = -1;
            if (x < visibleRight) {
                for (size_t i = 0; i < m_itemRects.size(); ++i) {
                    if (PtInRect(&m_itemRects[i], pt)) {
                        targetIndex = (int)i;
                        break;
                    }
                }
            }

            if (targetIndex != -1 && targetIndex != m_dragIndex &&
                targetIndex < (int)m_links.size() && m_dragIndex < (int)m_links.size()) {
                std::swap(m_links[m_dragIndex], m_links[targetIndex]);
                m_dragIndex = targetIndex;
                m_hoverIndex = targetIndex;
                SaveLinks();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }

        bool isIconHover = PtInRect(&m_rcIcon, pt);
        if (isIconHover != m_hoverIcon) {
            m_hoverIcon = isIconHover;
            InvalidateRect(hwnd, &m_rcIcon, FALSE);
        }

        int newHover = -1;
        if (isIconHover) {
            newHover = -2;
        } else if (x < visibleRight) {
            for (size_t i = 0; i < m_itemRects.size(); ++i) {
                if (PtInRect(&m_itemRects[i], pt)) {
                    newHover = (int)i;
                    break;
                }
            }
        }

        if (newHover != m_hoverIndex) {
            m_hoverIndex = newHover;
            UpdateTooltip(newHover);
            InvalidateRect(hwnd, nullptr, FALSE);

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        m_hoverScrollLeft = false;
        m_hoverScrollRight = false;
        if (m_hoverIcon) {
            m_hoverIcon = false;
            InvalidateRect(hwnd, &m_rcIcon, FALSE);
        }
        if (!m_dragging) {
            m_hoverIndex = -1;
            UpdateTooltip(-1);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CAPTURECHANGED: {
        m_dragging = false;
        m_dragPotential = false;
        m_dragIndex = -1;
        m_pressedIndex = -1;
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT pt = { x, y };

        bool wasDragging = m_dragging;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        m_dragging = false;
        m_dragPotential = false;
        m_pressedIndex = -1;

        if (wasDragging) {
            m_dragIndex = -1;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        m_dragIndex = -1;

        int visibleRight = m_showScrollButtons ? m_rcScrollLeft.left - Theme::Get().Scale(4, hwnd) : 100000;
        if (x < visibleRight) {
            for (size_t i = 0; i < m_itemRects.size(); ++i) {
                if (PtInRect(&m_itemRects[i], { x, y })) {
                    ActionTrigger trigger = GetCurrentTrigger();
                    WindowAction act = Config::Get().actionLinks.ResolveAction(trigger);
                    if (act == WindowAction::CurrentPane && onNavigate) onNavigate(m_links[i].path);
                    else if (act == WindowAction::OtherPane && onOpenSplit) onOpenSplit(m_links[i].path);
                    else if (act == WindowAction::NewTab && onOpenNewTab) onOpenNewTab(m_links[i].path);
                    else if (act == WindowAction::BgTab && onOpenBgTab) onOpenBgTab(m_links[i].path);
                    else if (act == WindowAction::Explorer) ShellExecuteW(nullptr, L"explore", m_links[i].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
            }
        }
        return 0;
    }

    case WM_MBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int visibleRight = m_showScrollButtons ? m_rcScrollLeft.left - Theme::Get().Scale(4, hwnd) : 100000;
        if (x < visibleRight) {
            for (size_t i = 0; i < m_itemRects.size(); ++i) {
                if (PtInRect(&m_itemRects[i], { x, y })) {
                    ActionTrigger trigger = GetCurrentTrigger(true, false);
                    WindowAction act = Config::Get().actionLinks.ResolveAction(trigger);
                    if (act == WindowAction::CurrentPane && onNavigate) onNavigate(m_links[i].path);
                    else if (act == WindowAction::OtherPane && onOpenSplit) onOpenSplit(m_links[i].path);
                    else if (act == WindowAction::NewTab && onOpenNewTab) onOpenNewTab(m_links[i].path);
                    else if (act == WindowAction::BgTab && onOpenBgTab) onOpenBgTab(m_links[i].path);
                    else if (act == WindowAction::Explorer) ShellExecuteW(nullptr, L"explore", m_links[i].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
            }
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        if (GetCapture() == hwnd) ReleaseCapture();
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT ptScreen = { x, y };
        ClientToScreen(hwnd, &ptScreen);

        int visibleRight = m_showScrollButtons ? m_rcScrollLeft.left - Theme::Get().Scale(4, hwnd) : 100000;
        int hitIndex = -1;
        if (x < visibleRight) {
            for (size_t i = 0; i < m_itemRects.size(); ++i) {
                if (PtInRect(&m_itemRects[i], { x, y })) {
                    hitIndex = (int)i;
                    break;
                }
            }
        }

        bool isZh = I18n::Get().IsChinese();
        HMENU hMenu = CreatePopupMenu();

        if (hitIndex >= 0) {
            AppendMenuW(hMenu, MF_STRING, 1, isZh ? L"打开(&O)" : L"Open(&O)");
            AppendMenuW(hMenu, MF_STRING, 2, isZh ? L"在新标签页中打开(&T)" : L"Open in New Tab(&T)");
            AppendMenuW(hMenu, MF_STRING, 7, isZh ? L"在后台新标签页中打开(&B)" : L"Open in Background Tab(&B)");
            AppendMenuW(hMenu, MF_STRING, 3, isZh ? L"在新栏中打开(&S)" : L"Open in New Pane(&S)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4, isZh ? L"复制路径(&C)" : L"Copy Path(&C)");
            AppendMenuW(hMenu, MF_STRING, 6, isZh ? L"自定义文字颜色(&K)..." : L"Set Custom Color(&K)...");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 5, isZh ? L"从链接栏移除(&D)" : L"Remove from Links Bar(&D)");
        } else {
            AppendMenuW(hMenu, MF_STRING, 10, isZh ? L"添加当前目录到链接栏(&A)" : L"Add Current Folder(&A)");
            AppendMenuW(hMenu, MF_STRING, 11, isZh ? L"重置为默认链接(&R)" : L"Reset to Default Links(&R)");
        }

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);

        if (hitIndex >= 0) {
            switch (cmd) {
            case 1: if (onNavigate) onNavigate(m_links[hitIndex].path); break;
            case 2: if (onOpenNewTab) onOpenNewTab(m_links[hitIndex].path); break;
            case 7: if (onOpenBgTab) onOpenBgTab(m_links[hitIndex].path); break;
            case 3: if (onOpenSplit) onOpenSplit(m_links[hitIndex].path); break;
            case 4: ShellUtils::CopyToClipboard(m_links[hitIndex].path); break;
            case 5: RemoveLink(hitIndex); break;
            case 6: {
                CHOOSECOLORW cc = { sizeof(cc) };
                cc.hwndOwner = hwnd;
                cc.lpCustColors = m_customColors;
                cc.rgbResult = m_links[hitIndex].color;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    SetLinkColor(hitIndex, cc.rgbResult);
                }
                break;
            }
            }
        } else {
            switch (cmd) {
            case 10:
                SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(9901, 0), 0);
                break;
            case 11: {
                Config::Get().links.clear();
                wchar_t userProfile[MAX_PATH];
                if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
                    Config::Get().links.push_back({ I18n::Get().T(StringId::Desktop), std::wstring(userProfile) + L"\\Desktop", 0 });
                    Config::Get().links.push_back({ I18n::Get().T(StringId::Downloads), std::wstring(userProfile) + L"\\Downloads", 0 });
                    Config::Get().links.push_back({ I18n::Get().T(StringId::Documents), std::wstring(userProfile) + L"\\Documents", 0 });
                    Config::Get().links.push_back({ I18n::Get().T(StringId::Pictures), std::wstring(userProfile) + L"\\Pictures", 0 });
                }
                Config::Get().Save();
                m_links = Config::Get().links;
                InvalidateRect(hwnd, nullptr, TRUE);
                break;
            }
            }
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK LinksBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LinksBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (LinksBar*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (LinksBar*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
