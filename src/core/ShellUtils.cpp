#include "ShellUtils.h"
#include "I18n.h"
#include "Config.h"
#include "..\resource.h"
#include <set>
#include <unordered_set>
#include <mutex>
#include <commoncontrols.h>
#include <propkey.h>
#include <propsys.h>
#include <knownfolders.h>
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "msimg32.lib")

#ifndef FOFX_ADDUNDORECORD
#define FOFX_ADDUNDORECORD 0x20000000
#endif

namespace ShellUtils {

static HIMAGELIST s_himlSmall = nullptr;
static HIMAGELIST s_himlLarge = nullptr;
static HIMAGELIST s_himlExtraLarge = nullptr;
static HIMAGELIST s_himlJumbo = nullptr;

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

    // Query system image lists via SHGetImageList (supports 256x256 Jumbo, 48x48 ExtraLarge, 32x32 Large, 16x16 Small)
    IImageList* piml = nullptr;
    if (SUCCEEDED(SHGetImageList(SHIL_SMALL, IID_PPV_ARGS(&piml))) && piml) {
        s_himlSmall = (HIMAGELIST)piml;
    }
    piml = nullptr;
    if (SUCCEEDED(SHGetImageList(SHIL_LARGE, IID_PPV_ARGS(&piml))) && piml) {
        s_himlLarge = (HIMAGELIST)piml;
    }
    piml = nullptr;
    if (SUCCEEDED(SHGetImageList(SHIL_EXTRALARGE, IID_PPV_ARGS(&piml))) && piml) {
        s_himlExtraLarge = (HIMAGELIST)piml;
    }
    piml = nullptr;
    if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_PPV_ARGS(&piml))) && piml) {
        s_himlJumbo = (HIMAGELIST)piml;
    }

    if (!s_himlSmall) {
        SHFILEINFOW sfi = { 0 };
        s_himlSmall = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    }
    if (!s_himlLarge) {
        SHFILEINFOW sfi = { 0 };
        s_himlLarge = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
    }
    if (!s_himlExtraLarge) s_himlExtraLarge = s_himlLarge;
    if (!s_himlJumbo) s_himlJumbo = s_himlExtraLarge;

    // Warm up ShellNew registry items and Shell background context menu in background
    std::thread([]() {
        GetShellNewItems();
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IShellItem* pItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(L"C:\\", nullptr, IID_PPV_ARGS(&pItem))) && pItem) {
            IShellFolder* pFolder = nullptr;
            if (SUCCEEDED(pItem->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&pFolder))) && pFolder) {
                IContextMenu* pCM = nullptr;
                if (SUCCEEDED(pFolder->CreateViewObject(nullptr, IID_PPV_ARGS(&pCM))) && pCM) {
                    HMENU hDummy = CreatePopupMenu();
                    pCM->QueryContextMenu(hDummy, 0, 1, 0x1000, CMF_NORMAL);
                    DestroyMenu(hDummy);
                    pCM->Release();
                }
                pFolder->Release();
            }
            pItem->Release();
        }
        if (SUCCEEDED(hr)) CoUninitialize();
    }).detach();
}

HIMAGELIST GetSystemImageList(int shilType) {
    if (!s_himlSmall || !s_himlLarge) {
        Initialize();
    }
    switch (shilType) {
    case SHIL_SMALL:      return s_himlSmall;
    case SHIL_LARGE:      return s_himlLarge;
    case SHIL_EXTRALARGE: return s_himlExtraLarge ? s_himlExtraLarge : s_himlLarge;
    case SHIL_JUMBO:      return s_himlJumbo ? s_himlJumbo : (s_himlExtraLarge ? s_himlExtraLarge : s_himlLarge);
    default:              return s_himlSmall;
    }
}

HIMAGELIST GetSystemImageList(bool large) {
    return GetSystemImageList(large ? SHIL_LARGE : SHIL_SMALL);
}

static int s_defaultFolderIcon = -1;
static std::mutex s_iconCacheMutex;
static std::unordered_map<std::wstring, int> s_extIconCache;
static std::unordered_map<std::wstring, int> s_customPathIconCache;
static std::unordered_map<std::wstring, std::wstring> s_extTypeCache;

int GetCustomPathIcon(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(s_iconCacheMutex);
    auto it = s_customPathIconCache.find(path);
    if (it != s_customPathIconCache.end()) {
        return it->second;
    }
    return -1;
}

static int ExtractCustomIcon(const std::wstring& filePath, int iconIndex = 0) {
    if (filePath.empty() || !PathFileExistsW(filePath.c_str())) return -1;

    HICON hLarge = nullptr;
    HICON hSmall = nullptr;
    UINT count = ExtractIconExW(filePath.c_str(), iconIndex, &hLarge, &hSmall, 1);
    int resultIcon = -1;
    if (count > 0 && (hSmall || hLarge)) {
        std::lock_guard<std::mutex> lock(s_iconCacheMutex);
        if (hSmall && s_himlSmall) {
            resultIcon = ImageList_ReplaceIcon(s_himlSmall, -1, hSmall);
        }
        if (hLarge && s_himlLarge) {
            int largeIdx = ImageList_ReplaceIcon(s_himlLarge, -1, hLarge);
            if (resultIcon < 0) resultIcon = largeIdx;
        }
        if (hLarge && s_himlExtraLarge && s_himlExtraLarge != s_himlLarge) {
            ImageList_ReplaceIcon(s_himlExtraLarge, -1, hLarge);
        }
        if (hLarge && s_himlJumbo && s_himlJumbo != s_himlLarge && s_himlJumbo != s_himlExtraLarge) {
            ImageList_ReplaceIcon(s_himlJumbo, -1, hLarge);
        }
    }
    if (hSmall) DestroyIcon(hSmall);
    if (hLarge) DestroyIcon(hLarge);
    return resultIcon;
}

static int GetShortcutIconIndex(const std::wstring& lnkPath) {
    SHFILEINFOW sfi = { 0 };
    if (SHGetFileInfoW(lnkPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX) != 0 && sfi.iIcon >= 0) {
        return sfi.iIcon;
    }

    SHFILEINFOW sfiGeneric = { 0 };
    SHGetFileInfoW(L"dummy.lnk", FILE_ATTRIBUTE_NORMAL, &sfiGeneric, sizeof(sfiGeneric),
                   SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    int fallbackIcon = sfiGeneric.iIcon;

    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl)))) {
        return fallbackIcon;
    }
    IPersistFile* ppf = nullptr;
    if (FAILED(psl->QueryInterface(IID_PPV_ARGS(&ppf)))) {
        psl->Release();
        return fallbackIcon;
    }

    int resultIcon = fallbackIcon;
    if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
        wchar_t iconPath[MAX_PATH] = { 0 };
        int iconIdx = 0;
        if (SUCCEEDED(psl->GetIconLocation(iconPath, MAX_PATH, &iconIdx)) && iconPath[0]) {
            if (PathFileExistsW(iconPath)) {
                SHFILEINFOW sfiLoc = { 0 };
                if (SHGetFileInfoW(iconPath, 0, &sfiLoc, sizeof(sfiLoc), SHGFI_SYSICONINDEX) != 0 && sfiLoc.iIcon >= 0) {
                    resultIcon = sfiLoc.iIcon;
                } else {
                    int extracted = ExtractCustomIcon(iconPath, iconIdx);
                    if (extracted >= 0) resultIcon = extracted;
                }
            }
        } else {
            wchar_t targetPath[MAX_PATH] = { 0 };
            WIN32_FIND_DATAW wfd = { 0 };
            if (SUCCEEDED(psl->GetPath(targetPath, MAX_PATH, &wfd, SLGP_UNCPRIORITY)) && targetPath[0]) {
                if (PathFileExistsW(targetPath)) {
                    SHFILEINFOW sfiTgt = { 0 };
                    if (SHGetFileInfoW(targetPath, 0, &sfiTgt, sizeof(sfiTgt), SHGFI_SYSICONINDEX) != 0 && sfiTgt.iIcon >= 0) {
                        resultIcon = sfiTgt.iIcon;
                    } else {
                        int extracted = ExtractCustomIcon(targetPath, 0);
                        if (extracted >= 0) resultIcon = extracted;
                    }
                }
            }
        }
    }
    ppf->Release();
    psl->Release();
    return resultIcon;
}

static std::unordered_map<wchar_t, int> s_driveIconCache;

