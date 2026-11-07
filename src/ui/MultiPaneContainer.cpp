#include "MultiPaneContainer.h"
#include "..\core\I18n.h"
#include "Theme.h"


static const wchar_t *MULTIPANE_CLASS = L"NeoMDIE_MultiPaneContainer";
static const wchar_t *PANEHEADER_CLASS = L"NeoMDIE_PaneHeader";

MultiPaneContainer::MultiPaneContainer() {}

MultiPaneContainer::~MultiPaneContainer() {}

HWND MultiPaneContainer::Create(HWND hwndParent, int id) {
  HINSTANCE hInst = GetModuleHandleW(nullptr);

  // Register Container class
  WNDCLASSEXW wc = {sizeof(wc)};
  if (!GetClassInfoExW(hInst, MULTIPANE_CLASS, &wc)) {
    wc.lpfnWndProc = MultiPaneContainer::WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = MULTIPANE_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
  }

  // Register Pane Header class
  WNDCLASSEXW wch = {sizeof(wch)};
  if (!GetClassInfoExW(hInst, PANEHEADER_CLASS, &wch)) {
    wch.style = CS_HREDRAW | CS_VREDRAW;
    wch.lpfnWndProc = MultiPaneContainer::HeaderWndProc;
    wch.hInstance = hInst;
    wch.lpszClassName = PANEHEADER_CLASS;
    wch.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wch);
  }

  m_hwnd = CreateWindowExW(
      0, MULTIPANE_CLASS, L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 800, 500,
      hwndParent, (HMENU)(INT_PTR)id, hInst, this);

  for (int i = 0; i < 4; ++i) {
    // 1. Create Header
    m_hHeaders[i] = CreateWindowExW(
        0, PANEHEADER_CLASS, L"",
        WS_CHILD | (i == 0 ? WS_VISIBLE : 0) | WS_CLIPSIBLINGS, 0, 0, 100, 26,
        m_hwnd, (HMENU)(INT_PTR)(2001 + i), hInst, this);
    SetWindowLongPtrW(m_hHeaders[i], GWLP_USERDATA, (LONG_PTR)this);

    // 2. Create ListView
    m_panes[i].Create(m_hwnd, 1001 + i);
    SetWindowSubclass(m_panes[i].GetHwnd(),
                      MultiPaneContainer::PaneSubclassProc, (UINT_PTR)i,
                      (DWORD_PTR)this);

    // Callbacks
    m_panes[i].onPathChanged = [this, i](const std::wstring &newPath) {
      UpdatePaneHeader(i);
      if (i == m_activePane && onActivePaneChanged) {
        onActivePaneChanged(m_activePane, newPath);
      }
    };

    m_panes[i].onGestureAction = [this, i](GestureAction act) {
      SetActivePane(i);
      if (onGestureAction) {
        onGestureAction(act);
      }
    };

    m_panes[i].onCustomCommand = [this](int cmd, const std::wstring &path) {
      if (onCustomCommand) {
        onCustomCommand(cmd, path);
      }
    };

    if (i > 0) {
      ShowWindow(m_panes[i].GetHwnd(), SW_HIDE);
    }
  }

  return m_hwnd;
}

int MultiPaneContainer::GetPaneCount() const {
  switch (m_layout) {
  case PaneLayout::Single:
    return 1;
  case PaneLayout::DualVertical:
  case PaneLayout::DualHorizontal:
    return 2;
  case PaneLayout::ThreeLeft:
    return 3;
  case PaneLayout::Quad:
    return 4;
  }
  return 1;
}

FileListView *MultiPaneContainer::GetActivePane() {
  return &m_panes[m_activePane];
}

