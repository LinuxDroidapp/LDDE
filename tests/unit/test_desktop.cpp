#include <gtest/gtest.h>
#include "ldde/desktop/desktop.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/shell/shm_buffer.hpp"

using namespace ldde;
using namespace ldde::desktop;

namespace {

std::shared_ptr<window::Window> create_test_window(window::WindowId id, const std::string& app_id, const std::string& title) {
    auto win = std::make_shared<window::Window>(id, nullptr, nullptr, nullptr);
    win->set_app_id(app_id);
    win->set_title(title);
    win->set_state(window::WindowState::Normal);
    win->set_visible(true);
    return win;
}

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

} // namespace

// =============================================================================
// DesktopState Tests
// =============================================================================

TEST(DesktopStateTest, InitialStateIsInitializing) {
    DesktopStateMachine sm;
    EXPECT_EQ(sm.state(), DesktopState::Initializing);
    EXPECT_FALSE(sm.is_ready());
    EXPECT_FALSE(sm.is_active());
    EXPECT_FALSE(sm.is_suspended());
    EXPECT_FALSE(sm.is_stopped());
}

TEST(DesktopStateTest, ValidTransitionsAndCallbacks) {
    DesktopStateMachine sm;
    std::vector<std::pair<DesktopState, DesktopState>> transitions;
    sm.on_state_changed([&](DesktopState old_s, DesktopState new_s) {
        transitions.emplace_back(old_s, new_s);
    });

    EXPECT_TRUE(sm.transition_to(DesktopState::Ready).is_ok());
    EXPECT_TRUE(sm.is_ready());

    EXPECT_TRUE(sm.transition_to(DesktopState::Active).is_ok());
    EXPECT_TRUE(sm.is_active());

    EXPECT_TRUE(sm.transition_to(DesktopState::Suspended).is_ok());
    EXPECT_TRUE(sm.is_suspended());

    EXPECT_TRUE(sm.transition_to(DesktopState::Active).is_ok());
    EXPECT_TRUE(sm.is_active());

    EXPECT_TRUE(sm.transition_to(DesktopState::Stopping).is_ok());
    EXPECT_TRUE(sm.transition_to(DesktopState::Stopped).is_ok());
    EXPECT_TRUE(sm.is_stopped());

    ASSERT_EQ(transitions.size(), 6u);
    EXPECT_EQ(transitions[0].first, DesktopState::Initializing);
    EXPECT_EQ(transitions[0].second, DesktopState::Ready);
    EXPECT_EQ(transitions[1].first, DesktopState::Ready);
    EXPECT_EQ(transitions[1].second, DesktopState::Active);
}

TEST(DesktopStateTest, InvalidTransitionRejected) {
    DesktopStateMachine sm;
    // Cannot jump from Initializing directly to Suspended
    EXPECT_TRUE(sm.transition_to(DesktopState::Suspended).is_error());
    EXPECT_EQ(sm.state(), DesktopState::Initializing);
}

TEST(DesktopStateTest, IdempotentTransition) {
    DesktopStateMachine sm;
    EXPECT_TRUE(sm.transition_to(DesktopState::Ready).is_ok());
    EXPECT_TRUE(sm.transition_to(DesktopState::Ready).is_ok());
    EXPECT_EQ(sm.state(), DesktopState::Ready);
}

// =============================================================================
// DesktopBackground Tests
// =============================================================================

TEST(DesktopBackgroundTest, ConfigLoadingAndProperties) {
    config::Config cfg;
    cfg.load_defaults();
    cfg.set("desktop", "background_mode", "solid");
    cfg.set("desktop", "background_color", "#202030");
    cfg.set("desktop", "ambient_glow", "false");

    DesktopBackground bg;
    bg.load_config(cfg);

    EXPECT_EQ(bg.mode(), DesktopBackgroundMode::Solid);
    EXPECT_NEAR(bg.color_top().r, 0.125, 0.05); // #202030 approx 0.125
    EXPECT_FALSE(bg.ambient_glow_enabled());
}

TEST(DesktopBackgroundTest, RenderToBuffer) {
    DesktopBackground bg;
    std::vector<uint8_t> mem(200 * 200 * 4, 0);
    shell::ShmBuffer valid_buf(200, 200, 200 * 4, mem.size(), -1, mem.data(), nullptr);

    bg.render(valid_buf, 1.0);
    // Ensure memory was written to (first pixel alpha is opaque)
    uint32_t first_pixel = *reinterpret_cast<uint32_t*>(mem.data());
    EXPECT_NE(first_pixel, 0u);
}

