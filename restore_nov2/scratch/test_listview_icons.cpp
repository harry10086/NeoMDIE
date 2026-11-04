#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commoncontrols.h>
#include <stdio.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

HIMAGELIST GetSysImgList() {
    typedef HRESULT(WINAPI* fnSHGetImageList)(int, REFIID, void**);
    HMODULE hShell = GetModuleHandleW(L"shell32.dll");
    auto pfn = (fnSHGetImageList)GetProcAddress(hShell, "SHGetImageList");
    IImageList* piml = nullptr;
    pfn(SHIL_SMALL, IID_PPV_ARGS(&piml));
    return (HIMAGELIST)piml;
}

int GetIcon(const wchar_t* path) {
    SHFILEINFOW sfi = { 0 };
    SHGetFileInfoW(path, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    return sfi.iIcon;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NOTIFY) {
        NMHDR* nm = (NMHDR*)lParam;
        if (nm->code == LVN_GETDISPINFOW) {
            NMLVDISPINFOW* pDisp = (NMLVDISPINFOW*)lParam;
            printf("LVN_GETDISPINFO: item=%d, subItem=%d, mask=0x%x (LVIF_IMAGE=%d)\n",
                   pDisp->item.iItem, pDisp->item.iSubItem, pDisp->item.mask, (pDisp->item.mask & LVIF_IMAGE) != 0);
            if (pDisp->item.mask & LVIF_TEXT) {
                pDisp->item.pszText = (LPWSTR)L"Item";
            }
            if (pDisp->item.mask & LVIF_IMAGE) {
                pDisp->item.iImage = (pDisp->item.iItem == 0) ? GetIcon(L"dummy_folder") : GetIcon(L"C:\\Windows\\notepad.exe");
                printf("  -> Set iImage=%d\n", pDisp->item.iImage);
            }
            return 0;
        }
    } else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int main() {
    CoInitialize(NULL);
    InitCommonControls();
    
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"TestLV";
    RegisterClassExW(&wc);

    HWND hwndMain = CreateWindowExW(0, L"TestLV", L"Test", WS_OVERLAPPEDWINDOW, 100, 100, 400, 300, NULL, NULL, wc.hInstance, NULL);
    HWND hList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHAREIMAGELISTS,
                                0, 0, 380, 260, hwndMain, (HMENU)1, wc.hInstance, NULL);

    HIMAGELIST himl = GetSysImgList();
    ListView_SetImageList(hList, himl, LVSIL_SMALL);

    LVCOLUMNW col = { 0 };
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 200;
    col.pszText = (LPWSTR)L"Name";
    ListView_InsertColumn(hList, 0, &col);

    ListView_SetItemCount(hList, 2);

    ShowWindow(hwndMain, SW_SHOW);
    UpdateWindow(hwndMain);

    MSG msg;
    int count = 0;
    while (count < 10 && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        count++;
    }
    return 0;
}
