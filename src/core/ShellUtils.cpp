#include "ShellUtils.h"
#include "I18n.h"

namespace ShellUtils {

static HIMAGELIST s_himlSmall = nullptr;
static HIMAGELIST s_himlLarge = nullptr;

void Initialize() {
    HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
    if (hShell32) {
        typedef BOOL(WINAPI* fnFileIconInit)(BOOL fRestoreCache);
        auto pfnFileIconInit = (fnFileIconInit)GetProcAddress(hShell32, MAKEINTRESOURCEA(660));
        if (pfnFileIconInit) {
            pfnFileIconInit(TRUE);
        }
    }

    SHFILEINFOW sfi = { 0 };
    s_himlSmall = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    s_himlLarge = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
}

HIMAGELIST GetSystemImageList(bool large) {
    if (!s_himlSmall || !s_himlLarge) {
        Initialize();
    }
    return large ? s_himlLarge : s_himlSmall;
}

static int s_defaultFolderIcon = -1;
static std::unordered_map<std::wstring, int> s_extIconCache;
static std::unordered_map<std::wstring, std::wstring> s_extTypeCache;

int GetItemIconIndex(const std::wstring& path, DWORD attr, bool isFolder) {
    if (isFolder) {
        if (path.length() <= 3) {
            SHFILEINFOW sfi = { 0 };
            if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON) != 0 && sfi.iIcon >= 0) {
                return sfi.iIcon;
            }
        }
        if (s_defaultFolderIcon >= 0) {
            return s_defaultFolderIcon;
        }
        SHFILEINFOW sfi = { 0 };
        SHGetFileInfoW(L"dummy_folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                       SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
        s_defaultFolderIcon = sfi.iIcon;
        return s_defaultFolderIcon;
    }

    const wchar_t* pExt = PathFindExtensionW(path.c_str());
    std::wstring ext = pExt ? pExt : L"";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    bool isCustomFile = (ext == L".exe" || ext == L".ico" || ext == L".lnk" || ext == L".cur" || ext == L".scr");
    if (!isCustomFile && !ext.empty()) {
        auto it = s_extIconCache.find(ext);
        if (it != s_extIconCache.end()) {
            return it->second;
        }
    }

    SHFILEINFOW sfi = { 0 };
    DWORD flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    if (isCustomFile && !path.empty() && PathFileExistsW(path.c_str())) {
        if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), flags) != 0 && sfi.iIcon >= 0) {
            return sfi.iIcon;
        }
    }

    SHGetFileInfoW((L"dummy" + ext).c_str(), attr ? attr : FILE_ATTRIBUTE_NORMAL,
                   &sfi, sizeof(sfi), flags | SHGFI_USEFILEATTRIBUTES);
    if (!isCustomFile && !ext.empty()) {
        s_extIconCache[ext] = sfi.iIcon;
    }
    return sfi.iIcon;
}

