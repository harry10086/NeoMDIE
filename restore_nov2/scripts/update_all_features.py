import sys

# 1. Update Config.h & Config.cpp
print("1. Updating Config.h & Config.cpp...")
with open("src/core/Config.h", "r", encoding="utf-8") as f:
    cfg_h = f.read()

target_action_struct = """struct TargetActionConfig {
    int click = 0;          // 0: Current Pane, 1: Other Pane, 2: New Tab, 3: Background New Tab, 4: Explorer, 5: None
    int shiftClick = 1;     // Other Pane
    int ctrlClick = 2;      // New Tab
    int shiftCtrlClick = 3; // Background New Tab
    int middleClick = 2;    // New Tab
    int doubleClick = 0;    // Current Pane
};
"""

if "TargetActionConfig" not in cfg_h:
    cfg_h = cfg_h.replace("class Config {", target_action_struct + "\nclass Config {")
    cfg_h = cfg_h.replace("int treeWidth = 240;", "int treeWidth = 280;")
    old_actions = """    // Window Actions (窗口动作)
    int actionTreeOpen = 0;    // 0: Current Pane, 1: Other Pane, 2: New Tab
    int actionLinksOpen = 0;   // 0: Current Pane, 1: Other Pane, 2: New Tab
    int actionAddressOpen = 0; // 0: Current Pane, 1: Other Pane, 2: New Tab"""
    new_actions = """    // Window Actions (窗口动作)
    TargetActionConfig actionTree;
    TargetActionConfig actionLinks;
    TargetActionConfig actionAddress;"""
    cfg_h = cfg_h.replace(old_actions, new_actions)
    with open("src/core/Config.h", "w", encoding="utf-8") as f:
        f.write(cfg_h)

with open("src/core/Config.cpp", "r", encoding="utf-8") as f:
    cfg_cpp = f.read()

if "actionTree.click" not in cfg_cpp:
    load_search = 'treeWidth = GetPrivateProfileIntW(sec, L"TreeWidth", 240, m_iniPath.c_str());'
    load_replace = 'treeWidth = GetPrivateProfileIntW(sec, L"TreeWidth", 280, m_iniPath.c_str());\n    if (treeWidth < 260) treeWidth = 280;'
    cfg_cpp = cfg_cpp.replace(load_search, load_replace)

    old_load_act = """    actionTreeOpen = GetPrivateProfileIntW(sec, L"ActionTreeOpen", 0, m_iniPath.c_str());
    actionLinksOpen = GetPrivateProfileIntW(sec, L"ActionLinksOpen", 0, m_iniPath.c_str());
    actionAddressOpen = GetPrivateProfileIntW(sec, L"ActionAddressOpen", 0, m_iniPath.c_str());"""
    new_load_act = """    actionTree.click = GetPrivateProfileIntW(sec, L"ActionTree_Click", 0, m_iniPath.c_str());
    actionTree.shiftClick = GetPrivateProfileIntW(sec, L"ActionTree_ShiftClick", 1, m_iniPath.c_str());
    actionTree.ctrlClick = GetPrivateProfileIntW(sec, L"ActionTree_CtrlClick", 2, m_iniPath.c_str());
    actionTree.shiftCtrlClick = GetPrivateProfileIntW(sec, L"ActionTree_ShiftCtrlClick", 3, m_iniPath.c_str());
    actionTree.middleClick = GetPrivateProfileIntW(sec, L"ActionTree_MiddleClick", 2, m_iniPath.c_str());
    actionTree.doubleClick = GetPrivateProfileIntW(sec, L"ActionTree_DoubleClick", 0, m_iniPath.c_str());

    actionLinks.click = GetPrivateProfileIntW(sec, L"ActionLinks_Click", 0, m_iniPath.c_str());
    actionLinks.shiftClick = GetPrivateProfileIntW(sec, L"ActionLinks_ShiftClick", 1, m_iniPath.c_str());
    actionLinks.ctrlClick = GetPrivateProfileIntW(sec, L"ActionLinks_CtrlClick", 2, m_iniPath.c_str());
    actionLinks.shiftCtrlClick = GetPrivateProfileIntW(sec, L"ActionLinks_ShiftCtrlClick", 3, m_iniPath.c_str());
    actionLinks.middleClick = GetPrivateProfileIntW(sec, L"ActionLinks_MiddleClick", 2, m_iniPath.c_str());
    actionLinks.doubleClick = GetPrivateProfileIntW(sec, L"ActionLinks_DoubleClick", 0, m_iniPath.c_str());

    actionAddress.click = GetPrivateProfileIntW(sec, L"ActionAddress_Click", 0, m_iniPath.c_str());
    actionAddress.shiftClick = GetPrivateProfileIntW(sec, L"ActionAddress_ShiftClick", 1, m_iniPath.c_str());
    actionAddress.ctrlClick = GetPrivateProfileIntW(sec, L"ActionAddress_CtrlClick", 2, m_iniPath.c_str());
    actionAddress.shiftCtrlClick = GetPrivateProfileIntW(sec, L"ActionAddress_ShiftCtrlClick", 3, m_iniPath.c_str());
    actionAddress.middleClick = GetPrivateProfileIntW(sec, L"ActionAddress_MiddleClick", 2, m_iniPath.c_str());
    actionAddress.doubleClick = GetPrivateProfileIntW(sec, L"ActionAddress_DoubleClick", 0, m_iniPath.c_str());"""
    cfg_cpp = cfg_cpp.replace(old_load_act, new_load_act)

    old_save_act = """    WritePrivateProfileStringW(sec, L"ActionTreeOpen", std::to_wstring(actionTreeOpen).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinksOpen", std::to_wstring(actionLinksOpen).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddressOpen", std::to_wstring(actionAddressOpen).c_str(), m_iniPath.c_str());"""
    new_save_act = """    WritePrivateProfileStringW(sec, L"ActionTree_Click", std::to_wstring(actionTree.click).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_ShiftClick", std::to_wstring(actionTree.shiftClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_CtrlClick", std::to_wstring(actionTree.ctrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_ShiftCtrlClick", std::to_wstring(actionTree.shiftCtrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_MiddleClick", std::to_wstring(actionTree.middleClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionTree_DoubleClick", std::to_wstring(actionTree.doubleClick).c_str(), m_iniPath.c_str());

    WritePrivateProfileStringW(sec, L"ActionLinks_Click", std::to_wstring(actionLinks.click).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_ShiftClick", std::to_wstring(actionLinks.shiftClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_CtrlClick", std::to_wstring(actionLinks.ctrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_ShiftCtrlClick", std::to_wstring(actionLinks.shiftCtrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_MiddleClick", std::to_wstring(actionLinks.middleClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionLinks_DoubleClick", std::to_wstring(actionLinks.doubleClick).c_str(), m_iniPath.c_str());

    WritePrivateProfileStringW(sec, L"ActionAddress_Click", std::to_wstring(actionAddress.click).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_ShiftClick", std::to_wstring(actionAddress.shiftClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_CtrlClick", std::to_wstring(actionAddress.ctrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_ShiftCtrlClick", std::to_wstring(actionAddress.shiftCtrlClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_MiddleClick", std::to_wstring(actionAddress.middleClick).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(sec, L"ActionAddress_DoubleClick", std::to_wstring(actionAddress.doubleClick).c_str(), m_iniPath.c_str());"""
    cfg_cpp = cfg_cpp.replace(old_save_act, new_save_act)

    with open("src/core/Config.cpp", "w", encoding="utf-8") as f:
        f.write(cfg_cpp)

