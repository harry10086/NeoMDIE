# -*- coding: utf-8 -*-
import sys, re

# ==========================================
# 1. Update MultiPaneContainer.h & .cpp
# ==========================================
with open(r'src\ui\MultiPaneContainer.h', 'r', encoding='utf-8') as f:
    h_text = f.read()

# Add onLayoutChanged and m_maximizedPane
old_mpc_cb = """  std::function<void(int activeIndex, const std::wstring &path)> onActivePaneChanged;
  std::function<void(GestureAction action)> onGestureAction;
  std::function<void(int neoCmd, const std::wstring &path)> onCustomCommand;"""

new_mpc_cb = """  std::function<void(int activeIndex, const std::wstring &path)> onActivePaneChanged;
  std::function<void(PaneLayout newLayout)> onLayoutChanged;
  std::function<void(GestureAction action)> onGestureAction;
  std::function<void(int neoCmd, const std::wstring &path)> onCustomCommand;"""

h_text = h_text.replace(old_mpc_cb, new_mpc_cb)

if "int m_maximizedPane = -1;" not in h_text:
    h_text = h_text.replace("PaneLayout m_prevSplitLayout = PaneLayout::DualVertical;",
                           "PaneLayout m_prevSplitLayout = PaneLayout::DualVertical;\n  int m_maximizedPane = -1;")

with open(r'src\ui\MultiPaneContainer.h', 'w', encoding='utf-8') as f:
    f.write(h_text)
print("MultiPaneContainer.h updated!")

with open(r'src\ui\MultiPaneContainer.cpp', 'r', encoding='utf-8') as f:
    cpp_text = f.read()

# Update ClosePane, ToggleMaximizePane, SetLayout, LayoutChildren
old_close = """void MultiPaneContainer::ClosePane(int index) {
  int count = GetPaneCount();
  if (count <= 1) {
    return;
  }

  if (m_layout == PaneLayout::DualVertical || m_layout == PaneLayout::DualHorizontal) {
    if (index == 0) {
      // Copy pane 1 path to pane 0
      m_panes[0].NavigateTo(m_panes[1].GetCurrentPath());
    }
    SetLayout(PaneLayout::Single);
    SetActivePane(0);
  } else if (m_layout == PaneLayout::Quad) {
    SetLayout(PaneLayout::ThreeLeft);
  } else if (m_layout == PaneLayout::ThreeLeft) {
    SetLayout(PaneLayout::DualVertical);
  }
}"""

new_close = """void MultiPaneContainer::ClosePane(int index) {
  int count = GetPaneCount();
  if (count <= 1) {
    return;
  }

  m_maximizedPane = -1;

  if (m_layout == PaneLayout::Quad) {
    // 4 -> 3
    for (int i = index; i < 3; ++i) {
      m_panes[i].NavigateTo(m_panes[i + 1].GetCurrentPath());
    }
    SetLayout(PaneLayout::ThreeLeft);
  } else if (m_layout == PaneLayout::ThreeLeft) {
    // 3 -> 2
    for (int i = index; i < 2; ++i) {
      m_panes[i].NavigateTo(m_panes[i + 1].GetCurrentPath());
    }
    SetLayout(PaneLayout::DualVertical);
  } else if (m_layout == PaneLayout::DualVertical || m_layout == PaneLayout::DualHorizontal) {
    // 2 -> 1
    if (index == 0) {
      m_panes[0].NavigateTo(m_panes[1].GetCurrentPath());
    }
    SetLayout(PaneLayout::Single);
    SetActivePane(0);
  }
}"""

old_toggle = """void MultiPaneContainer::ToggleMaximizePane(int index) {
  if (m_layout == PaneLayout::Single) {
    // Restore
    SetLayout(m_prevSplitLayout);
    SetActivePane(index);
  } else {
    // Maximize
    m_prevSplitLayout = m_layout;
    if (index > 0) {
      // Bring index path to 0
      std::wstring path = m_panes[index].GetCurrentPath();
      m_panes[0].NavigateTo(path);
    }
    SetLayout(PaneLayout::Single);
    SetActivePane(0);
  }
}"""

