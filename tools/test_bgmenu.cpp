#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

int main() {
    CoInitialize(nullptr);

    IShellItem* pItem = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(L"C:\\Users", nullptr, IID_PPV_ARGS(&pItem));
    printf("SHCreateItemFromParsingName hr = 0x%08X, pItem = %p\n", hr, pItem);

    if (SUCCEEDED(hr)) {
        IShellFolder* pFolder = nullptr;
        hr = pItem->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&pFolder));
        printf("BindToHandler(BHID_SFObject) hr = 0x%08X, pFolder = %p\n", hr, pFolder);

        if (SUCCEEDED(hr)) {
            IContextMenu* pContextMenu = nullptr;
            hr = pFolder->CreateViewObject(nullptr, IID_PPV_ARGS(&pContextMenu));
            printf("pFolder->CreateViewObject(IContextMenu) hr = 0x%08X, pContextMenu = %p\n", hr, pContextMenu);

            if (FAILED(hr)) {
                // Try BHID_SFViewObject or IShellItem::BindToHandler
                hr = pItem->BindToHandler(nullptr, BHID_SFViewObject, IID_PPV_ARGS(&pContextMenu));
                printf("pItem->BindToHandler(BHID_SFViewObject) hr = 0x%08X, pContextMenu = %p\n", hr, pContextMenu);
            }
            if (pContextMenu) {
                HMENU hMenu = CreatePopupMenu();
                pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x6FFF, CMF_NORMAL);
                int count = GetMenuItemCount(hMenu);
                printf("QueryContextMenu item count: %d\n", count);
                for (int i = 0; i < count; ++i) {
                    wchar_t str[128] = { 0 };
                    GetMenuStringW(hMenu, i, str, 128, MF_BYPOSITION);
                    wprintf(L"  Item %d: %s\n", i, str);
                }
                DestroyMenu(hMenu);
                pContextMenu->Release();
            }
            pFolder->Release();
        }
        pItem->Release();
    }

    CoUninitialize();
    return 0;
}
