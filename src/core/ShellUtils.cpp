#include "ShellUtils.h"
#include "I18n.h"
#include <set>

namespace ShellUtils {

static HIMAGELIST s_himlSmall = nullptr;
static HIMAGELIST s_himlLarge = nullptr;

static void SanitizeRegistryShellVerb() {
    const wchar_t* shellParents[] = {
        L"Software\\Classes\\Directory\\shell",
        L"Software\\Classes\\Folder\\shell",
        L"Software\\Classes\\Drive\\shell"
    };

    for (const auto* sp : shellParents) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, sp, 0, KEY_READ | KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            wchar_t val[MAX_PATH] = { 0 };
            DWORD size = sizeof(val);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, nullptr, nullptr, &type, (LPBYTE)val, &size) == ERROR_SUCCESS) {
                // If default verb is open_NeoMDIE, or points to something invalid
                if (_wcsicmp(val, L"open_NeoMDIE") == 0) {
                    RegDeleteValueW(hKey, nullptr);
                }
            }
            RegCloseKey(hKey);
        }
    }

    // Also remove legacy open_NeoMDIE subkeys if present
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE\\command");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE\\command");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE\\command");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE");
}

static std::vector<ShellNewItem> s_shellNewItems;
static bool s_shellNewLoaded = false;
static std::mutex s_shellNewMutex;

static HBITMAP s_hbmFolder = nullptr;
static HBITMAP s_hbmShortcut = nullptr;
static HBITMAP s_hbmMd = nullptr;
static HBITMAP s_hbmText = nullptr;

static HBITMAP IconToBitmap(HICON hIcon, int size) {
    if (!hIcon) return nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD* pBits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, (void**)&pBits, nullptr, 0);
    if (hbm && pBits) {
        memset(pBits, 0, size * size * sizeof(DWORD));
        HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hbm);
        DrawIconEx(hdcMem, 0, 0, hIcon, size, size, 0, nullptr, DI_NORMAL);
        SelectObject(hdcMem, oldBmp);

        bool hasAlpha = false;
        int totalPixels = size * size;
        for (int i = 0; i < totalPixels; ++i) {
            if ((pBits[i] & 0xFF000000) != 0) {
                hasAlpha = true;
                break;
            }
        }
        if (!hasAlpha) {
            for (int i = 0; i < totalPixels; ++i) {
                if ((pBits[i] & 0x00FFFFFF) != 0) {
                    pBits[i] |= 0xFF000000;
                }
            }
        }
    }
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hbm;
}

static void ParseShellNewKey(HKEY hShellNew, const std::wstring& ext, int iconSize, std::vector<ShellNewItem>& outList) {
    bool isZh = I18n::Get().IsChinese();
    DWORD type = 0;

    // Check MenuText
    wchar_t menuText[256] = { 0 };
    DWORD sz = sizeof(menuText);
    if (RegQueryValueExW(hShellNew, L"MenuText", nullptr, &type, (LPBYTE)menuText, &sz) == ERROR_SUCCESS) {
        wchar_t res[256] = { 0 };
        if (SHLoadIndirectString(menuText, res, 256, nullptr) == S_OK) {
            wcscpy_s(menuText, res);
        }
    }

    // Check ItemName
    wchar_t itemName[256] = { 0 };
    sz = sizeof(itemName);
    if (RegQueryValueExW(hShellNew, L"ItemName", nullptr, &type, (LPBYTE)itemName, &sz) == ERROR_SUCCESS) {
        wchar_t res[256] = { 0 };
        if (SHLoadIndirectString(itemName, res, 256, nullptr) == S_OK) {
            wcscpy_s(itemName, res);
        }
    }

    if (menuText[0] == 0) {
        sz = 256;
        AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_FRIENDLYDOCNAME, ext.c_str(), nullptr, menuText, &sz);
    }
    if (menuText[0] == 0 && itemName[0] != 0) {
        wcscpy_s(menuText, itemName);
    }
    if (menuText[0] == 0) {
        swprintf_s(menuText, isZh ? L"%s 文档" : L"%s Document", ext.c_str());
    }

    ShellNewItem item;
    item.ext = ext;
    item.menuText = menuText;

    if (itemName[0] != 0) {
        item.defaultName = itemName;
    } else {
        item.defaultName = (isZh ? L"新建 " : L"New ") + item.menuText;
    }

    wchar_t val[MAX_PATH] = { 0 };
    sz = sizeof(val);
    if (RegQueryValueExW(hShellNew, L"FileName", nullptr, &type, (LPBYTE)val, &sz) == ERROR_SUCCESS) {
        wchar_t expVal[MAX_PATH] = { 0 };
        ExpandEnvironmentStringsW(val, expVal, MAX_PATH);
        item.action = ShellNewAction::FileName;
        item.templatePath = expVal;
    } else if (RegQueryValueExW(hShellNew, L"NullFile", nullptr, &type, nullptr, nullptr) == ERROR_SUCCESS) {
        item.action = ShellNewAction::NullFile;
    } else if (RegQueryValueExW(hShellNew, L"Data", nullptr, &type, nullptr, &sz) == ERROR_SUCCESS) {
        item.action = ShellNewAction::Data;
        if (sz > 0) {
            item.data.resize(sz);
            RegQueryValueExW(hShellNew, L"Data", nullptr, &type, item.data.data(), &sz);
        }
    } else if (RegQueryValueExW(hShellNew, L"Command", nullptr, &type, (LPBYTE)val, &sz) == ERROR_SUCCESS) {
        wchar_t expVal[MAX_PATH] = { 0 };
        ExpandEnvironmentStringsW(val, expVal, MAX_PATH);
        item.action = ShellNewAction::Command;
        item.command = expVal;
    } else {
        return;
    }

    SHFILEINFOW sfi = { 0 };
    SHGetFileInfoW(ext.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
    if (sfi.hIcon) {
        item.hBitmap = IconToBitmap(sfi.hIcon, iconSize);
        DestroyIcon(sfi.hIcon);
    }

    outList.push_back(item);
}

