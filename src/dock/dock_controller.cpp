#include "ldde/dock/dock_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>

namespace ldde::dock {

DockController::DockController(
    DockStateMachine& state_machine,
    DockModel& model,
    DockLayout& layout,
    window::WindowManager& window_manager,
    window::WindowRegistry& window_registry,
    application::ApplicationCatalog& catalog,
    launcher::Launcher& launcher,
    std::shared_ptr<launcher::ApplicationLauncher> app_launcher)
    : state_machine_(state_machine),
      model_(model),
      layout_(layout),
      window_manager_(window_manager),
      window_registry_(window_registry),
      catalog_(catalog),
      launcher_(launcher),
      app_launcher_(std::move(app_launcher)) {
}

void DockController::request_render() {
    if (on_request_render_) {
        on_request_render_();
    }
}

bool DockController::handle_touch_down(int32_t x, int32_t y) {
    if (!state_machine_.is_visible()) return false;

    DockHitResult hit = layout_.hit_test(x, y);
    if (hit.type == DockHitType::None) return false;

    is_touch_active_ = true;
    is_scrolling_ = false;
    touch_start_x_ = x;
    touch_start_y_ = y;
    initial_scroll_x_ = layout_.scroll_offset_x();

    if (hit.type == DockHitType::LauncherButton) {
        pressed_index_ = -2;
    } else if (hit.type == DockHitType::Item) {
        pressed_index_ = hit.item_index;
    } else {
        pressed_index_ = -1;
    }

    request_render();
    return true;
}

bool DockController::handle_touch_motion(int32_t x, int32_t y) {
    (void)y;
    if (!state_machine_.is_visible() || !is_touch_active_) return false;

    int32_t dx = x - touch_start_x_;
    if (!is_scrolling_ && std::abs(dx) > 8) {
        is_scrolling_ = true;
        pressed_index_ = -1;
    }

    if (is_scrolling_ && layout_.has_overflow()) {
        layout_.set_scroll_offset_x(initial_scroll_x_ - dx);
        request_render();
        return true;
    }

    return true;
}

bool DockController::handle_touch_up(int32_t x, int32_t y) {
    if (!state_machine_.is_visible() || !is_touch_active_) return false;

    is_touch_active_ = false;

    if (!is_scrolling_) {
        DockHitResult hit = layout_.hit_test(x, y);
        if (hit.type == DockHitType::LauncherButton && pressed_index_ == -2) {
            activate_launcher();
        } else if (hit.type == DockHitType::Item && pressed_index_ == hit.item_index) {
            activate_item(hit.item_index);
        }
    }

    pressed_index_ = -1;
    is_scrolling_ = false;
    request_render();
    return true;
}

void DockController::handle_touch_cancel() {
    is_touch_active_ = false;
    is_scrolling_ = false;
    pressed_index_ = -1;
    request_render();
}

bool DockController::handle_pointer_motion(int32_t x, int32_t y) {
    if (!state_machine_.is_visible()) return false;

    DockHitResult hit = layout_.hit_test(x, y);
    int32_t new_hovered = -1;

    if (hit.type == DockHitType::LauncherButton) {
        new_hovered = -2;
    } else if (hit.type == DockHitType::Item) {
        new_hovered = hit.item_index;
    }

    if (new_hovered != hovered_index_) {
        hovered_index_ = new_hovered;
        request_render();
    }
    return hit.type != DockHitType::None;
}

bool DockController::handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y) {
    (void)button;
    if (!state_machine_.is_visible()) return false;

    if (state == 1) { // Button press
        DockHitResult hit = layout_.hit_test(x, y);
        if (hit.type == DockHitType::LauncherButton) {
            pressed_index_ = -2;
            request_render();
            return true;
        } else if (hit.type == DockHitType::Item) {
            pressed_index_ = hit.item_index;
            request_render();
            return true;
        }
    } else if (state == 0) { // Button release
        if (pressed_index_ != -1) {
            DockHitResult hit = layout_.hit_test(x, y);
            if (hit.type == DockHitType::LauncherButton && pressed_index_ == -2) {
                activate_launcher();
            } else if (hit.type == DockHitType::Item && pressed_index_ == hit.item_index) {
                activate_item(hit.item_index);
            }
            pressed_index_ = -1;
            request_render();
            return true;
        }
    }
    return false;
}

