#include <gtest/gtest.h>
#include "ldde/dock/dock.hpp"
#include "ldde/dock/dock_state.hpp"
#include "ldde/dock/dock_item.hpp"
#include "ldde/dock/dock_layout.hpp"
#include "ldde/dock/dock_model.hpp"
#include "ldde/dock/dock_controller.hpp"
#include "ldde/launcher/application_launcher.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/application/desktop_entry_source.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_management_backend.hpp"

using namespace ldde::core;
using namespace ldde::config;
using namespace ldde::dock;
using namespace ldde::application;
using namespace ldde::window;
using namespace ldde::display;
using namespace ldde::shell;
using namespace ldde::launcher;

namespace core = ldde::core;
namespace config = ldde::config;
namespace dock = ldde::dock;
namespace application = ldde::application;
namespace window = ldde::window;
namespace display = ldde::display;
namespace shell = ldde::shell;
namespace launcher = ldde::launcher;

// ============================================================================
// 1. Dock State Tests
// ============================================================================

TEST(DockStateTest, InitialStateIsVisible) {
    DockStateMachine sm;
    EXPECT_EQ(sm.state(), DockState::Visible);
    EXPECT_TRUE(sm.is_visible());
    EXPECT_FALSE(sm.is_hidden());
}

TEST(DockStateTest, ShowAndHideTransitions) {
    DockStateMachine sm;
    std::vector<std::pair<DockState, DockState>> transitions;
    sm.on_state_changed([&](DockState old_s, DockState new_s) {
        transitions.emplace_back(old_s, new_s);
    });

    EXPECT_TRUE(sm.request_hide().is_ok());
    EXPECT_EQ(sm.state(), DockState::Hidden);
    EXPECT_TRUE(sm.is_hidden());
    EXPECT_FALSE(sm.is_visible());

    EXPECT_TRUE(sm.request_show().is_ok());
    EXPECT_EQ(sm.state(), DockState::Visible);
    EXPECT_TRUE(sm.is_visible());

    EXPECT_TRUE(sm.toggle().is_ok());
    EXPECT_EQ(sm.state(), DockState::Hidden);

    EXPECT_TRUE(sm.toggle().is_ok());
    EXPECT_EQ(sm.state(), DockState::Visible);

    EXPECT_FALSE(transitions.empty());
}

TEST(DockStateTest, IdempotentShowAndHide) {
    DockStateMachine sm;
    int change_count = 0;
    sm.on_state_changed([&](DockState, DockState) {
        change_count++;
    });

    // Already Visible: showing again is a no-op
    EXPECT_TRUE(sm.request_show().is_ok());
    EXPECT_EQ(change_count, 0);

    EXPECT_TRUE(sm.request_hide().is_ok());
    int after_hide = change_count;
    EXPECT_TRUE(sm.request_hide().is_ok());
    EXPECT_EQ(change_count, after_hide);
}

// ============================================================================
// 2. Dock Item Tests
// ============================================================================

TEST(DockItemTest, PropertiesAndWindowTracking) {
    ApplicationId id("org.example.App.desktop");
    DockItem item(id, "Example App", ApplicationIconReference("example-app"), true);

    EXPECT_EQ(item.id(), id);
    EXPECT_EQ(item.name(), "Example App");
    EXPECT_TRUE(item.is_pinned());
    EXPECT_FALSE(item.is_running());
    EXPECT_FALSE(item.is_active());
    EXPECT_FALSE(item.is_minimized());
    EXPECT_EQ(item.window_count(), 0u);

    // Add window
    item.add_window(101);
    EXPECT_TRUE(item.is_running());
    EXPECT_EQ(item.window_count(), 1u);
    EXPECT_EQ(item.window_ids().front(), 101u);

    // Add another window (no duplicates if same ID)
    item.add_window(101);
    EXPECT_EQ(item.window_count(), 1u);

    item.add_window(102);
    EXPECT_EQ(item.window_count(), 2u);

    // Remove one window
    item.remove_window(101);
    EXPECT_TRUE(item.is_running());
    EXPECT_EQ(item.window_count(), 1u);
    EXPECT_EQ(item.window_ids().front(), 102u);

    // Remove last window
    item.remove_window(102);
    EXPECT_FALSE(item.is_running());
    EXPECT_EQ(item.window_count(), 0u);
}

