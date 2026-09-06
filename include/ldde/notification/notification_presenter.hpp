#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "ldde/notification/notification.hpp"
#include "ldde/notification/notification_store.hpp"
#include "ldde/core/event_loop.hpp"

namespace ldde::notification {

class NotificationPresenter {
public:
    using RequestRenderCallback = std::function<void()>;

    explicit NotificationPresenter(
        NotificationStore& store,
        core::EventLoop* event_loop = nullptr,
        size_t max_visible_popups = 3,
        uint32_t default_timeout_ms = 5000);

    ~NotificationPresenter();

    void set_event_loop(core::EventLoop* loop) noexcept { event_loop_ = loop; }
    void set_max_visible_popups(size_t max) noexcept { max_visible_popups_ = max; }
    [[nodiscard]] size_t max_visible_popups() const noexcept { return max_visible_popups_; }

    void set_default_timeout_ms(uint32_t ms) noexcept { default_timeout_ms_ = ms; }
    [[nodiscard]] uint32_t default_timeout_ms() const noexcept { return default_timeout_ms_; }

    // Popup lifecycle
    void show(NotificationId id);
    void dismiss(NotificationId id);
    void expire(NotificationId id);
    void refresh_visible();

    [[nodiscard]] bool has_visible_popups() const noexcept { return !visible_popups_.empty(); }
    [[nodiscard]] const std::vector<Notification*>& visible_popups() const noexcept { return visible_popups_; }
    [[nodiscard]] std::vector<Notification*>& visible_popups() noexcept { return visible_popups_; }

    void on_request_render(RequestRenderCallback cb) { render_callbacks_.push_back(std::move(cb)); }

private:
    void schedule_expiration(NotificationId id, int32_t timeout_ms, NotificationUrgency urgency);
    void cancel_expiration(NotificationId id);
    void notify_render();

    NotificationStore& store_;
    core::EventLoop* event_loop_;
    size_t max_visible_popups_;
    uint32_t default_timeout_ms_;

    std::vector<Notification*> visible_popups_;
    std::unordered_map<NotificationId, core::TimerId> active_timers_;
    std::vector<RequestRenderCallback> render_callbacks_;
};

} // namespace ldde::notification

