#pragma once
#include "..\Common.h"

enum class Language {
    Chinese = 0,
    English = 1
};

enum class StringId {
    AppName,
    ThisPC,
    QuickAccess,
    Desktop,
    Downloads,
    Documents,
    Pictures,
    Music,
    Videos,
    NewFolder,
    NewTextFile,
    Folder,
    DriveLocal,
    DriveRemovable,
    DriveCD,
    DriveNetwork,
    ColName,
    ColDateModified,
    ColType,
    ColSize,
    BtnTree,
    BtnSplit,
    BtnPreview,
    FilterCue,
    StatusItems,
    StatusSize,
    StatusReady,
    PreviewSelectHint,
    PreviewNoHandler,
    GestureBack,
    GestureForward,
    GestureUp,
    GestureCloseTab,
    GestureRefresh,
    GestureNewTab,
    MenuFile,
    MenuNewTab,
    MenuCloseTab,
    MenuCloseOtherTabs,
    MenuCloseAllTabs,
    MenuReopenClosedTab,
    MenuDuplicateTab,
    MenuPinTab,
    MenuUnpinTab,
    MenuNewFolder,
    MenuNewFile,
    MenuProperties,
    MenuExit,
    MenuEdit,
    MenuCut,
    MenuCopy,
    MenuCopyName,
    MenuCopyDir,
    MenuPaste,
    MenuPasteShortcut,
    MenuDelete,
    MenuRename,
    MenuSelectAll,
    MenuInvertSelection,
    MenuCopyPath,
    MenuView,
    MenuRefresh,
    MenuTree,
    MenuSplit,
    MenuPreview,
    MenuFilter,
    MenuDetails,
    MenuLargeIcons,
    MenuSmallIcons,
    MenuList,
    MenuShowHidden,
    MenuCalcFolderSize,
    MenuSortBy,
    MenuSortName,
    MenuSortDate,
    MenuSortType,
    MenuSortSize,
    MenuSortAsc,
    MenuSortDesc,
    MenuFavorites,
    MenuAddFavorites,
    MenuManageFavorites,
    MenuGo,
    MenuBack,
    MenuForward,
    MenuUp,
    MenuAddressBar,
    MenuTabs,
    MenuNextTab,
    MenuPrevTab,
    MenuTools,
    MenuTerminal,
    MenuPowerShell,
    MenuCmd,
    MenuVSCode,
    MenuReloadScripts,
    MenuScriptConsole,
    MenuLanguage,
    MenuLangChinese,
    MenuLangEnglish,
    MenuHelp,
    MenuAbout,
    MenuHelpShortcuts,
    MenuHelpGestures,
    MenuHelpMdieHome,
    MenuHelpGitHub,
    ShortcutsCheatsheet,
    GesturesCheatsheet,
    AboutDialogText,

    // Tab context menu
    TabMenuClose,
    TabMenuCloseOthers,
    TabMenuCloseRight,
    TabMenuPin,
    TabMenuUnpin,
    TabMenuDuplicate,
    TabMenuCopyPath,
    TabMenuOpenTerminal,

    // Settings
    SettingsTitle,
    SettingsGeneral,
    SettingsTheme,
    SettingsLanguage,
    SettingsShowTree,
    SettingsRestoreTabs,
    SettingsCalcFolderSize,
    SettingsGestures,
    SettingsBtnOK,
    SettingsBtnCancel,

    // Layout
    Layout1x1,
    Layout1x2,
    Layout2x1,
    Layout2x2,
    Layout1p2,
    LayoutSyncNav
};

class I18n {
public:
    static I18n& Get();

    void Initialize();
    void SetLanguage(Language lang);
    Language GetLanguage() const { return m_lang; }
    bool IsChinese() const { return m_lang == Language::Chinese; }

    const wchar_t* T(StringId id) const;

    // Callback when language changes
    std::function<void()> onLanguageChanged;

private:
    I18n();
    Language m_lang = Language::Chinese;
};
