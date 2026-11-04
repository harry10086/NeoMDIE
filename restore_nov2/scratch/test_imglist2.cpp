#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

int main() {
    SHFILEINFOW sfi = {0};
    HIMAGELIST hSmall = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    printf("Initial count = %d\n", ImageList_GetImageCount(hSmall));

    // Now query a new file type that might not be loaded yet
    ZeroMemory(&sfi, sizeof(sfi));
    SHGetFileInfoW(L"something.docx", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    printf("something.docx: iIcon = %d, new count = %d\n", sfi.iIcon, ImageList_GetImageCount(hSmall));

    ZeroMemory(&sfi, sizeof(sfi));
    SHGetFileInfoW(L"something.xlsx", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    printf("something.xlsx: iIcon = %d, new count = %d\n", sfi.iIcon, ImageList_GetImageCount(hSmall));

    ZeroMemory(&sfi, sizeof(sfi));
    SHGetFileInfoW(L"something.zip", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    printf("something.zip: iIcon = %d, new count = %d\n", sfi.iIcon, ImageList_GetImageCount(hSmall));

    return 0;
}
