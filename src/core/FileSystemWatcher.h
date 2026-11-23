#pragma once
#include "..\Common.h"

class FileSystemWatcher {
public:
    FileSystemWatcher();
    ~FileSystemWatcher();

    void Watch(const std::wstring& directory, HWND notifyWnd, UINT notifyMsg);
    void Stop();
    bool IsWatching() const { return m_running.load(); }

private:
    static void WatchWorker(std::wstring currentDir, HWND notifyWnd, UINT notifyMsg, HANDLE hStopEvent);

    std::wstring m_currentDir;
    HWND m_notifyWnd = nullptr;
    std::atomic<bool> m_running = false;
    std::thread m_thread;
    HANDLE m_hStopEvent = nullptr;
};
