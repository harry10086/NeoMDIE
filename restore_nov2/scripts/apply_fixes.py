import os
import re

print("Applying NeoMDIE enhancements...")

# 1. Update DirectoryTree.cpp
with open("src/ui/DirectoryTree.cpp", "r", encoding="utf-8") as f:
    tree_code = f.read()

old_rbutton = """    case WM_RBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TVHITTESTINFO tvhti = { 0 };
        tvhti.pt = pt;
        HTREEITEM hHit = TreeView_HitTest(hwnd, &tvhti);
        if (!hHit) hHit = tvhti.hItem;
        if (hHit) {
            TreeView_SelectItem(hwnd, hHit);
        }
        return 0;
    }

    case WM_RBUTTONUP:
        return 0;"""

new_rbutton = """    case WM_RBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TVHITTESTINFO tvhti = { 0 };
        tvhti.pt = pt;
        HTREEITEM hHit = TreeView_HitTest(hwnd, &tvhti);
        if (!hHit) hHit = tvhti.hItem;
        if (hHit) {
            bool oldSync = pThis->m_syncing;
            pThis->m_syncing = true; // Prevent right-click selection from navigating
            TreeView_SelectItem(hwnd, hHit);
            pThis->m_syncing = oldSync;
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);
        SendMessageW(hwnd, WM_CONTEXTMENU, (WPARAM)hwnd, MAKELPARAM(ptScreen.x, ptScreen.y));
        return 0;
    }"""

tree_code = tree_code.replace(old_rbutton.replace("\r\n", "\n"), new_rbutton.replace("\r\n", "\n"))
with open("src/ui/DirectoryTree.cpp", "w", encoding="utf-8") as f:
    f.write(tree_code)
print("Updated DirectoryTree.cpp")

# 2. Update LinksBar.cpp
with open("src/ui/LinksBar.cpp", "r", encoding="utf-8") as f:
    links_code = f.read()

old_links_draw = """            std::wstring itemText = L"\\U0001F4C1 " + m_links[i].name;

            SIZE sz;
            GetTextExtentPoint32W(hdc, itemText.c_str(), (int)itemText.size(), &sz);
            int padX = Theme::Get().Scale(10, hwnd);
            int itemW = sz.cx + padX * 2 + Theme::Get().Scale(8, hwnd);

            RECT rcItem = { curX, Theme::Get().Scale(2, hwnd), curX + itemW, rc.bottom - Theme::Get().Scale(3, hwnd) };
            m_itemRects.push_back(rcItem);

            if ((int)i == m_hoverIndex) {
                HBRUSH hbrHover = Theme::Get().BrushHover();
                FillRect(hdc, &rcItem, hbrHover);
            }

            COLORREF textColor = (m_links[i].color != 0) ? m_links[i].color : Theme::Get().ClrTextPrimary();
            SetTextColor(hdc, textColor);
            DrawTextW(hdc, itemText.c_str(), -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            curX += itemW + Theme::Get().Scale(4, hwnd);"""

new_links_draw = """            const std::wstring& itemText = m_links[i].name;

            SIZE sz;
            GetTextExtentPoint32W(hdc, itemText.c_str(), (int)itemText.size(), &sz);
            int padX = Theme::Get().Scale(10, hwnd);
            int itemW = sz.cx + padX * 2;

            RECT rcItem = { curX, Theme::Get().Scale(2, hwnd), curX + itemW, rc.bottom - Theme::Get().Scale(3, hwnd) };
            m_itemRects.push_back(rcItem);

            if ((int)i == m_hoverIndex) {
                HBRUSH hbrHover = Theme::Get().BrushHover();
                FillRect(hdc, &rcItem, hbrHover);
            }

            COLORREF textColor = (m_links[i].color != 0) ? m_links[i].color : Theme::Get().ClrTextPrimary();
            SetTextColor(hdc, textColor);
            DrawTextW(hdc, itemText.c_str(), -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            curX += itemW + Theme::Get().Scale(4, hwnd);"""

links_code = links_code.replace(old_links_draw.replace("\r\n", "\n"), new_links_draw.replace("\r\n", "\n"))
with open("src/ui/LinksBar.cpp", "w", encoding="utf-8") as f:
    f.write(links_code)
print("Updated LinksBar.cpp")

# 3. Update FilterBar.cpp for dark mode edit control
with open("src/ui/FilterBar.cpp", "r", encoding="utf-8") as f:
    filter_code = f.read()

old_filter_msg = """    case WM_ERASEBKGND:
        return 1;
    }"""

new_filter_msg = """    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());
        SetBkColor(hdc, Theme::Get().ClrSurface());
        return (INT_PTR)Theme::Get().BrushSurface();
    }
    case WM_ERASEBKGND:
        return 1;
    }"""

filter_code = filter_code.replace(old_filter_msg.replace("\r\n", "\n"), new_filter_msg.replace("\r\n", "\n"))
with open("src/ui/FilterBar.cpp", "w", encoding="utf-8") as f:
    f.write(filter_code)
print("Updated FilterBar.cpp")

# 4. Update BreadcrumbBar.cpp for dark mode edit control
with open("src/ui/BreadcrumbBar.cpp", "r", encoding="utf-8") as f:
    crumb_code = f.read()

old_crumb_msg = """    case WM_ERASEBKGND:
        return 1;
    }"""

new_crumb_msg = """    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());
        SetBkColor(hdc, Theme::Get().ClrSurface());
        return (INT_PTR)Theme::Get().BrushSurface();
    }
    case WM_ERASEBKGND:
        return 1;
    }"""

crumb_code = crumb_code.replace(old_crumb_msg.replace("\r\n", "\n"), new_crumb_msg.replace("\r\n", "\n"))
with open("src/ui/BreadcrumbBar.cpp", "w", encoding="utf-8") as f:
    f.write(crumb_code)
print("Updated BreadcrumbBar.cpp")
