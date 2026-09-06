#include <gtest/gtest.h>
#include "ldde/system/clock.hpp"
#include "ldde/system/network_status.hpp"
#include "ldde/system/audio_status.hpp"
#include "ldde/system/battery_status.hpp"
#include "ldde/system/display_status.hpp"
#include "ldde/system/session_status.hpp"
#include "ldde/system/system_data_provider.hpp"
#include "ldde/system/quick_controls.hpp"
#include "ldde/system/system_ui_state.hpp"
#include "ldde/system/system_ui_layout.hpp"
#include "ldde/system/system_ui_view.hpp"
#include "ldde/system/system_ui_controller.hpp"
#include "ldde/system/system_ui.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/config/config.hpp"

using namespace ldde;
using namespace ldde::system;

// 1. Clock Tests
TEST(ClockTest, FormattingAndCallbacks) {
    auto mock_prov = std::make_shared<MockClockProvider>();
    ClockInfo info;
    info.hours = 14;
    info.minutes = 35;
    info.seconds = 0;
    info.formatted_time = "14:35";
    info.formatted_date = "Sun, Sep 6";
    mock_prov->set_time(info);

    Clock clock(mock_prov, ClockFormat::Format24H, false);
    EXPECT_EQ(clock.info().formatted_time, "14:35");
    EXPECT_EQ(clock.info().formatted_date, "Sun, Sep 6");

    bool callback_fired = false;
    clock.on_changed([&callback_fired](const ClockInfo& ci) {
        callback_fired = true;
        EXPECT_EQ(ci.formatted_time, "14:36");
    });

    info.minutes = 36;
    info.formatted_time = "14:36";
    mock_prov->set_time(info);

    EXPECT_TRUE(clock.update());
    EXPECT_TRUE(callback_fired);
    EXPECT_FALSE(clock.update()); // no change
}

TEST(ClockTest, SystemClockProviderReturnsValidTime) {
    SystemClockProvider prov;
    auto t24 = prov.query_time(ClockFormat::Format24H, false);
    EXPECT_FALSE(t24.formatted_time.empty());
    EXPECT_FALSE(t24.formatted_date.empty());

    auto t12 = prov.query_time(ClockFormat::Format12H, true);
    EXPECT_FALSE(t12.formatted_time.empty());
}

// 2. Network Status Tests
TEST(NetworkStatusTest, StateAndTypeEnums) {
    EXPECT_EQ(network_state_name(NetworkState::Connected), "Connected");
    EXPECT_EQ(network_state_name(NetworkState::Disconnected), "Disconnected");
    EXPECT_EQ(network_state_name(NetworkState::Unavailable), "Unavailable");

    EXPECT_EQ(network_type_name(NetworkType::Ethernet), "Ethernet");
    EXPECT_EQ(network_type_name(NetworkType::Wifi), "Wi-Fi");
}

TEST(NetworkStatusTest, MockProviderToggleAndCallbacks) {
    auto mock_prov = std::make_shared<MockNetworkProvider>();
    NetworkInfo net_info;
    net_info.state = NetworkState::Connected;
    net_info.type = NetworkType::Wifi;
    net_info.interface_name = "wlan0";
    net_info.status_label = "Connected (wlan0)";
    net_info.is_connected = true;
    net_info.is_enabled = true;
    mock_prov->set_network_info(net_info);

    NetworkStatus net(mock_prov);
    EXPECT_TRUE(net.info().is_connected);
    EXPECT_EQ(net.info().interface_name, "wlan0");

    bool changed = false;
    net.on_changed([&changed](const NetworkInfo& ni) {
        changed = true;
        EXPECT_FALSE(ni.is_enabled);
        EXPECT_FALSE(ni.is_connected);
    });

    net.toggle_enabled();
    EXPECT_TRUE(changed);
    EXPECT_FALSE(net.info().is_enabled);
}

TEST(NetworkStatusTest, LinuxSysfsProviderGracefulHandling) {
    LinuxSysfsNetworkProvider prov("/nonexistent/sys/class/net");
    auto info = prov.query_network();
    EXPECT_EQ(info.state, NetworkState::Unavailable);
    EXPECT_FALSE(info.is_connected);
}

