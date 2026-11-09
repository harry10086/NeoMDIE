#include "SettingsDialog.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\GestureEngine.h"
#include "..\core\ShellUtils.h"
#include "..\core\ScriptEngine.h"
#include "Theme.h"
#include <commctrl.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <algorithm>

#define SETTINGS_CLASS L"NeoMDIE_SettingsDialog"

static std::function<void()> s_onSettingsChanged;
static int s_currentCategory = 0;
static std::vector<HWND> s_pageControls[10];

static const wchar_t* navTitlesZh[10] = {
    L" 常规与启动",
    L" 窗口动作设置",
    L" 窗口与多栏",
    L" 标签页偏好",
    L" 侧边栏与树状",
    L" 视图与外观",
    L" 搜索与过滤",
    L" 快捷键自定义",
    L" 鼠标手势设置",
    L" 工具栏配置"
};

static const wchar_t* navTitlesEn[10] = {
    L" General",
    L" Window Actions",
    L" Window & Panes",
    L" Tabs",
    L" Sidebar & Tree",
    L" View & List",
    L" Search & Filter",
    L" Hotkeys",
    L" Mouse Gestures",
    L" Toolbars"
};

// Controls
static HWND s_hCategoryList = nullptr;

// Page 0: General
static HWND s_hLangCombo = nullptr;
static HWND s_hChkRestoreTabs = nullptr;
static HWND s_hChkMinToTray = nullptr;
static HWND s_hChkCloseToTray = nullptr;
static HWND s_hChkSingleInstance = nullptr;
static HWND s_hChkReplaceExplorer = nullptr;

// Page 1: Window Actions (Target + 5 action triggers)
static HWND s_hActionTargetCombo = nullptr;
static HWND s_hActionCombos[5] = { nullptr };
static TargetActionConfig s_tempTreeAction;
static TargetActionConfig s_tempLinksAction;
static TargetActionConfig s_tempAddressAction;
static int s_curActionTarget = 0; // 0: Tree, 1: Links, 2: Address

// Page 2: Window & Panes
static HWND s_hLayoutCombo = nullptr;
static HWND s_hChkSyncNav = nullptr;
static HWND s_hChkLockToolbars = nullptr;

// Page 3: Tabs
static HWND s_hChkAllowDupTabs = nullptr;
static HWND s_hTabDblClickCombo = nullptr;

// Page 4: Tree & Sidebar
static HWND s_hChkTree = nullptr;
static HWND s_hChkAutoExpandTree = nullptr;
static HWND s_hChkPreview = nullptr;

// Page 5: View & File List
static HWND s_hViewModeCombo = nullptr;
static HWND s_hChkHidden = nullptr;
static HWND s_hChkFoldersFirst = nullptr;
static HWND s_hChkCalcSize = nullptr;
static HWND s_hChkZebraStriping = nullptr;
static HWND s_hChkGridLines = nullptr;

// Page 6: Search & Filter
static HWND s_hChkSearchSubfolders = nullptr;
static HWND s_hChkMatchCase = nullptr;

// Page 7: Hotkeys (MDIE style)
static HWND s_hShortcutList = nullptr;
static HWND s_hScActionCombo = nullptr;
static HWND s_hScKeyCombo = nullptr;
static HWND s_hScShift = nullptr;
static HWND s_hScCtrl = nullptr;
static HWND s_hScAlt = nullptr;
static HWND s_hBtnScAdd = nullptr;
static HWND s_hBtnScMod = nullptr;
static HWND s_hBtnScDel = nullptr;
static HWND s_hBtnScReset = nullptr;

struct ShortcutItem {
    std::wstring name;
    std::wstring key;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    std::wstring keyText;
};
static std::vector<ShortcutItem> s_tempShortcuts;

// Page 8: Mouse Gestures (MDIE style)
static HWND s_hChkGestures = nullptr;
static HWND s_hGestureList = nullptr;
static HWND s_hGestPatternCombo = nullptr;
static HWND s_hGestActionCombo = nullptr;
static HWND s_hGestContextCombo = nullptr;
static HWND s_hBtnGestAdd = nullptr;
static HWND s_hBtnGestMod = nullptr;
static HWND s_hBtnGestDel = nullptr;
static HWND s_hBtnGestReset = nullptr;
static std::vector<GestureEntry> s_tempGestures;

// Page 9: Appearance & Toolbars
static HWND s_hThemeCombo = nullptr;
static HWND s_hChkTBStandard = nullptr;
static HWND s_hChkTBAddress = nullptr;
static HWND s_hChkTBSearch = nullptr;
static HWND s_hChkTBLinks = nullptr;
static HWND s_hChkStatusBar = nullptr;

struct ActionDef {
    int id;
    const wchar_t* nameZh;
    const wchar_t* nameEn;
};

static const ActionDef s_allActions[] = {
    { (int)GestureAction::NavBack, L"后退", L"Back" },
    { (int)GestureAction::NavForward, L"前进", L"Forward" },
    { (int)GestureAction::NavUp, L"向上一级", L"Up to Parent" },
    { (int)GestureAction::Refresh, L"刷新", L"Refresh" },
    { (int)GestureAction::NewTab, L"新建标签页", L"New Tab" },
    { (int)GestureAction::CloseTab, L"关闭当前标签页", L"Close Current Tab" },
    { (int)GestureAction::CloseOtherTabs, L"关闭其他标签页", L"Close Other Tabs" },
    { (int)GestureAction::DuplicateTab, L"复制当前标签页", L"Duplicate Current Tab" },
    { (int)GestureAction::ReopenClosedTab, L"恢复最近关闭的标签页", L"Reopen Closed Tab" },
    { (int)GestureAction::LockTab, L"锁定/解锁当前标签页", L"Pin/Unpin Tab" },
    { (int)GestureAction::NextTab, L"切换到下一个标签页", L"Next Tab" },
    { (int)GestureAction::PrevTab, L"切换到上一个标签页", L"Previous Tab" },
    { (int)GestureAction::ToggleTree, L"显示/隐藏侧边栏目录树", L"Toggle Tree Sidebar" },
    { (int)GestureAction::TogglePreview, L"显示/隐藏快速预览窗格", L"Toggle Preview Pane" },
    { (int)GestureAction::OpenTerminal, L"打开命令行 CMD", L"Open Command Prompt CMD" },
    { (int)GestureAction::MaximizePane, L"最大化/还原当前栏", L"Maximize/Restore Pane" },
    { (int)GestureAction::ClosePane, L"关闭当前栏", L"Close Pane" },
    { (int)GestureAction::ScrollTop, L"滚动到列表顶部", L"Scroll to Top" },
    { (int)GestureAction::ScrollBottom, L"滚动到列表底部", L"Scroll to Bottom" },
    { (int)GestureAction::PageUp, L"向上翻页", L"Page Up" },
    { (int)GestureAction::PageDown, L"向下翻页", L"Page Down" },
    { (int)GestureAction::Copy, L"复制选中项", L"Copy" },
    { (int)GestureAction::Cut, L"剪切选中项", L"Cut" },
    { (int)GestureAction::Paste, L"粘贴", L"Paste" },
    { (int)GestureAction::SelectAll, L"全选", L"Select All" },
    { (int)GestureAction::NewFolder, L"新建文件夹", L"New Folder" },
    { (int)GestureAction::Properties, L"属性", L"Properties" },
    { (int)GestureAction::LayoutSingle, L"单栏模式", L"Single Pane" },
    { (int)GestureAction::Layout2H, L"左右双栏", L"Dual Pane Horizontal" },
    { (int)GestureAction::Layout2V, L"上下双栏", L"Dual Pane Vertical" },
    { (int)GestureAction::Layout4Grid, L"四分栏模式", L"4 Panes Grid" },
    { (int)GestureAction::CopyPath, L"复制当前路径", L"Copy Path" },
    { (int)GestureAction::Delete, L"删除选中项", L"Delete" },
    { (int)GestureAction::Rename, L"重命名", L"Rename" },
    { (int)GestureAction::NewMD, L"新建Markdown文档", L"New Markdown Document" },
    { (int)GestureAction::ToggleHidden, L"显示/隐藏隐藏文件", L"Toggle Hidden Files" },
    { (int)GestureAction::OpenSettings, L"选项与设置", L"Options & Settings" },
    { (int)GestureAction::InvertSelection, L"反向选择", L"Invert Selection" },
    { (int)GestureAction::Undo, L"撤销", L"Undo" }
};

struct GestPatternDef {
    const char* code;
    const wchar_t* labelZh;
    const wchar_t* labelEn;
};

static const GestPatternDef s_allPatterns[] = {
    { "L", L"← (向左)", L"← (Left)" },
    { "R", L"→ (向右)", L"→ (Right)" },
    { "U", L"↑ (向上)", L"↑ (Up)" },
    { "D", L"↓ (向下)", L"↓ (Down)" },
    { "UD", L"↕ (上下)", L"↕ (Up-Down)" },
    { "DU", L"↓↑ (下上)", L"↓↑ (Down-Up)" },
    { "LR", L"↔ (左右)", L"↔ (Left-Right)" },
    { "RL", L"←→ (右左)", L"←→ (Right-Left)" },
    { "UL", L"↖ (上左)", L"↖ (Up-Left)" },
    { "UR", L"↗ (上右)", L"↗ (Up-Right)" },
    { "DL", L"↙ (下左)", L"↙ (Down-Left)" },
    { "DR", L"↘ (下右)", L"↘ (Down-Right)" },
    { "LU", L"←↑ (左上)", L"←↑ (Left-Up)" },
    { "LD", L"←↓ (左下)", L"←↓ (Left-Down)" },
    { "RU", L"→↑ (右上)", L"→↑ (Right-Up)" },
    { "RD", L"→↓ (右下)", L"→↓ (Right-Down)" },
    { "LDR", L"←↓→ (左下右)", L"←↓→ (Left-Down-Right)" },
    { "RDL", L"→↓← (右下左)", L"→↓← (Right-Down-Left)" },
    { "LUR", L"←↑→ (左上右)", L"←↑→ (Left-Up-Right)" },
    { "RUL", L"→↑← (右上左)", L"→↑← (Right-Up-Left)" }
};

