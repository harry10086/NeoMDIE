#include "PreviewPane.h"
#include "Theme.h"

static const wchar_t* PREVIEWPANE_CLASS = L"NeoMDIE_PreviewPane";

PreviewPane::PreviewPane() {
}

PreviewPane::~PreviewPane() {
}

HWND PreviewPane::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, PREVIEWPANE_CLASS, &wc)) {
        wc.lpfnWndProc = PreviewPane::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = PREVIEWPANE_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, PREVIEWPANE_CLASS, L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 300, 400,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    m_host.Attach(m_hwnd);
    return m_hwnd;
}

void PreviewPane::SetFile(const std::wstring& filePath) {
    m_currentFile = filePath;
    m_host.LoadFile(filePath);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void PreviewPane::Clear() {
    m_currentFile.clear();
    m_host.Unload();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void PreviewPane::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
        int headerH = Theme::Get().Scale(26, m_hwnd);
        RECT rc = { 0, headerH, width, height };
        m_host.Resize(rc);
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void PreviewPane::ApplyTheme() {
    if (m_hwnd) {
        Theme::Get().ApplyToWindow(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

LRESULT PreviewPane::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) {
            EndPaint(hwnd, &ps);
            return 0;
        }

        int headerH = Theme::Get().Scale(26, hwnd);
        int btnW = Theme::Get().Scale(26, hwnd);
        m_rcClose = { rc.right - btnW, 0, rc.right, headerH };

        // 1. Header background
        RECT rcHeader = { 0, 0, rc.right, headerH };
        FillRect(hdc, &rcHeader, Theme::Get().BrushSurface());

        // Header bottom separator
        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        MoveToEx(hdc, 0, headerH - 1, nullptr);
        LineTo(hdc, rc.right, headerH - 1);
        SelectObject(hdc, oldPen);

        // Header title
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::Get().ClrTextPrimary());
        HFONT hFont = Theme::Get().GetUIFont(hwnd);
        HGDIOBJ oldFont = SelectObject(hdc, hFont);

        std::wstring title = L" 👁 快速预览";
        RECT rcTitle = { Theme::Get().Scale(6, hwnd), 0, m_rcClose.left - 4, headerH };
        DrawTextW(hdc, title.c_str(), -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Close button hover
        if (m_hoverClose) {
            HBRUSH hbrCloseHov = CreateSolidBrush(RGB(232, 17, 35));
            FillRect(hdc, &m_rcClose, hbrCloseHov);
            DeleteObject(hbrCloseHov);
            SetTextColor(hdc, RGB(255, 255, 255));
        } else {
            SetTextColor(hdc, Theme::Get().ClrTextSecondary());
        }

        HFONT hIconFont = Theme::Get().GetIconFont(11, hwnd);
        HGDIOBJ oldIconFont = SelectObject(hdc, hIconFont);
        wchar_t closeGlyph[2] = { 0xE8BB, 0 };
        DrawTextW(hdc, closeGlyph, 1, &m_rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldIconFont);
        DeleteObject(hIconFont);

        SelectObject(hdc, oldFont);

        // 2. Content background
        RECT rcContent = { 0, headerH, rc.right, rc.bottom };
        FillRect(hdc, &rcContent, Theme::Get().BrushBackground());

        // Left separator line
        RECT rcBorder = { 0, 0, 1, rc.bottom };
        FillRect(hdc, &rcBorder, Theme::Get().BrushBorder());

        // Draw fallback content if no active preview handler
        m_host.DrawFallback(hdc, rcContent);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        bool newHover = PtInRect(&m_rcClose, { x, y });
        if (m_hoverClose != newHover) {
            m_hoverClose = newHover;
            InvalidateRect(hwnd, &m_rcClose, TRUE);
            if (m_hoverClose) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (m_hoverClose) {
            m_hoverClose = false;
            InvalidateRect(hwnd, &m_rcClose, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (PtInRect(&m_rcClose, { x, y })) {
            if (onCloseRequested) {
                onCloseRequested();
            }
            return 0;
        }
        break;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int headerH = Theme::Get().Scale(26, hwnd);
        RECT rcContent = { 0, headerH, rc.right, rc.bottom };
        m_host.Resize(rcContent);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PreviewPane::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewPane* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (PreviewPane*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (PreviewPane*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
