#pragma once
#include "ShellItem.h"

enum NeoMDIECommand {
    CMD_NEOMDIE_OPEN_NEW_TAB = 0x7101,
    CMD_NEOMDIE_OPEN_SPLIT   = 0x7102,
    CMD_NEOMDIE_COPY_PATH    = 0x7103,
    CMD_NEOMDIE_TERMINAL     = 0x7104,
    CMD_NEOMDIE_POWERSHELL   = 0x7105,
    CMD_NEOMDIE_CMD          = 0x7106,
    CMD_NEOMDIE_ADD_LINKS    = 0x7107,
    CMD_NEOMDIE_SETTINGS     = 0x7108,

    // Background view modes
    CMD_BG_VIEW_DETAILS      = 0x7110,
    CMD_BG_VIEW_LARGE        = 0x7111,
    CMD_BG_VIEW_XLARGE       = 0x7112,
    CMD_BG_VIEW_MEDIUM       = 0x7113,
    CMD_BG_VIEW_SMALL        = 0x7114,
    CMD_BG_VIEW_LIST         = 0x7115,
    CMD_BG_VIEW_TILES        = 0x7116,

    // Background sort by
    CMD_BG_SORT_NAME         = 0x7120,
    CMD_BG_SORT_DATE         = 0x7121,
    CMD_BG_SORT_TYPE         = 0x7122,
    CMD_BG_SORT_SIZE         = 0x7123,
    CMD_BG_SORT_ASCENDING    = 0x7124,
    CMD_BG_SORT_DESCENDING   = 0x7125,

    // Background actions
    CMD_BG_REFRESH           = 0x7130,
    CMD_BG_PASTE             = 0x7131,
    CMD_BG_PASTE_SHORTCUT    = 0x7132,
    CMD_BG_UNDO              = 0x7133,
    CMD_BG_PROPERTIES        = 0x7134,
    CMD_BG_NEW_FOLDER        = 0x7135,
    CMD_BG_NEW_FILE          = 0x7136,
    CMD_BG_NEW_MD            = 0x7137
};

namespace ShellUtils {
    void Initialize();
    HIMAGELIST GetSystemImageList(bool large);

    int GetItemIconIndex(const std::wstring& path, DWORD attr, bool isFolder);
    std::wstring GetItemTypeName(const std::wstring& path, DWORD attr);

    bool EnumerateDirectory(const std::wstring& path, bool showHidden, std::vector<NeoShellItem>& outItems);
    void EnumerateDrives(std::vector<NeoShellItem>& outDrives);

    bool OpenItem(HWND hwnd, const std::wstring& path);
    bool ShowItemProperties(HWND hwnd, const std::wstring& path);
    bool ShowContextMenu(HWND hwnd, const std::vector<std::wstring>& paths, POINT ptScreen, std::function<void(int neoCmd, const std::wstring& path)> onCustomCommand = nullptr);
    bool ShowBackgroundContextMenu(HWND hwnd, const std::wstring& currentDir, POINT ptScreen, int currentViewMode = 0, int currentSortCol = 0, bool sortAsc = true, std::function<void(int neoCmd, const std::wstring& currentDir)> onCustomCommand = nullptr);

    // Clipboard & File Operations
    bool CopyFilesToClipboard(HWND hwnd, const std::vector<std::wstring>& paths, bool isCut);
    bool PasteFilesFromClipboard(HWND hwnd, const std::wstring& destDir);
    bool PasteShortcut(HWND hwnd, const std::wstring& destDir);
    bool DeleteFiles(HWND hwnd, const std::vector<std::wstring>& paths, bool permanent);
    bool RenameItem(HWND hwnd, const std::wstring& oldPath, const std::wstring& newName);
    bool CanPasteFromClipboard();
    bool GetDriveSpace(const std::wstring& drivePath, uint64_t& outFreeBytes, uint64_t& outTotalBytes);

    void LaunchTerminal(const std::wstring& dir);
    void LaunchPowerShell(const std::wstring& dir);
    void LaunchCmd(const std::wstring& dir);
    void LaunchVSCode(const std::wstring& dir);

    void CopyToClipboard(const std::wstring& text);
    std::wstring GetParentPath(const std::wstring& path);
    std::wstring GetDisplayName(const std::wstring& path);
    bool CreateShortcut(const std::wstring& shortcutPath, const std::wstring& targetPath, const std::wstring& args = L"", const std::wstring& desc = L"");

    // Replace Windows Explorer
    bool IsExplorerReplaced();
    bool SetExplorerReplaced(bool enable);
    void CheckForUpdates(HWND hwndParent);
}
