#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

int main() {
    CoInitialize(NULL);
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(L"D:\\github\\NeoMDIE");
    if (!pidl) {
        printf("ILCreateFromPathW failed\n");
        return 1;
    }
    IShellFolder* pFolder = nullptr;
    HRESULT hr = SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void**)&pFolder);
    printf("SHBindToObject hr=0x%lx, pFolder=%p\n", hr, pFolder);
    if (SUCCEEDED(hr) && pFolder) {
        IContextMenu* pMenu = nullptr;
        hr = pFolder->CreateViewObject(NULL, IID_IContextMenu, (void**)&pMenu);
        printf("CreateViewObject hr=0x%lx, pMenu=%p\n", hr, pMenu);
        if (SUCCEEDED(hr) && pMenu) {
            HMENU hMenu = CreatePopupMenu();
            pMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL | CMF_EXPLORE);
            int count = GetMenuItemCount(hMenu);
            printf("Background menu items count = %d\n", count);
            for (int i = 0; i < count; ++i) {
                wchar_t text[128] = {0};
                GetMenuStringW(hMenu, i, text, 128, MF_BYPOSITION);
                printf("  Item %d: %ls\n", i, text);
            }
            DestroyMenu(hMenu);
            pMenu->Release();
        }
        pFolder->Release();
    }
    ILFree(pidl);
    return 0;
}
