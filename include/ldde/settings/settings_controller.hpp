#pragma once

#include <functional>
#include <string>
#include <cstdint>
#include "ldde/settings/settings_types.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_store.hpp"
#include "ldde/settings/settings_navigation.hpp"
#include "ldde/settings/settings_layout.hpp"
#include "ldde/settings/settings_search.hpp"

namespace ldde::settings {

class SettingsController {
public:
    using RenderRequestCallback = std::function<void()>;
    using ActionCallback = std::function<void()>;
    using StartMoveCallback = std::function<void(int32_t x, int32_t y)>;

    SettingsController(SettingsStore& store,
                       SettingsNavigation& navigation,
                       SettingsLayout& layout,
                       const SettingsSearch& search);
    ~SettingsController() = default;

    // Input handlers
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    bool handle_pointer_motion(int32_t x, int32_t y);
    bool handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y);
    bool handle_pointer_axis(double delta_x, double delta_y);

    bool handle_key(uint32_t key_symbol, uint32_t state = 1, uint32_t modifiers = 0);

    // Callbacks
    void on_request_render(RenderRequestCallback cb) { on_request_render_ = std::move(cb); }
    void on_request_close(ActionCallback cb) { on_request_close_ = std::move(cb); }
    void on_request_minimize(ActionCallback cb) { on_request_minimize_ = std::move(cb); }
    void on_request_maximize(ActionCallback cb) { on_request_maximize_ = std::move(cb); }
    void on_start_move(StartMoveCallback cb) { on_start_move_ = std::move(cb); }

    [[nodiscard]] std::vector<const SettingDefinition*> current_visible_settings() const;

private:
    SettingsStore& store_;
    SettingsNavigation& navigation_;
    SettingsLayout& layout_;
    const SettingsSearch& search_;

    bool is_touch_active_ = false;
    bool is_scrolling_ = false;
    bool is_slider_dragging_ = false;
    std::string active_slider_key_;
    int32_t touch_start_x_ = 0;
    int32_t touch_start_y_ = 0;
    int32_t initial_scroll_y_ = 0;

    RenderRequestCallback on_request_render_;
    ActionCallback on_request_close_;
    ActionCallback on_request_minimize_;
    ActionCallback on_request_maximize_;
    StartMoveCallback on_start_move_;

    void request_render();
    void apply_slider_value(const std::string& key, double fraction);
};

} // namespace ldde::settings
