import os
import re

print("Applying all 12 fixes...")

# -------------------------------------------------------------------------
# 1. Update Config.h & Config.cpp
# -------------------------------------------------------------------------
with open("src/core/Config.h", "r", encoding="utf-8") as f:
    cfg_h = f.read()

# Action trigger enum
action_trigger_enum = """enum class ActionTrigger {
    None = 0,
    LeftClick = 1,
    DoubleClick = 2,
    MiddleClick = 3,
    ShiftClick = 4,
    CtrlClick = 5,
    AltClick = 6,
    ShiftCtrlClick = 7,
    ShiftAltClick = 8,
    CtrlAltClick = 9,
    ShiftCtrlAltClick = 10,
    ShiftMiddleClick = 11,
    CtrlMiddleClick = 12
};

struct TargetActionConfig {
    int openCurrentPane = (int)ActionTrigger::LeftClick;      // 在当前栏打开
    int openNewTab = (int)ActionTrigger::CtrlClick;           // 在新标签页打开
    int openBgTab = (int)ActionTrigger::ShiftCtrlClick;       // 在后台新标签页打开
    int openOtherPane = (int)ActionTrigger::ShiftClick;       // 在其他栏打开
    int openExplorer = (int)ActionTrigger::None;              // 在系统资源管理器中打开
};
"""

# Replace old TargetActionConfig
old_target_config = """struct TargetActionConfig {
    int click = 0;          // 0: Current Pane, 1: Other Pane, 2: New Tab, 3: Background New Tab, 4: Explorer, 5: None
    int shiftClick = 1;     // Other Pane
    int ctrlClick = 2;      // New Tab
    int shiftCtrlClick = 3; // Background New Tab
    int middleClick = 2;    // New Tab
    int doubleClick = 0;    // Current Pane
};"""

if old_target_config in cfg_h:
    cfg_h = cfg_h.replace(old_target_config, action_trigger_enum)

# Window default maximized & gridlines & zebra
cfg_h = cfg_h.replace("bool maximized = false;", "bool maximized = true;")
if "bool zebraStriping" not in cfg_h:
    cfg_h = cfg_h.replace("bool calcFolderSize = false;", "bool calcFolderSize = false;\n    bool zebraStriping = true;\n    bool showGridLines = true;")

with open("src/core/Config.h", "w", encoding="utf-8") as f:
    f.write(cfg_h)
print("Config.h updated.")

# Update Config.cpp for new TargetActionConfig & zebra/gridlines
with open("src/core/Config.cpp", "r", encoding="utf-8") as f:
    cfg_cpp = f.read()