class DummyWMBackend : public WindowManagementBackend {
public:
    Status activate(WindowId) override { return Status::ok(); }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId) override { return Status::ok(); }
    Status set_geometry(WindowId, const Rect&) override { return Status::ok(); }
    Status set_maximized(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_fullscreen(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_minimized(WindowId, bool) override { return Status::ok(); }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

// ============================================================================
// 3. Dock Model Tests
// ============================================================================

class DockModelTest : public ::testing::Test {
protected:
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    std::unique_ptr<WindowManager> wm;

    void SetUp() override {
        wm = std::make_unique<WindowManager>(registry, tracker, display_mgr, std::make_unique<DummyWMBackend>());
    }
};

TEST_F(DockModelTest, PinnedApplicationsLoadingAndOrdering) {
    DockModel model(catalog, registry, *wm);

    model.load_pinned_from_string("org.example.App1.desktop,org.example.App2.desktop;org.example.App3.desktop");
    EXPECT_EQ(model.item_count(), 3u);
    EXPECT_TRUE(model.is_pinned(ApplicationId("org.example.App1.desktop")));
    EXPECT_TRUE(model.is_pinned(ApplicationId("org.example.App2.desktop")));
    EXPECT_TRUE(model.is_pinned(ApplicationId("org.example.App3.desktop")));
    EXPECT_FALSE(model.is_pinned(ApplicationId("org.example.NonExistent.desktop")));

    // Pinned order is preserved
    EXPECT_EQ(model.item_at(0)->id().value(), "org.example.App1.desktop");
    EXPECT_EQ(model.item_at(1)->id().value(), "org.example.App2.desktop");
    EXPECT_EQ(model.item_at(2)->id().value(), "org.example.App3.desktop");
}

TEST_F(DockModelTest, DynamicPinAndUnpin) {
    DockModel model(catalog, registry, *wm);
    EXPECT_TRUE(model.pin(ApplicationId("calc.desktop")));
    EXPECT_EQ(model.item_count(), 1u);
    EXPECT_TRUE(model.is_pinned(ApplicationId("calc.desktop")));

    // Duplicate pin returns false
    EXPECT_FALSE(model.pin(ApplicationId("calc.desktop")));
    EXPECT_EQ(model.item_count(), 1u);

    // Unpin
    EXPECT_TRUE(model.unpin(ApplicationId("calc.desktop")));
    EXPECT_EQ(model.item_count(), 0u);
    EXPECT_FALSE(model.is_pinned(ApplicationId("calc.desktop")));
}

TEST_F(DockModelTest, CatalogSyncUpdatesMetadata) {
    std::string desktop_entry_content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Super Terminal\n"
        "Exec=xterm\n"
        "Icon=utilities-terminal\n";
    auto parsed = DesktopEntryParser::parse(desktop_entry_content);
    ASSERT_TRUE(parsed.is_ok());
    DesktopEntrySource src{"/usr/share/applications/term.desktop", DesktopEntrySourceType::System, 10};
    auto meta = ApplicationMetadata::from_desktop_entry(ApplicationId("term.desktop"), parsed.value(), src);
    ASSERT_TRUE(meta.is_ok());

    DockModel model(catalog, registry, *wm);
    model.pin(ApplicationId("term.desktop"));
    model.initialize_listeners();

    // Before catalog has it: available=false
    EXPECT_FALSE(model.item_at(0)->is_available());

    // Update catalog
    catalog.update_applications({meta.value()});

    // Automatically refreshed: available=true, localized name and icon populated
    EXPECT_TRUE(model.item_at(0)->is_available());
    EXPECT_EQ(model.item_at(0)->name(), "Super Terminal");
    EXPECT_EQ(model.item_at(0)->icon_ref().raw(), "utilities-terminal");
}

TEST_F(DockModelTest, WindowCreationSetsRunningAndDestructionCleansUp) {
    DockModel model(catalog, registry, *wm);
    model.pin(ApplicationId("editor.desktop"));
    model.initialize_listeners();

    EXPECT_FALSE(model.item_at(0)->is_running());
    EXPECT_EQ(model.item_at(0)->window_count(), 0u);

    // Add window for editor
    auto win = std::make_shared<Window>(101, nullptr, nullptr, nullptr);
    win->set_app_id("editor");
    win->set_title("Editor Window 1");
    registry.add_window(win);

    // Dock model automatically updates running state
    EXPECT_TRUE(model.item_at(0)->is_running());
    EXPECT_EQ(model.item_at(0)->window_count(), 1u);
    EXPECT_EQ(model.item_at(0)->window_ids().front(), 101u);

    // Add second window for same application
    auto win2 = std::make_shared<Window>(102, nullptr, nullptr, nullptr);
    win2->set_app_id("editor");
    win2->set_title("Editor Window 2");
    registry.add_window(win2);

    EXPECT_EQ(model.item_count(), 1u); // still 1 dock item for application
    EXPECT_EQ(model.item_at(0)->window_count(), 2u);

    // Remove one window
    registry.remove_window(101);
    EXPECT_TRUE(model.item_at(0)->is_running());
    EXPECT_EQ(model.item_at(0)->window_count(), 1u);

    // Remove last window
    registry.remove_window(102);
    EXPECT_FALSE(model.item_at(0)->is_running());
    EXPECT_EQ(model.item_at(0)->window_count(), 0u);
}

TEST_F(DockModelTest, UnpinnedRunningApplicationAppearsAndDisappears) {
    DockModel model(catalog, registry, *wm);
    model.load_pinned_from_string("pinned.desktop");
    model.initialize_listeners();

    EXPECT_EQ(model.item_count(), 1u);

    // Create an unpinned running application window
    auto unpinned_win = std::make_shared<Window>(201, nullptr, nullptr, nullptr);
    unpinned_win->set_app_id("unpinned_app");
    unpinned_win->set_title("Unpinned App Window");
    registry.add_window(unpinned_win);

    // Automatically appears in dock as unpinned item!
    EXPECT_EQ(model.item_count(), 2u);
    EXPECT_TRUE(model.item_at(0)->is_pinned());
    EXPECT_FALSE(model.item_at(1)->is_pinned());
    EXPECT_TRUE(model.item_at(1)->is_running());
    EXPECT_EQ(model.item_at(1)->window_count(), 1u);

    // Close unpinned application window
    registry.remove_window(201);

    // Automatically removed from dock
    EXPECT_EQ(model.item_count(), 1u);
    EXPECT_TRUE(model.item_at(0)->is_pinned());
}

// ============================================================================
// 4. Dock Layout Tests
// ============================================================================

TEST(DockLayoutTest, DynamicMetricsAndTouchTargets) {
    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    disp.logical_width = 720;
    disp.logical_height = 1280;
    disp.scale = 2.0;
    DisplayPolicy policy(disp);
    DesignTokens tokens = DesignTokens::create_scaled(2.0);

    core::Rect dock_rect{36, 1200, 648, 64};
    DockLayout layout;
    layout.update(policy, tokens, dock_rect, 4);

    EXPECT_GE(layout.item_size(), 40);
    EXPECT_EQ(layout.item_rects().size(), 4u);
    EXPECT_GT(layout.launcher_button_rect().width, 0);
    EXPECT_GT(layout.launcher_button_rect().height, 0);

    // Touch hit test launcher button
    auto hit_launcher = layout.hit_test(layout.launcher_button_rect().x + 5,
                                        layout.launcher_button_rect().y + 5);
    EXPECT_EQ(hit_launcher.type, DockHitType::LauncherButton);

    // Touch hit test item 0
    auto hit_item = layout.hit_test(layout.item_rects()[0].x + 5,
                                    layout.item_rects()[0].y + 5);
    EXPECT_EQ(hit_item.type, DockHitType::Item);
    EXPECT_EQ(hit_item.item_index, 0);
}

TEST(DockLayoutTest, OverflowAndScrollClamping) {
    DisplayInfo disp;
    disp.width = 360;
    disp.height = 800;
    disp.scale = 1.0;
    DisplayPolicy policy(disp);
    DesignTokens tokens = DesignTokens::create_scaled(1.0);

    core::Rect small_dock{0, 740, 300, 56};
    DockLayout layout;
    // 10 items will definitely overflow 300px
    layout.update(policy, tokens, small_dock, 10);

    EXPECT_TRUE(layout.has_overflow());
    EXPECT_GT(layout.max_scroll_x(), 0);

    layout.set_scroll_offset_x(50);
    EXPECT_EQ(layout.scroll_offset_x(), 50);

    // Clamps to max_scroll_x
    layout.set_scroll_offset_x(99999);
    EXPECT_EQ(layout.scroll_offset_x(), layout.max_scroll_x());

    // Clamps to 0
    layout.set_scroll_offset_x(-50);
    EXPECT_EQ(layout.scroll_offset_x(), 0);
}

// ============================================================================
// 5. Dock Controller Tests
// ============================================================================

class DockControllerTest : public ::testing::Test {
protected:
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    std::unique_ptr<WindowManager> wm;
    Launcher launcher;
    std::shared_ptr<MockApplicationLauncher> mock_launcher;
    DockStateMachine sm;
    DockLayout layout;
    std::unique_ptr<DockModel> model;
    std::unique_ptr<DockController> controller;

    void SetUp() override {
        wm = std::make_unique<WindowManager>(registry, tracker, display_mgr, std::make_unique<DummyWMBackend>());
        mock_launcher = std::make_shared<MockApplicationLauncher>();
        model = std::make_unique<DockModel>(catalog, registry, *wm);

        DisplayInfo disp;
        disp.width = 720;
        disp.height = 1280;
        DisplayPolicy policy(disp);
        DesignTokens tokens = DesignTokens::create_scaled(1.0);
        core::Rect dock_rect{0, 1200, 720, 64};
        layout.update(policy, tokens, dock_rect, 2);

        controller = std::make_unique<DockController>(
            sm, *model, layout, *wm, registry, catalog, launcher, mock_launcher);
    }
};

TEST_F(DockControllerTest, TapNotRunningLaunchesApp) {
    std::string desktop_entry_content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Test App\n"
        "Exec=test_app\n";
    auto parsed = DesktopEntryParser::parse(desktop_entry_content);
    ASSERT_TRUE(parsed.is_ok());
    DesktopEntrySource src{"/usr/share/applications/test.desktop", DesktopEntrySourceType::System, 10};
    auto meta = ApplicationMetadata::from_desktop_entry(ApplicationId("test.desktop"), parsed.value(), src);
    ASSERT_TRUE(meta.is_ok());
    catalog.update_applications({meta.value()});

    model->pin(ApplicationId("test.desktop"));

    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy policy(disp);
    DesignTokens tokens = DesignTokens::create_scaled(1.0);
    layout.update(policy, tokens, core::Rect{0, 1200, 720, 64}, model->item_count());

    EXPECT_EQ(mock_launcher->launch_count(), 0u);

    // Tap item 0
    core::Rect r = layout.item_rects()[0];
    EXPECT_TRUE(controller->handle_touch_down(r.x + 5, r.y + 5));
    EXPECT_TRUE(controller->handle_touch_up(r.x + 5, r.y + 5));

    EXPECT_EQ(mock_launcher->launch_count(), 1u);
    EXPECT_EQ(mock_launcher->last_request()->name, "Test App");
}

TEST_F(DockControllerTest, TapRunningItemActivatesAndActiveMinimizes) {
    model->pin(ApplicationId("app.desktop"));

    auto win = std::make_shared<Window>(501, nullptr, nullptr, nullptr);
    win->set_app_id("app.desktop");
    registry.add_window(win);

    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy policy(disp);
    DesignTokens tokens = DesignTokens::create_scaled(1.0);
    layout.update(policy, tokens, core::Rect{0, 1200, 720, 64}, model->item_count());

    // Initially window is not active
    EXPECT_FALSE(model->item_at(0)->is_active());

    // Tap item -> activates window
    controller->activate_item(0);
    EXPECT_EQ(registry.active_window_id(), 501u);

    // Refresh model state
    model->rebuild_items();
    EXPECT_TRUE(model->item_at(0)->is_active());

    // Tap active item again -> minimizes window!
    controller->activate_item(0);
    EXPECT_EQ(win->state(), WindowState::Minimized);
}

TEST_F(DockControllerTest, KeyboardNavigation) {
    model->load_pinned_from_string("app1.desktop,app2.desktop");

    // Right arrow navigates through items
    EXPECT_TRUE(controller->handle_key(0xff53)); // Right
    EXPECT_EQ(controller->selected_index(), 0);

    EXPECT_TRUE(controller->handle_key(0xff53)); // Right
    EXPECT_EQ(controller->selected_index(), 1);

    // Left arrow navigates back
    EXPECT_TRUE(controller->handle_key(0xff51)); // Left
    EXPECT_EQ(controller->selected_index(), 0);

    // Left arrow to launcher button (-2)
    EXPECT_TRUE(controller->handle_key(0xff51));
    EXPECT_TRUE(controller->handle_key(0xff51));
    EXPECT_EQ(controller->selected_index(), -2);

    // Escape clears selection
    EXPECT_TRUE(controller->handle_key(0xff1b));
    EXPECT_EQ(controller->selected_index(), -1);
}

// ============================================================================
// 6. Dock Facade Tests
// ============================================================================

TEST(DockFacadeTest, FullLifecycle) {
    ApplicationCatalog cat;
    WindowRegistry reg;
    DisplayManager disp_mgr;
    WindowManager wm(reg, *static_cast<WindowTracker*>(nullptr), disp_mgr, nullptr);
    Launcher launcher;
    Config config;
    config.load_defaults();

    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy policy(disp);

    Dock dock;
    EXPECT_TRUE(dock.initialize(cat, reg, wm, launcher, policy, config).is_ok());
    EXPECT_TRUE(dock.is_visible());

    // Render into ShmBuffer
    std::vector<uint8_t> mem(720 * 68 * 4, 0);
    ShmBuffer buf(720, 68, 720 * 4, mem.size(), -1, mem.data(), nullptr);

    ShellTheme theme;
    DesignTokens tokens = DesignTokens::create_scaled(1.0);
    dock.render(buf, theme, tokens);

    // Hide and Show
    EXPECT_TRUE(dock.hide().is_ok());
    EXPECT_FALSE(dock.is_visible());
    EXPECT_TRUE(dock.show().is_ok());
    EXPECT_TRUE(dock.is_visible());

    dock.shutdown();
}
