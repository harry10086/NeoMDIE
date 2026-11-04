#pragma once
#include "..\Common.h"

class FileSystemWatcher {
public:
    FileSystemWatcher();
    ~FileSystemWatcher();

    void Watch(const std::wstring& directory, HWND notifyWnd, UINT notifyMsg);
    void Stop();

private:
    void WatchThreadFunc();

    std::wstring m_currentDir;
    HWND m_notifyWnd = nullptr;
    UINT m_notifyMsg = 0;
    std::atomic<bool> m_running = false;
    std::thread m_thread;
    HANDLE m_hStopEvent = nullptr;
};
