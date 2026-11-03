#include "FolderSizeCalculator.h"

FolderSizeCalculator::FolderSizeCalculator() {
    m_running = true;
    m_workerThread = std::thread(&FolderSizeCalculator::WorkerLoop, this);
}

FolderSizeCalculator::~FolderSizeCalculator() {
    CancelAll();
    m_running = false;
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void FolderSizeCalculator::CancelAll() {
    m_cancelCurrent = true;
    std::lock_guard<std::mutex> lock(m_queueLock);
    m_queue.clear();
}

void FolderSizeCalculator::RequestFolderSize(const std::wstring& folderPath, HWND notifyWnd, UINT notifyMsg) {
    m_notifyWnd = notifyWnd;
    m_notifyMsg = notifyMsg;

    std::lock_guard<std::mutex> lock(m_queueLock);
    m_queue.push_back(folderPath);
    m_cv.notify_one();
}

uint64_t FolderSizeCalculator::CalculateRecursive(const std::wstring& path, std::atomic<bool>& cancelToken) {
    if (cancelToken || !m_running) return 0;

    uint64_t total = 0;
    std::wstring searchPath = Util::NormalizePath(path) + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoBasic, &fd,
                                    FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (hFind == INVALID_HANDLE_VALUE) return 0;

    std::vector<std::wstring> subdirs;

    do {
        if (cancelToken || !m_running) break;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                subdirs.push_back(Util::NormalizePath(path) + L"\\" + fd.cFileName);
            }
        } else {
            ULARGE_INTEGER uli;
            uli.LowPart = fd.nFileSizeLow;
            uli.HighPart = fd.nFileSizeHigh;
            total += uli.QuadPart;
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    for (const auto& sub : subdirs) {
        if (cancelToken || !m_running) break;
        total += CalculateRecursive(sub, cancelToken);
    }

    return total;
}

void FolderSizeCalculator::WorkerLoop() {
    while (m_running) {
        std::wstring targetPath;
        {
            std::unique_lock<std::mutex> lock(m_queueLock);
            m_cv.wait(lock, [this]() { return !m_running || !m_queue.empty(); });
            if (!m_running) break;

            targetPath = m_queue.front();
            m_queue.erase(m_queue.begin());
            m_cancelCurrent = false;
        }

        uint64_t size = CalculateRecursive(targetPath, m_cancelCurrent);
        if (!m_cancelCurrent && m_running && m_notifyWnd && IsWindow(m_notifyWnd)) {
            FolderSizeResult* pRes = new FolderSizeResult{ targetPath, size };
            PostMessageW(m_notifyWnd, m_notifyMsg, 0, (LPARAM)pRes);
        }
    }
}