new_toggle = """void MultiPaneContainer::ToggleMaximizePane(int index) {
  if (m_layout == PaneLayout::Single) {
    return; // Already single pane, no maximize/restore
  }
  if (m_maximizedPane >= 0) {
    // Restore back to normal multi-pane layout
    m_maximizedPane = -1;
  } else {
    // Temporarily maximize this pane
    m_maximizedPane = index;
    SetActivePane(index);
  }
  LayoutChildren();
  InvalidateRect(m_hwnd, nullptr, TRUE);
  for (int i = 0; i < 4; ++i) {
    UpdatePaneHeader(i);
  }
}"""

old_setlayout = """void MultiPaneContainer::SetLayout(PaneLayout layout) {
  m_layout = layout;
  int count = GetPaneCount();

  std::wstring curPath = m_panes[0].GetCurrentPath();
  if (curPath.empty())
    curPath = L"This PC";

  for (int i = 0; i < 4; ++i) {
    bool shouldShow = (i < count);
    ShowWindow(m_panes[i].GetHwnd(), shouldShow ? SW_SHOW : SW_HIDE);
    if (m_hHeaders[i]) {
      ShowWindow(m_hHeaders[i], shouldShow ? SW_SHOW : SW_HIDE);
      UpdatePaneHeader(i);
    }
    if (shouldShow && m_panes[i].GetCurrentPath().empty()) {
      m_panes[i].NavigateTo(curPath);
    }
  }

  if (m_activePane >= count) {
    m_activePane = 0;
  }

  LayoutChildren();
  InvalidateRect(m_hwnd, nullptr, TRUE);
}"""

new_setlayout = """void MultiPaneContainer::SetLayout(PaneLayout layout) {
  m_layout = layout;
  m_maximizedPane = -1; // Explicit layout change clears temporary maximize
  int count = GetPaneCount();

  std::wstring curPath = m_panes[0].GetCurrentPath();
  if (curPath.empty())
    curPath = L"This PC";

  for (int i = 0; i < 4; ++i) {
    bool shouldShow = (i < count);
    ShowWindow(m_panes[i].GetHwnd(), shouldShow ? SW_SHOW : SW_HIDE);
    if (m_hHeaders[i]) {
      ShowWindow(m_hHeaders[i], shouldShow ? SW_SHOW : SW_HIDE);
      UpdatePaneHeader(i);
    }
    if (shouldShow && m_panes[i].GetCurrentPath().empty()) {
      m_panes[i].NavigateTo(curPath);
    }
  }

  if (m_activePane >= count) {
    m_activePane = 0;
  }

  LayoutChildren();
  InvalidateRect(m_hwnd, nullptr, TRUE);

  if (onLayoutChanged) {
    onLayoutChanged(m_layout);
  }
}"""

for old_s, new_s in [(old_close, new_close), (old_toggle, new_toggle), (old_setlayout, new_setlayout)]:
    old_c = old_s.replace('\n', '\r\n')
    new_c = new_s.replace('\n', '\r\n')
    if old_c in cpp_text:
        cpp_text = cpp_text.replace(old_c, new_c, 1)
    elif old_s in cpp_text:
        cpp_text = cpp_text.replace(old_s, new_s, 1)
    else:
        print("MPC chunk not found:", old_s[:30])

# In LayoutChildren: support m_maximizedPane
old_lc_start = """void MultiPaneContainer::LayoutChildren() {
  if (!m_hwnd) return;
  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);"""

new_lc_start = """void MultiPaneContainer::LayoutChildren() {
  if (!m_hwnd) return;
  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);
  int totalW = rcClient.right - rcClient.left;
  int totalH = rcClient.bottom - rcClient.top;
  if (totalW <= 0 || totalH <= 0) return;

  int headerH = Theme::Get().Scale(26, m_hwnd);
  int borderPad = Theme::Get().Scale(1, m_hwnd);
  int splitW = Theme::Get().Scale(5, m_hwnd);

  auto positionPane = [&](int idx, int x, int y, int w, int h) {
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
  }"""

