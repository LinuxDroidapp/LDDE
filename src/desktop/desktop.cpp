#include "ldde/desktop/desktop.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::desktop {

Desktop::Desktop() = default;

Desktop::~Desktop() {
    shutdown();
}

core::Status Desktop::initialize(
    shell::Shell& shell,
    window::WindowRegistry& window_registry,
    window::WindowManager& window_manager,
    launcher::Launcher& launcher,
    dock::Dock& dock,
    switcher::Switcher& switcher,
    const display::DisplayPolicy& policy,
    const config::Config& config) {

    shell_ = &shell;
    display_policy_ = policy;

    background_.load_config(config);
    view_.load_config(config);

    model_ = std::make_unique<DesktopModel>(window_registry);
    controller_ = std::make_unique<DesktopController>(
        state_machine_, *model_, layout_, launcher, dock, switcher, window_manager);

    controller_->on_request_render([this]() {
        if (shell_) {
            shell_->render_all();
        }
    });

    // Wire D10 Desktop rendering into D1 Shell DesktopSurface
    shell_->desktop().set_render_callback([this](shell::ShmBuffer& buf, const shell::ShellTheme& theme) {
        render(buf, theme);
    });

    update_layout();

    core::Status s = state_machine_.transition_to(DesktopState::Ready);
    if (s.is_error()) {
        LDDE_LOG_ERROR(Desktop, "Failed to transition Desktop to Ready: " << s.to_string());
        return s;
    }

    LDDE_LOG_INFO(Desktop, "Desktop initialized successfully");
    return core::Status::ok();
}

void Desktop::shutdown() noexcept {
    if (state_machine_.is_stopped()) return;

    state_machine_.transition_to(DesktopState::Stopping);

    if (shell_) {
        shell_->desktop().set_render_callback(nullptr);
        shell_ = nullptr;
    }

    controller_.reset();
    model_.reset();

    state_machine_.transition_to(DesktopState::Stopped);
    LDDE_LOG_INFO(Desktop, "Desktop shut down");
}

core::Status Desktop::activate() {
    core::Status s = state_machine_.transition_to(DesktopState::Active);
    if (s.is_ok() && shell_) {
        shell_->render_all();
    }
    return s;
}

core::Status Desktop::suspend() {
    return state_machine_.transition_to(DesktopState::Suspended);
}

core::Status Desktop::resume() {
    core::Status s = state_machine_.transition_to(DesktopState::Active);
    if (s.is_ok() && shell_) {
        shell_->render_all();
    }
    return s;
}

void Desktop::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    update_layout();
    if (controller_) {
        controller_->notify_display_changed(policy);
    }
}

void Desktop::update_layout() {
    if (shell_) {
        layout_.update(display_policy_, shell_->tokens());
    } else {
        layout_.update(display_policy_, shell::DesignTokens::create_scaled(display_policy_.scale_policy().effective_scale()));
    }
}

void Desktop::render(shell::ShmBuffer& buffer, const shell::ShellTheme& /*theme*/) {
    if (!model_) return;
    view_.render(buffer, background_, layout_, *model_);
}

bool Desktop::handle_touch_down(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_touch_down(x, y);
}

bool Desktop::handle_touch_up(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_touch_up(x, y);
}

bool Desktop::handle_touch_motion(int32_t x, int32_t y) {
    if (!controller_) return false;
    return controller_->handle_touch_motion(x, y);
}

void Desktop::handle_touch_cancel() {
    if (controller_) {
        controller_->handle_touch_cancel();
    }
}

bool Desktop::handle_pointer_button(int32_t x, int32_t y, uint32_t button, uint32_t state) {
    if (!controller_) return false;
    return controller_->handle_pointer_button(x, y, button, state);
}

} // namespace ldde::desktop
