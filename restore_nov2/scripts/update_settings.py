import sys

with open("src/ui/SettingsDialog.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# 1. Update Page 0 to add s_hChkReplaceExplorer
old_page0_end = """  s_hChkCloseToTray = createChk(0, zh ? L"关闭窗口时最小化至托盘而非直接退出" : L"Close button minimizes to tray instead of exiting",
                               y0, 306, cfg.closeToTray);"""

new_page0_and_page1 = """  s_hChkCloseToTray = createChk(0, zh ? L"关闭窗口时最小化至托盘而非直接退出" : L"Close button minimizes to tray instead of exiting",
                               y0, 306, cfg.closeToTray);
  y0 += lineH + spacing * 2;

  createLabel(0, zh ? L"系统整合与关联:" : L"System Integration:", y0, true);
  y0 += lineH + Theme::Get().Scale(2, hwnd);

  s_hChkReplaceExplorer = createChk(0, zh ? L"替代系统资源管理器 (双击打开文件夹或驱动器时使用 NeoMDIE)" : L"Replace Windows Explorer (open folders & drives in NeoMDIE)",
                                    y0, 307, ShellUtils::IsExplorerReplaced());

  // =========================================================================
  // Page 1: 窗口动作 (Window Actions)
  // =========================================================================
  int yAct = topY;
  createLabel(1, zh ? L"侧边栏目录树打开目标:" : L"Directory Tree Action:", yAct, true);
  yAct += lineH + Theme::Get().Scale(2, hwnd);
  s_hTreeActionCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, yAct, Theme::Get().Scale(320, hwnd), lineH * 4,
      hwnd, (HMENU)318, hInst, nullptr);
  SendMessageW(s_hTreeActionCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前激活窗格中打开 (Active Pane)" : L"Open in Active Pane"));
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他分屏窗格中打开 (Other Split Pane)" : L"Open in Other Split Pane"));
  SendMessageW(s_hTreeActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"新建标签页打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hTreeActionCombo, CB_SETCURSEL, cfg.actionTreeOpen, 0);
  addCtrl(1, s_hTreeActionCombo);
  yAct += lineH + spacing * 2;

  createLabel(1, zh ? L"链接栏快捷方式打开目标:" : L"Links Bar Action:", yAct, true);
  yAct += lineH + Theme::Get().Scale(2, hwnd);
  s_hLinksActionCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, yAct, Theme::Get().Scale(320, hwnd), lineH * 4,
      hwnd, (HMENU)319, hInst, nullptr);
  SendMessageW(s_hLinksActionCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前激活窗格中打开 (Active Pane)" : L"Open in Active Pane"));
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他分屏窗格中打开 (Other Split Pane)" : L"Open in Other Split Pane"));
  SendMessageW(s_hLinksActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"新建标签页打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hLinksActionCombo, CB_SETCURSEL, cfg.actionLinksOpen, 0);
  addCtrl(1, s_hLinksActionCombo);
  yAct += lineH + spacing * 2;

  createLabel(1, zh ? L"地址栏回车与转到目标:" : L"Address Bar Go Action:", yAct, true);
  yAct += lineH + Theme::Get().Scale(2, hwnd);
  s_hAddressActionCombo = CreateWindowExW(
      0, L"COMBOBOX", L"",
      WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
      marginX, yAct, Theme::Get().Scale(320, hwnd), lineH * 4,
      hwnd, (HMENU)320, hInst, nullptr);
  SendMessageW(s_hAddressActionCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在当前激活窗格中打开 (Active Pane)" : L"Open in Active Pane"));
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"在对应/其他分屏窗格中打开 (Other Split Pane)" : L"Open in Other Split Pane"));
  SendMessageW(s_hAddressActionCombo, CB_ADDSTRING, 0, (LPARAM)(zh ? L"新建标签页打开 (New Tab)" : L"Open in New Tab"));
  SendMessageW(s_hAddressActionCombo, CB_SETCURSEL, cfg.actionAddressOpen, 0);
  addCtrl(1, s_hAddressActionCombo);"""

if old_page0_end in content:
    content = content.replace(old_page0_end, new_page0_and_page1, 1)
    print("Page 0 and Page 1 updated successfully.")
else:
    print("Error: old_page0_end not found!")
    sys.exit(1)

# 2. Shift Page 1..7 to Page 2..8
# Page 1: 窗口与分屏 -> Page 2
content = content.replace("// Page 1: 窗口与分屏", "// Page 2: 窗口与分屏")
content = content.replace("createLabel(1,", "createLabel(2,")
content = content.replace("createChk(1,", "createChk(2,")
content = content.replace("addCtrl(1, s_hLayoutCombo)", "addCtrl(2, s_hLayoutCombo)")

# Page 2: 标签页 -> Page 3
content = content.replace("// Page 2: 标签页", "// Page 3: 标签页")
content = content.replace("createLabel(2,", "createLabel(3,")
content = content.replace("createChk(2,", "createChk(3,")
content = content.replace("addCtrl(2, s_hTabDblClickCombo)", "addCtrl(3, s_hTabDblClickCombo)")

# Page 3: 目录树与侧边栏 -> Page 4
content = content.replace("// Page 3: 目录树与侧边栏", "// Page 4: 目录树与侧边栏")
content = content.replace("createLabel(3,", "createLabel(4,")
content = content.replace("createChk(3,", "createChk(4,")

# Page 4: 视图与文件列表 -> Page 5
content = content.replace("// Page 4: 视图与文件列表", "// Page 5: 视图与文件列表")
content = content.replace("createLabel(4,", "createLabel(5,")
content = content.replace("createChk(4,", "createChk(5,")
content = content.replace("addCtrl(4, s_hViewModeCombo)", "addCtrl(5, s_hViewModeCombo)")

# Page 5: 搜索与即时过滤 -> Page 6
content = content.replace("// Page 5: 搜索与即时过滤", "// Page 6: 搜索与即时过滤")
content = content.replace("createLabel(5,", "createLabel(6,")
content = content.replace("createChk(5,", "createChk(6,")

# Page 6: 鼠标手势 -> Page 7
content = content.replace("// Page 6: 鼠标手势", "// Page 7: 鼠标手势")
content = content.replace("createLabel(6,", "createLabel(7,")
content = content.replace("createChk(6,", "createChk(7,")
content = content.replace("addCtrl(6, s_hGestureList)", "addCtrl(7, s_hGestureList)")
content = content.replace("addCtrl(6, hBtn)", "addCtrl(7, hBtn)")

# Page 7: 外观与工具栏 -> Page 8
content = content.replace("// Page 7: 外观与工具栏", "// Page 8: 外观与工具栏")
content = content.replace("createLabel(7,", "createLabel(8,")
content = content.replace("createChk(7,", "createChk(8,")
content = content.replace("addCtrl(7, s_hThemeCombo)", "addCtrl(8, s_hThemeCombo)")

# 3. Update SaveSettings
old_save = """  // Page 0: General
  int langSel = (int)SendMessageW(s_hLangCombo, CB_GETCURSEL, 0, 0);
  Language newLang = (langSel == 1) ? Language::English : Language::Chinese;
  I18n::Get().SetLanguage(newLang);
  cfg.language = (int)newLang;

  cfg.restoreTabsOnStartup = (SendMessageW(s_hChkRestoreTabs, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.minimizeToTray = (SendMessageW(s_hChkMinToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.closeToTray = (SendMessageW(s_hChkCloseToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Page 1: Window & Split"""

new_save = """  // Page 0: General
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

  // Page 2: Window & Split"""

if old_save in content:
    content = content.replace(old_save, new_save, 1)
    print("SaveSettings updated successfully.")
else:
    print("Error: old_save not found!")
    sys.exit(1)

with open("src/ui/SettingsDialog.cpp", "w", encoding="utf-8") as f:
    f.write(content)

print("All SettingsDialog modifications applied!")