// 3. Audio Status Tests
TEST(AudioStatusTest, LevelCalculation) {
    EXPECT_EQ(compute_audio_level(75, false, true), AudioVolumeLevel::High);
    EXPECT_EQ(compute_audio_level(50, false, true), AudioVolumeLevel::Medium);
    EXPECT_EQ(compute_audio_level(20, false, true), AudioVolumeLevel::Low);
    EXPECT_EQ(compute_audio_level(75, true, true), AudioVolumeLevel::Muted);
    EXPECT_EQ(compute_audio_level(75, false, false), AudioVolumeLevel::Unavailable);
}

TEST(AudioStatusTest, MockProviderMuteAndVolume) {
    auto mock_prov = std::make_shared<MockAudioProvider>();
    AudioInfo info;
    info.volume_percent = 60;
    info.is_muted = false;
    info.is_available = true;
    info.level = AudioVolumeLevel::Medium;
    mock_prov->set_audio_info(info);

    AudioStatus audio(mock_prov);
    EXPECT_EQ(audio.info().volume_percent, 60);
    EXPECT_FALSE(audio.info().is_muted);

    audio.toggle_mute();
    EXPECT_TRUE(audio.info().is_muted);
    EXPECT_EQ(audio.info().level, AudioVolumeLevel::Muted);

    audio.set_volume(90);
    EXPECT_EQ(audio.info().volume_percent, 90);
}

TEST(AudioStatusTest, LinuxAudioProviderGracefulFallback) {
    LinuxAudioProvider prov("/nonexistent/proc/asound");
    auto info = prov.query_audio();
    EXPECT_FALSE(info.is_available);
    EXPECT_EQ(info.level, AudioVolumeLevel::Unavailable);
}

// 4. Battery Status Tests
TEST(BatteryStatusTest, StateEnumsAndGracefulUnavailable) {
    EXPECT_EQ(battery_state_name(BatteryState::Charging), "Charging");
    EXPECT_EQ(battery_state_name(BatteryState::Discharging), "Discharging");
    EXPECT_EQ(battery_state_name(BatteryState::Unavailable), "Unavailable");

    LinuxSysfsBatteryProvider prov("/nonexistent/sys/class/power_supply");
    auto info = prov.query_battery();
    EXPECT_FALSE(info.is_available);
    EXPECT_EQ(info.state, BatteryState::Unavailable);
}

TEST(BatteryStatusTest, MockProviderUpdates) {
    auto mock_prov = std::make_shared<MockBatteryProvider>();
    BatteryInfo b_info;
    b_info.percentage = 85;
    b_info.state = BatteryState::Charging;
    b_info.is_charging = true;
    b_info.is_available = true;
    b_info.is_present = true;
    mock_prov->set_battery_info(b_info);

    BatteryStatus batt(mock_prov);
    EXPECT_EQ(batt.info().percentage, 85);
    EXPECT_TRUE(batt.info().is_charging);
}

// 5. Display & Session Status Tests
TEST(DisplayStatusTest, PolicyIntegration) {
    display::DisplayInfo disp;
    disp.id = 1;
    disp.pixel_width = 1080;
    disp.pixel_height = 2400;
    disp.logical_width = 1080;
    disp.logical_height = 2400;
    disp.width = 1080;
    disp.height = 2400;
    display::DisplayPolicy policy(disp);

    DisplayStatus status(policy);
    EXPECT_EQ(status.info().width, 1080);
    EXPECT_EQ(status.info().height, 2400);
    EXPECT_TRUE(status.info().is_portrait);
    EXPECT_EQ(status.info().orientation_name, "PORTRAIT");
}

TEST(SessionStatusTest, DefaultValuesAndStatusLabel) {
    DesktopSessionStatusProvider prov("Weston", "LinuxDroid LDDE");
    auto info = prov.query_session();
    EXPECT_EQ(info.compositor_name, "Weston");
    EXPECT_EQ(info.desktop_identity, "LinuxDroid LDDE");
    EXPECT_EQ(info.state, SessionState::Active);
    EXPECT_TRUE(info.wayland_connected);
    EXPECT_NE(info.status_label.find("Active"), std::string::npos);
}

