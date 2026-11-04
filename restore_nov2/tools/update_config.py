import re

with open('src/core/Config.cpp', 'r', encoding='utf-8') as f:
    data = f.read()

# 1. Update window.maximized default to 1
data = data.replace(
    'window.maximized = GetPrivateProfileIntW(secWindow, L"Maximized", 0, m_iniPath.c_str()) != 0;',
    'window.maximized = GetPrivateProfileIntW(secWindow, L"Maximized", 1, m_iniPath.c_str()) != 0;'
)

# 2. Add load for singleInstance, zebraStriping, showGridLines
load_target = 'searchSubfoldersDefault = GetPrivateProfileIntW(secGeneral, L"SearchSubfolders", 0, m_iniPath.c_str()) != 0;'
load_repl = '''searchSubfoldersDefault = GetPrivateProfileIntW(secGeneral, L"SearchSubfolders", 0, m_iniPath.c_str()) != 0;
    singleInstance = GetPrivateProfileIntW(secGeneral, L"SingleInstance", 1, m_iniPath.c_str()) != 0;
    zebraStriping = GetPrivateProfileIntW(secGeneral, L"ZebraStriping", 0, m_iniPath.c_str()) != 0;
    showGridLines = GetPrivateProfileIntW(secGeneral, L"ShowGridLines", 0, m_iniPath.c_str()) != 0;'''
data = data.replace(load_target, load_repl)

# 3. Add default gestures
gest_target = '''        gestures = {
            { "L", 1, L"后退" },
            { "R", 2, L"前进" },
            { "U", 3, L"向上一级" },
            { "D", 4, L"关闭标签页" },
            { "UD", 6, L"刷新" },
            { "DR", 5, L"新建标签页" },
            { "UL", 7, L"复制标签页" },
            { "UR", 8, L"切换目录树" }
        };'''
gest_repl = '''        gestures = {
            { "L", 1, L"后退" },
            { "R", 2, L"前进" },
            { "U", 3, L"向上一级" },
            { "D", 4, L"关闭标签页" },
            { "UD", 6, L"刷新" },
            { "DR", 5, L"新建标签页" },
            { "UL", 7, L"复制标签页" },
            { "UR", 8, L"切换目录树" },
            { "LDR", 13, L"滚动到列表首端" },
            { "RDL", 14, L"滚动到列表末端" },
            { "LUR", 15, L"向上翻页" },
            { "RUL", 16, L"向下翻页" }
        };'''
data = data.replace(gest_target, gest_repl)

# 4. Add save for singleInstance, zebraStriping, showGridLines
save_target = 'writeInt(secGeneral, L"GesturesEnabled", gesturesEnabled ? 1 : 0);'
save_repl = '''writeInt(secGeneral, L"GesturesEnabled", gesturesEnabled ? 1 : 0);
    writeInt(secGeneral, L"SingleInstance", singleInstance ? 1 : 0);
    writeInt(secGeneral, L"ZebraStriping", zebraStriping ? 1 : 0);
    writeInt(secGeneral, L"ShowGridLines", showGridLines ? 1 : 0);'''
data = data.replace(save_target, save_repl)

with open('src/core/Config.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(data)
print('Config.cpp updated successfully')
