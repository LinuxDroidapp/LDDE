#include <gtest/gtest.h>
#include "ldde/switcher/switcher.hpp"
#include "ldde/switcher/switcher_state.hpp"
#include "ldde/switcher/switcher_item.hpp"
#include "ldde/switcher/switcher_mru.hpp"
#include "ldde/switcher/switcher_model.hpp"
#include "ldde/switcher/switcher_layout.hpp"
#include "ldde/switcher/switcher_controller.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_management_backend.hpp"

using namespace ldde::core;
using namespace ldde::config;
using namespace ldde::switcher;
using namespace ldde::application;
using namespace ldde::window;
using namespace ldde::display;
using namespace ldde::shell;

namespace {

class DummyWMBackend : public WindowManagementBackend {
public:
    WindowId last_activated = kInvalidWindowId;
    WindowId last_restored = kInvalidWindowId;

    Status activate(WindowId id) override {
        last_activated = id;
        return Status::ok();
    }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId) override { return Status::ok(); }
    Status set_geometry(WindowId, const Rect&) override { return Status::ok(); }
    Status set_maximized(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_fullscreen(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_minimized(WindowId id, bool minimized) override {
        if (!minimized) {
            last_restored = id;
        }
        return Status::ok();
    }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

std::shared_ptr<Window> create_test_window(WindowId id, std::string_view app_id, std::string_view title) {
    auto win = std::make_shared<Window>(id, nullptr, nullptr, nullptr);
    win->set_app_id(app_id);
    win->set_title(title);
    return win;
}

} // namespace

// ============================================================================
// 1. Switcher State Machine Tests
// ============================================================================

TEST(SwitcherStateTest, InitialStateIsClosed) {
    SwitcherStateMachine sm;
    EXPECT_EQ(sm.state(), SwitcherState::Closed);
    EXPECT_TRUE(sm.is_closed());
    EXPECT_FALSE(sm.is_open());
}

TEST(SwitcherStateTest, OpenAndCloseTransitions) {
    SwitcherStateMachine sm;
    std::vector<std::pair<SwitcherState, SwitcherState>> transitions;
    sm.on_state_changed([&](SwitcherState old_s, SwitcherState new_s) {
        transitions.emplace_back(old_s, new_s);
    });

    EXPECT_TRUE(sm.request_open().is_ok());
    EXPECT_TRUE(sm.is_open());
    EXPECT_EQ(sm.state(), SwitcherState::Open);

    EXPECT_TRUE(sm.start_selection().is_ok());
    EXPECT_EQ(sm.state(), SwitcherState::Selecting);

    EXPECT_TRUE(sm.request_activate().is_ok());
    EXPECT_EQ(sm.state(), SwitcherState::Activating);

    EXPECT_TRUE(sm.request_close().is_ok());
    EXPECT_TRUE(sm.is_closed());
    EXPECT_EQ(sm.state(), SwitcherState::Closed);

    EXPECT_FALSE(transitions.empty());
}

TEST(SwitcherStateTest, IdempotentOpenAndClose) {
    SwitcherStateMachine sm;
    int change_count = 0;
    sm.on_state_changed([&](SwitcherState, SwitcherState) {
        change_count++;
    });

    EXPECT_TRUE(sm.request_open().is_ok());
    int after_open = change_count;

    // Second open is a no-op
    EXPECT_TRUE(sm.request_open().is_ok());
    EXPECT_EQ(change_count, after_open);

    EXPECT_TRUE(sm.request_close().is_ok());
    int after_close = change_count;

    // Second close is a no-op
    EXPECT_TRUE(sm.request_close().is_ok());
    EXPECT_EQ(change_count, after_close);
}

TEST(SwitcherStateTest, CancelTransitions) {
    SwitcherStateMachine sm;
    EXPECT_TRUE(sm.request_open().is_ok());
    EXPECT_TRUE(sm.is_open());

    EXPECT_TRUE(sm.cancel().is_ok());
    EXPECT_TRUE(sm.is_closed());
}

// ============================================================================
// 2. Switcher MRU Tracker Tests
// ============================================================================

TEST(SwitcherMruTest, RecordFocusAndOrdering) {
    SwitcherMru mru;
    EXPECT_TRUE(mru.window_order().empty());
    EXPECT_TRUE(mru.app_order().empty());

    mru.record_focus(101, ApplicationId("firefox.desktop"));
    mru.record_focus(102, ApplicationId("gedit.desktop"));
    mru.record_focus(103, ApplicationId("terminal.desktop"));

    ASSERT_EQ(mru.window_order().size(), 3u);
    EXPECT_EQ(mru.window_order()[0], 103u);
    EXPECT_EQ(mru.window_order()[1], 102u);
    EXPECT_EQ(mru.window_order()[2], 101u);

    EXPECT_EQ(mru.get_window_rank(103), 0);
    EXPECT_EQ(mru.get_window_rank(102), 1);
    EXPECT_EQ(mru.get_window_rank(101), 2);
    EXPECT_EQ(mru.get_window_rank(999), INT_MAX);

    // Repeated focus on 101 moves it to front
    mru.record_focus(101, ApplicationId("firefox.desktop"));
    EXPECT_EQ(mru.window_order()[0], 101u);
    EXPECT_EQ(mru.window_order()[1], 103u);
    EXPECT_EQ(mru.window_order()[2], 102u);
}

TEST(SwitcherMruTest, DestroyedWindowRemovesFromMRU) {
    SwitcherMru mru;
    mru.record_focus(101, ApplicationId("app1"));
    mru.record_focus(102, ApplicationId("app2"));
    EXPECT_EQ(mru.window_order().size(), 2u);

    mru.record_window_destroyed(102);
    ASSERT_EQ(mru.window_order().size(), 1u);
    EXPECT_EQ(mru.window_order()[0], 101u);
    EXPECT_EQ(mru.get_window_rank(102), INT_MAX);
}

// ============================================================================
// 3. Switcher Item Tests
// ============================================================================

TEST(SwitcherItemTest, PropertiesAndWindowList) {
    SwitcherItem item(ApplicationId("browser.desktop"), 10, "Browser", "web-browser", "Main Tab", false);
    EXPECT_EQ(item.app_id().value(), "browser.desktop");
    EXPECT_EQ(item.primary_window_id(), 10u);
    EXPECT_EQ(item.display_name(), "Browser");
    EXPECT_EQ(item.icon_name(), "web-browser");
    EXPECT_EQ(item.window_title(), "Main Tab");
    EXPECT_FALSE(item.is_minimized());
    EXPECT_FALSE(item.is_selected());
    EXPECT_FALSE(item.is_current());
    EXPECT_EQ(item.window_count(), 1u);
    EXPECT_TRUE(item.has_window(10));

    item.add_window(11);
    EXPECT_EQ(item.window_count(), 2u);
    EXPECT_TRUE(item.has_window(11));

    EXPECT_TRUE(item.remove_window(10));
    EXPECT_EQ(item.window_count(), 1u);
    EXPECT_EQ(item.primary_window_id(), 11u); // Next window becomes primary
}

// ============================================================================
// 4. Switcher Model & Grouping Tests
// ============================================================================

class SwitcherModelTest : public ::testing::Test {
protected:
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    DummyWMBackend* backend = nullptr;
    std::unique_ptr<WindowManager> wm;

    void SetUp() override {
        auto b = std::make_unique<DummyWMBackend>();
        backend = b.get();
        wm = std::make_unique<WindowManager>(registry, tracker, display_mgr, std::move(b));
    }
};

TEST_F(SwitcherModelTest, GroupsMultipleWindowsByAppId) {
    auto win1 = create_test_window(101, "calculator", "Calculator Standard");
    auto win2 = create_test_window(102, "calculator", "Calculator Scientific");
    auto win3 = create_test_window(103, "editor", "Text Editor");

    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());
    ASSERT_TRUE(registry.add_window(win3).is_ok());

    SwitcherModel model(catalog, registry, *wm);
    // Should group 2 calculator windows into 1 item, and 1 editor window into 1 item
    ASSERT_EQ(model.item_count(), 2u);

    const auto* calc_item = model.find_by_app_id(ApplicationId("calculator"));
    ASSERT_NE(calc_item, nullptr);
    EXPECT_EQ(calc_item->window_count(), 2u);
    EXPECT_TRUE(calc_item->has_window(101));
    EXPECT_TRUE(calc_item->has_window(102));

    const auto* edit_item = model.find_by_app_id(ApplicationId("editor"));
    ASSERT_NE(edit_item, nullptr);
    EXPECT_EQ(edit_item->window_count(), 1u);
    EXPECT_TRUE(edit_item->has_window(103));
}

TEST_F(SwitcherModelTest, TransientWindowsAssociatedWithParent) {
    auto parent = create_test_window(201, "browser", "Browser Main");
    auto dialog = create_test_window(202, "browser_prefs", "Preferences");
    dialog->set_parent_id(201);

    ASSERT_TRUE(registry.add_window(parent).is_ok());
    ASSERT_TRUE(registry.add_window(dialog).is_ok());

    SwitcherModel model(catalog, registry, *wm);
    // Transient window should not create an independent item in Application mode
    ASSERT_EQ(model.item_count(), 1u);
    EXPECT_EQ(model.item_at(0)->primary_window_id(), 201u);
    EXPECT_TRUE(model.item_at(0)->has_window(202));
}

TEST_F(SwitcherModelTest, DestroyedWindowsExcluded) {
    auto win1 = create_test_window(301, "app1", "Window 1");
    auto win2 = create_test_window(302, "app2", "Window 2");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());

