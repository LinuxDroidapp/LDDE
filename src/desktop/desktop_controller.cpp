#include "ldde/desktop/desktop_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>

namespace ldde::desktop {

DesktopController::DesktopController(DesktopStateMachine& state_machine,
                                     DesktopModel& model,
                                     DesktopLayout& layout,
                                     launcher::Launcher& launcher,
                                     dock::Dock& dock,
                                     switcher::Switcher& switcher,
                                     window::WindowManager& window_manager)
    : state_machine_(state_machine),
      model_(model),
      layout_(layout),
      launcher_(launcher),
      dock_(dock),
      switcher_(switcher),
      window_manager_(window_manager) {

    model_.on_model_changed([this]() {
        request_render();
    });
}

void DesktopController::on_request_render(RenderRequestCallback callback) {
    on_request_render_ = std::move(callback);
}

void DesktopController::request_render() {
    if (on_request_render_) {
        on_request_render_();
    }
}

bool DesktopController::handle_touch_down(int32_t x, int32_t y) {
    if (!state_machine_.is_active() && !state_machine_.is_ready()) return false;

    touch_start_x_ = x;
    touch_start_y_ = y;
    touch_active_ = true;
    return true;
}

bool DesktopController::handle_touch_motion(int32_t /*x*/, int32_t /*y*/) {
    return touch_active_;
}

bool DesktopController::handle_touch_up(int32_t x, int32_t y) {
    if (!touch_active_) return false;
    touch_active_ = false;

    int32_t dx = x - touch_start_x_;
    int32_t dy = y - touch_start_y_;

    // Swipe up on desktop opens launcher
    if (dy < -60 && std::abs(dx) < 80) {
        LDDE_LOG_DEBUG(Desktop, "Desktop swipe-up detected; opening launcher");
        launcher_.open();
        return true;
    }

    // Tap on desktop
    if (std::abs(dx) < 24 && std::abs(dy) < 24) {
        on_desktop_tapped(x, y);
        return true;
    }

    return true;
}

void DesktopController::handle_touch_cancel() {
    touch_active_ = false;
}

bool DesktopController::handle_pointer_button(int32_t x, int32_t y, uint32_t /*button*/, uint32_t state) {
    if (state == 1) { // Pressed
        on_desktop_tapped(x, y);
        return true;
    }
    return false;
}

void DesktopController::on_desktop_tapped(int32_t /*x*/, int32_t /*y*/) {
    LDDE_LOG_DEBUG(Desktop, "Desktop background tapped");

    if (switcher_.is_open()) {
        switcher_.close();
    }
    if (launcher_.is_open()) {
        launcher_.close();
    }

    // Request redraw if empty state or focus state updated
    request_render();
}

void DesktopController::notify_display_changed(const display::DisplayPolicy& /*policy*/) {
    request_render();
}

} // namespace ldde::desktop
