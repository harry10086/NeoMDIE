import re

with open('src/ui/SettingsDialog.cpp', 'r', encoding='utf-8') as f:
    sd = f.read()

# 1. Dialog size increase to avoid button clipping
sd = sd.replace('int w = Theme::Get().Scale(820, hwndParent);', 'int w = Theme::Get().Scale(860, hwndParent);')
sd = sd.replace('int h = Theme::Get().Scale(630, hwndParent);', 'int h = Theme::Get().Scale(680, hwndParent);')

# 2. Add singleInstance checkbox handle
si_handle_target = 'static HWND s_hChkAutoExpand = nullptr;'
si_handle_repl = '''static HWND s_hChkAutoExpand = nullptr;
static HWND s_hChkSingleInstance = nullptr;'''
sd = sd.replace(si_handle_target, si_handle_repl, 1)

# 3. Add singleInstance checkbox to page 0
si_chk_target = '''  s_hChkSubfolders = createChk(0, zh ? L"搜索过滤默认包含子文件夹 (递归搜索)" : L"Search & Filter includes subfolders by default",
                              y0, 313, cfg.searchSubfoldersDefault);
  y0 += lineH + spacing;'''

si_chk_repl = '''  s_hChkSubfolders = createChk(0, zh ? L"搜索过滤默认包含子文件夹 (递归搜索)" : L"Search & Filter includes subfolders by default",
                              y0, 313, cfg.searchSubfoldersDefault);
  y0 += lineH + spacing;

  s_hChkSingleInstance = createChk(0, zh ? L"单实例模式 (只允许运行一个 NeoMDIE 进程，打开新路径时在当前窗口新标签打开)" : L"Single Instance Mode (Only run one NeoMDIE process, open paths in new tabs)",
                                   y0, 314, cfg.singleInstance);
  y0 += lineH + spacing;'''
sd = sd.replace(si_chk_target, si_chk_repl, 1)

# 4. Save singleInstance
si_save_target = 'cfg.searchSubfoldersDefault = (SendMessageW(s_hChkSubfolders, BM_GETCHECK, 0, 0) == BST_CHECKED);'
si_save_repl = '''cfg.searchSubfoldersDefault = (SendMessageW(s_hChkSubfolders, BM_GETCHECK, 0, 0) == BST_CHECKED);
  cfg.singleInstance = (SendMessageW(s_hChkSingleInstance, BM_GETCHECK, 0, 0) == BST_CHECKED);'''
sd = sd.replace(si_save_target, si_save_repl, 1)

# 5. Add new gesture patterns to s_allPatterns
pat_target = '    { "RD", L"右下", L"Right-Down" }'
pat_repl = '''    { "RD", L"右下", L"Right-Down" },
    { "LDR", L"←↓→ (左下右)", L"←↓→ (Left-Down-Right)" },
    { "RDL", L"→↓← (右下左)", L"→↓← (Right-Down-Left)" },
    { "LUR", L"←↑→ (左上右)", L"←↑→ (Left-Up-Right)" },
    { "RUL", L"→↑← (右上左)", L"→↑← (Right-Up-Left)" }'''
sd = sd.replace(pat_target, pat_repl, 1)

# 6. Change Gesture column header from 执行动作 to 执行功能
sd = sd.replace('lvcG.pszText = (LPWSTR)(zh ? L"执行动作" : L"Action");',
                'lvcG.pszText = (LPWSTR)(zh ? L"执行功能" : L"Action");')

# 7. ApplyToWindow in createChk and createCombo
old_create_chk = '''    SendMessageW(hChk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTheme(hChk, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    return addCtrl(page, hChk);'''

new_create_chk = '''    SendMessageW(hChk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTheme(hChk, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    Theme::Get().ApplyToWindow(hChk);
    return addCtrl(page, hChk);'''
sd = sd.replace(old_create_chk, new_create_chk, 1)

old_create_cmb = '''    SendMessageW(hCmb, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowTheme(hCmb, isDark ? L"DarkMode_CFD" : L"Explorer", nullptr);
    return addCtrl(page, hCmb);'''

