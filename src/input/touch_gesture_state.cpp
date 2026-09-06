#include "ldde/input/touch_gesture_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::input {

std::string_view gesture_state_name(GestureState state) noexcept {
    switch (state) {
        case GestureState::Idle:             return "Idle";
        case GestureState::ContactPending:   return "ContactPending";
        case GestureState::WindowFocus:      return "WindowFocus";
        case GestureState::Moving:           return "Moving";
        case GestureState::Resizing:         return "Resizing";
        case GestureState::ControlPress:     return "ControlPress";
        case GestureState::GestureCancelled: return "GestureCancelled";
        case GestureState::Completed:        return "Completed";
    }
    return "Unknown";
}

bool is_valid_transition(GestureState from, GestureState to) noexcept {
    if (from == to) {
        return true;
    }

    switch (from) {
        case GestureState::Idle:
            return to == GestureState::ContactPending;

        case GestureState::ContactPending:
            return to == GestureState::WindowFocus ||
                   to == GestureState::Moving ||
                   to == GestureState::Resizing ||
                   to == GestureState::ControlPress ||
                   to == GestureState::Completed ||
                   to == GestureState::GestureCancelled ||
                   to == GestureState::Idle;

        case GestureState::WindowFocus:
            return to == GestureState::Moving ||
                   to == GestureState::Completed ||
                   to == GestureState::GestureCancelled ||
                   to == GestureState::Idle;

        case GestureState::Moving:
            return to == GestureState::Completed ||
                   to == GestureState::GestureCancelled ||
                   to == GestureState::Idle;

        case GestureState::Resizing:
            return to == GestureState::Completed ||
                   to == GestureState::GestureCancelled ||
                   to == GestureState::Idle;

        case GestureState::ControlPress:
            return to == GestureState::Completed ||
                   to == GestureState::GestureCancelled ||
                   to == GestureState::Idle;

        case GestureState::GestureCancelled:
            return to == GestureState::Idle;

        case GestureState::Completed:
            return to == GestureState::Idle;
    }

    return false;
}

bool TouchGestureStateMachine::transition_to(GestureState new_state) {
    if (!is_valid_transition(state_, new_state)) {
        LDDE_LOG_WARN(Input, "Invalid touch gesture transition: "
                             << gesture_state_name(state_) << " -> "
                             << gesture_state_name(new_state));
        return false;
    }

    GestureState old = state_;
    state_ = new_state;

    if (observer_ && old != new_state) {
        observer_(old, new_state);
    }
    return true;
}

void TouchGestureStateMachine::reset() noexcept {
    GestureState old = state_;
    state_ = GestureState::Idle;
    if (observer_ && old != GestureState::Idle) {
        observer_(old, GestureState::Idle);
    }
}

} // namespace ldde::input
