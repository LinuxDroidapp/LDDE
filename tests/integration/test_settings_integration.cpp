#include <gtest/gtest.h>
#include "ldde/settings/settings_manager.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/launcher/application_launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/desktop/desktop.hpp"
#include "ldde/system/system_ui.hpp"
#include "ldde/notification/notification_manager.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/config/config.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_manager.hpp"

using namespace ldde;
using namespace ldde::settings;

namespace {

class DummyWMBackend : public window::WindowManagementBackend {
public:
    core::Status activate(window::WindowId) override { return core::Status::ok(); }
    core::Status deactivate(window::WindowId) override { return core::Status::ok(); }
    core::Status close(window::WindowId) override { return core::Status::ok(); }
    core::Status set_geometry(window::WindowId, const core::Rect&) override { return core::Status::ok(); }
    core::Status set_maximized(window::WindowId, bool, const core::Size&) override { return core::Status::ok(); }
    core::Status set_fullscreen(window::WindowId, bool, const core::Size&) override { return core::Status::ok(); }
    core::Status set_minimized(window::WindowId, bool) override { return core::Status::ok(); }
    core::Status start_move(window::WindowId, uint32_t) override { return core::Status::ok(); }
    core::Status start_resize(window::WindowId, window::ResizeEdge, uint32_t) override { return core::Status::ok(); }
};

display::DisplayPolicy make_policy(int32_t w, int32_t h) {
    display::DisplayInfo info;
    info.id = 1;
    info.name = "WL-1";
    info.width = w;
    info.height = h;
    info.pixel_width = w;
    info.pixel_height = h;
    info.logical_width = w;
    info.logical_height = h;
    info.geometry = core::Rect{0, 0, w, h};
    info.safe_insets = display::SafeInsets{32, 48, 0, 0};
    return display::DisplayPolicy(info);
}

} // namespace

class SettingsIntegrationTest : public ::testing::Test {
protected:
    display::DisplayPolicy policy_;
    config::Config config_;
    core::EventLoop loop_;
    window::WindowRegistry win_reg_;
    display::DisplayManager disp_mgr_;
    window::WindowTracker win_tracker_;
    std::unique_ptr<window::WindowManager> wm_;
    application::ApplicationCatalog catalog_;
    shell::Shell shell_;
    launcher::Launcher launcher_;
    dock::Dock dock_;
    switcher::Switcher switcher_;
    system::SystemUI system_ui_;
    notification::NotificationManager notif_mgr_;
    SettingsManager settings_mgr_;

    void SetUp() override {
        policy_ = make_policy(720, 1280);
        config_.load_defaults();
        loop_.initialize();

        wm_ = std::make_unique<window::WindowManager>(
            win_reg_, win_tracker_, disp_mgr_, std::make_unique<DummyWMBackend>());
        wm_->initialize(config_);

        // Use LinuxSessionApplicationLauncher as backend for launcher
        auto launcher_backend = std::make_shared<launcher::LinuxSessionApplicationLauncher>();
        launcher_.initialize(catalog_, policy_, config_, launcher_backend);

        dock_.initialize(catalog_, win_reg_, *wm_, launcher_, policy_, config_);
        switcher_.initialize(catalog_, win_reg_, *wm_, policy_, config_);
        system_ui_.initialize(shell_, policy_, config_);
        notif_mgr_.initialize(shell_, *wm_, catalog_, policy_, config_, loop_);

        // Initialize SettingsManager
        core::Status s = settings_mgr_.initialize(shell_, win_reg_, *wm_, catalog_, policy_, config_);
        ASSERT_TRUE(s.is_ok()) << s.to_string();

        // Wire built-in launcher handler
        launcher_backend->register_built_in_handler("org.linuxdroid.ldde.settings", [this](const launcher::LaunchRequest&) {
            if (launcher_.is_open()) launcher_.close();
            settings_mgr_.open();
            return true;
        });

        // Wire Quick Controls Settings tile
        system_ui_.controls_manager().on_open_settings([this]() {
            system_ui_.close_panel();
            settings_mgr_.open();
        });

        // Mutual exclusion wiring
        settings_mgr_.on_state_changed([this](SettingsWindowMode new_mode) {
            if (new_mode == SettingsWindowMode::Normal || new_mode == SettingsWindowMode::Maximized) {
                if (launcher_.is_open()) launcher_.close();
                if (switcher_.is_open()) switcher_.close();
                if (system_ui_.is_panel_open()) system_ui_.close_panel();
                if (notif_mgr_.is_notification_center_open()) notif_mgr_.close_notification_center();
            }
        });
    }

    void TearDown() override {
        settings_mgr_.shutdown();
        notif_mgr_.shutdown();
        system_ui_.shutdown();
        switcher_.shutdown();
        dock_.shutdown();
        launcher_.shutdown();
        wm_->shutdown();
    }
};

// ============================================================================
// 1. Catalog Registration and Launcher Interception
// ============================================================================

