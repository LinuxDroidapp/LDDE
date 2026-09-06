#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/launcher/launcher_state.hpp"
#include "ldde/launcher/launcher_model.hpp"
#include "ldde/launcher/launcher_layout.hpp"
#include "ldde/launcher/application_launcher.hpp"

namespace ldde::launcher {

enum class LauncherFocusArea {
    SearchBar = 0,
    Categories,
    Grid
};

class LauncherController {
public:
    using RequestRenderCallback = std::function<void()>;
    using LaunchedCallback = std::function<void(const application::ApplicationId&)>;
    using ClosedCallback = std::function<void()>;

    explicit LauncherController(
        std::shared_ptr<ApplicationLauncher> launcher_backend = nullptr,
        std::string theme_name = "hicolor");

    ~LauncherController() = default;

    // Component accessors
    [[nodiscard]] LauncherStateMachine& state_machine() noexcept { return state_machine_; }
    [[nodiscard]] const LauncherStateMachine& state_machine() const noexcept { return state_machine_; }
    [[nodiscard]] LauncherModel& model() noexcept { return model_; }
    [[nodiscard]] const LauncherModel& model() const noexcept { return model_; }
    [[nodiscard]] LauncherLayout& layout() noexcept { return layout_; }
    [[nodiscard]] const LauncherLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] LauncherState state() const noexcept { return state_machine_.state(); }
    [[nodiscard]] bool is_open() const noexcept { return state_machine_.is_open(); }
    [[nodiscard]] int scroll_y() const noexcept { return scroll_y_; }
    [[nodiscard]] const std::string& last_error_message() const noexcept { return last_error_message_; }
    [[nodiscard]] LauncherFocusArea focus_area() const noexcept { return focus_area_; }

    void set_launcher_backend(std::shared_ptr<ApplicationLauncher> backend) {
        launcher_backend_ = std::move(backend);
    }

    // Lifecycle / visibility
    core::Status open();
    core::Status close();
    core::Status toggle();

    // Display & Layout
    void update_layout(const display::DisplayPolicy& policy);

    // Input handlers
    bool handle_key_down(uint32_t key_symbol, uint32_t unicode_codepoint = 0);
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    // Launch dispatch
    LaunchResult launch_item(size_t index);
    LaunchResult launch_selected();

    // Callbacks
    void on_request_render(RequestRenderCallback cb) { on_request_render_ = std::move(cb); }
    void on_launched(LaunchedCallback cb) { on_launched_ = std::move(cb); }
    void on_closed(ClosedCallback cb) { on_closed_ = std::move(cb); }

private:
    LauncherStateMachine state_machine_;
    LauncherModel model_;
    LauncherLayout layout_;
    std::shared_ptr<ApplicationLauncher> launcher_backend_;

    int scroll_y_ = 0;
    std::string last_error_message_;
    LauncherFocusArea focus_area_ = LauncherFocusArea::SearchBar;

    // Touch interaction tracking
    bool touch_active_ = false;
    core::Point touch_down_point_{0, 0};
    int touch_down_scroll_y_ = 0;
    bool is_scrolling_ = false;
    std::optional<size_t> pressed_item_index_;

    RequestRenderCallback on_request_render_;
    LaunchedCallback on_launched_;
    ClosedCallback on_closed_;

    void request_render();
};

} // namespace ldde::launcher

