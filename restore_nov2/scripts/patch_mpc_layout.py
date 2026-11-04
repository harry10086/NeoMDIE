# -*- coding: utf-8 -*-
with open(r'src\ui\MultiPaneContainer.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = """  switch (m_layout) {"""
insert = """  if (m_maximizedPane >= 0 && m_maximizedPane < GetPaneCount()) {
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

if target in text:
    text = text.replace(target, insert, 1)
    with open(r'src\ui\MultiPaneContainer.cpp', 'w', encoding='utf-8') as f:
        f.write(text)
    print("MultiPaneContainer.cpp updated with maximizedPane layout logic!")
else:
    print("target not found!")
