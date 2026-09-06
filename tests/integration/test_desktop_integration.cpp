#include <gtest/gtest.h>
#include "ldde/desktop/desktop.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"

using namespace ldde;
using namespace ldde::desktop;

namespace {

class DummyWMBackend : public window::WindowManagementBackend {
public:
    window::WindowId last_activated = window::kInvalidWindowId;
    window::WindowId last_restored = window::kInvalidWindowId;

    core::Status activate(window::WindowId id) override {
        last_activated = id;
        return core::Status::ok();
    }
    core::Status deactivate(window::WindowId) override { return core::Status::ok(); }
    core::Status close(window::WindowId) override { return core::Status::ok(); }
    core::Status set_geometry(window::WindowId, const core::Rect&) override { return core::Status::ok(); }
    core::Status set_maximized(window::WindowId, bool, const core::Size&) override { return core::Status::ok(); }
    core::Status set_fullscreen(window::WindowId, bool, const core::Size&) override { return core::Status::ok(); }
    core::Status set_minimized(window::WindowId id, bool minimized) override {
        if (!minimized) {
            last_restored = id;
        }
        return core::Status::ok();
    }
    core::Status start_move(window::WindowId, uint32_t) override { return core::Status::ok(); }
    core::Status start_resize(window::WindowId, window::ResizeEdge, uint32_t) override { return core::Status::ok(); }
};

std::shared_ptr<window::Window> create_window(window::WindowId id, const std::string& app_id, const std::string& title) {
    auto win = std::make_shared<window::Window>(id, nullptr, nullptr, nullptr);
    win->set_app_id(app_id);
    win->set_title(title);
    win->set_state(window::WindowState::Normal);
    win->set_visible(true);
    return win;
}

} // namespace

class DesktopIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.load_defaults();
    }

    config::Config config_;
};

TEST_F(DesktopIntegrationTest, DesktopStartupAndShellComposition) {
    shell::Shell shell;
    window::WindowRegistry registry;
    window::WindowTracker tracker;
    display::DisplayManager display_mgr;

    display::DisplayInfo disp;
    disp.id = 1;
    disp.width = 1080;
    disp.height = 2400;
    disp.logical_width = 360;
    disp.logical_height = 800;
    disp.geometry = {0, 0, 360, 800};
    display::DisplayPolicy disp_policy(disp);

    application::ApplicationCatalog catalog;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, display_mgr, std::move(wm_backend));
    ASSERT_TRUE(wm.initialize(config_).is_ok());

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, disp_policy, config_).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, disp_policy, config_).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, disp_policy, config_).is_ok());

    Desktop desktop;
    ASSERT_TRUE(desktop.initialize(shell, registry, wm, launcher, dock, switcher, disp_policy, config_).is_ok());
    EXPECT_TRUE(desktop.is_ready());

    ASSERT_TRUE(desktop.activate().is_ok());
    EXPECT_TRUE(desktop.is_active());

    // Verify layout metrics
    EXPECT_EQ(desktop.layout().form_factor(), DesktopFormFactor::PhonePortrait);
    EXPECT_EQ(desktop.layout().screen_bounds().width, 360);
    EXPECT_EQ(desktop.layout().screen_bounds().height, 800);

    // Verify rendering into buffer
    std::vector<uint8_t> buffer_data(360 * 800 * 4, 0);
    shell::ShmBuffer buffer(360, 800, 360 * 4, buffer_data.size(), -1, buffer_data.data(), nullptr);
    desktop.render(buffer, shell.theme());

    uint32_t first_px = *reinterpret_cast<uint32_t*>(buffer_data.data());
    EXPECT_NE(first_px, 0u);

    desktop.shutdown();
    EXPECT_TRUE(desktop.state() == DesktopState::Stopped);
}

TEST_F(DesktopIntegrationTest, DesktopEmptyStateToWindowRunningAndReturn) {
    shell::Shell shell;
    window::WindowRegistry registry;
    window::WindowTracker tracker;
    display::DisplayManager display_mgr;

    display::DisplayInfo disp;
    disp.id = 1;
    disp.width = 720;
    disp.height = 1280;
    disp.logical_width = 360;
    disp.logical_height = 640;
    disp.geometry = {0, 0, 360, 640};
    display::DisplayPolicy disp_policy(disp);

    application::ApplicationCatalog catalog;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, display_mgr, std::move(wm_backend));
    ASSERT_TRUE(wm.initialize(config_).is_ok());

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, disp_policy, config_).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, disp_policy, config_).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, disp_policy, config_).is_ok());

    Desktop desktop;
    ASSERT_TRUE(desktop.initialize(shell, registry, wm, launcher, dock, switcher, disp_policy, config_).is_ok());
    ASSERT_TRUE(desktop.activate().is_ok());

    // Initial state: 0 windows, empty desktop
    EXPECT_EQ(desktop.model()->active_window_count(), 0u);
    EXPECT_TRUE(desktop.model()->is_empty());
    EXPECT_TRUE(desktop.model()->is_desktop_focused());

    // Launch/add window 1
    auto win1 = create_window(101, "editor", "Text Editor");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    registry.set_active_window(101);

    EXPECT_EQ(desktop.model()->active_window_count(), 1u);
    EXPECT_FALSE(desktop.model()->is_empty());
    EXPECT_FALSE(desktop.model()->is_desktop_focused());

    // Add window 2
    auto win2 = create_window(102, "terminal", "Terminal");
    ASSERT_TRUE(registry.add_window(win2).is_ok());
    registry.set_active_window(102);

    EXPECT_EQ(desktop.model()->active_window_count(), 2u);
    EXPECT_FALSE(desktop.model()->is_empty());

    // Close window 2
    win2->mark_destroyed();
    ASSERT_TRUE(registry.remove_window(102).is_ok());

    EXPECT_EQ(desktop.model()->active_window_count(), 1u);
    EXPECT_FALSE(desktop.model()->is_empty());

    // Close window 1 -> return to empty desktop
    win1->mark_destroyed();
    ASSERT_TRUE(registry.remove_window(101).is_ok());

    EXPECT_EQ(desktop.model()->active_window_count(), 0u);
    EXPECT_TRUE(desktop.model()->is_empty());
    EXPECT_TRUE(desktop.model()->is_desktop_focused());

    desktop.shutdown();
}

