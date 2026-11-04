with open('src/ui/MultiPaneContainer.h', 'r', encoding='utf-8') as f:
    hdr = f.read()

hdr_target = '  void UpdatePaneHeader(int index);'
hdr_repl = '''  void UpdatePaneHeader(int index);
  void ApplyTheme();'''
assert hdr_target in hdr
hdr = hdr.replace(hdr_target, hdr_repl, 1)

with open('src/ui/MultiPaneContainer.h', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(hdr)
print('MultiPaneContainer.h updated')

with open('src/ui/MultiPaneContainer.cpp', 'r', encoding='utf-8') as f:
    src = f.read()

src_target = 'void MultiPaneContainer::UpdateLayout(int x, int y, int width, int height) {'
src_repl = '''void MultiPaneContainer::ApplyTheme() {
  for (int i = 0; i < 4; ++i) {
    m_panes[i].ApplyTheme();
    if (m_hHeaders[i]) {
      InvalidateRect(m_hHeaders[i], nullptr, TRUE);
    }
  }
}

void MultiPaneContainer::UpdateLayout(int x, int y, int width, int height) {'''
assert src_target in src
src = src.replace(src_target, src_repl, 1)

with open('src/ui/MultiPaneContainer.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(src)
print('MultiPaneContainer.cpp updated')