std::wstring GetItemTypeName(const std::wstring& path, DWORD attr) {
    const wchar_t* pExt = PathFindExtensionW(path.c_str());
    std::wstring ext = pExt ? pExt : L"";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    if (!ext.empty()) {
        auto it = s_extTypeCache.find(ext);
        if (it != s_extTypeCache.end()) {
            return it->second;
        }
    }

    SHFILEINFOW sfi = { 0 };
    SHGetFileInfoW((L"dummy" + ext).c_str(), attr ? attr : FILE_ATTRIBUTE_NORMAL,
                   &sfi, sizeof(sfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
    std::wstring typeName = sfi.szTypeName;
    if (!ext.empty() && !typeName.empty()) {
        s_extTypeCache[ext] = typeName;
    }
    return typeName;
}

void EnumerateDrives(std::vector<NeoShellItem>& outDrives) {
    wchar_t driveStrings[512] = { 0 };
    DWORD len = GetLogicalDriveStringsW(512, driveStrings);
    if (len == 0) return;

    wchar_t* pDrive = driveStrings;
    while (*pDrive) {
        NeoShellItem item;
        item.fullPath = pDrive;
        item.name = pDrive;
        item.isFolder = true;
        item.isDrive = true;
        item.attributes = FILE_ATTRIBUTE_DIRECTORY;

        // Drive volume label and type
        wchar_t volName[MAX_PATH] = { 0 };
        wchar_t fsName[MAX_PATH] = { 0 };
        GetVolumeInformationW(pDrive, volName, MAX_PATH, nullptr, nullptr, nullptr, fsName, MAX_PATH);

        ULARGE_INTEGER freeBytes, totalBytes, totalFree;
        if (GetDiskFreeSpaceExW(pDrive, &freeBytes, &totalBytes, &totalFree)) {
            item.size = totalBytes.QuadPart;
        }

        UINT driveType = GetDriveTypeW(pDrive);
        std::wstring typeStr = I18n::Get().T(StringId::DriveLocal);
        if (driveType == DRIVE_REMOVABLE) typeStr = I18n::Get().T(StringId::DriveRemovable);
        else if (driveType == DRIVE_CDROM) typeStr = I18n::Get().T(StringId::DriveCD);
        else if (driveType == DRIVE_REMOTE) typeStr = I18n::Get().T(StringId::DriveNetwork);

        if (wcslen(volName) > 0) {
            item.name = std::wstring(volName) + L" (" + pDrive[0] + L":)";
        } else {
            item.name = typeStr + L" (" + pDrive[0] + L":)";
        }

        item.typeName = typeStr;
        item.iconIndex = GetItemIconIndex(pDrive, FILE_ATTRIBUTE_DIRECTORY, true);

        outDrives.push_back(item);
        pDrive += wcslen(pDrive) + 1;
    }
}

bool EnumerateDirectory(const std::wstring& path, bool showHidden, std::vector<NeoShellItem>& outItems) {
    outItems.clear();

    // Check if it's "This PC" / My Computer
    if (path == L"此电脑" || path == L"This PC" || path.empty()) {
        EnumerateDrives(outItems);
        return true;
    }

    std::wstring searchPath = Util::NormalizePath(path) + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoBasic, &fd,
                                    FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::vector<NeoShellItem> folders;
    std::vector<NeoShellItem> files;

    do {
        // Skip . and ..
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        if (!showHidden && (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
            continue;
        }

        NeoShellItem item;
        item.name = fd.cFileName;
        item.fullPath = Util::CombinePath(path, fd.cFileName);
        item.attributes = fd.dwFileAttributes;
        item.modifiedTime = fd.ftLastWriteTime;
        item.createdTime = fd.ftCreationTime;
        item.isFolder = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        // Extension
        const wchar_t* pExt = PathFindExtensionW(fd.cFileName);
        item.ext = pExt ? pExt : L"";

        if (item.isFolder) {
            item.size = 0;
            item.isSizeCalculated = false;
            item.typeName = I18n::Get().T(StringId::Folder);
            item.iconIndex = GetItemIconIndex(item.fullPath, fd.dwFileAttributes, true);
            folders.push_back(item);
        } else {
            ULARGE_INTEGER uli;
            uli.LowPart = fd.nFileSizeLow;
            uli.HighPart = fd.nFileSizeHigh;
            item.size = uli.QuadPart;
            item.isSizeCalculated = true;
            item.typeName = GetItemTypeName(item.fullPath, fd.dwFileAttributes);
            item.iconIndex = GetItemIconIndex(item.fullPath, fd.dwFileAttributes, false);
            files.push_back(item);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    outItems.reserve(folders.size() + files.size());
    outItems.insert(outItems.end(), folders.begin(), folders.end());
    outItems.insert(outItems.end(), files.begin(), files.end());
    return true;
}

bool OpenItem(HWND hwnd, const std::wstring& path) {
    HINSTANCE hInst = ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)hInst > 32;
}

bool ShowItemProperties(HWND hwnd, const std::wstring& path) {
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.hwnd = hwnd;
    sei.lpVerb = L"properties";
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

static IContextMenu2* g_pContextMenu2 = nullptr;
static IContextMenu3* g_pContextMenu3 = nullptr;

static LRESULT CALLBACK ContextMenuSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
    case WM_INITMENUPOPUP:
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
        if (g_pContextMenu3) {
            LRESULT lres = 0;
            if (SUCCEEDED(g_pContextMenu3->HandleMenuMsg2(msg, wParam, lParam, &lres))) {
                return lres;
            }
        } else if (g_pContextMenu2) {
            if (SUCCEEDED(g_pContextMenu2->HandleMenuMsg(msg, wParam, lParam))) {
                return 0;
            }
        }
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool ShowContextMenu(HWND hwnd, const std::vector<std::wstring>& paths, POINT ptScreen, std::function<void(int neoCmd, const std::wstring& path)> onCustomCommand) {
    if (paths.empty()) return false;

    bool shown = false;
    std::wstring firstPath = Util::NormalizePath(paths[0]);
    PIDLIST_ABSOLUTE pidlFirst = ILCreateFromPathW(firstPath.c_str());

    IShellFolder* pFolder = nullptr;
    PCUITEMID_CHILD pidlFirstChild = nullptr;

    if (pidlFirst) {
        if (SUCCEEDED(SHBindToParent(pidlFirst, IID_IShellFolder, (void**)&pFolder, &pidlFirstChild)) && pFolder) {
            std::vector<PCUITEMID_CHILD> childPidls;
            childPidls.push_back(pidlFirstChild);

            std::vector<PIDLIST_ABSOLUTE> extraPidls;
            for (size_t i = 1; i < paths.size(); ++i) {
                PIDLIST_ABSOLUTE pItem = ILCreateFromPathW(Util::NormalizePath(paths[i]).c_str());
                if (pItem) {
                    extraPidls.push_back(pItem);
                    childPidls.push_back(ILFindLastID(pItem));
                }
            }

            IContextMenu* pContextMenu = nullptr;
            HRESULT hr = pFolder->GetUIObjectOf(hwnd, (UINT)childPidls.size(), childPidls.data(),
                                                IID_IContextMenu, nullptr, (void**)&pContextMenu);

            if (SUCCEEDED(hr) && pContextMenu) {
                pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu2));
                pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu3));

                HMENU hMenu = CreatePopupMenu();
                bool isDir = PathIsDirectoryW(firstPath.c_str()) != FALSE;
                bool isZh = I18n::Get().IsChinese();

                // 1. Elevate NeoMDIE Custom Commands TO THE VERY TOP
                if (isDir) {
                    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_NEW_TAB, isZh ? L"在新标签页中打开(&T)" : L"Open in New Tab(&T)");
                    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_SPLIT, isZh ? L"在分屏中打开(&S)" : L"Open in Split Pane(&S)");
                }
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制完整路径(&A)" : L"Copy Full Path(&A)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_TERMINAL, isZh ? L"在此处打开命令提示符 CMD(&C)" : L"Open CMD Here(&C)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

                int topItemsCount = GetMenuItemCount(hMenu);
                pContextMenu->QueryContextMenu(hMenu, topItemsCount, 1, 0x6FFF, CMF_NORMAL | CMF_CANRENAME);

                SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);

                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                         ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

                RemoveWindowSubclass(hwnd, ContextMenuSubclassProc, 9999);

                if (g_pContextMenu3) { g_pContextMenu3->Release(); g_pContextMenu3 = nullptr; }
                if (g_pContextMenu2) { g_pContextMenu2->Release(); g_pContextMenu2 = nullptr; }

                if (cmd >= 0x7101 && cmd <= 0x7108) {
                    DestroyMenu(hMenu);
                    pContextMenu->Release();
                    for (auto p : extraPidls) ILFree(p);
                    pFolder->Release();
                    ILFree(pidlFirst);

                    if (onCustomCommand) {
                        onCustomCommand(cmd, firstPath);
                    }
                    return true;
                } else if (cmd > 0) {
                    CMINVOKECOMMANDINFOEX cmi = { 0 };
                    cmi.cbSize = sizeof(cmi);
                    cmi.fMask = CMIC_MASK_UNICODE;
                    cmi.hwnd = hwnd;
                    cmi.lpVerb = MAKEINTRESOURCEA(cmd - 1);
                    cmi.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
                    cmi.nShow = SW_SHOWNORMAL;
                    pContextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
                }

                DestroyMenu(hMenu);
                pContextMenu->Release();
                shown = true;
            }

            for (auto p : extraPidls) {
                ILFree(p);
            }
            pFolder->Release();
        }
        ILFree(pidlFirst);
    }

    // High reliability fallback menu if shell COM failed
    if (!shown) {
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1, I18n::Get().IsChinese() ? L"打开(&O)" : L"Open(&O)");
        AppendMenuW(hMenu, MF_STRING, 2, I18n::Get().T(StringId::MenuTerminal));
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 3, I18n::Get().T(StringId::MenuCut));
        AppendMenuW(hMenu, MF_STRING, 4, I18n::Get().T(StringId::MenuCopy));
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 5, I18n::Get().T(StringId::MenuDelete));
        AppendMenuW(hMenu, MF_STRING, 6, I18n::Get().T(StringId::MenuRename));
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 7, I18n::Get().T(StringId::MenuCopyPath));
        AppendMenuW(hMenu, MF_STRING, 8, I18n::Get().T(StringId::MenuProperties));

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);

        switch (cmd) {
        case 1: OpenItem(hwnd, paths[0]); break;
        case 2: LaunchTerminal(paths[0]); break;
        case 3: CopyFilesToClipboard(hwnd, paths, true); break;
        case 4: CopyFilesToClipboard(hwnd, paths, false); break;
        case 5: DeleteFiles(hwnd, paths, false); break;
        case 6: PostMessageW(hwnd, WM_KEYDOWN, VK_F2, 0); break;
        case 7: CopyToClipboard(paths[0]); break;
        case 8: ShowItemProperties(hwnd, paths[0]); break;
        }
    }

    return true;
}

