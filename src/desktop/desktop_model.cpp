#include "ldde/desktop/desktop_model.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::desktop {

DesktopModel::DesktopModel(window::WindowRegistry& registry)
    : registry_(registry) {

    registry_listener_id_ = registry_.add_listener([this](const window::WindowEvent& ev) {
        if (ev.type == window::WindowEventType::Created ||
            ev.type == window::WindowEventType::Closed ||
            ev.type == window::WindowEventType::Destroyed ||
            ev.type == window::WindowEventType::VisibilityChanged ||
            ev.type == window::WindowEventType::FocusChanged ||
            ev.type == window::WindowEventType::StateChanged) {
            refresh();
        }
    });

    evaluate_state();
}

DesktopModel::~DesktopModel() {
    if (registry_listener_id_ != 0) {
        registry_.remove_listener(registry_listener_id_);
        registry_listener_id_ = 0;
    }
}

void DesktopModel::refresh() {
    evaluate_state();
}

void DesktopModel::evaluate_state() {
    size_t count = 0;
    for (const auto& win : registry_.windows()) {
        if (!win) continue;
        auto lc = win->lifecycle_state();
        if (lc != window::WindowLifecycleState::Destroyed &&
            lc != window::WindowLifecycleState::Closing &&
            lc != window::WindowLifecycleState::Failed) {
            ++count;
        }
    }

    bool focused = false;
    if (count == 0) {
        focused = true;
    } else {
        auto active_id = registry_.active_window_id();
        if (!active_id.has_value()) {
            focused = true;
        } else {
            auto win = registry_.lookup(*active_id);
            if (!win || win->lifecycle_state() == window::WindowLifecycleState::Destroyed ||
                win->lifecycle_state() == window::WindowLifecycleState::Closing ||
                win->lifecycle_state() == window::WindowLifecycleState::Failed) {
                focused = true;
            }
        }
    }

    bool changed = (count != active_window_count_ || focused != is_desktop_focused_);

    active_window_count_ = count;
    is_desktop_focused_ = focused;

    if (changed) {
        LDDE_LOG_DEBUG(Desktop, "DesktopModel updated: active_windows=" << active_window_count_
                               << ", desktop_focused=" << (is_desktop_focused_ ? "true" : "false"));
        notify_changed();
    }
}

void DesktopModel::on_model_changed(ModelChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

void DesktopModel::notify_changed() {
    for (const auto& cb : callbacks_) {
        if (cb) {
            cb();
        }
    }
}

} // namespace ldde::desktop
