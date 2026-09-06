#pragma once

#include <string_view>
#include <functional>
#include "ldde/core/error.hpp"

namespace ldde::switcher {

enum class SwitcherState {
    Closed = 0,
    Opening,
    Open,
    Selecting,
    Activating,
    Closing
};

[[nodiscard]] std::string_view switcher_state_name(SwitcherState state) noexcept;

class SwitcherStateMachine {
public:
    using StateChangedCallback = std::function<void(SwitcherState old_state, SwitcherState new_state)>;

    SwitcherStateMachine() = default;
    ~SwitcherStateMachine() = default;

    [[nodiscard]] SwitcherState state() const noexcept { return state_; }
    [[nodiscard]] bool is_open() const noexcept {
        return state_ == SwitcherState::Opening ||
               state_ == SwitcherState::Open ||
               state_ == SwitcherState::Selecting ||
               state_ == SwitcherState::Activating;
    }
    [[nodiscard]] bool is_closed() const noexcept {
        return state_ == SwitcherState::Closed;
    }

    core::Status request_open();
    core::Status complete_open();
    core::Status start_selection();
    core::Status request_activate();
    core::Status request_close();
    core::Status cancel();
    void force_close() noexcept;

    void on_state_changed(StateChangedCallback cb) {
        state_changed_cb_ = std::move(cb);
    }

private:
    SwitcherState state_ = SwitcherState::Closed;
    StateChangedCallback state_changed_cb_;

    void transition_to(SwitcherState new_state);
};

} // namespace ldde::switcher