bool ShowBackgroundContextMenu(HWND hwnd, const std::wstring& currentDir, POINT ptScreen,
                               int currentViewMode, int currentSortCol, bool sortAsc,
                               std::function<void(int neoCmd, const std::wstring& currentDir)> onCustomCommand) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();

    // 0. Top NeoMDIE shortcuts
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_CMD, isZh ? L"在此处打开命令提示符 CMD(&C)" : L"Open CMD Here(&C)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制当前路径(&A)" : L"Copy Current Path(&A)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_NEW_TAB, isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // 1. View submenu
    HMENU hSubView = CreatePopupMenu();
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_XLARGE, isZh ? L"超大图标(&X)" : L"Extra large icons(&X)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_LARGE, isZh ? L"大图标(&R)" : L"Large icons(&R)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_MEDIUM, isZh ? L"中等图标(&M)" : L"Medium icons(&M)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_SMALL, isZh ? L"小图标(&N)" : L"Small icons(&N)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_LIST, isZh ? L"列表(&L)" : L"List(&L)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_DETAILS, isZh ? L"详细信息(&D)" : L"Details(&D)");
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_TILES, isZh ? L"平铺(&S)" : L"Tiles(&S)");

    int checkViewCmd = CMD_BG_VIEW_DETAILS;
    if (currentViewMode == 0) checkViewCmd = CMD_BG_VIEW_DETAILS;
    else if (currentViewMode == 1) checkViewCmd = CMD_BG_VIEW_LARGE;
    else if (currentViewMode == 2) checkViewCmd = CMD_BG_VIEW_SMALL;
    else if (currentViewMode == 3) checkViewCmd = CMD_BG_VIEW_LIST;
    else if (currentViewMode == 4) checkViewCmd = CMD_BG_VIEW_TILES;
    CheckMenuRadioItem(hSubView, CMD_BG_VIEW_DETAILS, CMD_BG_VIEW_TILES, checkViewCmd, MF_BYCOMMAND);
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubView, isZh ? L"查看(&V)" : L"View(&V)");

    // 2. Sort by submenu
    HMENU hSubSort = CreatePopupMenu();
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_NAME, isZh ? L"名称" : L"Name");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_DATE, isZh ? L"修改日期" : L"Date modified");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_TYPE, isZh ? L"类型" : L"Type");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_SIZE, isZh ? L"大小" : L"Size");
    AppendMenuW(hSubSort, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_ASCENDING, isZh ? L"递增" : L"Ascending");
    AppendMenuW(hSubSort, MF_STRING, CMD_BG_SORT_DESCENDING, isZh ? L"递减" : L"Descending");

    int checkSortCmd = CMD_BG_SORT_NAME + std::clamp(currentSortCol, 0, 3);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_NAME, CMD_BG_SORT_SIZE, checkSortCmd, MF_BYCOMMAND);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_ASCENDING, CMD_BG_SORT_DESCENDING,
                       sortAsc ? CMD_BG_SORT_ASCENDING : CMD_BG_SORT_DESCENDING, MF_BYCOMMAND);
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubSort, isZh ? L"排序方式(&O)" : L"Sort by(&O)");

    // 3. Refresh
    AppendMenuW(hMenu, MF_STRING, CMD_BG_REFRESH, isZh ? L"刷新(&E)\tF5" : L"Refresh(&E)\tF5");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // 4. Paste / Paste Shortcut / Undo
    bool canPaste = CanPasteFromClipboard() && currentDir != L"This PC" && currentDir != L"此电脑";
    AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), CMD_BG_PASTE, isZh ? L"粘贴(&P)\tCtrl+V" : L"Paste(&P)\tCtrl+V");
    AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), CMD_BG_PASTE_SHORTCUT, isZh ? L"粘贴快捷方式(&S)" : L"Paste shortcut(&S)");
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, CMD_BG_UNDO, isZh ? L"撤消(&U)\tCtrl+Z" : L"Undo(&U)\tCtrl+Z");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // 5. Query Shell Background Context Menu
    IContextMenu* pContextMenu = nullptr;
    IShellFolder* pFolder = nullptr;
    PIDLIST_ABSOLUTE pidl = nullptr;
    HMENU hShellMenu = nullptr;

    if (!currentDir.empty() && currentDir != L"This PC" && currentDir != L"此电脑") {
        pidl = ILCreateFromPathW(Util::NormalizePath(currentDir).c_str());
        if (pidl) {
            if (SUCCEEDED(SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void**)&pFolder)) && pFolder) {
                if (SUCCEEDED(pFolder->CreateViewObject(hwnd, IID_IContextMenu, (void**)&pContextMenu)) && pContextMenu) {
                    pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu2));
                    pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu3));

                    hShellMenu = CreatePopupMenu();
                    pContextMenu->QueryContextMenu(hShellMenu, 0, 1, 0x6FFF, CMF_NORMAL | CMF_EXPLORE);

                    // Transfer items from shell menu to our menu
                    int count = GetMenuItemCount(hShellMenu);
                    for (int i = 0; i < count; ++i) {
                        wchar_t text[256] = { 0 };
                        MENUITEMINFOW mii = { sizeof(mii) };
                        mii.fMask = MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_TYPE | MIIM_DATA | MIIM_BITMAP;
                        mii.dwTypeData = text;
                        mii.cch = 256;
                        if (GetMenuItemInfoW(hShellMenu, i, TRUE, &mii)) {
                            if (mii.hSubMenu) {
                                RemoveMenu(hShellMenu, i, MF_BYPOSITION);
                                InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
                                i--;
                                count--;
                            } else {
                                InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!pContextMenu) {
        // Fallback New submenu if shell COM not available
        HMENU hSubNew = CreatePopupMenu();
        AppendMenuW(hSubNew, MF_STRING, CMD_BG_NEW_FOLDER, I18n::Get().T(StringId::MenuNewFolder));
        AppendMenuW(hSubNew, MF_STRING, CMD_BG_NEW_FILE, I18n::Get().T(StringId::MenuNewFile));
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubNew, isZh ? L"新建(&W)" : L"New(&W)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, CMD_BG_PROPERTIES, I18n::Get().T(StringId::MenuProperties));
    }

    // Append NeoMDIE specific commands
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_SETTINGS, isZh ? L"选项与设置(&S)..." : L"Options & Settings(&S)...");

    SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

    RemoveWindowSubclass(hwnd, ContextMenuSubclassProc, 9999);

    if (g_pContextMenu3) { g_pContextMenu3->Release(); g_pContextMenu3 = nullptr; }
    if (g_pContextMenu2) { g_pContextMenu2->Release(); g_pContextMenu2 = nullptr; }

    DestroyMenu(hMenu);
    if (hShellMenu) DestroyMenu(hShellMenu);

    if (cmd >= 0x7100) {
        if (pContextMenu) { pContextMenu->Release(); pContextMenu = nullptr; }
        if (pFolder) { pFolder->Release(); pFolder = nullptr; }
        if (pidl) { ILFree(pidl); pidl = nullptr; }

        if (onCustomCommand) {
            onCustomCommand(cmd, currentDir);
        }
        return true;
    } else if (cmd > 0 && pContextMenu) {
        CMINVOKECOMMANDINFOEX cmi = { 0 };
        cmi.cbSize = sizeof(cmi);
        cmi.fMask = CMIC_MASK_UNICODE;
        cmi.hwnd = hwnd;
        cmi.lpVerb = MAKEINTRESOURCEA(cmd - 1);
        cmi.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
        cmi.nShow = SW_SHOWNORMAL;
        pContextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
    }

    if (pContextMenu) pContextMenu->Release();
    if (pFolder) pFolder->Release();
    if (pidl) ILFree(pidl);

    return true;
}

