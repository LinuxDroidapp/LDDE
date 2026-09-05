#include "ldde/core/event_loop.hpp"
#include "ldde/core/logging.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <csignal>

namespace ldde::core {

namespace {

uint32_t to_epoll_events(FdEvent events) noexcept {
    uint32_t ep = 0;
    if (events & FdEvent::Readable) ep |= EPOLLIN;
    if (events & FdEvent::Writable) ep |= EPOLLOUT;
    if (events & FdEvent::Error)    ep |= EPOLLERR;
    if (events & FdEvent::Hangup)   ep |= EPOLLHUP;
    return ep;
}

FdEvent from_epoll_events(uint32_t ep) noexcept {
    FdEvent events = static_cast<FdEvent>(0);
    if (ep & EPOLLIN)  events = events | FdEvent::Readable;
    if (ep & EPOLLOUT) events = events | FdEvent::Writable;
    if (ep & EPOLLERR) events = events | FdEvent::Error;
    if (ep & EPOLLHUP) events = events | FdEvent::Hangup;
    return events;
}

} // namespace

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
    stop();

    if (signal_fd_ >= 0) {
        close(signal_fd_);
        signal_fd_ = -1;
    }

    for (const auto& [id, entry] : timers_) {
        if (entry.fd >= 0) {
            close(entry.fd);
        }
    }
    timers_.clear();
    timer_fd_to_id_.clear();

    if (wakeup_fd_ >= 0) {
        close(wakeup_fd_);
        wakeup_fd_ = -1;
    }

    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

Status EventLoop::initialize() {
    if (epoll_fd_ >= 0) {
        return Status::ok();
    }

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("epoll_create1 failed: ") + std::strerror(errno));
    }

    wakeup_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeup_fd_ < 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("eventfd failed: ") + std::strerror(errno));
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = wakeup_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) < 0) {
        close(wakeup_fd_);
        wakeup_fd_ = -1;
        close(epoll_fd_);
        epoll_fd_ = -1;
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("epoll_ctl add wakeup_fd failed: ") + std::strerror(errno));
    }

    return Status::ok();
}

Status EventLoop::add_fd(int fd, FdEvent events, FdCallback callback) {
    if (epoll_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Internal, ErrorCode::InvalidLifecycleTransition,
                                 "EventLoop not initialized");
    }

    epoll_event ev{};
    ev.events = to_epoll_events(events);
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("epoll_ctl ADD failed for fd ") +
                                 std::to_string(fd) + ": " + std::strerror(errno));
    }

    fd_watches_[fd] = FdWatch{fd, events, std::move(callback)};
    return Status::ok();
}

Status EventLoop::modify_fd(int fd, FdEvent events) {
    if (epoll_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Internal, ErrorCode::InvalidLifecycleTransition,
                                 "EventLoop not initialized");
    }

    auto it = fd_watches_.find(fd);
    if (it == fd_watches_.end()) {
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::DeviceNotFound,
                                 std::string("fd not watched: ") + std::to_string(fd));
    }

    epoll_event ev{};
    ev.events = to_epoll_events(events);
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("epoll_ctl MOD failed for fd ") +
                                 std::to_string(fd) + ": " + std::strerror(errno));
    }

    it->second.events = events;
    return Status::ok();
}

Status EventLoop::remove_fd(int fd) {
    if (epoll_fd_ < 0) {
        return Status::ok();
    }

    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    fd_watches_.erase(fd);
    return Status::ok();
}

TimerId EventLoop::add_timer(std::chrono::milliseconds interval, bool recurring, TimerCallback callback) {
    if (epoll_fd_ < 0) {
        return 0;
    }

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        LDDE_LOG_ERROR(Core, "timerfd_create failed: " << std::strerror(errno));
        return 0;
    }

    itimerspec its{};
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(interval);
    auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(interval - sec);

    its.it_value.tv_sec = sec.count();
    its.it_value.tv_nsec = nsec.count();

    if (recurring) {
        its.it_interval = its.it_value;
    }

    if (timerfd_settime(tfd, 0, &its, nullptr) < 0) {
        LDDE_LOG_ERROR(Core, "timerfd_settime failed: " << std::strerror(errno));
        close(tfd);
        return 0;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tfd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tfd, &ev) < 0) {
        LDDE_LOG_ERROR(Core, "epoll_ctl ADD timer fd failed: " << std::strerror(errno));
        close(tfd);
        return 0;
    }

    TimerId id = next_timer_id_++;
    timers_[id] = TimerEntry{id, tfd, recurring, std::move(callback)};
    timer_fd_to_id_[tfd] = id;
    return id;
}

