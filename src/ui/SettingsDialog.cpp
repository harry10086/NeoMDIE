#include "SettingsDialog.h"
#include "..\core\Config.h"
#include "..\core\I18n.h"
#include "..\core\GestureEngine.h"
#include "..\core\ShellUtils.h"
#include "Theme.h"
#include "..\core\ShellUtils.h"

static const wchar_t *SETTINGS_CLASS = L"NeoMDIE_SettingsDialog";
static std::function<void()> s_onSettingsChanged;

static HWND s_hTab = nullptr;

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

void SettingsDialog::Show(HWND hwndParent, std::function<void()> onSettingsChanged, int initialCategory) {
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

  int w = Theme::Get().Scale(580, hwndParent);
  int h = Theme::Get().Scale(500, hwndParent);

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

static LRESULT CALLBACK DarkHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_ERASEBKGND && Theme::Get().IsDarkMode()) {
        return 1;
    }
    if (msg == WM_PAINT && Theme::Get().IsDarkMode()) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
        HGDIOBJ oldBm = SelectObject(hdcMem, hbmMem);

        FillRect(hdcMem, &rcClient, Theme::Get().BrushSurface());

        int count = Header_GetItemCount(hwnd);
        HFONT hFont = Theme::Get().GetUIFont(hwnd);
        HGDIOBJ oldFont = SelectObject(hdcMem, hFont);
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
        HPEN hPenBorder = Theme::Get().PenBorder();
        HGDIOBJ oldPen = SelectObject(hdcMem, hPenBorder);

        for (int i = 0; i < count; ++i) {
            RECT rcItem;
            Header_GetItemRect(hwnd, i, &rcItem);
            wchar_t text[256] = { 0 };
            HDITEMW hdi = { sizeof(hdi) };
            hdi.mask = HDI_TEXT | HDI_FORMAT;
            hdi.pszText = text;
            hdi.cchTextMax = 256;
            Header_GetItem(hwnd, i, &hdi);

            RECT rcText = rcItem;
            rcText.left += Theme::Get().Scale(8, hwnd);
            rcText.right -= Theme::Get().Scale(8, hwnd);
            DrawTextW(hdcMem, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            MoveToEx(hdcMem, rcItem.right - 1, rcItem.top + 3, nullptr);
            LineTo(hdcMem, rcItem.right - 1, rcItem.bottom - 3);
        }
        MoveToEx(hdcMem, 0, rcClient.bottom - 1, nullptr);
        LineTo(hdcMem, rcClient.right, rcClient.bottom - 1);

        BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, oldPen);
        SelectObject(hdcMem, oldFont);
        SelectObject(hdcMem, oldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
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

  int marginX = Theme::Get().Scale(24, hwnd);
  int lineH = Theme::Get().Scale(24, hwnd);
  int dlgW = Theme::Get().Scale(580, hwnd);
  int dlgH = Theme::Get().Scale(500, hwnd);

  // Tab Control
  s_hTab = CreateWindowExW(
      0, WC_TABCONTROLW, L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
      Theme::Get().Scale(12, hwnd), Theme::Get().Scale(12, hwnd),
      dlgW - Theme::Get().Scale(40, hwnd), dlgH - Theme::Get().Scale(90, hwnd),
      hwnd, (HMENU)200, hInst, nullptr);
  SendMessageW(s_hTab, WM_SETFONT, (WPARAM)hFont, TRUE);

  const wchar_t* tabTitlesZh[] = { L"常规", L"窗口与分屏", L"分页标签", L"侧边栏与树", L"视图与列表", L"搜索过滤", L"鼠标手势", L"外观工具栏" };
  const wchar_t* tabTitlesEn[] = { L"General", L"Window Actions", L"Window & Split", L"Tabs", L"Sidebar & Tree", L"View & List", L"Search & Filter", L"Gestures", L"Appearance" };

  for (int i = 0; i < 9; ++i) {
    TCITEMW tie = { 0 };
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)(zh ? tabTitlesZh[i] : tabTitlesEn[i]);
    TabCtrl_InsertItem(s_hTab, i, &tie);
  }

  // Helpers
  auto addCtrl = [&](int page, HWND hCtrl) {
    s_pageControls[page].push_back(hCtrl);
    return hCtrl;
  };

  auto createLabel = [&](int page, const wchar_t *text, int y, bool bold = false) {
    HWND hLbl = CreateWindowExW(0, L"STATIC", text, WS_CHILD,
                                marginX, y, Theme::Get().Scale(510, hwnd),
                                lineH, hwnd, nullptr, hInst, nullptr);
    SendMessageW(hLbl, WM_SETFONT, (WPARAM)(bold ? hFontBold : hFont), TRUE);
    return addCtrl(page, hLbl);
  };

  auto createChk = [&](int page, const wchar_t *text, int y, int id, bool checked) {
    HWND hChk = CreateWindowExW(0, L"BUTTON", text,
                                WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
                                marginX, y, Theme::Get().Scale(510, hwnd), lineH,
                                hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hChk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return addCtrl(page, hChk);
  };

  int topY = Theme::Get().Scale(52, hwnd);
  int spacing = Theme::Get().Scale(8, hwnd);

  // =========================================================================
  // Page 0: 常规 (General)
  // =========================================================================
  int y0 = topY;
  createLabel(0, zh ? L"语言设置 (Language):" : L"Language:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hLangCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y0, Theme::Get().Scale(280, hwnd), lineH * 4,
      hwnd, (HMENU)301, hInst, nullptr);
  SendMessageW(s_hLangCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"简体中文 (Simplified Chinese)");
  SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
  SendMessageW(s_hLangCombo, CB_SETCURSEL, zh ? 0 : 1, 0);
  addCtrl(0, s_hLangCombo);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"启动与实例:" : L"Startup & Instance:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkRestoreTabs = createChk(0, zh ? L"启动时自动恢复上次会话的所有标签页" : L"Restore tabs from previous session on startup",
                                y0, 302, cfg.restoreTabsOnStartup);
  y0 += lineH + spacing;

  s_hChkSingleInstance = createChk(0, zh ? L"单实例模式 (再次打开新窗口时激活已有主窗口)" : L"Single instance mode (activate existing window)",
                                   y0, 303, true);
  y0 += lineH + spacing;

  s_hChkSaveWindowPos = createChk(0, zh ? L"记住窗口位置与大小" : L"Remember window position and size",
                                  y0, 304, true);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"系统托盘动作:" : L"System Tray Actions:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkMinToTray = createChk(0, zh ? L"最小化时缩小至系统托盘" : L"Minimize to system notification tray",
                              y0, 305, cfg.minimizeToTray);
  y0 += lineH + spacing;

  s_hChkCloseToTray = createChk(0, zh ? L"关闭窗口时最小化至托盘而非直接退出" : L"Close button minimizes to tray instead of exiting",
                               y0, 306, cfg.closeToTray);

  // =========================================================================
  // Page 1: 窗口与分屏 (Window & Split)
  // =========================================================================
  int y1 = topY;
  createLabel(1, zh ? L"默认分屏窗口模式:" : L"Default Split Window Mode:", y1, true);
  y1 += lineH + Theme::Get().Scale(2, hwnd);

  s_hLayoutCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y1, Theme::Get().Scale(280, hwnd), lineH * 6,
      hwnd, (HMENU)311, hInst, nullptr);
  SendMessageW(s_hLayoutCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"单屏 (1x1)" : L"Single (1x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"左右双屏 (1x2)" : L"Dual Vertical (1x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"上下双屏 (2x1)" : L"Dual Horizontal (2x1)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"四分屏 (2x2)" : L"Quad (2x2)"));
  SendMessageW(s_hLayoutCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"左一右二 (1+2)" : L"Three Panes (1+2)"));
  SendMessageW(s_hLayoutCombo, CB_SETCURSEL, cfg.paneLayout, 0);
  addCtrl(1, s_hLayoutCombo);
  y1 += lineH + spacing * 2;

  createLabel(1, zh ? L"窗格标题栏与控制:" : L"Pane Headers & Controls:", y1, true);
  y1 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkPaneHeader = createChk(1, zh ? L"在每个分屏顶部显示路径栏及 [🗕 最小化] [🗖 最大化] [✕ 关闭] 按钮" : L"Show pane header with path & minimize/maximize/close buttons",
                               y1, 312, true);
  y1 += lineH + spacing;

  s_hChkSyncNav = createChk(1, zh ? L"启用多窗格同步浏览 (分屏窗口同步导航至相同文件夹)" : L"Enable synchronized multi-pane navigation",
                            y1, 313, cfg.syncNav);
  y1 += lineH + spacing;

  s_hChkLockToolbars = createChk(1, zh ? L"锁定工具栏 (禁止鼠标拖动调整地址栏与搜索栏长度)" : L"Lock toolbars (disable resizing address & search bars)",
                                 y1, 314, cfg.lockToolbars);

  // =========================================================================
  // Page 2: 分页标签 (Tabs)
  // =========================================================================
  int y2 = topY;
  createLabel(2, zh ? L"标签栏外观与控件:" : L"Tab Bar Appearance & Controls:", y2, true);
  y2 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkShowNewTabBtn = createChk(2, zh ? L"在标签栏右侧显示新建标签按钮 (+)" : L"Show new tab button (+) on tab bar",
                                  y2, 321, true);
  y2 += lineH + spacing;

  s_hChkShowCloseBtn = createChk(2, zh ? L"在每个标签页上显示关闭按钮 (✕)" : L"Show close button (✕) on active tab",
                                 y2, 322, true);
  y2 += lineH + spacing * 2;

  createLabel(2, zh ? L"标签行为与鼠标动作:" : L"Tab Behavior & Mouse Actions:", y2, true);
  y2 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkAllowDupTabs = createChk(2, zh ? L"允许重复打开相同文件夹的标签页" : L"Allow duplicate tabs for the same folder",
                                 y2, 323, cfg.allowDuplicateTabs);
  y2 += lineH + spacing * 2;

  createLabel(2, zh ? L"双击标签页执行的动作:" : L"Double click tab action:", y2, false);
  y2 += lineH + Theme::Get().Scale(2, hwnd);

  s_hTabDblClickCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y2, Theme::Get().Scale(280, hwnd), lineH * 5,
      hwnd, (HMENU)324, hInst, nullptr);
  SendMessageW(s_hTabDblClickCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"关闭标签页 (Close Tab)" : L"Close Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"锁定 / 固定标签页 (Pin Tab)" : L"Pin Tab"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"刷新标签页 (Refresh)" : L"Refresh"));
  SendMessageW(s_hTabDblClickCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"向上一级目录 (Go Up)" : L"Go Up"));
  SendMessageW(s_hTabDblClickCombo, CB_SETCURSEL, cfg.tabDoubleClickAction, 0);
  addCtrl(2, s_hTabDblClickCombo);

  // =========================================================================
  // Page 3: 目录树与侧边栏 (Sidebar & Tree)
  // =========================================================================
  int y3 = topY;
  createLabel(3, zh ? L"文件夹目录树 (Directory Tree):" : L"Directory Tree:", y3, true);
  y3 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkTree = createChk(3, zh ? L"启动时默认显示左侧文件夹目录树 (快捷键 F8)" : L"Show directory tree by default on launch (F8)",
                         y3, 331, cfg.showTree);
  y3 += lineH + spacing;

  s_hChkAutoExpandTree = createChk(3, zh ? L"浏览文件列表时，目录树自动同步展开并定位对应目录" : L"Auto expand tree and select folder when navigating list",
                                   y3, 332, cfg.autoExpandTree);
  y3 += lineH + spacing * 2;

  createLabel(3, zh ? L"快速预览窗格 (Preview Pane):" : L"Quick Preview Pane:", y3, true);
  y3 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkPreview = createChk(3, zh ? L"启动时默认开启右侧快速预览窗格 (快捷键 Alt+P)" : L"Show quick preview pane by default on launch (Alt+P)",
                            y3, 333, cfg.previewEnabled);
  y3 += lineH + spacing;

  createLabel(3, zh ? L"  * 支持 PDF, Word/Excel/PowerPoint, 图片, 纯文本/源代码, 媒体文件的极速预览" : L"  * Supports preview for PDF, Office, images, text, source code & media", y3, false);

  // =========================================================================
  // Page 4: 视图与文件列表 (View & File List)
  // =========================================================================
  int y4 = topY;
  createLabel(4, zh ? L"默认查看视图样式:" : L"Default View Mode:", y4, true);
  y4 += lineH + Theme::Get().Scale(2, hwnd);

  s_hViewModeCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y4, Theme::Get().Scale(280, hwnd), lineH * 7,
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
  addCtrl(4, s_hViewModeCombo);
  y4 += lineH + spacing * 2;

  createLabel(4, zh ? L"文件列表显示与计算选项:" : L"File List Display & Calculation:", y4, true);
  y4 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkHidden = createChk(4, zh ? L"显示隐藏的文件和系统文件" : L"Show hidden files and system files",
                           y4, 342, cfg.showHidden);
  y4 += lineH + spacing;

  s_hChkCalcSize = createChk(4, zh ? L"在后台自动计算并显示文件夹大小" : L"Automatically calculate folder size in background",
                             y4, 343, cfg.calcFolderSize);
  y4 += lineH + spacing;

  s_hChkFullRowSelect = createChk(4, zh ? L"详细信息视图中整行选择 (Full Row Select)" : L"Full row select in Details view",
                                  y4, 344, true);
  y4 += lineH + spacing;

  s_hChkZebraStriping = createChk(4, zh ? L"偶数行以交替轻微底色区分 (Zebra Striping)" : L"Alternate row background color (Zebra striping)",
                                  y4, 345, true);
  y4 += lineH + spacing * 2;

  createLabel(4, zh ? L"按键设置与快捷键分类 (MDIE 原版模式):" : L"Keyboard Shortcuts (MDIE Mode):", y4, true);
  y4 += lineH + Theme::Get().Scale(2, hwnd);
  createLabel(4, zh ? L"• 全体快捷键: 全局响应 (Ctrl+T 新建标签, Ctrl+W 关闭标签, Ctrl+Tab 换标签, Alt+D 地址, Ctrl+F 搜索, Ctrl+1..4 分屏, F5 刷新)"
                    : L"• Global Shortcuts: Active everywhere (Ctrl+T New Tab, Ctrl+W, Ctrl+Tab, Alt+D, Ctrl+F, Ctrl+1..4, F5)", y4, false);
  y4 += lineH + spacing;
  createLabel(4, zh ? L"• 列表快捷键: 仅文件列表聚焦时生效 (Space 快速预览, Backspace 返回上一级, Enter 打开, Home/End 顶底, PgUp/Dn 翻页, F2 重命名)"
                    : L"• List Shortcuts: Active in file list (Space preview, Backspace up, Enter open, Home/End top/bottom, PgUp/Dn, F2)", y4, false);

  // =========================================================================
  // Page 5: 搜索与即时过滤 (Search & Filter)
  // =========================================================================
  int y5 = topY;
  createLabel(5, zh ? L"即时搜索与过滤选项:" : L"Incremental Search & Filter Options:", y5, true);
  y5 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkIncremental = createChk(5, zh ? L"启用实时增量搜索过滤 (输入字符即刻过滤文件列表)" : L"Enable incremental search filtering while typing",
                                y5, 351, true);
  y5 += lineH + spacing;

  s_hChkSearchSubfolders = createChk(5, zh ? L"搜索过滤时默认递归包含所有子文件夹" : L"Include subfolders recursively by default when searching",
                                     y5, 352, cfg.searchSubfoldersDefault);
  y5 += lineH + spacing;

  s_hChkMatchCase = createChk(5, zh ? L"区分英文字母大小写 (Match Case)" : L"Match Case",
                              y5, 353, false);
  y5 += lineH + spacing * 2;

  createLabel(5, zh ? L"  * 提示: 工具栏上的搜索栏与地址栏可通过鼠标中间的分隔条自由调整宽度。" : L"  * Tip: The search bar length can be dynamically adjusted by dragging the toolbar divider.", y5, false);

  // =========================================================================
  // Page 6: 鼠标手势 (Gestures)
  // =========================================================================
  int y6 = topY;
  s_hChkGestures = createChk(6, zh ? L"启用鼠标右键划动手势 (按住鼠标右键划出轨迹)" : L"Enable mouse gestures (Right-drag)",
                             y6, 361, cfg.gesturesEnabled);
  y6 += lineH + spacing;

  createLabel(6, zh ? L"自定义手势与映射动作 (可自由添加、修改或删除):" : L"Custom Gestures (Add, edit action, or delete):", y6, true);
  y6 += lineH + Theme::Get().Scale(2, hwnd);

  s_tempGestures = cfg.gestures;

  s_hGestureList = CreateWindowExW(
      0, WC_LISTVIEWW, L"",
      WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
      marginX, y6, Theme::Get().Scale(510, hwnd), Theme::Get().Scale(140, hwnd),
      hwnd, (HMENU)362, hInst, nullptr);
  SendMessageW(s_hGestureList, WM_SETFONT, (WPARAM)hFont, TRUE);
  ListView_SetExtendedListViewStyle(s_hGestureList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  LVCOLUMNW lvcG = { 0 };
  lvcG.mask = LVCF_TEXT | LVCF_WIDTH;
  lvcG.cx = Theme::Get().Scale(210, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"手势轨迹 (Pattern)" : L"Pattern");
  ListView_InsertColumn(s_hGestureList, 0, &lvcG);

  lvcG.cx = Theme::Get().Scale(280, hwnd);
  lvcG.pszText = (LPWSTR)(zh ? L"执行动作 (Action)" : L"Action");
  ListView_InsertColumn(s_hGestureList, 1, &lvcG);

  PopulateGestureList(s_hGestureList);
  addCtrl(6, s_hGestureList);

  y6 += Theme::Get().Scale(145, hwnd);

  int gBtnW = Theme::Get().Scale(118, hwnd);
  int gBtnH = Theme::Get().Scale(26, hwnd);
  int gBtnX = marginX;

  auto createGBtn = [&](const wchar_t* txt, int id) {
      HWND hBtn = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                                 gBtnX, y6, gBtnW, gBtnH, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
      SendMessageW(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
      addCtrl(6, hBtn);
      gBtnX += gBtnW + Theme::Get().Scale(10, hwnd);
      return hBtn;
  };

  s_hBtnGestureAdd   = createGBtn(zh ? L"➕ 添加手势..." : L"➕ Add...", 363);
  s_hBtnGestureEdit  = createGBtn(zh ? L"✎ 修改动作..." : L"✎ Edit...", 364);
  s_hBtnGestureDel   = createGBtn(zh ? L"✕ 删除手势" : L"✕ Delete", 365);
  s_hBtnGestureReset = createGBtn(zh ? L"↺ 恢复默认" : L"↺ Defaults", 366);

  // =========================================================================
  // Page 7: 外观与工具栏 (Appearance & Toolbars)
  // =========================================================================
  int y7 = topY;
  createLabel(7, zh ? L"界面主题外观模式:" : L"Appearance Theme:", y7, true);
  y7 += lineH + Theme::Get().Scale(2, hwnd);

  s_hThemeCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, y7, Theme::Get().Scale(280, hwnd), lineH * 4,
      hwnd, (HMENU)371, hInst, nullptr);
  SendMessageW(s_hThemeCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"跟随 Windows 系统深浅模式" : L"Follow Windows System"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"浅色模式 (Light Mode)" : L"Light Mode"));
  SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"深色暗黑模式 (Dark Mode)" : L"Dark Mode"));
  int themeIdx = (cfg.themeMode == 0) ? 1 : ((cfg.themeMode == 1) ? 2 : 0);
  SendMessageW(s_hThemeCombo, CB_SETCURSEL, themeIdx, 0);
  addCtrl(7, s_hThemeCombo);
  y7 += lineH + spacing * 2;

  createLabel(7, zh ? L"工具栏与状态栏组件显示:" : L"Toolbars & Status Bar Visibility:", y7, true);
  y7 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkTBStandard = createChk(7, zh ? L"显示标准导航操作按钮栏 (后退/前进/上级/刷新/新建标签/目录树)" : L"Show standard navigation buttons bar",
                               y7, 372, cfg.showToolbarStandard);
  y7 += lineH + spacing;

  s_hChkTBAddress = createChk(7, zh ? L"显示地址面包屑导航栏" : L"Show address breadcrumb bar",
                              y7, 373, cfg.showToolbarAddress);
  y7 += lineH + spacing;

  s_hChkTBSearch = createChk(7, zh ? L"显示搜索与即时过滤栏" : L"Show search & incremental filter bar",
                             y7, 374, cfg.showToolbarSearch);
  y7 += lineH + spacing;

  s_hChkTBLinks = createChk(7, zh ? L"显示常用书签与链接栏" : L"Show bookmarks & links bar",
                            y7, 375, cfg.showToolbarLinks);
  y7 += lineH + spacing;

  s_hChkStatusBar = createChk(7, zh ? L"显示底部状态栏 (项目总数、占用大小等信息)" : L"Show status bar with item counts & size",
                             y7, 376, cfg.showStatusBar);

  // Initial tab page display
  SwitchTab(hwnd, 0);

  // OK & Cancel buttons
  int btnW = Theme::Get().Scale(96, hwnd);
  int btnH = Theme::Get().Scale(30, hwnd);
  int btnY = dlgH - Theme::Get().Scale(68, hwnd);

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

  // Page 1: Window & Split
  cfg.paneLayout = (int)SendMessageW(s_hLayoutCombo, CB_GETCURSEL, 0, 0);
  cfg.syncNav = (SendMessageW(s_hChkSyncNav, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.lockToolbars = (SendMessageW(s_hChkLockToolbars, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 2: Tabs
  cfg.allowDuplicateTabs = (SendMessageW(s_hChkAllowDupTabs, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.tabDoubleClickAction = (int)SendMessageW(s_hTabDblClickCombo, CB_GETCURSEL, 0, 0);

  // Page 3: Tree & Sidebar
  cfg.showTree = (SendMessageW(s_hChkTree, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.autoExpandTree = (SendMessageW(s_hChkAutoExpandTree, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.previewEnabled = (SendMessageW(s_hChkPreview, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 4: View & File List
  cfg.viewMode = (int)SendMessageW(s_hViewModeCombo, CB_GETCURSEL, 0, 0);
  cfg.showHidden = (SendMessageW(s_hChkHidden, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.calcFolderSize = (SendMessageW(s_hChkCalcSize, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 5: Search & Filter
  cfg.searchSubfoldersDefault = (SendMessageW(s_hChkSearchSubfolders, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 6: Gestures
  cfg.gesturesEnabled = (SendMessageW(s_hChkGestures, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.gestures = s_tempGestures;

  // Page 7: Appearance & Toolbars
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
  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->idFrom == 200 && pnm->code == TCN_SELCHANGE) {
      int cur = TabCtrl_GetCurSel(s_hTab);
      SwitchTab(hwnd, cur);
      return 0;
    } else if (pnm->idFrom == 362 && pnm->code == NM_DBLCLK) {
      SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(364, BN_CLICKED), (LPARAM)s_hBtnGestureEdit);
      return 0;
    }
    break;
  }
  case WM_COMMAND: {
    int id = LOWORD(wParam);
    if (id == 363) { // Add gesture
      HMENU hPop = CreatePopupMenu();
      struct Cand { const wchar_t* lbl; const char* c; };
      Cand cands[] = {
        { L"← 向左 (L)", "L" },
        { L"→ 向右 (R)", "R" },
        { L"↑ 向上 (U)", "U" },
        { L"↓ 向下 (D)", "D" },
        { L"↕ 上下 (UD)", "UD" },
        { L"↓↑ 下上 (DU)", "DU" },
        { L"↔ 左右 (LR)", "LR" },
        { L"←→ 右左 (RL)", "RL" },
        { L"↘ 下右 (DR)", "DR" },
        { L"↙ 下左 (DL)", "DL" },
        { L"↖ 上左 (UL)", "UL" },
        { L"↗ 上右 (UR)", "UR" },
        { L"←↓→ 左下右 (LDR)", "LDR" },
        { L"→↓← 右下左 (RDL)", "RDL" },
        { L"←↑→ 左上右 (LUR)", "LUR" },
        { L"→↑← 右上左 (RUL)", "RUL" }
      };
      int candCount = (int)(sizeof(cands) / sizeof(cands[0]));
      for (int i = 0; i < candCount; ++i) {
        AppendMenuW(hPop, MF_STRING, 5001 + i, cands[i].lbl);
      }
      POINT pt;
      GetCursorPos(&pt);
      int pick = TrackPopupMenu(hPop, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
      DestroyMenu(hPop);
      if (pick >= 5001 && pick < 5001 + candCount) {
        std::string code = cands[pick - 5001].c;
        bool exists = false;
        for (const auto& g : s_tempGestures) {
          if (g.code == code) { exists = true; break; }
        }
        if (!exists) {
          s_tempGestures.push_back({ code, 1, L"后退" });
          PopulateGestureList(s_hGestureList);
          int lastIdx = (int)s_tempGestures.size() - 1;
          ListView_SetItemState(s_hGestureList, lastIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
      }
      return 0;
    }
    if (id == 364) { // Edit gesture action
      int selIdx = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (selIdx >= 0 && selIdx < (int)s_tempGestures.size()) {
        HMENU hPop = CreatePopupMenu();
        bool zh = I18n::Get().IsChinese();

        auto addActionItem = [&](HMENU menu, GestureAction ga) {
          int actId = (int)ga;
          std::wstring aName = GestureEngine::GetActionName(ga);
          AppendMenuW(menu, MF_STRING, 6000 + actId, aName.c_str());
        };

        // Submenu: Navigation
        HMENU hNav = CreatePopupMenu();
        addActionItem(hNav, GestureAction::NavBack);
        addActionItem(hNav, GestureAction::NavForward);
        addActionItem(hNav, GestureAction::NavUp);
        addActionItem(hNav, GestureAction::Refresh);
        AppendMenuW(hPop, MF_POPUP, (UINT_PTR)hNav, zh ? L"导航与浏览" : L"Navigation");

        // Submenu: Tabs
        HMENU hTabs = CreatePopupMenu();
        addActionItem(hTabs, GestureAction::NewTab);
        addActionItem(hTabs, GestureAction::CloseTab);
        addActionItem(hTabs, GestureAction::CloseOtherTabs);
        addActionItem(hTabs, GestureAction::DuplicateTab);
        addActionItem(hTabs, GestureAction::ReopenClosedTab);
        addActionItem(hTabs, GestureAction::LockTab);
        addActionItem(hTabs, GestureAction::NextTab);
        addActionItem(hTabs, GestureAction::PrevTab);
        AppendMenuW(hPop, MF_POPUP, (UINT_PTR)hTabs, zh ? L"标签页管理" : L"Tabs");

        // Submenu: List & Scrolling
        HMENU hList = CreatePopupMenu();
        addActionItem(hList, GestureAction::ScrollTop);
        addActionItem(hList, GestureAction::ScrollBottom);
        addActionItem(hList, GestureAction::PageUp);
        addActionItem(hList, GestureAction::PageDown);
        AppendMenuW(hList, MF_SEPARATOR, 0, nullptr);
        addActionItem(hList, GestureAction::SelectAll);
        addActionItem(hList, GestureAction::InvertSelection);
        AppendMenuW(hPop, MF_POPUP, (UINT_PTR)hList, zh ? L"列表滚动与选择" : L"Scrolling & Selection");

        // Submenu: File Operations
        HMENU hFile = CreatePopupMenu();
        addActionItem(hFile, GestureAction::NewFolder);
        addActionItem(hFile, GestureAction::NewMD);
        AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
        addActionItem(hFile, GestureAction::Copy);
        addActionItem(hFile, GestureAction::Cut);
        addActionItem(hFile, GestureAction::Paste);
        addActionItem(hFile, GestureAction::Delete);
        addActionItem(hFile, GestureAction::Rename);
        addActionItem(hFile, GestureAction::CopyPath);
        addActionItem(hFile, GestureAction::Properties);
        AppendMenuW(hPop, MF_POPUP, (UINT_PTR)hFile, zh ? L"文件操作" : L"File Operations");

        // Submenu: View & Layout
        HMENU hView = CreatePopupMenu();
        addActionItem(hView, GestureAction::ToggleTree);
        addActionItem(hView, GestureAction::TogglePreview);
        addActionItem(hView, GestureAction::MaximizePane);
        addActionItem(hView, GestureAction::ClosePane);
        AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
        addActionItem(hView, GestureAction::LayoutSingle);
        addActionItem(hView, GestureAction::Layout2H);
        addActionItem(hView, GestureAction::Layout2V);
        addActionItem(hView, GestureAction::Layout4Grid);
        AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
        addActionItem(hView, GestureAction::ToggleHidden);
        AppendMenuW(hPop, MF_POPUP, (UINT_PTR)hView, zh ? L"视图与分栏" : L"View & Panes");

        // Submenu: Tools
        HMENU hTools = CreatePopupMenu();
        addActionItem(hTools, GestureAction::OpenTerminal);
        addActionItem(hTools, GestureAction::OpenSettings);
        AppendMenuW(hPop, MF_POPUP, (UINT_PTR)hTools, zh ? L"工具与选项" : L"Tools");

        POINT pt;
        GetCursorPos(&pt);
        int pick = TrackPopupMenu(hPop, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hPop);
        if (pick >= 6001 && pick <= 6038) {
          s_tempGestures[selIdx].action = pick - 6000;
          s_tempGestures[selIdx].name = GestureEngine::GetActionName((GestureAction)s_tempGestures[selIdx].action);
          PopulateGestureList(s_hGestureList);
          ListView_SetItemState(s_hGestureList, selIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
      }
      return 0;
    }
    if (id == 365) { // Delete gesture
      int selIdx = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (selIdx >= 0 && selIdx < (int)s_tempGestures.size()) {
        s_tempGestures.erase(s_tempGestures.begin() + selIdx);
        PopulateGestureList(s_hGestureList);
      }
      return 0;
    }
    if (id == 366) { // Reset gestures to default
      s_tempGestures = {
        { "L", 1, L"后退" },
        { "R", 2, L"前进" },
        { "U", 3, L"向上一级" },
        { "D", 4, L"关闭当前标签页" },
        { "UD", 6, L"刷新" },
        { "DR", 5, L"新建标签页" },
        { "UL", 7, L"复制当前标签页" },
        { "UR", 8, L"显示/隐藏侧边栏目录树" },
        { "LDR", 13, L"滚动到列表顶部" },
        { "RDL", 14, L"滚动到列表底部" },
        { "LUR", 15, L"向上翻页" },
        { "RUL", 16, L"向下翻页" }
      };
      PopulateGestureList(s_hGestureList);
      return 0;
    }
    if (id == IDOK) {
      SaveSettings(hwnd);
      DestroyWindow(hwnd);
      return 0;
    }
    if (id == IDCANCEL) {
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
