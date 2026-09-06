#include "ldde/switcher/switcher_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>

namespace ldde::switcher {

SwitcherController::SwitcherController(SwitcherStateMachine& state_machine,
                                       SwitcherModel& model,
                                       SwitcherLayout& layout,
                                       window::WindowManager& window_manager)
    : state_machine_(state_machine),
      model_(model),
      layout_(layout),
      window_manager_(window_manager) {

    model_.on_model_changed([this]() {
        if (selected_index_ >= model_.item_count()) {
            selected_index_ = model_.empty() ? 0 : model_.item_count() - 1;
        }
        sync_selected_item();
        request_render();
    });
}

void SwitcherController::sync_selected_item() {
    for (size_t i = 0; i < model_.item_count(); ++i) {
        auto* item = model_.item_at(i);
        if (item) {
            item->set_is_selected(i == selected_index_);
        }
    }
}

void SwitcherController::request_render() {
    if (on_request_render_) {
        on_request_render_();
    }
}

void SwitcherController::open() {
    if (state_machine_.is_open()) return;

    model_.rebuild_items();
    initial_active_wid_ = window_manager_.active_window_id();
    if (!initial_active_wid_.has_value()) {
        initial_active_wid_ = window_manager_.registry().active_window_id();
    }

    if (model_.item_count() >= 2) {
        selected_index_ = 1; // Standard Alt+Tab highlights the previous application
    } else {
        selected_index_ = 0;
    }

    state_machine_.request_open();
    sync_selected_item();
    request_render();
}

void SwitcherController::close() {
    if (state_machine_.is_closed()) return;
    state_machine_.request_close();
    request_render();
}

void SwitcherController::cancel() {
    if (state_machine_.is_closed()) return;

    if (initial_active_wid_.has_value()) {
        auto win = window_manager_.registry().lookup(*initial_active_wid_);
        if (win && win->lifecycle_state() != window::WindowLifecycleState::Destroyed) {
            window_manager_.activate(*initial_active_wid_);
        }
    }

    state_machine_.cancel();
    request_render();
}

void SwitcherController::toggle() {
    if (state_machine_.is_open()) {
        close();
    } else {
        open();
    }
}

void SwitcherController::select_next() {
    if (model_.empty()) return;
    state_machine_.start_selection();
    selected_index_ = (selected_index_ + 1) % model_.item_count();
    sync_selected_item();
    request_render();
}

void SwitcherController::select_prev() {
    if (model_.empty()) return;
    state_machine_.start_selection();
    selected_index_ = (selected_index_ + model_.item_count() - 1) % model_.item_count();
    sync_selected_item();
    request_render();
}

void SwitcherController::select_index(size_t index) {
    if (index >= model_.item_count()) return;
    state_machine_.start_selection();
    selected_index_ = index;
    sync_selected_item();
    request_render();
}

core::Status SwitcherController::activate_selected() {
    if (model_.empty()) {
        close();
        return core::Status::ok();
    }

    if (selected_index_ >= model_.item_count()) {
        selected_index_ = model_.item_count() - 1;
    }

    const auto* item = model_.item_at(selected_index_);
    if (!item || item->primary_window_id() == window::kInvalidWindowId) {
        close();
        return core::Status::error(core::ErrorCategory::Switcher,
                                   core::ErrorCode::SwitcherWindowNotFound,
                                   "Target switchable window not available");
    }

    window::WindowId target_wid = item->primary_window_id();
    LDDE_LOG_INFO(Switcher, "SWITCHER_ACTIVATION_REQUEST for window " << target_wid
                            << " (" << item->display_name() << ")");

    if (item->is_minimized()) {
        window_manager_.restore(target_wid);
    }

    core::Status s = window_manager_.activate(target_wid);
    if (s.is_ok()) {
        LDDE_LOG_INFO(Switcher, "SWITCHER_ACTIVATION_SUCCESS for window " << target_wid);
    } else {
        LDDE_LOG_WARN(Switcher, "SWITCHER_ACTIVATION_FAILED for window " << target_wid
                               << ": " << s.to_string());
    }

    state_machine_.request_activate();
    close();
    return s;
}

bool SwitcherController::handle_touch_down(int32_t x, int32_t y) {
    is_touch_active_ = true;
    touch_start_x_ = x;
    touch_start_y_ = y;
    touch_last_x_ = x;
    touch_last_y_ = y;
    touch_dragged_ = false;
    return true;
}

bool SwitcherController::handle_touch_motion(int32_t x, int32_t y) {
    if (!is_touch_active_) return false;

    int32_t dx = x - touch_last_x_;
    int32_t dy = y - touch_last_y_;
    touch_last_x_ = x;
    touch_last_y_ = y;

    if (std::abs(x - touch_start_x_) > 8 || std::abs(y - touch_start_y_) > 8) {
        touch_dragged_ = true;
    }

    if (touch_dragged_) {
        if (layout_.is_horizontal()) {
            layout_.scroll_by(-dx);
        } else {
            layout_.scroll_by(-dy);
        }
        request_render();
    }

    return true;
}

bool SwitcherController::handle_touch_up(int32_t x, int32_t y) {
    if (!is_touch_active_) return false;
    is_touch_active_ = false;

    if (!touch_dragged_) {
        auto hit = layout_.hit_test({x, y});
        if (hit.has_value()) {
            select_index(*hit);
            activate_selected();
        } else if (layout_.hit_test_backdrop({x, y})) {
            cancel();
        }
    }

    return true;
}

void SwitcherController::handle_touch_cancel() {
    is_touch_active_ = false;
    cancel();
}

bool SwitcherController::handle_pointer_motion(int32_t x, int32_t y) {
    auto hit = layout_.hit_test({x, y});
    if (hit.has_value() && *hit != selected_index_) {
        select_index(*hit);
    }
    return true;
}

bool SwitcherController::handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y) {
    if (button == 272 && state == 1) { // BTN_LEFT press
        auto hit = layout_.hit_test({x, y});
        if (hit.has_value()) {
            select_index(*hit);
            activate_selected();
        } else if (layout_.hit_test_backdrop({x, y})) {
            cancel();
        }
    }
    return true;
}

bool SwitcherController::handle_pointer_axis(double delta_x, double delta_y) {
    if (layout_.is_horizontal()) {
        layout_.scroll_by(static_cast<int32_t>(delta_y != 0.0 ? delta_y * 20 : delta_x * 20));
    } else {
        layout_.scroll_by(static_cast<int32_t>(delta_y * 20));
    }
    request_render();
    return true;
}

bool SwitcherController::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    if (state == 0) {
        // Key release: if Alt was released while in switching, could commit
        return false;
    }

    switch (key_symbol) {
        case 0xff1b: // Escape
            cancel();
            return true;

        case 0xff0d: // Return / Enter
        case 0xff8d: // Keypad Enter
            activate_selected();
            return true;

        case 0xff09: // Tab
            if ((modifiers & 1) != 0) { // Shift modifier held
                select_prev();
            } else {
                select_next();
            }
            return true;

        case 0xfe20: // XK_ISO_Left_Tab (Shift+Tab)
            select_prev();
            return true;

        case 0xff51: // Left
        case 0xff52: // Up
            select_prev();
            return true;

        case 0xff53: // Right
        case 0xff54: // Down
            select_next();
            return true;

        default:
            break;
    }

    return false;
}

} // namespace ldde::switcher
