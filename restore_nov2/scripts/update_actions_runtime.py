# Update DirectoryTree.cpp with actionTree logic
with open("src/ui/DirectoryTree.cpp", "r", encoding="utf-8") as f:
    tree_c = f.read()

old_tvn = """            case TVN_SELCHANGEDW: {
                if (pThis->m_syncing) break;
                NMTREEVIEWW* pnmtv = (NMTREEVIEWW*)lParam;
                TreeNodeData* pData = (TreeNodeData*)pnmtv->itemNew.lParam;
                if (pData && pThis->onPathSelected) {
                    if (pData->path == L"This PC" || pData->path == L"QuickAccess") {
                        pThis->onPathSelected(L"This PC");
                    } else if (!pData->path.empty()) {
                        pThis->onPathSelected(pData->path);
                    }
                }
                break;
            }"""

new_tvn = """            case TVN_SELCHANGEDW: {
                if (pThis->m_syncing) break;
                NMTREEVIEWW* pnmtv = (NMTREEVIEWW*)lParam;
                TreeNodeData* pData = (TreeNodeData*)pnmtv->itemNew.lParam;
                if (pData) {
                    std::wstring target = (pData->path == L"This PC" || pData->path == L"QuickAccess") ? L"This PC" : pData->path;
                    if (!target.empty()) {
                        bool isCtrl = (GetKeyState(VK_CONTROL) < 0);
                        bool isShift = (GetKeyState(VK_SHIFT) < 0);
                        int act = Config::Get().actionTree.click;
                        if (isCtrl && isShift) act = Config::Get().actionTree.shiftCtrlClick;
                        else if (isCtrl) act = Config::Get().actionTree.ctrlClick;
                        else if (isShift) act = Config::Get().actionTree.shiftClick;

                        if (act == 0) {
                            if (pThis->onPathSelected) pThis->onPathSelected(target);
                        } else if (act == 1) {
                            if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_SPLIT, target);
                        } else if (act == 2 || act == 3) {
                            if (pThis->onCustomCommand) pThis->onCustomCommand(CMD_NEOMDIE_OPEN_NEW_TAB, target);
                        } else if (act == 4) {
                            ShellExecuteW(nullptr, L"explore", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                        }
                    }
                }
                break;
            }"""

tree_c = tree_c.replace(old_tvn, new_tvn)
with open("src/ui/DirectoryTree.cpp", "w", encoding="utf-8") as f:
    f.write(tree_c)
print("DirectoryTree.cpp action matrix updated")

# Update LinksBar.cpp with actionLinks logic
with open("src/ui/LinksBar.cpp", "r", encoding="utf-8") as f:
    links_c = f.read()

old_links_click = """    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                if (onNavigate) {
                    onNavigate(m_links[i].path);
                }
                return 0;
            }
        }
        return 0;
    }

    case WM_MBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                if (onOpenNewTab) onOpenNewTab(m_links[i].path);
                return 0;
            }
        }
        return 0;
    }"""

new_links_click = """    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                bool isCtrl = (GetKeyState(VK_CONTROL) < 0);
                bool isShift = (GetKeyState(VK_SHIFT) < 0);
                int act = Config::Get().actionLinks.click;
                if (isCtrl && isShift) act = Config::Get().actionLinks.shiftCtrlClick;
                else if (isCtrl) act = Config::Get().actionLinks.ctrlClick;
                else if (isShift) act = Config::Get().actionLinks.shiftClick;

                if (act == 0 && onNavigate) onNavigate(m_links[i].path);
                else if (act == 1 && onOpenSplit) onOpenSplit(m_links[i].path);
                else if ((act == 2 || act == 3) && onOpenNewTab) onOpenNewTab(m_links[i].path);
                else if (act == 4) ShellExecuteW(nullptr, L"explore", m_links[i].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
        }
        return 0;
    }

    case WM_MBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        for (size_t i = 0; i < m_itemRects.size(); ++i) {
            if (PtInRect(&m_itemRects[i], { x, y })) {
                int act = Config::Get().actionLinks.middleClick;
                if (act == 0 && onNavigate) onNavigate(m_links[i].path);
                else if (act == 1 && onOpenSplit) onOpenSplit(m_links[i].path);
                else if ((act == 2 || act == 3) && onOpenNewTab) onOpenNewTab(m_links[i].path);
                else if (act == 4) ShellExecuteW(nullptr, L"explore", m_links[i].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
        }
        return 0;
    }"""

links_c = links_c.replace(old_links_click, new_links_click)
with open("src/ui/LinksBar.cpp", "w", encoding="utf-8") as f:
    f.write(links_c)
print("LinksBar.cpp action matrix updated")
