#include "ldde/settings/settings_manager.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::settings {

SettingsManager::SettingsManager() = default;

SettingsManager::~SettingsManager() {
    shutdown();
}

core::Status SettingsManager::initialize(shell::Shell& shell,
                                         window::WindowRegistry& window_registry,
                                         window::WindowManager& window_manager,
                                         application::ApplicationCatalog& catalog,
                                         const display::DisplayPolicy& policy,
                                         config::Config& config) {
    shell_ = &shell;
    window_registry_ = &window_registry;
    window_manager_ = &window_manager;
    catalog_ = &catalog;
    display_policy_ = policy;

    schema_ = SettingsSchema::create_default_schema();
    store_ = std::make_unique<SettingsStore>(config, schema_);
    search_ = std::make_unique<SettingsSearch>(schema_);
    controller_ = std::make_unique<SettingsController>(*store_, navigation_, layout_, *search_);

    layout_.update(display_policy_, navigation_, controller_->current_visible_settings(), false);

    controller_->on_request_render([this]() {
        if (on_request_render_) on_request_render_();
    });

    controller_->on_request_close([this]() {
        close();
    });

    controller_->on_request_minimize([this]() {
        minimize();
    });

    controller_->on_request_maximize([this]() {
        if (is_maximized()) {
            restore();
        } else {
            maximize();
        }
    });

    controller_->on_start_move([this](int32_t x, int32_t y) {
        if (window_ && window_manager_ && mode_ == SettingsWindowMode::Normal) {
            window_manager_->start_move(window_->id(), core::Point{x, y});
        }
    });

    register_desktop_entry();

    initialized_ = true;
    LDDE_LOG_INFO(Settings, "SettingsManager initialized successfully");
    return core::Status::ok();
}

void SettingsManager::shutdown() noexcept {
    if (!initialized_) return;

    if (window_ && window_registry_ && window_id_.has_value()) {
        window_->mark_destroyed();
        window_registry_->remove_window(*window_id_);
        window_.reset();
        window_id_.reset();
    }

    mode_ = SettingsWindowMode::Closed;
    initialized_ = false;
    LDDE_LOG_INFO(Settings, "SettingsManager shut down");
}

void SettingsManager::register_desktop_entry() {
    if (!catalog_) return;

    application::DesktopEntry entry;
    entry.set_value("Desktop Entry", "Type", "", "Application");
    entry.set_value("Desktop Entry", "Name", "", "Settings");
    entry.set_value("Desktop Entry", "GenericName", "", "Desktop Preferences");
    entry.set_value("Desktop Entry", "Comment", "", "Configure LDDE desktop environment preferences");
    entry.set_value("Desktop Entry", "Exec", "", "ldde --settings");
    entry.set_value("Desktop Entry", "Icon", "", "preferences-system");
    entry.set_value("Desktop Entry", "Categories", "", "Settings;DesktopSettings;");

    application::DesktopEntrySource source("/usr/share/applications/org.linuxdroid.ldde.settings.desktop", application::DesktopEntrySourceType::System);
    auto meta_res = application::ApplicationMetadata::from_desktop_entry(
        application::ApplicationId("org.linuxdroid.ldde.settings"), entry, source);
    if (meta_res.is_ok()) {
        catalog_->update_applications({meta_res.value()});
        LDDE_LOG_INFO(Settings, "Registered desktop identity 'org.linuxdroid.ldde.settings' in application catalog");
    }
}

void SettingsManager::ensure_window_created() {
    if (window_) return;

    window::WindowId id = 8888; // Stable dedicated window ID for in-process Settings
    if (window_registry_) {
        // Ensure ID is not already used
        while (window_registry_->lookup(id) != nullptr) {
            id++;
        }
    }

    window_ = std::make_shared<window::Window>(id, nullptr, nullptr, nullptr);
    window_->set_title("Settings");
    window_->set_app_id("org.linuxdroid.ldde.settings");
    window_->set_geometry(layout_.window_rect());
    window_->set_visible(true);
    static_cast<void>(window_->transition_to(window::WindowLifecycleState::Visible));

    if (window_registry_) {
        static_cast<void>(window_registry_->add_window(window_));
    }
    window_id_ = id;
}

