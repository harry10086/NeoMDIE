#include "SplitContainer.h"
#include "Theme.h"

static const wchar_t* SPLITCONTAINER_CLASS = L"NeoMDIE_SplitContainer";

SplitContainer::SplitContainer() {
}

SplitContainer::~SplitContainer() {
}

HWND SplitContainer::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, SPLITCONTAINER_CLASS, &wc)) {
        wc.lpfnWndProc = SplitContainer::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = SPLITCONTAINER_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, SPLITCONTAINER_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 800, 500,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    m_leftView.Create(m_hwnd, 1001);
    m_rightView.Create(m_hwnd, 1002);
    ShowWindow(m_rightView.GetHwnd(), SW_HIDE);

    return m_hwnd;
}

void SplitContainer::SetSplitEnabled(bool enabled) {
    if (m_splitEnabled != enabled) {
        m_splitEnabled = enabled;
        ShowWindow(m_rightView.GetHwnd(), m_splitEnabled ? SW_SHOW : SW_HIDE);
        if (m_splitEnabled && m_rightView.GetCurrentPath().empty()) {
            m_rightView.NavigateTo(m_leftView.GetCurrentPath());
        }
        LayoutChildren();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void SplitContainer::SetSplitRatio(int ratio) {
    m_splitRatio = std::clamp(ratio, 10, 90);
    LayoutChildren();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void SplitContainer::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
        LayoutChildren();
    }
}

void SplitContainer::LayoutChildren() {
    if (!m_hwnd) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int totalW = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if (!m_splitEnabled) {
        m_leftView.UpdateLayout(0, 0, totalW, h);
    } else {
        int splitBarWidth = Theme::Get().Scale(5, m_hwnd);
        int leftW = (totalW * m_splitRatio) / 100;
        int rightW = totalW - leftW - splitBarWidth;

        m_leftView.UpdateLayout(0, 0, leftW, h);

        m_rcSplitter.left = leftW;
        m_rcSplitter.top = 0;
        m_rcSplitter.right = leftW + splitBarWidth;
        m_rcSplitter.bottom = h;

        m_rightView.UpdateLayout(leftW + splitBarWidth, 0, rightW, h);
    }
}

LRESULT SplitContainer::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (m_splitEnabled) {
            HBRUSH hbr = Theme::Get().BrushBorder();
            FillRect(hdc, &m_rcSplitter, hbr);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_SETCURSOR: {
        if (m_splitEnabled) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (PtInRect(&m_rcSplitter, pt) || m_draggingSplitter) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        if (m_splitEnabled) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&m_rcSplitter, pt)) {
                m_draggingSplitter = true;
                SetCapture(hwnd);
                return 0;
            }
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (m_draggingSplitter && (wParam & MK_LBUTTON)) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT rc;
            GetClientRect(hwnd, &rc);
            int totalW = rc.right - rc.left;
            if (totalW > 50) {
                int newRatio = (pt.x * 100) / totalW;
                SetSplitRatio(newRatio);
            }
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (m_draggingSplitter) {
            m_draggingSplitter = false;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SplitContainer::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SplitContainer* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (SplitContainer*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (SplitContainer*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