void MultiPaneContainer::SetActivePane(int index, bool force) {
  int maxPanes = GetPaneCount();
  index = std::clamp(index, 0, maxPanes - 1);
  // Always apply active index to ensure full visual and focus sync
  if (force || m_activePane != index) {
    m_activePane = index;
    for (int i = 0; i < 4; ++i) {
      if (m_hHeaders[i]) {
        InvalidateRect(m_hHeaders[i], nullptr, TRUE);
      }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
    if (onActivePaneChanged) {
      onActivePaneChanged(m_activePane, m_panes[m_activePane].GetCurrentPath());
    }
  }
}

void MultiPaneContainer::CycleActivePane() {
  int count = GetPaneCount();
  if (count > 1) {
    SetActivePane((m_activePane + 1) % count);
    SetFocus(m_panes[m_activePane].GetHwnd());
  }
}

void MultiPaneContainer::ClosePane(int index) {
  int count = GetPaneCount();
  if (count <= 1 || index < 0 || index >= count) {
    return;
  }

  m_maximizedPane = -1;

  int nextActive = m_activePane;
  if (index == m_activePane) {
    if (nextActive >= count - 1) {
      nextActive = count - 2;
    }
  } else if (index < m_activePane) {
    nextActive--;
  }

  if (m_layout == PaneLayout::Quad) {
    // 4 -> 3
    for (int i = index; i < 3; ++i) {
      m_panes[i].NavigateTo(m_panes[i + 1].GetCurrentPath(), false);
      m_panes[i].SetHistory(m_panes[i + 1].GetHistory(), m_panes[i + 1].GetHistoryIndex());
      m_panes[i].RestoreViewState(m_panes[i + 1].GetViewState());
    }
    SetLayout(PaneLayout::ThreeLeft);
  } else if (m_layout == PaneLayout::ThreeLeft) {
    // 3 -> 2
    for (int i = index; i < 2; ++i) {
      m_panes[i].NavigateTo(m_panes[i + 1].GetCurrentPath(), false);
      m_panes[i].SetHistory(m_panes[i + 1].GetHistory(), m_panes[i + 1].GetHistoryIndex());
      m_panes[i].RestoreViewState(m_panes[i + 1].GetViewState());
    }
    SetLayout(PaneLayout::DualVertical);
  } else if (m_layout == PaneLayout::DualVertical ||
             m_layout == PaneLayout::DualHorizontal) {
    // 2 -> 1
    if (index == 0) {
      m_panes[0].NavigateTo(m_panes[1].GetCurrentPath(), false);
      m_panes[0].SetHistory(m_panes[1].GetHistory(), m_panes[1].GetHistoryIndex());
      m_panes[0].RestoreViewState(m_panes[1].GetViewState());
    }
    SetLayout(PaneLayout::Single);
  }

  nextActive = std::clamp(nextActive, 0, GetPaneCount() - 1);
  SetActivePane(nextActive, true);
  SetFocus(m_panes[nextActive].GetHwnd());
}

void MultiPaneContainer::MinimizePane(int index) {
  if (GetPaneCount() > 1) {
    ClosePane(index);
  } else {
    HWND hMain = GetAncestor(m_hwnd, GA_ROOT);
    if (hMain) {
      ShowWindow(hMain, SW_MINIMIZE);
    }
  }
}

void MultiPaneContainer::ToggleMaximizePane(int index) {
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
}

void MultiPaneContainer::UpdatePaneHeader(int index) {
  if (index >= 0 && index < 4 && m_hHeaders[index]) {
    InvalidateRect(m_hHeaders[index], nullptr, TRUE);
  }
}

void MultiPaneContainer::SetLayout(PaneLayout layout) {
  m_layout = layout;
  m_maximizedPane = -1; // Explicit layout change clears temporary maximize
  int count = GetPaneCount();


  for (int i = 0; i < 4; ++i) {
    bool shouldShow = (i < count);
    ShowWindow(m_panes[i].GetHwnd(), shouldShow ? SW_SHOW : SW_HIDE);
    if (m_hHeaders[i]) {
      bool showHdr = shouldShow && (count > 1);
      ShowWindow(m_hHeaders[i], showHdr ? SW_SHOW : SW_HIDE);
      UpdatePaneHeader(i);
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
}

void MultiPaneContainer::ApplyTheme() {
  if (m_hwnd)
    Theme::Get().ApplyToWindow(m_hwnd);
  for (int i = 0; i < 4; ++i) {
    m_panes[i].ApplyTheme();
    if (m_hHeaders[i]) {
      Theme::Get().ApplyToWindow(m_hHeaders[i]);
      InvalidateRect(m_hHeaders[i], nullptr, TRUE);
    }
  }
}

void MultiPaneContainer::SetShowHidden(bool show) {
  for (int i = 0; i < 4; ++i) {
    m_panes[i].SetShowHidden(show);
  }
}

void MultiPaneContainer::SetCalcFolderSize(bool calc) {
  for (int i = 0; i < 4; ++i) {
    m_panes[i].SetCalcFolderSize(calc);
  }
}

void MultiPaneContainer::UpdateLayout(int x, int y, int width, int height) {
  if (m_hwnd) {
    MoveWindow(m_hwnd, x, y, width, height, TRUE);
    LayoutChildren();
  }
}

void MultiPaneContainer::LayoutChildren() {
  if (!m_hwnd)
    return;

  RECT rc;
  GetClientRect(m_hwnd, &rc);
  int totalW = rc.right - rc.left;
  int totalH = rc.bottom - rc.top;

  int splitW = Theme::Get().Scale(6, m_hwnd);
  int count = GetPaneCount();
  int headerH = (count > 1) ? Theme::Get().Scale(26, m_hwnd) : 0;
  int borderPad = 1;

  auto positionPane = [&](int idx, int x, int y, int w, int h) {
    if (idx >= 0 && idx < 4) {
      ShowWindow(m_panes[idx].GetHwnd(), SW_SHOW);
      if (count > 1 && m_hHeaders[idx]) {
        ShowWindow(m_hHeaders[idx], SW_SHOW);
        SetWindowPos(m_hHeaders[idx], HWND_TOP, x, y, w, headerH,
                     SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        InvalidateRect(m_hHeaders[idx], nullptr, TRUE);
      } else if (m_hHeaders[idx]) {
        ShowWindow(m_hHeaders[idx], SW_HIDE);
      }
      m_panes[idx].UpdateLayout(x, y + headerH, w, h - headerH);
      InvalidateRect(m_panes[idx].GetHwnd(), nullptr, TRUE);
    }
  };

  if (m_maximizedPane >= 0 && m_maximizedPane < GetPaneCount()) {
    for (int i = 0; i < 4; ++i) {
      if (i == m_maximizedPane) {
        ShowWindow(m_panes[i].GetHwnd(), SW_SHOW);
        if (m_hHeaders[i])
          ShowWindow(m_hHeaders[i], SW_SHOW);
        positionPane(i, borderPad, borderPad, totalW - borderPad * 2,
                     totalH - borderPad * 2);
      } else {
        ShowWindow(m_panes[i].GetHwnd(), SW_HIDE);
        if (m_hHeaders[i])
          ShowWindow(m_hHeaders[i], SW_HIDE);
      }
    }
    return;
  }

  // Ensure inactive panes past count are hidden
  for (int i = count; i < 4; ++i) {
    ShowWindow(m_panes[i].GetHwnd(), SW_HIDE);
    if (m_hHeaders[i])
      ShowWindow(m_hHeaders[i], SW_HIDE);
  }

  switch (m_layout) {
  case PaneLayout::Single:
    positionPane(0, borderPad, borderPad, totalW - borderPad * 2,
                 totalH - borderPad * 2);
    break;

  case PaneLayout::DualVertical: {
    int minW = Theme::Get().Scale(90, m_hwnd);
    int w1 = (totalW * m_splitH) / 100 - splitW / 2;
    if (w1 < minW)
      w1 = minW;
    if (totalW - w1 - splitW < minW)
      w1 = totalW - minW - splitW;
    int w2 = totalW - w1 - splitW;
    if (w2 < 0)
      w2 = 0;
    positionPane(0, borderPad, borderPad, w1 - borderPad * 2,
                 totalH - borderPad * 2);
    positionPane(1, w1 + splitW + borderPad, borderPad, w2 - borderPad * 2,
                 totalH - borderPad * 2);
    break;
  }

  case PaneLayout::DualHorizontal: {
    int h1 = (totalH * m_splitV) / 100 - splitW / 2;
    int h2 = totalH - h1 - splitW;
    positionPane(0, borderPad, borderPad, totalW - borderPad * 2,
                 h1 - borderPad * 2);
    positionPane(1, borderPad, h1 + splitW + borderPad, totalW - borderPad * 2,
                 h2 - borderPad * 2);
    break;
  }

  case PaneLayout::Quad: {
    int w1 = (totalW * m_splitH) / 100 - splitW / 2;
    int w2 = totalW - w1 - splitW;
    int h1 = (totalH * m_splitV) / 100 - splitW / 2;
    int h2 = totalH - h1 - splitW;

    positionPane(0, borderPad, borderPad, w1 - borderPad * 2,
                 h1 - borderPad * 2);
    positionPane(1, w1 + splitW + borderPad, borderPad, w2 - borderPad * 2,
                 h1 - borderPad * 2);
    positionPane(2, borderPad, h1 + splitW + borderPad, w1 - borderPad * 2,
                 h2 - borderPad * 2);
    positionPane(3, w1 + splitW + borderPad, h1 + splitW + borderPad,
                 w2 - borderPad * 2, h2 - borderPad * 2);
    break;
  }

  case PaneLayout::ThreeLeft: {
    int w1 = (totalW * m_splitH) / 100 - splitW / 2;
    int w2 = totalW - w1 - splitW;
    int h1 = (totalH * m_splitV) / 100 - splitW / 2;
    int h2 = totalH - h1 - splitW;

    positionPane(0, borderPad, borderPad, w1 - borderPad * 2,
                 totalH - borderPad * 2);
    positionPane(1, w1 + splitW + borderPad, borderPad, w2 - borderPad * 2,
                 h1 - borderPad * 2);
    positionPane(2, w1 + splitW + borderPad, h1 + splitW + borderPad,
                 w2 - borderPad * 2, h2 - borderPad * 2);
    break;
  }
  }
}

static int s_headerHoverPane = -1;
static int s_headerHoverBtn = 0; // 0: none, 1: min, 2: max, 3: close

LRESULT MultiPaneContainer::HandleHeaderMessage(HWND hwnd, UINT msg,
                                                WPARAM wParam, LPARAM lParam,
                                                int paneIdx) {
  switch (msg) {
  case WM_SIZE:
  case WM_WINDOWPOSCHANGED:
    InvalidateRect(hwnd, nullptr, TRUE);
    return DefWindowProcW(hwnd, msg, wParam, lParam);

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdcPaint = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      EndPaint(hwnd, &ps);
      return 0;
    }

    // Double-buffered rendering to eliminate all flicker and ghosting
    HDC hdc = CreateCompatibleDC(hdcPaint);
    HBITMAP hbm = CreateCompatibleBitmap(hdcPaint, w, h);
    HGDIOBJ oldBm = SelectObject(hdc, hbm);

    bool isActive = (paneIdx == m_activePane);

    // Header Background
    HBRUSH hbrBg = isActive ? CreateSolidBrush(Theme::Get().ClrAccent())
                            : Theme::Get().BrushSurface();
    FillRect(hdc, &rc, hbrBg);
    if (isActive) {
      DeleteObject(hbrBg);
    }

    // Bottom border line
    HPEN hPen = Theme::Get().PenBorder();
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    MoveToEx(hdc, 0, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.right, rc.bottom - 1);
    SelectObject(hdc, oldPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc,
                 isActive ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary());
    HFONT hFont = Theme::Get().GetUIFont(hwnd);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    int btnW = Theme::Get().Scale(24, hwnd);
    RECT rcClose = {rc.right - btnW, 0, rc.right, rc.bottom - 1};
    RECT rcMax = {rc.right - btnW * 2, 0, rc.right - btnW, rc.bottom - 1};

    // Button hover effects
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
    if (path.empty())
      path = L"This PC";
    std::wstring titleText = L" [ " + path + L" ]";

    RECT rcText = {rc.left + Theme::Get().Scale(6, hwnd), 0,
                   rc.right - btnW * 2 - Theme::Get().Scale(4, hwnd),
                   rc.bottom - 1};
    DrawTextW(hdc, titleText.c_str(), -1, &rcText,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Header buttons: Maximize/Restore, Close
    COLORREF btnClr =
        isActive ? RGB(255, 255, 255) : Theme::Get().ClrTextPrimary();
    HFONT hIconFont = Theme::Get().GetIconFont(11, hwnd);
    HGDIOBJ oldIconFont = SelectObject(hdc, hIconFont);

    // Maximize / Restore
    SetTextColor(hdc, btnClr);
    wchar_t maxGlyph[2] = {
        (m_maximizedPane == paneIdx) ? (wchar_t)0xE923 : (wchar_t)0xE922, 0};
    DrawTextW(hdc, maxGlyph, 1, &rcMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Close
    if (GetPaneCount() > 1) {
      if (s_headerHoverPane == paneIdx && s_headerHoverBtn == 2) {
        SetTextColor(hdc, RGB(255, 255, 255));
      } else {
        SetTextColor(hdc, btnClr);
      }
      wchar_t closeGlyph[2] = {0xE8BB, 0};
      DrawTextW(hdc, closeGlyph, 1, &rcClose,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldIconFont);
    DeleteObject(hIconFont);

    SelectObject(hdc, oldFont);

    BitBlt(hdcPaint, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdc);

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_MOUSEMOVE: {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int x = GET_X_LPARAM(lParam);
    int btnW = Theme::Get().Scale(24, hwnd);
    int newHover = 0;
    if (x >= rc.right - btnW && x <= rc.right) {
      newHover = 2; // Close
    } else if (x >= rc.right - btnW * 2 && x < rc.right - btnW) {
      newHover = 1; // Maximize / Restore
    }
    if (s_headerHoverPane != paneIdx || s_headerHoverBtn != newHover) {
      s_headerHoverPane = paneIdx;
      s_headerHoverBtn = newHover;
      TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&tme);
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
  }

  case WM_MOUSELEAVE: {
    s_headerHoverPane = -1;
    s_headerHoverBtn = 0;
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  }

  case WM_LBUTTONDOWN: {
    SetActivePane(paneIdx);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int x = GET_X_LPARAM(lParam);
    int btnW = Theme::Get().Scale(24, hwnd);

    if (x >= rc.right - btnW && x <= rc.right) {
      if (GetPaneCount() > 1) {
        ClosePane(paneIdx);
      }
      return 0;
    } else if (x >= rc.right - btnW * 2 && x < rc.right - btnW) {
      ToggleMaximizePane(paneIdx);
      return 0;
    }
    return 0;
  }

  case WM_ERASEBKGND:
    return 1;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MultiPaneContainer::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                          LPARAM lParam) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int totalW = rc.right - rc.left;
  int totalH = rc.bottom - rc.top;
  int splitW = Theme::Get().Scale(6, hwnd);

  switch (msg) {
  case WM_SIZE:
    LayoutChildren();
    InvalidateRect(hwnd, nullptr, TRUE);
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HBRUSH hbrBg = Theme::Get().BrushBackground();
    FillRect(hdc, &rc, hbrBg);
    HBRUSH hbrSplitter = Theme::Get().BrushSurface();

    if (m_layout == PaneLayout::DualVertical && totalW > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      RECT rcSplit = {splitX, 0, splitX + splitW, totalH};
      FillRect(hdc, &rcSplit, hbrSplitter);
    } else if (m_layout == PaneLayout::DualHorizontal && totalH > 0) {
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      RECT rcSplit = {0, splitY, totalW, splitY + splitW};
      FillRect(hdc, &rcSplit, hbrSplitter);
    } else if (m_layout == PaneLayout::Quad && totalW > 0 && totalH > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      RECT rcSplitV = {splitX, 0, splitX + splitW, totalH};
      RECT rcSplitH = {0, splitY, totalW, splitY + splitW};
      FillRect(hdc, &rcSplitV, hbrSplitter);
      FillRect(hdc, &rcSplitH, hbrSplitter);
    } else if (m_layout == PaneLayout::ThreeLeft && totalW > 0 && totalH > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      RECT rcSplitV = {splitX, 0, splitX + splitW, totalH};
      RECT rcSplitH = {splitX + splitW, splitY, totalW, splitY + splitW};
      FillRect(hdc, &rcSplitV, hbrSplitter);
      FillRect(hdc, &rcSplitH, hbrSplitter);
    }

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_SETCURSOR: {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);

    if (m_layout == PaneLayout::DualVertical && totalW > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      if (pt.x >= splitX && pt.x <= splitX + splitW) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return TRUE;
      }
    } else if (m_layout == PaneLayout::DualHorizontal && totalH > 0) {
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      if (pt.y >= splitY && pt.y <= splitY + splitW) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return TRUE;
      }
    } else if (m_layout == PaneLayout::Quad && totalW > 0 && totalH > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      bool inX = (pt.x >= splitX && pt.x <= splitX + splitW);
      bool inY = (pt.y >= splitY && pt.y <= splitY + splitW);
      if (inX && inY) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return TRUE;
      } else if (inX) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return TRUE;
      } else if (inY) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return TRUE;
      }
    } else if (m_layout == PaneLayout::ThreeLeft && totalW > 0 && totalH > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      if (pt.x >= splitX && pt.x <= splitX + splitW) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return TRUE;
      } else if (pt.x > splitX + splitW && pt.y >= splitY &&
                 pt.y <= splitY + splitW) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return TRUE;
      }
    }
    break;
  }

  case WM_LBUTTONDOWN: {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    if (m_layout == PaneLayout::DualVertical && totalW > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      if (x >= splitX && x <= splitX + splitW) {
        m_draggingSplitH = true;
        SetCapture(hwnd);
        return 0;
      }
    } else if (m_layout == PaneLayout::DualHorizontal && totalH > 0) {
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      if (y >= splitY && y <= splitY + splitW) {
        m_draggingSplitV = true;
        SetCapture(hwnd);
        return 0;
      }
    } else if (m_layout == PaneLayout::Quad && totalW > 0 && totalH > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      bool inX = (x >= splitX && x <= splitX + splitW);
      bool inY = (y >= splitY && y <= splitY + splitW);
      if (inX || inY) {
        if (inX)
          m_draggingSplitH = true;
        if (inY)
          m_draggingSplitV = true;
        SetCapture(hwnd);
        return 0;
      }
    } else if (m_layout == PaneLayout::ThreeLeft && totalW > 0 && totalH > 0) {
      int splitX = (totalW * m_splitH) / 100 - splitW / 2;
      int splitY = (totalH * m_splitV) / 100 - splitW / 2;
      if (x >= splitX && x <= splitX + splitW) {
        m_draggingSplitH = true;
        SetCapture(hwnd);
        return 0;
      } else if (x > splitX + splitW && y >= splitY && y <= splitY + splitW) {
        m_draggingSplitV = true;
        SetCapture(hwnd);
        return 0;
      }
    }
    break;
  }

  case WM_LBUTTONDBLCLK: {
    m_splitH = 50;
    m_splitV = 50;
    LayoutChildren();
    InvalidateRect(hwnd, nullptr, TRUE);
    return 0;
  }

  case WM_MOUSEMOVE: {
    if ((m_draggingSplitH || m_draggingSplitV) && totalW > 0 && totalH > 0) {
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);
      if (m_draggingSplitH) {
        m_splitH = std::clamp((x * 100) / totalW, 10, 90);
      }
      if (m_draggingSplitV) {
        m_splitV = std::clamp((y * 100) / totalH, 10, 90);
      }
      LayoutChildren();
      InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    }
    break;
  }

  case WM_LBUTTONUP: {
    if (m_draggingSplitH || m_draggingSplitV) {
      m_draggingSplitH = false;
      m_draggingSplitV = false;
      ReleaseCapture();
      return 0;
    }
    break;
  }

  case WM_ERASEBKGND:
    return 1;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MultiPaneContainer::HeaderWndProc(HWND hwnd, UINT msg,
                                                   WPARAM wParam,
                                                   LPARAM lParam) {
  MultiPaneContainer *pThis =
      (MultiPaneContainer *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  if (pThis) {
    int paneIdx = (int)(GetWindowLongPtrW(hwnd, GWLP_ID) - 2001);
    return pThis->HandleHeaderMessage(hwnd, msg, wParam, lParam, paneIdx);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MultiPaneContainer::PaneSubclassProc(HWND hwnd, UINT msg,
                                                      WPARAM wParam,
                                                      LPARAM lParam,
                                                      UINT_PTR uIdSubclass,
                                                      DWORD_PTR dwRefData) {
  MultiPaneContainer *pThis = (MultiPaneContainer *)dwRefData;
  int paneIdx = (int)uIdSubclass;

  switch (msg) {
  case WM_SETFOCUS:
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
    pThis->SetActivePane(paneIdx);
    break;
  }

  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MultiPaneContainer::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam) {
  MultiPaneContainer *pThis = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
    pThis = (MultiPaneContainer *)cs->lpCreateParams;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    pThis->m_hwnd = hwnd;
  } else {
    pThis = (MultiPaneContainer *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  }

  if (pThis) {
    return pThis->HandleMessage(hwnd, msg, wParam, lParam);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