int GetItemIconIndex(const std::wstring& path, DWORD attr, bool isFolder, bool fastOnly) {
    if (isFolder) {
        if (!path.empty()) {
            std::wstring norm = Util::NormalizePath(path);
            if (_wcsicmp(norm.c_str(), L"\\\\wsl.localhost") == 0 ||
                _wcsicmp(norm.c_str(), L"Linux") == 0 ||
                norm.find(L"B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6") != std::wstring::npos) {
                return GetLinuxIconIndex();
            }
            if (_wcsnicmp(norm.c_str(), L"\\\\wsl.localhost\\", 16) == 0) {
                std::wstring rest = norm.substr(16);
                if (rest.find(L'\\') == std::wstring::npos) {
                    return GetLinuxIconIndex();
                }
            }
        }
        if (path.length() <= 3 && path.length() >= 2 && path[1] == L':') {
            wchar_t driveLetter = (wchar_t)towupper(path[0]);
            {
                std::lock_guard<std::mutex> lock(s_iconCacheMutex);
                auto it = s_driveIconCache.find(driveLetter);
                if (it != s_driveIconCache.end()) {
                    return it->second;
                }
            }
            SHFILEINFOW sfi = { 0 };
            UINT driveType = GetDriveTypeW(path.c_str());
            if (driveType == DRIVE_REMOTE) {
                // Avoid potential network hang for disconnected remote drives
                if (SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                                   SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES) != 0 && sfi.iIcon >= 0) {
                    std::lock_guard<std::mutex> lock(s_iconCacheMutex);
                    s_driveIconCache[driveLetter] = sfi.iIcon;
                    return sfi.iIcon;
                }
            } else {
                // Local fixed, removable USB, CD-ROM: retrieve real shell icon
                if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON) != 0 && sfi.iIcon >= 0) {
                    std::lock_guard<std::mutex> lock(s_iconCacheMutex);
                    s_driveIconCache[driveLetter] = sfi.iIcon;
                    return sfi.iIcon;
                }
            }
        }

        // Check special known folder icon cache (Desktop, Downloads, Documents, Pictures, Music, Videos)
        static std::unordered_map<std::wstring, int> s_specialFolderIcons;
        static bool s_specialInited = false;
        if (!s_specialInited) {
            s_specialInited = true;
            auto cacheFolder = [](int csidl) {
                wchar_t buf[MAX_PATH];
                if (SHGetFolderPathW(nullptr, csidl, nullptr, 0, buf) == S_OK) {
                    std::wstring norm = Util::NormalizePath(buf);
                    std::transform(norm.begin(), norm.end(), norm.begin(), ::towlower);
                    SHFILEINFOW sfi = { 0 };
                    if (SHGetFileInfoW(buf, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON) != 0 && sfi.iIcon >= 0) {
                        s_specialFolderIcons[norm] = sfi.iIcon;
                    }
                }
            };
            cacheFolder(CSIDL_DESKTOPDIRECTORY);
            cacheFolder(CSIDL_MYDOCUMENTS);
            cacheFolder(CSIDL_MYPICTURES);
            cacheFolder(CSIDL_MYMUSIC);
            cacheFolder(CSIDL_MYVIDEO);
            wchar_t userProfile[MAX_PATH];
            if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
                std::wstring dl = Util::NormalizePath(std::wstring(userProfile) + L"\\Downloads");
                std::wstring dlLower = dl;
                std::transform(dlLower.begin(), dlLower.end(), dlLower.begin(), ::towlower);
                SHFILEINFOW sfi = { 0 };
                if (SHGetFileInfoW(dl.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON) != 0 && sfi.iIcon >= 0) {
                    s_specialFolderIcons[dlLower] = sfi.iIcon;
                }
            }
        }
        if (!path.empty()) {
            std::wstring norm = Util::NormalizePath(path);
            std::wstring lower = norm;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            auto itS = s_specialFolderIcons.find(lower);
            if (itS != s_specialFolderIcons.end()) {
                return itS->second;
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

    bool isNetwork = (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\');
    bool isCustomFile = !isNetwork && (ext == L".exe" || ext == L".ico" || ext == L".lnk" || ext == L".cur" || ext == L".scr");

    if (isCustomFile) {
        {
            std::lock_guard<std::mutex> lock(s_iconCacheMutex);
            auto itC = s_customPathIconCache.find(path);
            if (itC != s_customPathIconCache.end()) {
                return itC->second;
            }
        }
        if (fastOnly) {
            // Return cached extension icon instantly without blocking on disk PE parsing
            std::lock_guard<std::mutex> lock(s_iconCacheMutex);
            auto itE = s_extIconCache.find(ext);
            if (itE != s_extIconCache.end()) {
                return itE->second;
            }
            SHFILEINFOW sfiGen = { 0 };
            SHGetFileInfoW((L"dummy" + ext).c_str(), attr ? attr : FILE_ATTRIBUTE_NORMAL,
                           &sfiGen, sizeof(sfiGen), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
            s_extIconCache[ext] = sfiGen.iIcon;
            return sfiGen.iIcon;
        }
    } else {
        std::lock_guard<std::mutex> lock(s_iconCacheMutex);
        auto it = s_extIconCache.find(ext);
        if (it != s_extIconCache.end()) {
            return it->second;
        }
    }

    if (ext == L".lnk" && !path.empty() && !isNetwork && PathFileExistsW(path.c_str())) {
        int scIcon = GetShortcutIconIndex(path);
        if (scIcon >= 0) {
            std::lock_guard<std::mutex> lock(s_iconCacheMutex);
            s_customPathIconCache[path] = scIcon;
            return scIcon;
        }
    } else if (isCustomFile && !path.empty() && !isNetwork && PathFileExistsW(path.c_str())) {
        SHFILEINFOW sfiCustom = { 0 };
        if (SHGetFileInfoW(path.c_str(), 0, &sfiCustom, sizeof(sfiCustom), SHGFI_SYSICONINDEX) != 0 && sfiCustom.iIcon >= 0) {
            std::lock_guard<std::mutex> lock(s_iconCacheMutex);
            s_customPathIconCache[path] = sfiCustom.iIcon;
            return sfiCustom.iIcon;
        }
        int extracted = ExtractCustomIcon(path, 0);
        if (extracted >= 0) {
            std::lock_guard<std::mutex> lock(s_iconCacheMutex);
            s_customPathIconCache[path] = extracted;
            return extracted;
        }
    }

    SHFILEINFOW sfi = { 0 };
    DWORD flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    SHGetFileInfoW((L"dummy" + ext).c_str(), attr ? attr : FILE_ATTRIBUTE_NORMAL,
                   &sfi, sizeof(sfi), flags | SHGFI_USEFILEATTRIBUTES);
    std::lock_guard<std::mutex> lock(s_iconCacheMutex);
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

std::wstring GetItemInfoTip(const std::wstring& path, bool isFolder, uint64_t size, FILETIME modifiedTime, const std::wstring& typeName, bool isSizeCalculated) {
    if (path.empty()) return L"";

    // 1. Try Windows Shell IQueryInfo to get genuine Explorer InfoTip
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr)) && pidl) {
        IShellFolder* pFolder = nullptr;
        PCUITEMID_CHILD pidlChild = nullptr;
        if (SUCCEEDED(SHBindToParent(pidl, IID_PPV_ARGS(&pFolder), &pidlChild)) && pFolder && pidlChild) {
            IQueryInfo* pqi = nullptr;
            if (SUCCEEDED(pFolder->GetUIObjectOf(nullptr, 1, &pidlChild, IID_IQueryInfo, nullptr, (void**)&pqi)) && pqi) {
                wchar_t* pTip = nullptr;
                if (SUCCEEDED(pqi->GetInfoTip(0, &pTip)) && pTip && pTip[0]) {
                    std::wstring result = pTip;
                    CoTaskMemFree(pTip);
                    pqi->Release();
                    pFolder->Release();
                    CoTaskMemFree(pidl);
                    if (result.find(L'\n') != std::wstring::npos || !isFolder) {
                        return result;
                    }
                }
                if (pTip) CoTaskMemFree(pTip);
                pqi->Release();
            }
            pFolder->Release();
        }
        CoTaskMemFree(pidl);
    }

    // 2. Structured fallback matching Windows Explorer
    bool isZh = I18n::Get().IsChinese();
    std::wstring tip;
    if (isFolder) {
        std::wstring dateStr = Util::FormatFileTime(modifiedTime);
        if (!dateStr.empty()) {
            tip += (isZh ? L"修改日期: " : L"Date modified: ") + dateStr;
        }

        if (isSizeCalculated && size > 0) {
            if (!tip.empty()) tip += L"\n";
            tip += (isZh ? L"文件夹大小: " : L"Folder size: ") + Util::FormatBytes(size);
        }

        // Fast scan of immediate items inside folder (up to 500 items)
        std::wstring pattern = Util::CombinePath(path, L"*");
        WIN32_FIND_DATAW wfd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &wfd);
        if (hFind != INVALID_HANDLE_VALUE) {
            int fileCount = 0;
            int dirCount = 0;
            do {
                if (wcscmp(wfd.cFileName, L".") == 0 || wcscmp(wfd.cFileName, L"..") == 0) continue;
                if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    dirCount++;
                } else {
                    fileCount++;
                }
                if (fileCount + dirCount >= 500) break;
            } while (FindNextFileW(hFind, &wfd));
            FindClose(hFind);

            if (!tip.empty()) tip += L"\n";
            if (fileCount == 0 && dirCount == 0) {
                tip += (isZh ? L"文件夹为空" : L"Folder is empty");
            } else {
                if (isZh) {
                    tip += L"包含: " + std::to_wstring(fileCount) + L" 个文件，" + std::to_wstring(dirCount) + L" 个文件夹";
                } else {
                    tip += L"Contains: " + std::to_wstring(fileCount) + L" files, " + std::to_wstring(dirCount) + L" folders";
                }
            }
        }
    } else {
        std::wstring type = typeName.empty() ? (isZh ? L"文件" : L"File") : typeName;
        tip += (isZh ? L"类型: " : L"Type: ") + type;
        tip += L"\n";
        tip += (isZh ? L"大小: " : L"Size: ") + Util::FormatBytes(size);
        std::wstring dateStr = Util::FormatFileTime(modifiedTime);
        if (!dateStr.empty()) {
            tip += L"\n";
            tip += (isZh ? L"修改日期: " : L"Date modified: ") + dateStr;
        }
    }
    return tip;
}

bool CanHaveThumbnail(const std::wstring& ext) {
    if (ext.empty()) return false;
    static const std::unordered_set<std::wstring> s_thumbExts = {
        L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".webp",
        L".tif", L".tiff", L".ico", L".heic", L".avif", L".svg",
        L".raw", L".cr2", L".nef", L".arw", L".dng",
        L".mp4", L".mkv", L".avi", L".mov", L".wmv", L".flv",
        L".m4v", L".webm", L".ts", L".rmvb", L".mpg", L".mpeg",
        L".pdf", L".psd", L".ai"
    };
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    return s_thumbExts.count(lower) > 0;
}

HBITMAP GetThumbnail(const std::wstring& path, int size) {
    if (path.empty()) return nullptr;
    IShellItemImageFactory* pFactory = nullptr;
    HRESULT hr = E_FAIL;
    if (IsVirtualShellPath(path)) {
        PIDLIST_ABSOLUTE pidl = HexToPidl(path);
        if (pidl) {
            hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&pFactory));
            ILFree(pidl);
        }
    } else {
        hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&pFactory));
    }
    if (FAILED(hr) || !pFactory) return nullptr;

    SIZE sz = { size, size };
    HBITMAP hbmp = nullptr;
    hr = pFactory->GetImage(sz, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
    pFactory->Release();
    if (SUCCEEDED(hr) && hbmp) {
        return hbmp;
    }
    return nullptr;
}

void ClearDriveIconCache() {
    std::lock_guard<std::mutex> lock(s_iconCacheMutex);
    s_driveIconCache.clear();
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

        SHFILEINFOW sfi = { 0 };
        // Query official Windows Shell display name, type name and icon (matches Explorer)
        if (SHGetFileInfoW(pDrive, 0, &sfi, sizeof(sfi),
                           SHGFI_DISPLAYNAME | SHGFI_TYPENAME | SHGFI_SYSICONINDEX | SHGFI_SMALLICON) != 0) {
            if (sfi.szDisplayName[0] != L'\0') {
                item.name = sfi.szDisplayName;
            } else {
                item.name = typeStr + L" (" + pDrive[0] + L":)";
            }
            if (sfi.szTypeName[0] != L'\0') {
                item.typeName = sfi.szTypeName;
            } else {
                item.typeName = typeStr;
            }
            item.iconIndex = sfi.iIcon;
        } else {
            item.name = typeStr + L" (" + pDrive[0] + L":)";
            item.typeName = typeStr;
            item.iconIndex = GetItemIconIndex(pDrive, FILE_ATTRIBUTE_DIRECTORY, true);
        }

        ULARGE_INTEGER freeBytes, totalBytes, totalFree;
        if (GetDiskFreeSpaceExW(pDrive, &freeBytes, &totalBytes, &totalFree)) {
            item.size = totalBytes.QuadPart;
        }

        outDrives.push_back(item);
        pDrive += wcslen(pDrive) + 1;
    }

    // Also enumerate Portable Devices / MTP / Shell virtual devices under This PC
    IShellItem* pComputer = nullptr;
    HRESULT hrComp = SHGetKnownFolderItem(FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(&pComputer));
    if (FAILED(hrComp)) {
        hrComp = SHCreateItemFromParsingName(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", NULL, IID_PPV_ARGS(&pComputer));
    }
    if (SUCCEEDED(hrComp) && pComputer) {
        IEnumShellItems* pEnum = nullptr;
        if (SUCCEEDED(pComputer->BindToHandler(NULL, BHID_EnumItems, IID_PPV_ARGS(&pEnum))) && pEnum) {
            IShellItem* pItem = nullptr;
            ULONG fetched = 0;
            while (pEnum->Next(1, &pItem, &fetched) == S_OK && pItem) {
                LPWSTR pszFsPath = nullptr;
                HRESULT hrFs = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFsPath);
                if (SUCCEEDED(hrFs) && pszFsPath) {
                    bool alreadyHave = false;
                    for (const auto& existing : outDrives) {
                        if (_wcsnicmp(existing.fullPath.c_str(), pszFsPath, 2) == 0) {
                            alreadyHave = true;
                            break;
                        }
                    }
                    CoTaskMemFree(pszFsPath);
                    if (alreadyHave) {
                        pItem->Release();
                        continue;
                    }
                }

                PIDLIST_ABSOLUTE pidl = nullptr;
                if (SUCCEEDED(SHGetIDListFromObject(pItem, &pidl)) && pidl) {
                    LPWSTR pszName = nullptr;
                    pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pszName);
                    std::wstring devName = (pszName && pszName[0]) ? pszName : L"";
                    if (pszName) CoTaskMemFree(pszName);

                    SFGAOF attrs = 0;
                    pItem->GetAttributes(SFGAO_FOLDER | SFGAO_STORAGE | SFGAO_STREAM, &attrs);
                    bool isFolder = (attrs & (SFGAO_FOLDER | SFGAO_STORAGE)) != 0;

                    if (isFolder && !devName.empty() && devName != L"Desktop" && devName != L"桌面") {
                        SHFILEINFOW sfi = { 0 };
                        SHGetFileInfoW((LPCWSTR)pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_TYPENAME);

                        NeoShellItem devItem;
                        devItem.name = devName;
                        devItem.fullPath = PidlToHex(pidl);
                        devItem.isFolder = true;
                        devItem.isDrive = false;
                        devItem.attributes = FILE_ATTRIBUTE_DIRECTORY;
                        devItem.typeName = (sfi.szTypeName[0] != L'\0') ? sfi.szTypeName : I18n::Get().T(StringId::DriveRemovable);
                        devItem.iconIndex = sfi.iIcon;
                        devItem.size = 0;

                        bool dup = false;
                        for (const auto& ex : outDrives) {
                            if (ex.name == devItem.name || ex.fullPath == devItem.fullPath) {
                                dup = true;
                                break;
                            }
                        }
                        if (!dup) {
                            outDrives.push_back(devItem);
                        }
                    }
                    ILFree(pidl);
                }
                pItem->Release();
            }
            pEnum->Release();
        }
        pComputer->Release();
    }

    SetErrorMode(oldErrMode);
}

