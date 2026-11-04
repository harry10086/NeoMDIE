# -*- coding: utf-8 -*-

cpp_code = r'''#include "SettingsDialog.h"
#include "Theme.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\GestureEngine.h"
#include <commctrl.h>
#include <windowsx.h>
#include <vector>
#include <string>

static const wchar_t *SETTINGS_CLASS = L"NeoMDIE_SettingsDialog";
static std::function<void()> s_onSettingsChanged;
static int s_currentCategory = 0;

static HWND s_hCategoryList = nullptr;
static std::vector<HWND> s_pageControls[10];

// Page 0: General
static HWND s_hLangCombo = nullptr;
static HWND s_hChkRestoreTabs = nullptr;
static HWND s_hChkMinToTray = nullptr;
static HWND s_hChkCloseToTray = nullptr;
static HWND s_hChkReplaceExplorer = nullptr;

// Page 1: Window Actions
static HWND s_hTreeActionCombo = nullptr;
static HWND s_hLinksActionCombo = nullptr;
static HWND s_hAddressActionCombo = nullptr;

// Page 2: Window & Panes
static HWND s_hLayoutCombo = nullptr;
static HWND s_hChkSyncNav = nullptr;
static HWND s_hChkPaneHeader = nullptr;
static HWND s_hChkLockToolbars = nullptr;

// Page 3: Tabs
static HWND s_hChkAllowDupTabs = nullptr;
static HWND s_hTabDblClickCombo = nullptr;
static HWND s_hChkShowNewTabBtn = nullptr;
static HWND s_hChkShowCloseBtn = nullptr;

// Page 4: Tree & Sidebar
static HWND s_hChkTree = nullptr;
static HWND s_hChkAutoExpandTree = nullptr;
static HWND s_hChkPreview = nullptr;

// Page 5: View & File List
static HWND s_hViewModeCombo = nullptr;
static HWND s_hChkHidden = nullptr;
static HWND s_hChkCalcSize = nullptr;
static HWND s_hChkFullRowSelect = nullptr;
static HWND s_hChkZebraStriping = nullptr;

// Page 6: Search & Filter
static HWND s_hChkSearchSubfolders = nullptr;
static HWND s_hChkMatchCase = nullptr;
static HWND s_hChkIncremental = nullptr;

// Page 7: Hotkeys (Shortcuts cheat-sheet)
static HWND s_hShortcutList = nullptr;

// Page 8: Mouse Gestures
static HWND s_hChkGestures = nullptr;
static HWND s_hGestureList = nullptr;
static HWND s_hBtnGestureAdd = nullptr;
static HWND s_hBtnGestureEdit = nullptr;
static HWND s_hBtnGestureDel = nullptr;
static HWND s_hBtnGestureReset = nullptr;
static std::vector<GestureEntry> s_tempGestures;

// Page 9: Appearance & Toolbars
static HWND s_hThemeCombo = nullptr;
static HWND s_hChkTBStandard = nullptr;
static HWND s_hChkTBAddress = nullptr;
static HWND s_hChkTBSearch = nullptr;
static HWND s_hChkTBLinks = nullptr;
static HWND s_hChkStatusBar = nullptr;

static void PopulateGestureList(HWND hList) {
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < s_tempGestures.size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        std::wstring pattern = GestureEngine::CodeToArrowString(s_tempGestures[i].code) + L"  (" + Util::Utf8ToWide(s_tempGestures[i].code) + L")";
        lvi.pszText = (LPWSTR)pattern.c_str();
        ListView_InsertItem(hList, &lvi);

        std::wstring actName = !s_tempGestures[i].name.empty() ? s_tempGestures[i].name : GestureEngine::GetActionName((GestureAction)s_tempGestures[i].action);
        ListView_SetItemText(hList, (int)i, 1, (LPWSTR)actName.c_str());
    }
}

static void PopulateShortcutList(HWND hList) {
    ListView_DeleteAllItems(hList);
    bool zh = I18n::Get().IsChinese();

    struct SCItem { const wchar_t* key; const wchar_t* desc; const wchar_t* cat; };
    static const SCItem s_shortcuts[] = {
        // Tabs & Navigation
        { L"Ctrl + T", zh ? L"新建标签页" : L"New Tab", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { L"Ctrl + W", zh ? L"关闭当前标签页" : L"Close Tab", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { L"Ctrl + Shift + T", zh ? L"恢复关闭的标签页" : L"Reopen Closed Tab", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { L"Ctrl + Tab", zh ? L"切换到下一个标签" : L"Next Tab", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { L"Ctrl + Shift + Tab", zh ? L"切换到上一个标签" : L"Previous Tab", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { L"Alt + Left / Right", zh ? L"后退 / 前进" : L"Back / Forward", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { L"Alt + Up / Backspace", zh ? L"返回上一级目录" : L"Go Up", zh ? L"标签与导航" : L"Tabs & Navigation" },
        { zh ? L"空白区双击左键" : L"Double Click Empty Space", zh ? L"返回上一级目录 (MDIE经典)" : L"Go Up to Parent Directory", zh ? L"标签与导航" : L"Tabs & Navigation" },

        // Window & Panes
        { L"Ctrl + 1", zh ? L"单栏布局" : L"Single Pane", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Ctrl + 2", zh ? L"左右双栏布局" : L"Dual Vertical Panes", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Ctrl + 3", zh ? L"上下双栏布局" : L"Dual Horizontal Panes", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Ctrl + 4", zh ? L"田字四栏布局" : L"Quad Panes (2x2)", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Ctrl + 5", zh ? L"1左2右三栏布局" : L"Three Panes (1+2)", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Ctrl + E", zh ? L"快速多栏切换" : L"Toggle Multi-Pane", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"F8", zh ? L"切换显示左侧目录树" : L"Toggle Directory Tree", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Alt + P", zh ? L"切换显示右侧预览窗格" : L"Toggle Preview Pane", zh ? L"多栏与窗口" : L"Panes & Window" },
        { L"Ctrl + ~", zh ? L"打开命令提示符终端" : L"Open CMD Terminal", zh ? L"多栏与窗口" : L"Panes & Window" },

        // Explorer & Files
        { L"F2", zh ? L"重命名选中文件/文件夹" : L"Rename Selected Item", zh ? L"文件操作" : L"File Operations" },
        { L"F5 / Ctrl + R", zh ? L"刷新当前目录" : L"Refresh Directory", zh ? L"文件操作" : L"File Operations" },
        { L"Delete", zh ? L"删除到回收站" : L"Delete to Recycle Bin", zh ? L"文件操作" : L"File Operations" },
        { L"Shift + Delete", zh ? L"永久删除" : L"Permanently Delete", zh ? L"文件操作" : L"File Operations" },
        { L"Ctrl + A", zh ? L"全选" : L"Select All", zh ? L"文件操作" : L"File Operations" },
        { L"Ctrl + C / X / V", zh ? L"复制 / 剪切 / 粘贴" : L"Copy / Cut / Paste", zh ? L"文件操作" : L"File Operations" },
        { L"Ctrl + Shift + C", zh ? L"复制完整路径 (Win11特性)" : L"Copy Full Path (Win11)", zh ? L"文件操作" : L"File Operations" },
        { L"Alt + Enter", zh ? L"查看项目属性" : L"Properties", zh ? L"文件操作" : L"File Operations" },
        { L"Ctrl + Shift + N", zh ? L"新建文件夹" : L"New Folder", zh ? L"文件操作" : L"File Operations" },
        { L"Alt + D / Ctrl + L / F6", zh ? L"聚焦到地址栏" : L"Focus Address Bar", zh ? L"文件操作" : L"File Operations" },
        { L"Ctrl + F / F3", zh ? L"聚焦到搜索过滤栏" : L"Focus Search/Filter Bar", zh ? L"文件操作" : L"File Operations" },
        { L"Home / End", zh ? L"滚动到列表最顶 / 最底" : L"Scroll to Top / Bottom", zh ? L"文件操作" : L"File Operations" }
    };

    for (int i = 0; i < (int)(sizeof(s_shortcuts)/sizeof(s_shortcuts[0])); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = (LPWSTR)s_shortcuts[i].key;
        ListView_InsertItem(hList, &lvi);
        ListView_SetItemText(hList, i, 1, (LPWSTR)s_shortcuts[i].desc);
        ListView_SetItemText(hList, i, 2, (LPWSTR)s_shortcuts[i].cat);
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

  int w = Theme::Get().Scale(760, hwndParent);
  int h = Theme::Get().Scale(590, hwndParent);

  RECT rcParent;
  GetWindowRect(hwndParent, &rcParent);
  int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
  int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

  HWND hwndDlg =
      CreateWindowExW(WS_EX_DLGMODALFRAME, SETTINGS_CLASS,
                      I18n::Get().IsChinese() ? L"NeoMDIE 选项与设置" : L"NeoMDIE Options & Settings",
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

void SettingsDialog::CreateControls(HWND hwnd) {
  HINSTANCE hInst = GetModuleHandleW(nullptr);
  HFONT hFont = Theme::Get().GetUIFont(hwnd);
  HFONT hFontBold = Theme::Get().GetBoldUIFont(hwnd);

  for (int i = 0; i < 10; ++i) {
    s_pageControls[i].clear();
  }

  bool zh = I18n::Get().IsChinese();
  Config &cfg = Config::Get();

  RECT rcClient;
  GetClientRect(hwnd, &rcClient);
  int clientW = rcClient.right - rcClient.left;
  int clientH = rcClient.bottom - rcClient.top;

  int navW = Theme::Get().Scale(160, hwnd);
  int navH = clientH - Theme::Get().Scale(60, hwnd);
  int marginX = Theme::Get().Scale(185, hwnd);
  int contentW = clientW - marginX - Theme::Get().Scale(24, hwnd);
  int lineH = Theme::Get().Scale(24, hwnd);
  int spacing = Theme::Get().Scale(6, hwnd);

  // 1. Left Vertical Navigation (Clean list without broken emojis)
  s_hCategoryList = CreateWindowExW(
      0, L"LISTBOX", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER,
      Theme::Get().Scale(14, hwnd), Theme::Get().Scale(14, hwnd),
      navW, navH,
      hwnd, (HMENU)201, hInst, nullptr);
  SendMessageW(s_hCategoryList, WM_SETFONT, (WPARAM)hFontBold, TRUE);
  SendMessageW(s_hCategoryList, LB_SETITEMHEIGHT, 0, Theme::Get().Scale(32, hwnd));

  const wchar_t* navTitlesZh[] = {
      L"常规设置",
      L"窗口动作",
      L"窗口与多栏",
      L"标签页",
      L"侧边栏与树",
      L"视图与列表",
      L"搜索与过滤",
      L"快捷键设置",
      L"鼠标手势",
      L"外观与工具栏"
  };
  const wchar_t* navTitlesEn[] = {
      L"General",
      L"Window Actions",
      L"Window & Panes",
      L"Tabs",
      L"Sidebar & Tree",
      L"View & List",
      L"Search & Filter",
      L"Hotkeys",
      L"Mouse Gestures",
      L"Appearance & TB"
  };

  for (int i = 0; i < 10; ++i) {
      SendMessageW(s_hCategoryList, LB_ADDSTRING, 0, (LPARAM)(zh ? navTitlesZh[i] : navTitlesEn[i]));
  }
  SendMessageW(s_hCategoryList, LB_SETCURSEL, s_currentCategory, 0);

  // Helpers
  auto addCtrl = [&](int page, HWND hCtrl) {
    s_pageControls[page].push_back(hCtrl);
    return hCtrl;
  };

  auto createLabel = [&](int page, const wchar_t *text, int y, bool bold = false) {
    HWND hLbl = CreateWindowExW(0, L"STATIC", text,
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                marginX, y, contentW, lineH,
                                hwnd, nullptr, hInst, nullptr);
    SendMessageW(hLbl, WM_SETFONT, (WPARAM)(bold ? hFontBold : hFont), TRUE);
    return addCtrl(page, hLbl);
  };

  auto createNote = [&](int page, const wchar_t *text, int y) {
    HWND hLbl = CreateWindowExW(0, L"STATIC", text,
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                marginX + Theme::Get().Scale(24, hwnd), y, contentW - Theme::Get().Scale(24, hwnd), lineH + Theme::Get().Scale(4, hwnd),
                                hwnd, nullptr, hInst, nullptr);
    SendMessageW(hLbl, WM_SETFONT, (WPARAM)hFont, TRUE);
    return addCtrl(page, hLbl);
  };

  auto createChk = [&](int page, const wchar_t *text, int y, int id, bool checked) {
    HWND hChk = CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP | WS_CLIPSIBLINGS,
        marginX, y, contentW, lineH, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hChk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return addCtrl(page, hChk);
  };

  auto createCombo = [&](int page, int y, int id, int w = 240) {
    HWND hCmb = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS,
        marginX, y, Theme::Get().Scale(w, hwnd), Theme::Get().Scale(200, hwnd),
        hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hCmb, WM_SETFONT, (WPARAM)hFont, TRUE);
    return addCtrl(page, hCmb);
  };

  // ----------------------------------------------------
  // Page 0: 常规设置 (General)
  // ----------------------------------------------------
  int y0 = Theme::Get().Scale(16, hwnd);
  createLabel(0, zh ? L"界面显示语言 (Language):" : L"Interface Language:", y0, true);
  y0 += lineH + spacing;
  s_hLangCombo = createCombo(0, y0, 301, 220);
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"简体中文 (Simplified Chinese)");
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"English (United States)");
  SendMessageW(s_hLangCombo, CB_SETCURSEL, (cfg.language == (int)Language::English) ? 1 : 0, 0);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"启动与系统托盘行为:" : L"Startup & Tray Behavior:", y0, true);
  y0 += lineH + spacing;

  s_hChkRestoreTabs = createChk(0, zh ? L"启动时恢复上次退出时未关闭的标签页" : L"Restore opened tabs on startup",
                                y0, 302, cfg.restoreTabsOnStartup);
  y0 += lineH + spacing;

  s_hChkMinToTray = createChk(0, zh ? L"最小化窗口时缩减至系统托盘通知区" : L"Minimize to system tray",
                              y0, 303, cfg.minimizeToTray);
  y0 += lineH + spacing;

  s_hChkCloseToTray = createChk(0, zh ? L"点击窗口关闭按钮 [✕] 时最小化到托盘而不退出" : L"Close button [X] minimizes to tray",
                                y0, 304, cfg.closeToTray);
  y0 += lineH + spacing;

  s_hChkReplaceExplorer = createChk(0, zh ? L"替代 Windows 系统原生资源管理器 (接管 Win+E 与文件夹打开)" : L"Replace default Windows Explorer (Win+E)",
                                    y0, 305, false);

  // ----------------------------------------------------
  // Page 1: 窗口动作 (Window Actions)
  // ----------------------------------------------------
  int yAction = Theme::Get().Scale(16, hwnd);
  createLabel(1, zh ? L"侧边栏目录树项目点击动作:" : L"Sidebar Directory Tree Item Click Action:", yAction, true);
  yAction += lineH;
  createNote(1, zh ? L"设置在左侧树状视图中单击或双击任意文件夹时的打开方式:" : L"Set how folders in sidebar tree should be opened:", yAction);
  yAction += lineH + spacing;
  s_hTreeActionCombo = createCombo(1, yAction, 306, 260);
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前栏打开 (Current Pane)" : L"Open in Current Pane"));
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他栏打开 (Other Pane)" : L"Open in Other Pane"));
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在新标签页中打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hTreeActionCombo, CB_SETCURSEL, std::clamp(cfg.actionTreeOpen, 0, 2), 0);
  yAction += lineH + spacing * 2;

  createLabel(1, zh ? L"链接栏书签项目点击动作:" : L"Bookmarks & Links Bar Click Action:", yAction, true);
  yAction += lineH;
  createNote(1, zh ? L"设置在顶部常用链接栏中点击书签或自定义目录时的打开方式:" : L"Set how bookmark buttons in links bar should be opened:", yAction);
  yAction += lineH + spacing;
  s_hLinksActionCombo = createCombo(1, yAction, 307, 260);
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前栏打开 (Current Pane)" : L"Open in Current Pane"));
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他栏打开 (Other Pane)" : L"Open in Other Pane"));
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在新标签页中打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hLinksActionCombo, CB_SETCURSEL, std::clamp(cfg.actionLinksOpen, 0, 2), 0);
  yAction += lineH + spacing * 2;

  createLabel(1, zh ? L"地址栏回车/转到动作:" : L"Address Bar Enter / Go Action:", yAction, true);
  yAction += lineH;
  createNote(1, zh ? L"设置在面包屑地址栏输入新路径并按下回车键时的打开方式:" : L"Set how pressing Enter in address bar should be opened:", yAction);
  yAction += lineH + spacing;
  s_hAddressActionCombo = createCombo(1, yAction, 308, 260);
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前栏打开 (Current Pane)" : L"Open in Current Pane"));
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他栏打开 (Other Pane)" : L"Open in Other Pane"));
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在新标签页中打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hAddressActionCombo, CB_SETCURSEL, std::clamp(cfg.actionAddressOpen, 0, 2), 0);

  // ----------------------------------------------------
  // Page 2: 窗口与多栏 (Window & Multi-Pane)
  // ----------------------------------------------------
  int y1 = Theme::Get().Scale(16, hwnd);
  createLabel(2, zh ? L"默认分栏多栏布局模式:" : L"Default Multi-Pane Mode:", y1, true);
  y1 += lineH + spacing;

  s_hLayoutCombo = createCombo(2, y1, 310, 240);
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"单栏 (1x1)" : L"Single (1x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"左右双栏 (1x2 垂直)" : L"Dual Vertical (1x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"上下双栏 (2x1 水平)" : L"Dual Horizontal (2x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"四栏 (2x2 田字格)" : L"Quad (2x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"三栏 (1左2右)" : L"Three Panes (1+2)"));
  SendMessageW(s_hLayoutCombo, CB_SETCURSEL, std::clamp(cfg.paneLayout, 0, 4), 0);
  y1 += lineH + spacing * 2;

  createLabel(2, zh ? L"多栏行为与窗口锁定:" : L"Pane Behavior & Layout Options:", y1, true);
  y1 += lineH + spacing;

  s_hChkPaneHeader = createChk(2, zh ? L"在每个栏顶部显示独立路径及 [最小化] [最大化] [关闭] 按钮" : L"Show pane header with path & window control buttons",
                               y1, 311, true);
  y1 += lineH + spacing;

  s_hChkSyncNav = createChk(2, zh ? L"启用多栏同步浏览 (各栏同步跟随导航至相同目录)" : L"Enable synchronized multi-pane navigation",
                            y1, 312, cfg.syncNav);
  y1 += lineH + spacing;

  s_hChkLockToolbars = createChk(2, zh ? L"锁定工具栏与分栏位置 (禁止拖拽改变尺寸)" : L"Lock toolbars and pane splitters",
                                 y1, 313, cfg.lockToolbars);

  // ----------------------------------------------------
  // Page 3: 标签页 (Tabs)
  // ----------------------------------------------------
  int y2 = Theme::Get().Scale(16, hwnd);
  createLabel(3, zh ? L"多标签页行为偏好:" : L"Tab Behavior & Controls:", y2, true);
  y2 += lineH + spacing;

  s_hChkAllowDupTabs = createChk(3, zh ? L"允许打开重复路径的标签页" : L"Allow opening duplicate tabs with same path",
                                 y2, 320, cfg.allowDuplicateTabs);
  y2 += lineH + spacing;

  s_hChkShowNewTabBtn = createChk(3, zh ? L"在标签栏右侧显示新建标签按钮 [+]" : L"Show new tab button [+] on tab bar",
                                  y2, 321, true);
  y2 += lineH + spacing;

  s_hChkShowCloseBtn = createChk(3, zh ? L"在每个标签页上显示关闭按钮 [✕]" : L"Show close button [x] on each tab",
                                 y2, 322, true);
  y2 += lineH + spacing * 2;

  createLabel(3, zh ? L"双击标签页时的触发动作:" : L"Action on double clicking a tab:", y2, true);
  y2 += lineH + spacing;

  s_hTabDblClickCombo = createCombo(3, y2, 323, 240);
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"关闭该标签页" : L"Close Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"锁定/解锁标签页" : L"Lock/Unlock Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"刷新当前标签页" : L"Refresh Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"向上一级目录" : L"Go to Parent Directory"));
  SendMessageW(s_hTabDblClickCombo, CB_SETCURSEL, std::clamp(cfg.tabDoubleClickAction, 0, 3), 0);

  // ----------------------------------------------------
  // Page 4: 侧边栏与树 (Sidebar & Tree)
  // ----------------------------------------------------
  int y3 = Theme::Get().Scale(16, hwnd);
  createLabel(4, zh ? L"左侧文件夹目录树与预览窗格:" : L"Sidebar & Quick Preview Options:", y3, true);
  y3 += lineH + spacing;

  s_hChkTree = createChk(4, zh ? L"启动时默认显示左侧文件夹目录树 (快捷键 F8)" : L"Show directory tree by default on launch (F8)",
                         y3, 330, cfg.showTree);
  y3 += lineH + spacing;

  s_hChkAutoExpandTree = createChk(4, zh ? L"切换标签或目录时，目录树自动联动展开并高亮定位" : L"Auto-expand and synchronize tree with active directory",
                                   y3, 331, cfg.autoExpandTree);
  y3 += lineH + spacing;

  s_hChkPreview = createChk(4, zh ? L"启动时默认开启右侧快速预览窗格 (快捷键 Alt+P)" : L"Show quick preview pane by default on launch (Alt+P)",
                            y3, 332, cfg.previewEnabled);

  // ----------------------------------------------------
  // Page 5: 视图与列表 (View & List)
  // ----------------------------------------------------
  int y4 = Theme::Get().Scale(16, hwnd);
  createLabel(5, zh ? L"文件列表展示与信息计算:" : L"View Mode & Details Options:", y4, true);
  y4 += lineH + spacing;

  s_hViewModeCombo = createCombo(5, y4, 340, 240);
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"详细信息 (Details)" : L"Details"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"大图标 (Large Icons)" : L"Large Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"小图标 (Small Icons)" : L"Small Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"列表 (List)" : L"List"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"平铺 (Tiles)" : L"Tiles"));
  SendMessageW(s_hViewModeCombo, CB_SETCURSEL, std::clamp(cfg.viewMode, 0, 4), 0);
  y4 += lineH + spacing * 2;

  s_hChkHidden = createChk(5, zh ? L"显示隐藏的文件和文件夹" : L"Show hidden files and folders",
                           y4, 341, cfg.showHidden);
  y4 += lineH + spacing;

  s_hChkCalcSize = createChk(5, zh ? L"自动计算文件夹占用大小 (后台异步计算)" : L"Calculate folder sizes automatically in background",
                             y4, 342, cfg.calcFolderSize);
  y4 += lineH + spacing;

  s_hChkFullRowSelect = createChk(5, zh ? L"详细信息视图启用整行选择 (Full Row Select)" : L"Full row select in details view",
                                  y4, 343, true);

  // ----------------------------------------------------
  // Page 6: 搜索与过滤 (Search & Filter)
  // ----------------------------------------------------
  int y5 = Theme::Get().Scale(16, hwnd);
  createLabel(6, zh ? L"即时搜索与过滤选项:" : L"Search & Filter Options:", y5, true);
  y5 += lineH + spacing;

  s_hChkSearchSubfolders = createChk(6, zh ? L"搜索过滤时默认递归包含子文件夹" : L"Search subfolders recursively by default",
                                     y5, 350, cfg.searchSubfoldersDefault);
  y5 += lineH + spacing;

  s_hChkMatchCase = createChk(6, zh ? L"默认开启区分大小写匹配" : L"Match case by default",
                              y5, 351, false);
  y5 += lineH + spacing;

  s_hChkIncremental = createChk(6, zh ? L"即时增量筛选 (随键盘输入实时过滤并高亮)" : L"Instant incremental filtering on typing",
                                y5, 352, true);

  // ----------------------------------------------------
  // Page 7: 快捷键设置 (Hotkeys & Shortcuts cheat-sheet)
  // ----------------------------------------------------
  int ySc = Theme::Get().Scale(16, hwnd);
  createLabel(7, zh ? L"常用快捷键速查表 (完全继承 Windows 原生资源管理器习惯):" : L"Default Keyboard Shortcuts (Inherits Windows Explorer):", ySc, true);
  ySc += lineH + spacing;

  s_hShortcutList = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
      marginX, ySc, contentW, clientH - ySc - Theme::Get().Scale(60, hwnd),
      hwnd, (HMENU)355, hInst, nullptr);
  ListView_SetExtendedListViewStyle(s_hShortcutList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  SendMessageW(s_hShortcutList, WM_SETFONT, (WPARAM)hFont, TRUE);

  LVCOLUMNW lvcSc = { 0 };
  lvcSc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
  lvcSc.fmt = LVCFMT_LEFT;

  lvcSc.cx = Theme::Get().Scale(180, hwnd);
  lvcSc.pszText = (LPWSTR)(zh ? L"快捷键 (Key)" : L"Shortcut");
  ListView_InsertColumn(s_hShortcutList, 0, &lvcSc);

  lvcSc.cx = Theme::Get().Scale(220, hwnd);
  lvcSc.pszText = (LPWSTR)(zh ? L"功能说明 (Description)" : L"Description");
  ListView_InsertColumn(s_hShortcutList, 1, &lvcSc);

  lvcSc.cx = Theme::Get().Scale(120, hwnd);
  lvcSc.pszText = (LPWSTR)(zh ? L"分类 (Category)" : L"Category");
  ListView_InsertColumn(s_hShortcutList, 2, &lvcSc);

  PopulateShortcutList(s_hShortcutList);
  addCtrl(7, s_hShortcutList);

  // ----------------------------------------------------
  // Page 8: 鼠标手势 (Mouse Gestures)
  // ----------------------------------------------------
  int y6 = Theme::Get().Scale(16, hwnd);
  s_hChkGestures = createChk(8, zh ? L"启用鼠标手势功能 (按住鼠标右键拖动触发)" : L"Enable mouse gestures (Hold Right-Click and drag)",
                             y6, 360, cfg.gesturesEnabled);
  y6 += lineH + spacing;

  s_tempGestures = cfg.gestures;
  s_hGestureList = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
      marginX, y6, contentW, Theme::Get().Scale(240, hwnd),
      hwnd, (HMENU)361, hInst, nullptr);
  ListView_SetExtendedListViewStyle(s_hGestureList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  SendMessageW(s_hGestureList, WM_SETFONT, (WPARAM)hFont, TRUE);

  LVCOLUMNW lvcG = { 0 };
  lvcG.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
  lvcG.fmt = LVCFMT_LEFT;
  lvcG.cx = Theme::Get().Scale(150, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"手势轨迹 (Pattern)" : L"Pattern");
  ListView_InsertColumn(s_hGestureList, 0, &lvcG);

  lvcG.cx = Theme::Get().Scale(350, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"对应执行动作 (Action)" : L"Action");
  ListView_InsertColumn(s_hGestureList, 1, &lvcG);

  PopulateGestureList(s_hGestureList);
  addCtrl(8, s_hGestureList);

  y6 += Theme::Get().Scale(248, hwnd);
  int btnGW = Theme::Get().Scale(80, hwnd);
  int btnGH = Theme::Get().Scale(26, hwnd);

  s_hBtnGestureAdd = CreateWindowExW(0, L"BUTTON", zh ? L"添加(&A)" : L"Add(&A)",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     marginX, y6, btnGW, btnGH, hwnd, (HMENU)363, hInst, nullptr);
  SendMessageW(s_hBtnGestureAdd, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestureAdd);

  s_hBtnGestureEdit = CreateWindowExW(0, L"BUTTON", zh ? L"修改动作(&E)" : L"Edit(&E)",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                      marginX + btnGW + Theme::Get().Scale(8, hwnd), y6, btnGW + Theme::Get().Scale(16, hwnd), btnGH, hwnd, (HMENU)364, hInst, nullptr);
  SendMessageW(s_hBtnGestureEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestureEdit);

  s_hBtnGestureDel = CreateWindowExW(0, L"BUTTON", zh ? L"删除(&D)" : L"Delete(&D)",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     marginX + (btnGW * 2) + Theme::Get().Scale(30, hwnd), y6, btnGW, btnGH, hwnd, (HMENU)365, hInst, nullptr);
  SendMessageW(s_hBtnGestureDel, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestureDel);

  s_hBtnGestureReset = CreateWindowExW(0, L"BUTTON", zh ? L"恢复默认(&R)" : L"Reset(&R)",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                       marginX + (btnGW * 3) + Theme::Get().Scale(38, hwnd), y6, btnGW + Theme::Get().Scale(16, hwnd), btnGH, hwnd, (HMENU)366, hInst, nullptr);
  SendMessageW(s_hBtnGestureReset, WM_SETFONT, (WPARAM)hFont, TRUE);
  addCtrl(8, s_hBtnGestureReset);

  // ----------------------------------------------------
  // Page 9: 外观与工具栏 (Appearance & Toolbars)
  // ----------------------------------------------------
  int y7 = Theme::Get().Scale(16, hwnd);
  createLabel(9, zh ? L"主题与深色模式 (Theme):" : L"Theme & Dark Mode:", y7, true);
  y7 += lineH + spacing;

  s_hThemeCombo = createCombo(9, y7, 370, 240);
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"跟随 Windows 系统深浅色外观" : L"Follow Windows System"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"浅色模式 (Light Theme)" : L"Light Theme"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"深色模式 (Dark Theme)" : L"Dark Theme"));
  int curTheme = (cfg.themeMode == 0) ? 1 : (cfg.themeMode == 1 ? 2 : 0);
  SendMessageW(s_hThemeCombo, CB_SETCURSEL, curTheme, 0);
  y7 += lineH + spacing * 2;

  createLabel(9, zh ? L"界面组件显隐控制 (MDIE 模块化工具栏):" : L"Toolbar Elements Visibility:", y7, true);
  y7 += lineH + spacing;

  s_hChkTBStandard = createChk(9, zh ? L"显示标准按钮工具栏 (前进/后退/上级/刷新等)" : L"Show standard navigation buttons bar",
                               y7, 372, cfg.showToolbarStandard);
  y7 += lineH + spacing;

  s_hChkTBAddress = createChk(9, zh ? L"显示面包屑地址栏 (Breadcrumb Address Bar)" : L"Show breadcrumb address bar",
                              y7, 373, cfg.showToolbarAddress);
  y7 += lineH + spacing;

  s_hChkTBSearch = createChk(9, zh ? L"显示搜索与即时过滤栏 (Instant Search Bar)" : L"Show search & filter bar",
                             y7, 374, cfg.showToolbarSearch);
  y7 += lineH + spacing;

  s_hChkTBLinks = createChk(9, zh ? L"显示常用书签与链接栏" : L"Show bookmarks & links bar",
                            y7, 375, cfg.showToolbarLinks);
  y7 += lineH + spacing;

  s_hChkStatusBar = createChk(9, zh ? L"显示底部状态栏 (项目总数、占用大小等信息)" : L"Show status bar with item counts & size",
                             y7, 376, cfg.showStatusBar);

  // Initial tab page display
  SwitchTab(hwnd, s_currentCategory);

  // Bottom OK & Cancel buttons (Cleanly positioned above the client bottom border)
  int btnW = Theme::Get().Scale(90, hwnd);
  int btnH = Theme::Get().Scale(30, hwnd);
  int btnY = clientH - Theme::Get().Scale(42, hwnd);

  HWND hBtnOK = CreateWindowExW(
      0, L"BUTTON", I18n::Get().T(StringId::SettingsBtnOK),
      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
      clientW - Theme::Get().Scale(210, hwnd), btnY, btnW, btnH,
      hwnd, (HMENU)IDOK, hInst, nullptr);
  SendMessageW(hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);

  HWND hBtnCancel = CreateWindowExW(
      0, L"BUTTON", I18n::Get().T(StringId::SettingsBtnCancel),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
      clientW - Theme::Get().Scale(110, hwnd), btnY, btnW, btnH,
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

  // Page 1: Window Actions
  cfg.actionTreeOpen = (int)SendMessageW(s_hTreeActionCombo, CB_GETCURSEL, 0, 0);
  cfg.actionLinksOpen = (int)SendMessageW(s_hLinksActionCombo, CB_GETCURSEL, 0, 0);
  cfg.actionAddressOpen = (int)SendMessageW(s_hAddressActionCombo, CB_GETCURSEL, 0, 0);

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

  // Page 5: View & List
  cfg.viewMode = (int)SendMessageW(s_hViewModeCombo, CB_GETCURSEL, 0, 0);
  cfg.showHidden = (SendMessageW(s_hChkHidden, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.calcFolderSize = (SendMessageW(s_hChkCalcSize, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 6: Search & Filter
  cfg.searchSubfoldersDefault = (SendMessageW(s_hChkSearchSubfolders, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 8: Gestures
  cfg.gesturesEnabled = (SendMessageW(s_hChkGestures, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.gestures = s_tempGestures;

  // Page 9: Appearance
  int themeSel = (int)SendMessageW(s_hThemeCombo, CB_GETCURSEL, 0, 0);
  cfg.themeMode = (themeSel == 1) ? 0 : (themeSel == 2 ? 1 : -1);

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
  case WM_CTLCOLORSTATIC: {
    HDC hdcStatic = (HDC)wParam;
    SetTextColor(hdcStatic, Theme::Get().ClrTextPrimary());
    SetBkColor(hdcStatic, Theme::Get().ClrBackground());
    return (INT_PTR)Theme::Get().BrushBackground();
  }
  case WM_CTLCOLOREDIT: {
    HDC hdcEdit = (HDC)wParam;
    SetTextColor(hdcEdit, Theme::Get().ClrTextPrimary());
    SetBkColor(hdcEdit, Theme::Get().ClrSurface());
    return (INT_PTR)Theme::Get().BrushSurface();
  }
  case WM_CTLCOLORLISTBOX: {
    HDC hdcList = (HDC)wParam;
    SetTextColor(hdcList, Theme::Get().ClrTextPrimary());
    SetBkColor(hdcList, Theme::Get().ClrSurface());
    return (INT_PTR)Theme::Get().BrushSurface();
  }
  case WM_CTLCOLORBTN: {
    return (INT_PTR)Theme::Get().BrushBackground();
  }
  case WM_ERASEBKGND: {
    HDC hdc = (HDC)wParam;
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::Get().BrushBackground());
    return 1;
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

    if (id == 363) { // Add gesture
      s_tempGestures.push_back({ "R", (int)GestureAction::NavForward, L"前进" });
      PopulateGestureList(s_hGestureList);
      return 0;
    } else if (id == 364) { // Edit gesture action
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempGestures.size()) {
        HMENU hAct = CreatePopupMenu();
        struct ActItem { int id; const wchar_t* lbl; };
        ActItem acts[] = {
          { (int)GestureAction::NavBack, L"后退 (Back)" },
          { (int)GestureAction::NavForward, L"前进 (Forward)" },
          { (int)GestureAction::NavUp, L"向上一级目录 (Go Up)" },
          { (int)GestureAction::Refresh, L"刷新 (Refresh)" },
          { (int)GestureAction::CloseTab, L"关闭当前标签页 (Close Tab)" },
          { (int)GestureAction::NewTab, L"新建标签页 (New Tab)" },
          { (int)GestureAction::DuplicateTab, L"复制标签页 (Duplicate Tab)" },
          { (int)GestureAction::ToggleTree, L"切换左侧目录树显隐 (Toggle Tree)" },
          { (int)GestureAction::TogglePreview, L"切换右侧快速预览显隐 (Toggle Preview)" },
          { (int)GestureAction::OpenTerminal, L"打开命令提示符/终端 (Open Terminal/Cmd)" },
          { (int)GestureAction::MaximizePane, L"最大化/还原当前栏 (Maximize/Restore Pane)" },
          { (int)GestureAction::ClosePane, L"关闭当前栏 (Close Pane)" }
        };
        for (int i = 0; i < 12; ++i) {
          AppendMenuW(hAct, MF_STRING, 6001 + i, acts[i].lbl);
        }
        POINT pt;
        GetCursorPos(&pt);
        int selA = TrackPopupMenu(hAct, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hAct);
        if (selA >= 6001 && selA <= 6012) {
          s_tempGestures[sel].action = acts[selA - 6001].id;
          s_tempGestures[sel].name = acts[selA - 6001].lbl;
          PopulateGestureList(s_hGestureList);
        }
      }
      return 0;
    } else if (id == 365) { // Delete gesture
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempGestures.size()) {
        s_tempGestures.erase(s_tempGestures.begin() + sel);
        PopulateGestureList(s_hGestureList);
      }
      return 0;
    } else if (id == 366) { // Reset defaults
      s_tempGestures = {
        { "L", 1, L"后退" },
        { "R", 2, L"前进" },
        { "U", 3, L"向上一级" },
        { "D", 4, L"关闭标签页" },
        { "UD", 6, L"刷新" },
        { "DR", 5, L"新建标签页" },
        { "UL", 7, L"复制标签页" },
        { "UR", 8, L"切换目录树" }
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
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
'''

with open(r'src\ui\SettingsDialog.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_code)
print("SettingsDialog.cpp rewritten with clean vertical navigation and hotkeys page!")
