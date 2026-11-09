#include "DropIndicatorOverlay.h"
#include "Theme.h"

static const wchar_t* OVERLAY_CLASS = L"NeoMDIE_SplitDropOverlay";

DropIndicatorOverlay::DropIndicatorOverlay() {
}

DropIndicatorOverlay::~DropIndicatorOverlay() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void DropIndicatorOverlay::Create(HWND hwndOwner) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, OVERLAY_CLASS, &wc)) {
        wc.lpfnWndProc = DropIndicatorOverlay::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = OVERLAY_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        OVERLAY_CLASS, L"", WS_POPUP,
        0, 0, 0, 0,
        hwndOwner, nullptr, hInst, this
    );

    if (m_hwnd) {
        SetLayeredWindowAttributes(m_hwnd, 0, 190, LWA_ALPHA);
    }
}

void DropIndicatorOverlay::Show(const RECT& targetRect, const std::wstring& text) {
    if (!m_hwnd) return;

    int w = targetRect.right - targetRect.left;
    int h = targetRect.bottom - targetRect.top;
    if (w <= 0 || h <= 0) {
        Hide();
        return;
    }

    bool rectChanged = (m_currentRect.left != targetRect.left ||
                        m_currentRect.top != targetRect.top ||
                        m_currentRect.right != targetRect.right ||
                        m_currentRect.bottom != targetRect.bottom);
    bool textChanged = (m_text != text);

    m_currentRect = targetRect;
    m_text = text;
    m_visible = true;

    if (rectChanged || !IsWindowVisible(m_hwnd)) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, targetRect.left, targetRect.top, w, h,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }

    if (rectChanged || textChanged) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void DropIndicatorOverlay::Hide() {
    if (!m_visible) return;
    m_visible = false;
    m_currentRect = { 0, 0, 0, 0 };
    m_text.clear();
    if (m_hwnd && IsWindow(m_hwnd)) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

LRESULT CALLBACK DropIndicatorOverlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DropIndicatorOverlay* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (DropIndicatorOverlay*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (DropIndicatorOverlay*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcPaint = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w > 0 && h > 0) {
            HDC hdc = CreateCompatibleDC(hdcPaint);
            HBITMAP hbm = CreateCompatibleBitmap(hdcPaint, w, h);
            HGDIOBJ oldBm = SelectObject(hdc, hbm);

            // Fill translucent accent
            COLORREF clrAccent = Theme::Get().ClrAccent();
            HBRUSH hbrBg = CreateSolidBrush(clrAccent);
            FillRect(hdc, &rc, hbrBg);
            DeleteObject(hbrBg);

            // White border 3px
            HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, 1, 1, rc.right - 1, rc.bottom - 1);
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);

            // Draw center pill badge if text is not empty
            if (pThis && !pThis->m_text.empty()) {
                HFONT hFont = Theme::Get().GetUIFont(hwnd);
                HGDIOBJ oldFont = SelectObject(hdc, hFont);

                SIZE sz = { 0 };
                GetTextExtentPoint32W(hdc, pThis->m_text.c_str(), (int)pThis->m_text.length(), &sz);

                int padX = Theme::Get().Scale(16, hwnd);
                int padY = Theme::Get().Scale(8, hwnd);
                int bw = sz.cx + padX * 2;
                int bh = sz.cy + padY * 2;

                RECT rcBadge = {
                    (w - bw) / 2,
                    (h - bh) / 2,
                    (w + bw) / 2,
                    (h + bh) / 2
                };

                // Badge background
                HBRUSH hbrBadge = CreateSolidBrush(RGB(24, 24, 28));
                HPEN hPenBadge = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                HGDIOBJ ob1 = SelectObject(hdc, hbrBadge);
                HGDIOBJ op1 = SelectObject(hdc, hPenBadge);
                RoundRect(hdc, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 10, 10);
                SelectObject(hdc, ob1);
                SelectObject(hdc, op1);
                DeleteObject(hbrBadge);
                DeleteObject(hPenBadge);

                // Badge text
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextW(hdc, pThis->m_text.c_str(), -1, &rcBadge,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, oldFont);
            }

            BitBlt(hdcPaint, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

            SelectObject(hdc, oldBm);
            DeleteObject(hbm);
            DeleteDC(hdc);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

