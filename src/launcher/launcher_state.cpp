#include "ldde/launcher/launcher_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::launcher {

std::string_view launcher_state_name(LauncherState state) noexcept {
    switch (state) {
        case LauncherState::Closed:       return "Closed";
        case LauncherState::Opening:      return "Opening";
        case LauncherState::Open:         return "Open";
        case LauncherState::Searching:    return "Searching";
        case LauncherState::Launching:    return "Launching";
        case LauncherState::LaunchFailed: return "LaunchFailed";
        case LauncherState::Closing:      return "Closing";
    }
    return "Unknown";
}

core::Status LauncherStateMachine::transition_to(LauncherState next_state) {
    if (state_ == next_state) {
        return core::Status::ok();
    }

    // Validate transitions
    bool valid = false;
    switch (state_) {
        case LauncherState::Closed:
            valid = (next_state == LauncherState::Opening || next_state == LauncherState::Open);
            break;
        case LauncherState::Opening:
            valid = (next_state == LauncherState::Open || next_state == LauncherState::Closing || next_state == LauncherState::Closed);
            break;
        case LauncherState::Open:
            valid = (next_state == LauncherState::Searching || next_state == LauncherState::Launching ||
                     next_state == LauncherState::Closing || next_state == LauncherState::Closed);
            break;
        case LauncherState::Searching:
            valid = (next_state == LauncherState::Open || next_state == LauncherState::Launching ||
                     next_state == LauncherState::Closing || next_state == LauncherState::Closed);
            break;
        case LauncherState::Launching:
            valid = (next_state == LauncherState::Open || next_state == LauncherState::LaunchFailed ||
                     next_state == LauncherState::Closing || next_state == LauncherState::Closed);
            break;
        case LauncherState::LaunchFailed:
            valid = (next_state == LauncherState::Open || next_state == LauncherState::Searching ||
                     next_state == LauncherState::Launching || next_state == LauncherState::Closing ||
                     next_state == LauncherState::Closed);
            break;
        case LauncherState::Closing:
            valid = (next_state == LauncherState::Closed || next_state == LauncherState::Opening || next_state == LauncherState::Open);
            break;
    }

    if (!valid) {
        LDDE_LOG_WARN(Launcher, "Invalid launcher state transition from "
                                << launcher_state_name(state_) << " to "
                                << launcher_state_name(next_state));
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::LauncherInvalidState,
                                 "Invalid launcher state transition");
    }

    LauncherState old = state_;
    state_ = next_state;
    LDDE_LOG_DEBUG(Launcher, "Launcher state changed from "
                             << launcher_state_name(old) << " to "
                             << launcher_state_name(state_));

    if (on_state_changed_) {
        on_state_changed_(old, state_);
    }

    return core::Status::ok();
}

core::Status LauncherStateMachine::request_open() {
    if (state_ == LauncherState::Open || state_ == LauncherState::Opening) {
        return core::Status::ok(); // Idempotent
    }
    return transition_to(LauncherState::Opening);
}

core::Status LauncherStateMachine::finish_open() {
    if (state_ == LauncherState::Open) {
        return core::Status::ok();
    }
    return transition_to(LauncherState::Open);
}

core::Status LauncherStateMachine::start_searching() {
    if (state_ == LauncherState::Searching) {
        return core::Status::ok();
    }
    if (state_ != LauncherState::Open && state_ != LauncherState::LaunchFailed) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::LauncherNotOpen,
                                 "Cannot start searching when launcher is not open");
    }
    return transition_to(LauncherState::Searching);
}

core::Status LauncherStateMachine::stop_searching() {
    if (state_ == LauncherState::Open) {
        return core::Status::ok();
    }
    if (state_ == LauncherState::Searching) {
        return transition_to(LauncherState::Open);
    }
    return core::Status::ok();
}

core::Status LauncherStateMachine::request_launch() {
    if (!is_open()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::LauncherNotOpen,
                                 "Cannot launch when launcher is not open");
    }
    return transition_to(LauncherState::Launching);
}

core::Status LauncherStateMachine::finish_launch() {
    if (state_ == LauncherState::Launching) {
        return transition_to(LauncherState::Closing);
    }
    return core::Status::ok();
}

core::Status LauncherStateMachine::fail_launch() {
    if (state_ == LauncherState::Launching) {
        return transition_to(LauncherState::LaunchFailed);
    }
    return core::Status::ok();
}

core::Status LauncherStateMachine::request_close() {
    if (state_ == LauncherState::Closed || state_ == LauncherState::Closing) {
        return core::Status::ok(); // Idempotent
    }
    return transition_to(LauncherState::Closing);
}

core::Status LauncherStateMachine::finish_close() {
    if (state_ == LauncherState::Closed) {
        return core::Status::ok();
    }
    return transition_to(LauncherState::Closed);
}

core::Status LauncherStateMachine::toggle() {
    if (is_open() || state_ == LauncherState::Opening) {
        core::Status s = request_close();
        if (s.is_ok()) {
            return finish_close();
        }
        return s;
    } else {
        core::Status s = request_open();
        if (s.is_ok()) {
            return finish_open();
        }
        return s;
    }
}

} // namespace ldde::launcher

