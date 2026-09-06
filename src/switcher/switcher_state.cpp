#include "ldde/switcher/switcher_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::switcher {

std::string_view switcher_state_name(SwitcherState state) noexcept {
    switch (state) {
        case SwitcherState::Closed:     return "Closed";
        case SwitcherState::Opening:    return "Opening";
        case SwitcherState::Open:       return "Open";
        case SwitcherState::Selecting:  return "Selecting";
        case SwitcherState::Activating: return "Activating";
        case SwitcherState::Closing:    return "Closing";
    }
    return "Unknown";
}

void SwitcherStateMachine::transition_to(SwitcherState new_state) {
    if (state_ == new_state) return;
    SwitcherState old_state = state_;
    state_ = new_state;
    LDDE_LOG_DEBUG(Switcher, "Switcher state changed: " << switcher_state_name(old_state)
                             << " -> " << switcher_state_name(new_state));
    if (state_changed_cb_) {
        state_changed_cb_(old_state, new_state);
    }
}

core::Status SwitcherStateMachine::request_open() {
    if (is_open()) {
        return core::Status::ok();
    }
    transition_to(SwitcherState::Opening);
    transition_to(SwitcherState::Open);
    return core::Status::ok();
}

core::Status SwitcherStateMachine::complete_open() {
    if (state_ == SwitcherState::Opening) {
        transition_to(SwitcherState::Open);
    }
    return core::Status::ok();
}

core::Status SwitcherStateMachine::start_selection() {
    if (state_ == SwitcherState::Open) {
        transition_to(SwitcherState::Selecting);
    }
    return core::Status::ok();
}

core::Status SwitcherStateMachine::request_activate() {
    if (state_ == SwitcherState::Selecting || state_ == SwitcherState::Open) {
        transition_to(SwitcherState::Activating);
    }
    return core::Status::ok();
}

core::Status SwitcherStateMachine::request_close() {
    if (state_ == SwitcherState::Closed) {
        return core::Status::ok();
    }
    transition_to(SwitcherState::Closing);
    transition_to(SwitcherState::Closed);
    return core::Status::ok();
}

core::Status SwitcherStateMachine::cancel() {
    return request_close();
}

void SwitcherStateMachine::force_close() noexcept {
    if (state_ != SwitcherState::Closed) {
        transition_to(SwitcherState::Closed);
    }
}

} // namespace ldde::switcher
