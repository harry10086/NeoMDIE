with open('src/ui/MainWindow.cpp', 'r', encoding='utf-8') as f:
    mw = f.read().replace('\r\n', '\n')

old_inv = """case GestureAction::InvertSelection: OnCommand(ID_EDIT_INVERT_SEL); break;"""
new_inv = """case GestureAction::InvertSelection: OnCommand(ID_EDIT_INVERT_SELECTION); break;"""
assert old_inv in mw, "old_inv not found"
mw = mw.replace(old_inv, new_inv)

old_dup_default = """    case ID_TOOLS_SCRIPT_CONSOLE:
        ScriptEngine::Get().ShowConsole();
        break;
    default:
        if (wmId >= ID_SCRIPT_COMMAND_START && wmId <= ID_SCRIPT_COMMAND_END) {
            FileListView* pView = m_multiPane.GetActivePane();
            std::wstring curPath = pView ? pView->GetCurrentPath() : L"";
            std::vector<std::wstring> sel = pView ? pView->GetSelectedPaths() : std::vector<std::wstring>();
            ScriptEngine::Get().ExecuteCommand(wmId, curPath, sel);
            break;
        }
        break;
    case ID_TOOLS_SETTINGS:"""

new_dup_default = """    case ID_TOOLS_SCRIPT_CONSOLE:
        ScriptEngine::Get().ShowConsole();
        break;
    case ID_TOOLS_SETTINGS:"""

assert old_dup_default in mw, "old_dup_default not found"
mw = mw.replace(old_dup_default, new_dup_default)

with open('src/ui/MainWindow.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(mw)
print("MainWindow.cpp duplicate default and ID_EDIT_INVERT_SELECTION fixed.")