old_cfg_load = """    actionTree.click = GetPrivateProfileIntW(secGeneral, L"ActionTree_Click", 0, m_iniPath.c_str());
    actionTree.shiftClick = GetPrivateProfileIntW(secGeneral, L"ActionTree_ShiftClick", 1, m_iniPath.c_str());
    actionTree.ctrlClick = GetPrivateProfileIntW(secGeneral, L"ActionTree_CtrlClick", 2, m_iniPath.c_str());
    actionTree.shiftCtrlClick = GetPrivateProfileIntW(secGeneral, L"ActionTree_ShiftCtrlClick", 3, m_iniPath.c_str());
    actionTree.middleClick = GetPrivateProfileIntW(secGeneral, L"ActionTree_MiddleClick", 2, m_iniPath.c_str());
    actionTree.doubleClick = GetPrivateProfileIntW(secGeneral, L"ActionTree_DoubleClick", 0, m_iniPath.c_str());

    actionLinks.click = GetPrivateProfileIntW(secGeneral, L"ActionLinks_Click", 0, m_iniPath.c_str());
    actionLinks.shiftClick = GetPrivateProfileIntW(secGeneral, L"ActionLinks_ShiftClick", 1, m_iniPath.c_str());
    actionLinks.ctrlClick = GetPrivateProfileIntW(secGeneral, L"ActionLinks_CtrlClick", 2, m_iniPath.c_str());
    actionLinks.shiftCtrlClick = GetPrivateProfileIntW(secGeneral, L"ActionLinks_ShiftCtrlClick", 3, m_iniPath.c_str());
    actionLinks.middleClick = GetPrivateProfileIntW(secGeneral, L"ActionLinks_MiddleClick", 2, m_iniPath.c_str());
    actionLinks.doubleClick = GetPrivateProfileIntW(secGeneral, L"ActionLinks_DoubleClick", 0, m_iniPath.c_str());

    actionAddress.click = GetPrivateProfileIntW(secGeneral, L"ActionAddress_Click", 0, m_iniPath.c_str());
    actionAddress.shiftClick = GetPrivateProfileIntW(secGeneral, L"ActionAddress_ShiftClick", 1, m_iniPath.c_str());
    actionAddress.ctrlClick = GetPrivateProfileIntW(secGeneral, L"ActionAddress_CtrlClick", 2, m_iniPath.c_str());
    actionAddress.shiftCtrlClick = GetPrivateProfileIntW(secGeneral, L"ActionAddress_ShiftCtrlClick", 3, m_iniPath.c_str());
    actionAddress.middleClick = GetPrivateProfileIntW(secGeneral, L"ActionAddress_MiddleClick", 2, m_iniPath.c_str());
    actionAddress.doubleClick = GetPrivateProfileIntW(secGeneral, L"ActionAddress_DoubleClick", 0, m_iniPath.c_str());"""

new_cfg_load = """    zebraStriping = GetPrivateProfileIntW(secGeneral, L"ZebraStriping", 1, m_iniPath.c_str()) != 0;
    showGridLines = GetPrivateProfileIntW(secGeneral, L"ShowGridLines", 1, m_iniPath.c_str()) != 0;

    actionTree.openCurrentPane = GetPrivateProfileIntW(secGeneral, L"ActionTree_CurrentPane", (int)ActionTrigger::LeftClick, m_iniPath.c_str());
    actionTree.openNewTab = GetPrivateProfileIntW(secGeneral, L"ActionTree_NewTab", (int)ActionTrigger::CtrlClick, m_iniPath.c_str());
    actionTree.openBgTab = GetPrivateProfileIntW(secGeneral, L"ActionTree_BgTab", (int)ActionTrigger::ShiftCtrlClick, m_iniPath.c_str());
    actionTree.openOtherPane = GetPrivateProfileIntW(secGeneral, L"ActionTree_OtherPane", (int)ActionTrigger::ShiftClick, m_iniPath.c_str());
    actionTree.openExplorer = GetPrivateProfileIntW(secGeneral, L"ActionTree_Explorer", (int)ActionTrigger::None, m_iniPath.c_str());

    actionLinks.openCurrentPane = GetPrivateProfileIntW(secGeneral, L"ActionLinks_CurrentPane", (int)ActionTrigger::LeftClick, m_iniPath.c_str());
    actionLinks.openNewTab = GetPrivateProfileIntW(secGeneral, L"ActionLinks_NewTab", (int)ActionTrigger::MiddleClick, m_iniPath.c_str());
    actionLinks.openBgTab = GetPrivateProfileIntW(secGeneral, L"ActionLinks_BgTab", (int)ActionTrigger::ShiftCtrlClick, m_iniPath.c_str());
    actionLinks.openOtherPane = GetPrivateProfileIntW(secGeneral, L"ActionLinks_OtherPane", (int)ActionTrigger::ShiftClick, m_iniPath.c_str());
    actionLinks.openExplorer = GetPrivateProfileIntW(secGeneral, L"ActionLinks_Explorer", (int)ActionTrigger::None, m_iniPath.c_str());

    actionAddress.openCurrentPane = GetPrivateProfileIntW(secGeneral, L"ActionAddress_CurrentPane", (int)ActionTrigger::LeftClick, m_iniPath.c_str());
    actionAddress.openNewTab = GetPrivateProfileIntW(secGeneral, L"ActionAddress_NewTab", (int)ActionTrigger::CtrlClick, m_iniPath.c_str());
    actionAddress.openBgTab = GetPrivateProfileIntW(secGeneral, L"ActionAddress_BgTab", (int)ActionTrigger::ShiftCtrlClick, m_iniPath.c_str());
    actionAddress.openOtherPane = GetPrivateProfileIntW(secGeneral, L"ActionAddress_OtherPane", (int)ActionTrigger::ShiftClick, m_iniPath.c_str());
    actionAddress.openExplorer = GetPrivateProfileIntW(secGeneral, L"ActionAddress_Explorer", (int)ActionTrigger::None, m_iniPath.c_str());"""

