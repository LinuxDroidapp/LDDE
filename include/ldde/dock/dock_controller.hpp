#pragma once

#include <memory>
#include <functional>
#include <cstdint>
#include "ldde/dock/dock_state.hpp"
#include "ldde/dock/dock_model.hpp"
#include "ldde/dock/dock_layout.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/launcher/application_launcher.hpp"

namespace ldde::dock {

class DockController {
public:
    using RequestRenderCallback = std::function<void()>;

    DockController(DockStateMachine& state_machine,
                   DockModel& model,
                   DockLayout& layout,
                   window::WindowManager& window_manager,
                   window::WindowRegistry& window_registry,
                   application::ApplicationCatalog& catalog,
                   launcher::Launcher& launcher,
                   std::shared_ptr<launcher::ApplicationLauncher> app_launcher);

    void set_application_launcher(std::shared_ptr<launcher::ApplicationLauncher> launcher) {
        app_launcher_ = std::move(launcher);
    }
    [[nodiscard]] std::shared_ptr<launcher::ApplicationLauncher> application_launcher() const noexcept {
        return app_launcher_;
    }

    // Input handlers (accept coordinates local to dock)
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_motion(int32_t x, int32_t y);
    bool handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y);
    bool handle_pointer_axis(double delta_x, double delta_y);

    bool handle_key(uint32_t key_symbol);

    // Actions
    void activate_launcher();
    void activate_item(size_t index);

    [[nodiscard]] int32_t hovered_index() const noexcept { return hovered_index_; }
    [[nodiscard]] int32_t pressed_index() const noexcept { return pressed_index_; }
    [[nodiscard]] int32_t selected_index() const noexcept { return selected_index_; }

    void on_request_render(RequestRenderCallback cb) { on_request_render_ = std::move(cb); }
    void request_render();

private:
    DockStateMachine& state_machine_;
    DockModel& model_;
    DockLayout& layout_;
    window::WindowManager& window_manager_;
    window::WindowRegistry& window_registry_;
    application::ApplicationCatalog& catalog_;
    launcher::Launcher& launcher_;
    std::shared_ptr<launcher::ApplicationLauncher> app_launcher_;

    int32_t hovered_index_ = -1;
    int32_t pressed_index_ = -1;
    int32_t selected_index_ = -1; // -2 for launcher button, >= 0 for items

    int32_t touch_start_x_ = 0;
    int32_t touch_start_y_ = 0;
    int32_t initial_scroll_x_ = 0;
    bool is_touch_active_ = false;
    bool is_scrolling_ = false;

    RequestRenderCallback on_request_render_;
};

} // namespace ldde::dock