bool CopyFilesToClipboard(HWND hwnd, const std::vector<std::wstring>& paths, bool isCut) {
    if (paths.empty()) return false;
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();

    // Calculate buffer size for double-null-terminated string list
    size_t totalChars = 0;
    for (const auto& p : paths) {
        totalChars += p.length() + 1;
    }
    totalChars += 1; // Final terminating null

    size_t dropFilesBytes = sizeof(DROPFILES) + totalChars * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GHND, dropFilesBytes);
    if (!hGlobal) {
        CloseClipboard();
        return false;
    }

    DROPFILES* pdf = (DROPFILES*)GlobalLock(hGlobal);
    pdf->pFiles = sizeof(DROPFILES);
    pdf->fWide = TRUE;

    wchar_t* pDest = (wchar_t*)((BYTE*)pdf + sizeof(DROPFILES));
    for (const auto& p : paths) {
        StringCchCopyW(pDest, p.length() + 1, p.c_str());
        pDest += p.length() + 1;
    }
    *pDest = L'\0'; // Final null
    GlobalUnlock(hGlobal);

    SetClipboardData(CF_HDROP, hGlobal);

    // Preferred DropEffect (DROPEFFECT_COPY or DROPEFFECT_MOVE)
    UINT uDropEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
    HGLOBAL hEffect = GlobalAlloc(GHND, sizeof(DWORD));
    if (hEffect) {
        DWORD* pdwEffect = (DWORD*)GlobalLock(hEffect);
        *pdwEffect = isCut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
        GlobalUnlock(hEffect);
        SetClipboardData(uDropEffect, hEffect);
    }

    CloseClipboard();
    return true;
}

