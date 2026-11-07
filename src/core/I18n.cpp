#include "I18n.h"
#include "Config.h"

I18n& I18n::Get() {
    static I18n instance;
    return instance;
}

I18n::I18n() {
}

void I18n::Initialize() {
    // Check config
    int savedLang = Config::Get().language;
    if (savedLang == 0 || savedLang == 1) {
        m_lang = (Language)savedLang;
    } else {
        // Auto detect system UI language
        LANGID langId = PRIMARYLANGID(GetUserDefaultUILanguage());
        m_lang = (langId == LANG_CHINESE) ? Language::Chinese : Language::English;
    }
}

void I18n::SetLanguage(Language lang) {
    if (m_lang != lang) {
        m_lang = lang;
        Config::Get().language = (int)lang;
        Config::Get().Save();
        if (onLanguageChanged) {
            onLanguageChanged();
        }
    }
}

const wchar_t* I18n::T(StringId id) const {
    bool zh = (m_lang == Language::Chinese);

    switch (id) {
    case StringId::AppName:            return L"NeoMDIE";
    case StringId::ThisPC:             return zh ? L"此电脑" : L"This PC";
    case StringId::QuickAccess:        return zh ? L"快速访问" : L"Quick Access";
    case StringId::Desktop:            return zh ? L"桌面" : L"Desktop";
    case StringId::Downloads:          return zh ? L"下载" : L"Downloads";
    case StringId::Documents:          return zh ? L"文档" : L"Documents";
    case StringId::Pictures:           return zh ? L"图片" : L"Pictures";
    case StringId::Music:              return zh ? L"音乐" : L"Music";
    case StringId::Videos:             return zh ? L"视频" : L"Videos";
    case StringId::NewFolder:          return zh ? L"新建文件夹" : L"New Folder";
    case StringId::NewTextFile:        return zh ? L"新建 TXT 文档.txt" : L"New TXT Document.txt";
    case StringId::Folder:             return zh ? L"文件夹" : L"Folder";
    case StringId::DriveLocal:         return zh ? L"本地磁盘" : L"Local Disk";
    case StringId::DriveRemovable:     return zh ? L"可移动磁盘" : L"Removable Disk";
    case StringId::DriveCD:            return zh ? L"CD/DVD 驱动器" : L"CD/DVD Drive";
    case StringId::DriveNetwork:       return zh ? L"网络驱动器" : L"Network Drive";

    case StringId::ColName:            return zh ? L"名称" : L"Name";
    case StringId::ColDateModified:    return zh ? L"修改日期" : L"Date modified";
    case StringId::ColType:            return zh ? L"类型" : L"Type";
    case StringId::ColSize:            return zh ? L"大小" : L"Size";

    case StringId::BtnTree:            return zh ? L"📁 目录树" : L"📁 Tree";
    case StringId::BtnSplit:           return zh ? L"⊞ 分栏" : L"⊞ Split";
    case StringId::BtnPreview:         return zh ? L"👁 预览" : L"👁 Preview";

    case StringId::FilterCue:          return zh ? L"即时过滤 (Ctrl+F)" : L"Filter files (Ctrl+F)";
    case StringId::StatusItems:        return zh ? L"%d 个项目" : L"%d items";
    case StringId::StatusSize:         return zh ? L"大小: %s" : L"Size: %s";
    case StringId::StatusReady:        return zh ? L"就绪" : L"Ready";

    case StringId::PreviewSelectHint:  return zh ? L"选择文件以预览" : L"Select a file to preview";
    case StringId::PreviewNoHandler:   return zh ? L"该文件暂无预览插件" : L"No preview available for this file";

    case StringId::GestureBack:        return zh ? L"← 后退" : L"← Back";
    case StringId::GestureForward:     return zh ? L"→ 前进" : L"→ Forward";
    case StringId::GestureUp:          return zh ? L"↑ 向上一级" : L"↑ Up";
    case StringId::GestureCloseTab:    return zh ? L"↓ 关闭标签" : L"↓ Close Tab";
    case StringId::GestureRefresh:     return zh ? L"↑↓ 刷新" : L"↑↓ Refresh";
    case StringId::GestureNewTab:      return zh ? L"↓→ 新建标签" : L"↓→ New Tab";

    case StringId::MenuFile:           return zh ? L"文件(&F)" : L"File(&F)";
    case StringId::MenuNewTab:         return zh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T";
    case StringId::MenuCloseTab:       return zh ? L"关闭当前标签页(&W)\tCtrl+W" : L"Close Current Tab(&W)\tCtrl+W";
    case StringId::MenuCloseOtherTabs: return zh ? L"关闭其他标签页(&O)" : L"Close Other Tabs(&O)";
    case StringId::MenuCloseAllTabs:   return zh ? L"关闭所有标签页(&A)" : L"Close All Tabs(&A)";
    case StringId::MenuReopenClosedTab:return zh ? L"重新打开已关闭标签页(&Z)\tCtrl+Shift+T" : L"Reopen Closed Tab(&Z)\tCtrl+Shift+T";
    case StringId::MenuDuplicateTab:   return zh ? L"复制标签页(&D)\tCtrl+K" : L"Duplicate Tab(&D)\tCtrl+K";
    case StringId::MenuPinTab:         return zh ? L"固定/锁定标签页(&P)" : L"Pin Tab(&P)";
    case StringId::MenuUnpinTab:       return zh ? L"取消固定标签页(&P)" : L"Unpin Tab(&P)";
    case StringId::MenuNewFolder:      return zh ? L"新建文件夹(&N)\tCtrl+Shift+N" : L"New Folder(&N)\tCtrl+Shift+N";
    case StringId::MenuNewFile:        return zh ? L"新建文本文件(&M)" : L"New Text Document(&M)";
    case StringId::MenuProperties:     return zh ? L"属性(&R)\tAlt+Enter" : L"Properties(&R)\tAlt+Enter";
    case StringId::MenuExit:           return zh ? L"退出(&X)\tAlt+F4" : L"Exit(&X)\tAlt+F4";

    case StringId::MenuEdit:           return zh ? L"编辑(&E)" : L"Edit(&E)";
    case StringId::MenuCut:            return zh ? L"剪切(&T)\tCtrl+X" : L"Cut(&T)\tCtrl+X";
    case StringId::MenuCopy:           return zh ? L"复制(&C)\tCtrl+C" : L"Copy(&C)\tCtrl+C";
    case StringId::MenuCopyName:       return zh ? L"复制文件名(&N)" : L"Copy File Name(&N)";
    case StringId::MenuCopyDir:        return zh ? L"复制所在目录路径(&D)" : L"Copy Directory Path(&D)";
    case StringId::MenuPaste:          return zh ? L"粘贴(&P)\tCtrl+V" : L"Paste(&P)\tCtrl+V";
    case StringId::MenuPasteShortcut:  return zh ? L"粘贴快捷方式(&S)" : L"Paste Shortcut(&S)";
    case StringId::MenuDelete:         return zh ? L"删除(&D)\tDel" : L"Delete(&D)\tDel";
    case StringId::MenuRename:         return zh ? L"重命名(&M)\tF2" : L"Rename(&M)\tF2";
    case StringId::MenuSelectAll:      return zh ? L"全选(&A)\tCtrl+A" : L"Select All(&A)\tCtrl+A";
    case StringId::MenuInvertSelection:return zh ? L"反向选择(&I)" : L"Invert Selection(&I)";
    case StringId::MenuCopyPath:       return zh ? L"复制完整路径(&Y)\tCtrl+Shift+C" : L"Copy Full Path(&Y)\tCtrl+Shift+C";

    case StringId::MenuView:           return zh ? L"查看(&V)" : L"View(&V)";
    case StringId::MenuRefresh:        return zh ? L"刷新(&R)\tF5" : L"Refresh(&R)\tF5";
    case StringId::MenuTree:           return zh ? L"目录树(&T)" : L"Folder Tree(&T)";
    case StringId::MenuSplit:          return zh ? L"多栏布局(&S)" : L"Multi-Pane Split(&S)";
    case StringId::MenuPreview:        return zh ? L"文件预览窗格(&P)\tAlt+P" : L"Preview Pane(&P)\tAlt+P";
    case StringId::MenuFilter:         return zh ? L"即时过滤查找(&F)\tCtrl+F" : L"Incremental Filter(&F)\tCtrl+F";
    case StringId::MenuDetails:        return zh ? L"详细信息(&D)" : L"Details(&D)";
    case StringId::MenuLargeIcons:     return zh ? L"大图标(&G)" : L"Large Icons(&G)";
    case StringId::MenuSmallIcons:     return zh ? L"小图标(&M)" : L"Small Icons(&M)";
    case StringId::MenuList:           return zh ? L"列表(&L)" : L"List(&L)";
    case StringId::MenuShowHidden:     return zh ? L"显示隐藏文件(&H)" : L"Show Hidden Files(&H)";
    case StringId::MenuCalcFolderSize: return zh ? L"后台计算文件夹大小(&C)" : L"Calculate Folder Sizes(&C)";
    case StringId::MenuSortBy:         return zh ? L"排序方式(&O)" : L"Sort by(&O)";
    case StringId::MenuSortName:       return zh ? L"名称(&N)" : L"Name(&N)";
    case StringId::MenuSortDate:       return zh ? L"修改日期(&D)" : L"Date modified(&D)";
    case StringId::MenuSortType:       return zh ? L"类型(&T)" : L"Type(&T)";
    case StringId::MenuSortSize:       return zh ? L"大小(&S)" : L"Size(&S)";
    case StringId::MenuSortAsc:        return zh ? L"递增(&A)" : L"Ascending(&A)";
    case StringId::MenuSortDesc:       return zh ? L"递减(&E)" : L"Descending(&E)";

    case StringId::MenuFavorites:      return zh ? L"收藏(&A)" : L"Favorites(&A)";
    case StringId::MenuAddFavorites:   return zh ? L"添加当前位置到收藏夹(&A)..." : L"Add Current Location to Favorites(&A)...";
    case StringId::MenuManageFavorites:return zh ? L"整理收藏夹 / 链接栏(&O)..." : L"Organize Favorites / Links(&O)...";

    case StringId::MenuGo:             return zh ? L"转到(&G)" : L"Go(&G)";
    case StringId::MenuBack:           return zh ? L"后退(&B)\tAlt+Left" : L"Back(&B)\tAlt+Left";
    case StringId::MenuForward:        return zh ? L"前进(&F)\tAlt+Right" : L"Forward(&F)\tAlt+Right";
    case StringId::MenuUp:             return zh ? L"向上一级(&U)\tAlt+Up" : L"Up One Level(&U)\tAlt+Up";
    case StringId::MenuAddressBar:     return zh ? L"转到地址栏(&L)\tCtrl+L" : L"Focus Address Bar(&L)\tCtrl+L";

    case StringId::MenuTabs:           return zh ? L"标签页(&B)" : L"Tabs(&B)";
    case StringId::MenuNextTab:        return zh ? L"下一个标签页(&N)\tCtrl+Tab" : L"Next Tab(&N)\tCtrl+Tab";
    case StringId::MenuPrevTab:        return zh ? L"上一个标签页(&P)\tCtrl+Shift+Tab" : L"Previous Tab(&P)\tCtrl+Shift+Tab";

    case StringId::MenuTools:          return zh ? L"工具(&T)" : L"Tools(&T)";
    case StringId::MenuTerminal:       return zh ? L"在此处打开 Windows Terminal(&W)\tCtrl+`" : L"Open Windows Terminal Here(&W)\tCtrl+`";
    case StringId::MenuPowerShell:     return zh ? L"在此处打开 PowerShell(&P)" : L"Open PowerShell Here(&P)";
    case StringId::MenuCmd:            return zh ? L"打开命令CMD(&C)" : L"Open CMD(&C)";
    case StringId::MenuVSCode:         return zh ? L"在此处用 VS Code 打开(&V)" : L"Open in VS Code Here(&V)";
    case StringId::MenuReloadScripts:  return zh ? L"重新载入扩展脚本(&R)\tCtrl+Shift+R" : L"Reload Extension Scripts(&R)\tCtrl+Shift+R";
    case StringId::MenuScriptConsole:  return zh ? L"扩展脚本控制台(&O)\tF12" : L"Script Console(&O)\tF12";

    case StringId::MenuLanguage:       return zh ? L"语言 / Language" : L"Language";
    case StringId::MenuLangChinese:    return L"简体中文 (Chinese)";
    case StringId::MenuLangEnglish:    return L"English";

    case StringId::MenuHelp:           return zh ? L"帮助(&H)" : L"Help(&H)";
    case StringId::MenuHelpShortcuts:  return zh ? L"键盘快捷键指南(&K)..." : L"Keyboard Shortcuts Cheatsheet(&K)...";
    case StringId::MenuHelpGestures:   return zh ? L"鼠标手势说明(&G)..." : L"Mouse Gestures Guide(&G)...";
    case StringId::MenuHelpMdieHome:   return zh ? L"MDIE 官方主页(&M)..." : L"MDIE Official Homepage(&M)...";
    case StringId::MenuHelpGitHub:     return zh ? L"GitHub 项目主页(&G)..." : L"GitHub Project Page(&G)...";
    case StringId::MenuAbout:          return zh ? L"关于 NeoMDIE(&A)..." : L"About NeoMDIE(&A)...";

    case StringId::ShortcutsCheatsheet:
        return zh ?
            L"【NeoMDIE 常用快捷键速查】\n\n"
            L"• 标签页管理:\n"
            L"  Ctrl + T : 新建标签页\n"
            L"  Ctrl + W : 关闭当前标签页\n"
            L"  Ctrl + Shift + T : 重新打开已关闭标签页\n"
            L"  Ctrl + K : 复制当前标签页\n"
            L"  Ctrl + Tab : 下一个标签页\n"
            L"  Ctrl + Shift + Tab : 上一个标签页\n\n"
            L"• 窗格与多栏分屏:\n"
            L"  Ctrl + 1 : 单栏视图\n"
            L"  Ctrl + 2 : 左右双栏\n"
            L"  Ctrl + 3 : 上下双栏\n"
            L"  Ctrl + 4 : 田字四栏\n"
            L"  Ctrl + 5 : 1左2右三栏\n"
            L"  Alt + P : 切换文件预览窗格\n\n"
            L"• 导航与查找:\n"
            L"  Alt + ← / Backspace : 后退\n"
            L"  Alt + → : 前进\n"
            L"  Alt + ↑ : 向上一级\n"
            L"  Ctrl + L / Alt + D / F6 : 转到地址栏\n"
            L"  Ctrl + F : 搜索与即时过滤\n"
            L"  F5 / Ctrl + R : 刷新目录\n\n"
            L"• 文件编辑与操作:\n"
            L"  Ctrl + C / X / V : 复制 / 剪切 / 粘贴\n"
            L"  Ctrl + Shift + C : 复制完整路径\n"
            L"  Del / Shift + Del : 删除 / 永久删除\n"
            L"  F2 : 重命名\n"
            L"  Ctrl + Shift + N : 新建文件夹\n\n"
            L"• 工具与终端:\n"
            L"  Ctrl + ` : 在当前目录打开 Windows Terminal\n"
            L"  F12 : 打开 JS 扩展脚本控制台\n"
            L"  Ctrl + Shift + R : 重新载入扩展脚本"
            :
            L"【NeoMDIE Keyboard Shortcuts Cheatsheet】\n\n"
            L"• Tab Management:\n"
            L"  Ctrl + T : New Tab\n"
            L"  Ctrl + W : Close Tab\n"
            L"  Ctrl + Shift + T : Reopen Closed Tab\n"
            L"  Ctrl + K : Duplicate Tab\n"
            L"  Ctrl + Tab : Next Tab\n"
            L"  Ctrl + Shift + Tab : Previous Tab\n\n"
            L"• Pane Layouts:\n"
            L"  Ctrl + 1 : Single Pane\n"
            L"  Ctrl + 2 : Dual Vertical Split\n"
            L"  Ctrl + 3 : Dual Horizontal Split\n"
            L"  Ctrl + 4 : Quad Grid Split\n"
            L"  Ctrl + 5 : Three Panes Split\n"
            L"  Alt + P : Toggle Preview Pane\n\n"
            L"• Navigation & Search:\n"
            L"  Alt + ← / Backspace : Back\n"
            L"  Alt + → : Forward\n"
            L"  Alt + ↑ : Up One Level\n"
            L"  Ctrl + L / Alt + D / F6 : Focus Address Bar\n"
            L"  Ctrl + F : Instant Search & Filter\n"
            L"  F5 / Ctrl + R : Refresh\n\n"
            L"• File Operations:\n"
            L"  Ctrl + C / X / V : Copy / Cut / Paste\n"
            L"  Ctrl + Shift + C : Copy Full Path\n"
            L"  Del / Shift + Del : Delete / Permanent Delete\n"
            L"  F2 : Rename\n"
            L"  Ctrl + Shift + N : New Folder\n\n"
            L"• Tools & Terminals:\n"
            L"  Ctrl + ` : Open Windows Terminal Here\n"
            L"  F12 : Script Console\n"
            L"  Ctrl + Shift + R : Reload Extension Scripts";

    case StringId::GesturesCheatsheet:
        return zh ?
            L"【NeoMDIE 鼠标手势使用指南】\n\n"
            L"在文件列表空白处或任意文件项上，按住鼠标【右键】并在屏幕上划动轨迹：\n\n"
            L"  ← (向左划) : 后退到上一访问文件夹\n"
            L"  → (向右划) : 前进到下一文件夹\n"
            L"  ↑ (向上划) : 返回上一级父目录\n"
            L"  ↓ (向下划) : 关闭当前标签页\n"
            L"  ↑↓ (先上后下) : 刷新当前目录\n"
            L"  ↓→ (先下后右) : 新建标签页\n\n"
            L"提示:\n"
            L"您可以在「工具 -> 选项与设置 -> 鼠标手势」中自由添加任意划动方向并绑定自定义操作！"
            :
            L"【NeoMDIE Mouse Gestures Guide】\n\n"
            L"Hold down the 【Right Mouse Button】 anywhere in the file view and stroke:\n\n"
            L"  ← (Left) : Back\n"
            L"  → (Right) : Forward\n"
            L"  ↑ (Up) : Up One Level\n"
            L"  ↓ (Down) : Close Current Tab\n"
            L"  ↑↓ (Up-Down) : Refresh\n"
            L"  ↓→ (Down-Right) : New Tab\n\n"
            L"Tip:\n"
            L"You can customize and add any gesture in Tools -> Options & Settings -> Gestures!";

    case StringId::TabMenuClose:        return zh ? L"关闭标签页" : L"Close Tab";
    case StringId::TabMenuCloseOthers:  return zh ? L"关闭其他标签页" : L"Close Other Tabs";
    case StringId::TabMenuCloseRight:   return zh ? L"关闭右侧标签页" : L"Close Tabs to the Right";
    case StringId::TabMenuPin:          return zh ? L"📌 固定标签页" : L"📌 Pin Tab";
    case StringId::TabMenuUnpin:        return zh ? L"取消固定标签页" : L"Unpin Tab";
    case StringId::TabMenuDuplicate:    return zh ? L"复制标签页" : L"Duplicate Tab";
    case StringId::TabMenuCopyPath:     return zh ? L"复制路径" : L"Copy Path";
    case StringId::TabMenuOpenTerminal: return zh ? L"在终端中打开" : L"Open in Terminal";

    case StringId::SettingsTitle:       return zh ? L"NeoMDIE 选项与设置" : L"NeoMDIE Options & Settings";
    case StringId::SettingsGeneral:     return zh ? L"常规设置" : L"General";
    case StringId::SettingsTheme:       return zh ? L"外观主题" : L"Appearance Theme";
    case StringId::SettingsLanguage:    return zh ? L"界面语言" : L"Interface Language";
    case StringId::SettingsShowTree:    return zh ? L"启动时默认显示左侧文件目录树" : L"Show Directory Tree on startup";
    case StringId::SettingsRestoreTabs: return zh ? L"启动时恢复上次关闭前的标签页" : L"Restore tabs from last session";
    case StringId::SettingsCalcFolderSize: return zh ? L"后台自动递归计算文件夹大小" : L"Calculate folder sizes in background";
    case StringId::SettingsGestures:    return zh ? L"启用右键平滑鼠标手势" : L"Enable smooth mouse gestures";
    case StringId::SettingsBtnOK:       return zh ? L"确定" : L"OK";
    case StringId::SettingsBtnCancel:   return zh ? L"取消" : L"Cancel";

    case StringId::Layout1x1:           return zh ? L"🗖 单栏视图 (1x1)" : L"🗖 Single Pane (1x1)";
    case StringId::Layout1x2:           return zh ? L"⫾ 左右双栏 (1x2)" : L"⫾ Dual Vertical (1x2)";
    case StringId::Layout2x1:           return zh ? L"⫽ 上下双栏 (2x1)" : L"⫽ Dual Horizontal (2x1)";
    case StringId::Layout2x2:           return zh ? L"⊞ 田字四栏 (2x2)" : L"⊞ Quad Grid (2x2)";
    case StringId::Layout1p2:           return zh ? L"⧉ 1左2右三栏 (1+2)" : L"⧉ Three Panes (1+2)";
    case StringId::LayoutSyncNav:       return zh ? L"同步浏览 (所有分栏联动)" : L"Synchronized Navigation";

    case StringId::AboutDialogText:
        return zh ?
            L"NeoMDIE v1.0.2\n"
            L"新一代轻量现代化多标签资源管理器 (复刻经典 MDIE)\n\n"
            L"特性:\n"
            L" • 极低内存开销 (< 5MB) 与瞬时启动\n"
            L" • 纯绿色便携免安装\n"
            L" • 默认左侧文件目录树导航\n"
            L" • 多标签页 / 多栏切换 (Ctrl+E)\n"
            L" • 中/英文双语无缝切换\n"
            L" • 右键鼠标手势 (←后退, →前进, ↑向上, ↓关标签)\n"
            L" • 毫秒级即时过滤查找 (Ctrl+F)\n"
            L" • Windows 原生 IPreviewHandler 预览窗格 (Alt+P)\n"
            L" • 现代 JS 扩展脚本引擎 (F12 控制台)\n"
            L" • 完美支持 Win10 / Win11 深色模式与高分屏"
            :
            L"NeoMDIE v1.0.2\n"
            L"Next-Generation Lightweight Tabbed File Manager (MDIE Remake)\n\n"
            L"Features:\n"
            L" • Ultra-low memory (< 5MB) and instant startup\n"
            L" • Portable and green (zero installation)\n"
            L" • Default left folder directory tree\n"
            L" • Multi-tabs / Dual-pane split view (Ctrl+E)\n"
            L" • English and Chinese bilingual support\n"
            L" • Mouse gestures (←Back, →Forward, ↑Up, ↓Close Tab)\n"
            L" • Incremental instant filtering (Ctrl+F)\n"
            L" • Native Windows IPreviewHandler Preview Pane (Alt+P)\n"
            L" • Modern JS extension script engine (F12 Console)\n"
            L" • Windows 10 & Windows 11 Dark Mode and High DPI";
    }
    return L"";
}
