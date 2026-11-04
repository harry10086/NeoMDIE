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
    m_links.clear();
    std::wstring iniPath = Config::Get().GetConfigFilePath();
    const wchar_t* sec = L"Links";

    int count = GetPrivateProfileIntW(sec, L"Count", -1, iniPath.c_str());
    if (count >= 0) {
        for (int i = 0; i < count; ++i) {
            wchar_t kName[32], kPath[32], kColor[32];
            StringCchPrintfW(kName, 32, L"Name%d", i);
            StringCchPrintfW(kPath, 32, L"Path%d", i);
            StringCchPrintfW(kColor, 32, L"Color%d", i);

            wchar_t vName[MAX_PATH], vPath[MAX_PATH];
            GetPrivateProfileStringW(sec, kName, L"", vName, MAX_PATH, iniPath.c_str());
            GetPrivateProfileStringW(sec, kPath, L"", vPath, MAX_PATH, iniPath.c_str());
            DWORD dwColor = (DWORD)GetPrivateProfileIntW(sec, kColor, 0, iniPath.c_str());

            if (wcslen(vPath) > 0) {
                m_links.push_back({ vName, vPath, (COLORREF)dwColor });
            }
        }
    }

    // Defaults if empty
    if (m_links.empty()) {
        wchar_t userProfile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            m_links.push_back({ I18n::Get().T(StringId::Desktop), std::wstring(userProfile) + L"\\Desktop", 0 });
            m_links.push_back({ I18n::Get().T(StringId::Downloads), std::wstring(userProfile) + L"\\Downloads", 0 });
            m_links.push_back({ I18n::Get().T(StringId::Documents), std::wstring(userProfile) + L"\\Documents", 0 });
            m_links.push_back({ I18n::Get().T(StringId::Pictures), std::wstring(userProfile) + L"\\Pictures", 0 });
        }
        SaveLinks();
    }
}

void LinksBar::SaveLinks() {
    std::wstring iniPath = Config::Get().GetConfigFilePath();
    const wchar_t* sec = L"Links";

    WritePrivateProfileSectionW(sec, L"", iniPath.c_str());
    WritePrivateProfileStringW(sec, L"Count", std::to_wstring(m_links.size()).c_str(), iniPath.c_str());

    for (size_t i = 0; i < m_links.size(); ++i) {
        wchar_t kName[32], kPath[32], kColor[32];
        StringCchPrintfW(kName, 32, L"Name%d", (int)i);
        StringCchPrintfW(kPath, 32, L"Path%d", (int)i);
        StringCchPrintfW(kColor, 32, L"Color%d", (int)i);

        WritePrivateProfileStringW(sec, kName, m_links[i].name.c_str(), iniPath.c_str());
        WritePrivateProfileStringW(sec, kPath, m_links[i].path.c_str(), iniPath.c_str());
        WritePrivateProfileStringW(sec, kColor, std::to_wstring((DWORD)m_links[i].color).c_str(), iniPath.c_str());
    }
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

void LinksBar::UpdateTooltip(int index) {
    if (!m_hwndTooltip) return;

    TOOLINFOW ti = { sizeof(ti) };
    ti.hwnd = m_hwnd;
    ti.uId = 1;

    if (index >= 0 && index < (int)m_links.size()) {
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

        HBRUSH hbrBg = Theme::Get().BrushSurface();
        FillRect(hdc, &rc, hbrBg);

        HPEN hPen = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        MoveToEx(hdc, 0, rc.bottom - 1, nullptr);
        LineTo(hdc, rc.right, rc.bottom - 1);
        SelectObject(hdc, oldPen);

        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = Theme::Get().GetUIFont(hwnd);
        HGDIOBJ oldFont = SelectObject(hdc, hFont);

        bool isZh = I18n::Get().IsChinese();
        std::wstring lbl = isZh ? L" 链接: " : L" Links: ";
        RECT rcLbl = { Theme::Get().Scale(4, hwnd), 0, Theme::Get().Scale(50, hwnd), rc.bottom - 1 };
        SetTextColor(hdc, Theme::Get().ClrTextSecondary());
        DrawTextW(hdc, lbl.c_str(), -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int curX = rcLbl.right + Theme::Get().Scale(4, hwnd);
        m_itemRects.clear();

        for (size_t i = 0; i < m_links.size(); ++i) {
            const std::wstring& itemText = m_links[i].name;

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

            curX += itemW + Theme::Get().Scale(4, hwnd);
        }

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
        m_hoverIndex = -1;
        UpdateTooltip(-1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                ActionTrigger trigger = GetCurrentTrigger();
                WindowAction act = Config::Get().actionLinks.ResolveAction(trigger);
                if (act == WindowAction::CurrentPane && onNavigate) onNavigate(m_links[i].path);
                else if (act == WindowAction::OtherPane && onOpenSplit) onOpenSplit(m_links[i].path);
                else if ((act == WindowAction::NewTab || act == WindowAction::BgTab) && onOpenNewTab) onOpenNewTab(m_links[i].path);
                else if (act == WindowAction::Explorer) ShellExecuteW(nullptr, L"explore", m_links[i].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
        }
        return 0;
    }

    case WM_MBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                ActionTrigger trigger = GetCurrentTrigger(true, false);
                WindowAction act = Config::Get().actionLinks.ResolveAction(trigger);
                if (act == WindowAction::CurrentPane && onNavigate) onNavigate(m_links[i].path);
                else if (act == WindowAction::OtherPane && onOpenSplit) onOpenSplit(m_links[i].path);
                else if ((act == WindowAction::NewTab || act == WindowAction::BgTab) && onOpenNewTab) onOpenNewTab(m_links[i].path);
                else if (act == WindowAction::Explorer) ShellExecuteW(nullptr, L"explore", m_links[i].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
