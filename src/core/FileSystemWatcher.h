#pragma once
#include "..\Common.h"

class FileSystemWatcher {
public:
    FileSystemWatcher();
    ~FileSystemWatcher();

    void Watch(const std::wstring& directory, HWND notifyWnd, UINT notifyMsg);
    void Stop();

private:
    static void WatchWorker(std::wstring currentDir, HWND notifyWnd, UINT notifyMsg, HANDLE hStopEvent);

    std::atomic<bool> m_running = false;
    std::thread m_thread;
    HANDLE m_hStopEvent = nullptr;
};
