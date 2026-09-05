#include "ldde/window/window.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::window {

namespace {

bool is_valid_transition(WindowLifecycleState from, WindowLifecycleState to) {
    if (to == WindowLifecycleState::Failed) return true;
    switch (from) {
        case WindowLifecycleState::Discovered:
            return to == WindowLifecycleState::Initializing || to == WindowLifecycleState::Destroyed;
        case WindowLifecycleState::Initializing:
            return to == WindowLifecycleState::Ready || to == WindowLifecycleState::Destroyed;
        case WindowLifecycleState::Ready:
            return to == WindowLifecycleState::Visible || to == WindowLifecycleState::Closing || to == WindowLifecycleState::Destroyed;
        case WindowLifecycleState::Visible:
            return to == WindowLifecycleState::Ready || to == WindowLifecycleState::Closing || to == WindowLifecycleState::Destroyed;
        case WindowLifecycleState::Closing:
            return to == WindowLifecycleState::Destroyed;
        case WindowLifecycleState::Destroyed:
            return false;
        case WindowLifecycleState::Failed:
            return to == WindowLifecycleState::Destroyed;
    }
    return false;
}

} // namespace

Window::Window(WindowId id,
               wl_surface* surface,
               xdg_surface* xdg_surf,
               xdg_toplevel* toplevel)
    : id_(id),
      surface_(surface),
      xdg_surface_(xdg_surf),
      toplevel_(toplevel),
      creation_time_(std::chrono::steady_clock::now()) {
    LDDE_LOG_DEBUG(Window, "Created window tracking record [id=" << id_ << "]");
}

Window::~Window() {
    mark_destroyed();
}

Status Window::transition_to(WindowLifecycleState next_state) {
    if (lifecycle_state_ == next_state) {
        return Status::ok();
    }

    if (!is_valid_transition(lifecycle_state_, next_state)) {
        LDDE_LOG_WARN(Window, "Invalid window lifecycle transition for window " << id_
                              << ": " << window_lifecycle_name(lifecycle_state_)
                              << " -> " << window_lifecycle_name(next_state));
        return LDDE_STATUS_ERROR(core::ErrorCategory::Application,
                                 core::ErrorCode::InvalidLifecycleTransition,
                                 "Invalid window lifecycle transition");
    }

    LDDE_LOG_DEBUG(Window, "Window " << id_ << " lifecycle: "
                          << window_lifecycle_name(lifecycle_state_)
                          << " -> " << window_lifecycle_name(next_state));
    lifecycle_state_ = next_state;
    return Status::ok();
}

void Window::set_title(std::string_view title) {
    if (title_ != title) {
        title_ = std::string(title);
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " title changed to: \"" << title_ << "\"");
    }
}

void Window::set_app_id(std::string_view app_id) {
    if (app_id_ != app_id) {
        app_id_ = std::string(app_id);
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " app_id changed to: \"" << app_id_ << "\"");
    }
}

void Window::set_geometry(const core::Rect& geom) {
    if (!(geometry_ == geom)) {
        geometry_ = geom;
        LDDE_LOG_TRACE(Window, "Window " << id_ << " geometry changed to: ("
                              << geom.x << ", " << geom.y << ", " << geom.width << "x" << geom.height << ")");
    }
}

void Window::set_surface_size(const core::Size& size) {
    surface_size_ = size;
}

void Window::set_state(WindowState state) {
    if (state_ != state) {
        state_ = state;
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " observed state: " << window_state_name(state_));
    }
}

void Window::set_requested_state(WindowState state) {
    requested_state_ = state;
}

void Window::set_active(bool active) {
    if (is_active_ != active) {
        is_active_ = active;
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " focus changed: " << (active ? "Focused" : "Unfocused"));
    }
}

void Window::set_visible(bool visible) {
    if (is_visible_ != visible) {
        is_visible_ = visible;
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " visibility changed: " << (visible ? "Visible" : "Hidden"));
    }
}

void Window::set_parent_id(std::optional<WindowId> parent_id) {
    parent_id_ = parent_id;
    if (parent_id) {
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " attached to parent window " << *parent_id);
    } else {
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " detached from parent");
    }
}

void Window::request_close() {
    LDDE_LOG_INFO(Window, "Requesting close for window " << id_);
    transition_to(WindowLifecycleState::Closing);
}

void Window::ack_configure(uint32_t serial) {
    last_configure_serial_ = serial;
    if (xdg_surface_) {
        xdg_surface_ack_configure(xdg_surface_, serial);
    }
}

void Window::mark_destroyed() noexcept {
    if (lifecycle_state_ != WindowLifecycleState::Destroyed) {
        lifecycle_state_ = WindowLifecycleState::Destroyed;
        LDDE_LOG_DEBUG(Window, "Window " << id_ << " marked destroyed");
    }
    toplevel_ = nullptr;
    xdg_surface_ = nullptr;
    surface_ = nullptr;
}

} // namespace ldde::window