static void LoadShellNewItemsInternal() {
    std::lock_guard<std::mutex> lock(s_shellNewMutex);
    for (auto& it : s_shellNewItems) {
        if (it.hBitmap) DeleteObject(it.hBitmap);
    }
    s_shellNewItems.clear();

    int iconSize = GetSystemMetrics(SM_CXSMICON);
    if (iconSize <= 0) iconSize = 16;

    if (!s_hbmFolder) {
        SHFILEINFOW sfi = { 0 };
        SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                       SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
        if (sfi.hIcon) {
            s_hbmFolder = IconToBitmap(sfi.hIcon, iconSize);
            DestroyIcon(sfi.hIcon);
        }
    }
    if (!s_hbmShortcut) {
        HICON hIcon = nullptr;
        // shell32.dll, 263 is the official standard New Shortcut document icon with arrow
        if (ExtractIconExW(L"shell32.dll", 263, nullptr, &hIcon, 1) > 0 && hIcon) {
            s_hbmShortcut = IconToBitmap(hIcon, iconSize);
            DestroyIcon(hIcon);
        } else {
            SHSTOCKICONINFO sii = { sizeof(sii) };
            if (SUCCEEDED(SHGetStockIconInfo(SIID_LINK, SHGSI_ICON | SHGSI_SMALLICON, &sii)) && sii.hIcon) {
                s_hbmShortcut = IconToBitmap(sii.hIcon, iconSize);
                DestroyIcon(sii.hIcon);
            }
        }
    }
    if (!s_hbmMd) {
        SHFILEINFOW sfi = { 0 };
        SHGetFileInfoW(L"dummy.md", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                       SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
        if (sfi.hIcon) {
            s_hbmMd = IconToBitmap(sfi.hIcon, iconSize);
            DestroyIcon(sfi.hIcon);
        }
    }
    if (!s_hbmText) {
        SHFILEINFOW sfi = { 0 };
        SHGetFileInfoW(L"dummy.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                       SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
        if (sfi.hIcon) {
            s_hbmText = IconToBitmap(sfi.hIcon, iconSize);
            DestroyIcon(sfi.hIcon);
        }
    }

    std::set<std::wstring> seenExts;
    DWORD idx = 0;
    wchar_t keyName[256];
    DWORD keyNameLen = 256;

    while (RegEnumKeyExW(HKEY_CLASSES_ROOT, idx++, keyName, &keyNameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        keyNameLen = 256;
        if (keyName[0] != L'.') continue;
        std::wstring ext = keyName;
        if (seenExts.count(ext)) continue;

        if (_wcsicmp(ext.c_str(), L".lnk") == 0 ||
            _wcsicmp(ext.c_str(), L".library-ms") == 0 ||
            _wcsicmp(ext.c_str(), L".txt") == 0 ||
            _wcsicmp(ext.c_str(), L".md") == 0) {
            continue;
        }

        HKEY hExt = nullptr;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, ext.c_str(), 0, KEY_READ, &hExt) == ERROR_SUCCESS) {
            HKEY hSN = nullptr;
            if (RegOpenKeyExW(hExt, L"ShellNew", 0, KEY_READ, &hSN) == ERROR_SUCCESS) {
                ParseShellNewKey(hSN, ext, iconSize, s_shellNewItems);
                seenExts.insert(ext);
                RegCloseKey(hSN);
            }

            DWORD subIdx = 0;
            wchar_t subName[256];
            DWORD subLen = 256;
            while (RegEnumKeyExW(hExt, subIdx++, subName, &subLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                subLen = 256;
                if (_wcsicmp(subName, L"ShellNew") == 0) continue;
                HKEY hSub = nullptr;
                if (RegOpenKeyExW(hExt, subName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                    HKEY hSubSN = nullptr;
                    if (RegOpenKeyExW(hSub, L"ShellNew", 0, KEY_READ, &hSubSN) == ERROR_SUCCESS) {
                        if (!seenExts.count(ext)) {
                            ParseShellNewKey(hSubSN, ext, iconSize, s_shellNewItems);
                            seenExts.insert(ext);
                        }
                        RegCloseKey(hSubSN);
                    }
                    RegCloseKey(hSub);
                }
            }

            wchar_t progId[256] = { 0 };
            DWORD progSz = sizeof(progId);
            if (RegQueryValueExW(hExt, nullptr, nullptr, nullptr, (LPBYTE)progId, &progSz) == ERROR_SUCCESS && progId[0]) {
                HKEY hProg = nullptr;
                if (RegOpenKeyExW(HKEY_CLASSES_ROOT, progId, 0, KEY_READ, &hProg) == ERROR_SUCCESS) {
                    HKEY hProgSN = nullptr;
                    if (RegOpenKeyExW(hProg, L"ShellNew", 0, KEY_READ, &hProgSN) == ERROR_SUCCESS) {
                        if (!seenExts.count(ext)) {
                            ParseShellNewKey(hProgSN, ext, iconSize, s_shellNewItems);
                            seenExts.insert(ext);
                        }
                        RegCloseKey(hProgSN);
                    }
                    RegCloseKey(hProg);
                }
            }

            RegCloseKey(hExt);
        }
    }

    std::sort(s_shellNewItems.begin(), s_shellNewItems.end(), [](const ShellNewItem& a, const ShellNewItem& b) {
        return _wcsicmp(a.menuText.c_str(), b.menuText.c_str()) < 0;
    });

    s_shellNewLoaded = true;
}

const std::vector<ShellNewItem>& GetShellNewItems() {
    if (!s_shellNewLoaded) {
        LoadShellNewItemsInternal();
    }
    return s_shellNewItems;
}

void ReloadShellNewItems() {
    LoadShellNewItemsInternal();
}

HBITMAP GetShellNewFolderBitmap() {
    if (!s_shellNewLoaded) GetShellNewItems();
    return s_hbmFolder;
}

HBITMAP GetShellNewShortcutBitmap() {
    if (!s_shellNewLoaded) GetShellNewItems();
    return s_hbmShortcut;
}

HBITMAP GetShellNewMdBitmap() {
    if (!s_shellNewLoaded) GetShellNewItems();
    return s_hbmMd;
}

HBITMAP GetShellNewTextBitmap() {
    if (!s_shellNewLoaded) GetShellNewItems();
    return s_hbmText;
}

static std::wstring ResolveTemplatePath(const std::wstring& filename) {
    if (PathFileExistsW(filename.c_str())) {
        return filename;
    }
    wchar_t winDir[MAX_PATH];
    if (GetWindowsDirectoryW(winDir, MAX_PATH)) {
        wchar_t candidate[MAX_PATH];
        PathCombineW(candidate, winDir, L"ShellNew");
        PathAppendW(candidate, filename.c_str());
        if (PathFileExistsW(candidate)) return candidate;
    }
    wchar_t appData[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData) == S_OK) {
        wchar_t candidate[MAX_PATH];
        PathCombineW(candidate, appData, L"Microsoft\\Windows\\Templates");
        PathAppendW(candidate, filename.c_str());
        if (PathFileExistsW(candidate)) return candidate;
    }
    wchar_t commonTemplates[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_TEMPLATES, nullptr, 0, commonTemplates) == S_OK) {
        wchar_t candidate[MAX_PATH];
        PathCombineW(candidate, commonTemplates, filename.c_str());
        if (PathFileExistsW(candidate)) return candidate;
    }
    return L"";
}

bool CreateShellNewFile(const ShellNewItem& item, const std::wstring& targetDir, std::wstring& outCreatedPath) {
    if (targetDir.empty() || !PathFileExistsW(targetDir.c_str())) return false;

    std::wstring baseName = item.defaultName;
    if (baseName.empty()) {
        bool isZh = I18n::Get().IsChinese();
        baseName = (isZh ? L"新建 " : L"New ") + item.menuText;
    }
    if (item.ext.length() > 0) {
        if (baseName.length() < item.ext.length() ||
            _wcsicmp(baseName.c_str() + baseName.length() - item.ext.length(), item.ext.c_str()) != 0) {
            baseName += item.ext;
        }
    }

    std::wstring newFilePath = Util::CombinePath(targetDir, baseName);
    if (PathFileExistsW(newFilePath.c_str())) {
        std::wstring nameWithoutExt = baseName;
        if (item.ext.length() > 0 && nameWithoutExt.length() > item.ext.length()) {
            nameWithoutExt = nameWithoutExt.substr(0, nameWithoutExt.length() - item.ext.length());
        }
        int idx = 2;
        while (PathFileExistsW(newFilePath.c_str())) {
            newFilePath = Util::CombinePath(targetDir, nameWithoutExt + L" (" + std::to_wstring(idx++) + L")" + item.ext);
        }
    }

    bool created = false;
    if (item.action == ShellNewAction::FileName) {
        std::wstring tplPath = ResolveTemplatePath(item.templatePath);
        if (!tplPath.empty() && PathFileExistsW(tplPath.c_str())) {
            created = CopyFileW(tplPath.c_str(), newFilePath.c_str(), FALSE) != FALSE;
        }
        if (!created) {
            HANDLE hF = CreateFileW(newFilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hF != INVALID_HANDLE_VALUE) {
                CloseHandle(hF);
                created = true;
            }
        }
    } else if (item.action == ShellNewAction::Data) {
        HANDLE hF = CreateFileW(newFilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hF != INVALID_HANDLE_VALUE) {
            if (!item.data.empty()) {
                DWORD written = 0;
                WriteFile(hF, item.data.data(), (DWORD)item.data.size(), &written, nullptr);
            }
            CloseHandle(hF);
            created = true;
        }
    } else {
        HANDLE hF = CreateFileW(newFilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hF != INVALID_HANDLE_VALUE) {
            CloseHandle(hF);
            created = true;
        }
    }

    if (created && PathFileExistsW(newFilePath.c_str())) {
        outCreatedPath = newFilePath;
        return true;
    }
    return false;
}

void Initialize() {
    SanitizeRegistryShellVerb();

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

    // Warm up ShellNew registry items
    GetShellNewItems();
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

    // Fast cache lookup for all extensions
    auto it = s_extIconCache.find(ext);
    if (it != s_extIconCache.end()) {
        return it->second;
    }

    bool isCustomFile = (ext == L".exe" || ext == L".ico" || ext == L".lnk" || ext == L".cur" || ext == L".scr");

    SHFILEINFOW sfi = { 0 };
    DWORD flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    if (isCustomFile && !path.empty() && PathFileExistsW(path.c_str())) {
        if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), flags) != 0 && sfi.iIcon >= 0) {
            return sfi.iIcon;
        }
    }

    SHGetFileInfoW((L"dummy" + ext).c_str(), attr ? attr : FILE_ATTRIBUTE_NORMAL,
                   &sfi, sizeof(sfi), flags | SHGFI_USEFILEATTRIBUTES);
    s_extIconCache[ext] = sfi.iIcon;
    return sfi.iIcon;
}

