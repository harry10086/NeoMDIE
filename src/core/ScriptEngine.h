#pragma once
#include "..\Common.h"

struct ScriptInfo {
    std::wstring filePath;
    std::wstring name;
    std::wstring description;
    std::wstring hotkey;
    bool showInMenu = true;
    bool showInToolbar = false;
    UINT commandId = 0;
};

class ScriptEngine {
public:
    static ScriptEngine& Get();

    void Initialize(HWND hwndMain);
    void ReloadScripts();
    const std::vector<ScriptInfo>& GetScripts() const { return m_scripts; }
    std::wstring GetScriptsDir() const { return Util::GetExecutableDir() + L"\\scripts"; }

    bool ExecuteScript(const std::wstring& filePath, const std::wstring& activePath, const std::vector<std::wstring>& selectedFiles);
    bool ExecuteCommand(UINT cmdId, const std::wstring& activePath, const std::vector<std::wstring>& selectedFiles);

    void ShowConsole();
    void Log(const std::wstring& msg);

private:
    ScriptEngine();
    ~ScriptEngine();

    void ParseScriptHeader(const std::wstring& filePath, ScriptInfo& info);

    HWND m_hwndMain = nullptr;
    HWND m_hwndConsole = nullptr;
    std::vector<ScriptInfo> m_scripts;
    std::vector<std::wstring> m_logs;
};
