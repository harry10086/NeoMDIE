with open('src/core/GestureEngine.cpp', 'r', encoding='utf-8') as f:
    ge = f.read()

old_arrows = '''std::wstring GestureEngine::CodeToArrowString(const std::string& code) {
    std::wstring s;
    for (char c : code) {
        if (c == 'L') s += L"🠔";
        else if (c == 'R') s += L"🠖";
        else if (c == 'U') s += L"🠕";
        else if (c == 'D') s += L"🠗";
    }
    return s;
}'''

new_arrows = '''std::wstring GestureEngine::CodeToArrowString(const std::string& code) {
    std::wstring s;
    for (char c : code) {
        if (c == 'L') s += L"←";
        else if (c == 'R') s += L"→";
        else if (c == 'U') s += L"↑";
        else if (c == 'D') s += L"↓";
    }
    return s;
}'''
assert old_arrows in ge
ge = ge.replace(old_arrows, new_arrows, 1)

with open('src/core/GestureEngine.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(ge)
print('GestureEngine.cpp updated')
