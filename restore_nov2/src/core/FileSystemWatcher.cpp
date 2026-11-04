#include "FileSystemWatcher.h"

FileSystemWatcher::FileSystemWatcher() {
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

FileSystemWatcher::~FileSystemWatcher() {
    Stop();
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
    }
}

void FileSystemWatcher::Watch(const std::wstring& directory, HWND notifyWnd, UINT notifyMsg) {
    Stop();

    if (directory.empty() || directory == L"此电脑" || directory == L"This PC") {
        return;
    }

    m_currentDir = directory;
    m_notifyWnd = notifyWnd;
    m_notifyMsg = notifyMsg;
    m_running = true;
    ResetEvent(m_hStopEvent);

    m_thread = std::thread(&FileSystemWatcher::WatchThreadFunc, this);
}

void FileSystemWatcher::Stop() {
    if (m_running) {
        m_running = false;
        SetEvent(m_hStopEvent);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

void FileSystemWatcher::WatchThreadFunc() {
    HANDLE hDir = CreateFileW(m_currentDir.c_str(), FILE_LIST_DIRECTORY,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (hDir == INVALID_HANDLE_VALUE) {
        m_running = false;
        return;
    }

    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    BYTE buffer[4096];
    DWORD bytesReturned = 0;

    while (m_running) {
        ResetEvent(overlapped.hEvent);
        BOOL success = ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            &bytesReturned, &overlapped, nullptr
        );

        if (!success) break;

        HANDLE handles[2] = { m_hStopEvent, overlapped.hEvent };
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Stop event signaled
            CancelIo(hDir);
            break;
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            // Directory changed! Post message with debouncing
            if (m_notifyWnd && IsWindow(m_notifyWnd)) {
                PostMessageW(m_notifyWnd, m_notifyMsg, 0, 0);
            }
            // Sleep briefly to debounce rapid bursts of changes
            Sleep(250);
        }
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
}