bool DockController::handle_pointer_axis(double delta_x, double delta_y) {
    if (!state_machine_.is_visible() || !layout_.has_overflow()) return false;

    double d = delta_y != 0.0 ? delta_y : delta_x;
    layout_.scroll_by(static_cast<int32_t>(d * 24.0));
    request_render();
    return true;
}

bool DockController::handle_key(uint32_t key_symbol) {
    if (!state_machine_.is_visible()) return false;

    // Arrow Left: 0xff51, Arrow Right: 0xff53, Return: 0xff0d, Space: 0x0020, Esc: 0xff1b
    if (key_symbol == 0xff51) { // Left
        if (selected_index_ > -2) {
            selected_index_--;
            hovered_index_ = selected_index_;
            request_render();
            return true;
        }
    } else if (key_symbol == 0xff53) { // Right
        int32_t max_idx = static_cast<int32_t>(model_.item_count()) - 1;
        if (selected_index_ < max_idx) {
            selected_index_++;
            hovered_index_ = selected_index_;
            request_render();
            return true;
        }
    } else if (key_symbol == 0xff0d || key_symbol == 0x0020) { // Enter or Space
        if (selected_index_ == -2) {
            activate_launcher();
            return true;
        } else if (selected_index_ >= 0 && selected_index_ < static_cast<int32_t>(model_.item_count())) {
            activate_item(selected_index_);
            return true;
        }
    } else if (key_symbol == 0xff1b) { // Escape
        if (selected_index_ != -1) {
            selected_index_ = -1;
            hovered_index_ = -1;
            request_render();
            return true;
        }
    }

    return false;
}

void DockController::activate_launcher() {
    LDDE_LOG_INFO(Dock, "Launcher button activated from Dock");
    launcher_.toggle();
}

void DockController::activate_item(size_t index) {
    const auto* item = model_.item_at(index);
    if (!item) return;

    LDDE_LOG_INFO(Dock, "Dock item activated: " << item->name()
                        << " (" << item->id().value() << "), running="
                        << (item->is_running() ? "true" : "false")
                        << ", active=" << (item->is_active() ? "true" : "false"));

    if (!item->is_running()) {
        // Case 1: Application is NOT running -> Launch shortcut
        const auto* meta = catalog_.find(item->id());
        if (!meta) {
            LDDE_LOG_WARN(Dock, "Cannot launch application " << item->id().value()
                                << ": not found in catalog");
            return;
        }

        if (!app_launcher_) {
            LDDE_LOG_ERROR(Dock, "Cannot launch application: ApplicationLauncher backend unavailable");
            return;
        }

        launcher::LaunchRequest req = launcher::LaunchRequest::from_metadata(*meta);
        LDDE_LOG_INFO(Dock, "Launching application '" << req.name << "' (" << req.executable << ")");
        auto res = app_launcher_->launch(req);
        if (!res.is_success()) {
            LDDE_LOG_ERROR(Dock, "Failed to launch application '" << req.name
                                 << "': " << res.error_message);
        }
    } else if (item->is_running() && !item->is_active()) {
        // Case 2: Application is running, but NOT active -> Focus/Restore
        if (item->window_ids().empty()) return;

        window::WindowId target_win_id = item->window_ids().front();
        auto win = window_registry_.lookup(target_win_id);
        if (win && win->state() == window::WindowState::Minimized) {
            LDDE_LOG_INFO(Dock, "Restoring minimized window " << target_win_id
                                << " for application '" << item->name() << "'");
            window_manager_.restore(target_win_id);
        }

        LDDE_LOG_INFO(Dock, "Activating window " << target_win_id
                            << " for application '" << item->name() << "'");
        window_manager_.activate(target_win_id);
    } else if (item->is_running() && item->is_active()) {
        // Case 3: Application is running AND currently active -> Minimize!
        if (item->window_ids().empty()) return;

        auto active_win = window_manager_.active_window_id();
        window::WindowId target_win_id = active_win.has_value() ? *active_win : item->window_ids().front();

        LDDE_LOG_INFO(Dock, "Minimizing active window " << target_win_id
                            << " for application '" << item->name() << "'");
        window_manager_.minimize(target_win_id);
    }
}

} // namespace ldde::dock
