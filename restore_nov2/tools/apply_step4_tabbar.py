with open('src/ui/TabBar.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

old_rbutton = r"""    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                POINT ptScreen = pt;
                ClientToScreen(hwnd, &ptScreen);
                ShowTabContextMenu((int)i, ptScreen);
                return 0;
            }
        }
        return 0;
    }"""

new_rbutton = r"""    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (PtInRect(&m_tabs[i].rc, pt)) {
                POINT ptScreen = pt;
                ClientToScreen(hwnd, &ptScreen);
                ShowTabContextMenu((int)i, ptScreen);
                return 0;
            }
        }
        // Clicked on empty space in TabBar: show TabBar background context menu!
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);
        HMENU hMenu = CreatePopupMenu();
        bool isZh = I18n::Get().IsChinese();
        AppendMenuW(hMenu, MF_STRING, 1, isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
        AppendMenuW(hMenu, MF_STRING | (m_closedHistory.empty() ? MF_GRAYED : 0), 2, isZh ? L"恢复关闭的标签页(&R)\tCtrl+Shift+T" : L"Reopen Closed Tab(&R)\tCtrl+Shift+T");
        AppendMenuW(hMenu, MF_STRING, 3, isZh ? L"关闭其他标签页(&O)" : L"Close Other Tabs(&O)");
        AppendMenuW(hMenu, MF_STRING, 4, isZh ? L"关闭当前标签页(&W)\tCtrl+W" : L"Close Current Tab(&W)\tCtrl+W");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 5, isZh ? L"在此处打开命令提示符 CMD(&C)" : L"Open CMD Here(&C)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 6, isZh ? L"NeoMDIE 设置(&S)..." : L"NeoMDIE Settings(&S)...");

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);

        switch (cmd) {
        case 1: AddTab(L"This PC", true); break;
        case 2: ReopenClosedTab(); break;
        case 3: CloseOtherTabs(m_activeIndex); break;
        case 4: CloseTab(m_activeIndex); break;
        case 5:
            if (m_activeIndex >= 0 && m_activeIndex < (int)m_tabs.size()) {
                ShellUtils::LaunchTerminal(m_tabs[m_activeIndex].path);
            }
            break;
        case 6:
            PostMessageW(GetAncestor(hwnd, GA_ROOT), WM_COMMAND, MAKEWPARAM(40056, 0), 0); // ID_TOOLS_SETTINGS
            break;
        }
        return 0;
    }"""

assert old_rbutton in text, "old_rbutton not found in TabBar.cpp"
text = text.replace(old_rbutton, new_rbutton)

with open('src/ui/TabBar.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("TabBar.cpp updated successfully.")
