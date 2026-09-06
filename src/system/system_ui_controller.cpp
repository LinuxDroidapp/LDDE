#include "ldde/system/system_ui_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>

namespace ldde::system {

namespace {
// X11 / Wayland keysyms
constexpr uint32_t kKeyEscape = 0xff1b;
constexpr uint32_t kKeyReturn = 0xff0d;
constexpr uint32_t kKeySpace  = 0x0020;
constexpr uint32_t kKeyTab    = 0xff09;
constexpr uint32_t kKeyLeft   = 0xff51;
constexpr uint32_t kKeyUp     = 0xff52;
constexpr uint32_t kKeyRight  = 0xff53;
constexpr uint32_t kKeyDown   = 0xff54;
constexpr uint32_t kModShift  = 0x01;
}

SystemUIController::SystemUIController(
    SystemPanelStateMachine& state_machine,
    SystemUILayout& layout,
    QuickControlsManager& controls_mgr,
    SystemDataProvider& data_provider)
    : state_machine_(state_machine),
      layout_(layout),
      controls_mgr_(controls_mgr),
      data_provider_(data_provider) {

    controls_mgr_.on_changed([this]() {
        request_render();
    });

    data_provider_.on_changed([this]() {
        request_render();
    });
}

core::Status SystemUIController::open_panel() {
    if (state_machine_.is_open()) {
        return core::Status::ok();
    }
    controls_mgr_.refresh_controls();
    core::Status s = state_machine_.transition_to(SystemPanelState::Open);
    if (s.is_ok()) {
        request_render();
    }
    return s;
}

core::Status SystemUIController::close_panel() {
    if (state_machine_.is_closed()) {
        return core::Status::ok();
    }
    core::Status s = state_machine_.transition_to(SystemPanelState::Closed);
    if (s.is_ok()) {
        request_render();
    }
    return s;
}

core::Status SystemUIController::toggle_panel() {
    if (state_machine_.is_open()) {
        return close_panel();
    } else {
        return open_panel();
    }
}

bool SystemUIController::handle_status_touch_down(int32_t x, int32_t y) {
    status_pressed_ = true;
    touch_down_pos_ = core::Point{x, y};
    last_touch_pos_ = touch_down_pos_;
    return true;
}

bool SystemUIController::handle_status_touch_up(int32_t x, int32_t y) {
    if (!status_pressed_) return false;
    status_pressed_ = false;

    if (x == 0 && y == 0) {
        x = last_touch_pos_.x;
        y = last_touch_pos_.y;
    }

    // Tap within status bar toggles panel
    int32_t dx = x - touch_down_pos_.x;
    int32_t dy = y - touch_down_pos_.y;
    if (std::hypot(dx, dy) < 30.0) {
        toggle_panel();
        return true;
    }
    return false;
}

bool SystemUIController::handle_panel_touch_down(int32_t x, int32_t y) {
    if (!state_machine_.is_open()) return false;
    is_touch_active_ = true;
    touch_down_pos_ = core::Point{x, y};
    last_touch_pos_ = touch_down_pos_;
    return true;
}

bool SystemUIController::handle_panel_touch_motion(int32_t x, int32_t y) {
    if (!state_machine_.is_open() || !is_touch_active_) return false;
    last_touch_pos_ = core::Point{x, y};
    return true;
}

bool SystemUIController::handle_panel_touch_up(int32_t x, int32_t y) {
    if (!state_machine_.is_open() || !is_touch_active_) return false;
    is_touch_active_ = false;

    if (x == 0 && y == 0) {
        x = last_touch_pos_.x;
        y = last_touch_pos_.y;
    }

    core::Point pt{x, y};

    // Check if user swiped up to dismiss panel
    int32_t dy = y - touch_down_pos_.y;
    if (dy < -40) {
        close_panel();
        return true;
    }

    // Check if tapped inside panel
    if (layout_.is_point_in_panel(pt)) {
        // Hit-test quick controls
        int32_t idx = layout_.hit_test_control(pt);
        if (idx >= 0) {
            controls_mgr_.activate_index(static_cast<size_t>(idx));
            request_render();
            return true;
        }
        return true; // consumed inside panel
    }

    // Tap outside panel: dismiss panel!
    close_panel();
    return true;
}

void SystemUIController::handle_panel_touch_cancel() {
    is_touch_active_ = false;
    status_pressed_ = false;
}

bool SystemUIController::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    if (!state_machine_.is_open()) return false;
    if (state == 0) return true; // Only handle key press

    if (key_symbol == kKeyEscape) {
        close_panel();
        return true;
    }

    if (key_symbol == kKeyTab) {
        if (modifiers & kModShift) {
            controls_mgr_.select_prev();
        } else {
            controls_mgr_.select_next();
        }
        request_render();
        return true;
    }

    if (key_symbol == kKeyRight || key_symbol == kKeyDown) {
        controls_mgr_.select_next();
        request_render();
        return true;
    }

    if (key_symbol == kKeyLeft || key_symbol == kKeyUp) {
        controls_mgr_.select_prev();
        request_render();
        return true;
    }

    if (key_symbol == kKeyReturn || key_symbol == kKeySpace) {
        controls_mgr_.activate_selected();
        request_render();
        return true;
    }

    return false;
}

void SystemUIController::on_request_render(RequestRenderCallback callback) {
    render_cb_ = std::move(callback);
}

void SystemUIController::request_render() {
    if (render_cb_) {
        render_cb_();
    }
}

} // namespace ldde::system