    SwitcherModel model(catalog, registry, *wm);
    EXPECT_EQ(model.item_count(), 2u);

    win2->mark_destroyed();
    ASSERT_TRUE(registry.remove_window(302).is_ok());

    EXPECT_EQ(model.item_count(), 1u);
    EXPECT_EQ(model.item_at(0)->primary_window_id(), 301u);
}

TEST_F(SwitcherModelTest, MRUSortingPreserved) {
    auto win1 = create_test_window(401, "app1", "App 1");
    auto win2 = create_test_window(402, "app2", "App 2");
    auto win3 = create_test_window(403, "app3", "App 3");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());
    ASSERT_TRUE(registry.add_window(win3).is_ok());

    SwitcherModel model(catalog, registry, *wm);
    // Focus app2, then app1
    model.mru().record_focus(402, ApplicationId("app2"));
    model.mru().record_focus(401, ApplicationId("app1"));
    model.rebuild_items();

    ASSERT_EQ(model.item_count(), 3u);
    EXPECT_EQ(model.item_at(0)->primary_window_id(), 401u);
    EXPECT_EQ(model.item_at(1)->primary_window_id(), 402u);
    EXPECT_EQ(model.item_at(2)->primary_window_id(), 403u);
}

// ============================================================================
// 5. Switcher Layout Tests
// ============================================================================

