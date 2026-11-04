#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <vector>
#include <string>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

int GetItemIconIndex(const std::wstring& path, DWORD attr, bool isFolder) {
    SHFILEINFOW sfi = { 0 };
    DWORD flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    if (isFolder) {
        flags |= SHGFI_USEFILEATTRIBUTES;
        SHGetFileInfoW(L"dummy_folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), flags);
    } else {
        if (PathFileExistsW(path.c_str())) {
            SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), flags);
        } else {
            flags |= SHGFI_USEFILEATTRIBUTES;
            SHGetFileInfoW(path.c_str(), attr ? attr : FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
        }
    }
    return sfi.iIcon;
}

int main() {
    std::wstring folder = L"D:\\github\\NeoMDIE\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(folder.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring fullPath = L"D:\\github\\NeoMDIE\\" + std::wstring(fd.cFileName);
            bool isFolder = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            int idx = GetItemIconIndex(fullPath, fd.dwFileAttributes, isFolder);
            printf("%-30ls: isFolder=%d, exists=%d, iconIndex=%d\n", fd.cFileName, isFolder, PathFileExistsW(fullPath.c_str()), idx);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return 0;
}
