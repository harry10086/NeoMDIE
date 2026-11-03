#include "ScriptEngine.h"
#include "..\resource.h"
#include "..\ui\Theme.h"

ScriptEngine& ScriptEngine::Get() {
    static ScriptEngine instance;
    return instance;
}

ScriptEngine::ScriptEngine() {
}

ScriptEngine::~ScriptEngine() {
    if (m_hwndConsole) {
        DestroyWindow(m_hwndConsole);
    }
}

void ScriptEngine::Initialize(HWND hwndMain) {
    m_hwndMain = hwndMain;
    ReloadScripts();
}

void ScriptEngine::ParseScriptHeader(const std::wstring& filePath, ScriptInfo& info) {
    info.filePath = filePath;
    info.name = PathFindFileNameW(filePath.c_str());
    info.description = L"";
    info.hotkey = L"";
    info.showInMenu = true;
    info.showInToolbar = false;

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    char buffer[4096] = { 0 };
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (bytesRead == 0) return;
    std::string content(buffer, bytesRead);
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("// ==/NeoScript==") != std::string::npos) break;
        if (line.find("// @name") != std::string::npos) {
            size_t pos = line.find("@name") + 5;
            while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
            info.name = Util::Utf8ToWide(line.substr(pos));
        } else if (line.find("// @description") != std::string::npos) {
            size_t pos = line.find("@description") + 12;
            while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
            info.description = Util::Utf8ToWide(line.substr(pos));
        } else if (line.find("// @hotkey") != std::string::npos) {
            size_t pos = line.find("@hotkey") + 7;
            while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
            info.hotkey = Util::Utf8ToWide(line.substr(pos));
        } else if (line.find("// @toolbar") != std::string::npos) {
            info.showInToolbar = true;
        }
    }
}

void ScriptEngine::ReloadScripts() {
    m_scripts.clear();

    std::wstring scriptsDir = Util::GetExecutableDir() + L"\\scripts";
    CreateDirectoryW(scriptsDir.c_str(), nullptr);

    std::wstring searchPattern = scriptsDir + L"\\*.js";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);

    UINT nextCmdId = ID_SCRIPT_COMMAND_START;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                ScriptInfo info;
                std::wstring fullPath = scriptsDir + L"\\" + fd.cFileName;
                ParseScriptHeader(fullPath, info);
                info.commandId = nextCmdId++;
                m_scripts.push_back(info);
                if (nextCmdId > ID_SCRIPT_COMMAND_END) break;
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    Log(L"扩展脚本重新加载完成，共载入 " + std::to_wstring(m_scripts.size()) + L" 个脚本。");
}

bool ScriptEngine::ExecuteCommand(UINT cmdId, const std::wstring& activePath, const std::vector<std::wstring>& selectedFiles) {
    for (const auto& s : m_scripts) {
        if (s.commandId == cmdId) {
            return ExecuteScript(s.filePath, activePath, selectedFiles);
        }
    }
    return false;
}

bool ScriptEngine::ExecuteScript(const std::wstring& filePath, const std::wstring& activePath, const std::vector<std::wstring>& selectedFiles) {
    Log(L"正在执行脚本: " + filePath);

    // Pass environment variables to script
    SetEnvironmentVariableW(L"NEOMDIE_ACTIVE_PATH", activePath.c_str());

    std::wstring filesJoined;
    for (const auto& f : selectedFiles) {
        if (!filesJoined.empty()) filesJoined += L"|";
        filesJoined += f;
    }
    SetEnvironmentVariableW(L"NEOMDIE_SELECTED_FILES", filesJoined.c_str());

    // Execute via Windows Script Host / cscript
    std::wstring cmd = L"cscript.exe //NoLogo \"" + filePath + L"\"";

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    BOOL success = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, activePath.c_str(), &si, &pi);

    CloseHandle(hWritePipe);

    if (success) {
        CloseHandle(pi.hThread);

        // Read output
        char buffer[1024];
        DWORD bytesRead = 0;
        std::string output;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);

        if (!output.empty()) {
            Log(Util::Utf8ToWide(output));
        }
        Log(L"脚本执行完毕。");
    } else {
        Log(L"脚本启动失败！");
    }

    CloseHandle(hReadPipe);
    return success != FALSE;
}

void ScriptEngine::Log(const std::wstring& msg) {
    m_logs.push_back(msg);
    if (m_hwndConsole && IsWindow(m_hwndConsole)) {
        HWND hEdit = GetDlgItem(m_hwndConsole, 101);
        if (hEdit) {
            std::wstring fullText;
            for (const auto& l : m_logs) {
                fullText += l + L"\r\n";
            }
            SetWindowTextW(hEdit, fullText.c_str());
            SendMessageW(hEdit, EM_SETSEL, (WPARAM)fullText.length(), (LPARAM)fullText.length());
            SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
        }
    }
}

static LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        HWND hEdit = GetDlgItem(hwnd, 101);
        if (hEdit) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(hEdit, 0, 0, rc.right, rc.bottom, TRUE);
        }
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ScriptEngine::ShowConsole() {
    if (!m_hwndConsole) {
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = ConsoleWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"NeoMDIE_ScriptConsole";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwndConsole = CreateWindowExW(
            WS_EX_TOOLWINDOW, L"NeoMDIE_ScriptConsole", L"NeoMDIE 扩展脚本控制台",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
            m_hwndMain, nullptr, hInst, nullptr
        );

        HWND hEdit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            0, 0, 600, 400, m_hwndConsole, (HMENU)101, hInst, nullptr
        );

        HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        Theme::Get().ApplyToWindow(m_hwndConsole);
    }

    ShowWindow(m_hwndConsole, SW_SHOW);
    SetForegroundWindow(m_hwndConsole);
}
