#include "ldde/launcher/launcher.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::launcher {

Launcher::Launcher()
    : controller_(nullptr) {}

Launcher::Launcher(std::shared_ptr<ApplicationLauncher> backend)
    : controller_(std::move(backend)) {}

Launcher::~Launcher() {
    shutdown();
}

core::Status Launcher::initialize(
    application::ApplicationCatalog& catalog,
    const display::DisplayPolicy& policy,
    const config::Config& config,
    std::shared_ptr<ApplicationLauncher> backend) {
    catalog_ = &catalog;
    display_policy_ = policy;

    desktop_identity_ = config.get_string_or("application", "desktop_identity", "LinuxDroid");
    std::string term = config.get_string_or("launcher", "terminal_emulator", "x-terminal-emulator");

    if (backend) {
        controller_.set_launcher_backend(std::move(backend));
    } else {
        controller_.set_launcher_backend(std::make_shared<LinuxSessionApplicationLauncher>(term));
    }

    // Subscribe to catalog change events
    catalog_->on_catalog_refreshed([this](const application::CatalogDiff& /*diff*/) {
        LDDE_LOG_DEBUG(Launcher, "Catalog refreshed notification received; updating launcher model");
        refresh_catalog();
    });

    catalog_->on_application_added([this](const application::ApplicationMetadata& /*app*/) {
        refresh_catalog();
    });

    catalog_->on_application_removed([this](const application::ApplicationMetadata& /*app*/) {
        refresh_catalog();
    });

    catalog_->on_application_changed([this](const application::ApplicationMetadata& /*app*/) {
        refresh_catalog();
    });

    refresh_catalog();
    controller_.update_layout(display_policy_);

    initialized_ = true;
    LDDE_LOG_INFO(Launcher, "Launcher initialized with " << controller_.model().item_count()
                           << " visible applications");

    return core::Status::ok();
}

void Launcher::shutdown() noexcept {
    if (initialized_) {
        controller_.close();
        catalog_ = nullptr;
        initialized_ = false;
        LDDE_LOG_INFO(Launcher, "Launcher shut down");
    }
}

core::Status Launcher::open() {
    return controller_.open();
}

core::Status Launcher::close() {
    return controller_.close();
}

core::Status Launcher::toggle() {
    return controller_.toggle();
}

void Launcher::update_display_policy(const display::DisplayPolicy& policy) {
    display_policy_ = policy;
    controller_.update_layout(policy);
}

void Launcher::render(
    shell::ShmBuffer& buffer,
    const shell::ShellTheme& theme,
    const shell::DesignTokens& tokens) {
    if (!controller_.is_open() && controller_.state() == LauncherState::Closed) {
        return;
    }

    LauncherView::render(
        buffer,
        theme,
        tokens,
        controller_.layout(),
        controller_.model(),
        controller_.state(),
        controller_.scroll_y(),
        controller_.last_error_message());
}

bool Launcher::handle_touch_down(int32_t x, int32_t y) {
    return controller_.handle_touch_down(x, y);
}

bool Launcher::handle_touch_motion(int32_t x, int32_t y) {
    return controller_.handle_touch_motion(x, y);
}

bool Launcher::handle_touch_up(int32_t x, int32_t y) {
    return controller_.handle_touch_up(x, y);
}

void Launcher::handle_touch_cancel() {
    controller_.handle_touch_cancel();
}

bool Launcher::handle_key(uint32_t key_symbol, uint32_t unicode_codepoint) {
    return controller_.handle_key_down(key_symbol, unicode_codepoint);
}

void Launcher::refresh_catalog() {
    if (catalog_) {
        controller_.model().update_from_catalog(*catalog_, desktop_identity_);
    }
}

} // namespace ldde::launcher

