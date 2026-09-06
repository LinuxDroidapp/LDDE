#include "ldde/system/system_ui_state.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::system {

std::string_view system_panel_state_name(SystemPanelState state) noexcept {
    switch (state) {
        case SystemPanelState::Closed:  return "Closed";
        case SystemPanelState::Opening: return "Opening";
        case SystemPanelState::Open:    return "Open";
        case SystemPanelState::Closing: return "Closing";
    }
    return "Unknown";
}

SystemPanelStateMachine::SystemPanelStateMachine(SystemPanelState initial_state)
    : state_(initial_state) {}

core::Status SystemPanelStateMachine::transition_to(SystemPanelState new_state) {
    std::vector<StateChangedCallback> callbacks_copy;
    SystemPanelState old_state;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == new_state) {
            return core::Status::ok();
        }

        bool valid = false;
        switch (state_) {
            case SystemPanelState::Closed:
                valid = (new_state == SystemPanelState::Opening || new_state == SystemPanelState::Open);
                break;
            case SystemPanelState::Opening:
                valid = (new_state == SystemPanelState::Open || new_state == SystemPanelState::Closed);
                break;
            case SystemPanelState::Open:
                valid = (new_state == SystemPanelState::Closing || new_state == SystemPanelState::Closed);
                break;
            case SystemPanelState::Closing:
                valid = (new_state == SystemPanelState::Closed || new_state == SystemPanelState::Open);
                break;
        }

        if (!valid) {
            LDDE_LOG_WARN(System, "Invalid SystemPanel transition: " << system_panel_state_name(state_)
                                 << " -> " << system_panel_state_name(new_state));
            return core::Status::error(core::ErrorCategory::System,
                                       core::ErrorCode::SystemPanelInvalidState,
                                       "Invalid SystemPanel state transition");
        }

        old_state = state_;
        state_ = new_state;
        callbacks_copy = callbacks_;
    }

    LDDE_LOG_DEBUG(System, "SystemPanel state changed: " << system_panel_state_name(old_state)
                           << " -> " << system_panel_state_name(new_state));

    for (const auto& cb : callbacks_copy) {
        if (cb) cb(old_state, new_state);
    }

    return core::Status::ok();
}

void SystemPanelStateMachine::on_state_changed(StateChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system