std::wstring GetItemTypeName(const std::wstring& path, DWORD attr) {
    const wchar_t* pExt = PathFindExtensionW(path.c_str());
    std::wstring ext = pExt ? pExt : L"";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    auto it = s_extTypeCache.find(ext);
    if (it != s_extTypeCache.end()) {
        return it->second;
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

    UINT oldErrMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    wchar_t* pDrive = driveStrings;
    while (*pDrive) {
        NeoShellItem item;
        item.fullPath = pDrive;
        item.name = pDrive;
        item.isFolder = true;
        item.isDrive = true;
        item.attributes = FILE_ATTRIBUTE_DIRECTORY;

        UINT driveType = GetDriveTypeW(pDrive);
        std::wstring typeStr = I18n::Get().T(StringId::DriveLocal);
        if (driveType == DRIVE_REMOVABLE) typeStr = I18n::Get().T(StringId::DriveRemovable);
        else if (driveType == DRIVE_CDROM) typeStr = I18n::Get().T(StringId::DriveCD);
        else if (driveType == DRIVE_REMOTE) typeStr = I18n::Get().T(StringId::DriveNetwork);

        wchar_t volName[MAX_PATH] = { 0 };
        wchar_t fsName[MAX_PATH] = { 0 };

        // Only query volume information for local fixed or ready removable drives to prevent network/optical drive hangs
        if (driveType == DRIVE_FIXED || driveType == DRIVE_RAMDISK || driveType == DRIVE_REMOVABLE) {
            GetVolumeInformationW(pDrive, volName, MAX_PATH, nullptr, nullptr, nullptr, fsName, MAX_PATH);

            ULARGE_INTEGER freeBytes, totalBytes, totalFree;
            if (GetDiskFreeSpaceExW(pDrive, &freeBytes, &totalBytes, &totalFree)) {
                item.size = totalBytes.QuadPart;
            }
        }

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

    SetErrorMode(oldErrMode);
}

bool EnumerateDirectory(const std::wstring& path, bool showHidden, std::vector<NeoShellItem>& outItems) {
    outItems.clear();

    // Check if it's "This PC" / My Computer
    if (path == L"此电脑" || path == L"This PC" || path == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}" || path.empty()) {
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
    if (path.empty()) return false;

    // 1. Special handling for "This PC" / Computer
    if (path == L"This PC" || path == L"此电脑" || path == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
        HINSTANCE hInst = ShellExecuteW(hwnd, L"open", L"ms-settings:about", nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)hInst > 32) return true;
        hInst = ShellExecuteW(hwnd, L"open", L"control.exe", L"/name Microsoft.System", nullptr, SW_SHOWNORMAL);
        return (INT_PTR)hInst > 32;
    }

    // 2. Normal files and folders: use official SHObjectProperties API
    if (SHObjectProperties(hwnd, SHOP_FILEPATH, path.c_str(), nullptr)) {
        return true;
    }

    // 3. Fallback: ShellExecuteEx with PIDL
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr)) && pidl) {
        SHELLEXECUTEINFOW sei = { 0 };
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_IDLIST | SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_NO_UI;
        sei.hwnd = hwnd;
        sei.lpVerb = L"properties";
        sei.lpIDList = pidl;
        sei.nShow = SW_SHOWNORMAL;
        BOOL ok = ShellExecuteExW(&sei);
        ILFree(pidl);
        if (ok) return true;
    }

    // 4. Fallback: ShellExecuteEx with lpFile
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_NO_UI;
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
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
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
    PIDLIST_ABSOLUTE pidlFirst = nullptr;
    if (firstPath == L"This PC" || firstPath == L"此电脑" || firstPath == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
        SHParseDisplayName(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", nullptr, &pidlFirst, 0, nullptr);
    } else {
        pidlFirst = ILCreateFromPathW(firstPath.c_str());
        if (!pidlFirst) {
            SHParseDisplayName(firstPath.c_str(), nullptr, &pidlFirst, 0, nullptr);
        }
    }

    IShellFolder* pFolder = nullptr;
    PCUITEMID_CHILD pidlFirstChild = nullptr;

    if (pidlFirst) {
        if (FAILED(SHBindToParent(pidlFirst, IID_IShellFolder, (void**)&pFolder, &pidlFirstChild)) || !pFolder) {
            if (SUCCEEDED(SHGetDesktopFolder(&pFolder)) && pFolder) {
                pidlFirstChild = pidlFirst;
            }
        }
        if (pFolder && pidlFirstChild) {
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
                    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_BG_TAB, isZh ? L"在后台新标签页中打开(&B)" : L"Open in Background Tab(&B)");
                    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_SPLIT, isZh ? L"在新栏中打开(&S)" : L"Open in New Pane(&S)");
                }
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制完整路径(&A)" : L"Copy Full Path(&A)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_TERMINAL, isZh ? L"打开命令CMD(&C)" : L"Open CMD(&C)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

                int topItemsCount = GetMenuItemCount(hMenu);
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                pContextMenu->QueryContextMenu(hMenu, topItemsCount, 1, 0x6FFF, CMF_EXPLORE | CMF_CANRENAME);
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));

                SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);

                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                         ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

                RemoveWindowSubclass(hwnd, ContextMenuSubclassProc, 9999);

                if (g_pContextMenu3) { g_pContextMenu3->Release(); g_pContextMenu3 = nullptr; }
                if (g_pContextMenu2) { g_pContextMenu2->Release(); g_pContextMenu2 = nullptr; }

                if (cmd >= 0x7101 && cmd <= 0x71FF) {
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
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_NEW_TAB, isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制当前路径(&A)" : L"Copy Current Path(&A)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_CMD, isZh ? L"打开命令CMD(&C)" : L"Open CMD(&C)");
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

    // 5. Complete "新建 (New)" Submenu (Fully synchronized with Windows Explorer + NeoMDIE Markdown)
    bool canCreate = (!currentDir.empty() && currentDir != L"This PC" && currentDir != L"此电脑" && PathFileExistsW(currentDir.c_str()));
    if (canCreate) {
        HMENU hSubNew = CreatePopupMenu();
        const auto& shellNewItems = GetShellNewItems();

        auto addMenuItem = [&](UINT cmd, const std::wstring& text, HBITMAP hbm) {
            MENUITEMINFOW mii = { sizeof(mii) };
            mii.fMask = MIIM_STRING | MIIM_ID | (hbm ? MIIM_BITMAP : 0);
            mii.wID = cmd;
            mii.hbmpItem = hbm;
            mii.dwTypeData = (LPWSTR)text.c_str();
            InsertMenuItemW(hSubNew, GetMenuItemCount(hSubNew), TRUE, &mii);
        };

        // Standard Top items: Folder & Shortcut
        addMenuItem(CMD_BG_NEW_FOLDER, isZh ? L"文件夹(&F)\tCtrl+Shift+N" : L"Folder(&F)\tCtrl+Shift+N", GetShellNewFolderBitmap());
        addMenuItem(CMD_BG_NEW_SHORTCUT, isZh ? L"快捷方式(&S)" : L"Shortcut(&S)", GetShellNewShortcutBitmap());
        AppendMenuW(hSubNew, MF_SEPARATOR, 0, nullptr);

        // NeoMDIE special item (Markdown) & Standard Text Document
        addMenuItem(CMD_BG_NEW_MD, isZh ? L"Markdown 文档(&M)" : L"Markdown Document(&M)", GetShellNewMdBitmap());
        addMenuItem(CMD_BG_NEW_FILE, isZh ? L"TXT文档(&T)" : L"TXT Document(&T)", GetShellNewTextBitmap());

        // Full system ShellNew items from registry scan (Office docx/xlsx/pptx/pdf, SolidWorks, Zip, etc.)
        if (!shellNewItems.empty()) {
            AppendMenuW(hSubNew, MF_SEPARATOR, 0, nullptr);
            for (size_t i = 0; i < shellNewItems.size() && (CMD_BG_NEW_DYNAMIC_BASE + (UINT)i <= CMD_BG_NEW_DYNAMIC_MAX); ++i) {
                const auto& it = shellNewItems[i];
                addMenuItem(CMD_BG_NEW_DYNAMIC_BASE + (UINT)i, it.menuText, it.hBitmap);
            }
        }

        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubNew, isZh ? L"新建(&W)" : L"New(&W)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    // 6. Query Shell Background Context Menu (for 3rd-party extensions like Git, VSCode, Terminal, etc.)
    IContextMenu* pContextMenu = nullptr;
    IShellFolder* pFolder = nullptr;

    if (canCreate) {
        IShellItem* pItem = nullptr;
        HRESULT hr = SHCreateItemFromParsingName(Util::NormalizePath(currentDir).c_str(), nullptr, IID_PPV_ARGS(&pItem));
        if (SUCCEEDED(hr) && pItem) {
            hr = pItem->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&pFolder));
            if (SUCCEEDED(hr) && pFolder) {
                pFolder->CreateViewObject(hwnd, IID_PPV_ARGS(&pContextMenu));
            }
            pItem->Release();
        }
    }

    if (pContextMenu) {
        pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu2));
        pContextMenu->QueryInterface(IID_PPV_ARGS(&g_pContextMenu3));
        int shellPos = GetMenuItemCount(hMenu);
        pContextMenu->QueryContextMenu(hMenu, shellPos, 1, 0x6FFF, CMF_NORMAL);

        // Filter out broken / duplicate Shell items:
        // Shell's duplicate "View", "Sort by", "Refresh", "Paste", and shell's broken "New" / "New Folder"
        int totalItems = GetMenuItemCount(hMenu);
        for (int i = totalItems - 1; i >= shellPos; --i) {
            wchar_t itemText[256] = { 0 };
            MENUITEMINFOW mii = { sizeof(mii) };
            mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
            mii.dwTypeData = itemText;
            mii.cch = 256;
            if (GetMenuItemInfoW(hMenu, i, TRUE, &mii)) {
                std::wstring s = itemText;
                if (s.find(L"授予访问权限") != std::wstring::npos ||
                    s.find(L"Give access to") != std::wstring::npos ||
                    s.find(L"新建") != std::wstring::npos ||
                    s.find(L"New Folder") != std::wstring::npos ||
                    s.find(L"New") != std::wstring::npos ||
                    s.find(L"查看") != std::wstring::npos ||
                    s.find(L"View") != std::wstring::npos ||
                    s.find(L"排序") != std::wstring::npos ||
                    s.find(L"Sort") != std::wstring::npos ||
                    s.find(L"刷新") != std::wstring::npos ||
                    s.find(L"Refresh") != std::wstring::npos ||
                    s.find(L"粘贴") != std::wstring::npos ||
                    s.find(L"Paste") != std::wstring::npos ||
                    s.find(L"属性") != std::wstring::npos ||
                    s.find(L"Properties") != std::wstring::npos) {
                    DeleteMenu(hMenu, i, MF_BYPOSITION);
                }
            }
        }
    }

    // Properties
    if (canCreate) {
        AppendMenuW(hMenu, MF_STRING, CMD_BG_PROPERTIES, I18n::Get().T(StringId::MenuProperties));
    }

    // Append NeoMDIE specific commands
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_SETTINGS, isZh ? L"NeoMDIE设置(&S)..." : L"NeoMDIE Settings(&S)...");

    // Cleanup any consecutive or leading/trailing separators
    bool lastWasSep = true;
    for (int i = 0; i < GetMenuItemCount(hMenu); ) {
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_FTYPE;
        if (GetMenuItemInfoW(hMenu, i, TRUE, &mii)) {
            bool isSep = (mii.fType & MFT_SEPARATOR) != 0;
            if (isSep && lastWasSep) {
                DeleteMenu(hMenu, i, MF_BYPOSITION);
                continue;
            }
            lastWasSep = isSep;
        }
        i++;
    }
    int mCount = GetMenuItemCount(hMenu);
    while (mCount > 0) {
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_FTYPE;
        if (GetMenuItemInfoW(hMenu, mCount - 1, TRUE, &mii) && (mii.fType & MFT_SEPARATOR)) {
            DeleteMenu(hMenu, mCount - 1, MF_BYPOSITION);
            mCount--;
        } else {
            break;
        }
    }

    SetWindowSubclass(hwnd, ContextMenuSubclassProc, 9999, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

    RemoveWindowSubclass(hwnd, ContextMenuSubclassProc, 9999);

    if (g_pContextMenu3) { g_pContextMenu3->Release(); g_pContextMenu3 = nullptr; }
    if (g_pContextMenu2) { g_pContextMenu2->Release(); g_pContextMenu2 = nullptr; }

    DestroyMenu(hMenu);

    if (cmd >= 0x7100) {
        if (pContextMenu) { pContextMenu->Release(); pContextMenu = nullptr; }
        if (pFolder) { pFolder->Release(); pFolder = nullptr; }

        if (onCustomCommand) {
            onCustomCommand(cmd, currentDir);
        }
        return true;
    } else if (cmd > 0 && pContextMenu) {
        CMINVOKECOMMANDINFOEX cmi = { 0 };
        cmi.cbSize = sizeof(cmi);
        cmi.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
        cmi.hwnd = hwnd;
        cmi.ptInvoke = ptScreen;
        std::string ansiDir = Util::WideToAnsi(currentDir);
        cmi.lpDirectory = ansiDir.c_str();
        cmi.lpDirectoryW = currentDir.c_str();
        cmi.lpVerb = MAKEINTRESOURCEA(cmd - 1);
        cmi.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
        cmi.nShow = SW_SHOWNORMAL;
        pContextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);

        // Always notify to refresh view after invoking shell command!
        if (onCustomCommand) {
            onCustomCommand(CMD_BG_REFRESH, currentDir);
        }
    }

    if (pContextMenu) pContextMenu->Release();
    if (pFolder) pFolder->Release();

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
    if (IsClipboardFormatAvailable(CF_HDROP)) return true;
    static UINT s_uFmtDescriptorW = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
    if (s_uFmtDescriptorW && IsClipboardFormatAvailable(s_uFmtDescriptorW)) return true;
    static UINT s_uFmtDescriptorA = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORA);
    if (s_uFmtDescriptorA && IsClipboardFormatAvailable(s_uFmtDescriptorA)) return true;
    static UINT s_uFmtShellIDList = RegisterClipboardFormatW(CFSTR_SHELLIDLIST);
    if (s_uFmtShellIDList && IsClipboardFormatAvailable(s_uFmtShellIDList)) return true;
    return false;
}