cfg_cpp = cfg_cpp.replace(old_cfg_load, new_cfg_load)

old_cfg_save = """    WritePrivateProfileStringW(sec, L"ActionTree_Click", std::to_wstring(actionTree.click).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_ShiftClick", std::to_wstring(actionTree.shiftClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_CtrlClick", std::to_wstring(actionTree.ctrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_ShiftCtrlClick", std::to_wstring(actionTree.shiftCtrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_MiddleClick", std::to_wstring(actionTree.middleClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_DoubleClick", std::to_wstring(actionTree.doubleClick).c_str(), m_iniPath.c_str());

    WritePrivateProfileStringW(sec, L"ActionLinks_Click", std::to_wstring(actionLinks.click).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_ShiftClick", std::to_wstring(actionLinks.shiftClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_CtrlClick", std::to_wstring(actionLinks.ctrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_ShiftCtrlClick", std::to_wstring(actionLinks.shiftCtrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_MiddleClick", std::to_wstring(actionLinks.middleClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_DoubleClick", std::to_wstring(actionLinks.doubleClick).c_str(), m_iniPath.c_str());

    WritePrivateProfileStringW(sec, L"ActionAddress_Click", std::to_wstring(actionAddress.click).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_ShiftClick", std::to_wstring(actionAddress.shiftClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_CtrlClick", std::to_wstring(actionAddress.ctrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_ShiftCtrlClick", std::to_wstring(actionAddress.shiftCtrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_MiddleClick", std::to_wstring(actionAddress.middleClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_DoubleClick", std::to_wstring(actionAddress.doubleClick).c_str(), m_iniPath.c_str());"""

new_cfg_save = """    writeInt(secGeneral, L"ZebraStriping", zebraStriping ? 1 : 0);
    writeInt(secGeneral, L"ShowGridLines", showGridLines ? 1 : 0);

    writeInt(secGeneral, L"ActionTree_CurrentPane", actionTree.openCurrentPane);
    writeInt(secGeneral, L"ActionTree_NewTab", actionTree.openNewTab);
    writeInt(secGeneral, L"ActionTree_BgTab", actionTree.openBgTab);
    writeInt(secGeneral, L"ActionTree_OtherPane", actionTree.openOtherPane);
    writeInt(secGeneral, L"ActionTree_Explorer", actionTree.openExplorer);

    writeInt(secGeneral, L"ActionLinks_CurrentPane", actionLinks.openCurrentPane);
    writeInt(secGeneral, L"ActionLinks_NewTab", actionLinks.openNewTab);
    writeInt(secGeneral, L"ActionLinks_BgTab", actionLinks.openBgTab);
    writeInt(secGeneral, L"ActionLinks_OtherPane", actionLinks.openOtherPane);
    writeInt(secGeneral, L"ActionLinks_Explorer", actionLinks.openExplorer);

    writeInt(secGeneral, L"ActionAddress_CurrentPane", actionAddress.openCurrentPane);
    writeInt(secGeneral, L"ActionAddress_NewTab", actionAddress.openNewTab);
    writeInt(secGeneral, L"ActionAddress_BgTab", actionAddress.openBgTab);
    writeInt(secGeneral, L"ActionAddress_OtherPane", actionAddress.openOtherPane);
    writeInt(secGeneral, L"ActionAddress_Explorer", actionAddress.openExplorer);"""