// =============================================================================
// DesktopLayout Tests
// =============================================================================

TEST(DesktopLayoutTest, PortraitPhoneLayout) {
    display::DisplayInfo info;
    info.id = 1;
    info.width = 1080;
    info.height = 2400;
    info.logical_width = 360;
    info.logical_height = 800;
    info.geometry = {0, 0, 360, 800};

    display::DisplayPolicy policy(info);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);

    DesktopLayout layout;
    layout.update(policy, tokens, shell::DockPosition::Bottom);

    EXPECT_EQ(layout.form_factor(), DesktopFormFactor::PhonePortrait);
    EXPECT_EQ(layout.layout_class(), display::LayoutClass::Compact);
    EXPECT_EQ(layout.screen_bounds().width, 360);
    EXPECT_EQ(layout.screen_bounds().height, 800);

    // Workspace should sit between status bar and dock
    EXPECT_GT(layout.workspace_bounds().height, 0);
    EXPECT_EQ(layout.workspace_bounds().y, layout.status_bounds().height);
    EXPECT_EQ(layout.workspace_bounds().height,
              layout.screen_bounds().height - layout.status_bounds().height - layout.dock_bounds().height);

    EXPECT_TRUE(layout.contains_point(core::Point{180, 400}));
    EXPECT_TRUE(layout.is_in_workspace(core::Point{180, 400}));
    EXPECT_FALSE(layout.is_in_workspace(core::Point{180, 5})); // In status bar
}

TEST(DesktopLayoutTest, LandscapePhoneLayout) {
    display::DisplayInfo info;
    info.id = 1;
    info.width = 2400;
    info.height = 1080;
    info.logical_width = 800;
    info.logical_height = 360;
    info.geometry = {0, 0, 800, 360};

    display::DisplayPolicy policy(info);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);

    DesktopLayout layout;
    layout.update(policy, tokens, shell::DockPosition::Bottom);

    EXPECT_EQ(layout.form_factor(), DesktopFormFactor::PhoneLandscape);
    EXPECT_EQ(layout.screen_bounds().width, 800);
    EXPECT_EQ(layout.screen_bounds().height, 360);
}

TEST(DesktopLayoutTest, TabletLayout) {
    display::DisplayInfo info;
    info.id = 1;
    info.width = 1600;
    info.height = 2560;
    info.logical_width = 800;
    info.logical_height = 1280;
    info.geometry = {0, 0, 800, 1280};

    display::DisplayPolicy policy(info);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);

    DesktopLayout layout;
    layout.update(policy, tokens, shell::DockPosition::Bottom);

    EXPECT_EQ(layout.form_factor(), DesktopFormFactor::Tablet);
    EXPECT_EQ(layout.layout_class(), display::LayoutClass::Standard);
}

// =============================================================================
// DesktopModel Tests
// =============================================================================

TEST(DesktopModelTest, WindowAdditionAndRemovalUpdatesEmptyState) {
    window::WindowRegistry registry;
    DesktopModel model(registry);

    EXPECT_EQ(model.active_window_count(), 0u);
    EXPECT_TRUE(model.is_empty());
    EXPECT_TRUE(model.is_desktop_focused());

    bool notified = false;
    model.on_model_changed([&]() { notified = true; });

    auto win1 = create_test_window(101, "terminal", "Terminal");
    ASSERT_TRUE(registry.add_window(win1).is_ok());

    EXPECT_TRUE(notified);
    EXPECT_EQ(model.active_window_count(), 1u);
    EXPECT_FALSE(model.is_empty());

    // Destroy window
    notified = false;
    win1->mark_destroyed();
    ASSERT_TRUE(registry.remove_window(101).is_ok());

    EXPECT_TRUE(notified);
    EXPECT_EQ(model.active_window_count(), 0u);
    EXPECT_TRUE(model.is_empty());
}

// =============================================================================
// DesktopController Tests
// =============================================================================

