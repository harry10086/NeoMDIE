#include "ModernDialogs.h"
#include "Theme.h"
#include "..\core\I18n.h"
#include "..\core\Config.h"
#include "..\resource.h"
#include <wininet.h>
#include <shellapi.h>
#include <windowsx.h>
#include <thread>
#include <atomic>
#include <memory>
#include <string>

#pragma comment(lib, "wininet.lib")

namespace ModernDialogs {

static const wchar_t* CURRENT_VERSION = L"v1.1.4";
static const wchar_t* REPO_URL = L"https://github.com/harry10086/NeoMDIE";
static const wchar_t* RELEASES_URL = L"https://github.com/harry10086/NeoMDIE/releases";
static const wchar_t* AUTHOR_URL = L"https://mianao.info";

static const wchar_t* ABOUT_CLASS = L"NeoMDIE_AboutDialog";
static const wchar_t* UPDATE_CLASS = L"NeoMDIE_UpdateDialog";

static const UINT WM_UPDATE_CHECK_DONE = WM_APP + 101;

// Helper: Draw modern rounded button (supports Primary Accent style or Secondary Surface style)
static void DrawModernButton(LPDRAWITEMSTRUCT pdis, HWND hwndDlg, bool isPrimary) {
    if (!pdis) return;
    bool isDown = (pdis->itemState & ODS_SELECTED) != 0;
    bool isFocus = (pdis->itemState & ODS_FOCUS) != 0;
    bool isDark = Theme::Get().IsDarkMode();

    FillRect(pdis->hDC, &pdis->rcItem, Theme::Get().BrushBackground());

    COLORREF fillClr, borderClr, textClr;
    if (isPrimary) {
        COLORREF accent = Theme::Get().ClrAccent();
        fillClr = isDown ? RGB(
            (std::max)(0, (int)GetRValue(accent) - 25),
            (std::max)(0, (int)GetGValue(accent) - 25),
            (std::max)(0, (int)GetBValue(accent) - 25)) : accent;
        borderClr = fillClr;
        textClr = RGB(255, 255, 255);
    } else {
        fillClr = isDark ? (isDown ? RGB(58, 60, 66) : RGB(44, 46, 51))
                         : (isDown ? RGB(224, 228, 235) : RGB(243, 245, 248));
        borderClr = isDark ? (isFocus ? Theme::Get().ClrAccent() : RGB(70, 72, 78))
                           : (isFocus ? Theme::Get().ClrAccent() : RGB(208, 212, 220));
        textClr = Theme::Get().ClrTextPrimary();
    }

    int rad = Theme::Get().Scale(6, hwndDlg);
    Theme::DrawSmoothRoundRect(pdis->hDC, pdis->rcItem, rad, fillClr, borderClr, 1.0f);

    wchar_t btnText[128] = { 0 };
    GetWindowTextW(pdis->hwndItem, btnText, 128);
    SetBkMode(pdis->hDC, TRANSPARENT);
    SetTextColor(pdis->hDC, textClr);
    HFONT hFont = isPrimary ? Theme::Get().GetUIFontBold(hwndDlg) : Theme::Get().GetUIFont(hwndDlg);
    HGDIOBJ oldFont = SelectObject(pdis->hDC, hFont);
    DrawTextW(pdis->hDC, btnText, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(pdis->hDC, oldFont);
}

static HWND CreateModernButton(HWND hwndParent, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        x, y, w, h, hwndParent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
}

static void SizeAndCenterDialog(HWND hwndDlg, HWND hwndParent, int clientW, int clientH) {
    RECT rcWin = { 0, 0, clientW, clientH };
    DWORD style = (DWORD)GetWindowLongW(hwndDlg, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongW(hwndDlg, GWL_EXSTYLE);
    AdjustWindowRectEx(&rcWin, style, FALSE, exStyle);
    int w = rcWin.right - rcWin.left;
    int h = rcWin.bottom - rcWin.top;

    RECT rcParent = { 0 };
    if (hwndParent && GetWindowRect(hwndParent, &rcParent)) {
        int px = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
        int py = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;
        SetWindowPos(hwndDlg, nullptr, px, py, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
        int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
        SetWindowPos(hwndDlg, nullptr, sx, sy, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

// ============================================================================
// 1. Modern About Dialog (Minimal & Clean: no redundant bottom buttons)
// ============================================================================
struct AboutState {
    HWND hwndParent = nullptr;
    HICON hAppIcon = nullptr;
    RECT rcAuthorLink = { 0 };
    RECT rcRepoLink = { 0 };
    bool hoverAuthor = false;
    bool hoverRepo = false;
};

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AboutState* st = (AboutState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        LPCREATESTRUCTW pcs = (LPCREATESTRUCTW)lParam;
        st = (AboutState*)pcs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);

        Theme::Get().ApplyToWindow(hwnd);
        int iconSz = Theme::Get().Scale(48, hwnd);
        st->hAppIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON),
                                         IMAGE_ICON, iconSz, iconSz, LR_DEFAULTCOLOR);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushBackground());
        return 1;
    }
    case WM_MOUSEMOVE: {
        if (!st) break;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool hA = PtInRect(&st->rcAuthorLink, pt) != FALSE;
        bool hR = PtInRect(&st->rcRepoLink, pt) != FALSE;
        if (hA != st->hoverAuthor || hR != st->hoverRepo) {
            st->hoverAuthor = hA;
            st->hoverRepo = hR;
            InvalidateRect(hwnd, &st->rcAuthorLink, FALSE);
            InvalidateRect(hwnd, &st->rcRepoLink, FALSE);
        }
        SetCursor(LoadCursorW(nullptr, (hA || hR) ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!st) break;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&st->rcAuthorLink, pt)) {
            ShellExecuteW(nullptr, L"open", AUTHOR_URL, nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        if (PtInRect(&st->rcRepoLink, pt)) {
            ShellExecuteW(nullptr, L"open", REPO_URL, nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient = { 0 };
        GetClientRect(hwnd, &rcClient);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
        HGDIOBJ oldBM = SelectObject(memDC, memBM);

        FillRect(memDC, &rcClient, Theme::Get().BrushBackground());
        SetBkMode(memDC, TRANSPARENT);

        bool isZh = I18n::Get().IsChinese();
        bool isDark = Theme::Get().IsDarkMode();
        int pad = Theme::Get().Scale(22, hwnd);
        int iconSz = Theme::Get().Scale(48, hwnd);

        // 1. Header: App Icon + Title + Subtitle + Version Pill
        if (st && st->hAppIcon) {
            DrawIconEx(memDC, pad, pad, st->hAppIcon, iconSz, iconSz, 0, nullptr, DI_NORMAL);
        }

        int textX = pad + iconSz + Theme::Get().Scale(16, hwnd);
        HFONT hTitleFont = CreateFontW(-Theme::Get().Scale(20, hwnd), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT hUiFont = Theme::Get().GetUIFont(hwnd);
        HFONT hBoldFont = Theme::Get().GetUIFontBold(hwnd);

        HGDIOBJ oldFont = SelectObject(memDC, hTitleFont);
        SetTextColor(memDC, Theme::Get().ClrTextPrimary());
        RECT rcTitle = { textX, pad - Theme::Get().Scale(2, hwnd), rcClient.right - pad, pad + Theme::Get().Scale(26, hwnd) };
        DrawTextW(memDC, L"NeoMDIE", -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Version pill right next to NeoMDIE
        SIZE szTitle = { 0 };
        GetTextExtentPoint32W(memDC, L"NeoMDIE", 7, &szTitle);
        int pillX = textX + szTitle.cx + Theme::Get().Scale(10, hwnd);
        int pillW = Theme::Get().Scale(104, hwnd);
        int pillH = Theme::Get().Scale(22, hwnd);
        int pillY = pad + Theme::Get().Scale(2, hwnd);
        RECT rcPill = { pillX, pillY, pillX + pillW, pillY + pillH };
        COLORREF pillBg = isDark ? RGB(28, 52, 78) : RGB(229, 241, 251);
        Theme::DrawSmoothRoundRect(memDC, rcPill, Theme::Get().Scale(11, hwnd), pillBg, Theme::Get().ClrAccent(), 1.0f);
        SelectObject(memDC, hBoldFont);
        SetTextColor(memDC, Theme::Get().ClrAccent());
        DrawTextW(memDC, L"v1.1.4 (x64)", -1, &rcPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Subtitle
        SelectObject(memDC, hUiFont);
        SetTextColor(memDC, Theme::Get().ClrTextSecondary());
        RECT rcSub = { textX, pad + Theme::Get().Scale(28, hwnd), rcClient.right - pad, pad + Theme::Get().Scale(48, hwnd) };
        DrawTextW(memDC, isZh ? L"现代化原生 Win32 多标签多栏文件管理器"
                              : L"Modern Native Win32 Tabbed & Multi-Pane File Manager",
                  -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 2. Rounded Info Card (cleanly encloses all 5 rows)
        int cardTop = pad + iconSz + Theme::Get().Scale(18, hwnd);
        int cardBottom = rcClient.bottom - pad;
        RECT rcCard = { pad, cardTop, rcClient.right - pad, cardBottom };
        Theme::DrawSmoothRoundRect(memDC, rcCard, Theme::Get().Scale(8, hwnd),
                                   Theme::Get().ClrSurface(), Theme::Get().ClrBorder(), 1.0f);

        int innerX = rcCard.left + Theme::Get().Scale(16, hwnd);
        int valX = rcCard.left + Theme::Get().Scale(104, hwnd);
        int rowY = rcCard.top + Theme::Get().Scale(12, hwnd);
        int rowH = Theme::Get().Scale(28, hwnd);

        auto drawRow = [&](const wchar_t* label, const wchar_t* value, bool isLink, bool isHovered, RECT* outLinkRc) {
            SelectObject(memDC, hBoldFont);
            SetTextColor(memDC, Theme::Get().ClrTextSecondary());
            RECT rcLbl = { innerX, rowY, valX - Theme::Get().Scale(8, hwnd), rowY + rowH };
            DrawTextW(memDC, label, -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(memDC, hUiFont);
            SetTextColor(memDC, isLink ? Theme::Get().ClrAccent() : Theme::Get().ClrTextPrimary());
            RECT rcVal = { valX, rowY, rcCard.right - Theme::Get().Scale(16, hwnd), rowY + rowH };
            DrawTextW(memDC, value, -1, &rcVal, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (outLinkRc) {
                SIZE szVal = { 0 };
                GetTextExtentPoint32W(memDC, value, (int)wcslen(value), &szVal);
                *outLinkRc = { valX, rowY + Theme::Get().Scale(4, hwnd),
                               (std::min)((int)(valX + szVal.cx), (int)rcVal.right), rowY + rowH - Theme::Get().Scale(4, hwnd) };
                if (isHovered) {
                    HPEN hPen = CreatePen(PS_SOLID, 1, Theme::Get().ClrAccent());
                    HGDIOBJ oldP = SelectObject(memDC, hPen);
                    int lineY = rowY + rowH - Theme::Get().Scale(5, hwnd);
                    MoveToEx(memDC, outLinkRc->left, lineY, nullptr);
                    LineTo(memDC, outLinkRc->right, lineY);
                    SelectObject(memDC, oldP);
                    DeleteObject(hPen);
                }
            }
            rowY += rowH;
        };

        drawRow(isZh ? L"作    者" : L"Author", L"Harry (https://mianao.info)", true, st ? st->hoverAuthor : false, st ? &st->rcAuthorLink : nullptr);
        drawRow(isZh ? L"开源仓库" : L"GitHub", L"https://github.com/harry10086/NeoMDIE", true, st ? st->hoverRepo : false, st ? &st->rcRepoLink : nullptr);
        drawRow(isZh ? L"致敬经典" : L"Inspired", L"MDIE (Multiple Document Interface Explorer)", false, false, nullptr);
        drawRow(isZh ? L"技术架构" : L"Tech", L"Pure C++17 / Native Win32 / Shell COM", false, false, nullptr);
        drawRow(isZh ? L"开源协议" : L"License", L"GNU General Public License v3 (GPLv3)", false, false, nullptr);

        SelectObject(memDC, oldFont);
        DeleteObject(hTitleFont);

        BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st && st->hAppIcon) {
            DestroyIcon(st->hAppIcon);
            st->hAppIcon = nullptr;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowAbout(HWND hwndParent) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, ABOUT_CLASS, &wc)) {
        wc.lpfnWndProc = AboutWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = ABOUT_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = Theme::Get().BrushBackground();
        RegisterClassExW(&wc);
    }

    bool isZh = I18n::Get().IsChinese();
    AboutState st;
    st.hwndParent = hwndParent;

    int clientW = Theme::Get().Scale(464, hwndParent);
    // 22 top + 48 header + 18 gap + (12 + 5*28 + 12 = 164) card + 22 bottom = 274
    int clientH = Theme::Get().Scale(274, hwndParent);

    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, ABOUT_CLASS,
        isZh ? L"关于 NeoMDIE" : L"About NeoMDIE",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 100, 100, hwndParent, nullptr, hInst, &st);
    if (!hwndDlg) return;

    SizeAndCenterDialog(hwndDlg, hwndParent, clientW, clientH);
    EnableWindow(hwndParent, FALSE);
    ShowWindow(hwndDlg, SW_SHOW);
    UpdateWindow(hwndDlg);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsWindow(hwndDlg)) break;
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
}

// ============================================================================
// 2. Modern Check for Updates Dialog (Minimal & Clean)
// ============================================================================
enum class UpdateCheckStatus {
    Checking,
    UpToDate,
    UpdateAvailable,
    Error
};

struct UpdateCheckResult {
    UpdateCheckStatus status = UpdateCheckStatus::Error;
    std::wstring tagName;
    std::wstring htmlUrl = RELEASES_URL;
    std::wstring errorMsg;
};

struct UpdateDialogState {
    HWND hwndParent = nullptr;
    UpdateCheckStatus status = UpdateCheckStatus::Checking;
    std::wstring tagName;
    std::wstring htmlUrl = RELEASES_URL;
    std::wstring detailMsg;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    HWND hBtnPrimary = nullptr;
    HWND hBtnSecondary = nullptr;
};

static void StartAsyncUpdateCheck(HWND hwndDlg, std::shared_ptr<std::atomic<bool>> alive) {
    bool isZh = I18n::Get().IsChinese();
    std::thread([hwndDlg, alive, isZh]() {
        auto* pRes = new UpdateCheckResult();
        HINTERNET hInternet = InternetOpenW(L"NeoMDIE-UpdateChecker", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!hInternet) {
            pRes->status = UpdateCheckStatus::Error;
            pRes->errorMsg = isZh ? L"无法初始化网络连接，请检查系统网络设置。" : L"Failed to initialize internet connection.";
        } else {
            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
            HINTERNET hUrl = InternetOpenUrlW(hInternet,
                L"https://api.github.com/repos/harry10086/NeoMDIE/releases/latest",
                L"User-Agent: NeoMDIE-UpdateChecker\r\nAccept: application/vnd.github.v3+json\r\n",
                (DWORD)-1, flags, 0);
            if (!hUrl) {
                InternetCloseHandle(hInternet);
                pRes->status = UpdateCheckStatus::Error;
                pRes->errorMsg = isZh ? L"无法连接到 GitHub 服务器，请稍后重试或访问 Releases。"
                                      : L"Unable to connect to GitHub. Retry or open Releases.";
            } else {
                std::string json;
                char buffer[2048];
                DWORD bytesRead = 0;
                while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    json.append(buffer, bytesRead);
                }
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);

                std::string tagName;
                size_t tagPos = json.find("\"tag_name\":");
                if (tagPos != std::string::npos) {
                    size_t start = json.find("\"", tagPos + 11);
                    if (start != std::string::npos) {
                        size_t end = json.find("\"", start + 1);
                        if (end != std::string::npos) {
                            tagName = json.substr(start + 1, end - start - 1);
                        }
                    }
                }

                std::string htmlUrl = "https://github.com/harry10086/NeoMDIE/releases";
                size_t urlPos = json.find("\"html_url\":");
                if (urlPos != std::string::npos) {
                    size_t start = json.find("\"", urlPos + 11);
                    if (start != std::string::npos) {
                        size_t end = json.find("\"", start + 1);
                        if (end != std::string::npos) {
                            htmlUrl = json.substr(start + 1, end - start - 1);
                        }
                    }
                }

                pRes->tagName = Util::Utf8ToWide(tagName);
                pRes->htmlUrl = Util::Utf8ToWide(htmlUrl);
                if (!pRes->tagName.empty() && pRes->tagName != CURRENT_VERSION) {
                    pRes->status = UpdateCheckStatus::UpdateAvailable;
                } else {
                    pRes->status = UpdateCheckStatus::UpToDate;
                }
            }
        }

        if (alive->load() && IsWindow(hwndDlg)) {
            PostMessageW(hwndDlg, WM_UPDATE_CHECK_DONE, 0, (LPARAM)pRes);
        } else {
            delete pRes;
        }
    }).detach();
}

static void LayoutUpdateButtons(HWND hwnd, UpdateDialogState* st) {
    if (!st) return;
    bool isZh = I18n::Get().IsChinese();
    RECT rcClient = { 0 };
    GetClientRect(hwnd, &rcClient);
    int cw = rcClient.right - rcClient.left;
    int pad = Theme::Get().Scale(22, hwnd);
    int btnH = Theme::Get().Scale(32, hwnd);
    int btnY = rcClient.bottom - pad - btnH;
    int gap = Theme::Get().Scale(12, hwnd);

    if (st->status == UpdateCheckStatus::Checking) {
        ShowWindow(st->hBtnSecondary, SW_HIDE);
        ShowWindow(st->hBtnPrimary, SW_HIDE);
    } else if (st->status == UpdateCheckStatus::UpToDate) {
        ShowWindow(st->hBtnPrimary, SW_HIDE);
        ShowWindow(st->hBtnSecondary, SW_SHOW);
        int btnW = Theme::Get().Scale(136, hwnd);
        SetWindowTextW(st->hBtnSecondary, isZh ? L"访问 Releases" : L"Open Releases");
        MoveWindow(st->hBtnSecondary, (cw - btnW) / 2, btnY, btnW, btnH, TRUE);
    } else if (st->status == UpdateCheckStatus::UpdateAvailable) {
        ShowWindow(st->hBtnSecondary, SW_HIDE);
        ShowWindow(st->hBtnPrimary, SW_SHOW);
        int btnW = Theme::Get().Scale(136, hwnd);
        SetWindowTextW(st->hBtnPrimary, isZh ? L"前往下载" : L"Download");
        MoveWindow(st->hBtnPrimary, (cw - btnW) / 2, btnY, btnW, btnH, TRUE);
    } else { // Error: centered pair [Retry] [Open Releases]
        ShowWindow(st->hBtnSecondary, SW_SHOW);
        ShowWindow(st->hBtnPrimary, SW_SHOW);
        int w1 = Theme::Get().Scale(96, hwnd);
        int w2 = Theme::Get().Scale(132, hwnd);
        int startX = (cw - (w1 + gap + w2)) / 2;
        SetWindowTextW(st->hBtnSecondary, isZh ? L"重试" : L"Retry");
        MoveWindow(st->hBtnSecondary, startX, btnY, w1, btnH, TRUE);
        SetWindowTextW(st->hBtnPrimary, isZh ? L"打开 Releases" : L"Open Releases");
        MoveWindow(st->hBtnPrimary, startX + w1 + gap, btnY, w2, btnH, TRUE);
    }
}

static LRESULT CALLBACK UpdateWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UpdateDialogState* st = (UpdateDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        LPCREATESTRUCTW pcs = (LPCREATESTRUCTW)lParam;
        st = (UpdateDialogState*)pcs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);

        Theme::Get().ApplyToWindow(hwnd);
        st->hBtnSecondary = CreateModernButton(hwnd, 2001, L"", 0, 0, 0, 0);
        st->hBtnPrimary = CreateModernButton(hwnd, IDOK, L"", 0, 0, 0, 0);
        LayoutUpdateButtons(hwnd, st);
        StartAsyncUpdateCheck(hwnd, st->alive);
        return 0;
    }
    case WM_SIZE: {
        LayoutUpdateButtons(hwnd, st);
        return 0;
    }
    case WM_UPDATE_CHECK_DONE: {
        UpdateCheckResult* pRes = (UpdateCheckResult*)lParam;
        if (pRes && st) {
            st->status = pRes->status;
            st->tagName = pRes->tagName;
            st->htmlUrl = pRes->htmlUrl.empty() ? RELEASES_URL : pRes->htmlUrl;
            st->detailMsg = pRes->errorMsg;
            LayoutUpdateButtons(hwnd, st);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        delete pRes;
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::Get().BrushBackground());
        return 1;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis) {
            DrawModernButton(pdis, hwnd, pdis->CtlID == IDOK);
            return TRUE;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient = { 0 };
        GetClientRect(hwnd, &rcClient);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
        HGDIOBJ oldBM = SelectObject(memDC, memBM);

        FillRect(memDC, &rcClient, Theme::Get().BrushBackground());
        SetBkMode(memDC, TRANSPARENT);

        bool isZh = I18n::Get().IsChinese();
        bool isDark = Theme::Get().IsDarkMode();
        int pad = Theme::Get().Scale(22, hwnd);

        int badgeSz = Theme::Get().Scale(44, hwnd);
        RECT rcBadge = { pad, pad, pad + badgeSz, pad + badgeSz };
        COLORREF badgeClr = Theme::Get().ClrAccent();
        wchar_t glyph[2] = { 0xE895, 0 };
        std::wstring heading;
        std::wstring subHeading;

        UpdateCheckStatus curStatus = st ? st->status : UpdateCheckStatus::Checking;
        if (curStatus == UpdateCheckStatus::Checking) {
            glyph[0] = (wchar_t)0xE895;
            badgeClr = Theme::Get().ClrAccent();
            heading = isZh ? L"正在检查更新..." : L"Checking for Updates...";
            subHeading = isZh ? L"正在连接 GitHub 获取最新发布版本信息" : L"Connecting to GitHub Releases API";
        } else if (curStatus == UpdateCheckStatus::UpToDate) {
            glyph[0] = (wchar_t)0xE73E;
            badgeClr = RGB(16, 152, 72);
            heading = isZh ? L"当前已是最新版本" : L"You Are Up to Date";
            subHeading = isZh ? L"您的 NeoMDIE 已是官方最新稳定版本，无需更新" : L"No newer updates are currently available";
        } else if (curStatus == UpdateCheckStatus::UpdateAvailable) {
            glyph[0] = (wchar_t)0xE896;
            badgeClr = Theme::Get().ClrAccent();
            heading = isZh ? (L"发现新版本: " + st->tagName) : (L"New Version Available: " + st->tagName);
            subHeading = isZh ? L"点击“前往下载”即可打开 GitHub 发布页获取最新版" : L"Click Download to open the GitHub Release page";
        } else {
            glyph[0] = (wchar_t)0xE7BA;
            badgeClr = RGB(216, 128, 16);
            heading = isZh ? L"检查更新失败" : L"Update Check Failed";
            subHeading = (st && !st->detailMsg.empty()) ? st->detailMsg
                       : (isZh ? L"无法连接到 GitHub 服务器" : L"Unable to connect to GitHub");
        }

        COLORREF badgeBg = isDark ? RGB(
            (GetRValue(badgeClr) + 40) / 3,
            (GetGValue(badgeClr) + 40) / 3,
            (GetBValue(badgeClr) + 40) / 3)
            : RGB(
            (GetRValue(badgeClr) + 255 * 4) / 5,
            (GetGValue(badgeClr) + 255 * 4) / 5,
            (GetBValue(badgeClr) + 255 * 4) / 5);
        Theme::DrawSmoothRoundRect(memDC, rcBadge, Theme::Get().Scale(10, hwnd), badgeBg, badgeClr, 1.0f);

        HFONT hIconFont = Theme::Get().GetIconFont(Theme::Get().Scale(22, hwnd), hwnd);
        HGDIOBJ oldFont = SelectObject(memDC, hIconFont);
        SetTextColor(memDC, badgeClr);
        DrawTextW(memDC, glyph, 1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(hIconFont);

        int textX = pad + badgeSz + Theme::Get().Scale(14, hwnd);
        HFONT hTitleFont = CreateFontW(-Theme::Get().Scale(16, hwnd), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        SelectObject(memDC, hTitleFont);
        SetTextColor(memDC, Theme::Get().ClrTextPrimary());
        RECT rcHead = { textX, pad, rcClient.right - pad, pad + Theme::Get().Scale(24, hwnd) };
        DrawTextW(memDC, heading.c_str(), -1, &rcHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DeleteObject(hTitleFont);

        HFONT hUiFont = Theme::Get().GetUIFont(hwnd);
        HFONT hBoldFont = Theme::Get().GetUIFontBold(hwnd);
        SelectObject(memDC, hUiFont);
        SetTextColor(memDC, Theme::Get().ClrTextSecondary());
        RECT rcSub = { textX, pad + Theme::Get().Scale(25, hwnd), rcClient.right - pad, pad + Theme::Get().Scale(46, hwnd) };
        DrawTextW(memDC, subHeading.c_str(), -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Version Info Card (cleanly encloses both rows with balanced margin above button)
        int cardTop = pad + badgeSz + Theme::Get().Scale(16, hwnd);
        int btnAreaH = Theme::Get().Scale(32, hwnd) + Theme::Get().Scale(18, hwnd);
        int cardBottom = rcClient.bottom - pad - btnAreaH;
        RECT rcCard = { pad, cardTop, rcClient.right - pad, cardBottom };
        Theme::DrawSmoothRoundRect(memDC, rcCard, Theme::Get().Scale(8, hwnd),
                                   Theme::Get().ClrSurface(), Theme::Get().ClrBorder(), 1.0f);

        int innerX = rcCard.left + Theme::Get().Scale(16, hwnd);
        int valX = rcCard.left + Theme::Get().Scale(108, hwnd);
        int rowH = Theme::Get().Scale(26, hwnd);
        int rowY = rcCard.top + (cardBottom - cardTop - rowH * 2) / 2;

        auto drawInfo = [&](const wchar_t* lbl, const std::wstring& val, COLORREF valClr) {
            SelectObject(memDC, hBoldFont);
            SetTextColor(memDC, Theme::Get().ClrTextSecondary());
            RECT rcL = { innerX, rowY, valX - Theme::Get().Scale(8, hwnd), rowY + rowH };
            DrawTextW(memDC, lbl, -1, &rcL, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(memDC, hUiFont);
            SetTextColor(memDC, valClr);
            RECT rcV = { valX, rowY, rcCard.right - Theme::Get().Scale(16, hwnd), rowY + rowH };
            DrawTextW(memDC, val.c_str(), -1, &rcV, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            rowY += rowH;
        };

        drawInfo(isZh ? L"当前版本" : L"Current", std::wstring(CURRENT_VERSION) + L" (x64 Release)", Theme::Get().ClrTextPrimary());
        if (curStatus == UpdateCheckStatus::Checking) {
            drawInfo(isZh ? L"最新版本" : L"Latest", isZh ? L"正在查询..." : L"Checking...", Theme::Get().ClrTextSecondary());
        } else if (curStatus == UpdateCheckStatus::UpToDate) {
            drawInfo(isZh ? L"最新版本" : L"Latest", st->tagName.empty() ? std::wstring(CURRENT_VERSION) : st->tagName, RGB(16, 152, 72));
        } else if (curStatus == UpdateCheckStatus::UpdateAvailable) {
            drawInfo(isZh ? L"最新版本" : L"Latest", st->tagName, Theme::Get().ClrAccent());
        } else {
            drawInfo(isZh ? L"发布地址" : L"Releases", RELEASES_URL, Theme::Get().ClrTextSecondary());
        }

        SelectObject(memDC, oldFont);

        BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDCANCEL) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDOK) {
            if (st && (st->status == UpdateCheckStatus::UpdateAvailable || st->status == UpdateCheckStatus::Error)) {
                ShellExecuteW(nullptr, L"open", st->htmlUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == 2001 && st) {
            if (st->status == UpdateCheckStatus::Error) { // Retry
                st->status = UpdateCheckStatus::Checking;
                LayoutUpdateButtons(hwnd, st);
                InvalidateRect(hwnd, nullptr, FALSE);
                StartAsyncUpdateCheck(hwnd, st->alive);
            } else if (st->status == UpdateCheckStatus::UpToDate) { // Open Releases
                ShellExecuteW(nullptr, L"open", RELEASES_URL, nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        break;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st) {
            st->alive->store(false);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowCheckUpdates(HWND hwndParent) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, UPDATE_CLASS, &wc)) {
        wc.lpfnWndProc = UpdateWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = UPDATE_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = Theme::Get().BrushBackground();
        RegisterClassExW(&wc);
    }

    bool isZh = I18n::Get().IsChinese();
    UpdateDialogState st;
    st.hwndParent = hwndParent;

    int clientW = Theme::Get().Scale(432, hwndParent);
    // 22 top + 44 header + 16 gap + 76 card + 18 gap + 32 btn + 22 bottom = 230
    int clientH = Theme::Get().Scale(230, hwndParent);

    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, UPDATE_CLASS,
        isZh ? L"检查更新" : L"Check for Updates",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 100, 100, hwndParent, nullptr, hInst, &st);
    if (!hwndDlg) return;

    SizeAndCenterDialog(hwndDlg, hwndParent, clientW, clientH);
    EnableWindow(hwndParent, FALSE);
    ShowWindow(hwndDlg, SW_SHOW);
    UpdateWindow(hwndDlg);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsWindow(hwndDlg)) break;
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
}

} // namespace ModernDialogs
