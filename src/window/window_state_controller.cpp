#include "ldde/window/window_state_controller.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::window {

WindowStateController::WindowStateController(WindowManagementBackend& backend, const WindowPlacement& placement)
    : backend_(backend), placement_(placement) {}

Status WindowStateController::maximize(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window in maximize");
    }

    if (window->state() == WindowState::Maximized) {
        return Status::ok();
    }

    if (window->state() == WindowState::Normal) {
        window->set_saved_geometry(window->geometry());
    }

    core::Rect usable = placement_.get_usable_area(display);
    core::Size target_size{usable.width, usable.height};

    auto status = backend_.set_maximized(window->id(), true, target_size);
    if (status.is_ok()) {
        core::Rect max_geom{usable.x, usable.y, usable.width, usable.height};
        window->set_geometry(max_geom);
        window->set_state(WindowState::Maximized);
        LDDE_LOG_INFO(Window, "Window " << window->id() << " maximized to " << usable.width << "x" << usable.height);
    }
    return status;
}

Status WindowStateController::restore(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window in restore");
    }

    if (window->state() == WindowState::Fullscreen) {
        return restore_fullscreen(window, display);
    }

    if (window->state() == WindowState::Minimized) {
        return restore_minimized(window);
    }

    if (window->state() != WindowState::Maximized) {
        return Status::ok();
    }

    core::Rect usable = placement_.get_usable_area(display);
    core::Rect restored_geom = window->saved_geometry().value_or(
        placement_.calculate_initial_geometry(display, 0, {0, 0}, window->min_size(), window->max_size())
    );

    restored_geom = placement_.clamp_to_usable(restored_geom, usable);
    core::Size target_size{restored_geom.width, restored_geom.height};

    auto status = backend_.set_maximized(window->id(), false, target_size);
    if (status.is_ok()) {
        static_cast<void>(backend_.set_geometry(window->id(), restored_geom));
        window->set_geometry(restored_geom);
        window->set_state(WindowState::Normal);
        window->set_saved_geometry(std::nullopt);
        LDDE_LOG_INFO(Window, "Window " << window->id() << " restored to " << restored_geom.width << "x"
                              << restored_geom.height << " at (" << restored_geom.x << "," << restored_geom.y << ")");
    }
    return status;
}

Status WindowStateController::minimize(const std::shared_ptr<Window>& window) {
    if (!window) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window in minimize");
    }

    if (window->state() == WindowState::Minimized) {
        return Status::ok();
    }

    auto status = backend_.set_minimized(window->id(), true);
    if (status.is_ok()) {
        window->set_state(WindowState::Minimized);
        window->set_visible(false);
        LDDE_LOG_INFO(Window, "Window " << window->id() << " logically minimized");
    }
    return status;
}

Status WindowStateController::restore_minimized(const std::shared_ptr<Window>& window) {
    if (!window) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window in restore_minimized");
    }

    if (window->state() != WindowState::Minimized) {
        return Status::ok();
    }

    auto status = backend_.set_minimized(window->id(), false);
    if (status.is_ok()) {
        window->set_state(WindowState::Normal);
        window->set_visible(true);
        LDDE_LOG_INFO(Window, "Window " << window->id() << " restored from minimized");
    }
    return status;
}

Status WindowStateController::fullscreen(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window in fullscreen");
    }

    if (window->state() == WindowState::Fullscreen) {
        return Status::ok();
    }

    if (window->state() == WindowState::Normal) {
        window->set_saved_geometry(window->geometry());
    }

    int32_t screen_w = display.width;
    int32_t screen_h = display.height;
    core::Size screen_size{screen_w, screen_h};

    auto status = backend_.set_fullscreen(window->id(), true, screen_size);
    if (status.is_ok()) {
        window->set_geometry(core::Rect{0, 0, screen_w, screen_h});
        window->set_state(WindowState::Fullscreen);
        LDDE_LOG_INFO(Window, "Window " << window->id() << " entered fullscreen " << screen_w << "x" << screen_h);
    }
    return status;
}

Status WindowStateController::restore_fullscreen(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) {
        return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window in restore_fullscreen");
    }

    if (window->state() != WindowState::Fullscreen) {
        return Status::ok();
    }

    core::Rect usable = placement_.get_usable_area(display);
    core::Rect restored_geom = window->saved_geometry().value_or(
        placement_.calculate_initial_geometry(display, 0, {0, 0}, window->min_size(), window->max_size())
    );

    restored_geom = placement_.clamp_to_usable(restored_geom, usable);
    core::Size target_size{restored_geom.width, restored_geom.height};

    auto status = backend_.set_fullscreen(window->id(), false, target_size);
    if (status.is_ok()) {
        static_cast<void>(backend_.set_geometry(window->id(), restored_geom));
        window->set_geometry(restored_geom);
        window->set_state(WindowState::Normal);
        window->set_saved_geometry(std::nullopt);
        LDDE_LOG_INFO(Window, "Window " << window->id() << " exited fullscreen to " << target_size.width << "x" << target_size.height);
    }
    return status;
}

Status WindowStateController::toggle_maximize(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window");
    if (window->state() == WindowState::Maximized) {
        return restore(window, display);
    }
    return maximize(window, display);
}

Status WindowStateController::toggle_fullscreen(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) return Status::error(core::ErrorCategory::Window, core::ErrorCode::WindowNotFound, "Null window");
    if (window->state() == WindowState::Fullscreen) {
        return restore_fullscreen(window, display);
    }
    return fullscreen(window, display);
}

void WindowStateController::adapt_to_display_change(const std::shared_ptr<Window>& window, const display::DisplayInfo& display) {
    if (!window) return;

    core::Rect usable = placement_.get_usable_area(display);

    if (window->state() == WindowState::Maximized) {
        core::Size new_size{usable.width, usable.height};
        static_cast<void>(backend_.set_maximized(window->id(), true, new_size));
        window->set_geometry(core::Rect{usable.x, usable.y, usable.width, usable.height});
    } else if (window->state() == WindowState::Fullscreen) {
        int32_t sw = display.width;
        int32_t sh = display.height;
        static_cast<void>(backend_.set_fullscreen(window->id(), true, core::Size{sw, sh}));
        window->set_geometry(core::Rect{0, 0, sw, sh});
    } else if (window->state() == WindowState::Normal) {
        core::Rect clamped = placement_.clamp_to_usable(window->geometry(), usable);
        if (clamped != window->geometry()) {
            static_cast<void>(backend_.set_geometry(window->id(), clamped));
        }
    }

    if (window->saved_geometry().has_value()) {
        window->set_saved_geometry(placement_.clamp_to_usable(window->saved_geometry().value(), usable));
    }
}

} // namespace ldde::window