static const wchar_t* s_keyChoices[] = {
    L"A", L"B", L"C", L"D", L"E", L"F", L"G", L"H", L"I", L"J",
    L"K", L"L", L"M", L"N", L"O", L"P", L"Q", L"R", L"S", L"T",
    L"U", L"V", L"W", L"X", L"Y", L"Z",
    L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9",
    L"F1", L"F2", L"F3", L"F4", L"F5", L"F6", L"F7", L"F8", L"F9", L"F10", L"F11", L"F12",
    L"Tab", L"Space", L"Enter", L"Esc", L"Backspace", L"Delete", L"Insert",
    L"Home", L"End", L"PageUp", L"PageDown",
    L"Left", L"Right", L"Up", L"Down"
};

static void UpdateActionCombosForCurrentTarget() {
    TargetActionConfig* pTarget = nullptr;
    if (s_curActionTarget == 0) pTarget = &s_tempTreeAction;
    else if (s_curActionTarget == 1) pTarget = &s_tempLinksAction;
    else pTarget = &s_tempAddressAction;

    int vals[5] = {
        pTarget->openCurrentPane,
        pTarget->openNewTab,
        pTarget->openBgTab,
        pTarget->openOtherPane,
        pTarget->openExplorer
    };

    for (int i = 0; i < 5; ++i) {
        if (s_hActionCombos[i]) {
            SendMessageW(s_hActionCombos[i], CB_SETCURSEL, std::clamp(vals[i], 0, 12), 0);
        }
    }
}

static void SaveActionCombosForCurrentTarget() {
    TargetActionConfig* pTarget = nullptr;
    if (s_curActionTarget == 0) pTarget = &s_tempTreeAction;
    else if (s_curActionTarget == 1) pTarget = &s_tempLinksAction;
    else pTarget = &s_tempAddressAction;

    pTarget->openCurrentPane = (int)SendMessageW(s_hActionCombos[0], CB_GETCURSEL, 0, 0);
    pTarget->openNewTab = (int)SendMessageW(s_hActionCombos[1], CB_GETCURSEL, 0, 0);
    pTarget->openBgTab = (int)SendMessageW(s_hActionCombos[2], CB_GETCURSEL, 0, 0);
    pTarget->openOtherPane = (int)SendMessageW(s_hActionCombos[3], CB_GETCURSEL, 0, 0);
    pTarget->openExplorer = (int)SendMessageW(s_hActionCombos[4], CB_GETCURSEL, 0, 0);
}

static void PopulateGestureList(HWND hList) {
    ListView_DeleteAllItems(hList);
    bool zh = I18n::Get().IsChinese();
    for (size_t i = 0; i < s_tempGestures.size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        std::wstring pattern = GestureEngine::CodeToArrowString(s_tempGestures[i].code) + L"  " + Util::Utf8ToWide(s_tempGestures[i].code);
        lvi.pszText = (LPWSTR)pattern.c_str();
        ListView_InsertItem(hList, &lvi);

        std::wstring actName = (!s_tempGestures[i].name.empty() && s_tempGestures[i].name.length() > 1)
            ? s_tempGestures[i].name : GestureEngine::GetActionName((GestureAction)s_tempGestures[i].action);
        ListView_SetItemText(hList, (int)i, 1, (LPWSTR)actName.c_str());
        ListView_SetItemText(hList, (int)i, 2, (LPWSTR)(zh ? L"全局 / 列表" : L"Global / List"));
    }
}

static std::wstring FormatShortcutKey(const ShortcutItem& item) {
    std::wstring s = L"";
    if (item.ctrl) s += L"Ctrl + ";
    if (item.shift) s += L"Shift + ";
    if (item.alt) s += L"Alt + ";
    s += item.key;
    return s;
}

static void ResetDefaultShortcuts() {
    bool zh = I18n::Get().IsChinese();
    s_tempShortcuts = {
        { zh ? L"新建标签页" : L"New Tab", L"T", false, true, false, L"Ctrl + T" },
        { zh ? L"关闭当前标签页" : L"Close Current Tab", L"W", false, true, false, L"Ctrl + W" },
        { zh ? L"切换到下一个标签页" : L"Next Tab", L"Tab", false, true, false, L"Ctrl + Tab" },
        { zh ? L"切换到上一个标签页" : L"Previous Tab", L"Tab", true, true, false, L"Ctrl + Shift + Tab" },
        { zh ? L"新建窗口" : L"New Window", L"N", false, true, false, L"Ctrl + N" },
        { zh ? L"后退" : L"Back", L"Left", false, false, true, L"Alt + Left" },
        { zh ? L"前进" : L"Forward", L"Right", false, false, true, L"Alt + Right" },
        { zh ? L"向上一级" : L"Up to Parent", L"Up", false, false, true, L"Alt + Up" },
        { zh ? L"刷新" : L"Refresh", L"F5", false, false, false, L"F5" },
        { zh ? L"聚焦到地址栏" : L"Focus Address Bar", L"D", false, false, true, L"Alt + D" },
        { zh ? L"聚焦到搜索过滤栏" : L"Focus Filter Bar", L"F", false, true, false, L"Ctrl + F" },
        { zh ? L"最大化/还原当前栏" : L"Maximize/Restore Pane", L"F11", false, false, false, L"F11" },
        { zh ? L"单栏视图" : L"Single Pane", L"1", true, true, false, L"Ctrl + Shift + 1" },
        { zh ? L"垂直双栏" : L"Dual Vertical Panes", L"2", true, true, false, L"Ctrl + Shift + 2" },
        { zh ? L"水平双栏" : L"Dual Horizontal Panes", L"3", true, true, false, L"Ctrl + Shift + 3" },
        { zh ? L"四分栏网格" : L"Quad Panes", L"4", true, true, false, L"Ctrl + Shift + 4" },
        { zh ? L"滚动到列表顶部" : L"Scroll to Top", L"Home", false, false, false, L"Home" },
        { zh ? L"滚动到列表底部" : L"Scroll to Bottom", L"End", false, false, false, L"End" },
        { zh ? L"向上翻页" : L"Page Up", L"PageUp", false, false, false, L"PageUp" },
        { zh ? L"向下翻页" : L"Page Down", L"PageDown", false, false, false, L"PageDown" },
        { zh ? L"全选" : L"Select All", L"A", false, true, false, L"Ctrl + A" },
        { zh ? L"反向选择" : L"Invert Selection", L"I", false, true, false, L"Ctrl + I" },
        { zh ? L"撤销" : L"Undo", L"Z", false, true, false, L"Ctrl + Z" },
        { zh ? L"复制选中项" : L"Copy", L"C", false, true, false, L"Ctrl + C" },
        { zh ? L"剪切选中项" : L"Cut", L"X", false, true, false, L"Ctrl + X" },
        { zh ? L"粘贴" : L"Paste", L"V", false, true, false, L"Ctrl + V" },
        { zh ? L"新建文件夹" : L"New Folder", L"N", true, true, false, L"Ctrl + Shift + N" },
        { zh ? L"属性" : L"Properties", L"Enter", false, false, true, L"Alt + Enter" }
    };
}

static void InitDefaultShortcuts() {
    const auto& saved = Config::Get().shortcuts;
    bool hasValid = false;
    for (const auto& item : saved) {
        if (item.name.length() > 1) {
            hasValid = true;
            break;
        }
    }
    if (hasValid) {
        s_tempShortcuts.clear();
        for (const auto& item : saved) {
            ShortcutItem sc;
            sc.name = item.name;
            sc.key = item.key;
            sc.shift = item.shift;
            sc.ctrl = item.ctrl;
            sc.alt = item.alt;
            sc.keyText = FormatShortcutKey(sc);
            s_tempShortcuts.push_back(sc);
        }
    } else {
        ResetDefaultShortcuts();
    }

    for (const auto& s : ScriptEngine::Get().GetScripts()) {
        if (!s.hotkey.empty()) {
            std::wstring sZh = L"【脚本】" + s.name;
            std::wstring sEn = L"[Script] " + s.name;
            bool exists = false;
            for (const auto& sc : s_tempShortcuts) {
                if (sc.name == sZh || sc.name == sEn || sc.name == s.name) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                ShortcutItem item;
                item.name = I18n::Get().IsChinese() ? sZh : sEn;
                std::wstring hk = s.hotkey;
                size_t plusPos;
                while ((plusPos = hk.find(L"+")) != std::wstring::npos) {
                    std::wstring mod = hk.substr(0, plusPos);
                    while (!mod.empty() && mod.front() == L' ') mod.erase(mod.begin());
                    while (!mod.empty() && mod.back() == L' ') mod.pop_back();
                    if (_wcsicmp(mod.c_str(), L"Ctrl") == 0) item.ctrl = true;
                    else if (_wcsicmp(mod.c_str(), L"Shift") == 0) item.shift = true;
                    else if (_wcsicmp(mod.c_str(), L"Alt") == 0) item.alt = true;
                    hk = hk.substr(plusPos + 1);
                }
                while (!hk.empty() && hk.front() == L' ') hk.erase(hk.begin());
                while (!hk.empty() && hk.back() == L' ') hk.pop_back();
                item.key = hk;
                item.keyText = FormatShortcutKey(item);
                s_tempShortcuts.push_back(item);
            }
        }
    }
}

static void PopulateShortcutList(HWND hList) {
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < s_tempShortcuts.size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)s_tempShortcuts[i].name.c_str();
        ListView_InsertItem(hList, &lvi);
        std::wstring k = FormatShortcutKey(s_tempShortcuts[i]);
        ListView_SetItemText(hList, (int)i, 1, (LPWSTR)k.c_str());
    }
}