bool IsVirtualShellPath(const std::wstring& path) {
    if (path.empty()) return false;
    if (path.rfind(L"shell:pidl:", 0) == 0) return true;
    if (path.rfind(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\", 0) == 0) return true;
    return false;
}

std::wstring PidlToHex(PCIDLIST_ABSOLUTE pidl) {
    if (!pidl) return L"";
    UINT cb = ILGetSize(pidl);
    const BYTE* p = (const BYTE*)pidl;
    std::wstring hex = L"shell:pidl:";
    const wchar_t digits[] = L"0123456789ABCDEF";
    hex.reserve(11 + cb * 2);
    for (UINT i = 0; i < cb; ++i) {
        hex += digits[(p[i] >> 4) & 0x0F];
        hex += digits[p[i] & 0x0F];
    }
    return hex;
}

PIDLIST_ABSOLUTE HexToPidl(const std::wstring& hex) {
    const std::wstring prefix = L"shell:pidl:";
    if (hex.rfind(prefix, 0) != 0) {
        if (hex.rfind(L"::{", 0) == 0) {
            PIDLIST_ABSOLUTE pidl = nullptr;
            if (SUCCEEDED(SHParseDisplayName(hex.c_str(), NULL, &pidl, 0, NULL)) && pidl) {
                return pidl;
            }
        }
        return nullptr;
    }
    size_t hexLen = hex.length() - prefix.length();
    if (hexLen == 0 || hexLen % 2 != 0) return nullptr;
    size_t byteCount = hexLen / 2;
    BYTE* pBytes = (BYTE*)CoTaskMemAlloc(byteCount);
    if (!pBytes) return nullptr;

    auto hexVal = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        return 0;
    };

    const wchar_t* p = hex.c_str() + prefix.length();
    for (size_t i = 0; i < byteCount; ++i) {
        pBytes[i] = (BYTE)((hexVal(p[i * 2]) << 4) | hexVal(p[i * 2 + 1]));
    }
    return (PIDLIST_ABSOLUTE)pBytes;
}

IShellItem* CreateShellItemFromPathOrPidl(const std::wstring& path) {
    if (path.empty()) return nullptr;
    if (IsVirtualShellPath(path)) {
        PIDLIST_ABSOLUTE pidl = HexToPidl(path);
        if (pidl) {
            IShellItem* psi = nullptr;
            SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&psi));
            ILFree(pidl);
            return psi;
        }
    }
    IShellItem* psi = nullptr;
    std::wstring norm = Util::NormalizePath(path);
    if (SUCCEEDED(SHCreateItemFromParsingName(norm.c_str(), nullptr, IID_PPV_ARGS(&psi))) && psi) {
        return psi;
    }
    return nullptr;
}

struct ShellNamespaceCacheEntry {
    DWORD tickCount = 0;
    bool showHidden = false;
    bool hideDotFiles = false;
    std::vector<NeoShellItem> items;
};
static std::mutex s_shellCacheMutex;
static std::unordered_map<std::wstring, ShellNamespaceCacheEntry> s_shellNamespaceCache;

bool GetCachedShellNamespace(const std::wstring& path, bool showHidden, std::vector<NeoShellItem>& outItems, bool hideDotFiles) {
    std::lock_guard<std::mutex> lock(s_shellCacheMutex);
    auto it = s_shellNamespaceCache.find(path);
    if (it != s_shellNamespaceCache.end()) {
        DWORD now = GetTickCount();
        if (now - it->second.tickCount < 20000 &&
            it->second.showHidden == showHidden &&
            it->second.hideDotFiles == hideDotFiles) {
            outItems = it->second.items;
            return true;
        }
    }
    return false;
}

void InvalidateVirtualShellCache(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(s_shellCacheMutex);
    if (path.empty()) {
        s_shellNamespaceCache.clear();
    } else {
        s_shellNamespaceCache.erase(path);
    }
}

bool EnumerateShellNamespace(const std::wstring& path, bool showHidden, std::vector<NeoShellItem>& outItems, bool hideDotFiles) {
    if (GetCachedShellNamespace(path, showHidden, outItems, hideDotFiles)) {
        return true;
    }

    PIDLIST_ABSOLUTE pidl = HexToPidl(path);
    if (!pidl) return false;

    IShellItem* pFolderItem = nullptr;
    HRESULT hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&pFolderItem));
    ILFree(pidl);
    if (FAILED(hr) || !pFolderItem) return false;

    IEnumShellItems* pEnum = nullptr;
    hr = pFolderItem->BindToHandler(NULL, BHID_EnumItems, IID_PPV_ARGS(&pEnum));
    if (FAILED(hr) || !pEnum) {
        pFolderItem->Release();
        return false;
    }

    std::vector<NeoShellItem> folders;
    std::vector<NeoShellItem> files;

    IShellItem* pChild = nullptr;
    ULONG fetched = 0;
    while (pEnum->Next(1, &pChild, &fetched) == S_OK && pChild) {
        LPWSTR pName = nullptr;
        pChild->GetDisplayName(SIGDN_NORMALDISPLAY, &pName);
        std::wstring nameStr = pName ? pName : L"";
        if (pName) CoTaskMemFree(pName);

        if (nameStr.empty()) {
            pChild->Release();
            continue;
        }

        if (hideDotFiles && nameStr[0] == L'.' && !showHidden) {
            pChild->Release();
            continue;
        }

        SFGAOF attrs = 0;
        pChild->GetAttributes(SFGAO_FOLDER | SFGAO_STORAGE | SFGAO_HIDDEN, &attrs);
        bool isFolder = (attrs & (SFGAO_FOLDER | SFGAO_STORAGE)) != 0;
        bool isHidden = (attrs & SFGAO_HIDDEN) != 0;

        if (isHidden && !showHidden) {
            pChild->Release();
            continue;
        }

        PIDLIST_ABSOLUTE childPidl = nullptr;
        if (SUCCEEDED(SHGetIDListFromObject(pChild, &childPidl)) && childPidl) {
            SHFILEINFOW sfi = { 0 };
            SHGetFileInfoW((LPCWSTR)childPidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_TYPENAME);

            NeoShellItem item;
            item.name = nameStr;
            item.fullPath = PidlToHex(childPidl);
            item.isFolder = isFolder;
            item.attributes = isFolder ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            if (isHidden) item.attributes |= FILE_ATTRIBUTE_HIDDEN;
            item.iconIndex = sfi.iIcon;
            item.typeName = (sfi.szTypeName[0] != L'\0') ? sfi.szTypeName : (isFolder ? I18n::Get().T(StringId::Folder) : L"文件");

            const wchar_t* pExt = PathFindExtensionW(nameStr.c_str());
            item.ext = pExt ? pExt : L"";

            if (!isFolder) {
                IShellItem2* pItem2 = nullptr;
                if (SUCCEEDED(pChild->QueryInterface(IID_PPV_ARGS(&pItem2))) && pItem2) {
                    ULONGLONG sizeVal = 0;
                    if (SUCCEEDED(pItem2->GetUInt64(PKEY_Size, &sizeVal))) {
                        item.size = sizeVal;
                        item.isSizeCalculated = true;
                    }
                    FILETIME ftMod = { 0 };
                    if (SUCCEEDED(pItem2->GetFileTime(PKEY_DateModified, &ftMod))) {
                        item.modifiedTime = ftMod;
                    }
                    FILETIME ftCreate = { 0 };
                    if (SUCCEEDED(pItem2->GetFileTime(PKEY_DateCreated, &ftCreate))) {
                        item.createdTime = ftCreate;
                    }
                    pItem2->Release();
                }
            } else {
                IShellItem2* pItem2 = nullptr;
                if (SUCCEEDED(pChild->QueryInterface(IID_PPV_ARGS(&pItem2))) && pItem2) {
                    FILETIME ftMod = { 0 };
                    if (SUCCEEDED(pItem2->GetFileTime(PKEY_DateModified, &ftMod))) {
                        item.modifiedTime = ftMod;
                    }
                    pItem2->Release();
                }
            }

            if (isFolder) {
                folders.push_back(std::move(item));
            } else {
                files.push_back(std::move(item));
            }
            ILFree(childPidl);
        }
        pChild->Release();
    }

    pEnum->Release();
    pFolderItem->Release();

    outItems.reserve(folders.size() + files.size());
    outItems.insert(outItems.end(), folders.begin(), folders.end());
    outItems.insert(outItems.end(), files.begin(), files.end());

    {
        std::lock_guard<std::mutex> lock(s_shellCacheMutex);
        ShellNamespaceCacheEntry entry;
        entry.tickCount = GetTickCount();
        entry.showHidden = showHidden;
        entry.hideDotFiles = hideDotFiles;
        entry.items = outItems;
        s_shellNamespaceCache[path] = std::move(entry);
    }
    return true;
}

