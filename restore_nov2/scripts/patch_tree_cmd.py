with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    s = f.read()

target = """        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };"""

repl = """        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };

    m_tree.onCustomCommand = [this](int cmd, const std::wstring& path) {
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB) {
            m_tabBar.AddTab(path, true);
        } else if (cmd == CMD_NEOMDIE_OPEN_SPLIT) {
            m_multiPane.SetLayout(PaneLayout::DualVertical);
            m_multiPane.GetPane(1).NavigateTo(path);
            m_multiPane.SetActivePane(1);
        } else if (cmd == CMD_NEOMDIE_ADD_LINKS) {
            m_linksBar.AddLink(L"", path);
        }
    };"""

if target in s:
    s = s.replace(target, repl, 1)
    with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8') as f:
        f.write(s)
    print('m_tree.onCustomCommand wired up!')
else:
    print('Target not found')