old_lc_whole = """void MultiPaneContainer::LayoutChildren() {
  if (!m_hwnd) return;
  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);
  int totalW = rcClient.right - rcClient.left;
  int totalH = rcClient.bottom - rcClient.top;
  if (totalW <= 0 || totalH <= 0) return;

  int headerH = Theme::Get().Scale(26, m_hwnd);
  int borderPad = Theme::Get().Scale(1, m_hwnd);
  int splitW = Theme::Get().Scale(5, m_hwnd);

  auto positionPane = [&](int idx, int x, int y, int w, int h) {
    if (idx >= 0 && idx < 4) {
      if (m_hHeaders[idx]) {
        SetWindowPos(m_hHeaders[idx], HWND_TOP, x, y, w, headerH, SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        InvalidateRect(m_hHeaders[idx], nullptr, TRUE);
      }
      m_panes[idx].UpdateLayout(x, y + headerH, w, h - headerH);
    }
  };"""

old_c = old_lc_whole.replace('\n', '\r\n')
new_c = new_lc_start.replace('\n', '\r\n')
if old_c in cpp_text:
    cpp_text = cpp_text.replace(old_c, new_c, 1)
elif old_lc_whole in cpp_text:
    cpp_text = cpp_text.replace(old_lc_whole, new_lc_start, 1)
else:
    print("old_lc_whole not found!")

# In HandleHeaderMessage: update maximize icon to show restore when maximized
old_icon_max = """    const wchar_t* icoMax = (GetPaneCount() == 1) ? L"\\uE923" : L"\\uE922";"""
new_icon_max = """    const wchar_t* icoMax = (m_maximizedPane == paneIdx) ? L"\\uE923" : L"\\uE922";"""
cpp_text = cpp_text.replace(old_icon_max, new_icon_max)

