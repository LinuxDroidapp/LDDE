#include <gtest/gtest.h>
#include "ldde/system/system_ui.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/desktop/desktop.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/config/config.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_manager.hpp"

using namespace ldde;
using namespace ldde::system;

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

} // namespace

class SystemUIIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        disp_info_.id = 1;
        disp_info_.name = "WL-1";
        disp_info_.pixel_width = 720;
        disp_info_.pixel_height = 1280;
        disp_info_.logical_width = 720;
        disp_info_.logical_height = 1280;
        disp_info_.width = 720;
        disp_info_.height = 1280;
        policy_ = display::DisplayPolicy(disp_info_);

        config_.load_defaults();

        clock_prov_ = std::make_shared<MockClockProvider>();
        ClockInfo c_info;
        c_info.hours = 12;
        c_info.minutes = 0;
        c_info.formatted_time = "12:00";
        c_info.formatted_date = "Sun, Sep 6";
        clock_prov_->set_time(c_info);

        net_prov_ = std::make_shared<MockNetworkProvider>();
        NetworkInfo n_info;
        n_info.state = NetworkState::Connected;
        n_info.type = NetworkType::Wifi;
        n_info.interface_name = "wlan0";
        n_info.status_label = "Connected (wlan0)";
        n_info.is_connected = true;
        n_info.is_enabled = true;
        net_prov_->set_network_info(n_info);

        audio_prov_ = std::make_shared<MockAudioProvider>();
        AudioInfo a_info;
        a_info.volume_percent = 70;
        a_info.is_muted = false;
        a_info.is_available = true;
        a_info.level = AudioVolumeLevel::High;
        audio_prov_->set_audio_info(a_info);

        batt_prov_ = std::make_shared<MockBatteryProvider>();
        BatteryInfo b_info;
        b_info.percentage = 95;
        b_info.state = BatteryState::Discharging;
        b_info.is_charging = false;
        b_info.is_present = true;
        b_info.is_available = true;
        batt_prov_->set_battery_info(b_info);

        data_provider_ = std::make_shared<SystemDataProvider>(
            clock_prov_, net_prov_, audio_prov_, batt_prov_);
    }

    display::DisplayInfo disp_info_;
    display::DisplayPolicy policy_;
    config::Config config_;
    shell::Shell shell_;

    std::shared_ptr<MockClockProvider> clock_prov_;
    std::shared_ptr<MockNetworkProvider> net_prov_;
    std::shared_ptr<MockAudioProvider> audio_prov_;
    std::shared_ptr<MockBatteryProvider> batt_prov_;
    std::shared_ptr<SystemDataProvider> data_provider_;
};

TEST_F(SystemUIIntegrationTest, StatusRenderingAndClockUpdate) {
    SystemUI sysui;
    auto s = sysui.initialize(shell_, policy_, config_, data_provider_);
    ASSERT_TRUE(s.is_ok());

    EXPECT_FALSE(sysui.is_panel_open());
    EXPECT_EQ(sysui.data_provider().clock().info().formatted_time, "12:00");

    // Advance clock
    ClockInfo c2;
    c2.hours = 12;
    c2.minutes = 1;
    c2.formatted_time = "12:01";
    c2.formatted_date = "Sun, Sep 6";
    clock_prov_->set_time(c2);
    EXPECT_TRUE(sysui.data_provider().clock().update());
    EXPECT_EQ(sysui.data_provider().clock().info().formatted_time, "12:01");

    // Buffer composition
    std::vector<uint8_t> mem(720 * 1280 * 4, 0);
    shell::ShmBuffer buffer(720, 1280, 720 * 4, mem.size(), -1, mem.data(), nullptr);
    shell::ShellTheme theme;
    auto tokens = shell::DesignTokens::create_scaled(1.0);
    EXPECT_NO_FATAL_FAILURE(sysui.render_status_bar(buffer, theme, tokens));

    sysui.shutdown();
}

