# -*- coding: utf-8 -*-
with open(r'src\ui\LinksBar.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('#include <commdlg.h>\n', '')
text = '#include "LinksBar.h"\n#include <commdlg.h>\n' + text[len('#include "LinksBar.h"\n'):]

with open(r'src\ui\LinksBar.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print("LinksBar.cpp include order fixed!")