bool CanPasteFromClipboard() {
    return IsClipboardFormatAvailable(CF_HDROP) != FALSE;
}

bool PasteFilesFromClipboard(HWND hwnd, const std::wstring& destDir) {
    if (destDir.empty() || !PathFileExistsW(destDir.c_str())) return false;
    if (!OpenClipboard(hwnd)) return false;

    HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
    if (!hDrop) {
        CloseClipboard();
        return false;
    }

    // Check if it's cut or copy
    bool isMove = false;
    UINT uDropEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
    HGLOBAL hEffect = (HGLOBAL)GetClipboardData(uDropEffect);
    if (hEffect) {
        DWORD* pdwEffect = (DWORD*)GlobalLock(hEffect);
        if (pdwEffect && (*pdwEffect & DROPEFFECT_MOVE)) {
            isMove = true;
        }
        GlobalUnlock(hEffect);
    }

    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> filesToPaste;
    for (UINT i = 0; i < count; ++i) {
        wchar_t buf[MAX_PATH];
        if (DragQueryFileW(hDrop, i, buf, MAX_PATH)) {
            filesToPaste.push_back(buf);
        }
    }
    CloseClipboard();

    if (filesToPaste.empty()) return false;

    // Use IFileOperation for native Windows progress dialog & undo
    IFileOperation* pfo = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
    if (SUCCEEDED(hr) && pfo) {
        pfo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR);
        pfo->SetOwnerWindow(hwnd);

        IShellItem* psiDest = nullptr;
        hr = SHCreateItemFromParsingName(destDir.c_str(), nullptr, IID_PPV_ARGS(&psiDest));
        if (SUCCEEDED(hr) && psiDest) {
            for (const auto& src : filesToPaste) {
                IShellItem* psiSrc = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(src.c_str(), nullptr, IID_PPV_ARGS(&psiSrc)))) {
                    if (isMove) {
                        pfo->MoveItem(psiSrc, psiDest, nullptr, nullptr);
                    } else {
                        pfo->CopyItem(psiSrc, psiDest, nullptr, nullptr);
                    }
                    psiSrc->Release();
                }
            }
            pfo->PerformOperations();
            psiDest->Release();
        }
        pfo->Release();

        // If it was cut, clear the clipboard after moving
        if (isMove) {
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                CloseClipboard();
            }
        }
        return true;
    }

    return false;
}

