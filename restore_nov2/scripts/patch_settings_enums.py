# -*- coding: utf-8 -*-
with open(r'src\ui\SettingsDialog.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

old_chunk = """        struct ActItem { int id; const wchar_t* lbl; };
        ActItem acts[] = {
          { (int)GestureAction::Back, L"后退 (Back)" },
          { (int)GestureAction::Forward, L"前进 (Forward)" },
          { (int)GestureAction::Up, L"向上一级目录 (Go Up)" },
          { (int)GestureAction::Refresh, L"刷新 (Refresh)" },
          { (int)GestureAction::CloseTab, L"关闭当前标签页 (Close Tab)" },
          { (int)GestureAction::NewTab, L"新建标签页 (New Tab)" },
          { (int)GestureAction::ReopenTab, L"恢复最后关闭的标签页 (Reopen Closed Tab)" },
          { (int)GestureAction::ScrollTop, L"滚动到列表最开头 (Scroll to Top)" },
          { (int)GestureAction::ScrollBottom, L"滚动到列表最末尾 (Scroll to Bottom)" },
          { (int)GestureAction::ToggleTree, L"切换左侧目录树显隐 (Toggle Tree)" },
          { (int)GestureAction::TogglePreview, L"切换右侧快速预览显隐 (Toggle Preview)" }
        };
        for (int i = 0; i < 11; ++i) {
          AppendMenuW(hAct, MF_STRING, 6001 + i, acts[i].lbl);
        }
        POINT pt;
        GetCursorPos(&pt);
        int selA = TrackPopupMenu(hAct, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hAct);
        if (selA >= 6001 && selA <= 6011) {
          s_tempGestures[sel].action = acts[selA - 6001].id;
          s_tempGestures[sel].name = acts[selA - 6001].lbl;
          PopulateGestureList(s_hGestureList);
        }
      }
      return 0;
    } else if (id == 365) { // Delete gesture
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempGestures.size()) {
        s_tempGestures.erase(s_tempGestures.begin() + sel);
        PopulateGestureList(s_hGestureList);
      }
      return 0;
    } else if (id == 366) { // Reset defaults
      Config defCfg;
      s_tempGestures = defCfg.gestures;
      PopulateGestureList(s_hGestureList);
      return 0;
    }"""

new_chunk = """        struct ActItem { int id; const wchar_t* lbl; };
        ActItem acts[] = {
          { (int)GestureAction::NavBack, L"后退 (Back)" },
          { (int)GestureAction::NavForward, L"前进 (Forward)" },
          { (int)GestureAction::NavUp, L"向上一级目录 (Go Up)" },
          { (int)GestureAction::Refresh, L"刷新 (Refresh)" },
          { (int)GestureAction::CloseTab, L"关闭当前标签页 (Close Tab)" },
          { (int)GestureAction::NewTab, L"新建标签页 (New Tab)" },
          { (int)GestureAction::DuplicateTab, L"复制标签页 (Duplicate Tab)" },
          { (int)GestureAction::ToggleTree, L"切换左侧目录树显隐 (Toggle Tree)" },
          { (int)GestureAction::TogglePreview, L"切换右侧快速预览显隐 (Toggle Preview)" },
          { (int)GestureAction::OpenTerminal, L"打开命令提示符/终端 (Open Terminal/Cmd)" },
          { (int)GestureAction::MaximizePane, L"最大化/还原当前分栏 (Maximize/Restore Pane)" },
          { (int)GestureAction::ClosePane, L"关闭当前分栏 (Close Pane)" }
        };
        for (int i = 0; i < 12; ++i) {
          AppendMenuW(hAct, MF_STRING, 6001 + i, acts[i].lbl);
        }
        POINT pt;
        GetCursorPos(&pt);
        int selA = TrackPopupMenu(hAct, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hAct);
        if (selA >= 6001 && selA <= 6012) {
          s_tempGestures[sel].action = acts[selA - 6001].id;
          s_tempGestures[sel].name = acts[selA - 6001].lbl;
          PopulateGestureList(s_hGestureList);
        }
      }
      return 0;
    } else if (id == 365) { // Delete gesture
      int sel = ListView_GetNextItem(s_hGestureList, -1, LVNI_SELECTED);
      if (sel >= 0 && sel < (int)s_tempGestures.size()) {
        s_tempGestures.erase(s_tempGestures.begin() + sel);
        PopulateGestureList(s_hGestureList);
      }
      return 0;
    } else if (id == 366) { // Reset defaults
      s_tempGestures = {
        { "L", 1, L"后退" },
        { "R", 2, L"前进" },
        { "U", 3, L"向上一级" },
        { "D", 4, L"关闭标签页" },
        { "UD", 6, L"刷新" },
        { "DR", 5, L"新建标签页" },
        { "UL", 7, L"复制标签页" },
        { "UR", 8, L"切换目录树" }
      };
      PopulateGestureList(s_hGestureList);
      return 0;
    }"""

old_chunk_crlf = old_chunk.replace('\n', '\r\n')
new_chunk_crlf = new_chunk.replace('\n', '\r\n')
if old_chunk_crlf in text:
    text = text.replace(old_chunk_crlf, new_chunk_crlf, 1)
elif old_chunk in text:
    text = text.replace(old_chunk, new_chunk, 1)
else:
    print("Chunk not found!")

with open(r'src\ui\SettingsDialog.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print("Updated SettingsDialog.cpp successfully!")