std::wstring GetVirtualItemDisplayName(const std::wstring& path, bool fullPath) {
    PIDLIST_ABSOLUTE pidl = HexToPidl(path);
    if (!pidl) return path;

    IShellItem* pItem = nullptr;
    HRESULT hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&pItem));
    ILFree(pidl);
    if (FAILED(hr) || !pItem) return path;

    std::wstring result;
    if (fullPath) {
        std::vector<std::wstring> segments;
        IShellItem* pCur = pItem;
        pCur->AddRef();
        while (pCur) {
            LPWSTR pName = nullptr;
            pCur->GetDisplayName(SIGDN_NORMALDISPLAY, &pName);
            if (pName) {
                segments.push_back(pName);
                CoTaskMemFree(pName);
            }
            IShellItem* pParent = nullptr;
            HRESULT hrParent = pCur->GetParent(&pParent);
            pCur->Release();
            pCur = SUCCEEDED(hrParent) ? pParent : nullptr;
        }
        for (int i = (int)segments.size() - 1; i >= 0; --i) {
            if (segments[i] == L"Desktop" || segments[i] == L"桌面") continue;
            if (!result.empty()) result += L"\\";
            result += segments[i];
        }
    } else {
        LPWSTR pName = nullptr;
        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pName)) && pName) {
            result = pName;
            CoTaskMemFree(pName);
        }
    }
    pItem->Release();
    return result.empty() ? path : result;
}

bool IsWslPath(const std::wstring& path) {
    if (path.empty()) return false;
    std::wstring norm = Util::NormalizePath(path);
    if (_wcsicmp(norm.c_str(), L"\\\\wsl.localhost") == 0 ||
        _wcsicmp(norm.c_str(), L"\\\\wsl$") == 0 ||
        _wcsicmp(norm.c_str(), L"Linux") == 0 ||
        norm.find(L"B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6") != std::wstring::npos ||
        norm.find(L"b2b4a4d1-2754-4140-a2eb-9a76d9d7cdc6") != std::wstring::npos) {
        return true;
    }
    if (_wcsnicmp(norm.c_str(), L"\\\\wsl.localhost\\", 16) == 0 ||
        _wcsnicmp(norm.c_str(), L"\\\\wsl$\\", 7) == 0) {
        return true;
    }
    return false;
}

bool IsValidDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    if (path == L"This PC" || path == L"此电脑" || _wcsicmp(path.c_str(), L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") == 0) {
        return true;
    }
    if (IsVirtualShellPath(path)) {
        return true;
    }
    if (IsWslPath(path)) {
        return true;
    }
    return PathFileExistsW(path.c_str()) != FALSE;
}

int GetLinuxIconIndex() {
    static int s_linuxIconIndex = -1;
    if (s_linuxIconIndex >= 0) return s_linuxIconIndex;

    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(L"::{B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6}", nullptr, &pidl, 0, nullptr)) && pidl) {
        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW((LPCWSTR)pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON) != 0 && sfi.iIcon >= 0) {
            s_linuxIconIndex = sfi.iIcon;
        }
        CoTaskMemFree(pidl);
    }
    if (s_linuxIconIndex < 0) {
        s_linuxIconIndex = GetItemIconIndex(L"", FILE_ATTRIBUTE_DIRECTORY, true);
    }
    return s_linuxIconIndex;
}

std::vector<NeoShellItem> EnumerateWslDistros() {
    std::vector<NeoShellItem> distros;
    std::unordered_set<std::wstring> seen;

    auto cleanDistroName = [](std::wstring name) -> std::wstring {
        size_t pos = name.find(L" (\\");
        if (pos != std::wstring::npos) {
            name = name.substr(0, pos);
        }
        return name;
    };

    // 1. Shell namespace enumeration (::{B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6} or \\wsl.localhost)
    PIDLIST_ABSOLUTE pidlLinux = nullptr;
    HRESULT hr = SHParseDisplayName(L"::{B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6}", nullptr, &pidlLinux, 0, nullptr);
    if (FAILED(hr)) {
        hr = SHParseDisplayName(L"\\\\wsl.localhost", nullptr, &pidlLinux, 0, nullptr);
    }
    if (SUCCEEDED(hr) && pidlLinux) {
        IShellFolder* pDesktop = nullptr;
        if (SUCCEEDED(SHGetDesktopFolder(&pDesktop))) {
            IShellFolder* pLinuxFolder = nullptr;
            if (SUCCEEDED(pDesktop->BindToObject(pidlLinux, nullptr, IID_IShellFolder, (void**)&pLinuxFolder))) {
                IEnumIDList* pEnum = nullptr;
                if (SUCCEEDED(pLinuxFolder->EnumObjects(nullptr, SHCONTF_FOLDERS, &pEnum)) && pEnum) {
                    PITEMID_CHILD pidlChild = nullptr;
                    ULONG fetched = 0;
                    while (pEnum->Next(1, &pidlChild, &fetched) == S_OK && fetched == 1) {
                        STRRET str;
                        wchar_t name[MAX_PATH] = { 0 };
                        if (SUCCEEDED(pLinuxFolder->GetDisplayNameOf(pidlChild, SHGDN_NORMAL, &str))) {
                            StrRetToBufW(&str, pidlChild, name, MAX_PATH);
                        }
                        if (name[0] != L'\0') {
                            std::wstring dName = cleanDistroName(name);
                            if (seen.find(dName) == seen.end()) {
                                seen.insert(dName);
                                NeoShellItem item;
                                item.name = dName;
                                item.fullPath = L"\\\\wsl.localhost\\" + dName;
                                item.isFolder = true;
                                item.isDrive = false;
                                item.size = 0;
                                item.isSizeCalculated = false;
                                item.typeName = I18n::Get().T(StringId::Folder);
                                item.iconIndex = GetLinuxIconIndex();
                                distros.push_back(item);
                            }
                        }
                        CoTaskMemFree(pidlChild);
                    }
                    pEnum->Release();
                }
                pLinuxFolder->Release();
            }
            pDesktop->Release();
        }
        CoTaskMemFree(pidlLinux);
    }

    // 2. Fallback / supplementary: HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss
    HKEY hLxss = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss", 0, KEY_READ, &hLxss) == ERROR_SUCCESS) {
        DWORD subIdx = 0;
        wchar_t subKeyName[256];
        DWORD subKeyLen = 256;
        while (RegEnumKeyExW(hLxss, subIdx++, subKeyName, &subKeyLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            subKeyLen = 256;
            HKEY hDistro = nullptr;
            if (RegOpenKeyExW(hLxss, subKeyName, 0, KEY_READ, &hDistro) == ERROR_SUCCESS) {
                wchar_t distroName[256] = { 0 };
                DWORD dNameSize = sizeof(distroName);
                if (RegQueryValueExW(hDistro, L"DistributionName", nullptr, nullptr, (LPBYTE)distroName, &dNameSize) == ERROR_SUCCESS && distroName[0]) {
                    std::wstring dName = cleanDistroName(distroName);
                    if (seen.find(dName) == seen.end()) {
                        seen.insert(dName);
                        NeoShellItem item;
                        item.name = dName;
                        item.fullPath = L"\\\\wsl.localhost\\" + dName;
                        item.isFolder = true;
                        item.isDrive = false;
                        item.size = 0;
                        item.isSizeCalculated = false;
                        item.typeName = I18n::Get().T(StringId::Folder);
                        item.iconIndex = GetLinuxIconIndex();
                        distros.push_back(item);
                    }
                }
                RegCloseKey(hDistro);
            }
        }
        RegCloseKey(hLxss);
    }

    return distros;
}

bool EnumerateDirectory(const std::wstring& path, bool showHidden, std::vector<NeoShellItem>& outItems, bool hideDotFiles) {
    outItems.clear();

    // Check if it's "This PC" / My Computer
    if (path == L"此电脑" || path == L"This PC" || path == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}" || path.empty()) {
        EnumerateDrives(outItems);
        return true;
    }

    // Check if it's virtual shell namespace (MTP Android device, storage, etc.)
    if (IsVirtualShellPath(path)) {
        return EnumerateShellNamespace(path, showHidden, outItems, hideDotFiles);
    }

    // Check if it's Linux virtual root
    std::wstring normPath = Util::NormalizePath(path);
    if (_wcsicmp(normPath.c_str(), L"\\\\wsl.localhost") == 0 ||
        _wcsicmp(normPath.c_str(), L"\\\\wsl$") == 0 ||
        _wcsicmp(normPath.c_str(), L"Linux") == 0 ||
        normPath.find(L"B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6") != std::wstring::npos) {
        outItems = EnumerateWslDistros();
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

        if (hideDotFiles && fd.cFileName[0] == L'.' && !showHidden) {
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
            item.iconIndex = GetItemIconIndex(item.fullPath, fd.dwFileAttributes, true, true);
            folders.push_back(item);
        } else {
            ULARGE_INTEGER uli;
            uli.LowPart = fd.nFileSizeLow;
            uli.HighPart = fd.nFileSizeHigh;
            item.size = uli.QuadPart;
            item.isSizeCalculated = true;
            item.typeName = GetItemTypeName(item.fullPath, fd.dwFileAttributes);
            item.iconIndex = GetItemIconIndex(item.fullPath, fd.dwFileAttributes, false, true);
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
    if (IsVirtualShellPath(path)) {
        PIDLIST_ABSOLUTE pidl = HexToPidl(path);
        if (pidl) {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_IDLIST;
            sei.hwnd = hwnd;
            sei.lpIDList = pidl;
            sei.nShow = SW_SHOWNORMAL;
            BOOL ok = ShellExecuteExW(&sei);
            ILFree(pidl);
            return ok != FALSE;
        }
    }
    HINSTANCE hInst = ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)hInst > 32;
}

