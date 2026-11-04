#pragma once
#include "..\Common.h"

struct FolderSizeResult {
    std::wstring folderPath;
    uint64_t totalSize;
};

class FolderSizeCalculator {
public:
    FolderSizeCalculator();
    ~FolderSizeCalculator();

    void RequestFolderSize(const std::wstring& folderPath, HWND notifyWnd, UINT notifyMsg);
    void CancelAll();

private:
    void WorkerLoop();
    uint64_t CalculateRecursive(const std::wstring& path, std::atomic<bool>& cancelToken);

    std::vector<std::wstring> m_queue;
    std::mutex m_queueLock;
    std::condition_variable m_cv;

    HWND m_notifyWnd = nullptr;
    UINT m_notifyMsg = 0;

    std::atomic<bool> m_running = false;
    std::atomic<bool> m_cancelCurrent = false;
    std::thread m_workerThread;
};
