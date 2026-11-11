#include "FilterBar.h"
#include "Theme.h"
#include "..\core\I18n.h"

static const wchar_t* FILTERBAR_CLASS = L"NeoMDIE_FilterBar";

FilterBar::FilterBar() {
}

FilterBar::~FilterBar() {
}

HWND FilterBar::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, FILTERBAR_CLASS, &wc)) {
        wc.lpfnWndProc = FilterBar::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = FILTERBAR_CLASS;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, FILTERBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 220, 30,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    m_hwndEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 180, 22,
        m_hwnd, (HMENU)101, hInst, nullptr
    );

    m_hwndOptBtn = CreateWindowExW(
        0, L"BUTTON", L"⚙",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 24, 22,
        m_hwnd, (HMENU)102, hInst, nullptr
    );

    Theme::Get().ApplyToWindow(m_hwndOptBtn);
    SendMessageW(m_hwndOptBtn, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    SetWindowSubclass(m_hwndEdit, FilterBar::EditSubclassProc, 1, (DWORD_PTR)this);
    UpdateLanguage();
    SendMessageW(m_hwndEdit, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    return m_hwnd;
}

void FilterBar::UpdateLanguage() {
    if (m_hwndEdit) {
        bool isZh = I18n::Get().IsChinese();
        SendMessageW(m_hwndEdit, EM_SETCUEBANNER, TRUE, (LPARAM)(isZh ? L"搜索过滤..." : L"Search & Filter..."));
    }
}

void FilterBar::Clear() {
    SetWindowTextW(m_hwndEdit, L"");
}

void FilterBar::SetText(const std::wstring& text) {
    if (m_hwndEdit) {
        m_suppressNotify = true;
        SetWindowTextW(m_hwndEdit, text.c_str());
        m_suppressNotify = false;
    }
}

void FilterBar::SetFocus() {
    ::SetFocus(m_hwndEdit);
    SendMessageW(m_hwndEdit, EM_SETSEL, 0, -1);
}

std::wstring FilterBar::GetText() const {
    wchar_t buf[256];
    GetWindowTextW(m_hwndEdit, buf, 256);
    return buf;
}

void FilterBar::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
        int optW = Theme::Get().Scale(24, m_hwnd);
        int padLeft = Theme::Get().Scale(28, m_hwnd);
        int editW = width - optW - padLeft - Theme::Get().Scale(6, m_hwnd);

        int fontH = Theme::Get().Scale(16, m_hwnd);
        if (m_hwndEdit) {
            HDC hdc = GetDC(m_hwndEdit);
            HFONT hFont = (HFONT)SendMessageW(m_hwndEdit, WM_GETFONT, 0, 0);
            if (!hFont) hFont = Theme::Get().GetUIFont(m_hwnd);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);
            TEXTMETRIC tm;
            if (GetTextMetrics(hdc, &tm)) {
                fontH = tm.tmHeight;
            }
            SelectObject(hdc, oldFont);
            ReleaseDC(m_hwndEdit, hdc);
        }

        int editH = fontH;
        int editY = (height - editH) / 2;
        if (m_hwndEdit) {
            MoveWindow(m_hwndEdit, padLeft, editY, editW, editH, TRUE);
        }
        if (m_hwndOptBtn) {
            int optH = height - Theme::Get().Scale(6, m_hwnd);
            int optY = (height - optH) / 2;
            MoveWindow(m_hwndOptBtn, width - optW - Theme::Get().Scale(4, m_hwnd),
                       optY, optW, optH, TRUE);
        }
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void FilterBar::ShowOptionsMenu() {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING | (m_searchSubfolders ? MF_CHECKED : MF_UNCHECKED),
                1, isZh ? L"包含子文件夹 (递归搜索)" : L"Include Subfolders (Recursive)");
    AppendMenuW(hMenu, MF_STRING | (m_matchCase ? MF_CHECKED : MF_UNCHECKED),
                2, isZh ? L"区分大小写" : L"Match Case");

    RECT rcBtn;
    GetWindowRect(m_hwndOptBtn, &rcBtn);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, rcBtn.left, rcBtn.bottom, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 1) {
        m_searchSubfolders = !m_searchSubfolders;
        if (onFilterChanged) {
            onFilterChanged(GetText(), m_searchSubfolders, m_matchCase);
        }
    } else if (cmd == 2) {
        m_matchCase = !m_matchCase;
        if (onFilterChanged) {
            onFilterChanged(GetText(), m_searchSubfolders, m_matchCase);
        }
    }
}