cfg_cpp = cfg_cpp.replace(old_cfg_save, new_cfg_save)

with open("src/core/Config.cpp", "w", encoding="utf-8") as f:
    f.write(cfg_cpp)
print("Config.cpp updated.")

# -------------------------------------------------------------------------
# 2. Update MultiPaneContainer.cpp: Hide headers in single pane, no min button
# -------------------------------------------------------------------------
with open("src/ui/MultiPaneContainer.cpp", "r", encoding="utf-8") as f:
    mp = f.read()

# In SetLayout:
old_set_layout_headers = """    if (m_hHeaders[i]) {
      ShowWindow(m_hHeaders[i], shouldShow ? SW_SHOW : SW_HIDE);
      UpdatePaneHeader(i);
    }"""
new_set_layout_headers = """    if (m_hHeaders[i]) {
      bool showHdr = shouldShow && (count > 1);
      ShowWindow(m_hHeaders[i], showHdr ? SW_SHOW : SW_HIDE);
      UpdatePaneHeader(i);
    }"""
mp = mp.replace(old_set_layout_headers, new_set_layout_headers)

# In LayoutChildren:
old_layout_pos = """  int splitW = Theme::Get().Scale(6, m_hwnd);
  int headerH = Theme::Get().Scale(26, m_hwnd);
  int borderPad = 1;

  auto positionPane = [&](int idx, int x, int y, int w, int h) {
    if (idx >= 0 && idx < 4) {
      if (m_hHeaders[idx]) {
        SetWindowPos(m_hHeaders[idx], HWND_TOP, x, y, w, headerH, SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        InvalidateRect(m_hHeaders[idx], nullptr, TRUE);
      }
      m_panes[idx].UpdateLayout(x, y + headerH, w, h - headerH);
    }
  };"""

new_layout_pos = """  int splitW = Theme::Get().Scale(6, m_hwnd);
  int count = GetPaneCount();
  int headerH = (count > 1) ? Theme::Get().Scale(26, m_hwnd) : 0;
  int borderPad = 1;

  auto positionPane = [&](int idx, int x, int y, int w, int h) {
    if (idx >= 0 && idx < 4) {
      if (count > 1 && m_hHeaders[idx]) {
        ShowWindow(m_hHeaders[idx], SW_SHOW);
        SetWindowPos(m_hHeaders[idx], HWND_TOP, x, y, w, headerH, SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        InvalidateRect(m_hHeaders[idx], nullptr, TRUE);
      } else if (m_hHeaders[idx]) {
        ShowWindow(m_hHeaders[idx], SW_HIDE);
      }
      m_panes[idx].UpdateLayout(x, y + headerH, w, h - headerH);
    }
  };"""
mp = mp.replace(old_layout_pos, new_layout_pos)