bool ShowItemProperties(HWND hwnd, const std::wstring& path) {
    if (path.empty()) return false;

    if (IsVirtualShellPath(path)) {
        PIDLIST_ABSOLUTE pidl = HexToPidl(path);
        if (pidl) {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_IDLIST | SEE_MASK_INVOKEIDLIST;
            sei.hwnd = hwnd;
            sei.lpVerb = L"properties";
            sei.lpIDList = pidl;
            sei.nShow = SW_SHOWNORMAL;
            BOOL ok = ShellExecuteExW(&sei);
            ILFree(pidl);
            return ok != FALSE;
        }
    }

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

static HRESULT SafeQueryContextMenu(IContextMenu* pContextMenu, HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags) {
    __try {
        return pContextMenu->QueryContextMenu(hMenu, indexMenu, idCmdFirst, idCmdLast, uFlags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_FAIL;
    }
}

bool ShowContextMenu(HWND hwnd, const std::vector<std::wstring>& paths, POINT ptScreen, std::function<void(int neoCmd, const std::wstring& path)> onCustomCommand) {
    if (paths.empty()) return false;

    bool shown = false;
    std::wstring firstPath = Util::NormalizePath(paths[0]);

    bool isNetwork = (firstPath.length() >= 2 && firstPath[0] == L'\\' && firstPath[1] == L'\\');
    if (!isNetwork && firstPath.length() >= 2 && firstPath[1] == L':') {
        wchar_t dRoot[4] = { firstPath[0], L':', L'\\', L'\0' };
        if (GetDriveTypeW(dRoot) == DRIVE_REMOTE) {
            isNetwork = true;
        }
    }

    if (isNetwork) {
        HMENU hMenu = CreatePopupMenu();
        bool isDir = PathIsDirectoryW(firstPath.c_str()) != FALSE;
        bool isZh = I18n::Get().IsChinese();

        if (isDir) {
            AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_NEW_TAB, isZh ? L"在新标签页中打开(&T)" : L"Open in New Tab(&T)");
            AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_BG_TAB, isZh ? L"在后台新标签页中打开(&B)" : L"Open in Background Tab(&B)");
            AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_SPLIT, isZh ? L"在新栏中打开(&S)" : L"Open in New Pane(&S)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_PIN_QUICKACCESS, isZh ? L"固定到快速访问(&P)" : L"Pin to Quick Access(&P)");
        }
        AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_EXPLORER, isZh ? L"在资源管理器中打开(&E)" : L"Open in File Explorer(&E)");
        AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制完整路径(&A)" : L"Copy Full Path(&A)");
        AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
        AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_TERMINAL, isZh ? L"打开命令CMD(&C)" : L"Open CMD(&C)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 50001, isZh ? L"剪切(&T)\tCtrl+X" : L"Cut(&T)\tCtrl+X");
        AppendMenuW(hMenu, MF_STRING, 50002, isZh ? L"复制(&C)\tCtrl+C" : L"Copy(&C)\tCtrl+C");
        AppendMenuW(hMenu, MF_STRING, 50003, isZh ? L"删除(&D)\tDel" : L"Delete(&D)\tDel");
        AppendMenuW(hMenu, MF_STRING, 50004, isZh ? L"重命名(&M)\tF2" : L"Rename(&M)\tF2");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 50005, isZh ? L"显示系统完整右键菜单(&S)..." : L"Show System Context Menu(&S)...");
        AppendMenuW(hMenu, MF_STRING, 50006, isZh ? L"属性(&P)" : L"Properties(&P)");

        if (GetCapture()) ReleaseCapture();
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd == 0) return true;
        if (cmd == CMD_NEOMDIE_OPEN_NEW_TAB || cmd == CMD_NEOMDIE_OPEN_BG_TAB ||
            cmd == CMD_NEOMDIE_OPEN_SPLIT || cmd == CMD_NEOMDIE_OPEN_EXPLORER ||
            cmd == CMD_NEOMDIE_COPY_PATH || cmd == CMD_NEOMDIE_ADD_LINKS ||
            cmd == CMD_NEOMDIE_PIN_QUICKACCESS || cmd == CMD_NEOMDIE_UNPIN_QUICKACCESS ||
            cmd == CMD_NEOMDIE_TERMINAL) {
            if (onCustomCommand) onCustomCommand(cmd, firstPath);
            return true;
        }
        if (cmd == 50001) {
            CopyFilesToClipboard(hwnd, paths, true);
            return true;
        }
        if (cmd == 50002) {
            CopyFilesToClipboard(hwnd, paths, false);
            return true;
        }
        if (cmd == 50003) {
            DeleteFiles(hwnd, paths, false);
            return true;
        }
        if (cmd == 50004) {
            PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(40017, 0), 0);
            return true;
        }
        if (cmd == 50006) {
            ShowItemProperties(hwnd, firstPath);
            return true;
        }
    }

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
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_PIN_QUICKACCESS, isZh ? L"固定到快速访问(&P)" : L"Pin to Quick Access(&P)");
                }
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_EXPLORER, isZh ? L"在资源管理器中打开(&E)" : L"Open in File Explorer(&E)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制完整路径(&A)" : L"Copy Full Path(&A)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_ADD_LINKS, isZh ? L"添加到链接栏(&L)" : L"Add to Links Bar(&L)");
                AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_TERMINAL, isZh ? L"打开命令CMD(&C)" : L"Open CMD(&C)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

                int topItemsCount = GetMenuItemCount(hMenu);
                if (GetCapture()) ReleaseCapture();
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));

                // Safe in-thread QueryContextMenu with Structured Exception Handling (__try / __except)
                // COM apartment rules require IContextMenu::QueryContextMenu to run on the creating STA thread,
                // or background thread inter-thread COM calls will block/hang or return raw unmangled verb strings.
                SafeQueryContextMenu(pContextMenu, hMenu, (UINT)topItemsCount, 1, 0x6FFF, CMF_NORMAL | CMF_EXPLORE);

                // Clean up any broken/raw verbs or shell items if necessary
                int totalItems = GetMenuItemCount(hMenu);
                for (int i = totalItems - 1; i >= topItemsCount; --i) {
                    wchar_t itemText[256] = { 0 };
                    MENUITEMINFOW mii = { sizeof(mii) };
                    mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
                    mii.dwTypeData = itemText;
                    mii.cch = 256;
                    if (GetMenuItemInfoW(hMenu, i, TRUE, &mii)) {
                        // Filter out empty or raw shell verb scripts if any
                        std::wstring s = itemText;
                        if (s.find(L"powershell -") != std::wstring::npos ||
                            s.find(L"cmd.exe /c") != std::wstring::npos) {
                            DeleteMenu(hMenu, i, MF_BYPOSITION);
                        }
                    }
                }

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
                               int currentViewMode, int currentSortCol, bool sortAsc, bool foldersFirst,
                               std::function<void(int neoCmd, const std::wstring& currentDir)> onCustomCommand) {
    bool isZh = I18n::Get().IsChinese();
    HMENU hMenu = CreatePopupMenu();

    // 0. Top NeoMDIE shortcuts
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_NEW_TAB, isZh ? L"新建标签页(&T)\tCtrl+T" : L"New Tab(&T)\tCtrl+T");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_OPEN_EXPLORER, isZh ? L"在资源管理器中打开(&E)" : L"Open in File Explorer(&E)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_COPY_PATH, isZh ? L"复制当前路径(&A)" : L"Copy Current Path(&A)");
    AppendMenuW(hMenu, MF_STRING, CMD_NEOMDIE_PIN_QUICKACCESS, isZh ? L"固定当前文件夹到快速访问(&P)" : L"Pin Current Folder to Quick Access(&P)");
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
    AppendMenuW(hSubView, MF_STRING, CMD_BG_VIEW_CONTENT, isZh ? L"内容(&C)" : L"Content(&C)");

    int checkViewCmd = CMD_BG_VIEW_DETAILS;
    switch (currentViewMode) {
    case 0: checkViewCmd = CMD_BG_VIEW_DETAILS; break;
    case 1: checkViewCmd = CMD_BG_VIEW_XLARGE; break;
    case 2: checkViewCmd = CMD_BG_VIEW_LARGE; break;
    case 3: checkViewCmd = CMD_BG_VIEW_MEDIUM; break;
    case 4: checkViewCmd = CMD_BG_VIEW_SMALL; break;
    case 5: checkViewCmd = CMD_BG_VIEW_LIST; break;
    case 6: checkViewCmd = CMD_BG_VIEW_TILES; break;
    case 7: checkViewCmd = CMD_BG_VIEW_CONTENT; break;
    default: checkViewCmd = CMD_BG_VIEW_DETAILS; break;
    }
    CheckMenuRadioItem(hSubView, CMD_BG_VIEW_DETAILS, CMD_BG_VIEW_CONTENT, checkViewCmd, MF_BYCOMMAND);
    AppendMenuW(hSubView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubView, MF_STRING | (Config::Get().showItemCheckBoxes ? MF_CHECKED : MF_UNCHECKED),
                CMD_BG_VIEW_ITEM_CHECKBOXES, isZh ? L"项目复选框(&X)" : L"Item check boxes(&X)");
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
    AppendMenuW(hSubSort, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubSort, MF_STRING | (foldersFirst ? MF_CHECKED : MF_UNCHECKED),
                CMD_BG_SORT_FOLDERS_FIRST, isZh ? L"文件夹在前" : L"Folders first");
    AppendMenuW(hSubSort, MF_STRING | (!foldersFirst ? MF_CHECKED : MF_UNCHECKED),
                CMD_BG_SORT_FILES_FIRST, isZh ? L"文件在前" : L"Files first");

    int checkSortCmd = CMD_BG_SORT_NAME + std::clamp(currentSortCol, 0, 3);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_NAME, CMD_BG_SORT_SIZE, checkSortCmd, MF_BYCOMMAND);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_ASCENDING, CMD_BG_SORT_DESCENDING,
                       sortAsc ? CMD_BG_SORT_ASCENDING : CMD_BG_SORT_DESCENDING, MF_BYCOMMAND);
    CheckMenuRadioItem(hSubSort, CMD_BG_SORT_FOLDERS_FIRST, CMD_BG_SORT_FILES_FIRST,
                       foldersFirst ? CMD_BG_SORT_FOLDERS_FIRST : CMD_BG_SORT_FILES_FIRST, MF_BYCOMMAND);
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

IDataObject* CreateFileDataObject(HWND hwnd, const std::wstring& currentDir, const std::vector<std::wstring>& paths);

bool CopyFilesToClipboard(HWND hwnd, const std::vector<std::wstring>& paths, bool isCut) {
    if (paths.empty()) return false;

    // Check if any path is a virtual shell path (such as MTP Android device)
    bool hasVirtual = false;
    for (const auto& p : paths) {
        if (IsVirtualShellPath(p)) {
            hasVirtual = true;
            break;
        }
    }

    if (hasVirtual) {
        IDataObject* pDataObj = CreateFileDataObject(hwnd, L"", paths);
        if (pDataObj) {
            UINT uDropEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
            FORMATETC fmt = { (CLIPFORMAT)uDropEffect, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM med = { TYMED_HGLOBAL };
            med.hGlobal = GlobalAlloc(GHND | GMEM_SHARE, sizeof(DWORD));
            if (med.hGlobal) {
                DWORD* pdwEffect = (DWORD*)GlobalLock(med.hGlobal);
                *pdwEffect = isCut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
                GlobalUnlock(med.hGlobal);
                pDataObj->SetData(&fmt, &med, TRUE);
            }
            HRESULT hr = OleSetClipboard(pDataObj);
            pDataObj->Release();
            return SUCCEEDED(hr);
        }
    }

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

static DWORD s_lastCutSeq = 0xFFFFFFFF;
static std::unordered_set<std::wstring> s_cutPathSet;

static void EnsureCutPathsUpdated() {
    DWORD curSeq = GetClipboardSequenceNumber();
    if (curSeq != s_lastCutSeq) {
        s_lastCutSeq = curSeq;
        s_cutPathSet.clear();

        if (IsClipboardFormatAvailable(CF_HDROP)) {
            if (OpenClipboard(nullptr)) {
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

                if (isMove) {
                    HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
                    if (hDrop) {
                        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                        for (UINT i = 0; i < count; ++i) {
                            wchar_t buf[MAX_PATH];
                            if (DragQueryFileW(hDrop, i, buf, MAX_PATH)) {
                                std::wstring p = Util::NormalizePath(buf);
                                for (auto& ch : p) ch = towlower(ch);
                                while (p.length() > 3 && p.back() == L'\\') {
                                    p.pop_back();
                                }
                                s_cutPathSet.insert(p);
                            }
                        }
                    }
                }
                CloseClipboard();
            }
        }
    }
}

bool IsPathCut(const std::wstring& path) {
    if (path.empty()) return false;
    EnsureCutPathsUpdated();
    if (s_cutPathSet.empty()) return false;

    std::wstring p = Util::NormalizePath(path);
    for (auto& ch : p) ch = towlower(ch);
    while (p.length() > 3 && p.back() == L'\\') {
        p.pop_back();
    }
    return s_cutPathSet.find(p) != s_cutPathSet.end();
}

bool HasCutFiles() {
    EnsureCutPathsUpdated();
    return !s_cutPathSet.empty();
}

void BlendRectWithColor(HDC hdc, const RECT& rc, COLORREF clr, BYTE alpha) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) return;

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 1;
    bmi.bmiHeader.biHeight = 1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD* pPixel = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, (void**)&pPixel, nullptr, 0);
    if (hBmp && pPixel) {
        pPixel->rgbRed = GetRValue(clr);
        pPixel->rgbGreen = GetGValue(clr);
        pPixel->rgbBlue = GetBValue(clr);
        pPixel->rgbReserved = 0;

        HGDIOBJ hOldBmp = SelectObject(memDC, hBmp);

        BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, 0 };
        AlphaBlend(hdc, rc.left, rc.top, w, h, memDC, 0, 0, 1, 1, bf);

        SelectObject(memDC, hOldBmp);
        DeleteObject(hBmp);
    }
    DeleteDC(memDC);
}

