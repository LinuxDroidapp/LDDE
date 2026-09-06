#include "ldde/dock/dock.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::dock {

Dock::Dock() = default;

Dock::~Dock() {
    shutdown();
}

core::Status Dock::initialize(
    application::ApplicationCatalog& catalog,
    window::WindowRegistry& registry,
    window::WindowManager& window_manager,
    launcher::Launcher& launcher,
    const display::DisplayPolicy& policy,
    const config::Config& config,
    std::shared_ptr<launcher::ApplicationLauncher> app_launcher) {

    display_policy_ = policy;
    enabled_ = config.get_bool("dock", "enabled").value_or(true);
    std::string vis = config.get_string("dock", "visibility").value_or("visible");
    if (vis == "hidden" || !enabled_) {
        state_machine_.request_hide();
    } else {
        state_machine_.request_show();
    }

    custom_item_size_ = static_cast<int32_t>(config.get_int("dock", "item_size").value_or(48));
    custom_spacing_ = static_cast<int32_t>(config.get_int("dock", "spacing").value_or(8));

    model_ = std::make_unique<DockModel>(catalog, registry, window_manager);

    std::string pinned = config.get_string("dock", "pinned").value_or("");
    if (!pinned.empty()) {
        model_->load_pinned_from_string(pinned);
    }
    model_->initialize_listeners();

    if (!app_launcher) {
        app_launcher = launcher.controller().launcher_backend();
        if (!app_launcher) {
            app_launcher = std::make_shared<launcher::LinuxSessionApplicationLauncher>();
        }
    }

    controller_ = std::make_unique<DockController>(
        state_machine_,
        *model_,
        layout_,
        window_manager,
        registry,
        catalog,
        launcher,
        std::move(app_launcher));

    model_->on_model_changed([this]() {
        update_layout();
        if (controller_) {
            controller_->request_render();
        }
    });

    LDDE_LOG_INFO(Dock, "Dock initialized with " << model_->item_count() << " items");
    return core::Status::ok();
}

void Dock::shutdown() noexcept {
    controller_.reset();
    model_.reset();
    LDDE_LOG_INFO(Dock, "Dock shut down");
}

core::Status Dock::show() {
    return state_machine_.request_show();
}

core::Status Dock::hide() {
    return state_machine_.request_hide();
}

core::Status Dock::toggle() {
    return state_machine_.toggle();
}

void Dock::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    update_layout();
}

void Dock::update_geometry(const core::Rect& dock_geom) {
    dock_geometry_ = dock_geom;
    update_layout();
}

void Dock::update_layout() {
    if (!model_) return;
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(display_policy_.scale_policy().effective_scale());
    layout_.update(display_policy_, tokens, dock_geometry_, model_->item_count(), custom_item_size_, custom_spacing_);
}

void Dock::render(shell::ShmBuffer& buffer,
                  const shell::ShellTheme& theme,
                  const shell::DesignTokens& tokens) {
    if (!state_machine_.is_visible() || !model_) return;

    dock_geometry_ = core::Rect{0, 0, buffer.width(), buffer.height()};
    layout_.update(display_policy_, tokens, dock_geometry_, model_->item_count(), custom_item_size_, custom_spacing_);

    int32_t hovered = controller_ ? controller_->hovered_index() : -1;
    int32_t pressed = controller_ ? controller_->pressed_index() : -1;

    view_.render(buffer, theme, tokens, layout_, *model_, icon_resolver_, hovered, pressed);
}

bool Dock::handle_touch_down(int32_t local_x, int32_t local_y) {
    if (!controller_) return false;
    return controller_->handle_touch_down(local_x, local_y);
}

bool Dock::handle_touch_motion(int32_t local_x, int32_t local_y) {
    if (!controller_) return false;
    return controller_->handle_touch_motion(local_x, local_y);
}

bool Dock::handle_touch_up(int32_t local_x, int32_t local_y) {
    if (!controller_) return false;
    return controller_->handle_touch_up(local_x, local_y);
}

void Dock::handle_touch_cancel() {
    if (controller_) {
        controller_->handle_touch_cancel();
    }
}

bool Dock::handle_pointer_motion(int32_t local_x, int32_t local_y) {
    if (!controller_) return false;
    return controller_->handle_pointer_motion(local_x, local_y);
}

bool Dock::handle_pointer_button(uint32_t button, uint32_t state, int32_t local_x, int32_t local_y) {
    if (!controller_) return false;
    return controller_->handle_pointer_button(button, state, local_x, local_y);
}

bool Dock::handle_pointer_axis(double delta_x, double delta_y) {
    if (!controller_) return false;
    return controller_->handle_pointer_axis(delta_x, delta_y);
}

bool Dock::handle_key(uint32_t key_symbol) {
    if (!controller_) return false;
    return controller_->handle_key(key_symbol);
}

bool Dock::pin(const application::ApplicationId& id) {
    if (!model_) return false;
    return model_->pin(id);
}

bool Dock::unpin(const application::ApplicationId& id) {
    if (!model_) return false;
    return model_->unpin(id);
}

bool Dock::is_pinned(const application::ApplicationId& id) const noexcept {
    if (!model_) return false;
    return model_->is_pinned(id);
}

} // namespace ldde::dock