bool PasteShortcut(HWND hwnd, const std::wstring& destDir) {
    if (destDir.empty() || !PathFileExistsW(destDir.c_str())) return false;
    if (!OpenClipboard(hwnd)) return false;

    HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
    if (!hDrop) {
        CloseClipboard();
        return false;
    }

    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> filesToLink;
    for (UINT i = 0; i < count; ++i) {
        wchar_t buf[MAX_PATH];
        if (DragQueryFileW(hDrop, i, buf, MAX_PATH)) {
            filesToLink.push_back(buf);
        }
    }
    CloseClipboard();

    if (filesToLink.empty()) return false;

    for (const auto& src : filesToLink) {
        wchar_t baseName[MAX_PATH];
        wcscpy_s(baseName, PathFindFileNameW(src.c_str()));
        PathRemoveExtensionW(baseName);

        std::wstring lnkName = std::wstring(baseName) + (I18n::Get().IsChinese() ? L" - 快捷方式.lnk" : L" - Shortcut.lnk");
        std::wstring lnkPath = destDir + L"\\" + lnkName;
        int suffix = 2;
        while (PathFileExistsW(lnkPath.c_str())) {
            lnkPath = destDir + L"\\" + baseName + (I18n::Get().IsChinese() ? L" - 快捷方式 (" : L" - Shortcut (") + std::to_wstring(suffix++) + L").lnk";
        }

        IShellLinkW* psl = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
            psl->SetPath(src.c_str());
            IPersistFile* ppf = nullptr;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                ppf->Save(lnkPath.c_str(), TRUE);
                ppf->Release();
            }
            psl->Release();
        }
    }
    return true;
}

