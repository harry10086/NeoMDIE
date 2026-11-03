#include "LinksBar.h"
#include "Theme.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"

static const wchar_t* LINKSBAR_CLASS = L"NeoMDIE_LinksBar";

LinksBar::LinksBar() {
}

LinksBar::~LinksBar() {
}

HWND LinksBar::Create(HWND hwndParent, int id) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, LINKSBAR_CLASS, &wc)) {
        wc.lpfnWndProc = LinksBar::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = LINKSBAR_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    m_hwnd = CreateWindowExW(
        0, LINKSBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 800, 28,
        hwndParent, (HMENU)(INT_PTR)id, hInst, this
    );

    LoadLinks();
    return m_hwnd;
}

void LinksBar::LoadLinks() {
    m_links.clear();
    std::wstring iniPath = Config::Get().GetConfigFilePath();
    const wchar_t* sec = L"Links";
    int count = GetPrivateProfileIntW(sec, L"Count", -1, iniPath.c_str());

    if (count <= 0) {
        // Defaults: Desktop, Downloads, Documents, Pictures, Drive C, Drive D
        wchar_t buf[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, buf) == S_OK) {
            m_links.push_back({ I18n::Get().T(StringId::Desktop), buf });
        }
        wchar_t userProfile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            m_links.push_back({ I18n::Get().T(StringId::Downloads), std::wstring(userProfile) + L"\\Downloads" });
        }
        if (SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, buf) == S_OK) {
            m_links.push_back({ I18n::Get().T(StringId::Documents), buf });
        }
        if (SHGetFolderPathW(nullptr, CSIDL_MYPICTURES, nullptr, 0, buf) == S_OK) {
            m_links.push_back({ I18n::Get().T(StringId::Pictures), buf });
        }
        if (PathFileExistsW(L"C:\\")) {
            m_links.push_back({ L"系统 (C:)", L"C:\\" });
        }
        if (PathFileExistsW(L"D:\\")) {
            m_links.push_back({ L"数据 (D:)", L"D:\\" });
        }
        SaveLinks();
    } else {
        for (int i = 0; i < count; ++i) {
            wchar_t kName[32], kPath[32];
            StringCchPrintfW(kName, 32, L"Name%d", i);
            StringCchPrintfW(kPath, 32, L"Path%d", i);

            wchar_t vName[MAX_PATH], vPath[MAX_PATH];
            GetPrivateProfileStringW(sec, kName, L"", vName, MAX_PATH, iniPath.c_str());
            GetPrivateProfileStringW(sec, kPath, L"", vPath, MAX_PATH, iniPath.c_str());

            if (wcslen(vPath) > 0) {
                m_links.push_back({ vName, vPath });
            }
        }
    }
}

void LinksBar::SaveLinks() {
    std::wstring iniPath = Config::Get().GetConfigFilePath();
    const wchar_t* sec = L"Links";

    // Clear old keys
    WritePrivateProfileSectionW(sec, L"", iniPath.c_str());

    wchar_t buf[32];
    StringCchPrintfW(buf, 32, L"%d", (int)m_links.size());
    WritePrivateProfileStringW(sec, L"Count", buf, iniPath.c_str());

    for (size_t i = 0; i < m_links.size(); ++i) {
        wchar_t kName[32], kPath[32];
        StringCchPrintfW(kName, 32, L"Name%d", (int)i);
        StringCchPrintfW(kPath, 32, L"Path%d", (int)i);

        WritePrivateProfileStringW(sec, kName, m_links[i].name.c_str(), iniPath.c_str());
        WritePrivateProfileStringW(sec, kPath, m_links[i].path.c_str(), iniPath.c_str());
    }
}

