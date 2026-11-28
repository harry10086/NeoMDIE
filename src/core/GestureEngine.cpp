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
    auto& i18n = I18n::Get();
    int intAct = static_cast<int>(action);
    if (intAct >= ID_SCRIPT_COMMAND_START && intAct <= ID_SCRIPT_COMMAND_END) {
        for (const auto& s : ScriptEngine::Get().GetScripts()) {
            if (s.commandId == (UINT)intAct) {
                return std::wstring(i18n.Pick(L"【脚本】", L"【腳本】", L"[Script] ", L"[スクリプト] ")) + s.name;
            }
        }
        return i18n.Pick(L"【扩展脚本】", L"【擴充腳本】", L"[Script]", L"[スクリプト]");
    }
    switch (action) {
    case GestureAction::NavBack:              return i18n.Pick(L"后退", L"上一頁", L"Back", L"戻る");
    case GestureAction::NavForward:           return i18n.Pick(L"前进", L"下一頁", L"Forward", L"進む");
    case GestureAction::NavUp:                return i18n.Pick(L"向上一级", L"上一層", L"Up to Parent", L"上のフォルダーへ");
    case GestureAction::CloseTab:             return i18n.Pick(L"关闭当前标签页", L"關閉目前分頁", L"Close Current Tab", L"現在のタブを閉じる");
    case GestureAction::NewTab:               return i18n.Pick(L"新建标签页", L"新增分頁", L"New Tab", L"新しいタブ");
    case GestureAction::Refresh:              return i18n.Pick(L"刷新", L"重新整理", L"Refresh", L"更新");
    case GestureAction::DuplicateTab:         return i18n.Pick(L"复制当前标签页", L"複製目前分頁", L"Duplicate Current Tab", L"現在のタブを複製");
    case GestureAction::ToggleTree:           return i18n.Pick(L"显示/隐藏侧边栏目录树", L"顯示/隱藏側邊欄目錄樹", L"Toggle Tree Sidebar", L"ツリー サイドバー切替");
    case GestureAction::TogglePreview:        return i18n.Pick(L"显示/隐藏快速预览窗格", L"顯示/隱藏快速預覽窗格", L"Toggle Preview Pane", L"プレビュー ウィンドウ切替");
    case GestureAction::OpenTerminal:         return i18n.Pick(L"打开命令行 CMD", L"開啟命令提示字元 CMD", L"Open Command Prompt CMD", L"コマンド プロンプトを開く");
    case GestureAction::MaximizePane:         return i18n.Pick(L"最大化/还原当前栏", L"最大化/還原目前窗格", L"Maximize/Restore Pane", L"ペインを最大化/復元");
    case GestureAction::ClosePane:            return i18n.Pick(L"关闭当前栏", L"關閉目前窗格", L"Close Pane", L"ペインを閉じる");
    case GestureAction::ScrollTop:            return i18n.Pick(L"滚动到列表顶部", L"捲動至清單頂端", L"Scroll to Top", L"先頭へスクロール");
    case GestureAction::ScrollBottom:         return i18n.Pick(L"滚动到列表底部", L"捲動至清單底端", L"Scroll to Bottom", L"末尾へスクロール");
    case GestureAction::PageUp:               return i18n.Pick(L"向上翻页", L"向上捲頁", L"Page Up", L"前のページへ");
    case GestureAction::PageDown:             return i18n.Pick(L"向下翻页", L"向下捲頁", L"Page Down", L"次のページへ");
    case GestureAction::NewFolder:            return i18n.Pick(L"新建文件夹", L"新增資料夾", L"New Folder", L"新しいフォルダー");
    case GestureAction::Copy:                 return i18n.Pick(L"复制选中项", L"複製選取項目", L"Copy", L"コピー");
    case GestureAction::Cut:                  return i18n.Pick(L"剪切选中项", L"剪下選取項目", L"Cut", L"切り取り");
    case GestureAction::Paste:                return i18n.Pick(L"粘贴", L"貼上", L"Paste", L"貼り付け");
    case GestureAction::SelectAll:            return i18n.Pick(L"全选", L"全選", L"Select All", L"すべて選択");
    case GestureAction::Properties:           return i18n.Pick(L"属性", L"內容", L"Properties", L"プロパティ");
    case GestureAction::LockTab:              return i18n.Pick(L"锁定/解锁当前标签页", L"釘選/取消釘選目前分頁", L"Pin/Unpin Tab", L"タブの固定/解除");
    case GestureAction::ReopenClosedTab:      return i18n.Pick(L"恢复最近关闭的标签页", L"還原最近關閉的分頁", L"Reopen Closed Tab", L"閉じたタブを再度開く");
    case GestureAction::NextTab:              return i18n.Pick(L"切换到下一个标签页", L"切換至下一個分頁", L"Next Tab", L"次のタブ");
    case GestureAction::PrevTab:              return i18n.Pick(L"切换到上一个标签页", L"切換至上一個分頁", L"Previous Tab", L"前のタブ");
    case GestureAction::CloseOtherTabs:       return i18n.Pick(L"关闭其他标签页", L"關閉其他分頁", L"Close Other Tabs", L"他のタブを閉じる");
    case GestureAction::LayoutSingle:         return i18n.Pick(L"单栏模式", L"單欄模式", L"Single Pane", L"単一ペイン");
    case GestureAction::Layout2H:             return i18n.Pick(L"左右双栏", L"左右雙欄", L"Dual Pane Horizontal", L"左右2ペイン");
    case GestureAction::Layout2V:             return i18n.Pick(L"上下双栏", L"上下雙欄", L"Dual Pane Vertical", L"上下2ペイン");
    case GestureAction::Layout4Grid:          return i18n.Pick(L"四分栏模式", L"四分欄模式", L"4 Panes Grid", L"4分割ペイン");
    case GestureAction::CopyPath:             return i18n.Pick(L"复制当前路径", L"複製目前路徑", L"Copy Path", L"パスをコピー");
    case GestureAction::Delete:               return i18n.Pick(L"删除选中项", L"刪除選取項目", L"Delete", L"削除");
    case GestureAction::Rename:               return i18n.Pick(L"重命名", L"重新命名", L"Rename", L"名前の変更");
    case GestureAction::NewMD:                return i18n.Pick(L"新建Markdown文档", L"新增 Markdown 文件", L"New Markdown Document", L"新しい Markdown ドキュメント");
    case GestureAction::ToggleHidden:         return i18n.Pick(L"显示/隐藏隐藏文件", L"顯示/隱藏隱藏檔案", L"Toggle Hidden Files", L"隠しファイルの表示切替");
    case GestureAction::OpenSettings:         return i18n.Pick(L"选项与设置", L"選項與設定", L"Options & Settings", L"オプションと設定");
    case GestureAction::InvertSelection:      return i18n.Pick(L"反向选择", L"反向選取", L"Invert Selection", L"選択の切り替え");
    case GestureAction::Undo:                 return i18n.Pick(L"撤销", L"復原", L"Undo", L"元に戻す");
    case GestureAction::ToggleCheckBoxes:     return i18n.Pick(L"显示/隐藏项目复选框", L"顯示/隱藏項目核取方塊", L"Toggle Item Checkboxes", L"項目チェック ボックス切替");
    case GestureAction::ToggleDotFiles:       return i18n.Pick(L"隐藏/显示点文件", L"隱藏/顯示點開頭檔案", L"Toggle Dotfiles", L"ドットファイルの表示切替");
    case GestureAction::ToggleCalcFolderSize: return i18n.Pick(L"计算文件夹大小", L"計算資料夾大小", L"Calculate Folder Size", L"フォルダー サイズ計算");
    case GestureAction::FocusAddress:         return i18n.Pick(L"聚焦到地址栏", L"聚焦至網址列", L"Focus Address Bar", L"アドレス バーにフォーカス");
    case GestureAction::FocusFilter:          return i18n.Pick(L"聚焦到搜索过滤栏", L"聚焦至搜尋篩選列", L"Focus Filter Bar", L"フィルター バーにフォーカス");
    case GestureAction::ToggleDarkMode:       return i18n.Pick(L"切换深色/浅色模式", L"切換深色/淺色模式", L"Toggle Dark Mode", L"ダーク/ライト モード切替");
    case GestureAction::ToggleMenuBar:        return i18n.Pick(L"显示/隐藏传统菜单栏", L"顯示/隱藏傳統功能表列", L"Toggle Menu Bar", L"メニュー バー表示切替");
    case GestureAction::ToggleSyncNav:        return i18n.Pick(L"多窗格同步浏览", L"多窗格同步瀏覽", L"Synchronized Navigation", L"ペイン同期ナビゲーション");
    default: return i18n.Pick(L"无动作", L"無動作", L"None", L"なし");
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
    auto& i18n = I18n::Get();
    if (m_gestureCode.empty()) {
        return i18n.Pick(L"按住右键划动手势", L"按住右鍵劃動手勢", L"Draw gesture...", L"右ドラッグでジェスチャー...");
    }

    std::wstring arrows = CodeToArrowString(m_gestureCode);
    const auto& gestures = Config::Get().gestures;
    for (const auto& g : gestures) {
        if (g.code == m_gestureCode) {
            std::wstring actName = !g.name.empty() ? g.name : GetActionName((GestureAction)g.action);
            return arrows + L" " + actName;
        }
    }

    return arrows + i18n.Pick(L" (识别中...)", L" (辨識中...)", L" (Recognizing...)", L" (認識中...)");
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