void SettingsDialog::Show(HWND hwndParent, std::function<void()> onSettingsChanged, int initialCategory) {
  s_onSettingsChanged = onSettingsChanged;
  s_currentCategory = std::clamp(initialCategory, 0, 9);
  HINSTANCE hInst = GetModuleHandleW(nullptr);

  WNDCLASSEXW wc = {sizeof(wc)};
  if (!GetClassInfoExW(hInst, SETTINGS_CLASS, &wc)) {
    wc.lpfnWndProc = SettingsDialog::WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = SETTINGS_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = Theme::Get().BrushBackground();
    RegisterClassExW(&wc);
  }

  int w = Theme::Get().Scale(860, hwndParent);
  int h = Theme::Get().Scale(680, hwndParent);

  RECT rcParent;
  GetWindowRect(hwndParent, &rcParent);
  int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
  int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

  HWND hwndDlg =
      CreateWindowExW(WS_EX_DLGMODALFRAME, SETTINGS_CLASS,
                      I18n::Get().IsChinese() ? L"NeoMDIE 设置与选项" : L"NeoMDIE Settings & Options",
                      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, x,
                      y, w, h, hwndParent, nullptr, hInst, nullptr);

  Theme::Get().ApplyToWindow(hwndDlg);
  CreateControls(hwndDlg);

  EnableWindow(hwndParent, FALSE);
  ShowWindow(hwndDlg, SW_SHOW);
  UpdateWindow(hwndDlg);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsWindow(hwndDlg))
      break;
    if (!IsDialogMessageW(hwndDlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  EnableWindow(hwndParent, TRUE);
  SetForegroundWindow(hwndParent);
}

void SettingsDialog::SwitchTab(HWND hwnd, int tabIndex) {
  s_currentCategory = tabIndex;
  for (int p = 0; p < 10; ++p) {
    int show = (p == tabIndex) ? SW_SHOW : SW_HIDE;
    for (HWND hCtrl : s_pageControls[p]) {
      if (hCtrl && IsWindow(hCtrl)) {
        ShowWindow(hCtrl, show);
      }
    }
  }
  InvalidateRect(hwnd, nullptr, TRUE);
}

static LRESULT CALLBACK SettingsComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (Theme::Get().IsDarkMode()) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HGDIOBJ oldBM = SelectObject(memDC, memBM);

            FillRect(memDC, &rc, Theme::Get().BrushSurface());

            HPEN hPen = CreatePen(PS_SOLID, 1, Theme::Get().ClrBorder());
            HGDIOBJ oldPen = SelectObject(memDC, hPen);
            SelectObject(memDC, GetStockObject(NULL_BRUSH));
            RoundRect(memDC, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
            SelectObject(memDC, oldPen);
            DeleteObject(hPen);

            int btnW = Theme::Get().Scale(24, hwnd);
            RECT rcArrow = { rc.right - btnW, rc.top, rc.right, rc.bottom };
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, Theme::Get().ClrTextSecondary());
            HFONT hIconFont = Theme::Get().GetIconFont(Theme::Get().Scale(10, hwnd), hwnd);
            HGDIOBJ oldIconFont = SelectObject(memDC, hIconFont);
            wchar_t arrowGlyph[2] = { 0xE70D, 0 };
            DrawTextW(memDC, arrowGlyph, 1, &rcArrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, oldIconFont);
            DeleteObject(hIconFont);

            int curSel = (int)SendMessageW(hwnd, CB_GETCURSEL, 0, 0);
            if (curSel != CB_ERR) {
                int len = (int)SendMessageW(hwnd, CB_GETLBTEXTLEN, curSel, 0);
                if (len > 0) {
                    std::wstring text(len + 1, L'\0');
                    SendMessageW(hwnd, CB_GETLBTEXT, curSel, (LPARAM)text.data());
                    text.resize(len);

                    RECT rcText = rc;
                    rcText.left += Theme::Get().Scale(8, hwnd);
                    rcText.right -= btnW;
                    SetTextColor(memDC, Theme::Get().ClrTextPrimary());
                    HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
                    if (!hFont) hFont = Theme::Get().GetUIFont(hwnd);
                    HGDIOBJ oldFont = SelectObject(memDC, hFont);
                    DrawTextW(memDC, text.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    SelectObject(memDC, oldFont);
                }
            }

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SettingsHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (Theme::Get().IsDarkMode()) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HGDIOBJ oldBM = SelectObject(memDC, memBM);

            FillRect(memDC, &rcClient, Theme::Get().BrushSurface());

            HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
            if (!hFont) hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(memDC, hFont);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, Theme::Get().ClrTextPrimary());

            HPEN hPenBorder = CreatePen(PS_SOLID, 1, Theme::Get().ClrBorder());
            HGDIOBJ oldPen = SelectObject(memDC, hPenBorder);

            int count = Header_GetItemCount(hwnd);
            for (int i = 0; i < count; ++i) {
                RECT rcItem;
                if (!Header_GetItemRect(hwnd, i, &rcItem)) continue;

                // Column divider
                MoveToEx(memDC, rcItem.right - 1, rcItem.top + 3, nullptr);
                LineTo(memDC, rcItem.right - 1, rcItem.bottom - 3);

                wchar_t text[256] = { 0 };
                HDITEMW hdi = { sizeof(hdi) };
                hdi.mask = HDI_TEXT | HDI_FORMAT;
                hdi.pszText = text;
                hdi.cchTextMax = 256;
                if (Header_GetItem(hwnd, i, &hdi)) {
                    RECT rcText = rcItem;
                    rcText.left += Theme::Get().Scale(6, hwnd);
                    rcText.right -= Theme::Get().Scale(6, hwnd);

                    UINT format = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_LEFT;
                    DrawTextW(memDC, text, -1, &rcText, format);
                }
            }

            // Bottom border
            MoveToEx(memDC, 0, rcClient.bottom - 1, nullptr);
            LineTo(memDC, rcClient.right, rcClient.bottom - 1);

            SelectObject(memDC, oldPen);
            DeleteObject(hPenBorder);
            SelectObject(memDC, oldFont);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void SettingsDialog::CreateControls(HWND hwnd) {
  HINSTANCE hInst = GetModuleHandleW(nullptr);
  HFONT hFont = Theme::Get().GetUIFont(hwnd);
  HFONT hFontBold = Theme::Get().GetUIFontBold(hwnd);

  for (int i = 0; i < 10; ++i) {
    s_pageControls[i].clear();
  }

  bool zh = I18n::Get().IsChinese();
  bool isDark = Theme::Get().IsDarkMode();
  Config &cfg = Config::Get();

  s_tempTreeAction = cfg.actionTree;
  s_tempLinksAction = cfg.actionLinks;
  s_tempAddressAction = cfg.actionAddress;
  s_curActionTarget = 0;

  RECT rcClient;
  GetClientRect(hwnd, &rcClient);
  int clientW = rcClient.right - rcClient.left;
  int clientH = rcClient.bottom - rcClient.top;

  int navW = Theme::Get().Scale(160, hwnd);
  int navH = clientH - Theme::Get().Scale(64, hwnd);
  int marginX = Theme::Get().Scale(185, hwnd);
  int contentW = clientW - marginX - Theme::Get().Scale(24, hwnd);
  int lineH = Theme::Get().Scale(24, hwnd);
  int spacing = Theme::Get().Scale(6, hwnd);

  // 1. Left Vertical Navigation Listbox with LBS_HASSTRINGS
  s_hCategoryList = CreateWindowExW(
      0, L"LISTBOX", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | WS_BORDER,
      Theme::Get().Scale(14, hwnd), Theme::Get().Scale(14, hwnd),
      navW, navH,
      hwnd, (HMENU)201, hInst, nullptr);

  SendMessageW(s_hCategoryList, LB_SETITEMHEIGHT, 0, Theme::Get().Scale(36, hwnd));

  for (int i = 0; i < 10; ++i) {
      SendMessageW(s_hCategoryList, LB_ADDSTRING, 0, (LPARAM)(zh ? navTitlesZh[i] : navTitlesEn[i]));
  }
  SendMessageW(s_hCategoryList, LB_SETCURSEL, s_currentCategory, 0);

  // Helpers
  auto addCtrl = [&](int page, HWND hCtrl) {
    s_pageControls[page].push_back(hCtrl);
    return hCtrl;
  };

  auto createLabel = [&](int page, const wchar_t *text, int x, int y, int w, int h, bool bold = false) {
    HWND hLbl = CreateWindowExW(0, L"STATIC", text,
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                x, y, w, h,
                                hwnd, nullptr, hInst, nullptr);
    SendMessageW(hLbl, WM_SETFONT, (WPARAM)(bold ? hFontBold : hFont), TRUE);
    return addCtrl(page, hLbl);
  };

  auto createChk = [&](int page, const wchar_t *text, int y, int id, bool checked) {
    HWND hChk = CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP | WS_CLIPSIBLINGS,
        marginX, y, contentW, lineH, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hChk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTheme(hChk, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    Theme::Get().ApplyToWindow(hChk);
    return addCtrl(page, hChk);
  };

  auto createCombo = [&](int page, int x, int y, int id, int w = 240) {
    HWND hCmb = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS,
        x, y, Theme::Get().Scale(w, hwnd), Theme::Get().Scale(240, hwnd),
        hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hCmb, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (isDark) {
      Theme::Get().ApplyToWindow(hCmb);
      SetWindowTheme(hCmb, L"CFD", nullptr);
      SetWindowSubclass(hCmb, SettingsComboSubclassProc, 1, 0);
      COMBOBOXINFO cbi = { sizeof(cbi) };
      if (GetComboBoxInfo(hCmb, &cbi) && cbi.hwndList) {
        Theme::Get().ApplyToWindow(cbi.hwndList);
        SetWindowTheme(cbi.hwndList, L"DarkMode_Explorer", nullptr);
      }
    } else {
      SetWindowTheme(hCmb, L"Explorer", nullptr);
    }
    return addCtrl(page, hCmb);
  };

  // ----------------------------------------------------
  // Page 0: 常规与启动 (General)
  // ----------------------------------------------------
  int y0 = Theme::Get().Scale(16, hwnd);
  createLabel(0, zh ? L"界面显示语言:" : L"Interface Language:", marginX, y0, contentW, lineH, true);
  y0 += lineH + spacing;

  s_hLangCombo = createCombo(0, marginX, y0, 301, 240);
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"中文 (简体)");
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"English (United States)");
  SendMessageW(s_hLangCombo, CB_SETCURSEL, (cfg.language == 1) ? 1 : 0, 0);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"启动与系统托盘行为:" : L"Startup & System Tray:", marginX, y0, contentW, lineH, true);
  y0 += lineH + spacing;

  s_hChkRestoreTabs = createChk(0, zh ? L"启动时恢复上次会话的所有标签页" : L"Restore tabs from previous session on startup",
                               y0, 302, cfg.restoreTabsOnStartup);
  y0 += lineH + spacing;

  s_hChkMinToTray = createChk(0, zh ? L"最小化窗口时缩至系统托盘区" : L"Minimize window to system tray",
                             y0, 303, cfg.minimizeToTray);
  y0 += lineH + spacing;

  s_hChkCloseToTray = createChk(0, zh ? L"点击窗口关闭按钮时最小化至托盘" : L"Close button minimizes to tray",
                               y0, 304, cfg.closeToTray);
  y0 += lineH + spacing;

  s_hChkSingleInstance = createChk(0, zh ? L"只运行一个 NeoMDIE 程序实例 (单实例模式)" : L"Single instance mode (allow only one running instance)",
                                   y0, 306, cfg.singleInstance);
  y0 += lineH + spacing;

  s_hChkReplaceExplorer = createChk(0, zh ? L"替代系统资源管理器作为默认文件管理器" : L"Replace Windows Explorer as default file manager",
                                   y0, 305, ShellUtils::IsExplorerReplaced());

  // ----------------------------------------------------
  // Page 1: 窗口动作设置 (Window Actions - Pure Labels)
  // ----------------------------------------------------
  int yAct = Theme::Get().Scale(16, hwnd);
  createLabel(1, zh ? L"目标对象:" : L"Target Object:", marginX, yAct, contentW, lineH, true);
  yAct += lineH + spacing;

  s_hActionTargetCombo = createCombo(1, marginX, yAct, 401, 270);
  SendMessageW(s_hActionTargetCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"侧边栏目录树" : L"Sidebar Directory Tree"));
  SendMessageW(s_hActionTargetCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"快捷链接栏" : L"Links Bar"));
  SendMessageW(s_hActionTargetCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"地址栏导航" : L"Address Bar"));
  SendMessageW(s_hActionTargetCombo, CB_SETCURSEL, s_curActionTarget, 0);
  yAct += lineH + spacing * 2;

  createLabel(1, zh ? L"执行动作与鼠标按键触发设置:" : L"Action & Trigger Settings:", marginX, yAct, contentW, lineH, true);
  yAct += lineH + spacing;

  const wchar_t* actionLabelsZh[5] = {
      L"在当前栏打开:",
      L"在新标签页打开:",
      L"在后台新标签页打开:",
      L"在其他分栏中打开:",
      L"在系统资源管理器中打开:"
  };
  const wchar_t* actionLabelsEn[5] = {
      L"Open in Current Pane:",
      L"Open in New Tab:",
      L"Open in Background Tab:",
      L"Open in Other Pane:",
      L"Open in Explorer:"
  };

  const wchar_t* triggerOptionsZh[13] = {
      L"无",
      L"鼠标左键单击",
      L"鼠标左键双击",
      L"鼠标中键单击",
      L"Shift + 单击",
      L"Ctrl + 单击",
      L"Alt + 单击",
      L"Shift + Ctrl + 单击",
      L"Shift + Alt + 单击",
      L"Ctrl + Alt + 单击",
      L"Shift + Ctrl + Alt + 单击",
      L"Shift + 中键单击",
      L"Ctrl + 中键单击"
  };
  const wchar_t* triggerOptionsEn[13] = {
      L"None",
      L"Left Click",
      L"Double Click",
      L"Middle Click",
      L"Shift + Click",
      L"Ctrl + Click",
      L"Alt + Click",
      L"Shift + Ctrl + Click",
      L"Shift + Alt + Click",
      L"Ctrl + Alt + Click",
      L"Shift + Ctrl + Alt + Click",
      L"Shift + Middle Click",
      L"Ctrl + Middle Click"
  };

  int lblW = Theme::Get().Scale(160, hwnd);
  int comboW = Theme::Get().Scale(270, hwnd);

  for (int i = 0; i < 5; ++i) {
      int rowY = yAct + i * Theme::Get().Scale(34, hwnd);
      createLabel(1, zh ? actionLabelsZh[i] : actionLabelsEn[i], marginX, rowY + Theme::Get().Scale(3, hwnd), lblW, lineH);
      s_hActionCombos[i] = createCombo(1, marginX + lblW + Theme::Get().Scale(10, hwnd), rowY, 410 + i, 270);
      for (int opt = 0; opt < 13; ++opt) {
          SendMessageW(s_hActionCombos[i], CB_ADDSTRING, 0, (LPARAM)(zh ? triggerOptionsZh[opt] : triggerOptionsEn[opt]));
      }
  }
  UpdateActionCombosForCurrentTarget();

  // ----------------------------------------------------
  // Page 2: 窗口与多栏 (Window & Multi-Pane)
  // ----------------------------------------------------
  int y1 = Theme::Get().Scale(16, hwnd);
  createLabel(2, zh ? L"默认分栏模式:" : L"Default Multi-Pane Mode:", marginX, y1, contentW, lineH, true);
  y1 += lineH + spacing;

  s_hLayoutCombo = createCombo(2, marginX, y1, 310, 240);
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"单栏 (1x1)" : L"Single (1x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"双栏 (1x2 垂直)" : L"Dual Vertical (1x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"双栏 (2x1 水平)" : L"Dual Horizontal (2x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"四栏 (2x2 网格)" : L"Quad (2x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"三栏 (1左2右)" : L"Three Panes (1+2)"));
  SendMessageW(s_hLayoutCombo, CB_SETCURSEL, std::clamp(cfg.paneLayout, 0, 4), 0);
  y1 += lineH + spacing * 2;

  createLabel(2, zh ? L"行为与布局选项:" : L"Pane Behavior & Layout Options:", marginX, y1, contentW, lineH, true);
  y1 += lineH + spacing;

  s_hChkSyncNav = createChk(2, zh ? L"同步导航 (所有分栏跟随当前路径)" : L"Synchronize navigation across all panes",
                            y1, 311, cfg.syncNav);
  y1 += lineH + spacing;

  s_hChkLockToolbars = createChk(2, zh ? L"锁定工具栏位置与尺寸 (禁止手动拉伸)" : L"Lock toolbar layout and widths",
                                 y1, 312, cfg.lockToolbars);

  // ----------------------------------------------------
  // Page 3: 标签页偏好 (Tabs)
  // ----------------------------------------------------
  int y2 = Theme::Get().Scale(16, hwnd);
  createLabel(3, zh ? L"标签页基本行为:" : L"Tab Behavior:", marginX, y2, contentW, lineH, true);
  y2 += lineH + spacing;

  s_hChkAllowDupTabs = createChk(3, zh ? L"允许打开重复路径的标签页" : L"Allow duplicate folder tabs",
                                y2, 320, cfg.allowDuplicateTabs);
  y2 += lineH + spacing;

  createLabel(3, zh ? L"双击标签页时的操作:" : L"Double-Click Tab Action:", marginX, y2, contentW, lineH, true);
  y2 += lineH + spacing;

  s_hTabDblClickCombo = createCombo(3, marginX, y2, 321, 240);
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"关闭该标签页" : L"Close Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"刷新该标签页" : L"Refresh Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"锁定/解锁标签页" : L"Toggle Pin Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"无动作" : L"Do Nothing"));
  SendMessageW(s_hTabDblClickCombo, CB_SETCURSEL, std::clamp(cfg.tabDoubleClickAction, 0, 3), 0);

  // ----------------------------------------------------
  // Page 4: 侧边栏与树状 (Tree & Sidebar)
  // ----------------------------------------------------
  int y3 = Theme::Get().Scale(16, hwnd);
  s_hChkTree = createChk(4, zh ? L"显示侧边栏目录树" : L"Show directory tree sidebar",
                         y3, 330, cfg.showTree);
  y3 += lineH + spacing;

  s_hChkAutoExpandTree = createChk(4, zh ? L"导航时侧边栏自动展开并定位当前文件夹" : L"Auto-expand and select active folder in tree",
                                  y3, 331, cfg.autoExpandTree);
  y3 += lineH + spacing;

  s_hChkPreview = createChk(4, zh ? L"开启右侧文件快速预览窗格" : L"Enable file preview pane",
                           y3, 332, cfg.previewEnabled);

  // ----------------------------------------------------
  // Page 5: 视图与外观 (View & Appearance)
  // ----------------------------------------------------
  int y4 = Theme::Get().Scale(16, hwnd);
  createLabel(5, zh ? L"默认文件列表视图模式:" : L"Default View Mode:", marginX, y4, contentW, lineH, true);
  y4 += lineH + spacing;

  s_hViewModeCombo = createCombo(5, marginX, y4, 340, 240);
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"详细信息" : L"Details"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"大图标" : L"Large Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"小图标" : L"Small Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"列表" : L"List"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"平铺" : L"Tiles"));
  SendMessageW(s_hViewModeCombo, CB_SETCURSEL, std::clamp(cfg.viewMode, 0, 4), 0);
  y4 += lineH + spacing * 2;

  createLabel(5, zh ? L"列表显示选项:" : L"List Display Options:", marginX, y4, contentW, lineH, true);
  y4 += lineH + spacing;

  s_hChkHidden = createChk(5, zh ? L"显示隐藏文件与文件夹" : L"Show hidden files and folders",
                          y4, 341, cfg.showHidden);
  y4 += lineH + spacing;

  s_hChkFoldersFirst = createChk(5, zh ? L"文件夹排在前面（取消则文件排在前面）" : L"Show folders first (uncheck for files first)",
                                y4, 345, cfg.foldersFirst);
  y4 += lineH + spacing;

  s_hChkCalcSize = createChk(5, zh ? L"后台自动计算文件夹真实大小" : L"Calculate folder sizes in background",
                            y4, 342, cfg.calcFolderSize);
  y4 += lineH + spacing;

  s_hChkGridLines = createChk(5, zh ? L"显示行间网格线" : L"Show grid lines between rows",
                              y4, 344, cfg.showGridLines);

  // ----------------------------------------------------
  // Page 6: 搜索与过滤 (Search & Filter)
  // ----------------------------------------------------
  int y5 = Theme::Get().Scale(16, hwnd);
  s_hChkSearchSubfolders = createChk(6, zh ? L"搜索过滤时默认递归包含子文件夹" : L"Include subfolders in search filter by default",
                                    y5, 350, cfg.searchSubfoldersDefault);
  y5 += lineH + spacing;

  s_hChkMatchCase = createChk(6, zh ? L"区分大小写" : L"Case sensitive search",
                              y5, 351, false);

  // ----------------------------------------------------
  // Page 7: 快捷键自定义 (Hotkeys - MDIE Design)
  // ----------------------------------------------------
  int ySc = Theme::Get().Scale(14, hwnd);
  createLabel(7, zh ? L"快捷键列表:" : L"Shortcuts List:", marginX, ySc, contentW, lineH, true);
  ySc += lineH + spacing;

  InitDefaultShortcuts();

  int scListH = Theme::Get().Scale(220, hwnd);
  s_hShortcutList = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
      marginX, ySc, contentW, scListH,
      hwnd, (HMENU)355, hInst, nullptr);
  ListView_SetExtendedListViewStyle(s_hShortcutList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  SendMessageW(s_hShortcutList, WM_SETFONT, (WPARAM)hFont, TRUE);

  // Apply dark mode theme to shortcut list
  COLORREF bgListClr = isDark ? Theme::Get().ClrSurface() : RGB(255, 255, 255);
  COLORREF textListClr = Theme::Get().ClrTextPrimary();
  ListView_SetBkColor(s_hShortcutList, bgListClr);
  ListView_SetTextBkColor(s_hShortcutList, bgListClr);
  ListView_SetTextColor(s_hShortcutList, textListClr);
  if (isDark) {
    Theme::Get().ApplyToWindow(s_hShortcutList);
    SetWindowTheme(s_hShortcutList, L"DarkMode_Explorer", nullptr);
  } else {
    SetWindowTheme(s_hShortcutList, L"Explorer", nullptr);
  }
  HWND hScHdr = ListView_GetHeader(s_hShortcutList);
  if (hScHdr) {
      SetWindowTheme(hScHdr, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
      SetWindowSubclass(hScHdr, SettingsHeaderSubclassProc, 1, 0);
  }

  LVCOLUMNW lvcSc = { 0 };
  lvcSc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
  lvcSc.fmt = LVCFMT_LEFT;
  lvcSc.cx = Theme::Get().Scale(240, hwnd);
  lvcSc.pszText = (LPWSTR)(zh ? L"功能名称" : L"Action Name");
  ListView_InsertColumn(s_hShortcutList, 0, &lvcSc);

  lvcSc.cx = Theme::Get().Scale(220, hwnd);
  lvcSc.pszText = (LPWSTR)(zh ? L"按键" : L"Shortcut Key");
  ListView_InsertColumn(s_hShortcutList, 1, &lvcSc);

  PopulateShortcutList(s_hShortcutList);
  addCtrl(7, s_hShortcutList);

  // MDIE Style Editing controls below list
  int editY = ySc + scListH + Theme::Get().Scale(10, hwnd);
  createLabel(7, zh ? L"功能名称:" : L"Action:", marginX, editY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);
  s_hScActionCombo = createCombo(7, marginX + Theme::Get().Scale(75, hwnd), editY, 354, 220);
  for (size_t a = 0; a < sizeof(s_allActions)/sizeof(s_allActions[0]); ++a) {
      SendMessageW(s_hScActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? s_allActions[a].nameZh : s_allActions[a].nameEn));
  }
  for (const auto& s : ScriptEngine::Get().GetScripts()) {
      std::wstring sName = (zh ? L"【脚本】" : L"[Script] ") + s.name;
      SendMessageW(s_hScActionCombo, CB_ADDSTRING, 0, (LPARAM)sName.c_str());
  }
  SendMessageW(s_hScActionCombo, CB_SETCURSEL, 0, 0);

  editY += Theme::Get().Scale(32, hwnd);
  createLabel(7, zh ? L"按键:" : L"Key:", marginX, editY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);
  s_hScKeyCombo = createCombo(7, marginX + Theme::Get().Scale(75, hwnd), editY, 355, 100);
  for (size_t k = 0; k < sizeof(s_keyChoices)/sizeof(s_keyChoices[0]); ++k) {
      SendMessageW(s_hScKeyCombo, CB_ADDSTRING, 0, (LPARAM)s_keyChoices[k]);
  }
  SendMessageW(s_hScKeyCombo, CB_SETCURSEL, 0, 0);

  int chkX = marginX + Theme::Get().Scale(185, hwnd);
  s_hScShift = CreateWindowExW(0, L"BUTTON", L"Shift", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                              chkX, editY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(65, hwnd), lineH, hwnd, (HMENU)367, hInst, nullptr);
  SendMessageW(s_hScShift, WM_SETFONT, (WPARAM)hFont, TRUE);
  SetWindowTheme(s_hScShift, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
  Theme::Get().ApplyToWindow(s_hScShift);
  addCtrl(7, s_hScShift);

  chkX += Theme::Get().Scale(70, hwnd);
  s_hScCtrl = CreateWindowExW(0, L"BUTTON", L"Ctrl", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                             chkX, editY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(60, hwnd), lineH, hwnd, (HMENU)368, hInst, nullptr);
  SendMessageW(s_hScCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
  SetWindowTheme(s_hScCtrl, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
  Theme::Get().ApplyToWindow(s_hScCtrl);
  addCtrl(7, s_hScCtrl);

  chkX += Theme::Get().Scale(65, hwnd);
  s_hScAlt = CreateWindowExW(0, L"BUTTON", L"Alt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                            chkX, editY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(55, hwnd), lineH, hwnd, (HMENU)369, hInst, nullptr);
  SendMessageW(s_hScAlt, WM_SETFONT, (WPARAM)hFont, TRUE);
  SetWindowTheme(s_hScAlt, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
  Theme::Get().ApplyToWindow(s_hScAlt);
  addCtrl(7, s_hScAlt);

  // Buttons Row
  editY += Theme::Get().Scale(34, hwnd);
  int btnW = Theme::Get().Scale(80, hwnd);
  int btnH = Theme::Get().Scale(28, hwnd);
  int btnX = marginX;

  s_hBtnScAdd = CreateWindowExW(0, L"BUTTON", zh ? L"增加(&A)" : L"Add(&A)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                               btnX, editY, btnW, btnH, hwnd, (HMENU)356, hInst, nullptr);
  SendMessageW(s_hBtnScAdd, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(7, s_hBtnScAdd);

  btnX += btnW + Theme::Get().Scale(8, hwnd);
  s_hBtnScMod = CreateWindowExW(0, L"BUTTON", zh ? L"修改(&M)" : L"Modify(&M)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                               btnX, editY, btnW, btnH, hwnd, (HMENU)357, hInst, nullptr);
  SendMessageW(s_hBtnScMod, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(7, s_hBtnScMod);

  btnX += btnW + Theme::Get().Scale(8, hwnd);
  s_hBtnScDel = CreateWindowExW(0, L"BUTTON", zh ? L"删除(&D)" : L"Delete(&D)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                               btnX, editY, btnW, btnH, hwnd, (HMENU)358, hInst, nullptr);
  SendMessageW(s_hBtnScDel, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(7, s_hBtnScDel);

  btnX += btnW + Theme::Get().Scale(8, hwnd);
  s_hBtnScReset = CreateWindowExW(0, L"BUTTON", zh ? L"恢复默认(&R)" : L"Reset(&R)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                 btnX, editY, btnW + Theme::Get().Scale(16, hwnd), btnH, hwnd, (HMENU)359, hInst, nullptr);
  SendMessageW(s_hBtnScReset, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(7, s_hBtnScReset);

  // ----------------------------------------------------
  // Page 8: 鼠标手势设置 (Mouse Gestures - MDIE Design)
  // ----------------------------------------------------
  int y6 = Theme::Get().Scale(14, hwnd);
  s_hChkGestures = createChk(8, zh ? L"启用鼠标手势 (按住鼠标右键拖动)" : L"Enable mouse gestures (Hold Right-Click and drag)",
                             y6, 360, cfg.gesturesEnabled);
  y6 += lineH + spacing;

  s_tempGestures = cfg.gestures;
  bool gestCorrupted = false;
  for (const auto& g : s_tempGestures) {
      if (g.name.length() <= 1) { gestCorrupted = true; break; }
  }
  if (s_tempGestures.empty() || gestCorrupted) {
      s_tempGestures = {
          { "L", (int)GestureAction::NavBack, zh ? L"后退" : L"Back" },
          { "R", (int)GestureAction::NavForward, zh ? L"前进" : L"Forward" },
          { "U", (int)GestureAction::NavUp, zh ? L"向上一级" : L"Up to Parent" },
          { "D", (int)GestureAction::Refresh, zh ? L"刷新" : L"Refresh" },
          { "UD", (int)GestureAction::NewTab, zh ? L"新建标签页" : L"New Tab" },
          { "DR", (int)GestureAction::CloseTab, zh ? L"关闭当前标签页" : L"Close Current Tab" },
          { "UL", (int)GestureAction::ToggleTree, zh ? L"显示/隐藏侧边栏目录树" : L"Toggle Tree Sidebar" },
          { "UR", (int)GestureAction::TogglePreview, zh ? L"显示/隐藏快速预览窗格" : L"Toggle Preview Pane" },
          { "RU", (int)GestureAction::ScrollTop, zh ? L"滚动到列表顶部" : L"Scroll to Top" },
          { "RD", (int)GestureAction::ScrollBottom, zh ? L"滚动到列表底部" : L"Scroll to Bottom" },
          { "LU", (int)GestureAction::PageUp, zh ? L"向上翻页" : L"Page Up" },
          { "LD", (int)GestureAction::PageDown, zh ? L"向下翻页" : L"Page Down" }
      };
  }
  int gestListH = Theme::Get().Scale(210, hwnd);
  s_hGestureList = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
      marginX, y6, contentW, gestListH,
      hwnd, (HMENU)361, hInst, nullptr);
  ListView_SetExtendedListViewStyle(s_hGestureList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  SendMessageW(s_hGestureList, WM_SETFONT, (WPARAM)hFont, TRUE);

  // Dark mode theme for gesture list
  ListView_SetBkColor(s_hGestureList, bgListClr);
  ListView_SetTextBkColor(s_hGestureList, bgListClr);
  ListView_SetTextColor(s_hGestureList, textListClr);
  if (isDark) {
    Theme::Get().ApplyToWindow(s_hGestureList);
    SetWindowTheme(s_hGestureList, L"DarkMode_Explorer", nullptr);
  } else {
    SetWindowTheme(s_hGestureList, L"Explorer", nullptr);
  }
  HWND hGestHdr = ListView_GetHeader(s_hGestureList);
  if (hGestHdr) {
      SetWindowTheme(hGestHdr, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
      SetWindowSubclass(hGestHdr, SettingsHeaderSubclassProc, 1, 0);
  }

  LVCOLUMNW lvcG = { 0 };
  lvcG.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
  lvcG.fmt = LVCFMT_LEFT;
  lvcG.cx = Theme::Get().Scale(150, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"手势轨迹" : L"Gesture Pattern");
  ListView_InsertColumn(s_hGestureList, 0, &lvcG);

  lvcG.cx = Theme::Get().Scale(230, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"执行功能" : L"Action");
  ListView_InsertColumn(s_hGestureList, 1, &lvcG);

  lvcG.cx = Theme::Get().Scale(110, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"场所" : L"Scope");
  ListView_InsertColumn(s_hGestureList, 2, &lvcG);

  PopulateGestureList(s_hGestureList);
  addCtrl(8, s_hGestureList);

  // MDIE Style Gesture Edit Controls below list
  int gEditY = y6 + gestListH + Theme::Get().Scale(10, hwnd);
  createLabel(8, zh ? L"手势轨迹:" : L"Pattern:", marginX, gEditY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);
  s_hGestPatternCombo = createCombo(8, marginX + Theme::Get().Scale(75, hwnd), gEditY, 377, 160);
  for (size_t p = 0; p < sizeof(s_allPatterns)/sizeof(s_allPatterns[0]); ++p) {
      SendMessageW(s_hGestPatternCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? s_allPatterns[p].labelZh : s_allPatterns[p].labelEn));
  }
  SendMessageW(s_hGestPatternCombo, CB_SETCURSEL, 0, 0);

  createLabel(8, zh ? L"执行功能:" : L"Action:", marginX + Theme::Get().Scale(245, hwnd), gEditY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);
  s_hGestActionCombo = createCombo(8, marginX + Theme::Get().Scale(320, hwnd), gEditY, 378, 180);
  for (size_t a = 0; a < sizeof(s_allActions)/sizeof(s_allActions[0]); ++a) {
      int idx = (int)SendMessageW(s_hGestActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? s_allActions[a].nameZh : s_allActions[a].nameEn));
      SendMessageW(s_hGestActionCombo, CB_SETITEMDATA, idx, (LPARAM)s_allActions[a].id);
  }
  for (const auto& s : ScriptEngine::Get().GetScripts()) {
      std::wstring sName = (zh ? L"【脚本】" : L"[Script] ") + s.name;
      int idx = (int)SendMessageW(s_hGestActionCombo, CB_ADDSTRING, 0, (LPARAM)sName.c_str());
      SendMessageW(s_hGestActionCombo, CB_SETITEMDATA, idx, (LPARAM)s.commandId);
  }
  SendMessageW(s_hGestActionCombo, CB_SETCURSEL, 0, 0);

  gEditY += Theme::Get().Scale(34, hwnd);
  createLabel(8, zh ? L"适用场所:" : L"Scope:", marginX, gEditY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);
  s_hGestContextCombo = createCombo(8, marginX + Theme::Get().Scale(75, hwnd), gEditY, 379, 160);
  SendMessageW(s_hGestContextCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"全局 / 列表" : L"Global / List"));
  SendMessageW(s_hGestContextCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"仅文件列表" : L"List Only"));
  SendMessageW(s_hGestContextCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"仅侧边栏" : L"Tree Only"));
  SendMessageW(s_hGestContextCombo, CB_SETCURSEL, 0, 0);

  // Gesture Buttons
  int gBtnX = marginX + Theme::Get().Scale(245, hwnd);
  int gBtnW = Theme::Get().Scale(65, hwnd);

  s_hBtnGestAdd = CreateWindowExW(0, L"BUTTON", zh ? L"增加(&A)" : L"Add(&A)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                 gBtnX, gEditY, gBtnW, btnH, hwnd, (HMENU)363, hInst, nullptr);
  SendMessageW(s_hBtnGestAdd, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestAdd);

  gBtnX += gBtnW + Theme::Get().Scale(6, hwnd);
  s_hBtnGestMod = CreateWindowExW(0, L"BUTTON", zh ? L"修改(&M)" : L"Modify(&M)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                 gBtnX, gEditY, gBtnW, btnH, hwnd, (HMENU)364, hInst, nullptr);
  SendMessageW(s_hBtnGestMod, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestMod);

  gBtnX += gBtnW + Theme::Get().Scale(6, hwnd);
  s_hBtnGestDel = CreateWindowExW(0, L"BUTTON", zh ? L"删除(&D)" : L"Delete(&D)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                 gBtnX, gEditY, gBtnW, btnH, hwnd, (HMENU)365, hInst, nullptr);
  SendMessageW(s_hBtnGestDel, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestDel);

  gBtnX += gBtnW + Theme::Get().Scale(6, hwnd);
  s_hBtnGestReset = CreateWindowExW(0, L"BUTTON", zh ? L"恢复默认(&R)" : L"Reset(&R)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                   gBtnX, gEditY, gBtnW + Theme::Get().Scale(24, hwnd), btnH, hwnd, (HMENU)366, hInst, nullptr);
  SendMessageW(s_hBtnGestReset, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestReset);

  // ----------------------------------------------------
  // Page 9: 工具栏配置 (Appearance & Toolbars)
  // ----------------------------------------------------
  int y7 = Theme::Get().Scale(16, hwnd);
  createLabel(9, zh ? L"深浅主题外观:" : L"Theme & Dark Mode:", marginX, y7, contentW, lineH, true);
  y7 += lineH + spacing;

  s_hThemeCombo = createCombo(9, marginX, y7, 370, 240);
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"跟随系统设置" : L"System Default"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"浅色模式" : L"Light Mode"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"深色模式" : L"Dark Mode"));
  int thSel = (cfg.themeMode == -1) ? 0 : ((cfg.themeMode == 1) ? 2 : 1);
  SendMessageW(s_hThemeCombo, CB_SETCURSEL, thSel, 0);
  y7 += lineH + spacing * 2;

  createLabel(9, zh ? L"工具栏与界面元素可见性:" : L"Toolbars & Interface Elements:", marginX, y7, contentW, lineH, true);
  y7 += lineH + spacing;

  s_hChkTBStandard = createChk(9, zh ? L"显示标准按钮工具栏 (前进、后退、刷新、新建)" : L"Show standard navigation buttons",
                               y7, 371, cfg.showToolbarStandard);
  y7 += lineH + spacing;

  s_hChkTBAddress = createChk(9, zh ? L"显示地址栏" : L"Show address bar & breadcrumbs",
                              y7, 372, cfg.showToolbarAddress);
  y7 += lineH + spacing;

  s_hChkTBSearch = createChk(9, zh ? L"显示搜索过滤栏" : L"Show filter & search bar",
                             y7, 373, cfg.showToolbarSearch);
  y7 += lineH + spacing;

  s_hChkTBLinks = createChk(9, zh ? L"显示快捷链接栏" : L"Show links bar",
                            y7, 374, cfg.showToolbarLinks);
  y7 += lineH + spacing;

  s_hChkStatusBar = createChk(9, zh ? L"显示底部状态栏" : L"Show status bar with item counts & size",
                             y7, 376, cfg.showStatusBar);

  // Initial tab page display
  SwitchTab(hwnd, s_currentCategory);

  // Bottom OK & Cancel buttons with generous padding
  int dlgBtnW = Theme::Get().Scale(95, hwnd);
  int dlgBtnH = Theme::Get().Scale(32, hwnd);
  int dlgBtnY = clientH - Theme::Get().Scale(48, hwnd);

  HWND hBtnOK = CreateWindowExW(
      0, L"BUTTON", I18n::Get().T(StringId::SettingsBtnOK),
      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
      clientW - Theme::Get().Scale(220, hwnd), dlgBtnY, dlgBtnW, dlgBtnH,
      hwnd, (HMENU)IDOK, hInst, nullptr);
  SendMessageW(hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);

  HWND hBtnCancel = CreateWindowExW(
      0, L"BUTTON", I18n::Get().T(StringId::SettingsBtnCancel),
      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
      clientW - Theme::Get().Scale(115, hwnd), dlgBtnY, dlgBtnW, dlgBtnH,
      hwnd, (HMENU)IDCANCEL, hInst, nullptr);
  SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void SettingsDialog::SaveSettings(HWND hwnd) {
  Config &cfg = Config::Get();

  // Page 0: General
  int langSel = (int)SendMessageW(s_hLangCombo, CB_GETCURSEL, 0, 0);
  Language newLang = (langSel == 1) ? Language::English : Language::Chinese;
  I18n::Get().SetLanguage(newLang);
  cfg.language = (int)newLang;

  cfg.restoreTabsOnStartup = (SendMessageW(s_hChkRestoreTabs, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.minimizeToTray = (SendMessageW(s_hChkMinToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.closeToTray = (SendMessageW(s_hChkCloseToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.singleInstance = (SendMessageW(s_hChkSingleInstance, BM_GETCHECK, 0, 0) == BST_CHECKED);

  bool replaceExp = (SendMessageW(s_hChkReplaceExplorer, BM_GETCHECK, 0, 0) == BST_CHECKED);
  if (replaceExp != ShellUtils::IsExplorerReplaced()) {
      ShellUtils::SetExplorerReplaced(replaceExp);
  }

  // Page 1: Window Actions
  SaveActionCombosForCurrentTarget();
  cfg.actionTree = s_tempTreeAction;
  cfg.actionLinks = s_tempLinksAction;
  cfg.actionAddress = s_tempAddressAction;

  // Page 2: Window & Panes
  cfg.paneLayout = (int)SendMessageW(s_hLayoutCombo, CB_GETCURSEL, 0, 0);
  cfg.syncNav = (SendMessageW(s_hChkSyncNav, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.lockToolbars = (SendMessageW(s_hChkLockToolbars, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 3: Tabs
  cfg.allowDuplicateTabs = (SendMessageW(s_hChkAllowDupTabs, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.tabDoubleClickAction = (int)SendMessageW(s_hTabDblClickCombo, CB_GETCURSEL, 0, 0);

  // Page 4: Tree & Sidebar
  cfg.showTree = (SendMessageW(s_hChkTree, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.autoExpandTree = (SendMessageW(s_hChkAutoExpandTree, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.previewEnabled = (SendMessageW(s_hChkPreview, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 5: View & File List
  cfg.viewMode = (int)SendMessageW(s_hViewModeCombo, CB_GETCURSEL, 0, 0);
  cfg.showHidden = (SendMessageW(s_hChkHidden, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.foldersFirst = (SendMessageW(s_hChkFoldersFirst, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.calcFolderSize = (SendMessageW(s_hChkCalcSize, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.showGridLines = (SendMessageW(s_hChkGridLines, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 6: Search & Filter
  cfg.searchSubfoldersDefault = (SendMessageW(s_hChkSearchSubfolders, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 7: Shortcuts
  cfg.shortcuts.clear();
  for (const auto& item : s_tempShortcuts) {
      ShortcutItemConfig sc;
      sc.name = item.name;
      sc.key = item.key;
      sc.shift = item.shift;
      sc.ctrl = item.ctrl;
      sc.alt = item.alt;
      cfg.shortcuts.push_back(sc);
  }

  // Page 8: Gestures
  cfg.gesturesEnabled = (SendMessageW(s_hChkGestures, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.gestures = s_tempGestures;

  // Page 9: Appearance & Toolbars
  int themeSel = (int)SendMessageW(s_hThemeCombo, CB_GETCURSEL, 0, 0);
  if (themeSel == 0) cfg.themeMode = -1;
  else if (themeSel == 1) cfg.themeMode = 0;
  else if (themeSel == 2) cfg.themeMode = 1;

  cfg.showToolbarStandard = (SendMessageW(s_hChkTBStandard, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.showToolbarAddress = (SendMessageW(s_hChkTBAddress, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.showToolbarSearch = (SendMessageW(s_hChkTBSearch, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.showToolbarLinks = (SendMessageW(s_hChkTBLinks, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.showStatusBar = (SendMessageW(s_hChkStatusBar, BM_GETCHECK, 0, 0) == BST_CHECKED);

  cfg.Save();

  if (s_onSettingsChanged) {
    s_onSettingsChanged();
  }
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_MEASUREITEM: {
    MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
    if (pmis && pmis->CtlID == 201) {
      pmis->itemHeight = Theme::Get().Scale(36, hwnd);
      return TRUE;
    }
    break;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
    if (pdis && pdis->CtlType == ODT_BUTTON) {
      bool isDown = (pdis->itemState & ODS_SELECTED) != 0;
      bool isDark = Theme::Get().IsDarkMode();
      HBRUSH hBg = isDark ? (isDown ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface())
                          : (isDown ? Theme::Get().BrushSelected() : CreateSolidBrush(RGB(240, 242, 245)));
      FillRect(pdis->hDC, &pdis->rcItem, hBg);

      HPEN hPen = CreatePen(PS_SOLID, 1, isDark ? Theme::Get().ClrBorder() : RGB(180, 185, 195));
      HGDIOBJ oldPen = SelectObject(pdis->hDC, hPen);
      SelectObject(pdis->hDC, GetStockObject(NULL_BRUSH));
      RoundRect(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom, 4, 4);
      SelectObject(pdis->hDC, oldPen);
      DeleteObject(hPen);
      if (!isDark) DeleteObject(hBg);

      wchar_t btnText[128] = { 0 };
      GetWindowTextW(pdis->hwndItem, btnText, 128);
      SetBkMode(pdis->hDC, TRANSPARENT);
      SetTextColor(pdis->hDC, isDark ? (isDown ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary())
                                     : (isDown ? RGB(255, 255, 255) : RGB(20, 20, 20)));
      HFONT hFont = Theme::Get().GetUIFont(hwnd);
      HGDIOBJ oldFont = SelectObject(pdis->hDC, hFont);
      DrawTextW(pdis->hDC, btnText, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(pdis->hDC, oldFont);
      return TRUE;
    }
    if (pdis && pdis->CtlID == 201) {
      bool isSelected = (pdis->itemState & ODS_SELECTED);
      bool isDark = Theme::Get().IsDarkMode();

      COLORREF bgClr = isSelected ? (isDark ? RGB(0, 90, 158) : RGB(0, 120, 215))
                                  : Theme::Get().ClrSurface();
      COLORREF textColor = isSelected ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary();

      HBRUSH hbg = CreateSolidBrush(bgClr);
      FillRect(pdis->hDC, &pdis->rcItem, hbg);
      DeleteObject(hbg);

      SetTextColor(pdis->hDC, textColor);
      SetBkMode(pdis->hDC, TRANSPARENT);

      HFONT hFont = isSelected ? Theme::Get().GetUIFontBold(hwnd) : Theme::Get().GetUIFont(hwnd);
      HGDIOBJ oldFont = SelectObject(pdis->hDC, hFont);

      const wchar_t* title = L"";
      bool zh = I18n::Get().IsChinese();
      if (pdis->itemID >= 0 && pdis->itemID < 10) {
        title = zh ? navTitlesZh[pdis->itemID] : navTitlesEn[pdis->itemID];
      }

      RECT rcText = pdis->rcItem;
      rcText.left += Theme::Get().Scale(16, hwnd);
      DrawTextW(pdis->hDC, title, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

      SelectObject(pdis->hDC, oldFont);
      return TRUE;
    }
    break;
  }
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLORBTN: {
    HDC hdc = (HDC)wParam;
    HWND hCtrl = (HWND)lParam;
    wchar_t clsName[32] = { 0 };
    GetClassNameW(hCtrl, clsName, 32);
    if (_wcsicmp(clsName, L"COMBOBOX") == 0) {
      SetTextColor(hdc, Theme::Get().ClrTextPrimary());
      SetBkColor(hdc, Theme::Get().ClrSurface());
      return (INT_PTR)Theme::Get().BrushSurface();
    }
    SetTextColor(hdc, Theme::Get().ClrTextPrimary());
    SetBkColor(hdc, Theme::Get().ClrBackground());
    return (INT_PTR)Theme::Get().BrushBackground();
  }
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORLISTBOX: {
    HDC hdc = (HDC)wParam;
    SetTextColor(hdc, Theme::Get().ClrTextPrimary());
    SetBkColor(hdc, Theme::Get().ClrSurface());
    return (INT_PTR)Theme::Get().BrushSurface();
  }
  case WM_CTLCOLORSCROLLBAR: {
    HDC hdc = (HDC)wParam;
    SetTextColor(hdc, Theme::Get().ClrTextPrimary());
    SetBkColor(hdc, Theme::Get().ClrSurface());
    return (INT_PTR)Theme::Get().BrushSurface();
  }
  case WM_ERASEBKGND: {
    HDC hdc = (HDC)wParam;
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::Get().BrushBackground());
    return 1;
  }
  case WM_NOTIFY: {
    NMHDR* pnm = (NMHDR*)lParam;
    if (pnm->code == NM_CUSTOMDRAW) {
      if (pnm->hwndFrom == ListView_GetHeader(s_hShortcutList) ||
          pnm->hwndFrom == ListView_GetHeader(s_hGestureList)) {
        NMCUSTOMDRAW* pnmcd = (NMCUSTOMDRAW*)lParam;
        if (pnmcd->dwDrawStage == CDDS_PREPAINT) {
          return CDRF_NOTIFYITEMDRAW;
        }
        if (pnmcd->dwDrawStage == CDDS_ITEMPREPAINT) {
          if (Theme::Get().IsDarkMode()) {
            SetTextColor(pnmcd->hdc, RGB(220, 220, 220));
            SetBkColor(pnmcd->hdc, Theme::Get().ClrSurface());
          }
          return CDRF_DODEFAULT;
        }
      }
    }
    if (pnm->hwndFrom == s_hShortcutList && pnm->code == LVN_ITEMCHANGED) {
      NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
      if ((pnmv->uNewState & LVIS_SELECTED) && pnmv->iItem >= 0 && pnmv->iItem < (int)s_tempShortcuts.size()) {
          const auto& sc = s_tempShortcuts[pnmv->iItem];
          // Find action index in combo by matching text
          int aCount = (int)SendMessageW(s_hScActionCombo, CB_GETCOUNT, 0, 0);
          for (int a = 0; a < aCount; ++a) {
              wchar_t buf[256] = { 0 };
              SendMessageW(s_hScActionCombo, CB_GETLBTEXT, a, (LPARAM)buf);
              if (sc.name == buf) {
                  SendMessageW(s_hScActionCombo, CB_SETCURSEL, (WPARAM)a, 0);
                  break;
              }
          }
          // Find key index in combo
          for (size_t k = 0; k < sizeof(s_keyChoices)/sizeof(s_keyChoices[0]); ++k) {
              if (sc.key == s_keyChoices[k]) {
                  SendMessageW(s_hScKeyCombo, CB_SETCURSEL, (WPARAM)k, 0);
                  break;
              }
          }
          SendMessageW(s_hScShift, BM_SETCHECK, sc.shift ? BST_CHECKED : BST_UNCHECKED, 0);
          SendMessageW(s_hScCtrl, BM_SETCHECK, sc.ctrl ? BST_CHECKED : BST_UNCHECKED, 0);
          SendMessageW(s_hScAlt, BM_SETCHECK, sc.alt ? BST_CHECKED : BST_UNCHECKED, 0);
      }
    }
    else if (pnm->hwndFrom == s_hGestureList && pnm->code == LVN_ITEMCHANGED) {
      NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
      if ((pnmv->uNewState & LVIS_SELECTED) && pnmv->iItem >= 0 && pnmv->iItem < (int)s_tempGestures.size()) {
          const auto& g = s_tempGestures[pnmv->iItem];
          for (size_t p = 0; p < sizeof(s_allPatterns)/sizeof(s_allPatterns[0]); ++p) {
              if (g.code == s_allPatterns[p].code) {
                  SendMessageW(s_hGestPatternCombo, CB_SETCURSEL, (WPARAM)p, 0);
                  break;
              }
          }
          int aCount = (int)SendMessageW(s_hGestActionCombo, CB_GETCOUNT, 0, 0);
          for (int a = 0; a < aCount; ++a) {
              int actId = (int)SendMessageW(s_hGestActionCombo, CB_GETITEMDATA, a, 0);
              if (g.action == actId) {
                  SendMessageW(s_hGestActionCombo, CB_SETCURSEL, (WPARAM)a, 0);
                  break;
              }
          }
      }
    }
    break;
  }
  case WM_COMMAND: {
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    if (id == 201 && code == LBN_SELCHANGE) {
      int sel = (int)SendMessageW(s_hCategoryList, LB_GETCURSEL, 0, 0);
      if (sel >= 0 && sel < 10) {
        SwitchTab(hwnd, sel);
      }
      return 0;
    }

    if (id == 401 && code == CBN_SELCHANGE) {
      SaveActionCombosForCurrentTarget();
      s_curActionTarget = (int)SendMessageW(s_hActionTargetCombo, CB_GETCURSEL, 0, 0);
      UpdateActionCombosForCurrentTarget();
      return 0;
    }

    // --- Page 7: MDIE Shortcuts Buttons ---
    if (id == 356) { // Add Shortcut
      int aIdx = (int)SendMessageW(s_hScActionCombo, CB_GETCURSEL, 0, 0);
      int kIdx = (int)SendMessageW(s_hScKeyCombo, CB_GETCURSEL, 0, 0);
      int aCount = (int)SendMessageW(s_hScActionCombo, CB_GETCOUNT, 0, 0);
      if (aIdx >= 0 && aIdx < aCount &&
          kIdx >= 0 && kIdx < (int)(sizeof(s_keyChoices)/sizeof(s_keyChoices[0]))) {
          ShortcutItem item;
          wchar_t actBuf[256] = { 0 };
          SendMessageW(s_hScActionCombo, CB_GETLBTEXT, aIdx, (LPARAM)actBuf);
          item.name = actBuf;
          item.key = s_keyChoices[kIdx];
          item.shift = (SendMessageW(s_hScShift, BM_GETCHECK, 0, 0) == BST_CHECKED);
          item.ctrl = (SendMessageW(s_hScCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
          item.alt = (SendMessageW(s_hScAlt, BM_GETCHECK, 0, 0) == BST_CHECKED);
          item.keyText = FormatShortcutKey(item);
          s_tempShortcuts.push_back(item);
          PopulateShortcutList(s_hShortcutList);
          ListView_SetItemState(s_hShortcutList, (int)s_tempShortcuts.size() - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
      }
      return 0;
    } else if (id == 357) { // Modify Shortcut
      int sel = ListView_GetNextItem(s_hShortcutList, -1, LVNI_SELECTED);
      int aIdx = (int)SendMessageW(s_hScActionCombo, CB_GETCURSEL, 0, 0);
      int kIdx = (int)SendMessageW(s_hScKeyCombo, CB_GETCURSEL, 0, 0);
      int aCount = (int)SendMessageW(s_hScActionCombo, CB_GETCOUNT, 0, 0);
      if (aIdx >= 0 && aIdx < aCount &&
          kIdx >= 0 && kIdx < (int)(sizeof(s_keyChoices)/sizeof(s_keyChoices[0]))) {
          wchar_t actBuf[256] = { 0 };
          SendMessageW(s_hScActionCombo, CB_GETLBTEXT, aIdx, (LPARAM)actBuf);
          if (sel < 0 || sel >= (int)s_tempShortcuts.size()) {
              for (size_t i = 0; i < s_tempShortcuts.size(); ++i) {
                  if (s_tempShortcuts[i].name == actBuf) {
                      sel = (int)i;
                      break;
                  }
              }
          }
          if (sel >= 0 && sel < (int)s_tempShortcuts.size()) {
              s_tempShortcuts[sel].name = actBuf;
              s_tempShortcuts[sel].key = s_keyChoices[kIdx];
              s_tempShortcuts[sel].shift = (SendMessageW(s_hScShift, BM_GETCHECK, 0, 0) == BST_CHECKED);
              s_tempShortcuts[sel].ctrl = (SendMessageW(s_hScCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
              s_tempShortcuts[sel].alt = (SendMessageW(s_hScAlt, BM_GETCHECK, 0, 0) == BST_CHECKED);
              s_tempShortcuts[sel].keyText = FormatShortcutKey(s_tempShortcuts[sel]);
              PopulateShortcutList(s_hShortcutList);
              ListView_SetItemState(s_hShortcutList, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
          } else {
              ShortcutItem item;
              item.name = actBuf;
              item.key = s_keyChoices[kIdx];
              item.shift = (SendMessageW(s_hScShift, BM_GETCHECK, 0, 0) == BST_CHECKED);
              item.ctrl = (SendMessageW(s_hScCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
              item.alt = (SendMessageW(s_hScAlt, BM_GETCHECK, 0, 0) == BST_CHECKED);
              item.keyText = FormatShortcutKey(item);
              s_tempShortcuts.push_back(item);
              PopulateShortcutList(s_hShortcutList);
              ListView_SetItemState(s_hShortcutList, (int)s_tempShortcuts.size() - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
          }
      }
      return 0;
    } else if (id == 358) { // Delete Shortcut
      int sel = ListView_GetNextItem(s_hShortcutList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempShortcuts.size()) {
          s_tempShortcuts.erase(s_tempShortcuts.begin() + sel);
          PopulateShortcutList(s_hShortcutList);
      }
      return 0;
    } else if (id == 359) { // Reset Shortcuts
      ResetDefaultShortcuts();
      PopulateShortcutList(s_hShortcutList);
      return 0;
    }

    // --- Page 8: MDIE Gestures Buttons ---
    if (id == 363) { // Add gesture
      int pIdx = (int)SendMessageW(s_hGestPatternCombo, CB_GETCURSEL, 0, 0);
      int aIdx = (int)SendMessageW(s_hGestActionCombo, CB_GETCURSEL, 0, 0);
      int aCount = (int)SendMessageW(s_hGestActionCombo, CB_GETCOUNT, 0, 0);
      if (pIdx >= 0 && pIdx < (int)(sizeof(s_allPatterns)/sizeof(s_allPatterns[0])) &&
          aIdx >= 0 && aIdx < aCount) {
          GestureEntry ge;
          ge.code = s_allPatterns[pIdx].code;
          ge.action = (int)SendMessageW(s_hGestActionCombo, CB_GETITEMDATA, aIdx, 0);
          wchar_t actBuf[256] = { 0 };
          SendMessageW(s_hGestActionCombo, CB_GETLBTEXT, aIdx, (LPARAM)actBuf);
          ge.name = actBuf;
          s_tempGestures.push_back(ge);
          PopulateGestureList(s_hGestureList);
          ListView_SetItemState(s_hGestureList, (int)s_tempGestures.size() - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
      }
      return 0;
    } else if (id == 364) { // Modify gesture
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      int pIdx = (int)SendMessageW(s_hGestPatternCombo, CB_GETCURSEL, 0, 0);
      int aIdx = (int)SendMessageW(s_hGestActionCombo, CB_GETCURSEL, 0, 0);
      int aCount = (int)SendMessageW(s_hGestActionCombo, CB_GETCOUNT, 0, 0);
      if (sel >= 0 && sel < (int)s_tempGestures.size() &&
          pIdx >= 0 && pIdx < (int)(sizeof(s_allPatterns)/sizeof(s_allPatterns[0])) &&
          aIdx >= 0 && aIdx < aCount) {
          s_tempGestures[sel].code = s_allPatterns[pIdx].code;
          s_tempGestures[sel].action = (int)SendMessageW(s_hGestActionCombo, CB_GETITEMDATA, aIdx, 0);
          wchar_t actBuf[256] = { 0 };
          SendMessageW(s_hGestActionCombo, CB_GETLBTEXT, aIdx, (LPARAM)actBuf);
          s_tempGestures[sel].name = actBuf;
          PopulateGestureList(s_hGestureList);
          ListView_SetItemState(s_hGestureList, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
      }
      return 0;
    } else if (id == 365) { // Delete gesture
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempGestures.size()) {
        s_tempGestures.erase(s_tempGestures.begin() + sel);
        PopulateGestureList(s_hGestureList);
      }
      return 0;
    } else if (id == 366) { // Reset gestures to default
      s_tempGestures = {
          { "L", (int)GestureAction::NavBack, L"后退" },
          { "R", (int)GestureAction::NavForward, L"前进" },
          { "U", (int)GestureAction::NavUp, L"向上一级" },
          { "D", (int)GestureAction::Refresh, L"刷新" },
          { "UD", (int)GestureAction::NewTab, L"新建标签页" },
          { "DR", (int)GestureAction::CloseTab, L"关闭当前标签页" },
          { "UL", (int)GestureAction::ToggleTree, L"显示/隐藏侧边栏目录树" },
          { "UR", (int)GestureAction::TogglePreview, L"显示/隐藏快速预览窗格" },
          { "RU", (int)GestureAction::ScrollTop, L"滚动到列表顶部" },
          { "RD", (int)GestureAction::ScrollBottom, L"滚动到列表底部" },
          { "LU", (int)GestureAction::PageUp, L"向上翻页" },
          { "LD", (int)GestureAction::PageDown, L"向下翻页" }
      };
      PopulateGestureList(s_hGestureList);
      return 0;
    }

    if (id == IDOK) {
      SaveSettings(hwnd);
      DestroyWindow(hwnd);
      return 0;
    } else if (id == IDCANCEL) {
      DestroyWindow(hwnd);
      return 0;
    }
    break;
  }
  case WM_CLOSE: {
    DestroyWindow(hwnd);
    return 0;
  }
  default:
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  return 0;
}