void LinksBar::AddLink(const std::wstring& name, const std::wstring& path) {
    if (path.empty()) return;
    std::wstring displayName = name;
    if (displayName.empty()) {
        displayName = ShellUtils::GetDisplayName(path);
    }
    m_links.push_back({ displayName, path });
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

void LinksBar::UpdateLayout(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

LRESULT LinksBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fill background
        HBRUSH hbrBg = Theme::Get().BrushSurface();
        FillRect(hdc, &rc, hbrBg);

        // Subtle bottom line
        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        MoveToEx(hdc, 0, rc.bottom - 1, nullptr);
        LineTo(hdc, rc.right, rc.bottom - 1);
        SelectObject(hdc, oldPen);

        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = Theme::Get().GetUIFont(hwnd);
        HGDIOBJ oldFont = SelectObject(hdc, hFont);

        // Draw Links label on left
        bool isZh = I18n::Get().IsChinese();
        std::wstring lbl = isZh ? L" 链接: " : L" Links: ";
        RECT rcLbl = { Theme::Get().Scale(4, hwnd), 0, Theme::Get().Scale(50, hwnd), rc.bottom - 1 };
        SetTextColor(hdc, Theme::Get().ClrTextSecondary());
        DrawTextW(hdc, lbl.c_str(), -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int curX = rcLbl.right + Theme::Get().Scale(4, hwnd);
        m_itemRects.clear();

        for (size_t i = 0; i < m_links.size(); ++i) {
            std::wstring itemText = L"\U0001F4C1 " + m_links[i].name;

            SIZE sz;
            GetTextExtentPoint32W(hdc, itemText.c_str(), (int)itemText.size(), &sz);
            int padX = Theme::Get().Scale(8, hwnd);
            int itemW = sz.cx + padX * 2;

            RECT rcItem = { curX, Theme::Get().Scale(2, hwnd), curX + itemW, rc.bottom - Theme::Get().Scale(3, hwnd) };
            m_itemRects.push_back(rcItem);

            // Draw button hover / normal
            if ((int)i == m_hoverIndex) {
                HBRUSH hbrHover = Theme::Get().BrushHover();
                FillRect(hdc, &rcItem, hbrHover);
            }

            SetTextColor(hdc, Theme::Get().ClrTextPrimary());
            DrawTextW(hdc, itemText.c_str(), -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            curX += itemW + Theme::Get().Scale(4, hwnd);
        }

        // Draw [+] add link button
        int addW = Theme::Get().Scale(26, hwnd);
        m_btnAddRect = { curX, Theme::Get().Scale(2, hwnd), curX + addW, rc.bottom - Theme::Get().Scale(3, hwnd) };
        if (m_hoverAdd) {
            HBRUSH hbrHover = Theme::Get().BrushHover();
            HPEN hPen = Theme::Get().PenBorder();
            HGDIOBJ oldBr = SelectObject(hdc, hbrHover);
            HGDIOBJ oldP = SelectObject(hdc, hPen);
            RoundRect(hdc, m_btnAddRect.left, m_btnAddRect.top, m_btnAddRect.right, m_btnAddRect.bottom, 4, 4);
            SelectObject(hdc, oldP);
            SelectObject(hdc, oldBr);
        }
        HFONT hMdlFont = CreateFontW(-Theme::Get().Scale(15, hwnd), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Segoe MDL2 Assets");
        HGDIOBJ oldMdl = SelectObject(hdc, hMdlFont);
        SetTextColor(hdc, Theme::Get().ClrAccent());
        DrawTextW(hdc, L"\uE710", -1, &m_btnAddRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldMdl);
        DeleteObject(hMdlFont);

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int newHover = -1;
        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                newHover = (int)i;
                break;
            }
        }

        bool newHoverAdd = PtInRect(&m_btnAddRect, { x, y }) != FALSE;

        if (newHover != m_hoverIndex || newHoverAdd != m_hoverAdd) {
            m_hoverIndex = newHover;
            m_hoverAdd = newHoverAdd;
            InvalidateRect(hwnd, nullptr, FALSE);

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        m_hoverIndex = -1;
        m_hoverAdd = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                if (GetKeyState(VK_CONTROL) < 0) {
                    if (onOpenNewTab) onOpenNewTab(m_links[i].path);
                } else {
                    if (onNavigate) onNavigate(m_links[i].path);
                }
                return 0;
            }
        }

        if (PtInRect(&m_btnAddRect, { x, y })) {
            // Trigger add current
            SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(9901, 0), 0);
            return 0;
        }
        return 0;
    }

    case WM_MBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                if (onOpenNewTab) onOpenNewTab(m_links[i].path);
                return 0;
            }
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT ptScreen = { x, y };
        ClientToScreen(hwnd, &ptScreen);

        int hitIndex = -1;
        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                hitIndex = (int)i;
                break;
            }
        }

        bool isZh = I18n::Get().IsChinese();
        HMENU hMenu = CreatePopupMenu();

        if (hitIndex >= 0) {
            AppendMenuW(hMenu, MF_STRING, 1, isZh ? L"打开(&O)" : L"Open(&O)");
            AppendMenuW(hMenu, MF_STRING, 2, isZh ? L"在新标签页中打开(&T)" : L"Open in New Tab(&T)");
            AppendMenuW(hMenu, MF_STRING, 3, isZh ? L"在分屏中打开(&S)" : L"Open in Split Pane(&S)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4, isZh ? L"复制路径(&C)" : L"Copy Path(&C)");
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
            case 3: if (onOpenSplit) onOpenSplit(m_links[hitIndex].path); break;
            case 4: ShellUtils::CopyToClipboard(m_links[hitIndex].path); break;
            case 5: RemoveLink(hitIndex); break;
            }
        } else {
            switch (cmd) {
            case 10:
                SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(9901, 0), 0);
                break;
            case 11: {
                std::wstring iniPath = Config::Get().GetConfigFilePath();
                WritePrivateProfileSectionW(L"Links", L"", iniPath.c_str());
                LoadLinks();
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
