#pragma once

#include <string_view>
#include <functional>

namespace ldde::input {

enum class GestureState {
    Idle,
    ContactPending,
    WindowFocus,
    Moving,
    Resizing,
    ControlPress,
    GestureCancelled,
    Completed
};

[[nodiscard]] std::string_view gesture_state_name(GestureState state) noexcept;
[[nodiscard]] bool is_valid_transition(GestureState from, GestureState to) noexcept;

class TouchGestureStateMachine {
public:
    using TransitionObserver = std::function<void(GestureState from, GestureState to)>;

    TouchGestureStateMachine() = default;
    explicit TouchGestureStateMachine(TransitionObserver observer)
        : observer_(std::move(observer)) {}

    [[nodiscard]] GestureState current_state() const noexcept { return state_; }
    [[nodiscard]] bool is_idle() const noexcept { return state_ == GestureState::Idle; }
    [[nodiscard]] bool is_active() const noexcept { return state_ != GestureState::Idle; }

    bool transition_to(GestureState new_state);
    void reset() noexcept;

    void set_observer(TransitionObserver observer) { observer_ = std::move(observer); }

private:
    GestureState state_ = GestureState::Idle;
    TransitionObserver observer_;
};

} // namespace ldde::input