bool CopyOrMoveFiles(HWND hwndOwner, const std::wstring& destDir, const std::vector<std::wstring>& srcFiles, bool isMove) {
    if (destDir.empty() || srcFiles.empty()) return false;
    if (!IsValidDirectory(destDir)) return false;

    // Launch dedicated background STA thread to execute operations asynchronously so UI is never frozen
    std::thread([hwndOwner, destDir, srcFiles, isMove]() {
        HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        std::wstring normDest = Util::NormalizePath(destDir);
        while (normDest.length() > 3 && normDest.back() == L'\\') {
            normDest.pop_back();
        }

        std::vector<std::wstring> sameDirFiles;
        std::vector<std::wstring> diffDirFiles;
        std::vector<std::wstring> changedSrcDirs;

        bool destIsVirtual = IsVirtualShellPath(destDir);

        for (const auto& src : srcFiles) {
            bool srcIsVirtual = IsVirtualShellPath(src);
            if (destIsVirtual || srcIsVirtual) {
                diffDirFiles.push_back(src);
                continue;
            }

            std::wstring normSrc = Util::NormalizePath(src);
            wchar_t sDir[MAX_PATH];
            StringCchCopyW(sDir, MAX_PATH, normSrc.c_str());
            PathRemoveFileSpecW(sDir);
            while (wcslen(sDir) > 3 && sDir[wcslen(sDir) - 1] == L'\\') {
                sDir[wcslen(sDir) - 1] = L'\0';
            }

            bool isSame = (_wcsicmp(sDir, normDest.c_str()) == 0);
            if (isMove) {
                if (isSame) {
                    continue;
                }
                diffDirFiles.push_back(normSrc);
                changedSrcDirs.push_back(sDir);
            } else {
                if (isSame) {
                    sameDirFiles.push_back(normSrc);
                } else {
                    diffDirFiles.push_back(normSrc);
                }
            }
        }

        bool anySuccess = false;

        // 1. 同目录复制：使用 FOF_RENAMEONCOLLISION 自动生成副本
        if (!sameDirFiles.empty()) {
            IFileOperation* pfoSame = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfoSame)))) {
                pfoSame->SetOperationFlags(FOF_ALLOWUNDO | FOFX_ADDUNDORECORD | FOF_NOCONFIRMMKDIR | FOF_RENAMEONCOLLISION);
                pfoSame->SetOwnerWindow(nullptr);

                IShellItem* psiDest = CreateShellItemFromPathOrPidl(destDir);
                if (psiDest) {
                    for (const auto& s : sameDirFiles) {
                        IShellItem* psiSrc = CreateShellItemFromPathOrPidl(s);
                        if (psiSrc) {
                            pfoSame->CopyItem(psiSrc, psiDest, nullptr, nullptr);
                            psiSrc->Release();
                        }
                    }
                    HRESULT hr = pfoSame->PerformOperations();
                    if (SUCCEEDED(hr)) {
                        anySuccess = true;
                    }
                    psiDest->Release();
                }
                pfoSame->Release();
            }
        }

        // 2. 跨目录复制或跨目录移动（支持本地磁盘与 Android/MTP 便携设备互拷）
        if (!diffDirFiles.empty()) {
            IFileOperation* pfoDiff = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfoDiff)))) {
                pfoDiff->SetOperationFlags(FOF_ALLOWUNDO | FOFX_ADDUNDORECORD | FOF_NOCONFIRMMKDIR);
                pfoDiff->SetOwnerWindow(nullptr);

                IShellItem* psiDest = CreateShellItemFromPathOrPidl(destDir);
                if (psiDest) {
                    for (const auto& s : diffDirFiles) {
                        IShellItem* psiSrc = CreateShellItemFromPathOrPidl(s);
                        if (psiSrc) {
                            if (isMove) {
                                pfoDiff->MoveItem(psiSrc, psiDest, nullptr, nullptr);
                            } else {
                                pfoDiff->CopyItem(psiSrc, psiDest, nullptr, nullptr);
                            }
                            psiSrc->Release();
                        }
                    }
                    HRESULT hr = pfoDiff->PerformOperations();
                    if (SUCCEEDED(hr)) {
                        anySuccess = true;
                    }
                    psiDest->Release();
                }
                pfoDiff->Release();
            }
        }

        if (anySuccess) {
            if (!destIsVirtual) {
                if (isMove && !diffDirFiles.empty()) {
                    std::vector<std::pair<std::wstring, std::wstring>> moves;
                    for (const auto& s : diffDirFiles) {
                        if (!IsVirtualShellPath(s)) {
                            moves.push_back({ s, normDest + L"\\" + PathFindFileNameW(s.c_str()) });
                        }
                    }
                    if (!moves.empty()) RecordMove(moves);
                } else if (!isMove && !diffDirFiles.empty()) {
                    std::vector<std::wstring> copied;
                    for (const auto& s : diffDirFiles) {
                        if (!IsVirtualShellPath(s)) {
                            copied.push_back(normDest + L"\\" + PathFindFileNameW(s.c_str()));
                        }
                    }
                    if (!copied.empty()) RecordCopy(copied);
                }
            }
            if (isMove) {
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    CloseClipboard();
                }
            }
            if (!destIsVirtual) {
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, normDest.c_str(), nullptr);
            }
            for (const auto& sd : changedSrcDirs) {
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, sd.c_str(), nullptr);
            }
            if (hwndOwner && IsWindow(hwndOwner)) {
                PostMessageW(hwndOwner, WM_COMMAND, MAKEWPARAM(ID_VIEW_REFRESH, 0), 0);
            }
        }

        if (SUCCEEDED(hrCom)) {
            CoUninitialize();
        }
    }).detach();

    return true;
}

bool PasteFilesFromClipboard(HWND hwnd, const std::wstring& destDir) {
    if (destDir.empty() || !IsValidDirectory(destDir)) return false;

    // 1. If target is physical directory and standard CF_HDROP is available
    if (!IsVirtualShellPath(destDir) && IsClipboardFormatAvailable(CF_HDROP)) {
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
                    return CopyOrMoveFiles(hwnd, destDir, filesToPaste, isMove);
                }
            } else {
                CloseClipboard();
            }
        }
    }

    // 2. Shell IDList / Virtual Shell objects (Android MTP devices, remote streams, etc.)
    // Launch dedicated background STA thread to execute operations asynchronously so UI is never frozen
    std::thread([hwnd, destDir]() {
        HRESULT hrCom = OleInitialize(nullptr);
        IDataObject* pDataObj = nullptr;
        if (SUCCEEDED(OleGetClipboard(&pDataObj)) && pDataObj) {
            IShellItem* psiDest = CreateShellItemFromPathOrPidl(destDir);
            if (psiDest) {
                bool isMove = false;
                UINT uDropEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
                FORMATETC fmtEffect = { (CLIPFORMAT)uDropEffect, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                STGMEDIUM medEffect = { 0 };
                if (SUCCEEDED(pDataObj->GetData(&fmtEffect, &medEffect))) {
                    DWORD* pdwEffect = (DWORD*)GlobalLock(medEffect.hGlobal);
                    if (pdwEffect && (*pdwEffect & DROPEFFECT_MOVE)) {
                        isMove = true;
                    }
                    GlobalUnlock(medEffect.hGlobal);
                    ReleaseStgMedium(&medEffect);
                }

                bool operationDone = false;
                IFileOperation* pfo = nullptr;
                if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo)))) {
                    pfo->SetOperationFlags(FOF_ALLOWUNDO | FOFX_ADDUNDORECORD | FOF_NOCONFIRMMKDIR);
                    pfo->SetOwnerWindow(hwnd);
                    HRESULT hrAction = isMove ? pfo->MoveItems(pDataObj, psiDest) : pfo->CopyItems(pDataObj, psiDest);
                    if (SUCCEEDED(hrAction) && SUCCEEDED(pfo->PerformOperations())) {
                        operationDone = true;
                    }
                    pfo->Release();
                }

                if (!operationDone) {
                    IDropTarget* pDropTarget = nullptr;
                    if (SUCCEEDED(psiDest->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pDropTarget))) && pDropTarget) {
                        POINTL pt = { 0, 0 };
                        DWORD dwEffect = isMove ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
                        if (SUCCEEDED(pDropTarget->DragEnter(pDataObj, MK_LBUTTON, pt, &dwEffect)) && dwEffect != DROPEFFECT_NONE) {
                            HRESULT hrDrop = pDropTarget->Drop(pDataObj, 0, pt, &dwEffect);
                            if (SUCCEEDED(hrDrop)) {
                                operationDone = true;
                            }
                        } else {
                            pDropTarget->DragLeave();
                        }
                        pDropTarget->Release();
                    }
                }
                psiDest->Release();
            }
            pDataObj->Release();
        }
        OleUninitialize();

        if (hwnd && IsWindow(hwnd)) {
            HWND hRoot = GetAncestor(hwnd, GA_ROOT);
            if (hRoot && IsWindow(hRoot)) {
                PostMessageW(hRoot, WM_COMMAND, MAKEWPARAM(ID_VIEW_REFRESH, 0), 0);
            }
            PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_VIEW_REFRESH, 0), 0);
        }
    }).detach();

    return true;
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
        DWORD flags = permanent ? 0 : (FOF_ALLOWUNDO | FOFX_ADDUNDORECORD);
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
        pfo->SetOperationFlags(FOF_ALLOWUNDO | FOFX_ADDUNDORECORD);
        pfo->SetOwnerWindow(hwnd);

        IShellItem* psi = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(oldPath.c_str(), nullptr, IID_PPV_ARGS(&psi)))) {
            pfo->RenameItem(psi, newName.c_str(), nullptr);
            psi->Release();
            hr = pfo->PerformOperations();
            pfo->Release();
            if (SUCCEEDED(hr)) {
                wchar_t dir[MAX_PATH];
                StringCchCopyW(dir, MAX_PATH, oldPath.c_str());
                PathRemoveFileSpecW(dir);
                std::wstring newPath = std::wstring(dir) + L"\\" + newName;
                RecordRename(oldPath, newPath);
            }
            return SUCCEEDED(hr);
        }
        pfo->Release();
    }
    return false;
}

