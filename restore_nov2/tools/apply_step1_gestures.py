with open('src/core/GestureEngine.h', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\r\n', '\n')

old_enum = """    LockTab = 23,
    ReopenClosedTab = 24,
    NextTab = 25,
    PrevTab = 26,
    CloseOtherTabs = 27
};"""

new_enum = """    LockTab = 23,
    ReopenClosedTab = 24,
    NextTab = 25,
    PrevTab = 26,
    CloseOtherTabs = 27,
    LayoutSingle = 28,
    Layout2H = 29,
    Layout2V = 30,
    Layout4Grid = 31,
    CopyPath = 32,
    Delete = 33,
    Rename = 34,
    NewMD = 35,
    ToggleHidden = 36,
    OpenSettings = 37,
    InvertSelection = 38
};"""

assert old_enum in text, "old_enum not found in GestureEngine.h"
text = text.replace(old_enum, new_enum)

with open('src/core/GestureEngine.h', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)
print("GestureEngine.h updated successfully.")

with open('src/core/GestureEngine.cpp', 'r', encoding='utf-8') as f:
    text2 = f.read()

text2 = text2.replace('\r\n', '\n')

old_actions = """    case GestureAction::CloseOtherTabs: return zh ? L"关闭其他标签页" : L"Close Other Tabs";
    default: return zh ? L"无动作" : L"None";"""

new_actions = """    case GestureAction::CloseOtherTabs: return zh ? L"关闭其他标签页" : L"Close Other Tabs";
    case GestureAction::LayoutSingle: return zh ? L"单栏模式" : L"Single Pane";
    case GestureAction::Layout2H:     return zh ? L"左右双栏" : L"Dual Pane Horizontal";
    case GestureAction::Layout2V:     return zh ? L"上下双栏" : L"Dual Pane Vertical";
    case GestureAction::Layout4Grid:  return zh ? L"四分栏模式" : L"4 Panes Grid";
    case GestureAction::CopyPath:     return zh ? L"复制当前路径" : L"Copy Path";
    case GestureAction::Delete:       return zh ? L"删除选中项" : L"Delete";
    case GestureAction::Rename:       return zh ? L"重命名" : L"Rename";
    case GestureAction::NewMD:        return zh ? L"新建Markdown文档" : L"New Markdown Document";
    case GestureAction::ToggleHidden: return zh ? L"显示/隐藏隐藏文件" : L"Toggle Hidden Files";
    case GestureAction::OpenSettings: return zh ? L"打开选项设置" : L"Options & Settings";
    case GestureAction::InvertSelection: return zh ? L"反向选择" : L"Invert Selection";
    default: return zh ? L"无动作" : L"None";"""

assert old_actions in text2, "old_actions not found in GestureEngine.cpp"
text2 = text2.replace(old_actions, new_actions)

with open('src/core/GestureEngine.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text2)
print("GestureEngine.cpp updated successfully.")