TEST_F(SettingsIntegrationTest, CatalogRegistrationAndBuiltInInterception) {
    // 1. Verify org.linuxdroid.ldde.settings was registered in catalog
    auto app_id = application::ApplicationId("org.linuxdroid.ldde.settings");
    EXPECT_TRUE(catalog_.contains(app_id));

    const auto* meta = catalog_.find(app_id);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->name(), "Settings");

    // 2. Open launcher
    EXPECT_TRUE(launcher_.open().is_ok());
    EXPECT_TRUE(launcher_.is_open());

    // 3. Launch settings via backend
    auto req = launcher::LaunchRequest::from_metadata(*meta);

    auto* backend = dynamic_cast<launcher::LinuxSessionApplicationLauncher*>(launcher_.controller().launcher_backend().get());
    ASSERT_NE(backend, nullptr);
    EXPECT_TRUE(backend->launch(req).is_success());

    // Launcher should close, and Settings should open!
    EXPECT_FALSE(launcher_.is_open());
    EXPECT_TRUE(settings_mgr_.is_open());
}

// ============================================================================
// 2. Quick Controls Integration
// ============================================================================

TEST_F(SettingsIntegrationTest, QuickControlsTileOpensSettings) {
    // Open system panel
    system_ui_.open_panel();
    EXPECT_TRUE(system_ui_.is_panel_open());

    // Trigger open settings from controls
    bool triggered = false;
    for (size_t i = 0; i < system_ui_.controls_manager().control_count(); ++i) {
        const auto* c = system_ui_.controls_manager().control_at(i);
        if (c && c->type == system::ControlType::Settings) {
            triggered = system_ui_.controls_manager().activate_index(i);
            break;
        }
    }
    EXPECT_TRUE(triggered);

    // System panel closes and settings opens
    EXPECT_FALSE(system_ui_.is_panel_open());
    EXPECT_TRUE(settings_mgr_.is_open());
}

// ============================================================================
// 3. Window Management & Authoritative Registry Integration
// ============================================================================

TEST_F(SettingsIntegrationTest, WindowLifecycleInRegistry) {
    EXPECT_FALSE(settings_mgr_.is_open());
    EXPECT_FALSE(settings_mgr_.window_id().has_value());

    // Open settings window
    settings_mgr_.open();
    EXPECT_TRUE(settings_mgr_.is_open());
    ASSERT_TRUE(settings_mgr_.window_id().has_value());

    auto win_id = *settings_mgr_.window_id();
    auto win = win_reg_.lookup(win_id);
    ASSERT_NE(win, nullptr);

    EXPECT_EQ(win->title(), "Settings");
    EXPECT_EQ(win->app_id(), "org.linuxdroid.ldde.settings");
    EXPECT_TRUE(win->is_visible());

    // Maximize
    settings_mgr_.maximize();
    EXPECT_TRUE(settings_mgr_.is_maximized());

    // Restore
    settings_mgr_.restore();
    EXPECT_FALSE(settings_mgr_.is_maximized());
    EXPECT_TRUE(settings_mgr_.is_open());

    // Minimize
    settings_mgr_.minimize();
    EXPECT_TRUE(settings_mgr_.is_minimized());
    EXPECT_FALSE(settings_mgr_.is_open());

    // Reopen / raise
    settings_mgr_.open();
    EXPECT_TRUE(settings_mgr_.is_open());

    // Close
    settings_mgr_.close();
    EXPECT_FALSE(settings_mgr_.is_open());
    EXPECT_FALSE(win->is_visible());
}

// ============================================================================
// 4. Subsystem Settings Propagation & Change Notification
// ============================================================================

TEST_F(SettingsIntegrationTest, RuntimeSettingPropagation) {
    std::string changed_key;
    settings_mgr_.store().on_setting_changed([&](const std::string& k, const SettingsValue&) {
        changed_key = k;
    });

    // Update dock enabled
    EXPECT_TRUE(settings_mgr_.store().set("dock.enabled", SettingsValue(false), false).is_ok());
    EXPECT_EQ(changed_key, "dock.enabled");
    EXPECT_FALSE(config_.get_bool_or("dock", "enabled", true));

    // Update dock item size
    EXPECT_TRUE(settings_mgr_.store().set("dock.item_size", SettingsValue(static_cast<int64_t>(64)), false).is_ok());
    EXPECT_EQ(changed_key, "dock.item_size");
    EXPECT_EQ(config_.get_int_or("dock", "item_size", 48), 64);
}

// ============================================================================
// 5. Display Orientation Adaptation
// ============================================================================

TEST_F(SettingsIntegrationTest, DisplayOrientationAdaptation) {
    settings_mgr_.open();
    EXPECT_TRUE(settings_mgr_.layout().is_portrait());
    auto portrait_rect = settings_mgr_.layout().window_rect();

    // Rotate to landscape
    auto landscape_policy = make_policy(1920, 1080);
    settings_mgr_.update_display_policy(landscape_policy);

    EXPECT_FALSE(settings_mgr_.layout().is_portrait());
    auto landscape_rect = settings_mgr_.layout().window_rect();

    EXPECT_NE(portrait_rect.width, landscape_rect.width);
    EXPECT_NE(portrait_rect.height, landscape_rect.height);

    // Authoritative window geometry updated as well
    auto win = win_reg_.lookup(*settings_mgr_.window_id());
    ASSERT_NE(win, nullptr);
    EXPECT_EQ(win->geometry(), landscape_rect);
}