enum class UndoActionType {
    Rename,
    Create,
    Copy,
    Move
};

struct UndoItem {
    UndoActionType type;
    std::wstring path1; // oldPath / createdPath
    std::wstring path2; // newPath
    std::vector<std::pair<std::wstring, std::wstring>> items; // (src, dst)
};

static std::vector<UndoItem> s_undoStack;
static std::mutex s_undoMutex;
static bool s_isUndoing = false;

void RecordRename(const std::wstring& oldPath, const std::wstring& newPath) {
    std::lock_guard<std::mutex> lock(s_undoMutex);
    if (s_isUndoing) return;
    s_undoStack.push_back({ UndoActionType::Rename, oldPath, newPath, {} });
}

void RecordCreate(const std::wstring& createdPath) {
    std::lock_guard<std::mutex> lock(s_undoMutex);
    if (s_isUndoing) return;
    s_undoStack.push_back({ UndoActionType::Create, createdPath, L"", {} });
}

void RecordCopy(const std::vector<std::wstring>& createdPaths) {
    std::lock_guard<std::mutex> lock(s_undoMutex);
    if (s_isUndoing) return;
    std::vector<std::pair<std::wstring, std::wstring>> items;
    for (const auto& p : createdPaths) {
        items.push_back({ L"", p });
    }
    s_undoStack.push_back({ UndoActionType::Copy, L"", L"", items });
}

void RecordMove(const std::vector<std::pair<std::wstring, std::wstring>>& movedPairs) {
    std::lock_guard<std::mutex> lock(s_undoMutex);
    if (s_isUndoing) return;
    s_undoStack.push_back({ UndoActionType::Move, L"", L"", movedPairs });
}

bool CanUndo() {
    std::lock_guard<std::mutex> lock(s_undoMutex);
    return !s_undoStack.empty();
}

bool ExecuteUndo(HWND hwnd) {
    UndoItem item;
    {
        std::lock_guard<std::mutex> lock(s_undoMutex);
        if (s_undoStack.empty()) {
            INPUT inputs[4] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_CONTROL;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = 'Z';
            inputs[2].type = INPUT_KEYBOARD;
            inputs[2].ki.wVk = 'Z';
            inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
            inputs[3].type = INPUT_KEYBOARD;
            inputs[3].ki.wVk = VK_CONTROL;
            inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, inputs, sizeof(INPUT));
            return false;
        }

        item = s_undoStack.back();
        s_undoStack.pop_back();
    }

    s_isUndoing = true;
    bool success = false;

    switch (item.type) {
    case UndoActionType::Rename: {
        std::wstring oldName = PathFindFileNameW(item.path1.c_str());
        success = RenameItem(hwnd, item.path2, oldName);
        break;
    }
    case UndoActionType::Create: {
        if (PathFileExistsW(item.path1.c_str())) {
            success = DeleteFiles(hwnd, { item.path1 }, false);
        }
        break;
    }
    case UndoActionType::Copy: {
        std::vector<std::wstring> toDelete;
        for (const auto& p : item.items) {
            if (PathFileExistsW(p.second.c_str())) {
                toDelete.push_back(p.second);
            }
        }
        if (!toDelete.empty()) {
            success = DeleteFiles(hwnd, toDelete, false);
        }
        break;
    }
    case UndoActionType::Move: {
        IFileOperation* pfo = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo)))) {
            pfo->SetOperationFlags(FOF_ALLOWUNDO | FOFX_ADDUNDORECORD | FOF_NOCONFIRMMKDIR);
            pfo->SetOwnerWindow(hwnd);
            for (const auto& pair : item.items) {
                const std::wstring& dst = pair.second;
                const std::wstring& src = pair.first;
                wchar_t srcDir[MAX_PATH];
                StringCchCopyW(srcDir, MAX_PATH, src.c_str());
                PathRemoveFileSpecW(srcDir);

                IShellItem* psiDst = nullptr;
                IShellItem* psiSrcParent = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(dst.c_str(), nullptr, IID_PPV_ARGS(&psiDst)))) {
                    if (SUCCEEDED(SHCreateItemFromParsingName(srcDir, nullptr, IID_PPV_ARGS(&psiSrcParent)))) {
                        pfo->MoveItem(psiDst, psiSrcParent, PathFindFileNameW(src.c_str()), nullptr);
                        psiSrcParent->Release();
                    }
                    psiDst->Release();
                }
            }
            HRESULT hr = pfo->PerformOperations();
            success = SUCCEEDED(hr);
            pfo->Release();
        }
        break;
    }
    }

    s_isUndoing = false;
    return success;
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

void OpenInExplorer(const std::wstring& path) {
    wchar_t winDir[MAX_PATH];
    GetWindowsDirectoryW(winDir, MAX_PATH);
    std::wstring explorerExe = std::wstring(winDir) + L"\\explorer.exe";

    if (path.empty() || path == L"This PC" || path == L"此电脑" || path == L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}") {
        std::wstring args = L"/n,/e,::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
        ShellExecuteW(nullptr, nullptr, explorerExe.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
        return;
    }

    if (PathIsDirectoryW(path.c_str())) {
        std::wstring args = L"/n,/e,\"" + path + L"\"";
        ShellExecuteW(nullptr, nullptr, explorerExe.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        std::wstring args = L"/n,/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, nullptr, explorerExe.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
    }
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
    if (IsVirtualShellPath(path)) {
        PIDLIST_ABSOLUTE pidl = HexToPidl(path);
        if (pidl) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&pItem))) && pItem) {
                IShellItem* pParent = nullptr;
                HRESULT hrParent = pItem->GetParent(&pParent);
                pItem->Release();
                if (SUCCEEDED(hrParent) && pParent) {
                    LPWSTR pszName = nullptr;
                    pParent->GetDisplayName(SIGDN_NORMALDISPLAY, &pszName);
                    std::wstring parentName = pszName ? pszName : L"";
                    if (pszName) CoTaskMemFree(pszName);

                    if (parentName == L"Desktop" || parentName == L"桌面" || parentName == L"This PC" || parentName == L"此电脑") {
                        pParent->Release();
                        ILFree(pidl);
                        return L"This PC";
                    }

                    PIDLIST_ABSOLUTE parentPidl = nullptr;
                    if (SUCCEEDED(SHGetIDListFromObject(pParent, &parentPidl)) && parentPidl) {
                        std::wstring res = PidlToHex(parentPidl);
                        ILFree(parentPidl);
                        pParent->Release();
                        ILFree(pidl);
                        return res;
                    }
                    pParent->Release();
                }
            }
            ILFree(pidl);
        }
        return L"This PC";
    }

    std::wstring norm = Util::NormalizePath(path);
    if (norm.empty() || norm == L"此电脑" || norm == L"This PC") {
        return L"This PC";
    }
    if (_wcsicmp(norm.c_str(), L"\\\\wsl.localhost") == 0 || _wcsicmp(norm.c_str(), L"Linux") == 0) {
        return L"This PC";
    }
    if (_wcsnicmp(norm.c_str(), L"\\\\wsl.localhost\\", 16) == 0) {
        std::wstring rest = norm.substr(16);
        if (rest.find(L'\\') == std::wstring::npos) {
            return L"\\\\wsl.localhost";
        }
    }

    wchar_t buf[MAX_PATH];
    StringCchCopyW(buf, MAX_PATH, norm.c_str());
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
    if (IsVirtualShellPath(path)) {
        return GetVirtualItemDisplayName(path, false);
    }
    if (path.empty() || path == L"此电脑" || path == L"This PC" || path.find(L"20D04FE0-3AEA-1069-A2D8-08002B30309D") != std::wstring::npos) {
        return I18n::Get().T(StringId::ThisPC);
    }
    if (_wcsicmp(path.c_str(), L"\\\\wsl.localhost") == 0 || _wcsicmp(path.c_str(), L"Linux") == 0 ||
        path.find(L"B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6") != std::wstring::npos) {
        return L"Linux";
    }
    std::wstring p = path;
    while (p.length() > 3 && (p.back() == L'\\' || p.back() == L'/')) {
        p.pop_back();
    }
    // Drive root like "C:\" -> query shell for drive display name (e.g. "本地磁盘 (C:)")
    if (p.length() <= 3 && p.length() >= 2 && p[1] == L':') {
        SHFILEINFOW sfi = { 0 };
        std::wstring root = p;
        if (root.back() != L'\\') root += L'\\';
        if (SHGetFileInfoW(root.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME) && sfi.szDisplayName[0]) {
            return sfi.szDisplayName;
        }
    }
    // Check known personal folders
    static std::unordered_map<std::wstring, std::wstring> s_knownFolderNames;
    static bool s_knownInited = false;
    if (!s_knownInited) {
        s_knownInited = true;
        auto addKnown = [](int csidl) {
            wchar_t buf[MAX_PATH];
            if (SHGetFolderPathW(nullptr, csidl, nullptr, 0, buf) == S_OK) {
                std::wstring norm = Util::NormalizePath(buf);
                std::transform(norm.begin(), norm.end(), norm.begin(), ::towlower);
                SHFILEINFOW sfi = { 0 };
                if (SHGetFileInfoW(buf, 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME) && sfi.szDisplayName[0]) {
                    s_knownFolderNames[norm] = sfi.szDisplayName;
                }
            }
        };
        addKnown(CSIDL_DESKTOPDIRECTORY);
        addKnown(CSIDL_MYDOCUMENTS);
        addKnown(CSIDL_MYPICTURES);
        addKnown(CSIDL_MYMUSIC);
        addKnown(CSIDL_MYVIDEO);
        wchar_t userProfile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            std::wstring dl = Util::NormalizePath(std::wstring(userProfile) + L"\\Downloads");
            std::wstring dlLower = dl;
            std::transform(dlLower.begin(), dlLower.end(), dlLower.begin(), ::towlower);
            SHFILEINFOW sfi = { 0 };
            if (SHGetFileInfoW(dl.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME) && sfi.szDisplayName[0]) {
                s_knownFolderNames[dlLower] = sfi.szDisplayName;
            }
        }
    }

    std::wstring normP = Util::NormalizePath(p);
    std::wstring lowerP = normP;
    std::transform(lowerP.begin(), lowerP.end(), lowerP.begin(), ::towlower);
    auto itK = s_knownFolderNames.find(lowerP);
    if (itK != s_knownFolderNames.end()) {
        return itK->second;
    }

    const wchar_t* fn = PathFindFileNameW(normP.c_str());
    if (fn && *fn) {
        return fn;
    }
    return normP;
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
            pmedium->pUnkForRelease = nullptr;
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
        if (IsVirtualShellPath(p)) {
            pidl = HexToPidl(p);
        } else {
            SHParseDisplayName(p.c_str(), nullptr, &pidl, 0, nullptr);
        }
        if (pidl) {
            pidls.push_back(pidl);
        }
    }

    IDataObject* pDataObj = nullptr;
    if (!pidls.empty()) {
        IShellItemArray* pArray = nullptr;
        if (SUCCEEDED(SHCreateShellItemArrayFromIDLists((UINT)pidls.size(), (PCIDLIST_ABSOLUTE_ARRAY)pidls.data(), &pArray)) && pArray) {
            if (FAILED(pArray->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&pDataObj))) || !pDataObj) {
                pArray->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pDataObj));
            }
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

    ReleaseCapture();

    IDataObject* pDataObj = CreateFileDataObject(hwnd, currentDir, paths);
    if (!pDataObj) return E_FAIL;

    SimpleDropSource* pDropSource = new SimpleDropSource();

    IDragSourceHelper* pdsh = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pdsh)))) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
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

