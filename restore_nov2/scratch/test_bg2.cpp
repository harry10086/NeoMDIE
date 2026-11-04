#include <windows.h>
#include <shlobj.h>
#include <iostream>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

int main() {
    CoInitialize(nullptr);
    SetConsoleOutputCP(CP_UTF8);
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(L"C:\\Users");
    IShellFolder* pFolder = nullptr;
    SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void**)&pFolder);
    if (!pFolder) {
        std::cout << "No folder\n";
        return 0;
    }

    // 1. CreateViewObject
    IContextMenu* pCM1 = nullptr;
    pFolder->CreateViewObject(nullptr, IID_IContextMenu, (void**)&pCM1);
    HMENU hMenu1 = CreatePopupMenu();
    if (pCM1) {
        pCM1->QueryContextMenu(hMenu1, 0, 1, 0x1000, CMF_NORMAL);
        int cnt = GetMenuItemCount(hMenu1);
        std::cout << "CreateViewObject count: " << cnt << std::endl;
        for (int i = 0; i < cnt; ++i) {
            wchar_t text[128] = { 0 };
            GetMenuStringW(hMenu1, i, text, 128, MF_BYPOSITION);
            UINT id = GetMenuItemID(hMenu1, i);
            HMENU hSub = GetSubMenu(hMenu1, i);
            std::wstring s = text;
            std::string utf8;
            int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                utf8.resize(len);
                WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &utf8[0], len, nullptr, nullptr);
            }
            std::cout << "  [" << i << "] id=" << id << " sub=" << (hSub ? "yes" : "no")
                      << " text=\"" << utf8 << "\"\n";
        }
    }
    return 0;
}
