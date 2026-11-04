#include "BreadcrumbBar.h"
#include "Theme.h"
#include "..\core\ShellUtils.h"

static const wchar_t* BREADCRUMB_CLASS = L"NeoMDIE_BreadcrumbBar";

BreadcrumbBar::BreadcrumbBar() {
}

BreadcrumbBar::~BreadcrumbBar() {
}

HWND BreadcrumbBar::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, BREADCRUMB_CLASS, &wc)) {
        wc.lpfnWndProc = BreadcrumbBar::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = BREADCRUMB_CLASS;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, BREADCRUMB_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 300, 32,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    // Child edit control for direct path entry
    m_hwndEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL,
        0, 0, 100, 24,
        m_hwnd, (HMENU)101, hInst, nullptr
    );

    SetWindowSubclass(m_hwndEdit, BreadcrumbBar::EditSubclassProc, 1, (DWORD_PTR)this);
    SendMessageW(m_hwndEdit, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    return m_hwnd;
}

void BreadcrumbBar::SetPath(const std::wstring& path) {
    m_currentPath = Util::NormalizePath(path);
    ParseSegments();
    if (m_editMode) {
        CancelEdit();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void BreadcrumbBar::ParseSegments() {
    m_segments.clear();
    if (m_currentPath.empty() || m_currentPath == L"此电脑" || m_currentPath == L"This PC") {
        BreadcrumbSegment seg;
        seg.name = L"此电脑";
        seg.targetPath = L"此电脑";
        m_segments.push_back(seg);
        return;
    }

    // Always start with "This PC"
    BreadcrumbSegment root;
    root.name = L"此电脑";
    root.targetPath = L"此电脑";
    m_segments.push_back(root);

    // Break down path: e.g. "C:\Users\harry" -> "C:", "Users", "harry"
    std::wstring accum;
    std::wistringstream ss(m_currentPath);
    std::wstring part;

    while (std::getline(ss, part, L'\\')) {
        if (part.empty()) continue;
        if (accum.empty()) {
            accum = part + L"\\";
        } else {
            accum += (accum.back() == L'\\' ? L"" : L"\\") + part;
        }

        BreadcrumbSegment seg;
        seg.name = part;
        seg.targetPath = accum;
        m_segments.push_back(seg);
    }
}

void BreadcrumbBar::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
        if (m_editMode && m_hwndEdit) {
            MoveWindow(m_hwndEdit, Theme::Get().Scale(6, m_hwnd), Theme::Get().Scale(4, m_hwnd),
                       width - Theme::Get().Scale(12, m_hwnd), height - Theme::Get().Scale(8, m_hwnd), TRUE);
        }
    }
}

void BreadcrumbBar::SwitchToEditMode() {
    m_editMode = true;
    SetWindowTextW(m_hwndEdit, m_currentPath.c_str());
    SendMessageW(m_hwndEdit, WM_SETFONT, (WPARAM)Theme::Get().GetUIFont(m_hwnd), TRUE);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    MoveWindow(m_hwndEdit, Theme::Get().Scale(6, m_hwnd), Theme::Get().Scale(4, m_hwnd),
               rc.right - Theme::Get().Scale(12, m_hwnd), rc.bottom - Theme::Get().Scale(8, m_hwnd), TRUE);

    ShowWindow(m_hwndEdit, SW_SHOW);
    SetFocus(m_hwndEdit);
    SendMessageW(m_hwndEdit, EM_SETSEL, 0, -1); // Select all
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void BreadcrumbBar::CommitEdit() {
    wchar_t buf[MAX_PATH];
    GetWindowTextW(m_hwndEdit, buf, MAX_PATH);
    m_editMode = false;
    ShowWindow(m_hwndEdit, SW_HIDE);

    std::wstring newPath = Util::NormalizePath(buf);
    if (!newPath.empty() && onPathNavigated) {
        onPathNavigated(newPath);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void BreadcrumbBar::CancelEdit() {
    m_editMode = false;
    ShowWindow(m_hwndEdit, SW_HIDE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void BreadcrumbBar::Draw(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

    // Surface background & border
    HBRUSH hbrBg = Theme::Get().BrushSurface();
    FillRect(hdcMem, &rcClient, hbrBg);

    HPEN hPen = Theme::Get().PenBorder();
    HGDIOBJ oldPen = SelectObject(hdcMem, hPen);
    SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
    Rectangle(hdcMem, 0, 0, w, h);

    if (!m_editMode) {
        HFONT hFont = Theme::Get().GetUIFont(m_hwnd);
        HGDIOBJ oldFont = SelectObject(hdcMem, hFont);
        SetBkMode(hdcMem, TRANSPARENT);

        int curX = Theme::Get().Scale(8, m_hwnd);
        int arrowWidth = Theme::Get().Scale(14, m_hwnd);

        for (size_t i = 0; i < m_segments.size(); ++i) {
            SIZE textSize;
            GetTextExtentPoint32W(hdcMem, m_segments[i].name.c_str(), (int)m_segments[i].name.length(), &textSize);

            m_segments[i].rc.left = curX;
            m_segments[i].rc.top = Theme::Get().Scale(2, m_hwnd);
            m_segments[i].rc.right = curX + textSize.cx + Theme::Get().Scale(8, m_hwnd);
            m_segments[i].rc.bottom = h - Theme::Get().Scale(2, m_hwnd);

            // Hover background for segment
            if ((int)i == m_hoverSegment) {
                HBRUSH hbrHover = Theme::Get().BrushHover();
                FillRect(hdcMem, &m_segments[i].rc, hbrHover);
            }

            // Draw segment text
            SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
            RECT rcText = m_segments[i].rc;
            DrawTextW(hdcMem, m_segments[i].name.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            curX = m_segments[i].rc.right;

            // Draw '>' separator if not last segment
            if (i + 1 < m_segments.size()) {
                RECT rcArrow = { curX, 0, curX + arrowWidth, h };
                SetTextColor(hdcMem, Theme::Get().ClrTextSecondary());
                DrawTextW(hdcMem, L">", -1, &rcArrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                curX += arrowWidth;
            }
        }

        SelectObject(hdcMem, oldFont);
    }

    SelectObject(hdcMem, oldPen);
    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
}

LRESULT BreadcrumbBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Draw(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushSurface());
        return 1;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);

        int oldHover = m_hoverSegment;
        m_hoverSegment = -1;

        for (size_t i = 0; i < m_segments.size(); ++i) {
            if (PtInRect(&m_segments[i].rc, pt)) {
                m_hoverSegment = (int)i;
                break;
            }
        }

        if (oldHover != m_hoverSegment) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (m_hoverSegment != -1) {
            m_hoverSegment = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (size_t i = 0; i < m_segments.size(); ++i) {
            if (PtInRect(&m_segments[i].rc, pt)) {
                if (onPathNavigated) {
                    onPathNavigated(m_segments[i].targetPath);
                }
                return 0;
            }
        }
        // Clicked on empty space: switch to edit mode
        SwitchToEditMode();
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK BreadcrumbBar::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    BreadcrumbBar* pThis = (BreadcrumbBar*)dwRefData;
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            pThis->CommitEdit();
            return 0;
        } else if (wParam == VK_ESCAPE) {
            pThis->CancelEdit();
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        pThis->CancelEdit();
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK BreadcrumbBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BreadcrumbBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (BreadcrumbBar*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (BreadcrumbBar*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
