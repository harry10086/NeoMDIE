#include "I18n.h"
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

static std::wstring ReadScriptFileToString(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return L"";

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return L"";
    }

    std::vector<BYTE> buffer(fileSize + 2, 0);
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (bytesRead == 0) return L"";

    std::wstring wContent;
    if (bytesRead >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        int len = MultiByteToWideChar(CP_UTF8, 0, (char*)(buffer.data() + 3), bytesRead - 3, nullptr, 0);
        if (len > 0) {
            wContent.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, (char*)(buffer.data() + 3), bytesRead - 3, &wContent[0], len);
        }
    } else if (bytesRead >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) {
        wContent.assign((wchar_t*)(buffer.data() + 2), (bytesRead - 2) / sizeof(wchar_t));
    } else {
        wContent = Util::DecodeOutput(std::string((char*)buffer.data(), bytesRead));
    }
    return wContent;
}

void ScriptEngine::ParseScriptHeader(const std::wstring& filePath, ScriptInfo& info) {
    info.filePath = filePath;
    info.name = PathFindFileNameW(filePath.c_str());
    info.description = L"";
    info.hotkey = L"";
    info.showInMenu = true;
    info.showInToolbar = false;

    std::wstring wContent = ReadScriptFileToString(filePath);
    if (wContent.empty()) return;

    auto cleanStr = [](const std::wstring& in) -> std::wstring {
        std::wstring s = in;
        while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ' || s.back() == L'\t')) {
            s.pop_back();
        }
        size_t start = 0;
        while (start < s.length() && (s[start] == L' ' || s[start] == L'\t' || s[start] == L'\r' || s[start] == L'\n')) {
            start++;
        }
        s = s.substr(start);

        // Unescape \uXXXX if present
        std::wstring unescaped;
        for (size_t i = 0; i < s.length(); i++) {
            if (s[i] == L'\\' && i + 5 < s.length() && (s[i + 1] == L'u' || s[i + 1] == L'U')) {
                wchar_t hex[5] = { s[i + 2], s[i + 3], s[i + 4], s[i + 5], 0 };
                wchar_t* endPtr = nullptr;
                long val = wcstol(hex, &endPtr, 16);
                if (endPtr == hex + 4 && val > 0) {
                    unescaped += (wchar_t)val;
                    i += 5;
                    continue;
                }
            }
            unescaped += s[i];
        }
        return unescaped;
    };

    std::wistringstream stream(wContent);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (line.find(L"// ==/NeoScript==") != std::wstring::npos) break;
        if (line.find(L"// @name") != std::wstring::npos) {
            size_t pos = line.find(L"@name") + 5;
            info.name = cleanStr(line.substr(pos));
        } else if (line.find(L"// @description") != std::wstring::npos) {
            size_t pos = line.find(L"@description") + 12;
            info.description = cleanStr(line.substr(pos));
        } else if (line.find(L"// @hotkey") != std::wstring::npos) {
            size_t pos = line.find(L"@hotkey") + 7;
            info.hotkey = cleanStr(line.substr(pos));
        } else if (line.find(L"// @toolbar") != std::wstring::npos) {
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
    bool isZh = I18n::Get().IsChinese();

    // Validate active directory
    std::wstring workingDir = activePath;
    if (workingDir.empty() || workingDir == L"This PC" || workingDir == L"此电脑" || !PathFileExistsW(workingDir.c_str())) {
        MessageBoxW(m_hwndMain,
            isZh ? L"请先在标签页中进入一个具体的本地磁盘或文件夹目录（而不是“此电脑”），再执行该扩展脚本。"
                 : L"Please navigate to a valid local directory before running this script.",
            isZh ? L"脚本执行提示" : L"Script Execution Notice",
            MB_OK | MB_ICONINFORMATION);
        return false;
    }

    Log(L"正在执行脚本: " + filePath);
    Log(L"工作目录: " + workingDir);

    SetEnvironmentVariableW(L"NEOMDIE_ACTIVE_PATH", workingDir.c_str());

    std::wstring filesJoined;
    for (const auto& f : selectedFiles) {
        if (!filesJoined.empty()) filesJoined += L"|";
        filesJoined += f;
    }
    SetEnvironmentVariableW(L"NEOMDIE_SELECTED_FILES", filesJoined.c_str());

    std::wstring scriptToRun = filePath;
    std::wstring tempScriptPath;

    std::wstring scriptContent = ReadScriptFileToString(filePath);
    bool hasNonAscii = false;
    for (wchar_t ch : scriptContent) {
        if (ch > 127) {
            hasNonAscii = true;
            break;
        }
    }

    if (hasNonAscii) {
        wchar_t tempDir[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempDir)) {
            wchar_t tempFile[MAX_PATH];
            if (GetTempFileNameW(tempDir, L"neo", 0, tempFile)) {
                DeleteFileW(tempFile);
                tempScriptPath = std::wstring(tempFile) + L".js";

                HANDLE hOut = CreateFileW(tempScriptPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hOut != INVALID_HANDLE_VALUE) {
                    BYTE bom[2] = { 0xFF, 0xFE };
                    DWORD written = 0;
                    WriteFile(hOut, bom, 2, &written, nullptr);
                    WriteFile(hOut, scriptContent.data(), (DWORD)(scriptContent.size() * sizeof(wchar_t)), &written, nullptr);
                    CloseHandle(hOut);
                    scriptToRun = tempScriptPath;
                }
            }
        }
    }

    std::wstring cmd = L"cscript.exe //NoLogo \"" + scriptToRun + L"\"";

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
                                  CREATE_NO_WINDOW, nullptr, workingDir.c_str(), &si, &pi);

    CloseHandle(hWritePipe);

    std::string output;
    if (success) {
        CloseHandle(pi.hThread);

        char buffer[1024];
        DWORD bytesRead = 0;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);

        if (!tempScriptPath.empty()) {
            DeleteFileW(tempScriptPath.c_str());
        }

        std::wstring wOutput = Util::DecodeOutput(output);

        while (!wOutput.empty() && (wOutput.back() == L'\r' || wOutput.back() == L'\n' || wOutput.back() == L' ' || wOutput.back() == L'\t')) {
            wOutput.pop_back();
        }
        while (!wOutput.empty() && (wOutput.front() == L'\r' || wOutput.front() == L'\n' || wOutput.front() == L' ' || wOutput.front() == L'\t')) {
            wOutput.erase(wOutput.begin());
        }

        if (!wOutput.empty()) {
            Log(wOutput);
            bool isErr = (wOutput.find(L"错误") != std::wstring::npos ||
                          wOutput.find(L"Error") != std::wstring::npos ||
                          wOutput.find(L"error") != std::wstring::npos ||
                          wOutput.find(L"Exception") != std::wstring::npos);
            MessageBoxW(m_hwndMain, wOutput.c_str(), isZh ? L"NeoMDIE 脚本执行结果" : L"Script Execution Result",
                        MB_OK | (isErr ? MB_ICONWARNING : MB_ICONINFORMATION));
        } else {
            Log(L"脚本执行完毕。");
        }
    } else {
        DWORD err = GetLastError();
        std::wstring errText = isZh ? (L"脚本启动失败，错误代码: " + std::to_wstring(err)) : (L"Failed to start script, error code: " + std::to_wstring(err));
        Log(errText);
        MessageBoxW(m_hwndMain, errText.c_str(), isZh ? L"脚本错误" : L"Script Error", MB_OK | MB_ICONERROR);

        if (!tempScriptPath.empty()) {
            DeleteFileW(tempScriptPath.c_str());
        }
    }

    CloseHandle(hReadPipe);
    return success != FALSE;
}

void ScriptEngine::Log(const std::wstring& msg) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[32];
    StringCchPrintfW(timeBuf, 32, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    std::wstring entry = timeBuf + msg;
    m_logs.push_back(entry);

    if (m_hwndConsole && IsWindow(m_hwndConsole)) {
        HWND hEdit = GetDlgItem(m_hwndConsole, 101);
        if (hEdit) {
            std::wstring line = entry + L"\r\n";
            int len = GetWindowTextLengthW(hEdit);
            SendMessageW(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
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

    ShowWindow(m_hwndConsole, SW_SHOW);
    SetForegroundWindow(m_hwndConsole);
}
