# -*- coding: utf-8 -*-
with open(r'src\ui\MainWindow.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = """    case ID_TOOLS_SHORTCUTS_SETTINGS:
    case ID_HELP_SHORTCUTS:
    case ID_HELP_GESTURES:
        OnCommand(ID_TOOLS_SETTINGS);
        break;"""

replacement = """    case ID_TOOLS_SHORTCUTS_SETTINGS:
    case ID_HELP_SHORTCUTS:
        SettingsDialog::Show(m_hwnd, [this]() {
            UpdateLanguageUI();
        }, 7);
        break;
    case ID_HELP_GESTURES:
        SettingsDialog::Show(m_hwnd, [this]() {
            UpdateLanguageUI();
        }, 8);
        break;"""

if target.replace('\n', '\r\n') in text:
    text = text.replace(target.replace('\n', '\r\n'), replacement.replace('\n', '\r\n'), 1)
elif target in text:
    text = text.replace(target, replacement, 1)
else:
    print("target not found!")

with open(r'src\ui\MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print("MainWindow.cpp shortcuts route updated!")
