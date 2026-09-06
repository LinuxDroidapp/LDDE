#pragma once

#include <string_view>
#include <functional>
#include <vector>

namespace ldde::notification {

enum class NotificationCenterState {
    Closed,
    Opening,
    Open,
    Closing
};

[[nodiscard]] constexpr std::string_view notification_center_state_name(NotificationCenterState state) noexcept {
    switch (state) {
        case NotificationCenterState::Closed:  return "Closed";
        case NotificationCenterState::Opening: return "Opening";
        case NotificationCenterState::Open:    return "Open";
        case NotificationCenterState::Closing: return "Closing";
    }
    return "Unknown";
}

class NotificationCenterStateMachine {
public:
    using StateChangedCallback = std::function<void(NotificationCenterState old_state, NotificationCenterState new_state)>;

    explicit NotificationCenterStateMachine(NotificationCenterState initial_state = NotificationCenterState::Closed);

    [[nodiscard]] NotificationCenterState current_state() const noexcept { return current_state_; }
    [[nodiscard]] bool is_open() const noexcept { return current_state_ == NotificationCenterState::Open; }
    [[nodiscard]] bool is_closed() const noexcept { return current_state_ == NotificationCenterState::Closed; }

    bool can_transition_to(NotificationCenterState target) const noexcept;
    bool transition_to(NotificationCenterState target);

    void on_state_changed(StateChangedCallback callback);

private:
    NotificationCenterState current_state_;
    std::vector<StateChangedCallback> callbacks_;
};

} // namespace ldde::notification