bool PasteFilesFromClipboard(HWND hwnd, const std::wstring& destDir) {
    if (destDir.empty() || !PathFileExistsW(destDir.c_str())) return false;

    // 1. If standard physical files (CF_HDROP) are on the clipboard,
    // prefer IFileOperation for synchronous execution, native progress dialog, and full undo support (Ctrl+Z)
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        if (OpenClipboard(hwnd)) {
            HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
            if (hDrop) {
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

                if (!filesToPaste.empty()) {
                    IFileOperation* pfo = nullptr;
                    HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
                    if (SUCCEEDED(hr) && pfo) {
                        pfo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR | FOF_RENAMEONCOLLISION);
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
                            hr = pfo->PerformOperations();
                            psiDest->Release();
                        }
                        pfo->Release();

                        if (SUCCEEDED(hr)) {
                            if (isMove) {
                                if (OpenClipboard(hwnd)) {
                                    EmptyClipboard();
                                    CloseClipboard();
                                }
                            }
                            return true;
                        }
                    }
                }
            } else {
                CloseClipboard();
            }
        }
    }

    // 2. Virtual remote streams (UU远程, 向日葵, ToDesk, Windows远程桌面 RDP, VMware, Outlook attachments)
    // Handle via native Windows Shell IDropTarget from OleGetClipboard
    IDataObject* pDataObj = nullptr;
    if (SUCCEEDED(OleGetClipboard(&pDataObj)) && pDataObj) {
        IShellItem* psiDest = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(destDir.c_str(), nullptr, IID_PPV_ARGS(&psiDest))) && psiDest) {
            IDropTarget* pDropTarget = nullptr;
            if (SUCCEEDED(psiDest->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pDropTarget))) && pDropTarget) {
                POINTL pt = { 0, 0 };
                DWORD dwEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
                if (SUCCEEDED(pDropTarget->DragEnter(pDataObj, MK_LBUTTON, pt, &dwEffect)) && dwEffect != DROPEFFECT_NONE) {
                    HRESULT hrDrop = pDropTarget->Drop(pDataObj, 0, pt, &dwEffect);
                    pDropTarget->Release();
                    psiDest->Release();
                    pDataObj->Release();
                    if (SUCCEEDED(hrDrop)) {
                        return true;
                    }
                } else {
                    pDropTarget->DragLeave();
                    pDropTarget->Release();
                }
            }
            psiDest->Release();
        }
        pDataObj->Release();
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
    if (path.empty() || path == L"此电脑" || path == L"This PC" || path.find(L"20D04FE0-3AEA-1069-A2D8-08002B30309D") != std::wstring::npos) {
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

// Fallback IDataObject supporting CF_HDROP
class SimpleFileDataObject : public IDataObject {
public:
    SimpleFileDataObject(const std::vector<std::wstring>& paths) : m_refCount(1), m_paths(paths) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&m_refCount);
        if (r == 0) delete this;
        return r;
    }

    // IDataObject
    STDMETHODIMP GetData(FORMATETC* pFormatetc, STGMEDIUM* pmedium) override {
        if (!pFormatetc || !pmedium) return E_POINTER;
        pmedium->pUnkForRelease = nullptr;

        if (pFormatetc->cfFormat == CF_HDROP && (pFormatetc->tymed & TYMED_HGLOBAL)) {
            size_t totalBytes = sizeof(DROPFILES);
            for (const auto& p : m_paths) {
                totalBytes += (p.length() + 1) * sizeof(wchar_t);
            }
            totalBytes += sizeof(wchar_t); // Double null

            HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, totalBytes);
            if (!hGlobal) return E_OUTOFMEMORY;

            DROPFILES* pdf = (DROPFILES*)GlobalLock(hGlobal);
            pdf->pFiles = sizeof(DROPFILES);
            pdf->fWide = TRUE;
            wchar_t* pDest = (wchar_t*)((BYTE*)pdf + sizeof(DROPFILES));
            for (const auto& p : m_paths) {
                StringCchCopyW(pDest, p.length() + 1, p.c_str());
                pDest += p.length() + 1;
            }
            *pDest = L'\0';
            GlobalUnlock(hGlobal);

            pmedium->tymed = TYMED_HGLOBAL;
            pmedium->hGlobal = hGlobal;
            return S_OK;
        }
        return DV_E_FORMATETC;
    }

    STDMETHODIMP GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }

    STDMETHODIMP QueryGetData(FORMATETC* pFormatetc) override {
        if (!pFormatetc) return E_POINTER;
        if (pFormatetc->cfFormat == CF_HDROP && (pFormatetc->tymed & TYMED_HGLOBAL)) {
            return S_OK;
        }
        return DV_E_FORMATETC;
    }

    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override { return DATA_S_SAMEFORMATETC; }
    STDMETHODIMP SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override {
        if (dwDirection == DATADIR_GET) {
            FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            return SHCreateStdEnumFmtEtc(1, &fmt, ppenumFormatEtc);
        }
        return E_NOTIMPL;
    }
    STDMETHODIMP DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    STDMETHODIMP DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }

private:
    LONG m_refCount;
    std::vector<std::wstring> m_paths;
};

class SimpleDropSource : public IDropSource {
public:
    SimpleDropSource() : m_refCount(1) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&m_refCount);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & (MK_LBUTTON | MK_RBUTTON))) return DRAGDROP_S_DROP;
        return S_OK;
    }
    STDMETHODIMP GiveFeedback(DWORD) override {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
private:
    LONG m_refCount;
};

IDataObject* CreateFileDataObject(HWND hwnd, const std::wstring& currentDir, const std::vector<std::wstring>& paths) {
    if (paths.empty()) return nullptr;

    std::vector<PIDLIST_ABSOLUTE> pidls;
    for (const auto& p : paths) {
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(p.c_str(), nullptr, &pidl, 0, nullptr)) && pidl) {
            pidls.push_back(pidl);
        }
    }

    IDataObject* pDataObj = nullptr;
    if (!pidls.empty()) {
        IShellItemArray* pArray = nullptr;
        if (SUCCEEDED(SHCreateShellItemArrayFromIDLists((UINT)pidls.size(), (PCIDLIST_ABSOLUTE_ARRAY)pidls.data(), &pArray)) && pArray) {
            pArray->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pDataObj));
            pArray->Release();
        }
        for (auto pidl : pidls) {
            CoTaskMemFree(pidl);
        }
    }

    if (!pDataObj) {
        pDataObj = new SimpleFileDataObject(paths);
    }
    return pDataObj;
}