# 2. Update GestureEngine.h & GestureEngine.cpp
print("2. Updating GestureEngine.h & GestureEngine.cpp...")
with open("src/core/GestureEngine.h", "r", encoding="utf-8") as f:
    ge_h = f.read()

old_actions_enum = """enum class GestureAction {
    None = 0,
    NavBack = 1,
    NavForward = 2,
    NavUp = 3,
    CloseTab = 4,
    NewTab = 5,
    Refresh = 6,
    DuplicateTab = 7,
    ToggleTree = 8,
    TogglePreview = 9,
    OpenTerminal = 10,
    MaximizePane = 11,
    ClosePane = 12
};"""

new_actions_enum = """enum class GestureAction {
    None = 0,
    NavBack = 1,
    NavForward = 2,
    NavUp = 3,
    CloseTab = 4,
    NewTab = 5,
    Refresh = 6,
    DuplicateTab = 7,
    ToggleTree = 8,
    TogglePreview = 9,
    OpenTerminal = 10,
    MaximizePane = 11,
    ClosePane = 12,
    ScrollTop = 13,
    ScrollBottom = 14,
    PageUp = 15,
    PageDown = 16,
    NewFolder = 17,
    Copy = 18,
    Cut = 19,
    Paste = 20,
    SelectAll = 21,
    Properties = 22,
    LockTab = 23,
    ReopenClosedTab = 24,
    NextTab = 25,
    PrevTab = 26,
    CloseOtherTabs = 27
};"""
ge_h = ge_h.replace(old_actions_enum, new_actions_enum)
with open("src/core/GestureEngine.h", "w", encoding="utf-8") as f:
    f.write(ge_h)

with open("src/core/GestureEngine.cpp", "r", encoding="utf-8") as f:
    ge_cpp = f.read()

