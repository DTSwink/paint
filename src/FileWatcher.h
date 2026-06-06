#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace sv {

class FileWatcher {
public:
    using Callback = std::function<void()>;

    ~FileWatcher();
    void Start(const std::filesystem::path& directory, Callback callback);
    void Stop();
    bool ConsumePending();

private:
    void WatchLoop();

    std::filesystem::path directory_;
    Callback callback_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> pending_{false};
    void* handle_ = nullptr;
};

} // namespace sv
