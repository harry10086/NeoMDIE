# -*- coding: utf-8 -*-
with open(r'src\ui\FilterBar.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = "void FilterBar::Clear() {\n    SetWindowTextW(m_hwndEdit, L\"\");\n}"
replacement = """void FilterBar::Clear() {
    SetWindowTextW(m_hwndEdit, L"");
}

void FilterBar::SetText(const std::wstring& text) {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, text.c_str());
    }
}"""

if target.replace('\n', '\r\n') in text:
    text = text.replace(target.replace('\n', '\r\n'), replacement.replace('\n', '\r\n'), 1)
elif target in text:
    text = text.replace(target, replacement, 1)
else:
    print("target not found!")

with open(r'src\ui\FilterBar.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print("FilterBar.cpp updated with SetText!")
