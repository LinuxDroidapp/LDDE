#include "ldde/window/window_focus.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::window {

WindowFocus::WindowFocus(WindowRegistry& registry, WindowStacking& stacking, WindowManagementBackend& backend)
    : registry_(registry), stacking_(stacking), backend_(backend) {}

Status WindowFocus::activate(WindowId id) {
    if (active_id_.has_value() && active_id_.value() == id) {
        stacking_.raise(id);
        return Status::ok();
    }

    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for activation");
    }

    if (win->state() == WindowState::Minimized) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::InvalidWindowState, "Cannot activate minimized window");
    }

    // Deactivate previous window if active
    if (active_id_.has_value()) {
        auto prev = registry_.lookup(active_id_.value());
        if (prev) {
            prev->set_active(false);
        }
        static_cast<void>(backend_.deactivate(active_id_.value()));
    }

    active_id_ = id;
    win->set_active(true);
    registry_.set_active_window(id);
    stacking_.raise(id);

    auto status = backend_.activate(id);
    if (!status.is_ok()) {
        LDDE_LOG_WARN(Window, "Backend activate failed for window " << id);
    }

    return status;
}

Status WindowFocus::deactivate() {
    if (!active_id_.has_value()) {
        return Status::ok();
    }

    WindowId prev_id = active_id_.value();
    auto prev = registry_.lookup(prev_id);
    if (prev) {
        prev->set_active(false);
    }
    active_id_ = std::nullopt;
    registry_.set_active_window(std::nullopt);

    return backend_.deactivate(prev_id);
}

void WindowFocus::handle_window_removed_or_hidden(WindowId id) {
    if (active_id_.has_value() && active_id_.value() == id) {
        active_id_ = std::nullopt;

        // Fallback to topmost visible window in the stack
        const auto& visible = stacking_.visible_stack(registry_);
        for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
            if (*it != id) {
                static_cast<void>(activate(*it));
                return;
            }
        }
    }
}

} // namespace ldde::window