TEST_F(SystemUIIntegrationTest, PanelToggleAndQuickControlInteraction) {
    SystemUI sysui;
    auto s = sysui.initialize(shell_, policy_, config_, data_provider_);
    ASSERT_TRUE(s.is_ok());

    // Tap status bar to open panel
    EXPECT_TRUE(sysui.handle_status_touch_down(100, 20));
    EXPECT_TRUE(sysui.handle_status_touch_up(100, 20));
    EXPECT_TRUE(sysui.is_panel_open());

    // Hit test quick control 0 (Audio Mute)
    const auto* tile0 = sysui.layout().control_tile_geometry(0);
    ASSERT_NE(tile0, nullptr);
    int32_t cx = tile0->x + tile0->width / 2;
    int32_t cy = tile0->y + tile0->height / 2;

    EXPECT_FALSE(sysui.data_provider().audio().info().is_muted);
    EXPECT_TRUE(sysui.handle_panel_touch_down(cx, cy));
    EXPECT_TRUE(sysui.handle_panel_touch_up(cx, cy));
    EXPECT_TRUE(sysui.data_provider().audio().info().is_muted);

    // Tap outside panel to dismiss
    int32_t outside_x = 10;
    int32_t outside_y = 1000;
    EXPECT_TRUE(sysui.handle_panel_touch_down(outside_x, outside_y));
    EXPECT_TRUE(sysui.handle_panel_touch_up(outside_x, outside_y));
    EXPECT_FALSE(sysui.is_panel_open());

    sysui.shutdown();
}

TEST_F(SystemUIIntegrationTest, OverlayCoordinationWithLauncherAndSwitcher) {
    application::ApplicationCatalog catalog;
    window::WindowRegistry registry;
    window::WindowTracker tracker;
    display::DisplayManager display_mgr;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, display_mgr, std::move(wm_backend));
    ASSERT_TRUE(wm.initialize(config_).is_ok());

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, policy_, config_).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, policy_, config_).is_ok());

    SystemUI sysui;
    ASSERT_TRUE(sysui.initialize(shell_, policy_, config_, data_provider_).is_ok());

    // Connect overlay callback
    shell_.overlay().set_render_callback([&](shell::ShmBuffer& buf, const shell::ShellTheme& theme) {
        if (switcher.is_open()) {
            switcher.render(buf, theme, shell_.tokens());
        } else if (sysui.is_panel_open()) {
            sysui.render_panel(buf, theme, shell_.tokens());
        } else if (launcher.is_open()) {
            launcher.render(buf, theme, shell_.tokens());
        }
    });

    sysui.state_machine().on_state_changed([&](SystemPanelState /*old*/, SystemPanelState new_st) {
        if (new_st == SystemPanelState::Open && launcher.is_open()) {
            launcher.close();
        }
        shell_.overlay().set_active(new_st == SystemPanelState::Open || launcher.is_open() || switcher.is_open());
    });

    // 1. Open launcher
    EXPECT_TRUE(launcher.open().is_ok());
    EXPECT_TRUE(launcher.is_open());

    // 2. Open System UI panel -> launcher should close
    EXPECT_TRUE(sysui.open_panel().is_ok());
    EXPECT_TRUE(sysui.is_panel_open());
    EXPECT_FALSE(launcher.is_open());
    EXPECT_TRUE(shell_.overlay().is_active());

    // 3. Close panel -> overlay becomes inactive
    EXPECT_TRUE(sysui.close_panel().is_ok());
    EXPECT_FALSE(sysui.is_panel_open());
    EXPECT_FALSE(shell_.overlay().is_active());

    launcher.shutdown();
    switcher.shutdown();
    sysui.shutdown();
}

TEST_F(SystemUIIntegrationTest, DisplayOrientationAdaptation) {
    SystemUI sysui;
    ASSERT_TRUE(sysui.initialize(shell_, policy_, config_, data_provider_).is_ok());

    // Rotate to landscape
    display::DisplayInfo landscape_disp;
    landscape_disp.id = 1;
    landscape_disp.pixel_width = 1280;
    landscape_disp.pixel_height = 720;
    landscape_disp.logical_width = 1280;
    landscape_disp.logical_height = 720;
    landscape_disp.width = 1280;
    landscape_disp.height = 720;
    display::DisplayPolicy landscape_policy(landscape_disp);

    shell_.update_display_policy(landscape_policy);
    sysui.update_display_policy(landscape_policy);

    EXPECT_TRUE(sysui.layout().status_bar_geometry().width >= 1280);
    EXPECT_FALSE(sysui.data_provider().display().info().is_portrait);
    EXPECT_EQ(sysui.data_provider().display().info().orientation_name, "LANDSCAPE");

    sysui.shutdown();
}
