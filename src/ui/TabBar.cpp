#include "TabBar.h"
#include "..\core\Config.h"
#include "..\core\DropTarget.h"
#include "..\core\I18n.h"
#include "..\core\ShellUtils.h"
#include "Theme.h"
#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

static const wchar_t *TABBAR_CLASS = L"NeoMDIE_TabBar";

TabBar::TabBar() {}

TabBar::~TabBar() {
  if (m_pDropTarget && m_hwnd) {
    RevokeDragDrop(m_hwnd);
    m_pDropTarget->Release();
    m_pDropTarget = nullptr;
  }
  if (m_hwndTooltip && IsWindow(m_hwndTooltip)) {
    DestroyWindow(m_hwndTooltip);
    m_hwndTooltip = nullptr;
  }
}

HWND TabBar::Create(HWND hwndParent, int id) {
  HINSTANCE hInst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc = {sizeof(wc)};
  if (!GetClassInfoExW(hInst, TABBAR_CLASS, &wc)) {
    wc.lpfnWndProc = TabBar::WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = TABBAR_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
  }

  m_hwndParent = hwndParent;
  m_hwnd = CreateWindowExW(0, TABBAR_CLASS, L"",
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 100,
                           36, hwndParent, (HMENU)(INT_PTR)id, hInst, this);

  // Create tooltip control for hover path display
  m_hwndTooltip = CreateWindowExW(
      WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
      WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
      CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr, hInst, nullptr);
  SetWindowPos(m_hwndTooltip, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  SendMessageW(m_hwndTooltip, TTM_SETMAXTIPWIDTH, 0, 600);

  m_pDropTarget = new DropTarget(
      m_hwnd, [this](POINT ptScreen, bool &outIsSubFolder) -> std::wstring {
        POINT ptClient = ptScreen;
        ScreenToClient(m_hwnd, &ptClient);
        for (size_t i = 0; i < m_tabs.size(); ++i) {
          if (PtInRect(&m_tabs[i].rc, ptClient)) {
            outIsSubFolder = true;
            return m_tabs[i].path;
          }
        }
        outIsSubFolder = false;
        return L"";
      });

  m_pDropTarget->onDragHover = [this](POINTL pt) {
    POINT ptClient = {pt.x, pt.y};
    ScreenToClient(m_hwnd, &ptClient);
    int hitTab = -1;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
      if (PtInRect(&m_tabs[i].rc, ptClient)) {
        hitTab = (int)i;
        break;
      }
    }
    if (hitTab != -1 && hitTab != m_activeIndex) {
      DWORD now = GetTickCount();
      if (m_hoverTabCandidate != hitTab) {
        m_hoverTabCandidate = hitTab;
        m_hoverTabTick = now;
      } else if (now - m_hoverTabTick > 600) {
        SetActiveTab(hitTab);
        m_hoverTabCandidate = -1;
      }
    } else {
      m_hoverTabCandidate = -1;
    }
  };

  m_pDropTarget->onDragLeave = [this]() { m_hoverTabCandidate = -1; };

  m_pDropTarget->onFilesDropped = [this](const std::wstring &targetDir,
                                         const std::vector<std::wstring> &files,
                                         bool isMove) {
    if (targetDir.empty() || targetDir == L"This PC" || targetDir == L"此电脑")
      return;
    ShellUtils::CopyOrMoveFiles(m_hwnd, targetDir, files, isMove);
  };

  RegisterDragDrop(m_hwnd, m_pDropTarget);

  return m_hwnd;
}

static const COLORREF s_tabPalette[] = {
    RGB(0, 120, 215),   // Azure Blue
    RGB(16, 137, 62),   // Emerald Green
    RGB(232, 17, 35),   // Coral Red
    RGB(136, 23, 152),  // Purple
    RGB(255, 140, 0),   // Orange
    RGB(0, 153, 188),   // Cyan / Teal
    RGB(234, 0, 94),    // Magenta / Pink
    RGB(107, 105, 214), // Lavender
    RGB(16, 124, 65),   // Forest Green
    RGB(202, 80, 16),   // Amber
    RGB(0, 99, 177),    // Cobalt
    RGB(180, 0, 158)    // Violet
};
static size_t s_tabColorIndex = 0;

int TabBar::AddTab(const std::wstring &path, bool activate) {
  if (path.find(L"645FF040-5081-101B-9F08-00AA002F954E") != std::wstring::npos ||
      path.find(L"RecycleBinFolder") != std::wstring::npos) {
      ShellExecuteW(nullptr, L"open", L"explorer.exe", L"::{645FF040-5081-101B-9F08-00AA002F954E}", nullptr, SW_SHOWNORMAL);
      return -1;
  }
  TabItem item;
  item.path = path;
  item.title = ShellUtils::GetDisplayName(path);
  item.iconIndex =
      ShellUtils::GetItemIconIndex(path, FILE_ATTRIBUTE_DIRECTORY, true);
  item.color = 0; // Default neutral unified styling
  item.layout = 0;
  item.panePaths = {path};
  item.activePaneIndex = 0;
  item.paneHistories = {{{path}, 0}};
  item.paneSelectedPaths = {{}};
  item.paneViewStates = {{0, L"", L"", {}}};

  int index = (int)m_tabs.size();
  m_tabs.push_back(item);

  RecalcLayout();
  if (activate) {
    SetActiveTab(index);
  } else {
    EnsureTabVisible(index);
    InvalidateRect(m_hwnd, nullptr, FALSE);
  }
  return index;
}

