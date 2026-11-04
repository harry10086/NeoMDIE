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
        SendMessageW(m_hwndEdit, EM_SETCUEBANNER, TRUE, (LPARAM)(isZh ? L"🔍 搜索过滤..." : L"🔍 Search & Filter..."));
    }
}

void FilterBar::Clear() {
    SetWindowTextW(m_hwndEdit, L"");
}

void FilterBar::SetText(const std::wstring& text) {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, text.c_str());
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
        int optW = Theme::Get().Scale(26, m_hwnd);
        int editW = width - optW - Theme::Get().Scale(10, m_hwnd);
        int editH = height - Theme::Get().Scale(8, m_hwnd);
        if (m_hwndEdit) {
            MoveWindow(m_hwndEdit, Theme::Get().Scale(6, m_hwnd), Theme::Get().Scale(4, m_hwnd),
                       editW, editH, TRUE);
        }
        if (m_hwndOptBtn) {
            MoveWindow(m_hwndOptBtn, width - optW - Theme::Get().Scale(4, m_hwnd),
                       Theme::Get().Scale(3, m_hwnd), optW, height - Theme::Get().Scale(6, m_hwnd), TRUE);
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
            if (pThis->onFilterChanged) {
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
            HBRUSH hbg = isDown ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface();
            FillRect(pdis->hDC, &pdis->rcItem, hbg);

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
        HBRUSH hbr = Theme::Get().BrushSurface();
        FillRect(hdc, &rc, hbr);

        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);
        SelectObject(hdc, oldPen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());
        SetBkColor(hdc, Theme::Get().ClrSurface());
        return (INT_PTR)Theme::Get().BrushSurface();
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
