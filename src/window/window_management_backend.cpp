#include "ldde/window/window_management_backend.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/core/logging.hpp"
#include "ldde/wayland/xdg-shell-client-protocol.h"

namespace ldde::window {

DefaultWindowManagementBackend::DefaultWindowManagementBackend(WindowTracker& tracker, WindowRegistry& registry)
    : tracker_(tracker), registry_(registry) {}

Status DefaultWindowManagementBackend::activate(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for activation");
    }

    std::vector<uint32_t> states;
    states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    if (win->state() == WindowState::Maximized) {
        states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    } else if (win->state() == WindowState::Fullscreen) {
        states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    }

    tracker_.send_configure_to_client(id, win->geometry().width, win->geometry().height, states);

    win->set_active(true);
    registry_.set_active_window(id);
    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::FocusChanged,
        .window_id = id,
        .window = win,
        .property_name = "focused"
    });

    LDDE_LOG_DEBUG(Window, "Window " << id << " activated via backend");
    return Status::ok();
}

Status DefaultWindowManagementBackend::deactivate(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for deactivation");
    }

    std::vector<uint32_t> states;
    if (win->state() == WindowState::Maximized) {
        states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    } else if (win->state() == WindowState::Fullscreen) {
        states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    }

    tracker_.send_configure_to_client(id, win->geometry().width, win->geometry().height, states);

    win->set_active(false);
    if (registry_.active_window_id() == id) {
        registry_.set_active_window(std::nullopt);
    }

    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::FocusChanged,
        .window_id = id,
        .window = win,
        .property_name = "unfocused"
    });

    LDDE_LOG_DEBUG(Window, "Window " << id << " deactivated via backend");
    return Status::ok();
}

Status DefaultWindowManagementBackend::close(WindowId id) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for close");
    }

    tracker_.send_close_to_client(id);
    LDDE_LOG_INFO(Window, "Close request sent to window " << id);
    return Status::ok();
}

Status DefaultWindowManagementBackend::set_geometry(WindowId id, const core::Rect& geom) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for set_geometry");
    }

    std::vector<uint32_t> states;
    if (win->is_active()) {
        states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    }
    if (win->state() == WindowState::Maximized) {
        states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    } else if (win->state() == WindowState::Fullscreen) {
        states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    }

    tracker_.send_configure_to_client(id, geom.width, geom.height, states);

    win->set_geometry(geom);
    win->set_surface_size(core::Size{geom.width, geom.height});

    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::GeometryChanged,
        .window_id = id,
        .window = win,
        .property_name = "geometry"
    });

    return Status::ok();
}

Status DefaultWindowManagementBackend::set_maximized(WindowId id, bool maximized, const core::Size& target_size) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for set_maximized");
    }

    std::vector<uint32_t> states;
    if (win->is_active()) {
        states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    }
    if (maximized) {
        states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    }

    tracker_.send_configure_to_client(id, target_size.width, target_size.height, states);

    win->set_state(maximized ? WindowState::Maximized : WindowState::Normal);
    core::Rect new_geom = win->geometry();
    new_geom.width = target_size.width;
    new_geom.height = target_size.height;
    win->set_geometry(new_geom);
    win->set_surface_size(target_size);

    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::StateChanged,
        .window_id = id,
        .window = win,
        .property_name = "state"
    });

    return Status::ok();
}

Status DefaultWindowManagementBackend::set_fullscreen(WindowId id, bool fullscreen, const core::Size& target_size) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for set_fullscreen");
    }

    std::vector<uint32_t> states;
    if (win->is_active()) {
        states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    }
    if (fullscreen) {
        states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    }

    tracker_.send_configure_to_client(id, target_size.width, target_size.height, states);

    win->set_state(fullscreen ? WindowState::Fullscreen : WindowState::Normal);
    core::Rect new_geom = win->geometry();
    new_geom.width = target_size.width;
    new_geom.height = target_size.height;
    win->set_geometry(new_geom);
    win->set_surface_size(target_size);

    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::StateChanged,
        .window_id = id,
        .window = win,
        .property_name = "state"
    });

    return Status::ok();
}

Status DefaultWindowManagementBackend::set_minimized(WindowId id, bool minimized) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for set_minimized");
    }

    win->set_state(minimized ? WindowState::Minimized : WindowState::Normal);
    win->set_visible(!minimized);

    if (minimized && win->is_active()) {
        win->set_active(false);
        if (registry_.active_window_id() == id) {
            registry_.set_active_window(std::nullopt);
        }
    }

    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::StateChanged,
        .window_id = id,
        .window = win,
        .property_name = "state"
    });
    registry_.dispatch_event(WindowEvent{
        .type = WindowEventType::VisibilityChanged,
        .window_id = id,
        .window = win,
        .property_name = "visibility"
    });

    LDDE_LOG_DEBUG(Window, "Window " << id << " minimized=" << (minimized ? "true" : "false") << " via backend");
    return Status::ok();
}

Status DefaultWindowManagementBackend::start_move(WindowId id, uint32_t serial) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for start_move");
    }
    LDDE_LOG_DEBUG(Window, "Start move requested for window " << id << " with serial " << serial);
    return Status::ok();
}

Status DefaultWindowManagementBackend::start_resize(WindowId id, ResizeEdge edge, uint32_t serial) {
    auto win = registry_.lookup(id);
    if (!win) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Window not found for start_resize");
    }
    LDDE_LOG_DEBUG(Window, "Start resize requested for window " << id << " (edge: " << resize_edge_name(edge) << ") with serial " << serial);
    return Status::ok();
}

} // namespace ldde::window