TEST(SwitcherLayoutTest, PortraitMetricsAndTouchTargets) {
    DisplayInfo info;
    info.id = 1;
    info.width = 1080;
    info.height = 2400;
    info.logical_width = 360;
    info.logical_height = 800;
    info.geometry = {0, 0, 360, 800};

    DisplayPolicy policy(info);
    SwitcherLayout layout;
    layout.update(policy, 3);

    EXPECT_FALSE(layout.is_horizontal());
    EXPECT_GE(layout.card_height(), 48); // Minimum touch target satisfied
    EXPECT_EQ(layout.item_rects().size(), 3u);

    // Hit test card 0
    auto hit0 = layout.hit_test({layout.item_rects()[0].x + 10, layout.item_rects()[0].y + 10});
    ASSERT_TRUE(hit0.has_value());
    EXPECT_EQ(*hit0, 0u);

    // Hit test outside cards (backdrop)
    EXPECT_TRUE(layout.hit_test_backdrop({5, 5}));
}

TEST(SwitcherLayoutTest, LandscapeMetrics) {
    DisplayInfo info;
    info.id = 1;
    info.width = 2400;
    info.height = 1080;
    info.logical_width = 800;
    info.logical_height = 360;
    info.geometry = {0, 0, 800, 360};

    DisplayPolicy policy(info);
    SwitcherLayout layout;
    layout.update(policy, 4);

    EXPECT_TRUE(layout.is_horizontal());
    EXPECT_GE(layout.card_width(), 48);
    EXPECT_GE(layout.card_height(), 48);
    EXPECT_EQ(layout.item_rects().size(), 4u);
}

// ============================================================================
// 6. Switcher Controller Tests
// ============================================================================

class SwitcherControllerTest : public ::testing::Test {
protected:
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    DummyWMBackend* raw_backend = nullptr;
    std::unique_ptr<WindowManager> wm;

    void SetUp() override {
        auto b = std::make_unique<DummyWMBackend>();
        raw_backend = b.get();
        wm = std::make_unique<WindowManager>(registry, tracker, display_mgr, std::move(b));
    }
};