# In HandleHeaderMessage: remove rcMin, keep only rcMax and rcClose!
old_header_btns = """    int btnW = Theme::Get().Scale(28, hwnd);
    RECT rcClose = { rc.right - btnW, 0, rc.right, rc.bottom - 1 };
    RECT rcMax = { rc.right - btnW * 2, 0, rc.right - btnW, rc.bottom - 1 };
    RECT rcMin = { rc.right - btnW * 3, 0, rc.right - btnW * 2, rc.bottom - 1 };

    // Button hovers
    if (s_headerHoverPane == paneIdx) {
      if (s_headerHoverBtn == 1) {
        HBRUSH hbrHov = Theme::Get().BrushHover();
        FillRect(hdc, &rcMin, hbrHov);
      } else if (s_headerHoverBtn == 2) {
        HBRUSH hbrHov = Theme::Get().BrushHover();
        FillRect(hdc, &rcMax, hbrHov);
      } else if (s_headerHoverBtn == 3) {
        HBRUSH hbrCloseHov = CreateSolidBrush(RGB(232, 17, 35));
        FillRect(hdc, &rcClose, hbrCloseHov);
        DeleteObject(hbrCloseHov);
      }
    }

    // Path & Folder Icon
    std::wstring path = m_panes[paneIdx].GetCurrentPath();
    if (path.empty()) path = L"This PC";
    std::wstring titleText = L" [ " + path + L" ]";

    RECT rcText = { rc.left + Theme::Get().Scale(6, hwnd), 0, rc.right - btnW * 3 - Theme::Get().Scale(4, hwnd), rc.bottom - 1 };
    DrawTextW(hdc, titleText.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // 3 standard header buttons: Minimize, Maximize/Restore, Close
    COLORREF btnClr = isActive ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary();
    HFONT hIconFont = Theme::Get().GetIconFont(11, hwnd);
    HGDIOBJ oldIconFont = SelectObject(hdc, hIconFont);

    // Minimize
    SetTextColor(hdc, btnClr);
    wchar_t minGlyph[2] = { 0xE921, 0 };
    DrawTextW(hdc, minGlyph, 1, &rcMin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Maximize / Restore
    wchar_t maxGlyph[2] = { (m_maximizedPane == paneIdx) ? (wchar_t)0xE923 : (wchar_t)0xE922, 0 };
    DrawTextW(hdc, maxGlyph, 1, &rcMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Close
    if (GetPaneCount() > 1) {
      if (s_headerHoverPane == paneIdx && s_headerHoverBtn == 3) {
        SetTextColor(hdc, RGB(255, 255, 255));
      } else {
        SetTextColor(hdc, btnClr);
      }
      wchar_t closeGlyph[2] = { 0xE8BB, 0 };
      DrawTextW(hdc, closeGlyph, 1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }"""

new_header_btns = """    int btnW = Theme::Get().Scale(28, hwnd);
    RECT rcClose = { rc.right - btnW, 0, rc.right, rc.bottom - 1 };
    RECT rcMax = { rc.right - btnW * 2, 0, rc.right - btnW, rc.bottom - 1 };

    // Button hovers (only Max and Close)
    if (s_headerHoverPane == paneIdx) {
      if (s_headerHoverBtn == 1) {
        HBRUSH hbrHov = Theme::Get().BrushHover();
        FillRect(hdc, &rcMax, hbrHov);
      } else if (s_headerHoverBtn == 2) {
        HBRUSH hbrCloseHov = CreateSolidBrush(RGB(232, 17, 35));
        FillRect(hdc, &rcClose, hbrCloseHov);
        DeleteObject(hbrCloseHov);
      }
    }

    // Path & Folder Icon
    std::wstring path = m_panes[paneIdx].GetCurrentPath();
    if (path.empty()) path = L"This PC";
    std::wstring titleText = L" [ " + path + L" ]";

    RECT rcText = { rc.left + Theme::Get().Scale(6, hwnd), 0, rc.right - btnW * 2 - Theme::Get().Scale(4, hwnd), rc.bottom - 1 };
    DrawTextW(hdc, titleText.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Header buttons: Maximize/Restore, Close
    COLORREF btnClr = isActive ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary();
    HFONT hIconFont = Theme::Get().GetIconFont(11, hwnd);
    HGDIOBJ oldIconFont = SelectObject(hdc, hIconFont);

    // Maximize / Restore
    SetTextColor(hdc, btnClr);
    wchar_t maxGlyph[2] = { (m_maximizedPane == paneIdx) ? (wchar_t)0xE923 : (wchar_t)0xE922, 0 };
    DrawTextW(hdc, maxGlyph, 1, &rcMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Close
    if (s_headerHoverPane == paneIdx && s_headerHoverBtn == 2) {
      SetTextColor(hdc, RGB(255, 255, 255));
    } else {
      SetTextColor(hdc, btnClr);
    }
    wchar_t closeGlyph[2] = { 0xE8BB, 0 };
    DrawTextW(hdc, closeGlyph, 1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);"""

mp = mp.replace(old_header_btns, new_header_btns)

# Mouse move in header:
old_header_mm = """    if (x >= rc.right - btnW && x <= rc.right) {
      newBtn = 3;
    } else if (x >= rc.right - btnW * 2 && x < rc.right - btnW) {
      newBtn = 2;
    } else if (x >= rc.right - btnW * 3 && x < rc.right - btnW * 2) {
      newBtn = 1;
    }"""