bool IsSystemSettingsOrControlPanel(const std::wstring& arg) {
    if (arg.empty()) return false;

    // 1. Control Panel CPL files, rundll invocations, or Windows Firewall components
    if (StrStrIW(arg.c_str(), L".cpl") ||
        StrStrIW(arg.c_str(), L"Control_RunDLL") ||
        StrStrIW(arg.c_str(), L"Microsoft.WindowsFirewall") ||
        StrStrIW(arg.c_str(), L"FirewallControlPanel")) {
        return true;
    }

    // 2. Shell virtual namespaces: shell:::{...} or :::{...} or ::{...}
    if (_wcsnicmp(arg.c_str(), L"shell:::", 8) == 0 ||
        _wcsnicmp(arg.c_str(), L":::{", 4) == 0 ||
        _wcsnicmp(arg.c_str(), L"::{", 3) == 0) {
        // Exclude "This PC" / My Computer GUID: {20D04FE0-3AEA-1069-A2D8-08002B30309D}
        if (StrStrIW(arg.c_str(), L"20D04FE0-3AEA-1069-A2D8-08002B30309D")) {
            return false;
        }
        // Exclude WSL Linux GUID: {B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6}
        if (StrStrIW(arg.c_str(), L"B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6")) {
            return false;
        }
        // Exclude internal PIDL virtual paths: shell:pidl:
        if (arg.find(L"shell:pidl:") != std::wstring::npos) {
            return false;
        }
        // All other GUID-based shell namespaces are system settings/control panels/recycle bin
        return true;
    }

    // 3. Named system shell special folders (e.g. shell:ControlPanelFolder, shell:System, shell:PrintersFolder)
    if (_wcsnicmp(arg.c_str(), L"shell:", 6) == 0) {
        if (_wcsnicmp(arg.c_str(), L"shell:pidl:", 11) == 0) {
            return false;
        }
        return true;
    }

    // 4. Known system control panel and settings GUIDs
    static const wchar_t* s_systemGuids[] = {
        L"4026492F-2F69-46B8-B9BF-5654FC07E423", // Windows Defender Firewall
        L"645FF040-5081-101B-9F08-00AA002F954E", // Recycle Bin
        L"21EC2020-3AEA-1069-A2DD-08002B30309D", // All Control Panel Items
        L"26EE0668-A00A-44D7-9371-BEB064C98683", // Control Panel
        L"7007ACC7-3202-11D1-AAD2-00805FC1270E", // Network Connections
        L"7B81BE6A-CE2B-4676-A29E-EB907A5126C5", // Programs and Features
        L"D20EA4E1-3957-11D2-A40B-0C5020524153", // Windows Tools
        L"ED834ED6-4B5A-4BFE-8F11-A626DCB6A921", // Personalization
        L"BB06C0E4-D293-4F75-8A90-CB05B6477EEE", // System
        L"96AE8D80-A222-4879-97AF-BD65E52C3713"  // Security and Maintenance
    };
    for (const auto* guid : s_systemGuids) {
        if (StrStrIW(arg.c_str(), guid)) {
            return true;
        }
    }

    return false;
}

bool SetExplorerReplaced(bool enable) {
    // Windows 10 and 11 delegate directory/folder clicks through DelegateExecute COM handlers.
    // To cleanly override Windows Explorer per-user without requiring Administrator/UAC elevation:
    // 1. Override HKCU\Software\Classes\Directory\shell\open\command & explore\command (file system directories)
    // 2. Override HKCU\Software\Classes\Drive\shell\open\command & explore\command (disk drives)
    // 3. Override HKCU\Software\Classes\CLSID\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\shell\open & explore (Explorer shortcuts & Win+E)
    // NOTE: We deliberately do NOT override HKCU\Software\Classes\Folder so system settings, Control Panel
    // applets (e.g. Windows Firewall), and virtual system folders are NOT intercepted!
    const wchar_t* openCmdKeys[] = {
        L"Software\\Classes\\Directory\\shell\\open\\command",
        L"Software\\Classes\\Directory\\shell\\explore\\command",
        L"Software\\Classes\\Directory\\shell\\opennewwindow\\command",
        L"Software\\Classes\\Directory\\shell\\opennewtab\\command",
        L"Software\\Classes\\Drive\\shell\\open\\command",
        L"Software\\Classes\\Drive\\shell\\explore\\command",
        L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\open\\command",
        L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\explore\\command",
        L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\opennewwindow\\command",
        L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\opennewtab\\command",
        L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\open\\command",
        L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\explore\\command",
        L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\opennewwindow\\command",
        L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\opennewtab\\command"
    };

    const wchar_t* shellParents[] = {
        L"Software\\Classes\\Directory\\shell",
        L"Software\\Classes\\Drive\\shell",
        L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell"
    };

    // Always clean up any legacy Folder\shell hooks so system settings/firewall are never trapped
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell");

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        // Direct invocation for instant folder opening
        std::wstring cmdStr = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
        const wchar_t* emptyDelegate = L"";

        // Set (Default) = "open" so verb-less opens always use our open verb
        for (const auto* sp : shellParents) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, sp, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)L"open", sizeof(L"open"));
                RegCloseKey(hKey);
            }
        }

        for (const auto* k : openCmdKeys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, k, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)cmdStr.c_str(), (DWORD)((cmdStr.length() + 1) * sizeof(wchar_t)));
                // Clear DelegateExecute so Windows Explorer doesn't hijack the verb
                RegSetValueExW(hKey, L"DelegateExecute", 0, REG_SZ, (const BYTE*)emptyDelegate, sizeof(wchar_t));
                RegCloseKey(hKey);
            }
        }

        // Set DisabledHotkeys to include 'E' so system Explorer doesn't claim Win+E at login
        HKEY hAdvKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hAdvKey, nullptr) == ERROR_SUCCESS) {
            wchar_t disKeys[64] = { 0 };
            DWORD size = sizeof(disKeys);
            DWORD type = 0;
            RegQueryValueExW(hAdvKey, L"DisabledHotkeys", nullptr, &type, (LPBYTE)disKeys, &size);
            std::wstring strDis = disKeys;
            if (strDis.find(L'E') == std::wstring::npos && strDis.find(L'e') == std::wstring::npos) {
                strDis += L"E";
                RegSetValueExW(hAdvKey, L"DisabledHotkeys", 0, REG_SZ, (const BYTE*)strDis.c_str(), (DWORD)((strDis.length() + 1) * sizeof(wchar_t)));
            }
            RegCloseKey(hAdvKey);
        }

        // Clean up legacy vbs script if present
        std::wstring scriptPath = GetLauncherScriptPath();
        if (!scriptPath.empty() && PathFileExistsW(scriptPath.c_str())) {
            DeleteFileW(scriptPath.c_str());
        }
    } else {
        for (const auto* k : openCmdKeys) {
            RegDeleteKeyW(HKEY_CURRENT_USER, k);
        }
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\explore");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\opennewwindow");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\opennewtab");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\explore");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\opennewwindow");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\opennewtab");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\explore");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\explore");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\opennewwindow");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell\\opennewtab");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}\\shell");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{5220cb21-c88d-4a0b-ae93-18f2e6469ced}");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\open\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\open");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\explore\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\explore");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\opennewwindow\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\opennewwindow");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\opennewtab\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell\\opennewtab");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\shell");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}");

        // Clean up legacy open_NeoMDIE if present
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\open_NeoMDIE");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\open_NeoMDIE");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE\\command");
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\open_NeoMDIE");

        // Clear (Default) value on shell parents
        for (const auto* sp : shellParents) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, sp, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, nullptr);
                RegCloseKey(hKey);
            }
        }
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell");

        // Remove 'E' from DisabledHotkeys
        HKEY hAdvKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", 0, KEY_READ | KEY_WRITE, &hAdvKey) == ERROR_SUCCESS) {
            wchar_t disKeys[64] = { 0 };
            DWORD size = sizeof(disKeys);
            DWORD type = 0;
            if (RegQueryValueExW(hAdvKey, L"DisabledHotkeys", nullptr, &type, (LPBYTE)disKeys, &size) == ERROR_SUCCESS) {
                std::wstring strDis = disKeys;
                size_t pos;
                while ((pos = strDis.find(L'E')) != std::wstring::npos) strDis.erase(pos, 1);
                while ((pos = strDis.find(L'e')) != std::wstring::npos) strDis.erase(pos, 1);
                if (strDis.empty()) {
                    RegDeleteValueW(hAdvKey, L"DisabledHotkeys");
                } else {
                    RegSetValueExW(hAdvKey, L"DisabledHotkeys", 0, REG_SZ, (const BYTE*)strDis.c_str(), (DWORD)((strDis.length() + 1) * sizeof(wchar_t)));
                }
            }
            RegCloseKey(hAdvKey);
        }

        // Clean launcher script
        std::wstring scriptPath = GetLauncherScriptPath();
        if (!scriptPath.empty() && PathFileExistsW(scriptPath.c_str())) {
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

        const std::wstring currentVer = L"v1.1.4";
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

struct DriveSpaceCacheEntry {
    uint64_t freeBytes = 0;
    uint64_t totalBytes = 0;
    DWORD lastTick = 0;
    bool success = false;
};
static std::unordered_map<wchar_t, DriveSpaceCacheEntry> s_driveSpaceCache;
static std::mutex s_driveSpaceMutex;

bool GetDriveSpace(const std::wstring& drivePath, uint64_t& outFreeBytes, uint64_t& outTotalBytes) {
    if (drivePath.length() >= 2 && drivePath[1] == L':') {
        wchar_t letter = (wchar_t)towupper(drivePath[0]);
        DWORD now = GetTickCount();
        {
            std::lock_guard<std::mutex> lock(s_driveSpaceMutex);
            auto it = s_driveSpaceCache.find(letter);
            if (it != s_driveSpaceCache.end() && (now - it->second.lastTick < 5000)) {
                if (it->second.success) {
                    outFreeBytes = it->second.freeBytes;
                    outTotalBytes = it->second.totalBytes;
                    return true;
                }
                return false;
            }
        }

        std::wstring root = drivePath.substr(0, 2) + L"\\";
        ULARGE_INTEGER freeBytesAvailable = { 0 };
        ULARGE_INTEGER totalNumberOfBytes = { 0 };
        ULARGE_INTEGER totalNumberOfFreeBytes = { 0 };
        bool ok = (GetDiskFreeSpaceExW(root.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes) != 0);

        {
            std::lock_guard<std::mutex> lock(s_driveSpaceMutex);
            DriveSpaceCacheEntry entry;
            entry.lastTick = now;
            entry.success = ok;
            if (ok) {
                entry.freeBytes = freeBytesAvailable.QuadPart;
                entry.totalBytes = totalNumberOfBytes.QuadPart;
                outFreeBytes = entry.freeBytes;
                outTotalBytes = entry.totalBytes;
            }
            s_driveSpaceCache[letter] = entry;
        }
        return ok;
    }
    return false;
}

} // namespace ShellUtils
