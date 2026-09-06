#include "ldde/system/session_status.hpp"

namespace ldde::system {

std::string_view session_state_name(SessionState state) noexcept {
    switch (state) {
        case SessionState::Active:      return "Active";
        case SessionState::Idle:        return "Idle";
        case SessionState::Locked:      return "Locked";
        case SessionState::Terminating: return "Terminating";
    }
    return "Unknown";
}

DesktopSessionStatusProvider::DesktopSessionStatusProvider(std::string compositor_name,
                                                           std::string desktop_identity)
    : compositor_name_(std::move(compositor_name)),
      desktop_identity_(std::move(desktop_identity)) {}

SessionInfo DesktopSessionStatusProvider::query_session() {
    SessionInfo info;
    info.state = state_;
    info.compositor_name = compositor_name_;
    info.desktop_identity = desktop_identity_;
    info.wayland_connected = wayland_connected_;
    info.status_label = std::string(session_state_name(state_)) + " (" + compositor_name_ + ")";
    return info;
}

void DesktopSessionStatusProvider::set_state(SessionState state) {
    state_ = state;
}

void DesktopSessionStatusProvider::set_wayland_connected(bool connected) {
    wayland_connected_ = connected;
}

SessionStatus::SessionStatus(std::shared_ptr<SessionStatusProvider> provider)
    : provider_(std::move(provider)) {
    if (!provider_) {
        provider_ = std::make_shared<DesktopSessionStatusProvider>();
    }
    update();
}

void SessionStatus::set_provider(std::shared_ptr<SessionStatusProvider> provider) {
    provider_ = std::move(provider);
    if (!provider_) {
        provider_ = std::make_shared<DesktopSessionStatusProvider>();
    }
    update();
}

bool SessionStatus::update() {
    if (!provider_) return false;
    SessionInfo new_info = provider_->query_session();

    bool changed = (new_info.state != current_info_.state ||
                    new_info.compositor_name != current_info_.compositor_name ||
                    new_info.wayland_connected != current_info_.wayland_connected);

    current_info_ = std::move(new_info);

    if (changed) {
        for (const auto& cb : callbacks_) {
            if (cb) cb(current_info_);
        }
    }
    return changed;
}

void SessionStatus::on_changed(SessionChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system