void TabBar::CloseTab(int index) {
  if (index < 0 || index >= (int)m_tabs.size())
    return;
  if (m_tabs[index].isPinned)
    return; // Cannot close pinned tab

  UpdateTooltip(-1);
  if (!m_tabs[index].path.empty()) {
    m_closedHistory.push_back(m_tabs[index].path);
  }
  m_tabs.erase(m_tabs.begin() + index);

  if (onTabClosed) {
    onTabClosed(index);
  }

  if (m_tabs.empty()) {
    AddTab(L"This PC", true);
    return;
  }

  if (m_activeIndex >= (int)m_tabs.size()) {
    m_activeIndex = (int)m_tabs.size() - 1;
  }

  RecalcLayout();
  if (onTabChanged) {
    onTabChanged(m_activeIndex);
  }
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseOtherTabs(int index) {
  if (index < 0 || index >= (int)m_tabs.size())
    return;
  TabItem keepTab = m_tabs[index];

  std::vector<TabItem> remaining;
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    if ((int)i == index || m_tabs[i].isPinned) {
      remaining.push_back(m_tabs[i]);
    } else if (!m_tabs[i].path.empty()) {
      m_closedHistory.push_back(m_tabs[i].path);
    }
  }

  m_tabs = std::move(remaining);
  m_activeIndex = 0;
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    if (m_tabs[i].path == keepTab.path) {
      m_activeIndex = (int)i;
      break;
    }
  }

  RecalcLayout();
  if (onTabChanged) {
    onTabChanged(m_activeIndex);
  }
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseTabsToLeft(int index) {
  if (index < 0 || index >= (int)m_tabs.size())
    return;

  std::vector<TabItem> remaining;
  TabItem activeTab = m_tabs[m_activeIndex];
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    if ((int)i >= index || m_tabs[i].isPinned) {
      remaining.push_back(m_tabs[i]);
    } else if (!m_tabs[i].path.empty()) {
      m_closedHistory.push_back(m_tabs[i].path);
    }
  }

  m_tabs = std::move(remaining);
  m_activeIndex = 0;
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    if (m_tabs[i].path == activeTab.path) {
      m_activeIndex = (int)i;
      break;
    }
  }

  RecalcLayout();
  if (onTabChanged) {
    onTabChanged(m_activeIndex);
  }
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseTabsToRight(int index) {
  if (index < 0 || index >= (int)m_tabs.size())
    return;

  std::vector<TabItem> remaining;
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    if ((int)i <= index || m_tabs[i].isPinned) {
      remaining.push_back(m_tabs[i]);
    } else if (!m_tabs[i].path.empty()) {
      m_closedHistory.push_back(m_tabs[i].path);
    }
  }

  m_tabs = std::move(remaining);
  if (m_activeIndex >= (int)m_tabs.size()) {
    m_activeIndex = (int)m_tabs.size() - 1;
  }

  RecalcLayout();
  if (onTabChanged) {
    onTabChanged(m_activeIndex);
  }
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::CloseAllTabs() {
  std::vector<TabItem> remaining;
  for (const auto &t : m_tabs) {
    if (t.isPinned) {
      remaining.push_back(t);
    } else if (!t.path.empty()) {
      m_closedHistory.push_back(t.path);
    }
  }

  if (remaining.empty()) {
    m_tabs.clear();
    AddTab(L"This PC", true);
    return;
  }

  m_tabs = std::move(remaining);
  m_activeIndex = 0;
  RecalcLayout();
  if (onTabChanged) {
    onTabChanged(m_activeIndex);
  }
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::ReopenClosedTab() {
  if (!m_closedHistory.empty()) {
    std::wstring lastPath = m_closedHistory.back();
    m_closedHistory.pop_back();
    AddTab(lastPath, true);
  }
}

void TabBar::NextTab() {
  if (m_tabs.empty())
    return;
  int next = (m_activeIndex + 1) % (int)m_tabs.size();
  SetActiveTab(next);
}

void TabBar::PrevTab() {
  if (m_tabs.empty())
    return;
  int prev = (m_activeIndex - 1 + (int)m_tabs.size()) % (int)m_tabs.size();
  SetActiveTab(prev);
}

void TabBar::TogglePinTab(int index) {
  if (index >= 0 && index < (int)m_tabs.size()) {
    m_tabs[index].isPinned = !m_tabs[index].isPinned;
    RecalcLayout();
    InvalidateRect(m_hwnd, nullptr, FALSE);
  }
}

bool TabBar::IsTabPinned(int index) const {
  if (index >= 0 && index < (int)m_tabs.size()) {
    return m_tabs[index].isPinned;
  }
  return false;
}

void TabBar::SetActiveTab(int index, bool force) {
  if (index < 0 || index >= (int)m_tabs.size())
    return;
  if (force || m_activeIndex != index) {
    m_activeIndex = index;
    EnsureTabVisible(m_activeIndex);
    if (onTabChanged) {
      onTabChanged(m_activeIndex);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
  } else {
    EnsureTabVisible(index);
  }
}

std::wstring TabBar::GetTabPath(int index) const {
  if (index >= 0 && index < (int)m_tabs.size()) {
    return m_tabs[index].path;
  }
  return L"";
}

void TabBar::SetTabPath(int index, const std::wstring &path) {
  if (index >= 0 && index < (int)m_tabs.size()) {
    m_tabs[index].path = path;
    m_tabs[index].title = ShellUtils::GetDisplayName(path);
    m_tabs[index].iconIndex =
        ShellUtils::GetItemIconIndex(path, FILE_ATTRIBUTE_DIRECTORY, true);
    if (m_tabs[index].panePaths.empty()) {
      m_tabs[index].panePaths.push_back(path);
    } else {
      if (m_tabs[index].activePaneIndex >= (int)m_tabs[index].panePaths.size()) {
        m_tabs[index].panePaths.resize(m_tabs[index].activePaneIndex + 1, path);
      }
      m_tabs[index].panePaths[m_tabs[index].activePaneIndex] = path;



    }
    if (m_hoverIndex == index) {
      UpdateTooltip(index);
    }
    RecalcLayout();
    InvalidateRect(m_hwnd, nullptr, FALSE);
  }
}

void TabBar::SetTabColor(int index, COLORREF color) {
  if (index >= 0 && index < (int)m_tabs.size()) {
    m_tabs[index].color = color;
    InvalidateRect(m_hwnd, nullptr, FALSE);
  }
}

COLORREF TabBar::GetTabColor(int index) const {
  if (index >= 0 && index < (int)m_tabs.size()) {
    return m_tabs[index].color;
  }
  return 0;
}

void TabBar::UpdateLayout(int width, int height) {
  if (m_hwnd) {
    MoveWindow(m_hwnd, 0, 0, width, height, TRUE);
    RecalcLayout();
  }
}

void TabBar::ShowTabContextMenu(int index, POINT ptScreen) {
  if (index < 0 || index >= (int)m_tabs.size())
    return;

  HMENU hMenu = CreatePopupMenu();
  bool pinned = m_tabs[index].isPinned;
  bool isZh = I18n::Get().IsChinese();

  AppendMenuW(hMenu, MF_STRING | (pinned ? MF_GRAYED : 0), 1,
              I18n::Get().T(StringId::TabMenuClose));
  AppendMenuW(hMenu, MF_STRING, 2, I18n::Get().T(StringId::TabMenuCloseOthers));
  AppendMenuW(hMenu, MF_STRING, 3, I18n::Get().T(StringId::TabMenuCloseRight));
  AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hMenu, MF_STRING, 4,
              pinned ? I18n::Get().T(StringId::TabMenuUnpin)
                     : I18n::Get().T(StringId::TabMenuPin));
  AppendMenuW(hMenu, MF_STRING, 5, I18n::Get().T(StringId::TabMenuDuplicate));

  // Tab Color submenu
  HMENU hColorSub = CreatePopupMenu();
  AppendMenuW(hColorSub, MF_STRING, 101, isZh ? L"天蓝色" : L"Azure Blue");
  AppendMenuW(hColorSub, MF_STRING, 102, isZh ? L"翡翠绿" : L"Emerald Green");
  AppendMenuW(hColorSub, MF_STRING, 103, isZh ? L"珊瑚红" : L"Coral Red");
  AppendMenuW(hColorSub, MF_STRING, 104, isZh ? L"高雅紫" : L"Purple");
  AppendMenuW(hColorSub, MF_STRING, 105, isZh ? L"活力橙" : L"Orange");
  AppendMenuW(hColorSub, MF_STRING, 106, isZh ? L"薄荷青" : L"Teal");
  AppendMenuW(hColorSub, MF_STRING, 107, isZh ? L"蔷薇粉" : L"Pink");
  AppendMenuW(hColorSub, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hColorSub, MF_STRING, 110,
              isZh ? L"随机新颜色(&R)" : L"Random Color(&R)");
  AppendMenuW(hColorSub, MF_STRING, 120,
              isZh ? L"自定义颜色(&O)..." : L"Choose Custom Color(&O)...");
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSub,
              isZh ? L"标签颜色(&C)" : L"Tab Color(&C)");

  AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hMenu, MF_STRING, 6, I18n::Get().T(StringId::TabMenuCopyPath));
  AppendMenuW(hMenu, MF_STRING, 7,
              I18n::Get().T(StringId::TabMenuOpenTerminal));

  int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x,
                           ptScreen.y, 0, m_hwnd, nullptr);
  DestroyMenu(hMenu);

  if (cmd >= 101 && cmd <= 107) {
    COLORREF colors[] = {RGB(0, 120, 215),  RGB(16, 137, 62), RGB(232, 17, 35),
                         RGB(136, 23, 152), RGB(255, 140, 0), RGB(0, 153, 188),
                         RGB(234, 0, 94)};
    SetTabColor(index, colors[cmd - 101]);
    return;
  } else if (cmd == 110) {
    SetTabColor(index,
                s_tabPalette[s_tabColorIndex++ %
                             (sizeof(s_tabPalette) / sizeof(s_tabPalette[0]))]);
    return;
  } else if (cmd == 120) {
    static COLORREF customColors[16] = {0};
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner = m_hwnd;
    cc.rgbResult = m_tabs[index].color;
    cc.lpCustColors = customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&cc)) {
      SetTabColor(index, cc.rgbResult);
    }
    return;
  }

  switch (cmd) {
  case 1:
    CloseTab(index);
    break;
  case 2:
    CloseOtherTabs(index);
    break;
  case 3:
    CloseTabsToRight(index);
    break;
  case 4:
    TogglePinTab(index);
    break;
  case 5:
    AddTab(m_tabs[index].path, true);
    break;
  case 6:
    ShellUtils::CopyToClipboard(m_tabs[index].path);
    break;
  case 7:
    if (onOpenTerminalRequested) {
      onOpenTerminalRequested(m_tabs[index].path);
    } else {
      ShellUtils::LaunchTerminal(m_tabs[index].path);
    }
    break;
  }
}

