#include "FileWatcher.h"
#include <Windows.h>

namespace sv {

FileWatcher::~FileWatcher() {
    Stop();
}

void FileWatcher::Start(const std::filesystem::path& directory, Callback callback) {
    Stop();
    directory_ = directory;
    callback_ = std::move(callback);
    running_ = true;
    thread_ = std::thread(&FileWatcher::WatchLoop, this);
}

void FileWatcher::Stop() {
    running_ = false;
    if (handle_) {
        CancelIoEx(static_cast<HANDLE>(handle_), nullptr);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    if (handle_) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
}

bool FileWatcher::ConsumePending() {
    return pending_.exchange(false);
}

void FileWatcher::WatchLoop() {
    const HANDLE dir = CreateFileW(
        directory_.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (dir == INVALID_HANDLE_VALUE) {
        return;
    }
    handle_ = dir;

    char buffer[8192];
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        CloseHandle(dir);
        handle_ = nullptr;
        return;
    }

    while (running_) {
        ResetEvent(ov.hEvent);
        DWORD bytesReturned = 0;
        const BOOL started = ReadDirectoryChangesW(
            dir, buffer, sizeof(buffer), TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE,
            &bytesReturned, &ov, nullptr);
        if (!started) {
            const DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                break;
            }
        }

        const DWORD wait = WaitForSingleObject(ov.hEvent, 200);
        if (wait == WAIT_OBJECT_0) {
            DWORD transferred = 0;
            if (GetOverlappedResult(dir, &ov, &transferred, FALSE)) {
                pending_ = true;
                if (callback_) {
                    callback_();
                }
            }
        }
    }

    CloseHandle(ov.hEvent);
}

} // namespace sv