TEST_F(DesktopIntegrationTest, DesktopOverlayCoordinationAndTapDismiss) {
    shell::Shell shell;
    window::WindowRegistry registry;
    window::WindowTracker tracker;
    display::DisplayManager display_mgr;

    display::DisplayInfo disp;
    disp.id = 1;
    disp.width = 720;
    disp.height = 1280;
    display::DisplayPolicy disp_policy(disp);

    application::ApplicationCatalog catalog;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, display_mgr, std::move(wm_backend));
    ASSERT_TRUE(wm.initialize(config_).is_ok());

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, disp_policy, config_).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, disp_policy, config_).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, disp_policy, config_).is_ok());

    Desktop desktop;
    ASSERT_TRUE(desktop.initialize(shell, registry, wm, launcher, dock, switcher, disp_policy, config_).is_ok());
    ASSERT_TRUE(desktop.activate().is_ok());

    // 1. Open Launcher
    ASSERT_TRUE(launcher.open().is_ok());
    EXPECT_TRUE(launcher.is_open());

    // Tap on desktop -> closes Launcher
    EXPECT_TRUE(desktop.handle_touch_down(100, 100));
    EXPECT_TRUE(desktop.handle_touch_up(100, 100));
    EXPECT_FALSE(launcher.is_open());

    // 2. Open Switcher
    ASSERT_TRUE(switcher.open().is_ok());
    EXPECT_TRUE(switcher.is_open());

    // Tap on desktop -> closes Switcher
    EXPECT_TRUE(desktop.handle_touch_down(100, 100));
    EXPECT_TRUE(desktop.handle_touch_up(100, 100));
    EXPECT_FALSE(switcher.is_open());

    desktop.shutdown();
}

TEST_F(DesktopIntegrationTest, DesktopDisplayOrientationAdaptation) {
    shell::Shell shell;
    window::WindowRegistry registry;
    window::WindowTracker tracker;
    display::DisplayManager display_mgr;

    display::DisplayInfo portrait_disp;
    portrait_disp.id = 1;
    portrait_disp.width = 1080;
    portrait_disp.height = 2400;
    portrait_disp.logical_width = 360;
    portrait_disp.logical_height = 800;
    portrait_disp.geometry = {0, 0, 360, 800};
    display::DisplayPolicy portrait_policy(portrait_disp);

    application::ApplicationCatalog catalog;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, display_mgr, std::move(wm_backend));
    ASSERT_TRUE(wm.initialize(config_).is_ok());

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, portrait_policy, config_).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, portrait_policy, config_).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, portrait_policy, config_).is_ok());

    Desktop desktop;
    ASSERT_TRUE(desktop.initialize(shell, registry, wm, launcher, dock, switcher, portrait_policy, config_).is_ok());
    ASSERT_TRUE(desktop.activate().is_ok());

    EXPECT_EQ(desktop.layout().form_factor(), DesktopFormFactor::PhonePortrait);
    EXPECT_EQ(desktop.layout().screen_bounds().width, 360);
    EXPECT_EQ(desktop.layout().screen_bounds().height, 800);

    // Dynamic rotation to landscape
    display::DisplayInfo landscape_disp = portrait_disp;
    landscape_disp.width = 2400;
    landscape_disp.height = 1080;
    landscape_disp.logical_width = 800;
    landscape_disp.logical_height = 360;
    landscape_disp.geometry = {0, 0, 800, 360};
    display::DisplayPolicy landscape_policy(landscape_disp);

    desktop.update_display_policy(landscape_policy);

    EXPECT_EQ(desktop.layout().form_factor(), DesktopFormFactor::PhoneLandscape);
    EXPECT_EQ(desktop.layout().screen_bounds().width, 800);
    EXPECT_EQ(desktop.layout().screen_bounds().height, 360);

    desktop.shutdown();
}