void EventLoop::cancel_timer(TimerId id) {
    auto it = timers_.find(id);
    if (it == timers_.end()) {
        return;
    }

    int tfd = it->second.fd;
    if (epoll_fd_ >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, tfd, nullptr);
    }
    close(tfd);
    timer_fd_to_id_.erase(tfd);
    timers_.erase(it);
}

Status EventLoop::register_signals(const std::vector<int>& signals, SignalCallback callback) {
    if (epoll_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Internal, ErrorCode::InvalidLifecycleTransition,
                                 "EventLoop not initialized");
    }

    sigset_t mask;
    sigemptyset(&mask);
    for (int sig : signals) {
        sigaddset(&mask, sig);
    }

    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("pthread_sigmask failed: ") + std::strerror(errno));
    }

    if (signal_fd_ >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, signal_fd_, nullptr);
        close(signal_fd_);
        signal_fd_ = -1;
    }

    signal_fd_ = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("signalfd failed: ") + std::strerror(errno));
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = signal_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, signal_fd_, &ev) < 0) {
        close(signal_fd_);
        signal_fd_ = -1;
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("epoll_ctl add signal_fd failed: ") + std::strerror(errno));
    }

    signal_callback_ = std::move(callback);
    return Status::ok();
}

void EventLoop::post(PostCallback task) {
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        pending_tasks_.push_back(std::move(task));
    }
    wakeup();
}

void EventLoop::wakeup() {
    if (wakeup_fd_ >= 0) {
        uint64_t val = 1;
        write(wakeup_fd_, &val, sizeof(val));
    }
}

void EventLoop::process_pending_tasks() {
    std::vector<PostCallback> tasks;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks.swap(pending_tasks_);
    }

    for (const auto& task : tasks) {
        if (task) {
            task();
        }
    }
}

void EventLoop::handle_wakeup() {
    uint64_t val = 0;
    read(wakeup_fd_, &val, sizeof(val));
    process_pending_tasks();
}

void EventLoop::handle_signals() {
    signalfd_siginfo sinfo{};
    while (read(signal_fd_, &sinfo, sizeof(sinfo)) > 0) {
        if (signal_callback_) {
            signal_callback_(static_cast<int>(sinfo.ssi_signo));
        }
    }
}

void EventLoop::handle_timer(int tfd) {
    uint64_t expirations = 0;
    read(tfd, &expirations, sizeof(expirations));

    auto it_id = timer_fd_to_id_.find(tfd);
    if (it_id == timer_fd_to_id_.end()) {
        return;
    }

    TimerId id = it_id->second;
    auto it_entry = timers_.find(id);
    if (it_entry == timers_.end()) {
        return;
    }

    TimerCallback cb = it_entry->second.callback;
    bool recurring = it_entry->second.recurring;

    if (!recurring) {
        cancel_timer(id);
    }

    if (cb) {
        cb();
    }
}

Status EventLoop::dispatch(int timeout_ms) {
    if (epoll_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Internal, ErrorCode::InvalidLifecycleTransition,
                                 "EventLoop not initialized");
    }

    constexpr int kMaxEvents = 32;
    epoll_event events[kMaxEvents];

    int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) {
            return Status::ok();
        }
        return LDDE_STATUS_ERROR(ErrorCategory::Resource, ErrorCode::IoError,
                                 std::string("epoll_wait failed: ") + std::strerror(errno));
    }

    for (int i = 0; i < nfds; ++i) {
        int fd = events[i].data.fd;

        if (fd == wakeup_fd_) {
            handle_wakeup();
        } else if (fd == signal_fd_) {
            handle_signals();
        } else if (timer_fd_to_id_.contains(fd)) {
            handle_timer(fd);
        } else {
            auto it = fd_watches_.find(fd);
            if (it != fd_watches_.end() && it->second.callback) {
                FdEvent ev = from_epoll_events(events[i].events);
                it->second.callback(fd, ev);
            }
        }
    }

    process_pending_tasks();
    return Status::ok();
}

Status EventLoop::run() {
    if (epoll_fd_ < 0) {
        return LDDE_STATUS_ERROR(ErrorCategory::Internal, ErrorCode::InvalidLifecycleTransition,
                                 "EventLoop not initialized");
    }

    is_running_ = true;
    should_stop_ = false;

    while (!should_stop_) {
        Status s = dispatch(100);
        if (s.is_error()) {
            is_running_ = false;
            return s;
        }
    }

    is_running_ = false;
    return Status::ok();
}

void EventLoop::stop() {
    should_stop_ = true;
    wakeup();
}

} // namespace ldde::core

