#pragma once

#include <string_view>
#include <functional>
#include "ldde/core/error.hpp"

namespace ldde::launcher {

enum class LauncherState {
    Closed = 0,
    Opening,
    Open,
    Searching,
    Launching,
    LaunchFailed,
    Closing
};

[[nodiscard]] std::string_view launcher_state_name(LauncherState state) noexcept;

class LauncherStateMachine {
public:
    using StateChangedCallback = std::function<void(LauncherState old_state, LauncherState new_state)>;

    LauncherStateMachine() = default;

    [[nodiscard]] LauncherState state() const noexcept { return state_; }
    [[nodiscard]] bool is_closed() const noexcept { return state_ == LauncherState::Closed; }
    [[nodiscard]] bool is_open() const noexcept {
        return state_ == LauncherState::Open ||
               state_ == LauncherState::Searching ||
               state_ == LauncherState::Launching ||
               state_ == LauncherState::LaunchFailed;
    }
    [[nodiscard]] bool is_active() const noexcept {
        return state_ != LauncherState::Closed;
    }

    core::Status request_open();
    core::Status finish_open();
    core::Status start_searching();
    core::Status stop_searching();
    core::Status request_launch();
    core::Status finish_launch();
    core::Status fail_launch();
    core::Status request_close();
    core::Status finish_close();
    core::Status toggle();

    void on_state_changed(StateChangedCallback cb) { on_state_changed_ = std::move(cb); }

private:
    LauncherState state_ = LauncherState::Closed;
    StateChangedCallback on_state_changed_;

    core::Status transition_to(LauncherState next_state);
};

} // namespace ldde::launcher