// 6. SystemDataProvider Composite Tests
TEST(SystemDataProviderTest, CompositeRefreshAndChangeNotification) {
    auto clock_prov = std::make_shared<MockClockProvider>();
    auto net_prov = std::make_shared<MockNetworkProvider>();
    auto audio_prov = std::make_shared<MockAudioProvider>();
    auto batt_prov = std::make_shared<MockBatteryProvider>();

    SystemDataProvider provider(clock_prov, net_prov, audio_prov, batt_prov);

    bool changed = false;
    provider.on_changed([&changed]() {
        changed = true;
    });

    AudioInfo ai;
    ai.volume_percent = 40;
    ai.is_available = true;
    audio_prov->set_audio_info(ai);
    provider.audio().update();

    EXPECT_TRUE(changed);
}

// 7. Quick Controls Tests
TEST(QuickControlsTest, ControlCapabilitiesAndActions) {
    auto clock_prov = std::make_shared<MockClockProvider>();
    auto net_prov = std::make_shared<MockNetworkProvider>();
    auto audio_prov = std::make_shared<MockAudioProvider>();
    auto batt_prov = std::make_shared<MockBatteryProvider>();

    AudioInfo ai;
    ai.volume_percent = 50;
    ai.is_muted = false;
    ai.is_available = true;
    audio_prov->set_audio_info(ai);

    SystemDataProvider data_provider(clock_prov, net_prov, audio_prov, batt_prov);
    QuickControlsManager controls_mgr(data_provider);

    EXPECT_GE(controls_mgr.control_count(), 4u);
    const auto* audio_ctrl = controls_mgr.control_at(0);
    ASSERT_NE(audio_ctrl, nullptr);
    EXPECT_EQ(audio_ctrl->id, "audio_mute");
    EXPECT_EQ(audio_ctrl->capability, ControlCapability::Available);

    // Activate audio mute control
    EXPECT_TRUE(controls_mgr.activate_index(0));
    EXPECT_TRUE(data_provider.audio().info().is_muted);

    // Keyboard selection navigation
    EXPECT_EQ(controls_mgr.selected_index(), 0);
    controls_mgr.select_next();
    EXPECT_EQ(controls_mgr.selected_index(), 1);
    controls_mgr.select_prev();
    EXPECT_EQ(controls_mgr.selected_index(), 0);
}

// 8. State Machine Tests
TEST(SystemPanelStateMachineTest, ValidAndInvalidTransitions) {
    SystemPanelStateMachine sm(SystemPanelState::Closed);
    EXPECT_TRUE(sm.is_closed());
    EXPECT_FALSE(sm.is_open());

    // Valid open
    EXPECT_TRUE(sm.transition_to(SystemPanelState::Opening).is_ok());
    EXPECT_TRUE(sm.transition_to(SystemPanelState::Open).is_ok());
    EXPECT_TRUE(sm.is_open());

    // Valid close
    EXPECT_TRUE(sm.transition_to(SystemPanelState::Closing).is_ok());
    EXPECT_TRUE(sm.transition_to(SystemPanelState::Closed).is_ok());
    EXPECT_TRUE(sm.is_closed());

    // Invalid transition
    EXPECT_TRUE(sm.transition_to(SystemPanelState::Closing).is_error());
}

// 9. Layout Tests
TEST(SystemUILayoutTest, ResponsiveMetricsAndTouchTargets) {
    display::DisplayInfo disp;
    disp.id = 1;
    disp.pixel_width = 720;
    disp.pixel_height = 1280;
    disp.logical_width = 720;
    disp.logical_height = 1280;
    disp.width = 720;
    disp.height = 1280;
    display::DisplayPolicy policy(disp);

    auto tokens = shell::DesignTokens::create_scaled(1.5);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy, tokens);

    SystemUILayout layout;
    layout.update(policy, shell_layout, tokens, 4);

    EXPECT_EQ(layout.status_bar_geometry(), shell_layout.status_geometry());
    EXPECT_GT(layout.panel_geometry().width, 250);
    EXPECT_GT(layout.panel_geometry().height, 200);

    // Ensure all quick control tiles meet minimum touch target >= 48dp (scaled: 72px)
    for (const auto& tile_geom : layout.control_tile_geometries()) {
        EXPECT_GE(tile_geom.height, tokens.min_touch_target_px);
        EXPECT_GE(tile_geom.width, tokens.min_touch_target_px);
    }

    // Hit test
    core::Point inside_panel{layout.panel_geometry().x + 10, layout.panel_geometry().y + 10};
    EXPECT_TRUE(layout.is_point_in_panel(inside_panel));
    core::Point outside_panel{layout.panel_geometry().x - 10, layout.panel_geometry().y - 10};
    EXPECT_FALSE(layout.is_point_in_panel(outside_panel));
}

