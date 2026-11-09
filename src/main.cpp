#include "Common.h"
#include "resource.h"
#include "core\Config.h"
#include "core\ShellUtils.h"
#include "ui\Theme.h"
#include "ui\MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // 1. Enable Per-Monitor V2 DPI Awareness
    using fnSetProcessDpiAwarenessContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto pfnSetDpi = (fnSetProcessDpiAwarenessContext)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (pfnSetDpi) {
        pfnSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    // 2. Initialize OLE / COM
    HRESULT hr = OleInitialize(nullptr);
    if (FAILED(hr)) return 1;

    // 3. Load Configuration
    Config& cfg = Config::Get();
    cfg.Load();

    // Parse command line folder argument early
    std::wstring folderArg;
    int nArgs = 0;
    LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (szArglist && nArgs > 1) {
        folderArg = szArglist[1];
        LocalFree(szArglist);
    }
    while (!folderArg.empty() && (folderArg.back() == L'"' || folderArg.back() == L' ')) folderArg.pop_back();
    while (!folderArg.empty() && folderArg.front() == L'"') folderArg.erase(folderArg.begin());
    if (folderArg.length() == 2 && folderArg[1] == L':') folderArg += L"\\";
    if (folderArg.find(L"645FF040-5081-101B-9F08-00AA002F954E") != std::wstring::npos ||
        folderArg.find(L"RecycleBinFolder") != std::wstring::npos) {
        ShellExecuteW(nullptr, L"open", L"explorer.exe", L"::{645FF040-5081-101B-9F08-00AA002F954E}", nullptr, SW_SHOWNORMAL);
        OleUninitialize();
        return 0;
    }
    if (folderArg.find(L"20D04FE0-3AEA-1069-A2D8-08002B30309D") != std::wstring::npos) {
        folderArg = L"This PC";
    }

    // 4. Single Instance / Explorer Replace Check & IPC
    HANDLE hMutex = nullptr;
    if (cfg.singleInstance || ShellUtils::IsExplorerReplaced()) {
        hMutex = CreateMutexW(nullptr, FALSE, L"Local\\NeoMDIE_SingleInstance_Mutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND hExisting = FindWindowW(L"NeoMDIE_MainWindow", nullptr);
            if (hExisting) {
                if (!folderArg.empty()) {
                    COPYDATASTRUCT cds;
                    cds.dwData = 1;
                    cds.cbData = (DWORD)((folderArg.length() + 1) * sizeof(wchar_t));
                    cds.lpData = (PVOID)folderArg.c_str();
                    SendMessageW(hExisting, WM_COPYDATA, 0, (LPARAM)&cds);
                }
                if (!IsWindowVisible(hExisting) || IsIconic(hExisting)) {
                    ShowWindow(hExisting, SW_RESTORE);
                }
                SetForegroundWindow(hExisting);
                if (hMutex) CloseHandle(hMutex);
                OleUninitialize();
                return 0;
            }
        }
    }

    // 5. Initialize Common Controls
    INITCOMMONCONTROLSEX icex = { sizeof(icex) };
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_COOL_CLASSES | ICC_USEREX_CLASSES;
    InitCommonControlsEx(&icex);

    // 6. Initialize Theme and Shell
    Theme::Get().Initialize();
    if (cfg.themeMode == 0) {
        Theme::Get().SetDarkMode(false);
    } else if (cfg.themeMode == 1) {
        Theme::Get().SetDarkMode(true);
    }
    ShellUtils::Initialize();

    // 7. Create Main Window
    MainWindow mainWin;
    HWND hwnd = mainWin.Create(hInstance, nCmdShow, folderArg);
    if (!hwnd) {
        if (hMutex) CloseHandle(hMutex);
        OleUninitialize();
        return 1;
    }

    // 8. Dynamic Accelerators (Managed by MainWindow & Config)
    HACCEL hAccel = mainWin.GetAcceleratorTable();

    // 9. Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        bool skipAccel = false;
        HWND hFocus = GetFocus();
        if (hFocus && msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
            WCHAR cls[32] = { 0 };
            GetClassNameW(hFocus, cls, 32);
            if (_wcsicmp(cls, L"Edit") == 0) {
                if (msg.wParam == VK_DELETE || msg.wParam == VK_BACK ||
                    msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT ||
                    msg.wParam == VK_UP || msg.wParam == VK_DOWN ||
                    msg.wParam == VK_HOME || msg.wParam == VK_END ||
                    msg.wParam == VK_PRIOR || msg.wParam == VK_NEXT) {
                    skipAccel = true;
                } else if (GetKeyState(VK_CONTROL) < 0) {
                    if (msg.wParam == 'A' || msg.wParam == 'C' || msg.wParam == 'V' ||
                        msg.wParam == 'X' || msg.wParam == 'Z') {
                        skipAccel = true;
                    }
                }
            }
        }

        HACCEL curAccel = mainWin.GetAcceleratorTable();
        if (!curAccel) curAccel = hAccel;

        if (skipAccel || !curAccel || !TranslateAcceleratorW(hwnd, curAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hMutex) CloseHandle(hMutex);
    OleUninitialize();
    return (int)msg.wParam;
}
