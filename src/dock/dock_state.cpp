#include "ldde/dock/dock_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::dock {

std::string_view dock_state_name(DockState state) noexcept {
    switch (state) {
        case DockState::Hidden:   return "Hidden";
        case DockState::Showing:  return "Showing";
        case DockState::Visible:  return "Visible";
        case DockState::Hiding:   return "Hiding";
    }
    return "Unknown";
}

core::Status DockStateMachine::transition_to(DockState next_state) {
    if (state_ == next_state) {
        return core::Status::ok();
    }

    DockState old_state = state_;
    state_ = next_state;

    LDDE_LOG_DEBUG(Dock, "Dock state changed from " << dock_state_name(old_state)
                         << " to " << dock_state_name(next_state));

    if (on_state_changed_) {
        on_state_changed_(old_state, next_state);
    }

    return core::Status::ok();
}

core::Status DockStateMachine::request_show() {
    if (state_ == DockState::Visible || state_ == DockState::Showing) {
        return core::Status::ok();
    }
    transition_to(DockState::Showing);
    return transition_to(DockState::Visible);
}

core::Status DockStateMachine::finish_show() {
    if (state_ == DockState::Showing) {
        return transition_to(DockState::Visible);
    }
    return core::Status::ok();
}

core::Status DockStateMachine::request_hide() {
    if (state_ == DockState::Hidden || state_ == DockState::Hiding) {
        return core::Status::ok();
    }
    transition_to(DockState::Hiding);
    return transition_to(DockState::Hidden);
}

core::Status DockStateMachine::finish_hide() {
    if (state_ == DockState::Hiding) {
        return transition_to(DockState::Hidden);
    }
    return core::Status::ok();
}

core::Status DockStateMachine::toggle() {
    if (is_visible()) {
        return request_hide();
    } else {
        return request_show();
    }
}

} // namespace ldde::dock