void TabBar::UpdateTooltip(int index) {
  if (!m_hwndTooltip)
    return;

  TOOLINFOW ti = {sizeof(ti)};
  ti.hwnd = m_hwnd;
  ti.uId = 1;

  if (index >= 0 && index < (int)m_tabs.size()) {
    const auto &tab = m_tabs[index];
    std::wstring text;
    if (tab.path.empty() || tab.path == L"This PC" || tab.path == L"此电脑" ||
        tab.path == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
      text = I18n::Get().T(StringId::ThisPC);
    } else {
      if (!tab.title.empty() && tab.title != tab.path) {
        text = tab.title + L"\n" + tab.path;
      } else {
        text = tab.path;
      }
    }
    m_tooltipText = text;

    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (LPWSTR)m_tooltipText.c_str();
    ti.rect = tab.rc;

    SendMessageW(m_hwndTooltip, TTM_DELTOOLW, 0, (LPARAM)&ti);
    SendMessageW(m_hwndTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
  } else {
    SendMessageW(m_hwndTooltip, TTM_DELTOOLW, 0, (LPARAM)&ti);
  }
}

void TabBar::EnsureTabVisible(int index) {
  if (!m_hwnd || index < 0 || index >= (int)m_tabs.size())
    return;

  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);
  int totalWidth = rcClient.right - rcClient.left;
  int capWidth = (!Config::Get().showMenuBar) ? Theme::Get().Scale(46 * 3, m_hwnd) : 0;
  int scrollBtnW = Theme::Get().Scale(22, m_hwnd);
  int rightReservedW = capWidth + (m_showScrollButtons
                           ? (scrollBtnW * 2 + Theme::Get().Scale(10, m_hwnd))
                           : 0);
  int visibleRight = totalWidth - rightReservedW;
  int startX = m_leftOffset + Theme::Get().Scale(4, m_hwnd);

  int minTabWidth = Theme::Get().Scale(90, m_hwnd);
  int spacing = Theme::Get().Scale(2, m_hwnd);

  int tabVirtLeft = startX + index * (minTabWidth + spacing);
  int tabVirtRight = tabVirtLeft + minTabWidth;

  if (tabVirtLeft - m_scrollOffset < startX) {
    m_scrollOffset = tabVirtLeft - startX;
  } else if (tabVirtRight - m_scrollOffset > visibleRight) {
    m_scrollOffset = tabVirtRight - visibleRight;
  }

  RecalcLayout();
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::ScrollBy(int delta) {
  m_scrollOffset += delta;
  RecalcLayout();
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::RecalcLayout() {
  if (!m_hwnd || m_tabs.empty())
    return;

  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);
  int totalWidth = rcClient.right - rcClient.left;
  int h = rcClient.bottom - rcClient.top;

  int newTabBtnWidth = Theme::Get().Scale(24, m_hwnd);
  int newTabBtnHeight = Theme::Get().Scale(24, m_hwnd);
  int scrollBtnW = Theme::Get().Scale(22, m_hwnd);
  int scrollBtnH = Theme::Get().Scale(24, m_hwnd);
  int spacing = Theme::Get().Scale(2, m_hwnd);
  int startX = m_leftOffset + Theme::Get().Scale(4, m_hwnd);
  int tabHeight = h - Theme::Get().Scale(4, m_hwnd);
  int closeSize = Theme::Get().Scale(16, m_hwnd);

  int maxTabWidth = Theme::Get().Scale(200, m_hwnd);
  int minTabWidth = Theme::Get().Scale(90, m_hwnd);

  int capWidth = (!Config::Get().showMenuBar) ? Theme::Get().Scale(46 * 3, m_hwnd) : 0;
  if (!Config::Get().showMenuBar) {
    int capBtnW = Theme::Get().Scale(46, m_hwnd);
    m_rcCloseBtn = {totalWidth - capBtnW, 0, totalWidth, h};
    m_rcMaxBtn   = {m_rcCloseBtn.left - capBtnW, 0, m_rcCloseBtn.left, h};
    m_rcMinBtn   = {m_rcMaxBtn.left - capBtnW, 0, m_rcMaxBtn.left, h};
  } else {
    SetRectEmpty(&m_rcMinBtn);
    SetRectEmpty(&m_rcMaxBtn);
    SetRectEmpty(&m_rcCloseBtn);
  }

  int tabCount = (int)m_tabs.size();
  int availableWidthNoScroll =
      totalWidth - capWidth - m_leftOffset - newTabBtnWidth - Theme::Get().Scale(12, m_hwnd);
  int totalRequiredWidth = tabCount * minTabWidth + (tabCount - 1) * spacing;

  if (totalRequiredWidth <= availableWidthNoScroll) {
    // Fits comfortably without scroll buttons
    m_showScrollButtons = false;
    m_scrollOffset = 0;
    m_rcScrollLeft = {0, 0, 0, 0};
    m_rcScrollRight = {0, 0, 0, 0};

    int tabWidth = maxTabWidth;
    if (tabCount * tabWidth + (tabCount - 1) * spacing >
        availableWidthNoScroll) {
      tabWidth = (availableWidthNoScroll - (tabCount - 1) * spacing) / tabCount;
      if (tabWidth < minTabWidth)
        tabWidth = minTabWidth;
    }

    int curX = startX;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
      m_tabs[i].rc.left = curX;
      m_tabs[i].rc.top = Theme::Get().Scale(4, m_hwnd);
      m_tabs[i].rc.right = curX + tabWidth;
      m_tabs[i].rc.bottom = m_tabs[i].rc.top + tabHeight;

      m_tabs[i].rcClose.right =
          m_tabs[i].rc.right - Theme::Get().Scale(6, m_hwnd);
      m_tabs[i].rcClose.left = m_tabs[i].rcClose.right - closeSize;
      m_tabs[i].rcClose.top = m_tabs[i].rc.top + (tabHeight - closeSize) / 2;
      m_tabs[i].rcClose.bottom = m_tabs[i].rcClose.top + closeSize;

      curX += tabWidth + spacing;
    }

    m_rcNewTab.left = curX + Theme::Get().Scale(4, m_hwnd);
    m_rcNewTab.top = Theme::Get().Scale(6, m_hwnd);
    m_rcNewTab.right = m_rcNewTab.left + newTabBtnWidth;
    m_rcNewTab.bottom = m_rcNewTab.top + newTabBtnHeight;
  } else {
    // Overflow! Only now show scroll buttons on the right.
    m_showScrollButtons = true;

    int btnY = Theme::Get().Scale(6, m_hwnd);
    int scrollRightEdge = totalWidth - capWidth - Theme::Get().Scale(4, m_hwnd);
    m_rcScrollRight.right = scrollRightEdge;
    m_rcScrollRight.left = m_rcScrollRight.right - scrollBtnW;
    m_rcScrollRight.top = btnY;
    m_rcScrollRight.bottom = btnY + scrollBtnH;

    m_rcScrollLeft.right = m_rcScrollRight.left - Theme::Get().Scale(2, m_hwnd);
    m_rcScrollLeft.left = m_rcScrollLeft.right - scrollBtnW;
    m_rcScrollLeft.top = btnY;
    m_rcScrollLeft.bottom = btnY + scrollBtnH;

    int rightReservedW = capWidth + scrollBtnW * 2 + Theme::Get().Scale(10, m_hwnd);
    int tabVisibleAreaW = totalWidth - rightReservedW;

    int tabWidth = minTabWidth;

    int totalContentWidth = startX + tabCount * (tabWidth + spacing) +
                            newTabBtnWidth + Theme::Get().Scale(8, m_hwnd);
    int maxScroll = totalContentWidth - tabVisibleAreaW;
    if (maxScroll < 0)
      maxScroll = 0;
    m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);

    int curX = startX - m_scrollOffset;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
      m_tabs[i].rc.left = curX;
      m_tabs[i].rc.top = Theme::Get().Scale(4, m_hwnd);
      m_tabs[i].rc.right = curX + tabWidth;
      m_tabs[i].rc.bottom = m_tabs[i].rc.top + tabHeight;

      m_tabs[i].rcClose.right =
          m_tabs[i].rc.right - Theme::Get().Scale(6, m_hwnd);
      m_tabs[i].rcClose.left = m_tabs[i].rcClose.right - closeSize;
      m_tabs[i].rcClose.top = m_tabs[i].rc.top + (tabHeight - closeSize) / 2;
      m_tabs[i].rcClose.bottom = m_tabs[i].rcClose.top + closeSize;

      curX += tabWidth + spacing;
    }

    m_rcNewTab.left = curX + Theme::Get().Scale(4, m_hwnd);
    m_rcNewTab.top = Theme::Get().Scale(6, m_hwnd);
    m_rcNewTab.right = m_rcNewTab.left + newTabBtnWidth;
    m_rcNewTab.bottom = m_rcNewTab.top + newTabBtnHeight;
  }
}