HRESULT DoFileDragDrop(HWND hwnd, const std::wstring& currentDir, const std::vector<std::wstring>& paths, DWORD* pdwEffectOut) {
    if (paths.empty()) return E_INVALIDARG;

    IDataObject* pDataObj = CreateFileDataObject(hwnd, currentDir, paths);
    if (!pDataObj) return E_FAIL;

    SimpleDropSource* pDropSource = new SimpleDropSource();

    IDragSourceHelper* pdsh = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pdsh)))) {
        POINT pt;
        GetCursorPos(&pt);
        pdsh->InitializeFromWindow(hwnd, &pt, pDataObj);
        pdsh->Release();
    }

    DWORD dwEffect = 0;
    DWORD dwOK = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    HRESULT hr = DoDragDrop(pDataObj, pDropSource, dwOK, &dwEffect);
    if (pdwEffectOut) *pdwEffectOut = dwEffect;

    pDropSource->Release();
    pDataObj->Release();
    return hr;
}

bool IsExplorerReplaced() {
    HKEY hKey = nullptr;
    // On Win 10 & 11, check HKCU\Software\Classes\Directory\shell\open\command
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open\\command", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH] = { 0 };
        DWORD size = sizeof(val);
        DWORD type = 0;
        bool hasCmd = (RegQueryValueExW(hKey, nullptr, nullptr, &type, (LPBYTE)val, &size) == ERROR_SUCCESS);
        RegCloseKey(hKey);
        if (hasCmd && wcsstr(val, L"NeoMDIE") != nullptr) {
            return true;
        }
    }
    return false;
}

