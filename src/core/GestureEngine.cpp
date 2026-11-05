#include "GestureEngine.h"
#include "I18n.h"
#include "Config.h"
#include "ScriptEngine.h"
#include "..\resource.h"
#include "..\ui\Theme.h"

static const wchar_t* GESTURE_HUD_CLASS = L"NeoMDIE_GestureHUD";

static LRESULT CALLBACK GestureHudWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Dark rounded background
        HBRUSH hbrBg = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(hdc, &rc, hbrBg);
        DeleteObject(hbrBg);

        // Neon Blue border
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 160, 255));
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 0, 0, rc.right, rc.bottom, 12, 12);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);

        // Draw gesture name text
        wchar_t text[128] = { 0 };
        GetWindowTextW(hwnd, text, 128);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        HFONT hFont = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(hdc, hFont);

        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

GestureEngine::GestureEngine() {
}

GestureEngine::~GestureEngine() {
    HideHud();
    if (m_hHudWnd) {
        DestroyWindow(m_hHudWnd);
        m_hHudWnd = nullptr;
    }
}

void GestureEngine::Attach(HWND hTargetWnd) {
    m_hTargetWnd = hTargetWnd;
}

void GestureEngine::Detach() {
    HideHud();
    m_hTargetWnd = nullptr;
}

void GestureEngine::OnRButtonDown(HWND hwnd, int x, int y) {
    if (!Config::Get().gesturesEnabled) {
        m_isRDown = false;
        return;
    }
    m_isRDown = true;
    m_isDragging = false;
    m_startPoint = { x, y };
    m_lastVectorPoint = { x, y };
    m_points.clear();
    m_points.push_back(m_startPoint);
    m_gestureCode.clear();
}