TEST(DesktopControllerTest, TapEmptyDesktopDismissesOverlays) {
    DesktopStateMachine sm;
    sm.transition_to(DesktopState::Ready);
    sm.transition_to(DesktopState::Active);

    window::WindowRegistry registry;
    DesktopModel model(registry);
    DesktopLayout layout;

    application::ApplicationCatalog catalog;
    window::WindowTracker tracker;
    display::DisplayManager dm;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, dm, std::move(wm_backend));

    display::DisplayInfo info;
    info.width = 720;
    info.height = 1280;
    display::DisplayPolicy dp(info);
    config::Config cfg;
    cfg.load_defaults();

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, dp, cfg).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, dp, cfg).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, dp, cfg).is_ok());

    DesktopController controller(sm, model, layout, launcher, dock, switcher, wm);

    // Open launcher
    ASSERT_TRUE(launcher.open().is_ok());
    EXPECT_TRUE(launcher.is_open());

    // Tap on desktop -> should close launcher
    EXPECT_TRUE(controller.handle_touch_down(100, 100));
    EXPECT_TRUE(controller.handle_touch_up(100, 100));
    EXPECT_FALSE(launcher.is_open());
}

TEST(DesktopControllerTest, SwipeUpOpensLauncher) {
    DesktopStateMachine sm;
    sm.transition_to(DesktopState::Ready);
    sm.transition_to(DesktopState::Active);

    window::WindowRegistry registry;
    DesktopModel model(registry);
    DesktopLayout layout;

    application::ApplicationCatalog catalog;
    window::WindowTracker tracker;
    display::DisplayManager dm;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, dm, std::move(wm_backend));

    display::DisplayInfo info;
    info.width = 720;
    info.height = 1280;
    display::DisplayPolicy dp(info);
    config::Config cfg;
    cfg.load_defaults();

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, dp, cfg).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, dp, cfg).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, dp, cfg).is_ok());

    DesktopController controller(sm, model, layout, launcher, dock, switcher, wm);

    EXPECT_FALSE(launcher.is_open());

    // Swipe up: touch down at y=500, touch up at y=400 (dy = -100)
    EXPECT_TRUE(controller.handle_touch_down(180, 500));
    EXPECT_TRUE(controller.handle_touch_up(180, 400));

    EXPECT_TRUE(launcher.is_open());
}

// =============================================================================
// DesktopFacade Tests
// =============================================================================

TEST(DesktopFacadeTest, FullLifecycleAndDisplayAdaptation) {
    shell::Shell shell;
    window::WindowRegistry registry;
    window::WindowTracker tracker;
    display::DisplayManager dm;
    auto wm_backend = std::make_unique<DummyWMBackend>();
    window::WindowManager wm(registry, tracker, dm, std::move(wm_backend));

    application::ApplicationCatalog catalog;
    display::DisplayInfo info;
    info.id = 1;
    info.width = 1080;
    info.height = 2400;
    info.logical_width = 360;
    info.logical_height = 800;
    info.geometry = {0, 0, 360, 800};
    display::DisplayPolicy dp(info);

    config::Config cfg;
    cfg.load_defaults();

    launcher::Launcher launcher;
    ASSERT_TRUE(launcher.initialize(catalog, dp, cfg).is_ok());

    dock::Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, dp, cfg).is_ok());

    switcher::Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, dp, cfg).is_ok());

    Desktop desktop;
    EXPECT_EQ(desktop.state(), DesktopState::Initializing);

    ASSERT_TRUE(desktop.initialize(shell, registry, wm, launcher, dock, switcher, dp, cfg).is_ok());
    EXPECT_TRUE(desktop.is_ready());

    EXPECT_TRUE(desktop.activate().is_ok());
    EXPECT_TRUE(desktop.is_active());

    EXPECT_TRUE(desktop.suspend().is_ok());
    EXPECT_TRUE(desktop.is_suspended());

    EXPECT_TRUE(desktop.resume().is_ok());
    EXPECT_TRUE(desktop.is_active());

    // Orientation change to landscape
    display::DisplayInfo landscape_info = info;
    landscape_info.width = 2400;
    landscape_info.height = 1080;
    landscape_info.logical_width = 800;
    landscape_info.logical_height = 360;
    landscape_info.geometry = {0, 0, 800, 360};
    display::DisplayPolicy landscape_dp(landscape_info);

    desktop.update_display_policy(landscape_dp);
    EXPECT_EQ(desktop.layout().form_factor(), DesktopFormFactor::PhoneLandscape);
    EXPECT_EQ(desktop.layout().screen_bounds().width, 800);
    EXPECT_EQ(desktop.layout().screen_bounds().height, 360);

    desktop.shutdown();
    EXPECT_EQ(desktop.state(), DesktopState::Stopped);
}
