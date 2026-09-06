#include "ldde/system/system_ui.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::system {

SystemUI::SystemUI() = default;

SystemUI::~SystemUI() {
    shutdown();
}

core::Status SystemUI::initialize(
    shell::Shell& shell,
    const display::DisplayPolicy& policy,
    const config::Config& config,
    std::shared_ptr<SystemDataProvider> data_provider) {
    shell_ = &shell;
    display_policy_ = policy;

    if (data_provider) {
        data_provider_ = std::move(data_provider);
    } else {
        data_provider_ = std::make_shared<SystemDataProvider>();
    }

    // Configure Clock
    std::string clk_fmt = config.get_string_or("system", "clock_format", "24h");
    bool show_sec = config.get_bool_or("system", "show_seconds", false);
    data_provider_->clock().set_format(clk_fmt == "12h" ? ClockFormat::Format12H : ClockFormat::Format24H);
    data_provider_->clock().set_show_seconds(show_sec);

    // Update display status provider with initial policy
    data_provider_->display().update_policy(display_policy_);

    // Initialize QuickControlsManager
    controls_mgr_ = std::make_unique<QuickControlsManager>(*data_provider_);

    // Initialize SystemUIController
    controller_ = std::make_unique<SystemUIController>(state_machine_, layout_, *controls_mgr_, *data_provider_);

    controller_->on_request_render([this]() {
        if (shell_) {
            shell_->render_all();
        }
    });

    // Wire status bar rendering into Shell's StatusRegion
    shell_->status_region().set_render_callback(
        [this](shell::ShmBuffer& buf, const shell::ShellTheme& theme, const shell::DesignTokens& tokens) {
            render_status_bar(buf, theme, tokens);
        });

    update_layout();

    initialized_ = true;
    LDDE_LOG_INFO(System, "System UI initialized successfully");
    return core::Status::ok();
}

void SystemUI::shutdown() noexcept {
    if (!initialized_) return;

    if (controller_ && state_machine_.is_open()) {
        controller_->close_panel();
    }

    if (shell_) {
        shell_->status_region().set_render_callback(nullptr);
        shell_ = nullptr;
    }

    controller_.reset();
    controls_mgr_.reset();
    data_provider_.reset();
    initialized_ = false;
    LDDE_LOG_INFO(System, "System UI shut down");
}

void SystemUI::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    if (data_provider_) {
        data_provider_->display().update_policy(policy);
    }
    update_layout();
    if (shell_) {
        shell_->render_all();
    }
}

void SystemUI::update_layout() {
    if (shell_ && controls_mgr_) {
        layout_.update(display_policy_, shell_->layout(), shell_->tokens(), controls_mgr_->control_count());
    }
}

void SystemUI::render_status_bar(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens) {
    if (!data_provider_) return;
    SystemUIView::render_status_bar(buffer, theme, tokens, layout_, *data_provider_);
}

void SystemUI::render_panel(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens) {
    if (!data_provider_ || !controls_mgr_) return;
    SystemUIView::render_system_panel(buffer, theme, tokens, layout_, *data_provider_, *controls_mgr_);
}

bool SystemUI::is_panel_open() const noexcept {
    return state_machine_.is_open();
}

core::Status SystemUI::open_panel() {
    if (!controller_) return core::Status::error(core::ErrorCategory::System,
                                                core::ErrorCode::SystemUIInitializationFailed,
                                                "Controller not initialized");
    return controller_->open_panel();
}

core::Status SystemUI::close_panel() {
    if (!controller_) return core::Status::error(core::ErrorCategory::System,
                                                core::ErrorCode::SystemUIInitializationFailed,
                                                "Controller not initialized");
    return controller_->close_panel();
}

core::Status SystemUI::toggle_panel() {
    if (!controller_) return core::Status::error(core::ErrorCategory::System,
                                                core::ErrorCode::SystemUIInitializationFailed,
                                                "Controller not initialized");
    return controller_->toggle_panel();
}

bool SystemUI::handle_status_touch_down(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_status_touch_down(x, y);
}

bool SystemUI::handle_status_touch_up(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_status_touch_up(x, y);
}

bool SystemUI::handle_panel_touch_down(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_panel_touch_down(x, y);
}

bool SystemUI::handle_panel_touch_motion(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_panel_touch_motion(x, y);
}

bool SystemUI::handle_panel_touch_up(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_panel_touch_up(x, y);
}

void SystemUI::handle_panel_touch_cancel() {
    if (controller_) {
        controller_->handle_panel_touch_cancel();
    }
}

bool SystemUI::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    if (!controller_) return false;
    return controller_->handle_key(key_symbol, state, modifiers);
}

} // namespace ldde::system