bool DeleteFiles(HWND hwnd, const std::vector<std::wstring>& paths, bool permanent) {
    if (paths.empty()) return false;

    IFileOperation* pfo = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
    if (SUCCEEDED(hr) && pfo) {
        DWORD flags = permanent ? 0 : FOF_ALLOWUNDO;
        pfo->SetOperationFlags(flags);
        pfo->SetOwnerWindow(hwnd);

        for (const auto& p : paths) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(p.c_str(), nullptr, IID_PPV_ARGS(&psi)))) {
                pfo->DeleteItem(psi, nullptr);
                psi->Release();
            }
        }
        hr = pfo->PerformOperations();
        pfo->Release();
        return SUCCEEDED(hr);
    }
    return false;
}

bool RenameItem(HWND hwnd, const std::wstring& oldPath, const std::wstring& newName) {
    if (oldPath.empty() || newName.empty()) return false;

    IFileOperation* pfo = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
    if (SUCCEEDED(hr) && pfo) {
        pfo->SetOperationFlags(FOF_ALLOWUNDO);
        pfo->SetOwnerWindow(hwnd);

        IShellItem* psi = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(oldPath.c_str(), nullptr, IID_PPV_ARGS(&psi)))) {
            pfo->RenameItem(psi, newName.c_str(), nullptr);
            psi->Release();
            hr = pfo->PerformOperations();
            pfo->Release();
            return SUCCEEDED(hr);
        }
        pfo->Release();
    }
    return false;
}

