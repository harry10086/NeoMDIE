# -*- coding: utf-8 -*-

# 1. SettingsDialog.h
h_code = """#pragma once
#include "..\\Common.h"

class SettingsDialog {
public:
  static void Show(HWND hwndParent, std::function<void()> onSettingsChanged, int initialCategory = 0);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void CreateControls(HWND hwnd);
  static void SwitchTab(HWND hwnd, int tabIndex);
  static void SaveSettings(HWND hwnd);
};
"""
with open(r'src\ui\SettingsDialog.h', 'w', encoding='utf-8') as f:
    f.write(h_code)
print("SettingsDialog.h fixed!")

# 2. SettingsDialog.cpp: GetBoldUIFont -> GetUIFontBold
with open(r'src\ui\SettingsDialog.cpp', 'r', encoding='utf-8') as f:
    sd_cpp = f.read()

sd_cpp = sd_cpp.replace("Theme::Get().GetBoldUIFont(hwnd)", "Theme::Get().GetUIFontBold(hwnd)")
with open(r'src\ui\SettingsDialog.cpp', 'w', encoding='utf-8') as f:
    f.write(sd_cpp)
print("SettingsDialog.cpp fixed!")

# 3. LinksBar.cpp: include commdlg.h
with open(r'src\ui\LinksBar.cpp', 'r', encoding='utf-8') as f:
    lb_cpp = f.read()

if "<commdlg.h>" not in lb_cpp:
    lb_cpp = '#include <commdlg.h>\n' + lb_cpp
with open(r'src\ui\LinksBar.cpp', 'w', encoding='utf-8') as f:
    f.write(lb_cpp)
print("LinksBar.cpp fixed!")

# 4. MultiPaneContainer.cpp: clean GetPaneCount and place maximize check in LayoutChildren
with open(r'src\ui\MultiPaneContainer.cpp', 'r', encoding='utf-8') as f:
    mpc_cpp = f.read()

bad_gpc = """int MultiPaneContainer::GetPaneCount() const {
  if (m_maximizedPane >= 0 && m_maximizedPane < GetPaneCount()) {
    for (int i = 0; i < 4; ++i) {
      if (i == m_maximizedPane) {
        ShowWindow(m_panes[i].GetHwnd(), SW_SHOW);
        if (m_hHeaders[i]) ShowWindow(m_hHeaders[i], SW_SHOW);
        positionPane(i, borderPad, borderPad, totalW - borderPad * 2, totalH - borderPad * 2);
      } else {
        ShowWindow(m_panes[i].GetHwnd(), SW_HIDE);
        if (m_hHeaders[i]) ShowWindow(m_hHeaders[i], SW_HIDE);
      }
    }
    return;
  }

  switch (m_layout) {"""

good_gpc = """int MultiPaneContainer::GetPaneCount() const {
  switch (m_layout) {"""

mpc_cpp = mpc_cpp.replace(bad_gpc.replace('\n', '\r\n'), good_gpc.replace('\n', '\r\n'))
if bad_gpc in mpc_cpp:
    mpc_cpp = mpc_cpp.replace(bad_gpc, good_gpc)

lc_marker = """  auto positionPane = [&](int idx, int x, int y, int w, int h) {
    if (idx >= 0 && idx < 4) {
      if (m_hHeaders[idx]) {
        SetWindowPos(m_hHeaders[idx], HWND_TOP, x, y, w, headerH, SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        InvalidateRect(m_hHeaders[idx], nullptr, TRUE);
      }
      m_panes[idx].UpdateLayout(x, y + headerH, w, h - headerH);
    }
  };

  switch (m_layout) {"""

lc_replacement = """  auto positionPane = [&](int idx, int x, int y, int w, int h) {
    if (idx >= 0 && idx < 4) {
      if (m_hHeaders[idx]) {
        SetWindowPos(m_hHeaders[idx], HWND_TOP, x, y, w, headerH, SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        InvalidateRect(m_hHeaders[idx], nullptr, TRUE);
      }
      m_panes[idx].UpdateLayout(x, y + headerH, w, h - headerH);
    }
  };

  if (m_maximizedPane >= 0 && m_maximizedPane < GetPaneCount()) {
    for (int i = 0; i < 4; ++i) {
      if (i == m_maximizedPane) {
        ShowWindow(m_panes[i].GetHwnd(), SW_SHOW);
        if (m_hHeaders[i]) ShowWindow(m_hHeaders[i], SW_SHOW);
        positionPane(i, borderPad, borderPad, totalW - borderPad * 2, totalH - borderPad * 2);
      } else {
        ShowWindow(m_panes[i].GetHwnd(), SW_HIDE);
        if (m_hHeaders[i]) ShowWindow(m_hHeaders[i], SW_HIDE);
      }
    }
    return;
  }

  switch (m_layout) {"""

mpc_cpp = mpc_cpp.replace(lc_marker.replace('\n', '\r\n'), lc_replacement.replace('\n', '\r\n'))
if lc_marker in mpc_cpp:
    mpc_cpp = mpc_cpp.replace(lc_marker, lc_replacement)

with open(r'src\ui\MultiPaneContainer.cpp', 'w', encoding='utf-8') as f:
    f.write(mpc_cpp)
print("MultiPaneContainer.cpp fixed!")

# 5. FilterBar.h & .cpp: Add SetText
with open(r'src\ui\FilterBar.h', 'r', encoding='utf-8') as f:
    fb_h = f.read()

if "void SetText(const std::wstring& text);" not in fb_h:
    fb_h = fb_h.replace("void Clear();", "void Clear();\n    void SetText(const std::wstring& text);")
    with open(r'src\ui\FilterBar.h', 'w', encoding='utf-8') as f:
        f.write(fb_h)
    print("FilterBar.h fixed!")

with open(r'src\ui\FilterBar.cpp', 'r', encoding='utf-8') as f:
    fb_cpp = f.read()

if "void FilterBar::SetText" not in fb_cpp:
    add_settext = """void FilterBar::Clear() {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, L"");
    }
}

void FilterBar::SetText(const std::wstring& text) {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, text.c_str());
    }
}"""
    fb_cpp = fb_cpp.replace("""void FilterBar::Clear() {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, L"");
    }
}""", add_settext)
    with open(r'src\ui\FilterBar.cpp', 'w', encoding='utf-8') as f:
        f.write(fb_cpp)
    print("FilterBar.cpp fixed!")

print("All fixes applied!")
