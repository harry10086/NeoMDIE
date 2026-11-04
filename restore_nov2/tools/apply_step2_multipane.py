with open('src/ui/MultiPaneContainer.cpp', 'r', encoding='utf-8') as f:
    text2 = f.read()

text2 = text2.replace('\r\n', '\n')

old_impl = """void MultiPaneContainer::ApplyTheme() {
  for (int i = 0; i < 4; ++i) {
    m_panes[i].ApplyTheme();
    if (m_hHeaders[i]) {
      InvalidateRect(m_hHeaders[i], nullptr, TRUE);
    }
  }
}"""

new_impl = """void MultiPaneContainer::ApplyTheme() {
  if (m_hwnd) Theme::Get().ApplyToWindow(m_hwnd);
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
}"""

assert old_impl in text2, "old_impl not found in MultiPaneContainer.cpp"
text2 = text2.replace(old_impl, new_impl)

with open('src/ui/MultiPaneContainer.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text2)
print("MultiPaneContainer.cpp updated successfully.")