with open(r'src\ui\MultiPaneContainer.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_text)
print("MultiPaneContainer.cpp updated!")

# ==========================================
# 2. Update MainWindow.cpp
# ==========================================
with open(r'src\ui\MainWindow.cpp', 'r', encoding='utf-8') as f:
    mw_text = f.read()

# 2a. Wire onLayoutChanged in MainWindow::CreateControls
old_multi_setup = """    // MultiPane gesture & custom command callbacks
    m_multiPane.onGestureAction = [this](GestureAction act) {"""

new_multi_setup = """    // MultiPane layout change callback
    m_multiPane.onLayoutChanged = [this](PaneLayout newLayout) {
        Config::Get().paneLayout = (int)newLayout;
        Config::Get().splitEnabled = (newLayout != PaneLayout::Single);
        BuildMenu();
        LayoutAll();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    };

    // MultiPane gesture & custom command callbacks
    m_multiPane.onGestureAction = [this](GestureAction act) {"""

mw_text = mw_text.replace(old_multi_setup.replace('\n', '\r\n'), new_multi_setup.replace('\n', '\r\n'))
if old_multi_setup in mw_text:
    mw_text = mw_text.replace(old_multi_setup, new_multi_setup)

# 2b. Instant preview toggle
old_toggle_prev = """void MainWindow::TogglePreview() {
    m_previewVisible = !m_previewVisible;
    ShowWindow(m_preview.GetHwnd(), m_previewVisible ? SW_SHOW : SW_HIDE);
    if (m_previewVisible) {
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            auto sel = activeView->GetSelectedPaths();
            if (!sel.empty()) {
                m_preview.SetFile(sel[0]);
            }
        }
    } else {
        m_preview.Clear();
    }
    BuildMenu();
    LayoutAll();
}"""

new_toggle_prev = """void MainWindow::TogglePreview() {
    m_previewVisible = !m_previewVisible;
    BuildMenu();
    LayoutAll();
    UpdateWindow(m_hwnd); // Instant visual update of toolbar preview icon to gray!

    if (m_previewVisible) {
        ShowWindow(m_preview.GetHwnd(), SW_SHOW);
        FileListView* activeView = m_multiPane.GetActivePane();
        if (activeView) {
            auto sel = activeView->GetSelectedPaths();
            if (!sel.empty()) {
                m_preview.SetFile(sel[0]);
            }
        }
    } else {
        ShowWindow(m_preview.GetHwnd(), SW_HIDE);
        m_preview.Clear();
    }
}"""

mw_text = mw_text.replace(old_toggle_prev.replace('\n', '\r\n'), new_toggle_prev.replace('\n', '\r\n'))
if old_toggle_prev in mw_text:
    mw_text = mw_text.replace(old_toggle_prev, new_toggle_prev)

# 2c. Update About dialog and Github link
old_about = """    case ID_HELP_GITHUB:
        ShellExecuteW(nullptr, L"open", L"https://github.com/mianao/NeoMDIE", nullptr, nullptr, SW_SHOWNORMAL);
        break;"""

new_about = """    case ID_HELP_GITHUB:
        ShellExecuteW(nullptr, L"open", L"https://github.com/harry10086/NeoMDIE", nullptr, nullptr, SW_SHOWNORMAL);
        break;"""
mw_text = mw_text.replace(old_about.replace('\n', '\r\n'), new_about.replace('\n', '\r\n'))
if old_about in mw_text:
    mw_text = mw_text.replace(old_about, new_about)

old_about_body = """    case ID_HELP_ABOUT: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring aboutText = isZh ?
            L"NeoMDIE - 现代化纯原生 Win32 多标签/多分屏文件管理器\\n"
            L"版本: v2.1.0 (x64 Release)\\n\\n"
            L"作者: Harry (mianao)\\n"
            L"开源项目仓库: https://github.com/mianao/NeoMDIE\\n"
            L"致敬经典: MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"技术特色:\\n"
            L"• 纯 C++17 / Native Win32 API / Windows Shell COM 引擎\\n"
            L"• 静态编译 (/MT)，零额外运行时依赖，轻量疾速 (<1MB)\\n"
            L"• 支持多栏分屏、鼠标手势、多标签彩色标记与即时搜索过滤\\n\\n"
            L"基于 GNU General Public License v3 (GPLv3) 开源协议发布。" :
            L"NeoMDIE - Modern Native Win32 Tabbed & Multi-Pane File Manager\\n"
            L"Version: v2.1.0 (x64 Release)\\n\\n"
            L"Author: Harry (mianao)\\n"
            L"Repository: https://github.com/mianao/NeoMDIE\\n"
            L"Inspired by: MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"Pure C++17 / Native Win32 / Shell COM. Zero dependencies.\\n"
            L"Licensed under the GNU General Public License v3 (GPLv3).";
        MessageBoxW(m_hwnd, aboutText.c_str(), isZh ? L"关于 NeoMDIE" : L"About NeoMDIE", MB_OK | MB_ICONINFORMATION);
        break;
    }"""

new_about_body = """    case ID_HELP_ABOUT: {
        bool isZh = I18n::Get().IsChinese();
        std::wstring aboutText = isZh ?
            L"NeoMDIE - 现代化纯原生 Win32 多标签/多栏文件管理器\\n"
            L"版本: v1.0.0 (x64 Release)\\n\\n"
            L"作者: Harry (https://mianao.info)\\n"
            L"开源项目仓库: https://github.com/harry10086/NeoMDIE\\n"
            L"致敬经典: MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"技术特色:\\n"
            L"• 纯 C++17 / Native Win32 API / Windows Shell COM 引擎\\n"
            L"• 静态编译 (/MT)，零额外运行时依赖，轻量疾速 (<1MB)\\n"
            L"• 支持多栏分栏、鼠标手势、多标签彩色标记与即时搜索过滤\\n\\n"
            L"基于 GNU General Public License v3 (GPLv3) 开源协议发布。" :
            L"NeoMDIE - Modern Native Win32 Tabbed & Multi-Pane File Manager\\n"
            L"Version: v1.0.0 (x64 Release)\\n\\n"
            L"Author: Harry (https://mianao.info)\\n"
            L"Repository: https://github.com/harry10086/NeoMDIE\\n"
            L"Inspired by: MDIE (Tabbed File Manager for Windows)\\n\\n"
            L"Pure C++17 / Native Win32 / Shell COM. Zero dependencies.\\n"
            L"Licensed under the GNU General Public License v3 (GPLv3).";
        MessageBoxW(m_hwnd, aboutText.c_str(), isZh ? L"关于 NeoMDIE" : L"About NeoMDIE", MB_OK | MB_ICONINFORMATION);
        break;
    }"""

mw_text = mw_text.replace(old_about_body.replace('\n', '\r\n'), new_about_body.replace('\n', '\r\n'))
if old_about_body in mw_text:
    mw_text = mw_text.replace(old_about_body, new_about_body)

# 2d. Sync filter bar on tab change
old_tab_sync = """        m_breadcrumb.SetPath(path);
        if (m_treeVisible && Config::Get().autoExpandTree) {
            m_tree.SelectPath(path);
        }
        UpdateNavButtons();
        AddToHistory(path);
        UpdateWindowTitle(path);"""

new_tab_sync = """        m_breadcrumb.SetPath(path);
        m_filterBar.SetText(activeView ? activeView->GetFilterQuery() : L"");
        if (m_treeVisible && Config::Get().autoExpandTree) {
            m_tree.SelectPath(path);
        }
        UpdateNavButtons();
        AddToHistory(path);
        UpdateWindowTitle(path);"""

mw_text = mw_text.replace(old_tab_sync.replace('\n', '\r\n'), new_tab_sync.replace('\n', '\r\n'))
if old_tab_sync in mw_text:
    mw_text = mw_text.replace(old_tab_sync, new_tab_sync)

# 2e. Terminology: replace all "分屏" with "栏" / "分栏" in MainWindow strings
mw_text = mw_text.replace("四分屏(&Q)", "四栏(&Q)")
mw_text = mw_text.replace("多栏分屏 (F4)", "多栏布局 (F4)")

with open(r'src\ui\MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(mw_text)
print("MainWindow.cpp updated!")

# ==========================================
# 3. Update I18n strings (replace 分屏 with 栏)
# ==========================================
with open(r'src\core\I18n.cpp', 'r', encoding='utf-8') as f:
    i18n_text = f.read()

i18n_text = i18n_text.replace("⊞ 分屏", "⊞ 分栏")
i18n_text = i18n_text.replace("多栏分屏(&S)", "多栏布局(&S)")
i18n_text = i18n_text.replace("左右双栏分屏", "左右双栏")
i18n_text = i18n_text.replace("上下双栏分屏", "上下双栏")
i18n_text = i18n_text.replace("田字四分屏", "田字四栏")
i18n_text = i18n_text.replace("1左2右三分屏", "1左2右三栏")
i18n_text = i18n_text.replace("⊞ 田字四分屏 (2x2)", "⊞ 田字四栏 (2x2)")
i18n_text = i18n_text.replace("⧉ 1左2右三分屏 (1+2)", "⧉ 1左2右三栏 (1+2)")
i18n_text = i18n_text.replace("同步浏览 (所有分屏联动)", "同步浏览 (所有分栏联动)")
i18n_text = i18n_text.replace("双栏分屏 (Ctrl+E)", "多栏切换 (Ctrl+E)")

with open(r'src\core\I18n.cpp', 'w', encoding='utf-8') as f:
    f.write(i18n_text)
print("I18n.cpp updated!")

with open(r'src\resources\NeoMDIE.rc', 'r', encoding='utf-8') as f:
    rc_text = f.read()

rc_text = rc_text.replace("双栏分屏(&S)\\tCtrl+E", "多栏切换(&S)\\tCtrl+E")
with open(r'src\resources\NeoMDIE.rc', 'w', encoding='utf-8') as f:
    f.write(rc_text)
print("NeoMDIE.rc updated!")
