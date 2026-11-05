#pragma once

#define IDI_APP_ICON                    101

#define IDC_NEOMDIE                     102
#define IDR_MAIN_ACCEL                  103

// Commands - File
#define ID_FILE_NEW_TAB                 40001
#define ID_FILE_CLOSE_TAB               40002
#define ID_FILE_DUPLICATE_TAB           40003
#define ID_FILE_NEW_FOLDER              40004
#define ID_FILE_NEW_FILE                40005
#define ID_FILE_PROPERTIES              40006
#define ID_FILE_EXIT                    40007
#define ID_FILE_CLOSE_OTHER_TABS        40008
#define ID_FILE_CLOSE_ALL_TABS          40009
#define ID_FILE_REOPEN_CLOSED_TAB       40018
#define ID_FILE_PIN_TAB                 40019

#define ID_FILE_NEW_WINDOW_SPECIFY      40090
#define ID_FILE_NEW_WINDOW_DESKTOP      40091
#define ID_FILE_NEW_WINDOW_CURRENT      40092
#define ID_FILE_NEW_WINDOW_CLIPBOARD    40093
#define ID_FILE_CLOSE_LEFT_TABS         40094
#define ID_FILE_CLOSE_RIGHT_TABS        40095
#define ID_FILE_OPEN_EXPLORER           40096
#define ID_FILE_CREATE_SHORTCUT         40097

// Commands - Edit
#define ID_EDIT_UNDO                    40098
#define ID_EDIT_CUT                     40010
#define ID_EDIT_COPY                    40011
#define ID_EDIT_PASTE                   40012
#define ID_EDIT_DELETE                  40013
#define ID_EDIT_RENAME                  40014
#define ID_EDIT_SELECT_ALL              40015
#define ID_EDIT_INVERT_SELECTION        40016
#define ID_EDIT_COPY_PATH               40017
#define ID_EDIT_COPY_NAME               40035
#define ID_EDIT_COPY_DIR                40036
#define ID_EDIT_PASTE_SHORTCUT          40037

// Commands - View
#define ID_VIEW_REFRESH                 40020
#define ID_VIEW_SPLIT_TOGGLE            40021
#define ID_VIEW_PREVIEW_TOGGLE          40022
#define ID_VIEW_FILTER_FOCUS            40023
#define ID_VIEW_ADDRESS_FOCUS           40024
#define ID_VIEW_DETAILS                 40025
#define ID_VIEW_ICONS_LARGE             40026
#define ID_VIEW_ICONS_SMALL             40027
#define ID_VIEW_LIST                    40028
#define ID_VIEW_TILES                   40029
#define ID_VIEW_SHOW_HIDDEN             40030
#define ID_VIEW_CALC_FOLDER_SIZE        40031
#define ID_VIEW_TREE_TOGGLE             40032
#define ID_VIEW_ICONS_XLARGE            40033
#define ID_VIEW_ICONS_MEDIUM            40034
#define ID_VIEW_SORT_NAME               40038
#define ID_VIEW_SORT_DATE               40039
#define ID_VIEW_SORT_TYPE               40044
#define ID_VIEW_SORT_SIZE               40045
#define ID_VIEW_SORT_ASC                40046
#define ID_VIEW_SORT_DESC               40047
#define ID_VIEW_TOGGLE_LINKS            40048

#define ID_VIEW_LOCK_TOOLBARS           40099
#define ID_VIEW_TB_STANDARD             40100
#define ID_VIEW_TB_ADDRESS              40101
#define ID_VIEW_TB_LINKS                40102
#define ID_VIEW_TB_SEARCH               40103
#define ID_VIEW_STATUSBAR               40104
#define ID_VIEW_MINIMIZE_TRAY           40105
#define ID_NAV_UP_NEW_TAB               40106

// Commands - Navigation
#define ID_NAV_BACK                     40040
#define ID_NAV_FORWARD                  40041
#define ID_NAV_UP                       40042
#define ID_NAV_HOME                     40043