new_create_cmb = '''    SendMessageW(hCmb, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowTheme(hCmb, isDark ? L"DarkMode_CFD" : L"Explorer", nullptr);
    Theme::Get().ApplyToWindow(hCmb);
    return addCtrl(page, hCmb);'''
sd = sd.replace(old_create_cmb, new_create_cmb, 1)

# 8. Use BS_OWNERDRAW for action buttons
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                               btnX, editY, btnW, btnH, hwnd, (HMENU)356, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                               btnX, editY, btnW, btnH, hwnd, (HMENU)356, hInst, nullptr);')
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                               btnX, editY, btnW, btnH, hwnd, (HMENU)357, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                               btnX, editY, btnW, btnH, hwnd, (HMENU)357, hInst, nullptr);')
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                               btnX, editY, btnW, btnH, hwnd, (HMENU)358, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                               btnX, editY, btnW, btnH, hwnd, (HMENU)358, hInst, nullptr);')
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                                 btnX, editY, btnW + Theme::Get().Scale(16, hwnd), btnH, hwnd, (HMENU)359, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                                 btnX, editY, btnW + Theme::Get().Scale(16, hwnd), btnH, hwnd, (HMENU)359, hInst, nullptr);')

# Gesture buttons
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                               gBtnX, gEditY, btnW, btnH, hwnd, (HMENU)362, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                               gBtnX, gEditY, btnW, btnH, hwnd, (HMENU)362, hInst, nullptr);')
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                               gBtnX, gEditY, btnW, btnH, hwnd, (HMENU)363, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                               gBtnX, gEditY, btnW, btnH, hwnd, (HMENU)363, hInst, nullptr);')
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                               gBtnX, gEditY, btnW, btnH, hwnd, (HMENU)364, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                               gBtnX, gEditY, btnW, btnH, hwnd, (HMENU)364, hInst, nullptr);')
sd = sd.replace('WS_CHILD | WS_VISIBLE | WS_TABSTOP,\r\n                                 gBtnX, gEditY, btnW + Theme::Get().Scale(16, hwnd), btnH, hwnd, (HMENU)365, hInst, nullptr);',
                'WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,\r\n                                 gBtnX, gEditY, btnW + Theme::Get().Scale(16, hwnd), btnH, hwnd, (HMENU)365, hInst, nullptr);')

# Dialog bottom buttons
sd = sd.replace('WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,',
                'WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,')
sd = sd.replace('WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,',
                'WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,')

# 9. WM_DRAWITEM button handling
old_drawitem = '''  case WM_DRAWITEM: {
    DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
    if (pdis && pdis->CtlID == 201) {'''

new_drawitem = '''  case WM_DRAWITEM: {
    DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
    if (pdis && pdis->CtlType == ODT_BUTTON) {
      bool isDown = (pdis->itemState & ODS_SELECTED) != 0;
      bool isDark = Theme::Get().IsDarkMode();
      HBRUSH hBg = isDark ? (isDown ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface())
                          : (isDown ? Theme::Get().BrushSelected() : CreateSolidBrush(RGB(240, 242, 245)));
      FillRect(pdis->hDC, &pdis->rcItem, hBg);

      HPEN hPen = isDark ? Theme::Get().PenBorder() : CreatePen(PS_SOLID, 1, RGB(180, 185, 195));
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
    if (pdis && pdis->CtlID == 201) {'''
sd = sd.replace(old_drawitem, new_drawitem, 1)

# 10. WM_NOTIFY Header CustomDraw
old_notify = '''  case WM_NOTIFY: {
    NMHDR* pnm = (NMHDR*)lParam;
    if (pnm->hwndFrom == s_hShortcutList && pnm->code == LVN_ITEMCHANGED) {'''

new_notify = '''  case WM_NOTIFY: {
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
    if (pnm->hwndFrom == s_hShortcutList && pnm->code == LVN_ITEMCHANGED) {'''
sd = sd.replace(old_notify, new_notify, 1)

with open('src/ui/SettingsDialog.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(sd)
print('SettingsDialog.cpp updated')
