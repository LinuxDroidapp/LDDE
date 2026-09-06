#pragma once

#include "ldde/desktop/desktop_state.hpp"
#include "ldde/desktop/desktop_model.hpp"
#include "ldde/desktop/desktop_layout.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/core/types.hpp"
#include <functional>

namespace ldde::desktop {

class DesktopController {
public:
    using RenderRequestCallback = std::function<void()>;

    DesktopController(DesktopStateMachine& state_machine,
                      DesktopModel& model,
                      DesktopLayout& layout,
                      launcher::Launcher& launcher,
                      dock::Dock& dock,
                      switcher::Switcher& switcher,
                      window::WindowManager& window_manager);
    ~DesktopController() = default;

    DesktopController(const DesktopController&) = delete;
    DesktopController& operator=(const DesktopController&) = delete;

    void on_request_render(RenderRequestCallback callback);

    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_button(int32_t x, int32_t y, uint32_t button, uint32_t state);

    void notify_display_changed(const display::DisplayPolicy& policy);

private:
    DesktopStateMachine& state_machine_;
    DesktopModel& model_;
    DesktopLayout& layout_;
    launcher::Launcher& launcher_;
    dock::Dock& dock_;
    switcher::Switcher& switcher_;
    window::WindowManager& window_manager_;

    RenderRequestCallback on_request_render_;

    int32_t touch_start_x_ = 0;
    int32_t touch_start_y_ = 0;
    bool touch_active_ = false;

    void request_render();
    void on_desktop_tapped(int32_t x, int32_t y);
};

} // namespace ldde::desktop
