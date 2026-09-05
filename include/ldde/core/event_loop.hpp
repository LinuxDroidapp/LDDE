#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <mutex>
#include "ldde/core/error.hpp"

namespace ldde::core {

enum class FdEvent : uint32_t {
    Readable = 0x01,
    Writable = 0x02,
    Error    = 0x04,
    Hangup   = 0x08
};

inline constexpr FdEvent operator|(FdEvent a, FdEvent b) noexcept {
    return static_cast<FdEvent>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool operator&(FdEvent a, FdEvent b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

using FdCallback = std::function<void(int fd, FdEvent events)>;
using TimerCallback = std::function<void()>;
using SignalCallback = std::function<void(int signum)>;
using PostCallback = std::function<void()>;
using TimerId = uint64_t;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    Status initialize();

    Status add_fd(int fd, FdEvent events, FdCallback callback);
    Status modify_fd(int fd, FdEvent events);
    Status remove_fd(int fd);

    TimerId add_timer(std::chrono::milliseconds interval, bool recurring, TimerCallback callback);
    void cancel_timer(TimerId id);

    Status register_signals(const std::vector<int>& signals, SignalCallback callback);

    void post(PostCallback task);
    void wakeup();

    Status dispatch(int timeout_ms = -1);
    Status run();
    void stop();

    [[nodiscard]] bool is_running() const noexcept { return is_running_; }

private:
    int epoll_fd_ = -1;
    int wakeup_fd_ = -1;
    int signal_fd_ = -1;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};

    struct FdWatch {
        int fd;
        FdEvent events;
        FdCallback callback;
    };
    std::unordered_map<int, FdWatch> fd_watches_;

    struct TimerEntry {
        TimerId id;
        int fd;
        bool recurring;
        TimerCallback callback;
    };
    std::unordered_map<TimerId, TimerEntry> timers_;
    std::unordered_map<int, TimerId> timer_fd_to_id_;
    TimerId next_timer_id_ = 1;

    SignalCallback signal_callback_;

    std::mutex tasks_mutex_;
    std::vector<PostCallback> pending_tasks_;

    void process_pending_tasks();
    void handle_wakeup();
    void handle_signals();
    void handle_timer(int tfd);
};

} // namespace ldde::core

