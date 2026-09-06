#include "ldde/notification/notification_center_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::notification {

NotificationCenterStateMachine::NotificationCenterStateMachine(NotificationCenterState initial_state)
    : current_state_(initial_state) {}

bool NotificationCenterStateMachine::can_transition_to(NotificationCenterState target) const noexcept {
    if (current_state_ == target) return false;

    switch (current_state_) {
        case NotificationCenterState::Closed:
            return target == NotificationCenterState::Opening || target == NotificationCenterState::Open;
        case NotificationCenterState::Opening:
            return target == NotificationCenterState::Open || target == NotificationCenterState::Closing || target == NotificationCenterState::Closed;
        case NotificationCenterState::Open:
            return target == NotificationCenterState::Closing || target == NotificationCenterState::Closed;
        case NotificationCenterState::Closing:
            return target == NotificationCenterState::Closed || target == NotificationCenterState::Opening || target == NotificationCenterState::Open;
    }
    return false;
}

bool NotificationCenterStateMachine::transition_to(NotificationCenterState target) {
    if (current_state_ == target) return true;
    if (!can_transition_to(target)) {
        LDDE_LOG_WARN(Notification, "Invalid notification center state transition from "
                      << notification_center_state_name(current_state_)
                      << " to " << notification_center_state_name(target));
        return false;
    }

    auto old = current_state_;
    current_state_ = target;
    LDDE_LOG_DEBUG(Notification, "Notification center state: "
                   << notification_center_state_name(old) << " -> "
                   << notification_center_state_name(target));

    for (const auto& cb : callbacks_) {
        if (cb) cb(old, target);
    }
    return true;
}

void NotificationCenterStateMachine::on_state_changed(StateChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::notification