old_act_names = """    case GestureAction::MaximizePane: return zh ? L"最大化/还原分屏" : L"Maximize/Restore Pane";
    case GestureAction::ClosePane:    return zh ? L"关闭分屏" : L"Close Pane";
    default: return zh ? L"无动作" : L"None";"""

new_act_names = """    case GestureAction::MaximizePane: return zh ? L"最大化/还原当前栏" : L"Maximize/Restore Pane";
    case GestureAction::ClosePane:    return zh ? L"关闭当前栏" : L"Close Pane";
    case GestureAction::ScrollTop:    return zh ? L"滚动到列表首端" : L"Scroll to Top";
    case GestureAction::ScrollBottom: return zh ? L"滚动到列表末端" : L"Scroll to Bottom";
    case GestureAction::PageUp:       return zh ? L"向上翻页" : L"Page Up";
    case GestureAction::PageDown:     return zh ? L"向下翻页" : L"Page Down";
    case GestureAction::NewFolder:    return zh ? L"新建文件夹" : L"New Folder";
    case GestureAction::Copy:         return zh ? L"复制选中项" : L"Copy";
    case GestureAction::Cut:          return zh ? L"剪切选中项" : L"Cut";
    case GestureAction::Paste:        return zh ? L"粘贴剪贴板" : L"Paste";
    case GestureAction::SelectAll:    return zh ? L"全部选择" : L"Select All";
    case GestureAction::Properties:   return zh ? L"查看属性" : L"Properties";
    case GestureAction::LockTab:      return zh ? L"锁定/解锁当前标签" : L"Lock/Unlock Tab";
    case GestureAction::ReopenClosedTab: return zh ? L"恢复关闭的标签" : L"Reopen Closed Tab";
    case GestureAction::NextTab:      return zh ? L"切换到下一个标签" : L"Next Tab";
    case GestureAction::PrevTab:      return zh ? L"切换到上一个标签" : L"Previous Tab";
    case GestureAction::CloseOtherTabs: return zh ? L"关闭其他标签页" : L"Close Other Tabs";
    default: return zh ? L"无动作" : L"None";"""

ge_cpp = ge_cpp.replace(old_act_names, new_act_names)
with open("src/core/GestureEngine.cpp", "w", encoding="utf-8") as f:
    f.write(ge_cpp)

# 3. Update Theme.cpp (Native font and menu dark mode)
print("3. Updating Theme.cpp...")
with open("src/ui/Theme.cpp", "r", encoding="utf-8") as f:
    theme_cpp = f.read()

old_font_code = """        LOGFONTW lf = { 0 };
        lf.lfHeight = fontSize;
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Segoe UI");

        m_fontUI = CreateFontIndirectW(&lf);"""

new_font_code = """        LOGFONTW lf = { 0 };
        if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &lf, 0)) {
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
        } else {
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
            StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
        }

        m_fontUI = CreateFontIndirectW(&lf);"""

theme_cpp = theme_cpp.replace(old_font_code, new_font_code)

old_set_dark = """void Theme::SetDarkMode(bool dark) {
    m_isDark = dark;
    m_autoMode = false;
    if (m_pfnSetPreferredAppMode) {
        m_pfnSetPreferredAppMode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
    }
    RecreateGdiObjects();
}"""

new_set_dark = """void Theme::SetDarkMode(bool dark) {
    m_isDark = dark;
    m_autoMode = false;
    if (m_pfnSetPreferredAppMode) {
        m_pfnSetPreferredAppMode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
    }
    using fnFlushMenuThemes = void(WINAPI*)();
    static auto pfnFlush = (fnFlushMenuThemes)GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(136));
    if (pfnFlush) pfnFlush();

    RecreateGdiObjects();
}"""

theme_cpp = theme_cpp.replace(old_set_dark, new_set_dark)
with open("src/ui/Theme.cpp", "w", encoding="utf-8") as f:
    f.write(theme_cpp)

# 4. Update MultiPaneContainer.cpp (Header buttons logic and clean layouts)
print("4. Updating MultiPaneContainer.cpp...")
with open("src/ui/MultiPaneContainer.cpp", "r", encoding="utf-8") as f:
    mp_cpp = f.read()

# Fix maxGlyph to check m_maximizedPane == paneIdx
old_max_glyph = "wchar_t maxGlyph[2] = { (m_layout == PaneLayout::Single) ? (wchar_t)0xE923 : (wchar_t)0xE922, 0 };"
new_max_glyph = "wchar_t maxGlyph[2] = { (m_maximizedPane == paneIdx) ? (wchar_t)0xE923 : (wchar_t)0xE922, 0 };"
mp_cpp = mp_cpp.replace(old_max_glyph, new_max_glyph)

with open("src/ui/MultiPaneContainer.cpp", "w", encoding="utf-8") as f:
    f.write(mp_cpp)

print("Done basic enhancements script.")
