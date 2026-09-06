#pragma once

#include <string_view>
#include <functional>
#include <vector>
#include <mutex>
#include "ldde/core/error.hpp"

namespace ldde::system {

enum class SystemPanelState {
    Closed,
    Opening,
    Open,
    Closing
};

[[nodiscard]] std::string_view system_panel_state_name(SystemPanelState state) noexcept;

class SystemPanelStateMachine {
public:
    using StateChangedCallback = std::function<void(SystemPanelState old_state, SystemPanelState new_state)>;

    explicit SystemPanelStateMachine(SystemPanelState initial_state = SystemPanelState::Closed);

    [[nodiscard]] SystemPanelState state() const noexcept { return state_; }
    [[nodiscard]] bool is_closed() const noexcept { return state_ == SystemPanelState::Closed; }
    [[nodiscard]] bool is_open() const noexcept { return state_ == SystemPanelState::Open; }
    [[nodiscard]] bool is_animating() const noexcept {
        return state_ == SystemPanelState::Opening || state_ == SystemPanelState::Closing;
    }

    core::Status transition_to(SystemPanelState new_state);
    void on_state_changed(StateChangedCallback callback);

private:
    mutable std::mutex mutex_;
    SystemPanelState state_ = SystemPanelState::Closed;
    std::vector<StateChangedCallback> callbacks_;
};

} // namespace ldde::system