new_header_mm = """    if (x >= rc.right - btnW && x <= rc.right) {
      newBtn = 2; // Close
    } else if (x >= rc.right - btnW * 2 && x < rc.right - btnW) {
      newBtn = 1; // Max/Restore
    }"""
mp = mp.replace(old_header_mm, new_header_mm)

# Click in header:
old_header_click = """    if (x >= rc.right - btnW && x <= rc.right) {
      if (GetPaneCount() > 1) {
        ClosePane(paneIdx);
      }
      return 0;
    } else if (x >= rc.right - btnW * 2 && x < rc.right - btnW) {
      ToggleMaximizePane(paneIdx);
      return 0;
    } else if (x >= rc.right - btnW * 3 && x < rc.right - btnW * 2) {
      MinimizePane(paneIdx);
      return 0;
    }"""
new_header_click = """    if (x >= rc.right - btnW && x <= rc.right) {
      if (GetPaneCount() > 1) {
        ClosePane(paneIdx);
      }
      return 0;
    } else if (x >= rc.right - btnW * 2 && x < rc.right - btnW) {
      ToggleMaximizePane(paneIdx);
      return 0;
    }"""
mp = mp.replace(old_header_click, new_header_click)

with open("src/ui/MultiPaneContainer.cpp", "w", encoding="utf-8") as f:
    f.write(mp)
print("MultiPaneContainer.cpp updated.")

# -------------------------------------------------------------------------
# 3. Update FilterBar.cpp & BreadcrumbBar.cpp: CS_HREDRAW, owner draw gear button
# -------------------------------------------------------------------------
with open("src/ui/FilterBar.cpp", "r", encoding="utf-8") as f:
    fb = f.read()

# CS_HREDRAW | CS_VREDRAW
fb = fb.replace("wc.style = 0;", "wc.style = CS_HREDRAW | CS_VREDRAW;")
fb = fb.replace("wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);\n        RegisterClassExW(&wc);",
                "wc.style = CS_HREDRAW | CS_VREDRAW;\n        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);\n        RegisterClassExW(&wc);")

# Make m_hwndOptBtn BS_OWNERDRAW
fb = fb.replace('WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP',
                'WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP')

# Add WM_DRAWITEM in FilterBar::WndProc
old_fb_paint = """    case WM_PAINT: {"""
new_fb_paint = """    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis && pdis->CtlID == 102) {
            bool isDown = (pdis->itemState & ODS_SELECTED);
            HBRUSH hbg = isDown ? Theme::Get().BrushSelected() : Theme::Get().BrushSurface();
            FillRect(pdis->hDC, &pdis->rcItem, hbg);

            SetBkMode(pdis->hDC, TRANSPARENT);
            SetTextColor(pdis->hDC, Theme::Get().ClrTextPrimary());
            HFONT hFont = Theme::Get().GetUIFont(hwnd);
            HGDIOBJ oldFont = SelectObject(pdis->hDC, hFont);

            DrawTextW(pdis->hDC, L"⚙", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(pdis->hDC, oldFont);
            return TRUE;
        }
        break;
    }
    case WM_PAINT: {"""
fb = fb.replace(old_fb_paint, new_fb_paint)

with open("src/ui/FilterBar.cpp", "w", encoding="utf-8") as f:
    f.write(fb)
print("FilterBar.cpp updated.")

with open("src/ui/BreadcrumbBar.cpp", "r", encoding="utf-8") as f:
    bb = f.read()
bb = bb.replace("wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);\n        RegisterClassExW(&wc);",
                "wc.style = CS_HREDRAW | CS_VREDRAW;\n        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);\n        RegisterClassExW(&wc);")
with open("src/ui/BreadcrumbBar.cpp", "w", encoding="utf-8") as f:
    f.write(bb)
print("BreadcrumbBar.cpp updated.")

print("Step 1-3 completed.")