bool GestureEngine::OnMouseMove(HWND hwnd, int x, int y, bool isRButtonDown) {
    if (!m_isRDown && !isRButtonDown) {
        return false;
    }

    if (!m_isDragging) {
        int dx = x - m_startPoint.x;
        int dy = y - m_startPoint.y;
        if (abs(dx) + abs(dy) >= 12) {
            m_isDragging = true;
            SetCapture(hwnd);
            POINT ptScreen = { x, y };
            ClientToScreen(hwnd, &ptScreen);
            ShowHud(ptScreen);
        } else {
            return false;
        }
    }

    ProcessMovement(x, y);

    // Draw visual mouse trail
    HWND hTarget = m_hTargetWnd ? m_hTargetWnd : hwnd;
    HDC hdc = GetDC(hTarget);
    if (hdc && m_points.size() >= 2) {
        HPEN hPen = CreatePen(PS_SOLID, 4, RGB(0, 160, 255));
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        size_t n = m_points.size();
        MoveToEx(hdc, m_points[n - 2].x, m_points[n - 2].y, nullptr);
        LineTo(hdc, m_points[n - 1].x, m_points[n - 1].y);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
        ReleaseDC(hTarget, hdc);
    }

    POINT ptScreen = { x, y };
    ClientToScreen(hwnd, &ptScreen);
    if (m_hHudWnd) {
        SetWindowPos(m_hHudWnd, HWND_TOPMOST, ptScreen.x + 20, ptScreen.y + 20, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        UpdateHudText();
    }

    return true;
}

bool GestureEngine::OnRButtonUp(HWND hwnd, int x, int y, GestureAction& outAction) {
    outAction = GestureAction::None;
    m_isRDown = false;

    if (m_isDragging) {
        m_isDragging = false;
        ReleaseCapture();
        HideHud();

        HWND hTarget = m_hTargetWnd ? m_hTargetWnd : hwnd;
        InvalidateRect(hTarget, nullptr, TRUE);

        outAction = MatchGesture();
        m_gestureCode.clear();
        m_points.clear();
        return true; // Gesture was active! Consume to prevent context menu
    }

    m_gestureCode.clear();
    m_points.clear();
    return false; // Did not drag, normal right-click!
}

void GestureEngine::ProcessMovement(int x, int y) {
    int dx = x - m_lastVectorPoint.x;
    int dy = y - m_lastVectorPoint.y;

    if (abs(dx) < 14 && abs(dy) < 14) {
        return;
    }

    char dir = 0;
    if (abs(dx) > abs(dy)) {
        dir = (dx > 0) ? 'R' : 'L';
    } else {
        dir = (dy > 0) ? 'D' : 'U';
    }

    if (m_gestureCode.empty() || m_gestureCode.back() != dir) {
        m_gestureCode.push_back(dir);
    }

    m_lastVectorPoint = { x, y };
    m_points.push_back({ x, y });
}

std::wstring GestureEngine::GetActionName(GestureAction action) {
    bool zh = I18n::Get().IsChinese();
    int intAct = static_cast<int>(action);
    if (intAct >= ID_SCRIPT_COMMAND_START && intAct <= ID_SCRIPT_COMMAND_END) {
        for (const auto& s : ScriptEngine::Get().GetScripts()) {
            if (s.commandId == (UINT)intAct) {
                return (zh ? L"【脚本】" : L"[Script] ") + s.name;
            }
        }
        return zh ? L"【扩展脚本】" : L"[Script]";
    }
    switch (action) {
    case GestureAction::NavBack:         return zh ? L"后退" : L"Back";
    case GestureAction::NavForward:      return zh ? L"前进" : L"Forward";
    case GestureAction::NavUp:           return zh ? L"向上一级" : L"Up to Parent";
    case GestureAction::CloseTab:        return zh ? L"关闭当前标签页" : L"Close Current Tab";
    case GestureAction::NewTab:          return zh ? L"新建标签页" : L"New Tab";
    case GestureAction::Refresh:         return zh ? L"刷新" : L"Refresh";
    case GestureAction::DuplicateTab:    return zh ? L"复制当前标签页" : L"Duplicate Current Tab";
    case GestureAction::ToggleTree:      return zh ? L"显示/隐藏侧边栏目录树" : L"Toggle Tree Sidebar";
    case GestureAction::TogglePreview:   return zh ? L"显示/隐藏快速预览窗格" : L"Toggle Preview Pane";
    case GestureAction::OpenTerminal:    return zh ? L"打开命令行 CMD" : L"Open Command Prompt CMD";
    case GestureAction::MaximizePane:    return zh ? L"最大化/还原当前栏" : L"Maximize/Restore Pane";
    case GestureAction::ClosePane:       return zh ? L"关闭当前栏" : L"Close Pane";
    case GestureAction::ScrollTop:       return zh ? L"滚动到列表顶部" : L"Scroll to Top";
    case GestureAction::ScrollBottom:    return zh ? L"滚动到列表底部" : L"Scroll to Bottom";
    case GestureAction::PageUp:          return zh ? L"向上翻页" : L"Page Up";
    case GestureAction::PageDown:        return zh ? L"向下翻页" : L"Page Down";
    case GestureAction::NewFolder:       return zh ? L"新建文件夹" : L"New Folder";
    case GestureAction::Copy:            return zh ? L"复制选中项" : L"Copy";
    case GestureAction::Cut:             return zh ? L"剪切选中项" : L"Cut";
    case GestureAction::Paste:           return zh ? L"粘贴" : L"Paste";
    case GestureAction::SelectAll:       return zh ? L"全选" : L"Select All";
    case GestureAction::Properties:      return zh ? L"属性" : L"Properties";
    case GestureAction::LockTab:         return zh ? L"锁定/解锁当前标签页" : L"Pin/Unpin Tab";
    case GestureAction::ReopenClosedTab: return zh ? L"恢复最近关闭的标签页" : L"Reopen Closed Tab";
    case GestureAction::NextTab:         return zh ? L"切换到下一个标签页" : L"Next Tab";
    case GestureAction::PrevTab:         return zh ? L"切换到上一个标签页" : L"Previous Tab";
    case GestureAction::CloseOtherTabs:  return zh ? L"关闭其他标签页" : L"Close Other Tabs";
    case GestureAction::LayoutSingle:    return zh ? L"单栏模式" : L"Single Pane";
    case GestureAction::Layout2H:        return zh ? L"左右双栏" : L"Dual Pane Horizontal";
    case GestureAction::Layout2V:        return zh ? L"上下双栏" : L"Dual Pane Vertical";
    case GestureAction::Layout4Grid:     return zh ? L"四分栏模式" : L"4 Panes Grid";
    case GestureAction::CopyPath:        return zh ? L"复制当前路径" : L"Copy Path";
    case GestureAction::Delete:          return zh ? L"删除选中项" : L"Delete";
    case GestureAction::Rename:          return zh ? L"重命名" : L"Rename";
    case GestureAction::NewMD:           return zh ? L"新建Markdown文档" : L"New Markdown Document";
    case GestureAction::ToggleHidden:    return zh ? L"显示/隐藏隐藏文件" : L"Toggle Hidden Files";
    case GestureAction::OpenSettings:    return zh ? L"选项与设置" : L"Options & Settings";
    case GestureAction::InvertSelection: return zh ? L"反向选择" : L"Invert Selection";
    default: return zh ? L"无动作" : L"None";
    }
}

std::wstring GestureEngine::CodeToArrowString(const std::string& code) {
    std::wstring s;
    for (char c : code) {
        if (c == 'L') s += (wchar_t)0x2190;      // ←
        else if (c == 'R') s += (wchar_t)0x2192; // →
        else if (c == 'U') s += (wchar_t)0x2191; // ↑
        else if (c == 'D') s += (wchar_t)0x2193; // ↓
    }
    return s;
}

GestureAction GestureEngine::MatchGesture() const {
    const auto& gestures = Config::Get().gestures;
    for (const auto& g : gestures) {
        if (g.code == m_gestureCode) {
            return (GestureAction)g.action;
        }
    }
    return GestureAction::None;
}

std::wstring GestureEngine::GetCurrentGestureName() const {
    bool zh = I18n::Get().IsChinese();
    if (m_gestureCode.empty()) {
        return zh ? L"按住右键划动手势" : L"Draw gesture...";
    }

    std::wstring arrows = CodeToArrowString(m_gestureCode);
    const auto& gestures = Config::Get().gestures;
    for (const auto& g : gestures) {
        if (g.code == m_gestureCode) {
            std::wstring actName = !g.name.empty() ? g.name : GetActionName((GestureAction)g.action);
            return arrows + L" " + actName;
        }
    }

    return arrows + (zh ? L" (识别中...)" : L" (Recognizing...)");
}

void GestureEngine::ShowHud(const POINT& ptScreen) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    if (!GetClassInfoExW(hInst, GESTURE_HUD_CLASS, &wc)) {
        wc.lpfnWndProc = GestureHudWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = GESTURE_HUD_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    if (!m_hHudWnd) {
        m_hHudWnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            GESTURE_HUD_CLASS, L"", WS_POPUP,
            ptScreen.x + 20, ptScreen.y + 20, 220, 44,
            m_hTargetWnd, nullptr, hInst, nullptr
        );
    }

    UpdateHudText();
    SetWindowPos(m_hHudWnd, HWND_TOPMOST, ptScreen.x + 20, ptScreen.y + 20, 220, 44,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void GestureEngine::UpdateHudText() {
    if (m_hHudWnd) {
        std::wstring name = GetCurrentGestureName();
        SetWindowTextW(m_hHudWnd, name.c_str());
        InvalidateRect(m_hHudWnd, nullptr, TRUE);
    }
}

void GestureEngine::HideHud() {
    if (m_hHudWnd) {
        ShowWindow(m_hHudWnd, SW_HIDE);
    }
}