static std::wstring GetLauncherScriptPath() {
    PWSTR pAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pAppData)) && pAppData) {
        std::wstring dir = std::wstring(pAppData) + L"\\NeoMDIE";
        CreateDirectoryW(dir.c_str(), nullptr);
        std::wstring script = dir + L"\\open_folder.vbs";
        CoTaskMemFree(pAppData);
        return script;
    }
    return L"";
}

static bool CreateLauncherScript(const std::wstring& scriptPath, const std::wstring& exePath) {
    std::string vbs;
    vbs += "' NeoMDIE Explorer Replacement Self-Healing Launcher\r\n";
    vbs += "Set fso = CreateObject(\"Scripting.FileSystemObject\")\r\n";
    vbs += "Set shell = CreateObject(\"WScript.Shell\")\r\n";
    vbs += "Dim target\r\n";
    vbs += "target = \"\"\r\n";
    vbs += "If WScript.Arguments.Count > 0 Then\r\n";
    vbs += "    target = WScript.Arguments(0)\r\n";
    vbs += "End If\r\n\r\n";

    std::string utf8Exe = Util::WideToUtf8(exePath);
    std::string cleanExe;
    for (char c : utf8Exe) {
        if (c == '"') cleanExe += "\"\"";
        else cleanExe += c;
    }

    vbs += "exePath = \"" + cleanExe + "\"\r\n\r\n";
    vbs += "If fso.FileExists(exePath) Then\r\n";
    vbs += "    If target = \"\" Then\r\n";
    vbs += "        shell.Run \"\"\"\" & exePath & \"\"\"\", 1, False\r\n";
    vbs += "    Else\r\n";
    vbs += "        shell.Run \"\"\"\" & exePath & \"\"\" \"\"\" & target & \"\"\"\", 1, False\r\n";
    vbs += "    End If\r\n";
    vbs += "Else\r\n";
    vbs += "    ' NeoMDIE was deleted or moved: open native Windows Explorer and self-heal!\r\n";
    vbs += "    If target = \"\" Then\r\n";
    vbs += "        shell.Run \"explorer.exe\", 1, False\r\n";
    vbs += "    Else\r\n";
    vbs += "        shell.Run \"explorer.exe \"\"\" & target & \"\"\"\", 1, False\r\n";
    vbs += "    End If\r\n";
    vbs += "    On Error Resume Next\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Directory\\shell\\open\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Directory\\shell\\open\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Folder\\shell\\open\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Folder\\shell\\open\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Drive\\shell\\open\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Drive\\shell\\open\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Directory\\shell\\open_NeoMDIE\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Directory\\shell\\open_NeoMDIE\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Folder\\shell\\open_NeoMDIE\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Folder\\shell\\open_NeoMDIE\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Drive\\shell\\open_NeoMDIE\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Drive\\shell\\open_NeoMDIE\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\open\\command\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\open\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\\"\r\n";
    vbs += "    shell.RegDelete \"HKCU\\Software\\Classes\\Folder\\shell\\\"\r\n";
    vbs += "End If\r\n";

    HANDLE hFile = CreateFileW(scriptPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(hFile, vbs.c_str(), (DWORD)vbs.length(), &bytesWritten, nullptr);
        CloseHandle(hFile);
        return true;
    }
    return false;
}

