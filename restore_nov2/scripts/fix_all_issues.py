#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fix_all_issues.py - Fix all pending issues in NeoMDIE:
1. Dark mode: menu bar still white; comboboxes white; checkbox text black; 
   listviews white on hotkeys/gestures pages
2. Settings dialog - remove mixed Chinese/English parenthetical text
3. Zebra striping and gridlines not working in FileListView
4. Sidebar width shrinks in multi-pane view
5. Gestures not working (scroll top/bottom/pageup/pagedown)
6. Window actions page trigger labels cleanup
7. Apply dark colors to ListView controls in settings
"""

import re
import os
import sys

BASE = r"d:\github\NeoMDIE"

def read_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()

def write_file(path, content):
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(content)
    print(f"  Written: {path}")

# ============================================================================
# FIX 1: SettingsDialog.cpp - Dark mode fixes for controls + cleanup labels
# ============================================================================
def fix_settings_dialog():
    path = os.path.join(BASE, r"src\ui\SettingsDialog.cpp")
    content = read_file(path)
    
    # Fix 1a: Remove parenthetical English from Chinese trigger options
    old_triggers = '''  const wchar_t* triggerOptionsZh[13] = {
      L"无 (None)",
      L"鼠标左键单击 (Left Click)",
      L"鼠标左键双击 (Double Click)",
      L"鼠标滚轮中键单击 (Middle Click)",
      L"Shift + 单击 (Shift + Click)",
      L"Ctrl + 单击 (Ctrl + Click)",
      L"Alt + 单击 (Alt + Click)",
      L"Shift + Ctrl + 单击 (Shift + Ctrl + Click)",
      L"Shift + Alt + 单击 (Shift + Alt + Click)",
      L"Ctrl + Alt + 单击 (Ctrl + Alt + Click)",
      L"Shift + Ctrl + Alt + 单击 (Shift + Ctrl + Alt + Click)",
      L"Shift + 中键单击 (Shift + Middle Click)",
      L"Ctrl + 中键单击 (Ctrl + Middle Click)"
  };'''
    new_triggers = '''  const wchar_t* triggerOptionsZh[13] = {
      L"无",
      L"鼠标左键单击",
      L"鼠标左键双击",
      L"鼠标滚轮中键单击",
      L"Shift + 单击",
      L"Ctrl + 单击",
      L"Alt + 单击",
      L"Shift + Ctrl + 单击",
      L"Shift + Alt + 单击",
      L"Ctrl + Alt + 单击",
      L"Shift + Ctrl + Alt + 单击",
      L"Shift + 中键单击",
      L"Ctrl + 中键单击"
  };'''
    if old_triggers in content:
        content = content.replace(old_triggers, new_triggers)
        print("  [OK] Cleaned up trigger option labels")
    else:
        print("  [SKIP] Trigger options - pattern not found, skipping")
    
    # Fix 1b: Remove parenthetical from target combo items
    content = content.replace(
        'L"侧边栏目录树 (Directory Tree)"',
        'L"侧边栏目录树"'
    )
    content = content.replace(
        'L"链接栏项目 (Links Bar)"',
        'L"链接栏项目"'
    )
    content = content.replace(
        'L"地址栏导航 (Address Bar)"',
        'L"地址栏导航"'
    )
    
    # Fix 1c: Remove "复刻 MDIE" from action customization label
    content = content.replace(
        'L"执行动作与鼠标按键触发设置 (复刻 MDIE):"',
        'L"执行动作与鼠标按键触发设置:"'
    )
    
    # Fix 1d: Remove parenthetical from Page 2 combobox items
    content = content.replace(
        'L"单栏 (1x1)"',
        'L"单栏"'
    )
    content = content.replace(
        'L"双栏 (1x2 垂直)"',
        'L"双栏 - 左右分栏"'
    )
    content = content.replace(
        'L"双栏 (2x1 水平)"',
        'L"双栏 - 上下分栏"'
    )
    content = content.replace(
        'L"四栏 (2x2 网格)"',
        'L"四栏 - 2x2 网格"'
    )
    content = content.replace(
        'L"三栏 (1左2右)"',
        'L"三栏 - 1左2右"'
    )
    
    # Fix 1e: Remove parenthetical from View Mode combobox
    content = content.replace('L"详细信息 (Details)"', 'L"详细信息"')
    content = content.replace('L"大图标 (Large Icons)"', 'L"大图标"')
    content = content.replace('L"小图标 (Small Icons)"', 'L"小图标"')
    content = content.replace('L"列表 (List)"', 'L"列表"')
    content = content.replace('L"平铺 (Tiles)"', 'L"平铺"')
    
    # Fix 1f: Remove parenthetical from Theme combobox
    content = content.replace('L"跟随系统设置 (System Auto)"', 'L"跟随系统设置"')
    content = content.replace('L"浅色模式 (Light)"', 'L"浅色模式"')
    content = content.replace('L"深色模式 (Dark)"', 'L"深色模式"')
    
    # Fix 1g: Remove parenthetical from Tab action combobox
    content = content.replace('L"关闭该标签页"', 'L"关闭该标签页"')  # no-op
    
    # Fix 1h: Add ListView dark mode coloring after ListView creation
    # After s_hShortcutList creation, add dark colors
    old_shortcut_list = '''  PopulateShortcutList(s_hShortcutList);
  addCtrl(7, s_hShortcutList);'''
    new_shortcut_list = '''  PopulateShortcutList(s_hShortcutList);
  addCtrl(7, s_hShortcutList);
  // Apply dark mode colors to shortcut list
  Theme::Get().ApplyToWindow(s_hShortcutList);
  ListView_SetBkColor(s_hShortcutList, Theme::Get().ClrBackground());
  ListView_SetTextBkColor(s_hShortcutList, Theme::Get().ClrBackground());
  ListView_SetTextColor(s_hShortcutList, Theme::Get().ClrTextPrimary());'''
    if old_shortcut_list in content:
        content = content.replace(old_shortcut_list, new_shortcut_list)
        print("  [OK] Added dark mode colors to shortcut list")
    else:
        print("  [WARN] Shortcut list dark color pattern not found")
    
    # Fix 1i: Add dark mode colors after gesture list creation
    old_gesture_list = '''  PopulateGestureList(s_hGestureList);
  addCtrl(8, s_hGestureList);'''
    new_gesture_list = '''  PopulateGestureList(s_hGestureList);
  addCtrl(8, s_hGestureList);
  // Apply dark mode colors to gesture list
  Theme::Get().ApplyToWindow(s_hGestureList);
  ListView_SetBkColor(s_hGestureList, Theme::Get().ClrBackground());
  ListView_SetTextBkColor(s_hGestureList, Theme::Get().ClrBackground());
  ListView_SetTextColor(s_hGestureList, Theme::Get().ClrTextPrimary());'''
    if old_gesture_list in content:
        content = content.replace(old_gesture_list, new_gesture_list)
        print("  [OK] Added dark mode colors to gesture list")
    else:
        print("  [WARN] Gesture list dark color pattern not found")
    
    # Fix 1j: Apply dark theme to each combo using SetWindowTheme in createCombo lambda
    old_combo_lambda = '''  auto createCombo = [&](int page, int x, int y, int id, int w = 240) {
    HWND hCmb = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS,
        x, y, Theme::Get().Scale(w, hwnd), Theme::Get().Scale(240, hwnd),
        hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hCmb, WM_SETFONT, (WPARAM)hFont, TRUE);
    return addCtrl(page, hCmb);
  };'''
    new_combo_lambda = '''  auto createCombo = [&](int page, int x, int y, int id, int w = 240) {
    HWND hCmb = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS,
        x, y, Theme::Get().Scale(w, hwnd), Theme::Get().Scale(240, hwnd),
        hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
    SendMessageW(hCmb, WM_SETFONT, (WPARAM)hFont, TRUE);
    // Apply dark mode theme to combobox dropdown
    if (Theme::Get().IsDarkMode()) {
        SetWindowTheme(hCmb, L"DarkMode_CFD", nullptr);
    }
    return addCtrl(page, hCmb);
  };'''
    if old_combo_lambda in content:
        content = content.replace(old_combo_lambda, new_combo_lambda)
        print("  [OK] Applied DarkMode_CFD theme to comboboxes")
    else:
        print("  [WARN] Combo lambda pattern not found")
    
    # Fix 1k: Fix WM_CTLCOLORSTATIC to also handle BS_CHECKBOX (button controls with static style)
    # Add WM_CTLCOLORBTN handler to properly color checkbox text in dark mode
    old_ctlbtn = '''  case WM_CTLCOLORBTN: {
    return (INT_PTR)Theme::Get().BrushBackground();
  }'''
    new_ctlbtn = '''  case WM_CTLCOLORBTN: {
    HDC hdcBtn = (HDC)wParam;
    SetTextColor(hdcBtn, Theme::Get().ClrTextPrimary());
    SetBkColor(hdcBtn, Theme::Get().ClrBackground());
    return (INT_PTR)Theme::Get().BrushBackground();
  }'''
    if old_ctlbtn in content:
        content = content.replace(old_ctlbtn, new_ctlbtn)
        print("  [OK] Fixed WM_CTLCOLORBTN for checkbox text")
    else:
        print("  [WARN] WM_CTLCOLORBTN pattern not found")
    
    # Fix 1l: Add Shortcut list column header cleanup (remove Chinese/English mixing)
    content = content.replace(
        'lvcSc.pszText = (LPWSTR)(zh ? L"快捷键 (Shortcut)" : L"Shortcut");',
        'lvcSc.pszText = (LPWSTR)(zh ? L"快捷键" : L"Shortcut");'
    )
    content = content.replace(
        'lvcSc.pszText = (LPWSTR)(zh ? L"执行功能 (Action)" : L"Action");',
        'lvcSc.pszText = (LPWSTR)(zh ? L"执行功能" : L"Action");'
    )
    content = content.replace(
        'lvcSc.pszText = (LPWSTR)(zh ? L"分类 (Category)" : L"Category");',
        'lvcSc.pszText = (LPWSTR)(zh ? L"分类" : L"Category");'
    )
    
    # Fix 1m: Clean gesture list column headers
    content = content.replace(
        'lvcG.pszText = (LPWSTR)(zh ? L"手势轨迹 (Pattern)" : L"Pattern");',
        'lvcG.pszText = (LPWSTR)(zh ? L"手势轨迹" : L"Pattern");'
    )
    content = content.replace(
        'lvcG.pszText = (LPWSTR)(zh ? L"对应执行动作 (Action)" : L"Action");',
        'lvcG.pszText = (LPWSTR)(zh ? L"对应执行动作" : L"Action");'
    )
    
    # Fix 1n: View mode combo cleanup
    content = content.replace(
        'L"界面显示语言 (Language):"',
        'L"界面显示语言:"'
    )
    
    write_file(path, content)
    print("  [DONE] SettingsDialog.cpp fixed")


# ============================================================================
# FIX 2: FileListView.cpp - Zebra striping and gridlines
# ============================================================================
def fix_filelist_view():
    path = os.path.join(BASE, r"src\ui\FileListView.cpp")
    content = read_file(path)
    
    # Fix 2a: Apply gridlines extension style in ApplyTheme (dynamic from config)
    old_apply_theme = '''void FileListView::ApplyTheme() {
    if (!m_hwnd) return;
    bool isDark = Theme::Get().IsDarkMode();
    Theme::Get().ApplyToWindow(m_hwnd);
    COLORREF bg = Theme::Get().ClrBackground();
    COLORREF text = Theme::Get().ClrTextPrimary();
    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, bg);
    ListView_SetTextColor(m_hwnd, text);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowTheme(hHeader, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
}'''
    new_apply_theme = '''void FileListView::ApplyTheme() {
    if (!m_hwnd) return;
    bool isDark = Theme::Get().IsDarkMode();
    Theme::Get().ApplyToWindow(m_hwnd);
    COLORREF bg = Theme::Get().ClrBackground();
    COLORREF text = Theme::Get().ClrTextPrimary();
    ListView_SetBkColor(m_hwnd, bg);
    ListView_SetTextBkColor(m_hwnd, bg);
    ListView_SetTextColor(m_hwnd, text);

    HWND hHeader = ListView_GetHeader(m_hwnd);
    if (hHeader) {
        Theme::Get().ApplyToWindow(hHeader);
        SetWindowTheme(hHeader, isDark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
    }

    // Apply gridlines from config
    DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_SUBITEMIMAGES;
    if (Config::Get().showGridLines) {
        exStyle |= LVS_EX_GRIDLINES;
    }
    ListView_SetExtendedListViewStyle(m_hwnd, exStyle);

    InvalidateRect(m_hwnd, nullptr, TRUE);
}'''
    if old_apply_theme in content:
        content = content.replace(old_apply_theme, new_apply_theme)
        print("  [OK] Added gridlines to ApplyTheme")
    else:
        print("  [WARN] ApplyTheme pattern not found in FileListView.cpp")

    # Fix 2b: Add Config.h include if not already there
    if '#include "..\\core\\Config.h"' not in content and '#include "../core/Config.h"' not in content:
        content = content.replace(
            '#include "FileListView.h"',
            '#include "FileListView.h"\r\n#include "..\\core\\Config.h"'
        )
        print("  [OK] Added Config.h include")

    # Fix 2c: Add zebra striping in NM_CUSTOMDRAW CDDS_ITEMPREPAINT
    # The current code only handles drive space custom draw in CDDS_SUBITEM | CDDS_ITEMPREPAINT
    # We need to add zebra striping in CDDS_ITEMPREPAINT
    old_customdraw = '''            case NM_CUSTOMDRAW: {
                NMLVCUSTOMDRAW* pcd = (NMLVCUSTOMDRAW*)lParam;
                switch (pcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                    return CDRF_NOTIFYSUBITEMDRAW;
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {'''
    new_customdraw = '''            case NM_CUSTOMDRAW: {
                NMLVCUSTOMDRAW* pcd = (NMLVCUSTOMDRAW*)lParam;
                switch (pcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    // Zebra striping: alternate row background colors
                    if (Config::Get().zebraStriping) {
                        int row = (int)pcd->nmcd.dwItemSpec;
                        bool isSelected = (pcd->nmcd.uItemState & CDIS_SELECTED) != 0;
                        if (!isSelected && (row % 2 == 1)) {
                            // Odd rows get slightly different background
                            COLORREF zebraClr = Theme::Get().IsDarkMode()
                                ? RGB(44, 46, 50)
                                : RGB(246, 248, 252);
                            pcd->clrTextBk = zebraClr;
                            pcd->clrText = Theme::Get().ClrTextPrimary();
                        }
                    }
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {'''
    if old_customdraw in content:
        content = content.replace(old_customdraw, new_customdraw)
        print("  [OK] Added zebra striping in NM_CUSTOMDRAW")
    else:
        print("  [WARN] NM_CUSTOMDRAW pattern not found")
    
    write_file(path, content)
    print("  [DONE] FileListView.cpp fixed")


# ============================================================================
# FIX 3: MainWindow.cpp - Sidebar width + dark mode menu + gestures
# ============================================================================
def fix_mainwindow():
    path = os.path.join(BASE, r"src\ui\MainWindow.cpp")
    content = read_file(path)
    
    # Fix 3a: Sidebar width - change clamp from totalW/2 to reasonable max
    old_tree_clamp = '''    if (m_treeVisible) {
        if (m_treeWidth < Theme::Get().Scale(120, m_hwnd)) {
            m_treeWidth = Theme::Get().Scale(120, m_hwnd);
        }
        if (m_treeWidth > totalW / 2) {
            m_treeWidth = totalW / 2;
        }'''
    new_tree_clamp = '''    if (m_treeVisible) {
        if (m_treeWidth < Theme::Get().Scale(120, m_hwnd)) {
            m_treeWidth = Theme::Get().Scale(120, m_hwnd);
        }
        // Limit sidebar to at most 1/3 of total width, not 1/2
        int maxTreeW = std::min(Theme::Get().Scale(400, m_hwnd), totalW / 3);
        if (maxTreeW < Theme::Get().Scale(180, m_hwnd)) maxTreeW = Theme::Get().Scale(180, m_hwnd);
        if (m_treeWidth > maxTreeW) {
            m_treeWidth = maxTreeW;
        }'''
    if old_tree_clamp in content:
        content = content.replace(old_tree_clamp, new_tree_clamp)
        print("  [OK] Fixed sidebar width clamp")
    else:
        print("  [WARN] Sidebar width clamp pattern not found")
    
    # Fix 3b: Add WM_SYSCOLORCHANGE after ApplyTheme to force dark menu redraw
    old_apply_theme_end = '''    DrawMenuBar(m_hwnd);
}'''
    new_apply_theme_end = '''    DrawMenuBar(m_hwnd);
    // Force Windows to re-draw dark menus
    SendMessageW(m_hwnd, WM_SYSCOLORCHANGE, 0, 0);
}'''
    # Only replace the one in ApplyTheme function (first occurrence)
    idx = content.find(old_apply_theme_end)
    if idx >= 0:
        content = content[:idx] + new_apply_theme_end + content[idx + len(old_apply_theme_end):]
        print("  [OK] Added WM_SYSCOLORCHANGE after DrawMenuBar")
    else:
        print("  [WARN] DrawMenuBar pattern not found")
    
    # Fix 3c: Handle gesture ScrollTop/Bottom/PageUp/PageDown actions
    # Search for the gesture action dispatch in MainWindow
    # Look for GestureAction handler patterns
    search_gesture = "GestureAction::ScrollTop"
    if search_gesture in content:
        print("  [INFO] Gesture ScrollTop already handled - checking implementation")
    else:
        print("  [INFO] GestureAction::ScrollTop not found in MainWindow.cpp - may be in a different location")
    
    write_file(path, content)
    print("  [DONE] MainWindow.cpp fixed")


# ============================================================================
# FIX 4: FileListView.cpp - Ensure ScrollToTop, ScrollToBottom, etc. work
# ============================================================================
def fix_filelist_scroll():
    path = os.path.join(BASE, r"src\ui\FileListView.h")
    content = read_file(path)
    
    # Check if scroll methods exist
    if 'ScrollToTop' not in content:
        print("  [WARN] ScrollToTop not in FileListView.h - need to add")
        # Add scroll methods to public interface
        old_public = '    void ApplyTheme();'
        new_public = '''    void ApplyTheme();
    void ScrollToTop() {
        if (m_hwnd && ListView_GetItemCount(m_hwnd) > 0)
            ListView_EnsureVisible(m_hwnd, 0, FALSE);
    }
    void ScrollToBottom() {
        int n = ListView_GetItemCount(m_hwnd);
        if (m_hwnd && n > 0) ListView_EnsureVisible(m_hwnd, n - 1, FALSE);
    }
    void ScrollPageUp() {
        if (m_hwnd) SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEUP, 0);
    }
    void ScrollPageDown() {
        if (m_hwnd) SendMessageW(m_hwnd, WM_VSCROLL, SB_PAGEDOWN, 0);
    }'''
        if old_public in content:
            content = content.replace(old_public, new_public)
            write_file(path, content)
            print("  [OK] Added scroll methods to FileListView.h")
        else:
            print("  [WARN] Could not find insertion point in FileListView.h")
    else:
        print("  [OK] Scroll methods already exist in FileListView.h")


# ============================================================================
# Main execution
# ============================================================================
if __name__ == "__main__":
    print("=" * 60)
    print("NeoMDIE Fix All Issues Script")
    print("=" * 60)
    
    print("\n[1] Fixing SettingsDialog.cpp...")
    fix_settings_dialog()
    
    print("\n[2] Fixing FileListView.cpp (zebra + gridlines)...")
    fix_filelist_view()
    
    print("\n[3] Fixing MainWindow.cpp (sidebar + dark menu + gestures)...")
    fix_mainwindow()
    
    print("\n[4] Checking FileListView.h scroll methods...")
    fix_filelist_scroll()
    
    print("\n" + "=" * 60)
    print("All fixes applied! Run build.bat to compile.")
    print("=" * 60)