LRESULT CALLBACK FilterBar::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FilterBar* pThis = (FilterBar*)dwRefData;
    switch (msg) {
    case WM_SETFOCUS:
        pThis->m_isFocused = true;
        InvalidateRect(pThis->m_hwnd, nullptr, TRUE);
        break;
    case WM_KILLFOCUS:
        pThis->m_isFocused = false;
        InvalidateRect(pThis->m_hwnd, nullptr, TRUE);
        break;
    case WM_KEYUP: {
        if (wParam == VK_ESCAPE) {
            pThis->Clear();
            if (pThis->onEscapePressed) pThis->onEscapePressed();
            return 0;
        }
        if (pThis->onFilterChanged) {
            pThis->onFilterChanged(pThis->GetText(), pThis->m_searchSubfolders, pThis->m_matchCase);
        }
        break;
    }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK FilterBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FilterBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (FilterBar*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (FilterBar*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == 102 && pThis) {
            pThis->ShowOptionsMenu();
            return 0;
        } else if (id == 101 && code == EN_CHANGE && pThis) {
            if (!pThis->m_suppressNotify && pThis->onFilterChanged) {
                pThis->onFilterChanged(pThis->GetText(), pThis->m_searchSubfolders, pThis->m_matchCase);
            }
            return 0;
        }
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis && pdis->CtlID == 102) {
            bool isDown = (pdis->itemState & ODS_SELECTED);
            COLORREF boxBg = Theme::Get().IsDarkMode() ? RGB(32, 33, 36) : RGB(241, 243, 244);
            HBRUSH hClearBg = CreateSolidBrush(boxBg);
            FillRect(pdis->hDC, &pdis->rcItem, hClearBg);
            DeleteObject(hClearBg);

            if (isDown) {
                HBRUSH hbrDown = Theme::Get().BrushSelected();
                HPEN hPen = Theme::Get().PenBorder();
                HGDIOBJ oldBrush = SelectObject(pdis->hDC, hbrDown);
                HGDIOBJ oldPen = SelectObject(pdis->hDC, hPen);
                int rad = Theme::Get().Scale(6, hwnd);
                RoundRect(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom, rad, rad);
                SelectObject(pdis->hDC, oldBrush);
                SelectObject(pdis->hDC, oldPen);
            }

            SetBkMode(pdis->hDC, TRANSPARENT);
            SetTextColor(pdis->hDC, Theme::Get().ClrTextPrimary());
            HFONT hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(pdis->hDC, hFont);

            DrawTextW(pdis->hDC, L"⚙", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(pdis->hDC, oldFont);
            return TRUE;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushSurface());

        int rad = Theme::Get().Scale(10, hwnd);
        RECT rcBox = { 0, 0, rc.right - 1, rc.bottom - 1 };
        COLORREF boxBg = Theme::Get().IsDarkMode() ? RGB(32, 33, 36) : RGB(241, 243, 244);
        HBRUSH hbrBox = CreateSolidBrush(boxBg);
        HPEN hPen = pThis->m_isFocused ? CreatePen(PS_SOLID, 2, Theme::Get().ClrAccent()) : Theme::Get().PenBorder();

        HGDIOBJ oldBrush = SelectObject(hdc, hbrBox);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        RoundRect(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, rad, rad);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(hbrBox);
        if (pThis->m_isFocused) DeleteObject(hPen);

        // Draw crisp modern Search magnifying glass icon (Segoe Fluent / MDL2: 0xE721)
        int iconX = Theme::Get().Scale(8, hwnd);
        int iconW = Theme::Get().Scale(16, hwnd);
        RECT rcIcon = { iconX, 0, iconX + iconW, rc.bottom };
        HFONT hIconFont = Theme::Get().GetIconFont(12, hwnd);
        HGDIOBJ oldIconFont = SelectObject(hdc, hIconFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, pThis->m_isFocused ? Theme::Get().ClrAccent() : Theme::Get().ClrTextSecondary());
        wchar_t searchGlyph[2] = { (wchar_t)0xE721, 0 };
        DrawTextW(hdc, searchGlyph, 1, &rcIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldIconFont);
        DeleteObject(hIconFont);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdcEdit = (HDC)wParam;
        COLORREF boxBg = Theme::Get().IsDarkMode() ? RGB(32, 33, 36) : RGB(241, 243, 244);
        SetTextColor(hdcEdit, Theme::Get().ClrTextPrimary());
        SetBkColor(hdcEdit, boxBg);
        static HBRUSH s_hbrSearchEdit = nullptr;
        if (s_hbrSearchEdit) DeleteObject(s_hbrSearchEdit);
        s_hbrSearchEdit = CreateSolidBrush(boxBg);
        return (INT_PTR)s_hbrSearchEdit;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushSurface());
        return 1;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