TEST_F(SwitcherControllerTest, InitialSelectionHighlightsPreviousApp) {
    auto win1 = create_test_window(501, "app1", "App 1");
    auto win2 = create_test_window(502, "app2", "App 2");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());

    SwitcherStateMachine sm;
    SwitcherLayout layout;
    SwitcherModel model(catalog, registry, *wm);
    SwitcherController ctrl(sm, model, layout, *wm);

    ctrl.open();
    EXPECT_TRUE(sm.is_open());
    // With 2 items, index 1 is selected by default for quick toggle!
    EXPECT_EQ(ctrl.selected_index(), 1u);
}

TEST_F(SwitcherControllerTest, SelectionCyclingWithKeyboard) {
    auto win1 = create_test_window(601, "app1", "App 1");
    auto win2 = create_test_window(602, "app2", "App 2");
    auto win3 = create_test_window(603, "app3", "App 3");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());
    ASSERT_TRUE(registry.add_window(win3).is_ok());

    SwitcherStateMachine sm;
    SwitcherLayout layout;
    SwitcherModel model(catalog, registry, *wm);
    SwitcherController ctrl(sm, model, layout, *wm);

    ctrl.open();
    EXPECT_EQ(ctrl.selected_index(), 1u);

    // Tab moves next
    EXPECT_TRUE(ctrl.handle_key(0xff09)); // Tab
    EXPECT_EQ(ctrl.selected_index(), 2u);

    // Tab wraps to 0
    EXPECT_TRUE(ctrl.handle_key(0xff09));
    EXPECT_EQ(ctrl.selected_index(), 0u);

    // Shift+Tab moves previous
    EXPECT_TRUE(ctrl.handle_key(0xfe20)); // ISO_Left_Tab
    EXPECT_EQ(ctrl.selected_index(), 2u);

    // Esc cancels
    EXPECT_TRUE(ctrl.handle_key(0xff1b)); // Escape
    EXPECT_TRUE(sm.is_closed());
}

TEST_F(SwitcherControllerTest, EnterKeyActivatesSelected) {
    auto win1 = create_test_window(701, "app1", "App 1");
    auto win2 = create_test_window(702, "app2", "App 2");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());

    SwitcherStateMachine sm;
    SwitcherLayout layout;
    SwitcherModel model(catalog, registry, *wm);
    SwitcherController ctrl(sm, model, layout, *wm);

    ctrl.open();
    EXPECT_EQ(ctrl.selected_index(), 1u);

    // Press Enter to activate
    EXPECT_TRUE(ctrl.handle_key(0xff0d)); // Enter
    EXPECT_TRUE(sm.is_closed());
    EXPECT_EQ(raw_backend->last_activated, 702u);
}

TEST_F(SwitcherControllerTest, MinimizedWindowRestoredBeforeActivation) {
    auto win = create_test_window(801, "app1", "App 1");
    win->set_state(WindowState::Minimized);
    ASSERT_TRUE(registry.add_window(win).is_ok());

    SwitcherStateMachine sm;
    SwitcherLayout layout;
    SwitcherModel model(catalog, registry, *wm);
    SwitcherController ctrl(sm, model, layout, *wm);

    ctrl.open();
    EXPECT_EQ(ctrl.selected_index(), 0u);
    EXPECT_TRUE(ctrl.activate_selected().is_ok());

    EXPECT_EQ(raw_backend->last_restored, 801u);
    EXPECT_EQ(raw_backend->last_activated, 801u);
}

// ============================================================================
// 7. Switcher Facade Tests
// ============================================================================

TEST(SwitcherFacadeTest, FullLifecycleAndConfiguration) {
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    auto wm = std::make_unique<WindowManager>(registry, tracker, display_mgr, std::make_unique<DummyWMBackend>());

    DisplayInfo info;
    info.id = 1;
    info.geometry = {0, 0, 720, 1280};
    DisplayPolicy policy(info);

    Config config;
    config.load_defaults();

    Switcher switcher;
    EXPECT_TRUE(switcher.initialize(catalog, registry, *wm, policy, config).is_ok());
    EXPECT_FALSE(switcher.is_open());

    EXPECT_TRUE(switcher.open().is_ok());
    EXPECT_TRUE(switcher.is_open());

    EXPECT_TRUE(switcher.close().is_ok());
    EXPECT_FALSE(switcher.is_open());

    switcher.shutdown();
}
