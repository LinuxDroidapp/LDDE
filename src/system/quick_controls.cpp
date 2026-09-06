#include "ldde/system/quick_controls.hpp"
#include <algorithm>

namespace ldde::system {

std::string_view control_capability_name(ControlCapability cap) noexcept {
    switch (cap) {
        case ControlCapability::Available:   return "Available";
        case ControlCapability::Unavailable: return "Unavailable";
        case ControlCapability::Unsupported: return "Unsupported";
        case ControlCapability::Error:       return "Error";
    }
    return "Unknown";
}

std::string_view control_type_name(ControlType type) noexcept {
    switch (type) {
        case ControlType::AudioMute:     return "Audio Mute";
        case ControlType::NetworkToggle: return "Network";
        case ControlType::DisplayInfo:   return "Display";
        case ControlType::SessionAction: return "Session";
        case ControlType::Notifications: return "Notifications";
    }
    return "Unknown";
}

QuickControlsManager::QuickControlsManager(SystemDataProvider& data_provider)
    : data_provider_(data_provider) {
    refresh_controls();

    data_provider_.on_changed([this]() {
        refresh_controls();
    });
}

void QuickControlsManager::refresh_controls() {
    controls_.clear();

    // 1. Audio Mute control
    {
        const auto& audio_info = data_provider_.audio().info();
        QuickControl ctrl;
        ctrl.type = ControlType::AudioMute;
        ctrl.id = "audio_mute";
        ctrl.label = "Audio";
        ctrl.capability = audio_info.is_available ? ControlCapability::Available : ControlCapability::Unavailable;
        ctrl.is_active = audio_info.is_muted;
        ctrl.status_text = audio_info.is_available ? (audio_info.is_muted ? "Muted" : std::to_string(audio_info.volume_percent) + "%") : "Unavailable";
        ctrl.action = [this]() {
            data_provider_.audio().toggle_mute();
        };
        controls_.push_back(std::move(ctrl));
    }

    // 2. Network Toggle control
    {
        const auto& net_info = data_provider_.network().info();
        QuickControl ctrl;
        ctrl.type = ControlType::NetworkToggle;
        ctrl.id = "network_toggle";
        ctrl.label = "Network";
        ctrl.capability = (net_info.state != NetworkState::Unavailable) ? ControlCapability::Available : ControlCapability::Unavailable;
        ctrl.is_active = net_info.is_connected && net_info.is_enabled;
        ctrl.status_text = net_info.status_label;
        ctrl.action = [this]() {
            data_provider_.network().toggle_enabled();
        };
        controls_.push_back(std::move(ctrl));
    }

    // 3. Display Info control
    {
        const auto& disp_info = data_provider_.display().info();
        QuickControl ctrl;
        ctrl.type = ControlType::DisplayInfo;
        ctrl.id = "display_info";
        ctrl.label = "Display";
        ctrl.capability = ControlCapability::Available;
        ctrl.is_active = false;
        ctrl.status_text = disp_info.orientation_name;
        ctrl.action = []() {
            // Display status presentation
        };
        controls_.push_back(std::move(ctrl));
    }

    // 4. Session Action control
    {
        const auto& sess_info = data_provider_.session().info();
        QuickControl ctrl;
        ctrl.type = ControlType::SessionAction;
        ctrl.id = "session_action";
        ctrl.label = "Session";
        ctrl.capability = ControlCapability::Available;
        ctrl.is_active = (sess_info.state == SessionState::Active);
        ctrl.status_text = sess_info.status_label;
        ctrl.action = [this]() {
            if (data_provider_.session().info().state == SessionState::Active) {
                // Session toggle state demo
            }
        };
        controls_.push_back(std::move(ctrl));
    }

    // 5. Notifications control
    {
        QuickControl ctrl;
        ctrl.type = ControlType::Notifications;
        ctrl.id = "notifications";
        ctrl.label = "Notifications";
        ctrl.capability = ControlCapability::Available;
        ctrl.is_active = false;
        ctrl.status_text = "Center";
        ctrl.action = [this]() {
            if (open_notifications_callback_) {
                open_notifications_callback_();
            }
        };
        controls_.push_back(std::move(ctrl));
    }

    if (selected_index_ >= static_cast<int32_t>(controls_.size())) {
        selected_index_ = controls_.empty() ? 0 : static_cast<int32_t>(controls_.size() - 1);
    }

    notify_changed();
}

const QuickControl* QuickControlsManager::control_at(size_t index) const noexcept {
    if (index < controls_.size()) {
        return &controls_[index];
    }
    return nullptr;
}

QuickControl* QuickControlsManager::control_at(size_t index) noexcept {
    if (index < controls_.size()) {
        return &controls_[index];
    }
    return nullptr;
}

int32_t QuickControlsManager::hit_test(int32_t x, int32_t y) const noexcept {
    for (size_t i = 0; i < controls_.size(); ++i) {
        if (controls_[i].geometry.contains(core::Point{x, y})) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

void QuickControlsManager::select_next() {
    if (controls_.empty()) return;
    selected_index_ = (selected_index_ + 1) % static_cast<int32_t>(controls_.size());
    notify_changed();
}

void QuickControlsManager::select_prev() {
    if (controls_.empty()) return;
    selected_index_ = (selected_index_ - 1 + static_cast<int32_t>(controls_.size())) % static_cast<int32_t>(controls_.size());
    notify_changed();
}

void QuickControlsManager::set_selected_index(int32_t index) {
    if (index >= 0 && index < static_cast<int32_t>(controls_.size())) {
        selected_index_ = index;
        notify_changed();
    }
}

bool QuickControlsManager::activate_selected() {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int32_t>(controls_.size())) {
        return activate_index(static_cast<size_t>(selected_index_));
    }
    return false;
}

bool QuickControlsManager::activate_index(size_t index) {
    if (index < controls_.size()) {
        auto& ctrl = controls_[index];
        if (ctrl.capability == ControlCapability::Available && ctrl.action) {
            ctrl.action();
            refresh_controls();
            return true;
        }
    }
    return false;
}

void QuickControlsManager::set_control_geometry(size_t index, const core::Rect& geom) {
    if (index < controls_.size()) {
        controls_[index].geometry = geom;
    }
}

void QuickControlsManager::on_changed(ControlsChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

void QuickControlsManager::notify_changed() {
    for (const auto& cb : callbacks_) {
        if (cb) cb();
    }
}

} // namespace ldde::system

