#pragma once

#include <string_view>
#include <functional>
#include "ldde/core/error.hpp"

namespace ldde::dock {

enum class DockState {
    Hidden = 0,
    Showing,
    Visible,
    Hiding
};

[[nodiscard]] std::string_view dock_state_name(DockState state) noexcept;

class DockStateMachine {
public:
    using StateChangedCallback = std::function<void(DockState old_state, DockState new_state)>;

    DockStateMachine() = default;

    [[nodiscard]] DockState state() const noexcept { return state_; }
    [[nodiscard]] bool is_hidden() const noexcept { return state_ == DockState::Hidden; }
    [[nodiscard]] bool is_visible() const noexcept { return state_ == DockState::Visible || state_ == DockState::Showing; }

    core::Status request_show();
    core::Status finish_show();
    core::Status request_hide();
    core::Status finish_hide();
    core::Status toggle();

    void on_state_changed(StateChangedCallback cb) { on_state_changed_ = std::move(cb); }

private:
    DockState state_ = DockState::Visible; // default to Visible
    StateChangedCallback on_state_changed_;

    core::Status transition_to(DockState next_state);
};

} // namespace ldde::dock