// Commands - Tools
#define ID_TOOLS_OPEN_TERMINAL          40050
#define ID_TOOLS_OPEN_CMD               40051
#define ID_TOOLS_OPEN_POWERSHELL        40052
#define ID_TOOLS_OPEN_VSCODE            40053
#define ID_TOOLS_RELOAD_SCRIPTS         40054
#define ID_TOOLS_SCRIPT_CONSOLE         40055
#define ID_TOOLS_SETTINGS               40056
#define ID_TOOLS_GESTURES               40111

// Commands - Favorites
#define ID_FAVORITES_ADD                40061
#define ID_FAVORITES_ADD_LINKS          40110
#define ID_FAVORITES_MANAGE             40062
#define ID_FAV_THIS_PC                  40063
#define ID_FAV_DESKTOP                  40064
#define ID_FAV_DOWNLOADS                40065
#define ID_FAV_DOCUMENTS                40066
#define ID_FAV_PICTURES                 40067
#define ID_FAV_USER_PROFILE             40068

// Commands - Window Operations & Tabs
#define ID_TAB_NEXT                     40072
#define ID_TAB_PREV                     40073
#define ID_WINDOW_MAXIMIZE              40107
#define ID_WINDOW_MINIMIZE              40108
#define ID_WINDOW_CLOSE                 40109
#define ID_TOOLS_ALLOW_DUP_TABS         40112
#define ID_TOOLS_MIN_TO_TRAY            40113
#define ID_TOOLS_AUTO_EXPAND_TREE       40114
#define ID_TOOLS_OPEN_SCRIPTS_DIR       40115
#define ID_TOOLS_FOLDER_OPTIONS         40116
#define ID_WINDOW_MAIN_MAXIMIZE         40117
#define ID_WINDOW_MAIN_MINIMIZE         40118

// Commands - Help
#define ID_HELP_ABOUT                   40060
#define ID_HELP_SHORTCUTS               40074
#define ID_HELP_GESTURES                40075
#define ID_HELP_MDIE_HOMEPAGE           40076
#define ID_HELP_GITHUB                  40077
#define ID_HELP_CHECK_UPDATES           40165

// Scroll commands
#define ID_SCROLL_TOP                   40160
#define ID_SCROLL_PAGEUP                40161
#define ID_SCROLL_PAGEDOWN              40162
#define ID_SCROLL_BOTTOM                40163

// New Markdown
#define ID_FILE_NEW_MD                  40164

// Tools & Toolbar settings
#define ID_TOOLS_SHORTCUTS_SETTINGS     40166
#define ID_TOOLS_RESTORE_EXPLORER       40167
#define ID_VIEW_TB_SWAP_ORDER           40167
#define ID_VIEW_TB_RESET                40168
#define ID_VIEW_DARK_MODE               40169

// Language
#define ID_LANG_ZH                      40070
#define ID_LANG_EN                      40071

// Layouts
#define ID_VIEW_LAYOUT_1X1              40081
#define ID_VIEW_LAYOUT_1X2              40082
#define ID_VIEW_LAYOUT_2X1              40083
#define ID_VIEW_LAYOUT_2X2              40084
#define ID_VIEW_LAYOUT_1P2              40085
#define ID_VIEW_SYNC_NAV                40086

// Dynamic command ranges
#define ID_SCRIPT_COMMAND_START         41000
#define ID_SCRIPT_COMMAND_END           41999
#define ID_FAV_DRIVE_START              42000
#define ID_FAV_DRIVE_END                42050
#define ID_FAV_ITEM_START               42100
#define ID_FAV_ITEM_END                 42200
#define ID_HISTORY_START                42300
#define ID_HISTORY_END                  42350
#define ID_TAB_ITEM_START               42400
#define ID_TAB_ITEM_END                 42450
#define ID_RECENT_CLOSED_START          42500
#define ID_RECENT_CLOSED_END            42520
