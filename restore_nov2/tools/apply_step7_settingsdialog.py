with open('src/ui/SettingsDialog.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

# 1. Add static HWND s_hChkSingleInstance
old_vars = """// Page 0: General
static HWND s_hLangCombo = nullptr;
static HWND s_hChkRestoreTabs = nullptr;
static HWND s_hChkMinToTray = nullptr;
static HWND s_hChkCloseToTray = nullptr;
static HWND s_hChkReplaceExplorer = nullptr;"""

new_vars = """// Page 0: General
static HWND s_hLangCombo = nullptr;
static HWND s_hChkRestoreTabs = nullptr;
static HWND s_hChkMinToTray = nullptr;
static HWND s_hChkCloseToTray = nullptr;
static HWND s_hChkSingleInstance = nullptr;
static HWND s_hChkReplaceExplorer = nullptr;"""

assert old_vars in text, "old_vars not found"
text = text.replace(old_vars, new_vars)

# 2. Add s_allActions entries
old_actions = """    { (int)GestureAction::SelectAll, L"全选", L"Select All" },
    { (int)GestureAction::NewFolder, L"新建文件夹", L"New Folder" },
    { (int)GestureAction::Properties, L"属性", L"Properties" }
};"""

new_actions = """    { (int)GestureAction::SelectAll, L"全选", L"Select All" },
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
    { (int)GestureAction::InvertSelection, L"反向选择", L"Invert Selection" }
};"""

assert old_actions in text, "old_actions not found"
text = text.replace(old_actions, new_actions)

# 3. Create s_hChkSingleInstance on Page 0
old_page0 = """  s_hChkCloseToTray = createChk(0, zh ? L"点击窗口关闭按钮时最小化至托盘" : L"Close button minimizes to tray",
                               y0, 304, cfg.closeToTray);
  y0 += lineH + spacing;

  s_hChkReplaceExplorer = createChk(0, zh ? L"替代系统资源管理器作为默认文件管理器" : L"Replace Windows Explorer as default file manager",
                                   y0, 305, ShellUtils::IsExplorerReplaced());"""

new_page0 = """  s_hChkCloseToTray = createChk(0, zh ? L"点击窗口关闭按钮时最小化至托盘" : L"Close button minimizes to tray",
                               y0, 304, cfg.closeToTray);
  y0 += lineH + spacing;

  s_hChkSingleInstance = createChk(0, zh ? L"只运行一个 NeoMDIE 程序实例 (单实例模式)" : L"Single instance mode (allow only one running instance)",
                                   y0, 306, cfg.singleInstance);
  y0 += lineH + spacing;

  s_hChkReplaceExplorer = createChk(0, zh ? L"替代系统资源管理器作为默认文件管理器" : L"Replace Windows Explorer as default file manager",
                                   y0, 305, ShellUtils::IsExplorerReplaced());"""

assert old_page0 in text, "old_page0 not found"
text = text.replace(old_page0, new_page0)

# 4. Save s_hChkSingleInstance in SaveSettings
old_save0 = """  cfg.restoreTabsOnStartup = (SendMessageW(s_hChkRestoreTabs, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.minimizeToTray = (SendMessageW(s_hChkMinToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.closeToTray = (SendMessageW(s_hChkCloseToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);

  bool replaceExp = (SendMessageW(s_hChkReplaceExplorer, BM_GETCHECK, 0, 0) == BST_CHECKED);"""

new_save0 = """  cfg.restoreTabsOnStartup = (SendMessageW(s_hChkRestoreTabs, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.minimizeToTray = (SendMessageW(s_hChkMinToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.closeToTray = (SendMessageW(s_hChkCloseToTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.singleInstance = (SendMessageW(s_hChkSingleInstance, BM_GETCHECK, 0, 0) == BST_CHECKED);

  bool replaceExp = (SendMessageW(s_hChkReplaceExplorer, BM_GETCHECK, 0, 0) == BST_CHECKED);"""

assert old_save0 in text, "old_save0 not found"
text = text.replace(old_save0, new_save0)

# 5. Rename "执行动作" label
old_lbl_action = """  createLabel(8, zh ? L"执行动作:" : L"Action:", marginX + Theme::Get().Scale(245, hwnd), gEditY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);"""
new_lbl_action = """  createLabel(8, zh ? L"执行功能:" : L"Action:", marginX + Theme::Get().Scale(245, hwnd), gEditY + Theme::Get().Scale(3, hwnd), Theme::Get().Scale(70, hwnd), lineH);"""

assert old_lbl_action in text, "old_lbl_action not found"
text = text.replace(old_lbl_action, new_lbl_action)

# 6. Fix listbox item draw (white text on white background)
old_draw201 = """    if (pdis && pdis->CtlID == 201) {
      bool isSelected = (pdis->itemState & ODS_SELECTED);
      HBRUSH hbg = isSelected ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface();
      FillRect(pdis->hDC, &pdis->rcItem, hbg);

      COLORREF textColor = isSelected ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary();
      SetTextColor(pdis->hDC, textColor);
      SetBkMode(pdis->hDC, TRANSPARENT);"""

new_draw201 = """    if (pdis && pdis->CtlID == 201) {
      bool isSelected = (pdis->itemState & ODS_SELECTED);
      bool isDark = Theme::Get().IsDarkMode();

      COLORREF bgClr = isSelected ? (isDark ? RGB(0, 90, 158) : RGB(0, 120, 215))
                                  : Theme::Get().ClrSurface();
      COLORREF textColor = isSelected ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary();

      HBRUSH hbg = CreateSolidBrush(bgClr);
      FillRect(pdis->hDC, &pdis->rcItem, hbg);
      DeleteObject(hbg);

      SetTextColor(pdis->hDC, textColor);
      SetBkMode(pdis->hDC, TRANSPARENT);"""

assert old_draw201 in text, "old_draw201 not found"
text = text.replace(old_draw201, new_draw201)

with open('src/ui/SettingsDialog.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("SettingsDialog.cpp updated successfully.")
