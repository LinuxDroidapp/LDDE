#include "ldde/desktop/desktop_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::desktop {

std::string_view desktop_state_name(DesktopState state) noexcept {
    switch (state) {
        case DesktopState::Initializing: return "Initializing";
        case DesktopState::Ready:        return "Ready";
        case DesktopState::Active:       return "Active";
        case DesktopState::Suspended:    return "Suspended";
        case DesktopState::Stopping:     return "Stopping";
        case DesktopState::Stopped:      return "Stopped";
        case DesktopState::Failed:       return "Failed";
    }
    return "Unknown";
}

DesktopStateMachine::DesktopStateMachine() = default;

DesktopState DesktopStateMachine::state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool DesktopStateMachine::is_ready() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == DesktopState::Ready;
}

bool DesktopStateMachine::is_active() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == DesktopState::Active;
}

bool DesktopStateMachine::is_suspended() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == DesktopState::Suspended;
}

bool DesktopStateMachine::is_stopped() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == DesktopState::Stopped;
}

bool DesktopStateMachine::is_valid_transition(DesktopState from, DesktopState to) const noexcept {
    if (from == to) return true;
    if (to == DesktopState::Failed) return true;

    switch (from) {
        case DesktopState::Initializing:
            return to == DesktopState::Ready || to == DesktopState::Stopping;
        case DesktopState::Ready:
            return to == DesktopState::Active || to == DesktopState::Stopping;
        case DesktopState::Active:
            return to == DesktopState::Suspended || to == DesktopState::Stopping;
        case DesktopState::Suspended:
            return to == DesktopState::Active || to == DesktopState::Stopping;
        case DesktopState::Stopping:
            return to == DesktopState::Stopped;
        case DesktopState::Stopped:
            return to == DesktopState::Initializing;
        case DesktopState::Failed:
            return to == DesktopState::Stopping || to == DesktopState::Stopped;
    }
    return false;
}

core::Status DesktopStateMachine::transition_to(DesktopState target_state) {
    StateChangeCallback cb;
    DesktopState old_state;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == target_state) {
            return core::Status::ok();
        }

        if (!is_valid_transition(state_, target_state)) {
            LDDE_LOG_WARN(Desktop, "Invalid desktop state transition from "
                                   << desktop_state_name(state_) << " to "
                                   << desktop_state_name(target_state));
            return core::Status::error(core::ErrorCategory::Desktop,
                                       core::ErrorCode::DesktopInvalidState,
                                       "Invalid desktop state transition");
        }

        old_state = state_;
        state_ = target_state;
        cb = on_state_changed_;
    }

    LDDE_LOG_DEBUG(Desktop, "Desktop state changed: "
                            << desktop_state_name(old_state) << " -> "
                            << desktop_state_name(target_state));

    if (cb) {
        cb(old_state, target_state);
    }

    return core::Status::ok();
}

void DesktopStateMachine::on_state_changed(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_state_changed_ = std::move(callback);
}

} // namespace ldde::desktop
