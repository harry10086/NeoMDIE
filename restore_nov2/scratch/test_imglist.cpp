#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commoncontrols.h>
#include <commctrl.h>
#include <stdio.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

int main() {
    CoInitialize(NULL);
    SHFILEINFOW sfi = {0};
    HIMAGELIST hSmall1 = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    printf("SHGetFileInfo Small HIMAGELIST = %p, icon count = %d\n", hSmall1, ImageList_GetImageCount(hSmall1));

    IImageList* piml = NULL;
    HRESULT hr = SHGetImageList(SHIL_SMALL, IID_PPV_ARGS(&piml));
    printf("SHGetImageList hr=%lx, piml=%p\n", hr, piml);
    if (SUCCEEDED(hr)) {
        int count = 0;
        piml->GetImageCount(&count);
        printf("IImageList count = %d\n", count);
        HIMAGELIST hSmall2 = (HIMAGELIST)piml;
        printf("Are they equal? %d\n", (hSmall1 == hSmall2));
    }
    return 0;
}