void LaunchTerminal(const std::wstring& dir) {
    std::wstring cmd = L"wt.exe -d \"" + dir + L"\"";
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        LaunchPowerShell(dir);
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void LaunchPowerShell(const std::wstring& dir) {
    ShellExecuteW(nullptr, L"open", L"powershell.exe", nullptr, dir.c_str(), SW_SHOWNORMAL);
}

void LaunchCmd(const std::wstring& dir) {
    ShellExecuteW(nullptr, L"open", L"cmd.exe", nullptr, dir.c_str(), SW_SHOWNORMAL);
}

void LaunchVSCode(const std::wstring& dir) {
    ShellExecuteW(nullptr, L"open", L"code.cmd", L".", dir.c_str(), SW_SHOWNORMAL);
}

void CopyToClipboard(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    size_t bytes = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), bytes);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

std::wstring GetParentPath(const std::wstring& path) {
    wchar_t buf[MAX_PATH];
    StringCchCopyW(buf, MAX_PATH, path.c_str());
    PathRemoveBackslashW(buf);
    if (PathIsRootW(buf)) {
        return L"This PC";
    }
    if (PathRemoveFileSpecW(buf)) {
        return buf;
    }
    return L"This PC";
}

std::wstring GetDisplayName(const std::wstring& path) {
    if (path.empty() || path == L"此电脑" || path == L"This PC") {
        return I18n::Get().T(StringId::ThisPC);
    }
    wchar_t buf[MAX_PATH];
    StringCchCopyW(buf, MAX_PATH, path.c_str());
    return PathFindFileNameW(buf);
}

bool CreateShortcut(const std::wstring& shortcutPath, const std::wstring& targetPath, const std::wstring& args, const std::wstring& desc) {
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        if (!args.empty()) psl->SetArguments(args.c_str());
        if (!desc.empty()) psl->SetDescription(desc.c_str());
        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_PPV_ARGS(&ppf));
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hr);
}

bool IsExplorerReplaced() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH * 2] = { 0 };
        DWORD size = sizeof(val);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, nullptr, nullptr, &type, (LPBYTE)val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::wstring s(val);
            if (s.find(L"NeoMDIE") != std::wstring::npos) {
                return true;
            }
        } else {
            RegCloseKey(hKey);
        }
    }
    return false;
}

bool SetExplorerReplaced(bool enable) {
    const wchar_t* keys[] = {
        L"Software\\Classes\\Directory\\shell\\open\\command",
        L"Software\\Classes\\Folder\\shell\\open\\command",
        L"Software\\Classes\\Drive\\shell\\open\\command"
    };

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmdStr = L"\"" + std::wstring(exePath) + L"\" \"%1\"";

        for (const auto* k : keys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)cmdStr.c_str(), (DWORD)((cmdStr.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }
        }
    } else {
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool GetDriveSpace(const std::wstring& drivePath, uint64_t& outFreeBytes, uint64_t& outTotalBytes) {
    ULARGE_INTEGER freeBytesAvailable = { 0 };
    ULARGE_INTEGER totalNumberOfBytes = { 0 };
    ULARGE_INTEGER totalNumberOfFreeBytes = { 0 };
    std::wstring root = drivePath;
    if (root.length() >= 2 && root[1] == L':') {
        root = root.substr(0, 2) + L"\\";
    }
    if (GetDiskFreeSpaceExW(root.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        outFreeBytes = freeBytesAvailable.QuadPart;
        outTotalBytes = totalNumberOfBytes.QuadPart;
        return true;
    }
    return false;
}

} // namespace ShellUtils
