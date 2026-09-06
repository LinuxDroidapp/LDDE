#include "ldde/switcher/switcher.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::switcher {

Switcher::Switcher() = default;
Switcher::~Switcher() {
    shutdown();
}

core::Status Switcher::initialize(
    application::ApplicationCatalog& catalog,
    window::WindowRegistry& registry,
    window::WindowManager& window_manager,
    const display::DisplayPolicy& policy,
    const config::Config& config) {

    enabled_ = config.get_bool_or("switcher", "enabled", true);
    display_policy_ = policy;

    model_ = std::make_unique<SwitcherModel>(catalog, registry, window_manager);

    std::string mode_str = config.get_string_or("switcher", "presentation", "application");
    if (mode_str == "window") {
        model_->set_presentation_mode(SwitcherPresentationMode::Window);
    } else {
        model_->set_presentation_mode(SwitcherPresentationMode::Application);
    }

    controller_ = std::make_unique<SwitcherController>(state_machine_, *model_, layout_, window_manager);

    model_->on_model_changed([this]() {
        update_layout();
    });

    update_layout();

    LDDE_LOG_INFO(Switcher, "Switcher initialized with " << model_->item_count() << " switchable items");
    return core::Status::ok();
}

void Switcher::shutdown() noexcept {
    if (controller_) {
        controller_->close();
    }
    controller_.reset();
    model_.reset();
    LDDE_LOG_INFO(Switcher, "Switcher shut down");
}

core::Status Switcher::open() {
    if (!enabled_) {
        return core::Status::error(core::ErrorCategory::Switcher,
                                   core::ErrorCode::SwitcherInvalidState,
                                   "Switcher is disabled");
    }
    if (!controller_) {
        return core::Status::error(core::ErrorCategory::Switcher,
                                   core::ErrorCode::SwitcherInvalidState,
                                   "Switcher not initialized");
    }
    LDDE_LOG_INFO(Switcher, "SWITCHER_OPEN");
    update_layout();
    controller_->open();
    return core::Status::ok();
}

core::Status Switcher::close() {
    if (!controller_) return core::Status::ok();
    LDDE_LOG_INFO(Switcher, "SWITCHER_CLOSE");
    controller_->close();
    return core::Status::ok();
}

core::Status Switcher::toggle() {
    if (is_open()) {
        return close();
    }
    return open();
}

void Switcher::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    update_layout();
}

void Switcher::update_layout() {
    if (!model_) return;
    layout_.update(display_policy_, model_->item_count());
}

void Switcher::render(shell::ShmBuffer& buffer,
                      const shell::ShellTheme& theme,
                      const shell::DesignTokens& tokens) {
    if (!model_ || !controller_) return;
    view_.render(buffer, theme, tokens, layout_, *model_,
                state_machine_.state(), controller_->selected_index(), icon_resolver_);
}

bool Switcher::handle_touch_down(int32_t x, int32_t y) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_touch_down(x, y);
}

bool Switcher::handle_touch_motion(int32_t x, int32_t y) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_touch_motion(x, y);
}

bool Switcher::handle_touch_up(int32_t x, int32_t y) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_touch_up(x, y);
}

void Switcher::handle_touch_cancel() {
    if (!controller_ || !is_open()) return;
    controller_->handle_touch_cancel();
}

bool Switcher::handle_pointer_motion(int32_t x, int32_t y) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_pointer_motion(x, y);
}

bool Switcher::handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_pointer_button(button, state, x, y);
}

bool Switcher::handle_pointer_axis(double delta_x, double delta_y) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_pointer_axis(delta_x, delta_y);
}

bool Switcher::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    if (!controller_ || !is_open()) return false;
    return controller_->handle_key(key_symbol, state, modifiers);
}

} // namespace ldde::switcher
