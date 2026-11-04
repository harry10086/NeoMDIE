with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

old_block = r"""    for (int i = 0; i < 4; ++i) {
        setupViewCallbacks(m_multiPane.GetPane(i), i);
    }"""

new_block = r"""    for (int i = 0; i < 4; ++i) {
        setupViewCallbacks(m_multiPane.GetPane(i), i);
    }
    m_multiPane.SetShowHidden(Config::Get().showHidden);
    m_multiPane.SetCalcFolderSize(Config::Get().calcFolderSize);"""

assert old_block in text, "old_block not found"
text = text.replace(old_block, new_block, 1)

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("MainWindow.cpp updated with initial hidden & calcFolderSize calls.")
