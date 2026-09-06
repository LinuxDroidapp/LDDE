#pragma once

#include "ldde/core/error.hpp"
#include <string_view>
#include <functional>
#include <mutex>

namespace ldde::desktop {

enum class DesktopState {
    Initializing,
    Ready,
    Active,
    Suspended,
    Stopping,
    Stopped,
    Failed
};

[[nodiscard]] std::string_view desktop_state_name(DesktopState state) noexcept;

class DesktopStateMachine {
public:
    using StateChangeCallback = std::function<void(DesktopState old_state, DesktopState new_state)>;

    DesktopStateMachine();
    ~DesktopStateMachine() = default;

    [[nodiscard]] DesktopState state() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] bool is_active() const noexcept;
    [[nodiscard]] bool is_suspended() const noexcept;
    [[nodiscard]] bool is_stopped() const noexcept;

    core::Status transition_to(DesktopState target_state);

    void on_state_changed(StateChangeCallback callback);

private:
    mutable std::mutex mutex_;
    DesktopState state_ = DesktopState::Initializing;
    StateChangeCallback on_state_changed_;

    [[nodiscard]] bool is_valid_transition(DesktopState from, DesktopState to) const noexcept;
};

} // namespace ldde::desktop
