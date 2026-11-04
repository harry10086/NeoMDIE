code = '''#include "SettingsDialog.h"
#include "..\\core\\Config.h"
#include "..\\core\\I18n.h"
#include "..\\core\\GestureEngine.h"
#include "..\\core\\ShellUtils.h"
#include "Theme.h"

static const wchar_t *SETTINGS_CLASS = L"NeoMDIE_SettingsDialog";
static std::function<void()> s_onSettingsChanged;

static HWND s_hCategoryList = nullptr;

// 9 Pages
static std::vector<HWND> s_pageControls[9];

// Page 0: General
static HWND s_hLangCombo = nullptr;
static HWND s_hChkRestoreTabs = nullptr;
static HWND s_hChkSingleInstance = nullptr;
static HWND s_hChkSaveWindowPos = nullptr;
static HWND s_hChkMinToTray = nullptr;
static HWND s_hChkCloseToTray = nullptr;
static HWND s_hChkReplaceExplorer = nullptr;

// Page 1: Window Actions (窗口动作)
static HWND s_hTreeActionCombo = nullptr;
static HWND s_hLinksActionCombo = nullptr;
static HWND s_hAddressActionCombo = nullptr;

// Page 2: Window & Split
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

// Page 7: Mouse Gestures
static HWND s_hChkGestures = nullptr;
static HWND s_hGestureList = nullptr;
static HWND s_hBtnGestureAdd = nullptr;
static HWND s_hBtnGestureEdit = nullptr;
static HWND s_hBtnGestureDel = nullptr;
static HWND s_hBtnGestureReset = nullptr;
static std::vector<GestureEntry> s_tempGestures;

// Page 8: Appearance & Toolbars
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

void SettingsDialog::Show(HWND hwndParent, std::function<void()> onSettingsChanged) {
  s_onSettingsChanged = onSettingsChanged;
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

  // Generous window dimensions so every item and explanation is fully visible
  int w = Theme::Get().Scale(760, hwndParent);
  int h = Theme::Get().Scale(580, hwndParent);

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
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
      DestroyWindow(hwndDlg);
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  EnableWindow(hwndParent, TRUE);
  SetForegroundWindow(hwndParent);
}

void SettingsDialog::SwitchTab(HWND hwnd, int tabIndex) {
  for (int p = 0; p < 9; ++p) {
    for (HWND hCtrl : s_pageControls[p]) {
      ShowWindow(hCtrl, (p == tabIndex) ? SW_SHOW : SW_HIDE);
    }
  }
}

void SettingsDialog::CreateControls(HWND hwnd) {
  for (int p = 0; p < 9; ++p) {
    s_pageControls[p].clear();
  }

  HINSTANCE hInst = GetModuleHandleW(nullptr);
  HFONT hFont = Theme::Get().GetUIFont(hwnd);
  HFONT hFontBold = Theme::Get().GetUIFontBold(hwnd);

  bool zh = I18n::Get().IsChinese();
  Config &cfg = Config::Get();

  int dlgW = Theme::Get().Scale(760, hwnd);
  int dlgH = Theme::Get().Scale(580, hwnd);

  int navW = Theme::Get().Scale(180, hwnd);
  int navH = dlgH - Theme::Get().Scale(90, hwnd);
  int marginX = Theme::Get().Scale(210, hwnd);
  int contentW = dlgW - marginX - Theme::Get().Scale(28, hwnd);
  int lineH = Theme::Get().Scale(24, hwnd);
  int spacing = Theme::Get().Scale(6, hwnd);

  // 1. Left Vertical Navigation (竖式排列)
  s_hCategoryList = CreateWindowExW(
      0, L"LISTBOX", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER,
      Theme::Get().Scale(14, hwnd), Theme::Get().Scale(14, hwnd),
      navW, navH,
      hwnd, (HMENU)201, hInst, nullptr);
  SendMessageW(s_hCategoryList, WM_SETFONT, (WPARAM)hFontBold, TRUE);

  const wchar_t* navTitlesZh[] = {
      L" ⚙ 常规设置",
      L" 🪟 窗口动作",
      L" ◫ 窗口与分屏",
      L" 📑 标签页",
      L" 🌳 侧边栏与树",
      L" 📋 视图与列表",
      L" 🔍 搜索与过滤",
      L" 🖱 鼠标手势",
      L" 🎨 外观与工具栏"
  };
  const wchar_t* navTitlesEn[] = {
      L" General",
      L" Window Actions",
      L" Window & Split",
      L" Tabs",
      L" Sidebar & Tree",
      L" View & List",
      L" Search & Filter",
      L" Mouse Gestures",
      L" Appearance & TB"
  };

  for (int i = 0; i < 9; ++i) {
      SendMessageW(s_hCategoryList, LB_ADDSTRING, 0, (LPARAM)(zh ? navTitlesZh[i] : navTitlesEn[i]));
  }
  SendMessageW(s_hCategoryList, LB_SETCURSEL, 0, 0);

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

  auto createChk = [&](int page, const wchar_t *text, int y, int id, bool checked) {
    HWND hChk = CreateWindowExW(0, L"BUTTON", text,
                                WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP | WS_CLIPSIBLINGS,
                                marginX, y, contentW, lineH,
                                hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hChk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return addCtrl(page, hChk);
  };

  int topY = Theme::Get().Scale(16, hwnd);

  // =========================================================================
  // Page 0: 常规设置 (General)
  // =========================================================================
  int y0 = topY;
  createLabel(0, zh ? L"界面显示语言 (Language):" : L"Language:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hLangCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y0, Theme::Get().Scale(320, hwnd), lineH * 4,
      hwnd, (HMENU)301, hInst, nullptr);
  SendMessageW(s_hLangCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"简体中文 (Simplified Chinese)");
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
  SendMessageW(s_hLangCombo, CB_SETCURSEL, zh ? 0 : 1, 0);
  addCtrl(0, s_hLangCombo);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"启动与实例管理:" : L"Startup & Instance Options:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkRestoreTabs = createChk(0, zh ? L"启动时自动恢复上次关闭前的所有标签页" : L"Restore tabs from previous session on startup",
                                y0, 302, cfg.restoreTabsOnStartup);
  y0 += lineH + spacing;

  s_hChkSingleInstance = createChk(0, zh ? L"单实例模式 (再次启动 NeoMDIE 时激活已有窗口并新建标签)" : L"Single instance mode (activate existing window and add tab)",
                                   y0, 303, true);
  y0 += lineH + spacing;

  s_hChkSaveWindowPos = createChk(0, zh ? L"记住窗口最后关闭时的屏幕位置与尺寸" : L"Remember window position and size upon exit",
                                  y0, 304, true);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"系统托盘动作:" : L"System Notification Tray:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkMinToTray = createChk(0, zh ? L"最小化时缩小至任务栏右下角系统托盘" : L"Minimize to system notification tray",
                              y0, 305, cfg.minimizeToTray);
  y0 += lineH + spacing;

  s_hChkCloseToTray = createChk(0, zh ? L"点击右上角关闭 [✕] 按钮时最小化至托盘而非直接退出" : L"Close button minimizes to tray instead of exiting",
                               y0, 306, cfg.closeToTray);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"系统文件关联与整合:" : L"System File Explorer Integration:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkReplaceExplorer = createChk(0, zh ? L"替代系统资源管理器 (双击打开文件夹或磁盘时默认使用 NeoMDIE)" : L"Replace Windows Explorer (open folders & drives in NeoMDIE)",
                                    y0, 307, ShellUtils::IsExplorerReplaced());

  // =========================================================================
  // Page 1: 窗口动作 (Window Actions)
  // =========================================================================
  int yAct = topY;
  createLabel(1, zh ? L"侧边栏目录树打开目标:" : L"Directory Tree Navigation Action:", yAct, true);
  yAct += lineH;
  createLabel(1, zh ? L"在左侧目录树中单击或双击任意文件夹时，文件在何处展现:" : L"When clicking a folder in the sidebar directory tree, open target in:", yAct, false);
  yAct += lineH + Theme::Get().Scale(2, hwnd);

  s_hTreeActionCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, yAct, Theme::Get().Scale(350, hwnd), lineH * 4,
      hwnd, (HMENU)318, hInst, nullptr);
  SendMessageW(s_hTreeActionCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前激活窗格中打开 (Active Pane)" : L"Open in Active Pane"));
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他分屏窗格中打开 (Other Split Pane)" : L"Open in Other Split Pane"));
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"新建标签页打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hTreeActionCombo, CB_SETCURSEL, cfg.actionTreeOpen, 0);
  addCtrl(1, s_hTreeActionCombo);
  yAct += lineH + spacing * 3;

  createLabel(1, zh ? L"链接栏快捷方式打开目标:" : L"Links Bar Action:", yAct, true);
  yAct += lineH;
  createLabel(1, zh ? L"在顶部链接栏中点击书签或常用目录快捷方式时，文件在何处展现:" : L"When clicking an item on the links toolbar, open target in:", yAct, false);
  yAct += lineH + Theme::Get().Scale(2, hwnd);

  s_hLinksActionCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, yAct, Theme::Get().Scale(350, hwnd), lineH * 4,
      hwnd, (HMENU)319, hInst, nullptr);
  SendMessageW(s_hLinksActionCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前激活窗格中打开 (Active Pane)" : L"Open in Active Pane"));
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他分屏窗格中打开 (Other Split Pane)" : L"Open in Other Split Pane"));
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"新建标签页打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hLinksActionCombo, CB_SETCURSEL, cfg.actionLinksOpen, 0);
  addCtrl(1, s_hLinksActionCombo);
  yAct += lineH + spacing * 3;

  createLabel(1, zh ? L"地址栏回车与转到目标:" : L"Address Bar Go Action:", yAct, true);
  yAct += lineH;
  createLabel(1, zh ? L"在地址面包屑/输入栏中回车或选择路径跳转时，文件在何处展现:" : L"When pressing Enter or navigating in the address bar, open target in:", yAct, false);
  yAct += lineH + Theme::Get().Scale(2, hwnd);

  s_hAddressActionCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, yAct, Theme::Get().Scale(350, hwnd), lineH * 4,
      hwnd, (HMENU)320, hInst, nullptr);
  SendMessageW(s_hAddressActionCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前激活窗格中打开 (Active Pane)" : L"Open in Active Pane"));
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他分屏窗格中打开 (Other Split Pane)" : L"Open in Other Split Pane"));
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"新建标签页打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hAddressActionCombo, CB_SETCURSEL, cfg.actionAddressOpen, 0);
  addCtrl(1, s_hAddressActionCombo);

  // =========================================================================
  // Page 2: 窗口与分屏 (Window & Split)
  // =========================================================================
  int y1 = topY;
  createLabel(2, zh ? L"默认分屏多栏布局模式:" : L"Default Split Window Mode:", y1, true);
  y1 += lineH + Theme::Get().Scale(2, hwnd);

  s_hLayoutCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y1, Theme::Get().Scale(320, hwnd), lineH * 6,
      hwnd, (HMENU)311, hInst, nullptr);
  SendMessageW(s_hLayoutCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"单屏 (1x1)" : L"Single (1x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"左右双栏 (1x2)" : L"Dual Vertical (1x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"上下双栏 (2x1)" : L"Dual Horizontal (2x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"四分屏 (2x2)" : L"Quad (2x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"左一右二 (1+2)" : L"Three Panes (1+2)"));
  SendMessageW(s_hLayoutCombo, CB_SETCURSEL, cfg.paneLayout, 0);
  addCtrl(2, s_hLayoutCombo);
  y1 += lineH + spacing * 2;

  createLabel(2, zh ? L"窗格顶栏与多栏联动控制:" : L"Pane Headers & Controls:", y1, true);
  y1 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkPaneHeader = createChk(2, zh ? L"在每个分屏顶部显示独立路径栏及 [🗕 最小化] [🗖 最大化] [✕ 关闭] 按钮" : L"Show pane header with path & minimize/maximize/close buttons",
                               y1, 312, true);
  y1 += lineH + spacing;

  s_hChkSyncNav = createChk(2, zh ? L"启用多窗格同步浏览 (分屏窗口同步跟随导航至相同目录)" : L"Enable synchronized multi-pane navigation",
                            y1, 313, cfg.syncNav);
  y1 += lineH + spacing;

  s_hChkLockToolbars = createChk(2, zh ? L"锁定工具栏 (锁定地址栏与搜索栏宽度，禁止鼠标拖动分割条)" : L"Lock toolbars (disable resizing address & search bars)",
                                 y1, 314, cfg.lockToolbars);

  // =========================================================================
  // Page 3: 标签页 (Tabs)
  // =========================================================================
  int y2 = topY;
  createLabel(3, zh ? L"标签栏外观与常用控件:" : L"Tab Bar Appearance & Controls:", y2, true);
  y2 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkShowNewTabBtn = createChk(3, zh ? L"在标签栏右侧显示新建标签按钮 (+)" : L"Show new tab button (+) on tab bar",
                                  y2, 321, true);
  y2 += lineH + spacing;

  s_hChkShowCloseBtn = createChk(3, zh ? L"在每个标签页上显示关闭按钮 (✕)" : L"Show close button (✕) on active tab",
                                 y2, 322, true);
  y2 += lineH + spacing * 2;

  createLabel(3, zh ? L"标签行为与鼠标动作:" : L"Tab Behavior & Mouse Actions:", y2, true);
  y2 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkAllowDupTabs = createChk(3, zh ? L"允许重复打开相同文件夹的标签页" : L"Allow duplicate tabs for the same folder",
                                 y2, 323, cfg.allowDuplicateTabs);
  y2 += lineH + spacing * 2;

  createLabel(3, zh ? L"双击标签页时执行的动作:" : L"Double click tab action:", y2, false);
  y2 += lineH + Theme::Get().Scale(2, hwnd);

  s_hTabDblClickCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y2, Theme::Get().Scale(320, hwnd), lineH * 5,
      hwnd, (HMENU)324, hInst, nullptr);
  SendMessageW(s_hTabDblClickCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"关闭标签页 (Close Tab)" : L"Close Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"锁定 / 固定标签页 (Pin Tab)" : L"Pin Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"刷新标签页 (Refresh)" : L"Refresh"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"向上一级目录 (Go Up)" : L"Go Up"));
  SendMessageW(s_hTabDblClickCombo, CB_SETCURSEL, cfg.tabDoubleClickAction, 0);
  addCtrl(3, s_hTabDblClickCombo);

  // =========================================================================
  // Page 4: 目录树与侧边栏 (Sidebar & Tree)
  // =========================================================================
  int y3 = topY;
  createLabel(4, zh ? L"文件夹目录树侧边栏 (Directory Tree):" : L"Directory Tree:", y3, true);
  y3 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkTree = createChk(4, zh ? L"启动时默认显示左侧文件夹目录树 (快捷键 F8)" : L"Show directory tree by default on launch (F8)",
                         y3, 331, cfg.showTree);
  y3 += lineH + spacing;

  s_hChkAutoExpandTree = createChk(4, zh ? L"浏览文件列表时，目录树自动同步展开并定位对应目录" : L"Auto expand tree and select folder when navigating list",
                                   y3, 332, cfg.autoExpandTree);
  y3 += lineH + spacing * 2;

  createLabel(4, zh ? L"快速预览窗格 (Quick Preview Pane):" : L"Quick Preview Pane:", y3, true);
  y3 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkPreview = createChk(4, zh ? L"启动时默认开启右侧快速预览窗格 (快捷键 Alt+P)" : L"Show quick preview pane by default on launch (Alt+P)",
                            y3, 333, cfg.previewEnabled);
  y3 += lineH + spacing;

  createLabel(4, zh ? L"  * 支持 PDF, Word/Excel/PowerPoint, 图片, 纯文本, 源代码, 音视频的极速预览" : L"  * Supports preview for PDF, Office, images, text, source code & media", y3, false);

  // =========================================================================
  // Page 5: 视图与文件列表 (View & File List)
  // =========================================================================
  int y4 = topY;
  createLabel(5, zh ? L"默认文件列表样式:" : L"Default View Mode:", y4, true);
  y4 += lineH + Theme::Get().Scale(2, hwnd);

  s_hViewModeCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y4, Theme::Get().Scale(320, hwnd), lineH * 7,
      hwnd, (HMENU)341, hInst, nullptr);
  SendMessageW(s_hViewModeCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"详细信息 (Details)" : L"Details"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"超大图标 (Extra Large Icons)" : L"Extra Large Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"大图标 (Large Icons)" : L"Large Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"中等图标 (Medium Icons)" : L"Medium Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"小图标 (Small Icons)" : L"Small Icons"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"列表 (List)" : L"List"));
  SendMessageW(s_hViewModeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"平铺 (Tiles)" : L"Tiles"));
  SendMessageW(s_hViewModeCombo, CB_SETCURSEL, cfg.viewMode, 0);
  addCtrl(5, s_hViewModeCombo);
  y4 += lineH + spacing * 2;

  createLabel(5, zh ? L"文件列表显示与计算选项:" : L"File List Display & Calculation:", y4, true);
  y4 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkHidden = createChk(5, zh ? L"显示隐藏的文件和系统文件" : L"Show hidden files and system files",
                           y4, 342, cfg.showHidden);
  y4 += lineH + spacing;

  s_hChkCalcSize = createChk(5, zh ? L"在后台自动计算并显示文件夹大小" : L"Automatically calculate folder size in background",
                             y4, 343, cfg.calcFolderSize);
  y4 += lineH + spacing;

  s_hChkFullRowSelect = createChk(5, zh ? L"详细信息视图中整行选择 (Full Row Select)" : L"Full row select in Details view",
                                  y4, 344, true);
  y4 += lineH + spacing;

  s_hChkZebraStriping = createChk(5, zh ? L"偶数行以交替轻微底色区分 (Zebra Striping)" : L"Alternate row background color (Zebra striping)",
                                  y4, 345, true);
  y4 += lineH + spacing * 2;

  createLabel(5, zh ? L"快捷键分类说明 (MDIE 经典模式):" : L"Keyboard Shortcuts (MDIE Mode):", y4, true);
  y4 += lineH;
  createLabel(5, zh ? L"• 全体快捷键: Ctrl+T (新建标签), Ctrl+W (关标签), Ctrl+Tab, Alt+D, Ctrl+F, F5"
                    : L"• Global: Ctrl+T, Ctrl+W, Ctrl+Tab, Alt+D, Ctrl+F, F5", y4, false);
  y4 += lineH;
  createLabel(5, zh ? L"• 列表快捷键: Space (快速预览), Backspace (上级), Enter (打开), Home/End (顶底), F2 (重命名)"
                    : L"• List: Space (preview), Backspace (up), Enter (open), Home/End, F2", y4, false);

  // =========================================================================
  // Page 6: 搜索与即时过滤 (Search & Filter)
  // =========================================================================
  int y5 = topY;
  createLabel(6, zh ? L"即时搜索与过滤选项:" : L"Incremental Search & Filter Options:", y5, true);
  y5 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkIncremental = createChk(6, zh ? L"启用实时增量搜索过滤 (输入字符即刻过滤文件列表)" : L"Enable incremental search filtering while typing",
                                y5, 351, true);
  y5 += lineH + spacing;

  s_hChkSearchSubfolders = createChk(6, zh ? L"搜索过滤时默认递归包含所有子文件夹" : L"Include subfolders recursively by default when searching",
                                     y5, 352, cfg.searchSubfoldersDefault);
  y5 += lineH + spacing;

  s_hChkMatchCase = createChk(6, zh ? L"区分英文字母大小写 (Match Case)" : L"Match Case",
                              y5, 353, false);
  y5 += lineH + spacing * 2;

  createLabel(6, zh ? L"  * 提示: 工具栏上的搜索栏与地址栏可通过鼠标中间的分隔条自由调整宽度。" : L"  * Tip: The search bar length can be dynamically adjusted by dragging the toolbar divider.", y5, false);

  // =========================================================================
  // Page 7: 鼠标手势 (Gestures)
  // =========================================================================
  int y6 = topY;
  s_hChkGestures = createChk(7, zh ? L"启用鼠标右键划动手势 (按住鼠标右键划出轨迹)" : L"Enable mouse gestures (Right-drag)",
                             y6, 361, cfg.gesturesEnabled);
  y6 += lineH + spacing;

  createLabel(7, zh ? L"自定义手势与映射动作 (可自由添加、修改或删除):" : L"Custom Gestures (Add, edit action, or delete):", y6, true);
  y6 += lineH + Theme::Get().Scale(2, hwnd);

  s_tempGestures = cfg.gestures;

  s_hGestureList = CreateWindowExW(
      0, WC_LISTVIEWW, L"",
      WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
      marginX, y6, contentW, Theme::Get().Scale(160, hwnd),
      hwnd, (HMENU)362, hInst, nullptr);
  SendMessageW(s_hGestureList, WM_SETFONT, (WPARAM)hFont, TRUE);
  ListView_SetExtendedListViewStyle(s_hGestureList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  LVCOLUMNW lvcG = { 0 };
  lvcG.mask = LVCF_TEXT | LVCF_WIDTH;
  lvcG.cx = Theme::Get().Scale(220, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"手势轨迹 (Pattern)" : L"Pattern");
  ListView_InsertColumn(s_hGestureList, 0, &lvcG);

  lvcG.cx = contentW - lvcG.cx - Theme::Get().Scale(25, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"执行动作 (Action)" : L"Action");
  ListView_InsertColumn(s_hGestureList, 1, &lvcG);

  PopulateGestureList(s_hGestureList);
  addCtrl(7, s_hGestureList);

  y6 += Theme::Get().Scale(168, hwnd);

  int gBtnW = Theme::Get().Scale(110, hwnd);
  int gBtnH = Theme::Get().Scale(28, hwnd);
  int gBtnX = marginX;

  auto createGBtn = [&](const wchar_t* txt, int id) {
      HWND hBtn = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                                 gBtnX, y6, gBtnW, gBtnH, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
      SendMessageW(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
      addCtrl(7, hBtn);
      gBtnX += gBtnW + Theme::Get().Scale(10, hwnd);
      return hBtn;
  };

  s_hBtnGestureAdd   = createGBtn(zh ? L"➕ 添加手势..." : L"➕ Add...", 363);
  s_hBtnGestureEdit  = createGBtn(zh ? L"✎ 修改动作..." : L"✎ Edit...", 364);
  s_hBtnGestureDel   = createGBtn(zh ? L"✕ 删除手势" : L"✕ Delete", 365);
  s_hBtnGestureReset = createGBtn(zh ? L"↺ 恢复默认" : L"↺ Defaults", 366);

  // =========================================================================
  // Page 8: 外观与工具栏 (Appearance & Toolbars)
  // =========================================================================
  int y7 = topY;
  createLabel(8, zh ? L"界面深浅主题外观模式:" : L"Appearance Theme:", y7, true);
  y7 += lineH + Theme::Get().Scale(2, hwnd);

  s_hThemeCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y7, Theme::Get().Scale(320, hwnd), lineH * 4,
      hwnd, (HMENU)371, hInst, nullptr);
  SendMessageW(s_hThemeCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"跟随 Windows 系统深浅模式" : L"Follow Windows System"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"浅色模式 (Light Mode)" : L"Light Mode"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"深色暗黑模式 (Dark Mode)" : L"Dark Mode"));
  int themeIdx = (cfg.themeMode == 0) ? 1 : ((cfg.themeMode == 1) ? 2 : 0);
  SendMessageW(s_hThemeCombo, CB_SETCURSEL, themeIdx, 0);
  addCtrl(8, s_hThemeCombo);
  y7 += lineH + spacing * 2;

  createLabel(8, zh ? L"工具栏与状态栏组件显隐:" : L"Toolbars & Status Bar Visibility:", y7, true);
  y7 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkTBStandard = createChk(8, zh ? L"显示标准导航操作按钮栏 (后退/前进/上级/刷新/新建标签/目录树)" : L"Show standard navigation buttons bar",
                               y7, 372, cfg.showToolbarStandard);
  y7 += lineH + spacing;

  s_hChkTBAddress = createChk(8, zh ? L"显示地址面包屑导航栏" : L"Show address breadcrumb bar",
                              y7, 373, cfg.showToolbarAddress);
  y7 += lineH + spacing;

  s_hChkTBSearch = createChk(8, zh ? L"显示搜索与即时过滤栏" : L"Show search & incremental filter bar",
                             y7, 374, cfg.showToolbarSearch);
  y7 += lineH + spacing;

  s_hChkTBLinks = createChk(8, zh ? L"显示常用书签与链接栏" : L"Show bookmarks & links bar",
                            y7, 375, cfg.showToolbarLinks);
  y7 += lineH + spacing;

  s_hChkStatusBar = createChk(8, zh ? L"显示底部状态栏 (项目总数、占用大小等信息)" : L"Show status bar with item counts & size",
                             y7, 376, cfg.showStatusBar);

  // Initial tab page display
  SwitchTab(hwnd, 0);

  // Bottom OK & Cancel buttons
  int btnW = Theme::Get().Scale(96, hwnd);
  int btnH = Theme::Get().Scale(30, hwnd);
  int btnY = dlgH - Theme::Get().Scale(65, hwnd);

  HWND hBtnOK = CreateWindowExW(
      0, L"BUTTON", I18n::Get().T(StringId::SettingsBtnOK),
      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
      dlgW - Theme::Get().Scale(240, hwnd), btnY, btnW, btnH,
      hwnd, (HMENU)IDOK, hInst, nullptr);
  SendMessageW(hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);

  HWND hBtnCancel = CreateWindowExW(
      0, L"BUTTON", I18n::Get().T(StringId::SettingsBtnCancel),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
      dlgW - Theme::Get().Scale(130, hwnd), btnY, btnW, btnH,
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

  bool replaceExp = (SendMessageW(s_hChkReplaceExplorer, BM_GETCHECK, 0, 0) == BST_CHECKED);
  if (replaceExp != ShellUtils::IsExplorerReplaced()) {
    ShellUtils::SetExplorerReplaced(replaceExp);
  }

  // Page 1: Window Actions
  cfg.actionTreeOpen = (int)SendMessageW(s_hTreeActionCombo, CB_GETCURSEL, 0, 0);
  cfg.actionLinksOpen = (int)SendMessageW(s_hLinksActionCombo, CB_GETCURSEL, 0, 0);
  cfg.actionAddressOpen = (int)SendMessageW(s_hAddressActionCombo, CB_GETCURSEL, 0, 0);

  // Page 2: Window & Split
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
  cfg.calcFolderSize = (SendMessageW(s_hChkCalcSize, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 6: Search & Filter
  cfg.searchSubfoldersDefault = (SendMessageW(s_hChkSearchSubfolders, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 7: Gestures
  cfg.gesturesEnabled = (SendMessageW(s_hChkGestures, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.gestures = s_tempGestures;

  // Page 8: Appearance & Toolbars
  int themeSel = (int)SendMessageW(s_hThemeCombo, CB_GETCURSEL, 0, 0);
  if (themeSel == 1) cfg.themeMode = 0;
  else if (themeSel == 2) cfg.themeMode = 1;
  else cfg.themeMode = -1;

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
    HDC hdc = (HDC)wParam;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::Get().ClrTextPrimary());
    return (LRESULT)Theme::Get().BrushBackground();
  }

  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORLISTBOX: {
    HDC hdc = (HDC)wParam;
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, Theme::Get().ClrSurface());
    SetTextColor(hdc, Theme::Get().ClrTextPrimary());
    return (LRESULT)Theme::Get().BrushSurface();
  }

  case WM_CTLCOLORBTN: {
    HDC hdc = (HDC)wParam;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::Get().ClrTextPrimary());
    return (LRESULT)Theme::Get().BrushBackground();
  }

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int sepX = Theme::Get().Scale(200, hwnd);
    RECT rcSep = { sepX, Theme::Get().Scale(14, hwnd), sepX + 1, rc.bottom - Theme::Get().Scale(70, hwnd) };
    FillRect(hdc, &rcSep, Theme::Get().BrushBorder());
    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->idFrom == 362 && pnm->code == NM_DBLCLK) {
      SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(364, BN_CLICKED), (LPARAM)s_hBtnGestureEdit);
      return 0;
    }
    break;
  }

  case WM_COMMAND: {
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    if (id == 201 && code == LBN_SELCHANGE) {
      int sel = (int)SendMessageW(s_hCategoryList, LB_GETCURSEL, 0, 0);
      SwitchTab(hwnd, sel);
      return 0;
    }

    if (id == 363) { // Add gesture
      HMENU hPop = CreatePopupMenu();
      struct Cand { const wchar_t* lbl; const char* c; };
      Cand cands[] = {
        { L"🠔 向左 (L)", "L" },
        { L"🠖 向右 (R)", "R" },
        { L"🠕 向上 (U)", "U" },
        { L"🠗 向下 (D)", "D" },
        { L"🠗🠖 下然后右 (DR)", "DR" },
        { L"🠕🠖 上然后右 (UR)", "UR" },
        { L"🠔🠕 左然后上 (LU)", "LU" },
        { L"🠔🠗 左然后下 (LD)", "LD" },
        { L"🠕🠗 上然后下 (UD)", "UD" },
        { L"🠗🠕 下然后上 (DU)", "DU" }
      };
      for (int i = 0; i < 10; ++i) {
        AppendMenuW(hPop, MF_STRING, 5001 + i, cands[i].lbl);
      }
      POINT pt;
      GetCursorPos(&pt);
      int selC = TrackPopupMenu(hPop, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
      DestroyMenu(hPop);
      if (selC >= 5001 && selC <= 5010) {
        std::string newCode = cands[selC - 5001].c;
        bool exists = false;
        for (const auto& g : s_tempGestures) {
          if (g.code == newCode) { exists = true; break; }
        }
        if (!exists) {
          s_tempGestures.push_back({ newCode, (int)GestureAction::CloseTab, L"" });
          PopulateGestureList(s_hGestureList);
        }
      }
      return 0;
    } else if (id == 364) { // Edit gesture action
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempGestures.size()) {
        HMENU hAct = CreatePopupMenu();
        struct ActItem { int id; const wchar_t* lbl; };
        ActItem acts[] = {
          { (int)GestureAction::Back, L"后退 (Back)" },
          { (int)GestureAction::Forward, L"前进 (Forward)" },
          { (int)GestureAction::Up, L"向上一级目录 (Go Up)" },
          { (int)GestureAction::Refresh, L"刷新 (Refresh)" },
          { (int)GestureAction::CloseTab, L"关闭当前标签页 (Close Tab)" },
          { (int)GestureAction::NewTab, L"新建标签页 (New Tab)" },
          { (int)GestureAction::ReopenTab, L"恢复最后关闭的标签页 (Reopen Closed Tab)" },
          { (int)GestureAction::ScrollTop, L"滚动到列表最开头 (Scroll to Top)" },
          { (int)GestureAction::ScrollBottom, L"滚动到列表最末尾 (Scroll to Bottom)" },
          { (int)GestureAction::ToggleTree, L"切换左侧目录树显隐 (Toggle Tree)" },
          { (int)GestureAction::TogglePreview, L"切换右侧快速预览显隐 (Toggle Preview)" }
        };
        for (int i = 0; i < 11; ++i) {
          AppendMenuW(hAct, MF_STRING, 6001 + i, acts[i].lbl);
        }
        POINT pt;
        GetCursorPos(&pt);
        int selA = TrackPopupMenu(hAct, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hAct);
        if (selA >= 6001 && selA <= 6011) {
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
      Config defCfg;
      s_tempGestures = defCfg.gestures;
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

with open("src/ui/SettingsDialog.cpp", "w", encoding="utf-8") as f:
    f.write(code)

print("SettingsDialog.cpp rewritten with vertical navigation and visible descriptions!")