void SettingsManager::set_mode(SettingsWindowMode new_mode) {
    if (mode_ == new_mode) return;
    mode_ = new_mode;
    if (on_state_changed_) {
        on_state_changed_(mode_);
    }
    if (on_request_render_) {
        on_request_render_();
    }
}

void SettingsManager::open() {
    if (mode_ == SettingsWindowMode::Normal || mode_ == SettingsWindowMode::Maximized) {
        if (window_manager_ && window_) {
            window_manager_->activate(window_->id());
            window_manager_->raise(window_->id());
        }
        return;
    }

    ensure_window_created();
    if (window_) {
        window_->set_visible(true);
        if (window_manager_) {
            window_manager_->activate(window_->id());
            window_manager_->raise(window_->id());
        }
    }

    layout_.update(display_policy_, navigation_, controller_->current_visible_settings(), false);
    set_mode(SettingsWindowMode::Normal);
    LDDE_LOG_INFO(Settings, "Settings window opened");
}

void SettingsManager::close() {
    if (mode_ == SettingsWindowMode::Closed) return;

    if (window_) {
        window_->set_visible(false);
    }
    set_mode(SettingsWindowMode::Closed);
    LDDE_LOG_INFO(Settings, "Settings window closed");
}

void SettingsManager::toggle() {
    if (is_open()) {
        close();
    } else {
        open();
    }
}

void SettingsManager::minimize() {
    if (mode_ == SettingsWindowMode::Closed) return;

    if (window_ && window_manager_) {
        window_manager_->minimize(window_->id());
    }
    set_mode(SettingsWindowMode::Minimized);
    LDDE_LOG_INFO(Settings, "Settings window minimized");
}

void SettingsManager::maximize() {
    if (mode_ == SettingsWindowMode::Closed) return;

    if (window_ && window_manager_) {
        window_manager_->maximize(window_->id());
    }
    layout_.update(display_policy_, navigation_, controller_->current_visible_settings(), true);
    set_mode(SettingsWindowMode::Maximized);
    LDDE_LOG_INFO(Settings, "Settings window maximized");
}

void SettingsManager::restore() {
    if (mode_ == SettingsWindowMode::Closed) return;

    if (window_ && window_manager_) {
        window_manager_->restore(window_->id());
    }
    layout_.update(display_policy_, navigation_, controller_->current_visible_settings(), false);
    set_mode(SettingsWindowMode::Normal);
    LDDE_LOG_INFO(Settings, "Settings window restored");
}

bool SettingsManager::is_open() const noexcept {
    return mode_ == SettingsWindowMode::Normal || mode_ == SettingsWindowMode::Maximized;
}

void SettingsManager::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    if (controller_) {
        layout_.update(display_policy_, navigation_, controller_->current_visible_settings(), is_maximized());
        if (window_) {
            window_->set_geometry(layout_.window_rect());
        }
        if (on_request_render_) {
            on_request_render_();
        }
    }
}

void SettingsManager::render(shell::ShmBuffer& buffer,
                             const shell::ShellTheme& theme,
                             const shell::DesignTokens& tokens) {
    if (!is_open() || !controller_) return;

    auto visible = controller_->current_visible_settings();
    layout_.update(display_policy_, navigation_, visible, is_maximized());
    view_.render(buffer, theme, tokens, layout_, navigation_, *store_, visible, navigation_.selected_index());
}

bool SettingsManager::handle_touch_down(int32_t x, int32_t y) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_touch_down(x, y);
}

bool SettingsManager::handle_touch_motion(int32_t x, int32_t y) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_touch_motion(x, y);
}

bool SettingsManager::handle_touch_up(int32_t x, int32_t y) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_touch_up(x, y);
}

void SettingsManager::handle_touch_cancel() {
    if (!is_open() || !controller_) return;
    controller_->handle_touch_cancel();
}

bool SettingsManager::handle_pointer_motion(int32_t x, int32_t y) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_pointer_motion(x, y);
}

bool SettingsManager::handle_pointer_button(uint32_t button, uint32_t state, int32_t x, int32_t y) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_pointer_button(button, state, x, y);
}

bool SettingsManager::handle_pointer_axis(double delta_x, double delta_y) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_pointer_axis(delta_x, delta_y);
}

bool SettingsManager::handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers) {
    if (!is_open() || !controller_) return false;
    return controller_->handle_key(key_symbol, state, modifiers);
}

} // namespace ldde::settings
