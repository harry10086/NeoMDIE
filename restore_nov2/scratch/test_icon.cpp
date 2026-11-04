#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

int main() {
    SHFILEINFOW sfi = {0};
    // 1. Existing txt file
    DWORD flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    SHGetFileInfoW(L"C:\\Windows\\System32\\notepad.exe", 0, &sfi, sizeof(sfi), flags);
    printf("notepad.exe: iconIndex=%d, type=%ls\n", sfi.iIcon, sfi.szTypeName);

    ZeroMemory(&sfi, sizeof(sfi));
    SHGetFileInfoW(L"dummy_folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), flags | SHGFI_USEFILEATTRIBUTES);
    printf("dummy_folder: iconIndex=%d, type=%ls\n", sfi.iIcon, sfi.szTypeName);

    ZeroMemory(&sfi, sizeof(sfi));
    SHGetFileInfoW(L"test.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags | SHGFI_USEFILEATTRIBUTES);
    printf("test.txt (USEFILEATTRIBUTES): iconIndex=%d, type=%ls\n", sfi.iIcon, sfi.szTypeName);

    ZeroMemory(&sfi, sizeof(sfi));
    SHGetFileInfoW(L"C:\\dummy.pdf", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags | SHGFI_USEFILEATTRIBUTES);
    printf("dummy.pdf: iconIndex=%d, type=%ls\n", sfi.iIcon, sfi.szTypeName);

    return 0;
}