// 10. Controller Tests
TEST(SystemUIControllerTest, StatusTapTogglesAndOutsideTapDismisses) {
    SystemPanelStateMachine sm(SystemPanelState::Closed);
    SystemUILayout layout;

    display::DisplayInfo disp;
    disp.id = 1;
    disp.pixel_width = 720;
    disp.pixel_height = 1280;
    disp.logical_width = 720;
    disp.logical_height = 1280;
    disp.width = 720;
    disp.height = 1280;
    display::DisplayPolicy policy(disp);
    auto tokens = shell::DesignTokens::create_scaled(1.0);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy, tokens);

    auto clock_prov = std::make_shared<MockClockProvider>();
    auto net_prov = std::make_shared<MockNetworkProvider>();
    auto audio_prov = std::make_shared<MockAudioProvider>();
    auto batt_prov = std::make_shared<MockBatteryProvider>();
    SystemDataProvider data_provider(clock_prov, net_prov, audio_prov, batt_prov);
    QuickControlsManager controls_mgr(data_provider);

    layout.update(policy, shell_layout, tokens, controls_mgr.control_count());

    SystemUIController controller(sm, layout, controls_mgr, data_provider);

    // Tap on status bar opens panel
    EXPECT_TRUE(controller.handle_status_touch_down(100, 20));
    EXPECT_TRUE(controller.handle_status_touch_up(100, 20));
    EXPECT_TRUE(sm.is_open());

    // Tap outside panel dismisses panel
    EXPECT_TRUE(controller.handle_panel_touch_down(10, 1000));
    EXPECT_TRUE(controller.handle_panel_touch_up(10, 1000));
    EXPECT_TRUE(sm.is_closed());

    // Re-open and test Escape key dismissal
    EXPECT_TRUE(controller.open_panel().is_ok());
    EXPECT_TRUE(sm.is_open());
    EXPECT_TRUE(controller.handle_key(0xff1b)); // Escape
    EXPECT_TRUE(sm.is_closed());
}

// 11. Facade End-to-End Tests
TEST(SystemUIFacadeTest, FullLifecycleAndRendering) {
    shell::Shell shell;
    display::DisplayInfo disp;
    disp.id = 1;
    disp.pixel_width = 720;
    disp.pixel_height = 1280;
    disp.logical_width = 720;
    disp.logical_height = 1280;
    disp.width = 720;
    disp.height = 1280;
    display::DisplayPolicy policy(disp);
    config::Config config;

    SystemUI sysui;
    auto s = sysui.initialize(shell, policy, config);
    EXPECT_TRUE(s.is_ok());
    EXPECT_FALSE(sysui.is_panel_open());

    // Toggle panel
    EXPECT_TRUE(sysui.toggle_panel().is_ok());
    EXPECT_TRUE(sysui.is_panel_open());

    // Test render status bar and panel into dummy buffer
    std::vector<uint8_t> mem(720 * 1280 * 4, 0);
    shell::ShmBuffer buffer(720, 1280, 720 * 4, mem.size(), -1, mem.data(), nullptr);
    shell::ShellTheme theme;
    auto tokens = shell::DesignTokens::create_scaled(1.0);

    EXPECT_NO_FATAL_FAILURE(sysui.render_status_bar(buffer, theme, tokens));
    EXPECT_NO_FATAL_FAILURE(sysui.render_panel(buffer, theme, tokens));

    // Close panel
    EXPECT_TRUE(sysui.close_panel().is_ok());
    EXPECT_FALSE(sysui.is_panel_open());

    sysui.shutdown();
}
