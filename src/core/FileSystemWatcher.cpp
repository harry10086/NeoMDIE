#include "FileSystemWatcher.h"

FileSystemWatcher::FileSystemWatcher() {
}

FileSystemWatcher::~FileSystemWatcher() {
    Stop();
}

void FileSystemWatcher::Watch(const std::wstring& directory, HWND notifyWnd, UINT notifyMsg) {
    Stop();

    if (directory.empty() || directory == L"此电脑" || directory == L"This PC") {
        return;
    }

    m_running = true;
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE hStop = m_hStopEvent;

    m_thread = std::thread([directory, notifyWnd, notifyMsg, hStop]() {
        WatchWorker(directory, notifyWnd, notifyMsg, hStop);
    });
}

void FileSystemWatcher::Stop() {
    if (m_running) {
        m_running = false;
        if (m_hStopEvent) {
            SetEvent(m_hStopEvent);
            m_hStopEvent = nullptr;
        }
        if (m_thread.joinable()) {
            m_thread.detach();
        }
    }
}

void FileSystemWatcher::WatchWorker(std::wstring currentDir, HWND notifyWnd, UINT notifyMsg, HANDLE hStopEvent) {
    HANDLE hDir = CreateFileW(currentDir.c_str(), FILE_LIST_DIRECTORY,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (hDir == INVALID_HANDLE_VALUE) {
        if (hStopEvent) CloseHandle(hStopEvent);
        return;
    }

    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    BYTE buffer[4096];
    DWORD bytesReturned = 0;

    while (true) {
        ResetEvent(overlapped.hEvent);
        BOOL success = ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            &bytesReturned, &overlapped, nullptr
        );

        if (!success) break;

        HANDLE handles[2] = { hStopEvent, overlapped.hEvent };
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Stop event signaled
            CancelIo(hDir);
            break;
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            // Directory changed! Post message with debouncing
            if (notifyWnd && IsWindow(notifyWnd)) {
                PostMessageW(notifyWnd, notifyMsg, 0, 0);
            }
            // Wait briefly to debounce rapid bursts of changes, wake up immediately if stop event signaled
            if (WaitForSingleObject(hStopEvent, 250) == WAIT_OBJECT_0) {
                CancelIo(hDir);
                break;
            }
        } else {
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
    if (hStopEvent) CloseHandle(hStopEvent);
}