void TabBar::Draw(HDC hdc) {
  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);
  int w = rcClient.right - rcClient.left;
  int h = rcClient.bottom - rcClient.top;

  HDC hdcMem = CreateCompatibleDC(hdc);
  HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
  HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

  // Background
  HBRUSH hbrBg = Theme::Get().BrushBackground();
  FillRect(hdcMem, &rcClient, hbrBg);

  HIMAGELIST himlSmall = ShellUtils::GetSystemImageList(false);
  HFONT hFont = Theme::Get().GetUIFont(m_hwnd);
  HGDIOBJ oldFont = SelectObject(hdcMem, hFont);
  SetBkMode(hdcMem, TRANSPARENT);

  int startX = m_leftOffset + Theme::Get().Scale(4, m_hwnd);
  int capWidth = (!Config::Get().showMenuBar) ? Theme::Get().Scale(46 * 3, m_hwnd) : 0;
  int rightReservedW = capWidth + (m_showScrollButtons
                           ? (Theme::Get().Scale(22, m_hwnd) * 2 +
                              Theme::Get().Scale(10, m_hwnd))
                           : 0);
  int tabVisibleAreaW = w - rightReservedW;

  // Apply clipping region so tabs never draw over scroll buttons or menu button
  HRGN hRgnClip = CreateRectRgn(m_leftOffset, 0, tabVisibleAreaW, h);
  SelectClipRgn(hdcMem, hRgnClip);
  DeleteObject(hRgnClip);

  // Draw tabs
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    const auto &tab = m_tabs[i];
    if (tab.rc.right < 0 || tab.rc.left > tabVisibleAreaW)
      continue;

    bool isActive = ((int)i == m_activeIndex);
    bool isHover = ((int)i == m_hoverIndex);

    // Tab surface with Chrome-style rounded corners
    int r = Theme::Get().Scale(10, m_hwnd);
    RECT rcDraw = tab.rc;
    rcDraw.top += Theme::Get().Scale(2, m_hwnd);

    if (isActive) {
      // Active tab: elevated surface with rounded top corners and seamless bottom
      HBRUSH hbrTab = CreateSolidBrush(Theme::Get().ClrSurface());
      HGDIOBJ oldBrush = SelectObject(hdcMem, hbrTab);
      HGDIOBJ oldPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));
      RoundRect(hdcMem, rcDraw.left, rcDraw.top, rcDraw.right, rcDraw.bottom + r * 2, r * 2, r * 2);
      SelectObject(hdcMem, oldBrush);
      SelectObject(hdcMem, oldPen);
      DeleteObject(hbrTab);
    } else if (isHover) {
      // Inactive tab hover: rounded pill
      HBRUSH hbrHover = CreateSolidBrush(Theme::Get().ClrHover());
      HGDIOBJ oldBrush = SelectObject(hdcMem, hbrHover);
      HGDIOBJ oldPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));
      RoundRect(hdcMem, rcDraw.left + 2, rcDraw.top + 2, rcDraw.right - 2, rcDraw.bottom - 2, r, r);
      SelectObject(hdcMem, oldBrush);
      SelectObject(hdcMem, oldPen);
      DeleteObject(hbrHover);
    }

    // Separator line between inactive tabs (Chrome/Edge style)
    if (!isActive && ((int)i + 1 < (int)m_tabs.size()) && ((int)i + 1 != m_activeIndex) && !isHover && ((int)i + 1 != m_hoverIndex)) {
      int sepX = tab.rc.right - 1;
      int sepTop = tab.rc.top + Theme::Get().Scale(8, m_hwnd);
      int sepBottom = tab.rc.bottom - Theme::Get().Scale(8, m_hwnd);
      HPEN hPenSep = CreatePen(PS_SOLID, 1, Theme::Get().ClrBorder());
      HGDIOBJ oldPen = SelectObject(hdcMem, hPenSep);
      MoveToEx(hdcMem, sepX, sepTop, nullptr);
      LineTo(hdcMem, sepX, sepBottom);
      SelectObject(hdcMem, oldPen);
      DeleteObject(hPenSep);
    }

    // Active indicator line at bottom
    if (isActive) {
      RECT rcLine = tab.rc;
      rcLine.top = rcLine.bottom - Theme::Get().Scale(3, m_hwnd);
      COLORREF accentClr = (tab.color != 0) ? tab.color : Theme::Get().ClrAccent();
      HBRUSH hbrAccent = CreateSolidBrush(accentClr);
      FillRect(hdcMem, &rcLine, hbrAccent);
      DeleteObject(hbrAccent);
    }

    // Icon
    int iconY = tab.rc.top + (tab.rc.bottom - tab.rc.top - 16) / 2;
    int iconX = tab.rc.left + Theme::Get().Scale(8, m_hwnd);
    if (himlSmall) {
      ImageList_Draw(himlSmall, tab.iconIndex, hdcMem, iconX, iconY,
                     ILD_TRANSPARENT);
    }

    // Color tag dot for manually tagged tabs
    if (tab.color != 0 && !isActive) {
      int dotSize = Theme::Get().Scale(6, m_hwnd);
      int dotX = iconX - Theme::Get().Scale(3, m_hwnd);
      int dotY = tab.rc.top + (tab.rc.bottom - tab.rc.top - dotSize) / 2;
      RECT rcDot = {dotX, dotY, dotX + dotSize, dotY + dotSize};
      HBRUSH hbrDot = CreateSolidBrush(tab.color);
      HGDIOBJ oldBrush = SelectObject(hdcMem, hbrDot);
      HGDIOBJ oldPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));
      Ellipse(hdcMem, rcDot.left, rcDot.top, rcDot.right, rcDot.bottom);
      SelectObject(hdcMem, oldBrush);
      SelectObject(hdcMem, oldPen);
      DeleteObject(hbrDot);
    }

    // Title
    RECT rcText = tab.rc;
    rcText.left = iconX + 20;
    rcText.right = tab.isPinned
                       ? tab.rc.right - Theme::Get().Scale(6, m_hwnd)
                       : (tab.rcClose.left - Theme::Get().Scale(4, m_hwnd));
    SetTextColor(hdcMem, isActive ? Theme::Get().ClrTextPrimary()
                                  : Theme::Get().ClrTextSecondary());
    std::wstring displayTitle = tab.isPinned ? (L"📌 " + tab.title) : tab.title;
    DrawTextW(hdcMem, displayTitle.c_str(), -1, &rcText,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                  DT_NOPREFIX);

    // Close button '×' (only if not pinned)
    if (!tab.isPinned && (isHover || isActive)) {
      bool closeHover = (isHover && m_hoverClose);
      if (closeHover) {
        HBRUSH hbrCloseHover = CreateSolidBrush(Theme::Get().IsDarkMode() ? RGB(70, 70, 70) : RGB(218, 218, 218));
        HGDIOBJ oldBrush = SelectObject(hdcMem, hbrCloseHover);
        HGDIOBJ oldPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));
        RoundRect(hdcMem, tab.rcClose.left, tab.rcClose.top, tab.rcClose.right, tab.rcClose.bottom, 6, 6);
        SelectObject(hdcMem, oldBrush);
        SelectObject(hdcMem, oldPen);
        DeleteObject(hbrCloseHover);
        SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
      } else {
        SetTextColor(hdcMem, Theme::Get().ClrTextSecondary());
      }
      HFONT hCloseFont = Theme::Get().GetIconFont(10, m_hwnd);
      HGDIOBJ oldCloseF = SelectObject(hdcMem, hCloseFont);
      wchar_t closeGlyph[2] = {0xE8BB, 0};
      RECT rcClose = tab.rcClose;
      DrawTextW(hdcMem, closeGlyph, 1, &rcClose,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(hdcMem, oldCloseF);
      DeleteObject(hCloseFont);
    }
  }

  // Draw '+' new tab button
  if (m_rcNewTab.right >= 0 && m_rcNewTab.left <= tabVisibleAreaW) {
    if (m_hoverNewTab) {
      HBRUSH hbrNewTab = CreateSolidBrush(Theme::Get().ClrHover());
      HPEN hPen = Theme::Get().PenBorder();
      HGDIOBJ oldBr = SelectObject(hdcMem, hbrNewTab);
      HGDIOBJ oldP = SelectObject(hdcMem, hPen);
      int rPlus = Theme::Get().Scale(8, m_hwnd);
      RoundRect(hdcMem, m_rcNewTab.left, m_rcNewTab.top, m_rcNewTab.right,
                m_rcNewTab.bottom, rPlus * 2, rPlus * 2);
      SelectObject(hdcMem, oldP);
      SelectObject(hdcMem, oldBr);
      DeleteObject(hbrNewTab);
    }
    HFONT hPlusFont = Theme::Get().GetIconFont(12, m_hwnd);
    HGDIOBJ oldPlusF = SelectObject(hdcMem, hPlusFont);
    SetTextColor(hdcMem, m_hoverNewTab ? Theme::Get().ClrAccent()
                                       : Theme::Get().ClrTextPrimary());
    wchar_t plusGlyph[2] = {0xE710, 0};
    DrawTextW(hdcMem, plusGlyph, 1, &m_rcNewTab,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdcMem, oldPlusF);
    DeleteObject(hPlusFont);
  }

  // Restore clip region
  SelectClipRgn(hdcMem, nullptr);

  // Chrome window caption buttons (Minimize, Maximize/Restore, Close)
  if (!Config::Get().showMenuBar) {
    bool isMax = IsZoomed(m_hwndParent);
    HFONT hCapFont = Theme::Get().GetIconFont(10, m_hwnd);
    HGDIOBJ oldCapFont = SelectObject(hdcMem, hCapFont);

    // Minimize button
    if (m_hoverCaptionBtn == 1) {
      FillRect(hdcMem, &m_rcMinBtn, Theme::Get().BrushHover());
    }
    SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
    SetBkMode(hdcMem, TRANSPARENT);
    wchar_t minGlyph[2] = { (wchar_t)0xE921, 0 };
    DrawTextW(hdcMem, minGlyph, 1, &m_rcMinBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Maximize / Restore button
    if (m_hoverCaptionBtn == 2) {
      FillRect(hdcMem, &m_rcMaxBtn, Theme::Get().BrushHover());
    }
    SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
    wchar_t maxGlyph[2] = { isMax ? (wchar_t)0xE923 : (wchar_t)0xE922, 0 };
    DrawTextW(hdcMem, maxGlyph, 1, &m_rcMaxBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Close button
    if (m_hoverCaptionBtn == 3) {
      HBRUSH hbrClose = CreateSolidBrush(RGB(232, 17, 35));
      FillRect(hdcMem, &m_rcCloseBtn, hbrClose);
      DeleteObject(hbrClose);
      SetTextColor(hdcMem, RGB(255, 255, 255));
    } else {
      SetTextColor(hdcMem, Theme::Get().ClrTextPrimary());
    }
    wchar_t closeGlyph[2] = { (wchar_t)0xE8BB, 0 };
    DrawTextW(hdcMem, closeGlyph, 1, &m_rcCloseBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, oldCapFont);
    DeleteObject(hCapFont);
  }

  // Draw scroll buttons if overflowing
  if (m_showScrollButtons) {
    RECT rcScrollBg = {w - rightReservedW, 0, w - capWidth, h - 1};
    HBRUSH hbrBgRight = Theme::Get().BrushBackground();
    FillRect(hdcMem, &rcScrollBg, hbrBgRight);

    // Separator line before scroll buttons
    HPEN hPenSep = Theme::Get().PenBorder();
    HGDIOBJ oldPSep = SelectObject(hdcMem, hPenSep);
    MoveToEx(hdcMem, rcScrollBg.left, Theme::Get().Scale(6, m_hwnd), nullptr);
    LineTo(hdcMem, rcScrollBg.left, h - Theme::Get().Scale(6, m_hwnd));
    SelectObject(hdcMem, oldPSep);

    HFONT hIconFont = Theme::Get().GetIconFont(11, m_hwnd);
    HGDIOBJ oldIconF = SelectObject(hdcMem, hIconFont);

    // Left button '<'
    bool leftCanScroll = (m_scrollOffset > 0);
    if (m_hoverScrollLeft && leftCanScroll) {
      HBRUSH hbrHover = CreateSolidBrush(Theme::Get().ClrHover());
      FillRect(hdcMem, &m_rcScrollLeft, hbrHover);
      DeleteObject(hbrHover);
    }
    SetTextColor(hdcMem, leftCanScroll ? (m_hoverScrollLeft
                                              ? Theme::Get().ClrAccent()
                                              : Theme::Get().ClrTextPrimary())
                                       : Theme::Get().ClrTextSecondary());
    wchar_t leftGlyph[2] = {0xE76B, 0}; // ChevronLeft
    DrawTextW(hdcMem, leftGlyph, 1, &m_rcScrollLeft,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Right button '>'
    int minTabWidth = Theme::Get().Scale(90, m_hwnd);
    int spacing = Theme::Get().Scale(2, m_hwnd);
    int newTabBtnWidth = Theme::Get().Scale(24, m_hwnd);
    int totalContentWidth = startX +
                            (int)m_tabs.size() * (minTabWidth + spacing) +
                            newTabBtnWidth + Theme::Get().Scale(8, m_hwnd);
    int maxScroll = totalContentWidth - tabVisibleAreaW;
    bool rightCanScroll = (m_scrollOffset < maxScroll);
    if (m_hoverScrollRight && rightCanScroll) {
      HBRUSH hbrHover = CreateSolidBrush(Theme::Get().ClrHover());
      FillRect(hdcMem, &m_rcScrollRight, hbrHover);
      DeleteObject(hbrHover);
    }
    SetTextColor(hdcMem, rightCanScroll ? (m_hoverScrollRight
                                               ? Theme::Get().ClrAccent()
                                               : Theme::Get().ClrTextPrimary())
                                        : Theme::Get().ClrTextSecondary());
    wchar_t rightGlyph[2] = {0xE76C, 0}; // ChevronRight
    DrawTextW(hdcMem, rightGlyph, 1, &m_rcScrollRight,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, oldIconF);
    DeleteObject(hIconFont);
  }

  // Bottom border separator line
  RECT rcBorder = {0, h - 1, w, h};
  HBRUSH hbrBorder = Theme::Get().BrushBorder();
  FillRect(hdcMem, &rcBorder, hbrBorder);

  SelectObject(hdcMem, oldFont);
  BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

  SelectObject(hdcMem, oldBm);
  DeleteObject(hbm);
  DeleteDC(hdcMem);
}

LRESULT TabBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam,
                              LPARAM lParam) {
  switch (msg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    Draw(hdc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1;

  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&tme);

    int oldHover = m_hoverIndex;
    bool oldClose = m_hoverClose;
    bool oldNewTab = m_hoverNewTab;
    bool oldLeft = m_hoverScrollLeft;
    bool oldRight = m_hoverScrollRight;

    m_hoverIndex = -1;
    m_hoverClose = false;
    m_hoverNewTab = false;
    m_hoverScrollLeft = false;
    m_hoverScrollRight = false;

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int capWidth = (!Config::Get().showMenuBar) ? Theme::Get().Scale(46 * 3, hwnd) : 0;
    int rightReservedW =
        capWidth + (m_showScrollButtons
            ? (Theme::Get().Scale(22, hwnd) * 2 + Theme::Get().Scale(10, hwnd))
            : 0);
    int visibleRight = w - rightReservedW;

    int oldCapHover = m_hoverCaptionBtn;
    m_hoverCaptionBtn = 0;
    if (!Config::Get().showMenuBar) {
      if (PtInRect(&m_rcMinBtn, pt)) m_hoverCaptionBtn = 1;
      else if (PtInRect(&m_rcMaxBtn, pt)) m_hoverCaptionBtn = 2;
      else if (PtInRect(&m_rcCloseBtn, pt)) m_hoverCaptionBtn = 3;
    }
    if (oldCapHover != m_hoverCaptionBtn) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }

    if (m_showScrollButtons) {
      m_hoverScrollLeft = PtInRect(&m_rcScrollLeft, pt);
      m_hoverScrollRight = PtInRect(&m_rcScrollRight, pt);
    }

    if (pt.x < visibleRight) {
      m_hoverNewTab = PtInRect(&m_rcNewTab, pt);

      for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (PtInRect(&m_tabs[i].rc, pt)) {
          m_hoverIndex = (int)i;
          m_hoverClose = PtInRect(&m_tabs[i].rcClose, pt);
          break;
        }
      }
    }

    if (m_dragging && (wParam & MK_LBUTTON) && m_dragIndex >= 0) {
      if (!m_dragStarted) {
        if (abs(pt.x - m_dragStart.x) >= 4 || abs(pt.y - m_dragStart.y) >= 4) {
          m_dragStarted = true;
        }
      }

      if (m_dragStarted) {
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int h = rcClient.bottom - rcClient.top;
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);

        bool inTabBarZone = (pt.y >= -25 && pt.y <= h + 18);

        if (inTabBarZone) {
          // Inside TabBar: Hide split drop overlay
          if (onTabDragMove) {
            onTabDragMove(-1, -1, ptScreen);
          }

          // Horizontal tab reordering
          int targetIdx = m_dragIndex;
          for (int i = 0; i < (int)m_tabs.size(); ++i) {
            int midX = (m_tabs[i].rc.left + m_tabs[i].rc.right) / 2;
            if (i < m_dragIndex) {
              if (pt.x < midX) {
                targetIdx = i;
                break;
              }
            } else if (i > m_dragIndex) {
              if (pt.x > midX) {
                targetIdx = i;
              }
            }
          }

          if (targetIdx != m_dragIndex && targetIdx >= 0 && targetIdx < (int)m_tabs.size()) {
            TabItem item = std::move(m_tabs[m_dragIndex]);
            m_tabs.erase(m_tabs.begin() + m_dragIndex);
            m_tabs.insert(m_tabs.begin() + targetIdx, std::move(item));

            if (m_activeIndex == m_dragIndex) {
              m_activeIndex = targetIdx;
            } else if (m_dragIndex < m_activeIndex && targetIdx >= m_activeIndex) {
              m_activeIndex--;
            } else if (m_dragIndex > m_activeIndex && targetIdx <= m_activeIndex) {
              m_activeIndex++;
            }

            m_dragIndex = targetIdx;
            RecalcLayout();
            if (onTabOrderChanged) {
              onTabOrderChanged();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
          }

          // Track hover over another tab for potential host merge target
          int curHoverTab = -1;
          for (int i = 0; i < (int)m_tabs.size(); ++i) {
            if (pt.x >= m_tabs[i].rc.left && pt.x <= m_tabs[i].rc.right) {
              curHoverTab = i;
              break;
            }
          }

          if (curHoverTab != -1 && curHoverTab != m_dragIndex) {
            m_targetHostTab = curHoverTab;
            if (m_hoverTabCandidate != curHoverTab) {
              m_hoverTabCandidate = curHoverTab;
              m_hoverTabTick = GetTickCount();
            } else if (GetTickCount() - m_hoverTabTick > 450) {
              if (m_activeIndex != curHoverTab) {
                SetActiveTab(curHoverTab);
              }
            }
          }
        } else if (pt.y > h + 18) {
          // Dragged downward into file view area: Split/Merge mode!
          int hostTab = m_targetHostTab;
          if (hostTab < 0 || hostTab >= (int)m_tabs.size() || hostTab == m_dragIndex) {
            if (m_prevActiveIndex >= 0 && m_prevActiveIndex < (int)m_tabs.size() && m_prevActiveIndex != m_dragIndex) {
              hostTab = m_prevActiveIndex;
            } else {
              hostTab = (m_dragIndex > 0) ? (m_dragIndex - 1) : ((int)m_tabs.size() > 1 ? 1 : 0);
            }
          }
          if (onTabDragMove) {
            onTabDragMove(m_dragIndex, hostTab, ptScreen);
          }
        } else {
          // Above tab bar or out of bounds
          if (onTabDragMove) {
            onTabDragMove(-1, -1, ptScreen);
          }
        }
      }
    }

    // Update hover tooltip
    if (m_dragging) {
      UpdateTooltip(-1);
    } else if (oldHover != m_hoverIndex) {
      UpdateTooltip(m_hoverIndex);
    }

    if (oldHover != m_hoverIndex || oldClose != m_hoverClose ||
        oldNewTab != m_hoverNewTab || oldLeft != m_hoverScrollLeft ||
        oldRight != m_hoverScrollRight) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
  }

  case WM_MOUSELEAVE:
    m_hoverIndex = -1;
    m_hoverClose = false;
    m_hoverNewTab = false;
    m_hoverScrollLeft = false;
    m_hoverScrollRight = false;
    m_hoverCaptionBtn = 0;
    if (!m_dragging) {
      UpdateTooltip(-1);
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;

  case WM_MOUSEWHEEL: {
    if (!m_showScrollButtons)
      return 0;
    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int step = Theme::Get().Scale(60, hwnd);
    if (delta < 0) {
      ScrollBy(step);
    } else {
      ScrollBy(-step);
    }
    return 0;
  }

  case WM_LBUTTONDOWN: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

    if (!Config::Get().showMenuBar) {
      if (PtInRect(&m_rcMinBtn, pt)) {
        ShowWindow(m_hwndParent, SW_MINIMIZE);
        return 0;
      }
      if (PtInRect(&m_rcMaxBtn, pt)) {
        ShowWindow(m_hwndParent, IsZoomed(m_hwndParent) ? SW_RESTORE : SW_MAXIMIZE);
        return 0;
      }
      if (PtInRect(&m_rcCloseBtn, pt)) {
        PostMessage(m_hwndParent, WM_CLOSE, 0, 0);
        return 0;
      }
    }

    if (m_showScrollButtons) {
      if (PtInRect(&m_rcScrollLeft, pt)) {
        ScrollBy(-Theme::Get().Scale(100, hwnd));
        return 0;
      }
      if (PtInRect(&m_rcScrollRight, pt)) {
        ScrollBy(Theme::Get().Scale(100, hwnd));
        return 0;
      }
    }

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int capWidth = (!Config::Get().showMenuBar) ? Theme::Get().Scale(46 * 3, hwnd) : 0;
    int rightReservedW =
        capWidth + (m_showScrollButtons
            ? (Theme::Get().Scale(22, hwnd) * 2 + Theme::Get().Scale(10, hwnd))
            : 0);
    int visibleRight = w - rightReservedW;

    if (pt.x < visibleRight) {
      if (PtInRect(&m_rcNewTab, pt)) {
        if (onNewTabRequested)
          onNewTabRequested();
        return 0;
      }

      for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (PtInRect(&m_tabs[i].rc, pt)) {
          if (PtInRect(&m_tabs[i].rcClose, pt)) {
            CloseTab((int)i);
            return 0;
          } else {
            m_prevActiveIndex = m_activeIndex;
            SetActiveTab((int)i);
            m_dragging = true;
            m_dragStarted = false;
            m_dragIndex = (int)i;
            m_targetHostTab = -1;
            m_hoverTabCandidate = -1;
            m_hoverTabTick = 0;
            m_dragStart = pt;
            SetCapture(hwnd);
            return 0;
          }
        }
      }
    }

    // Clicked empty area of TabBar -> native window dragging!
    if (!Config::Get().showMenuBar && m_hoverCaptionBtn == 0) {
      ReleaseCapture();
      SendMessage(m_hwndParent, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      return 0;
    }
    return 0;
  }

  case WM_LBUTTONUP: {
    if (m_dragging) {
      int dragIdx = m_dragIndex;
      int hostTab = m_targetHostTab;
      bool wasStarted = m_dragStarted;
      m_dragging = false;
      m_dragStarted = false;
      m_dragIndex = -1;
      m_targetHostTab = -1;
      m_hoverTabCandidate = -1;
      ReleaseCapture();

      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      POINT ptScreen = pt;
      ClientToScreen(hwnd, &ptScreen);
      RECT rcClient;
      GetClientRect(hwnd, &rcClient);
      int h = rcClient.bottom - rcClient.top;

      if (wasStarted && pt.y > h + 18) {
        if (hostTab < 0 || hostTab >= (int)m_tabs.size() || hostTab == dragIdx) {
          if (m_prevActiveIndex >= 0 && m_prevActiveIndex < (int)m_tabs.size() && m_prevActiveIndex != dragIdx) {
            hostTab = m_prevActiveIndex;
          } else {
            hostTab = (dragIdx > 0) ? (dragIdx - 1) : ((int)m_tabs.size() > 1 ? 1 : 0);
          }
        }
        if (onTabDragEnd) {
          onTabDragEnd(dragIdx, hostTab, ptScreen);
        }
      } else {
        if (onTabDragMove) {
          onTabDragMove(-1, -1, ptScreen);
        }
      }
      InvalidateRect(hwnd, nullptr, FALSE);
    } else {
      m_dragging = false;
      m_dragStarted = false;
      m_dragIndex = -1;
      m_targetHostTab = -1;
      m_hoverTabCandidate = -1;
      ReleaseCapture();
    }
    return 0;
  }
  case WM_CAPTURECHANGED: {
    if (m_dragging) {
      m_dragging = false;
      m_dragStarted = false;
      m_dragIndex = -1;
      m_targetHostTab = -1;
      m_hoverTabCandidate = -1;
      if (onTabDragMove) {
        POINT ptScreen = {0, 0};
        GetCursorPos(&ptScreen);
        onTabDragMove(-1, -1, ptScreen);
      }
    }
    return 0;
  }

  case WM_RBUTTONUP: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    for (size_t i = 0; i < m_tabs.size(); ++i) {
      if (PtInRect(&m_tabs[i].rc, pt)) {
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);
        ShowTabContextMenu((int)i, ptScreen);
        return 0;
      }
    }
    // Clicked on empty space in TabBar: show TabBar background context menu!
    POINT ptScreen = pt;
    ClientToScreen(hwnd, &ptScreen);
    HMENU hMenu = CreatePopupMenu();
    bool isZh = I18n::Get().IsChinese();
    AppendMenuW(hMenu, MF_STRING, 1,
                isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
    AppendMenuW(hMenu, MF_STRING | (m_closedHistory.empty() ? MF_GRAYED : 0), 2,
                isZh ? L"恢复关闭的标签页(&R)\tCtrl+Shift+T"
                     : L"Reopen Closed Tab(&R)\tCtrl+Shift+T");
    AppendMenuW(hMenu, MF_STRING, 3,
                isZh ? L"关闭其他标签页(&O)" : L"Close Other Tabs(&O)");
    AppendMenuW(hMenu, MF_STRING, 4,
                isZh ? L"关闭当前标签页(&W)\tCtrl+W"
                     : L"Close Current Tab(&W)\tCtrl+W");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 5,
                isZh ? L"打开命令CMD(&C)" : L"Open CMD(&C)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 6,
                isZh ? L"NeoMDIE 设置(&S)..." : L"NeoMDIE Settings(&S)...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x,
                             ptScreen.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case 1:
      AddTab(L"This PC", true);
      break;
    case 2:
      ReopenClosedTab();
      break;
    case 3:
      CloseOtherTabs(m_activeIndex);
      break;
    case 4:
      CloseTab(m_activeIndex);
      break;
    case 5:
      if (m_activeIndex >= 0 && m_activeIndex < (int)m_tabs.size()) {
        ShellUtils::LaunchTerminal(m_tabs[m_activeIndex].path);
      }
      break;
    case 6:
      PostMessageW(GetAncestor(hwnd, GA_ROOT), WM_COMMAND, MAKEWPARAM(40056, 0),
                   0); // ID_TOOLS_SETTINGS
      break;
    }
    return 0;
  }

  case WM_MBUTTONUP: {
    // Middle click to close tab
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    for (size_t i = 0; i < m_tabs.size(); ++i) {
      if (PtInRect(&m_tabs[i].rc, pt)) {
        CloseTab((int)i);
        return 0;
      }
    }
    return 0;
  }

  case WM_LBUTTONDBLCLK: {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (!Config::Get().showMenuBar) {
      bool onTab = false;
      for (const auto& t : m_tabs) {
        if (PtInRect(&t.rc, pt)) { onTab = true; break; }
      }
      if (!onTab && !PtInRect(&m_rcNewTab, pt) && !PtInRect(&m_rcMinBtn, pt) && !PtInRect(&m_rcMaxBtn, pt) && !PtInRect(&m_rcCloseBtn, pt)) {
        ShowWindow(m_hwndParent, IsZoomed(m_hwndParent) ? SW_RESTORE : SW_MAXIMIZE);
        return 0;
      }
    }
    bool onTab = false;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
      if (PtInRect(&m_tabs[i].rc, pt)) {
        onTab = true;
        break;
      }
    }
    if (!onTab && onNewTabRequested) {
      onNewTabRequested();
    }
    return 0;
  }
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TabBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                 LPARAM lParam) {
  TabBar *pThis = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
    pThis = (TabBar *)cs->lpCreateParams;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    pThis->m_hwnd = hwnd;
  } else {
    pThis = (TabBar *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  }

  if (pThis) {
    return pThis->HandleMessage(hwnd, msg, wParam, lParam);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
