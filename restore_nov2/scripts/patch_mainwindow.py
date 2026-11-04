import re

with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Add Markdown item to hNewSubmenu
target_new = 'AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_FILE, isZh ? L"文本文档(&T)" : L"Text Document(&T)");'
replacement_new = target_new + '\n    AppendMenuW(hNewSubmenu, MF_STRING, ID_FILE_NEW_MD, isZh ? L"Markdown 文档(&M)" : L"Markdown Document(&M)");'
code = code.replace(target_new, replacement_new, 1)

# 2. Add Scroll submenu under View
target_view = 'AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_REFRESH, isZh ? L"刷新(&R)\\tF5" : L"Refresh(&R)\\tF5");'
replacement_scroll = '''    // 滚动 (X)
    HMENU hScrollSubmenu = CreatePopupMenu();
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_TOP, isZh ? L"列表开头(&T)\\tHome" : L"Top of List(&T)\\tHome");
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_PAGEUP, isZh ? L"此页开头(&U)\\tPage Up" : L"Top of Page(&U)\\tPage Up");
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_PAGEDOWN, isZh ? L"此页结尾(&D)\\tPage Down" : L"Bottom of Page(&D)\\tPage Down");
    AppendMenuW(hScrollSubmenu, MF_STRING, ID_SCROLL_BOTTOM, isZh ? L"列表结尾(&B)\\tEnd" : L"Bottom of List(&B)\\tEnd");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hScrollSubmenu, isZh ? L"滚动(&X)" : L"Scroll(&X)");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    ''' + target_view
code = code.replace(target_view, replacement_scroll, 1)

# 3. Simplify Tools Menu (keep only CMD, Shortcuts, Settings)
old_tools = '''    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_TERMINAL, isZh ? L"打开 Windows Terminal(&T)" : L"Open Windows Terminal(&T)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_POWERSHELL, isZh ? L"打开 PowerShell(&P)" : L"Open PowerShell(&P)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, isZh ? L"打开 CMD 命令行(&C)" : L"Open CMD Command Prompt(&C)");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_VSCODE, isZh ? L"在 VS Code 中打开(&V)" : L"Open in VS Code(&V)");'''

new_tools = '''    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_OPEN_CMD, isZh ? L"打开命令行 CMD(&C)" : L"Open Command Prompt CMD(&C)");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SHORTCUTS_SETTINGS, isZh ? L"按键设置(&K)..." : L"Keyboard Shortcuts(&K)...");
    AppendMenuW(hToolsMenu, MF_STRING, ID_TOOLS_SETTINGS, isZh ? L"选项设置(&O)..." : L"Options(&O)...");'''
code = code.replace(old_tools, new_tools, 1)

# 4. Clean Help Menu (only GitHub, Check Updates, About)
old_help = '''    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_SHORTCUTS, isZh ? L"帮助与键盘快捷键(&K)" : L"Keyboard Shortcuts(&K)");
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GESTURES, isZh ? L"鼠标手势说明(&G)" : L"Mouse Gestures Cheatsheet(&G)");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_MDIE_HOMEPAGE, isZh ? L"MDIE 官方主页(&M)" : L"MDIE Homepage(&M)");
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GITHUB, isZh ? L"NeoMDIE GitHub 开源仓库(&G)" : L"NeoMDIE GitHub(&G)");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_ABOUT, isZh ? L"关于 NeoMDIE(&A)..." : L"About NeoMDIE(&A)...");'''

new_help = '''    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_GITHUB, isZh ? L"NeoMDIE 开源仓库(&G)" : L"NeoMDIE GitHub Repository(&G)");
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_CHECK_UPDATES, isZh ? L"检查更新(&U)..." : L"Check for Updates(&U)...");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_ABOUT, isZh ? L"关于 NeoMDIE(&A)..." : L"About NeoMDIE(&A)...");'''
code = code.replace(old_help, new_help, 1)

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Menus patched successfully.")