bool SetExplorerReplaced(bool enable) {
    // Windows 10 and 11 delegate directory/folder clicks through DelegateExecute COM handlers.
    // To cleanly override Windows Explorer per-user without requiring Administrator/UAC elevation:
    // 1. Override HKCU\Software\Classes\Directory\shell\open\command
    // 2. Override HKCU\Software\Classes\Folder\shell\open\command
    // 3. Override HKCU\Software\Classes\Drive\shell\open\command
    // 4. Set DelegateExecute = "" (empty string) to suppress Explorer's internal COM delegate!
    const wchar_t* openCmdKeys[] = {
        L"Software\\Classes\\Directory\\shell\\open\\command",
        L"Software\\Classes\\Folder\\shell\\open\\command",
        L"Software\\Classes\\Drive\\shell\\open\\command",
        L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\open\\command"
    };

    const wchar_t* shellParents[] = {
        L"Software\\Classes\\Directory\\shell",
        L"Software\\Classes\\Folder\\shell",
        L"Software\\Classes\\Drive\\shell"
    };

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        std::wstring scriptPath = GetLauncherScriptPath();
        std::wstring cmdStr;
        if (!scriptPath.empty() && CreateLauncherScript(scriptPath, exePath)) {
            // Use silent VBS launcher which self-heals if NeoMDIE is deleted/moved
            cmdStr = L"wscript.exe //B \"" + scriptPath + L"\" \"%1\"";
        } else {
            // Fallback direct invocation
            cmdStr = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
        }

        const wchar_t* emptyDelegate = L"";

        // First clean any stale default verb or ghost verbs
        for (const auto* sp : shellParents) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, sp, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, nullptr);
                RegCloseKey(hKey);
            }
        }

        for (const auto* k : openCmdKeys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)cmdStr.c_str(), (DWORD)((cmdStr.length() + 1) * sizeof(wchar_t)));
                // Clear DelegateExecute so Windows Explorer doesn't hijack the open verb
                RegSetValueExW(hKey, L"DelegateExecute", 0, REG_SZ, (const BYTE*)emptyDelegate, sizeof(wchar_t));
                RegCloseKey(hKey);
            }
        }
    } else {
        for (const auto* k : openCmdKeys) {
            RegDeleteKeyW(HKEY_CURRENT_USER, k);
        }
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}");

        // Clean up legacy open_NeoMDIE if present
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE");

        // CRITICAL: Clear (Default) value on shell parents to eliminate "找不到应用程序"
        for (const auto* sp : shellParents) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, sp, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, nullptr);
                RegCloseKey(hKey);
            }
        }
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell");

        // Clean launcher script
        std::wstring scriptPath = GetLauncherScriptPath();
        if (!scriptPath.empty()) {
            DeleteFileW(scriptPath.c_str());
        }
    }

    std::thread([]() {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }).detach();
    return true;
}

void CheckForUpdates(HWND hwndParent) {
    bool isZh = I18n::Get().IsChinese();
    std::thread([hwndParent, isZh]() {
        HINTERNET hInternet = InternetOpenW(L"NeoMDIE-UpdateChecker", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!hInternet) {
            MessageBoxW(hwndParent,
                isZh ? L"无法初始化网络连接以检查更新。" : L"Failed to initialize internet connection.",
                isZh ? L"检查更新" : L"Check for Updates", MB_OK | MB_ICONWARNING);
            return;
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
        HINTERNET hUrl = InternetOpenUrlW(hInternet,
            L"https://api.github.com/repos/harry10086/NeoMDIE/releases/latest",
            L"User-Agent: NeoMDIE-UpdateChecker\r\nAccept: application/vnd.github.v3+json\r\n",
            (DWORD)-1, flags, 0);

        if (!hUrl) {
            InternetCloseHandle(hInternet);
            int res = MessageBoxW(hwndParent,
                isZh ? L"无法连接到 GitHub 检查更新。\n\n是否直接前往 GitHub 仓库 Releases 页面？"
                     : L"Unable to connect to GitHub.\n\nOpen Releases page in browser?",
                isZh ? L"检查更新" : L"Check for Updates", MB_YESNO | MB_ICONQUESTION);
            if (res == IDYES) {
                ShellExecuteW(nullptr, L"open", L"https://github.com/harry10086/NeoMDIE/releases", nullptr, nullptr, SW_SHOWNORMAL);
            }
            return;
        }

        std::string json;
        char buffer[2048];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            json.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        std::string tagName;
        size_t tagPos = json.find("\"tag_name\":");
        if (tagPos != std::string::npos) {
            size_t start = json.find("\"", tagPos + 11);
            if (start != std::string::npos) {
                size_t end = json.find("\"", start + 1);
                if (end != std::string::npos) {
                    tagName = json.substr(start + 1, end - start - 1);
                }
            }
        }

        std::string htmlUrl = "https://github.com/harry10086/NeoMDIE/releases";
        size_t urlPos = json.find("\"html_url\":");
        if (urlPos != std::string::npos) {
            size_t start = json.find("\"", urlPos + 11);
            if (start != std::string::npos) {
                size_t end = json.find("\"", start + 1);
                if (end != std::string::npos) {
                    htmlUrl = json.substr(start + 1, end - start - 1);
                }
            }
        }

        std::wstring wTagName = Util::Utf8ToWide(tagName);
        std::wstring wHtmlUrl = Util::Utf8ToWide(htmlUrl);

        const std::wstring currentVer = L"v1.0.1";
        if (!wTagName.empty() && wTagName != currentVer) {
            std::wstring msg = isZh
                ? (L"发现 NeoMDIE 新版本: " + wTagName + L" (当前版本: " + currentVer + L")\n\n下载路径与发布页面:\n" + wHtmlUrl + L"\n\n是否立即前往 GitHub 下载更新？")
                : (L"New version found: " + wTagName + L" (Current: " + currentVer + L")\n\nDownload URL:\n" + wHtmlUrl + L"\n\nOpen download page now?");
            int res = MessageBoxW(hwndParent, msg.c_str(), isZh ? L"发现新版本" : L"Update Available", MB_YESNO | MB_ICONINFORMATION);
            if (res == IDYES) {
                ShellExecuteW(nullptr, L"open", wHtmlUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else {
            std::wstring msg = isZh
                ? (L"您当前使用的是最新版本 (" + currentVer + L")。\n\n没有可用的更新。")
                : (L"You are using the latest version (" + currentVer + L").\n\nNo updates available.");
            MessageBoxW(hwndParent, msg.c_str(), isZh ? L"检查更新" : L"Check for Updates", MB_OK | MB_ICONINFORMATION);
        }
    }).detach();
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
