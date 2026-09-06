#include "ldde/notification/notification_presenter.hpp"
#include "ldde/core/logging.hpp"
#include <algorithm>

namespace ldde::notification {

NotificationPresenter::NotificationPresenter(
    NotificationStore& store,
    core::EventLoop* event_loop,
    size_t max_visible_popups,
    uint32_t default_timeout_ms)
    : store_(store),
      event_loop_(event_loop),
      max_visible_popups_(max_visible_popups),
      default_timeout_ms_(default_timeout_ms) {}

NotificationPresenter::~NotificationPresenter() {
    for (const auto& [id, timer_id] : active_timers_) {
        if (event_loop_) {
            event_loop_->cancel_timer(timer_id);
        }
    }
    active_timers_.clear();
}

void NotificationPresenter::show(NotificationId id) {
    auto* notif = store_.find(id);
    if (!notif) return;

    // Check if already in visible popups
    auto it = std::find_if(visible_popups_.begin(), visible_popups_.end(),
                           [id](const Notification* n) { return n && n->id() == id; });

    if (it != visible_popups_.end()) {
        // Refresh timer on update/replaces
        cancel_expiration(id);
        schedule_expiration(id, notif->expire_timeout_ms(), notif->urgency());
        notify_render();
        return;
    }

    // If at capacity, pop oldest non-critical
    if (visible_popups_.size() >= max_visible_popups_) {
        for (auto rit = visible_popups_.rbegin(); rit != visible_popups_.rend(); ++rit) {
            if (*rit && !(*rit)->is_critical()) {
                cancel_expiration((*rit)->id());
                visible_popups_.erase(std::next(rit).base());
                break;
            }
        }
    }

    if (visible_popups_.size() < max_visible_popups_) {
        visible_popups_.insert(visible_popups_.begin(), notif);
        store_.mark_displayed(id);
        schedule_expiration(id, notif->expire_timeout_ms(), notif->urgency());
    }

    notify_render();
}

void NotificationPresenter::dismiss(NotificationId id) {
    cancel_expiration(id);

    visible_popups_.erase(
        std::remove_if(visible_popups_.begin(), visible_popups_.end(),
                       [id](const Notification* n) { return n && n->id() == id; }),
        visible_popups_.end());

    store_.dismiss(id);
    refresh_visible();
    notify_render();
}

void NotificationPresenter::expire(NotificationId id) {
    cancel_expiration(id);

    visible_popups_.erase(
        std::remove_if(visible_popups_.begin(), visible_popups_.end(),
                       [id](const Notification* n) { return n && n->id() == id; }),
        visible_popups_.end());

    store_.expire(id);
    refresh_visible();
    notify_render();
}

void NotificationPresenter::refresh_visible() {
    // Fill any available slots from active notifications in store
    if (visible_popups_.size() < max_visible_popups_) {
        auto active = store_.active_notifications();
        for (const auto* n : active) {
            if (visible_popups_.size() >= max_visible_popups_) break;
            if (!n) continue;

            bool already = std::any_of(visible_popups_.begin(), visible_popups_.end(),
                                       [n](const Notification* item) { return item && item->id() == n->id(); });
            if (!already) {
                auto* mut = store_.find(n->id());
                if (mut) {
                    visible_popups_.push_back(mut);
                    store_.mark_displayed(n->id());
                    schedule_expiration(n->id(), mut->expire_timeout_ms(), mut->urgency());
                }
            }
        }
    }
}

void NotificationPresenter::schedule_expiration(
    NotificationId id,
    int32_t timeout_ms,
    NotificationUrgency urgency) {
    if (!event_loop_) return;
    if (urgency == NotificationUrgency::Critical || timeout_ms == 0) {
        // Persistent - do not auto-expire
        return;
    }

    uint32_t duration = default_timeout_ms_;
    if (timeout_ms > 0) {
        duration = static_cast<uint32_t>(timeout_ms);
    } else if (urgency == NotificationUrgency::Low) {
        duration = std::min(default_timeout_ms_, 3000u);
    }

    core::TimerId tid = event_loop_->add_timer(
        std::chrono::milliseconds(duration),
        false, // non-recurring
        [this, id]() {
            expire(id);
        });

    active_timers_[id] = tid;
}

void NotificationPresenter::cancel_expiration(NotificationId id) {
    auto it = active_timers_.find(id);
    if (it != active_timers_.end()) {
        if (event_loop_) {
            event_loop_->cancel_timer(it->second);
        }
        active_timers_.erase(it);
    }
}

void NotificationPresenter::notify_render() {
    for (const auto& cb : render_callbacks_) {
        if (cb) cb();
    }
}

} // namespace ldde::notification

