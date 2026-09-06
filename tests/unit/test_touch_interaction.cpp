#include <gtest/gtest.h>
#include <memory>
#include "ldde/input/touch_gesture_state.hpp"
#include "ldde/input/touch_interaction_policy.hpp"
#include "ldde/input/touch_hit_testing.hpp"
#include "ldde/input/window_drag_controller.hpp"
#include "ldde/input/window_resize_controller.hpp"
#include "ldde/input/window_control_interaction.hpp"
#include "ldde/input/touch_interaction_manager.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/window/window_management_backend.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/display/display_policy.hpp"

using namespace ldde;
using namespace ldde::input;
using namespace ldde::window;
using namespace ldde::display;

namespace {

class DummyBackend : public WindowManagementBackend {
public:
    WindowRegistry* registry = nullptr;
    Status activate(WindowId) override { return Status::ok(); }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId id) override {
        if (registry) registry->remove_window(id);
        return Status::ok();
    }
    Status set_geometry(WindowId id, const core::Rect& geom) override {
        if (registry) {
            auto win = registry->lookup(id);
            if (win) win->set_geometry(geom);
        }
        return Status::ok();
    }
    Status set_maximized(WindowId id, bool maximized, const core::Size&) override {
        if (registry) {
            auto win = registry->lookup(id);
            if (win) win->set_state(maximized ? WindowState::Maximized : WindowState::Normal);
        }
        return Status::ok();
    }
    Status set_fullscreen(WindowId id, bool fullscreen, const core::Size&) override {
        if (registry) {
            auto win = registry->lookup(id);
            if (win) win->set_state(fullscreen ? WindowState::Fullscreen : WindowState::Normal);
        }
        return Status::ok();
    }
    Status set_minimized(WindowId id, bool minimized) override {
        if (registry) {
            auto win = registry->lookup(id);
            if (win) win->set_state(minimized ? WindowState::Minimized : WindowState::Normal);
        }
        return Status::ok();
    }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

DisplayInfo create_mobile_display() {
    DisplayInfo disp;
    disp.id = 1;
    disp.name = "DSI-1";
    disp.width = 540;
    disp.height = 1200;
    disp.pixel_width = 1080;
    disp.pixel_height = 2400;
    disp.logical_width = 540;
    disp.logical_height = 1200;
    disp.scale = 2;
    disp.orientation = Orientation::Portrait;
    return disp;
}

std::shared_ptr<Window> create_test_window(WindowId id, const core::Rect& geom) {
    auto win = std::make_shared<Window>(id, nullptr, nullptr, nullptr);
    static_cast<void>(win->transition_to(WindowLifecycleState::Initializing));
    static_cast<void>(win->transition_to(WindowLifecycleState::Ready));
    static_cast<void>(win->transition_to(WindowLifecycleState::Visible));
    win->set_geometry(geom);
    win->set_visible(true);
    win->set_state(WindowState::Normal);
    return win;
}

} // namespace

// =============================================================================
// 1. Gesture State Machine Tests
// =============================================================================

TEST(TouchGestureStateTest, StateNames) {
    EXPECT_EQ(gesture_state_name(GestureState::Idle), "Idle");
    EXPECT_EQ(gesture_state_name(GestureState::ContactPending), "ContactPending");
    EXPECT_EQ(gesture_state_name(GestureState::WindowFocus), "WindowFocus");
    EXPECT_EQ(gesture_state_name(GestureState::Moving), "Moving");
    EXPECT_EQ(gesture_state_name(GestureState::Resizing), "Resizing");
    EXPECT_EQ(gesture_state_name(GestureState::ControlPress), "ControlPress");
    EXPECT_EQ(gesture_state_name(GestureState::GestureCancelled), "GestureCancelled");
    EXPECT_EQ(gesture_state_name(GestureState::Completed), "Completed");
}

TEST(TouchGestureStateTest, ValidTransitions) {
    TouchGestureStateMachine sm;
    EXPECT_TRUE(sm.is_idle());

    // Idle -> ContactPending -> Moving -> Completed -> Idle
    EXPECT_TRUE(sm.transition_to(GestureState::ContactPending));
    EXPECT_TRUE(sm.is_active());
    EXPECT_TRUE(sm.transition_to(GestureState::Moving));
    EXPECT_TRUE(sm.transition_to(GestureState::Completed));
    EXPECT_TRUE(sm.transition_to(GestureState::Idle));
    EXPECT_TRUE(sm.is_idle());

    // Idle -> ContactPending -> Resizing -> Completed -> Idle
    EXPECT_TRUE(sm.transition_to(GestureState::ContactPending));
    EXPECT_TRUE(sm.transition_to(GestureState::Resizing));
    EXPECT_TRUE(sm.transition_to(GestureState::Completed));
    EXPECT_TRUE(sm.transition_to(GestureState::Idle));

    // Idle -> ContactPending -> ControlPress -> Completed -> Idle
    EXPECT_TRUE(sm.transition_to(GestureState::ContactPending));
    EXPECT_TRUE(sm.transition_to(GestureState::ControlPress));
    EXPECT_TRUE(sm.transition_to(GestureState::Completed));
    EXPECT_TRUE(sm.transition_to(GestureState::Idle));

    // Cancellation paths
    EXPECT_TRUE(sm.transition_to(GestureState::ContactPending));
    EXPECT_TRUE(sm.transition_to(GestureState::GestureCancelled));
    EXPECT_TRUE(sm.transition_to(GestureState::Idle));

    EXPECT_TRUE(sm.transition_to(GestureState::ContactPending));
    EXPECT_TRUE(sm.transition_to(GestureState::Moving));
    EXPECT_TRUE(sm.transition_to(GestureState::GestureCancelled));
    EXPECT_TRUE(sm.transition_to(GestureState::Idle));
}

TEST(TouchGestureStateTest, InvalidTransitionsRejected) {
    TouchGestureStateMachine sm;

    // From Idle, cannot jump straight to Moving or Resizing or Completed
    EXPECT_FALSE(sm.transition_to(GestureState::Moving));
    EXPECT_EQ(sm.current_state(), GestureState::Idle);

    EXPECT_FALSE(sm.transition_to(GestureState::Resizing));
    EXPECT_EQ(sm.current_state(), GestureState::Idle);

    EXPECT_FALSE(sm.transition_to(GestureState::Completed));
    EXPECT_EQ(sm.current_state(), GestureState::Idle);

    // From Moving, cannot jump to Resizing
    EXPECT_TRUE(sm.transition_to(GestureState::ContactPending));
    EXPECT_TRUE(sm.transition_to(GestureState::Moving));
    EXPECT_FALSE(sm.transition_to(GestureState::Resizing));
    EXPECT_EQ(sm.current_state(), GestureState::Moving);
}

// =============================================================================
// 2. Touch Interaction Policy Tests
// =============================================================================

TEST(TouchInteractionPolicyTest, PolicyDefaultsAndMetrics) {
    config::Config cfg;
    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);

    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);
    EXPECT_TRUE(policy.touch_enabled);
    EXPECT_TRUE(policy.double_tap_enabled);
    EXPECT_EQ(policy.move_threshold_px, 10);
    EXPECT_EQ(policy.double_tap_interval_ms, 350u);
    EXPECT_EQ(policy.double_tap_slop_px, 16);
    EXPECT_EQ(policy.control_touch_target_px, 48);
    EXPECT_EQ(policy.resize_touch_target_px, 28);
    EXPECT_EQ(policy.header_touch_height_px, 44);
}

TEST(TouchInteractionPolicyTest, PolicyConfigOverrides) {
    config::Config cfg;
    cfg.set("input", "touch_move_threshold", "18");
    cfg.set("input", "touch_double_tap_timeout", "400");
    cfg.set("input", "touch_resize_target", "36");

    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);

    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);
    EXPECT_EQ(policy.move_threshold_px, 18);
    EXPECT_EQ(policy.double_tap_interval_ms, 400u);
    EXPECT_EQ(policy.resize_touch_target_px, 36);
}

// =============================================================================
// 3. Touch Hit Testing Tests
// =============================================================================

TEST(TouchHitTestingTest, HitTestWindowControlsAndEdges) {
    WindowRegistry registry;
    WindowStacking stacking;
    config::Config cfg;
    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);
    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);

    // Window at (100, 100), size (400, 500)
    core::Rect geom{100, 100, 400, 500};
    auto win = create_test_window(1, geom);
    registry.add_window(win);
    stacking.add(1);

    TouchHitTesting hit_tester(registry, stacking, policy);

    // 1. Close button: rightmost header target [100 + 400 - 48, 100] = [452, 100] size 48x48
    HitTestResult hit = hit_tester.hit_test(core::Point{460, 110});
    EXPECT_EQ(hit.type, HitTargetType::CloseControl);
    EXPECT_EQ(hit.window_id, 1u);

    // 2. Maximize button: [100 + 400 - 96, 100] = [404, 100] size 48x48
    hit = hit_tester.hit_test(core::Point{420, 110});
    EXPECT_EQ(hit.type, HitTargetType::MaximizeControl);
    EXPECT_EQ(hit.window_id, 1u);

    // 3. Minimize button: [100 + 400 - 144, 100] = [356, 100] size 48x48
    hit = hit_tester.hit_test(core::Point{370, 110});
    EXPECT_EQ(hit.type, HitTargetType::MinimizeControl);
    EXPECT_EQ(hit.window_id, 1u);

    // 4. Title bar drag area: (100 + 50, 110)
    hit = hit_tester.hit_test(core::Point{150, 110});
    EXPECT_EQ(hit.type, HitTargetType::TitleBar);
    EXPECT_EQ(hit.window_id, 1u);

    // 5. Window content: center of window (250, 300)
    hit = hit_tester.hit_test(core::Point{250, 300});
    EXPECT_EQ(hit.type, HitTargetType::WindowContent);
    EXPECT_EQ(hit.window_id, 1u);

    // 6. Right edge resize: x = 500 + 10 (within 28px margin of right border at x=500), y = 300
    hit = hit_tester.hit_test(core::Point{510, 300});
    EXPECT_EQ(hit.type, HitTargetType::ResizeEdge);
    EXPECT_EQ(hit.resize_edge, ResizeEdge::Right);

    // 7. Bottom edge resize: x = 300, y = 600 + 10 (bottom border at y=600)
    hit = hit_tester.hit_test(core::Point{300, 610});
    EXPECT_EQ(hit.type, HitTargetType::ResizeEdge);
    EXPECT_EQ(hit.resize_edge, ResizeEdge::Bottom);

    // 8. Bottom-Right corner resize: x = 510, y = 610
    hit = hit_tester.hit_test(core::Point{510, 610});
    EXPECT_EQ(hit.type, HitTargetType::ResizeEdge);
    EXPECT_EQ(hit.resize_edge, ResizeEdge::BottomRight);

    // 9. Outside window: (50, 50)
    hit = hit_tester.hit_test(core::Point{50, 50});
    EXPECT_EQ(hit.type, HitTargetType::None);
}

TEST(TouchHitTestingTest, OverlappingWindowsTopmostPriority) {
    WindowRegistry registry;
    WindowStacking stacking;
    config::Config cfg;
    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);
    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);

    // Window 1 at (50, 50, 300, 300)
    auto win1 = create_test_window(1, core::Rect{50, 50, 300, 300});
    // Window 2 at (100, 100, 300, 300), overlapping Window 1
    auto win2 = create_test_window(2, core::Rect{100, 100, 300, 300});

    registry.add_window(win1);
    registry.add_window(win2);
    stacking.add(1);
    stacking.add(2); // win2 on top

    TouchHitTesting hit_tester(registry, stacking, policy);

    // Touch at overlapping point (150, 150) -> must hit topmost win2
    HitTestResult hit = hit_tester.hit_test(core::Point{150, 150});
    EXPECT_EQ(hit.window_id, 2u);

    // Raise win1
    stacking.raise(1);
    hit = hit_tester.hit_test(core::Point{150, 150});
    EXPECT_EQ(hit.window_id, 1u);
}

TEST(TouchHitTestingTest, MinimizedWindowIgnored) {
    WindowRegistry registry;
    WindowStacking stacking;
    config::Config cfg;
    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);
    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);

    auto win1 = create_test_window(1, core::Rect{50, 50, 300, 300});
    registry.add_window(win1);
    stacking.add(1);

    TouchHitTesting hit_tester(registry, stacking, policy);

    // Normal state hits
    HitTestResult hit = hit_tester.hit_test(core::Point{100, 100});
    EXPECT_EQ(hit.window_id, 1u);

    // Minimize window -> must not be hit
    win1->set_state(WindowState::Minimized);
    hit = hit_tester.hit_test(core::Point{100, 100});
    EXPECT_EQ(hit.type, HitTargetType::None);
}

TEST(TouchHitTestingTest, FullscreenWindowSuppressesControls) {
    WindowRegistry registry;
    WindowStacking stacking;
    config::Config cfg;
    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);
    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);

    auto win1 = create_test_window(1, core::Rect{0, 0, 720, 1280});
    win1->set_state(WindowState::Fullscreen);
    registry.add_window(win1);
    stacking.add(1);

    TouchHitTesting hit_tester(registry, stacking, policy);

    // Touch at top-right corner where close button would normally be
    HitTestResult hit = hit_tester.hit_test(core::Point{700, 20});
    EXPECT_EQ(hit.type, HitTargetType::WindowContent); // Not CloseControl!
    EXPECT_EQ(hit.window_id, 1u);
}

TEST(TouchHitTestingTest, MaximizedWindowSuppressesResizeEdges) {
    WindowRegistry registry;
    WindowStacking stacking;
    config::Config cfg;
    DisplayInfo disp = create_mobile_display();
    DisplayPolicy dp(disp);
    TouchInteractionPolicy policy = TouchInteractionPolicy::from_config_and_display(cfg, dp);

    auto win1 = create_test_window(1, core::Rect{0, 0, 720, 1200});
    win1->set_state(WindowState::Maximized);
    registry.add_window(win1);
    stacking.add(1);

    TouchHitTesting hit_tester(registry, stacking, policy);

    // Touch at bottom edge -> should be WindowContent, not ResizeEdge!
    HitTestResult hit = hit_tester.hit_test(core::Point{360, 1195});
    EXPECT_EQ(hit.type, HitTargetType::WindowContent);

    // Close control still active
    hit = hit_tester.hit_test(core::Point{700, 20});
    EXPECT_EQ(hit.type, HitTargetType::CloseControl);
}

// =============================================================================
// 4. Touch-to-Move and Tap vs Drag Arbitration Tests
// =============================================================================

TEST(TouchInteractionTest, TapOnTitleBarActivatesWindow) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win1 = create_test_window(1, core::Rect{50, 50, 200, 200});
    auto win2 = create_test_window(2, core::Rect{300, 300, 200, 200});
    registry.add_window(win1);
    registry.add_window(win2);
    win1->set_geometry(core::Rect{50, 50, 200, 200});
    win2->set_geometry(core::Rect{300, 300, 200, 200});
    wm.activate(1);

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // win1 is active. Tap on win2 title bar at (350, 320)
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{350, 320}, 1000));
    EXPECT_EQ(touch_mgr.state(), GestureState::ContactPending);

    // Touch up without movement
    EXPECT_TRUE(touch_mgr.handle_touch_up(0, 1050));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);

    // win2 should now be active!
    EXPECT_EQ(wm.active_window_id(), 2u);
    EXPECT_EQ(win2->geometry(), (core::Rect{300, 300, 200, 200}));
}

TEST(TouchInteractionTest, DragOnTitleBarMovesWindow) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Touch down on title bar at (150, 120)
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{150, 120}, 1000));
    EXPECT_EQ(touch_mgr.state(), GestureState::ContactPending);

    // Small movement below threshold (5px) -> still ContactPending
    EXPECT_TRUE(touch_mgr.handle_touch_motion(0, core::Point{153, 124}, 1010));
    EXPECT_EQ(touch_mgr.state(), GestureState::ContactPending);
    EXPECT_EQ(win->geometry().x, 100);

    // Movement exceeding threshold (dx = +30, dy = +40) -> transitions to Moving
    EXPECT_TRUE(touch_mgr.handle_touch_motion(0, core::Point{180, 160}, 1020));
    EXPECT_EQ(touch_mgr.state(), GestureState::Moving);
    EXPECT_TRUE(touch_mgr.drag_controller().is_active());

    // Window geometry updated during drag
    EXPECT_EQ(touch_mgr.drag_controller().current_geometry().x, 130);
    EXPECT_EQ(touch_mgr.drag_controller().current_geometry().y, 140);

    // Complete drag
    EXPECT_TRUE(touch_mgr.handle_touch_up(0, 1050));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);
    EXPECT_EQ(win->geometry().x, 130);
    EXPECT_EQ(win->geometry().y, 140);
}

TEST(TouchInteractionTest, DoubleTapTitleBarTogglesMaximize) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // First tap on title bar at (150, 120)
    touch_mgr.handle_touch_down(0, core::Point{150, 120}, 1000);
    touch_mgr.handle_touch_up(0, 1050);
    EXPECT_EQ(win->state(), WindowState::Normal);

    // Second tap 150ms later within slop (152, 121)
    touch_mgr.handle_touch_down(0, core::Point{152, 121}, 1200);
    touch_mgr.handle_touch_up(0, 1250);

    // Window should now be Maximized!
    EXPECT_EQ(win->state(), WindowState::Maximized);

    // Another double tap restores the window
    int title_y = win->geometry().y + 20;
    touch_mgr.handle_touch_down(0, core::Point{150, title_y}, 1500);
    touch_mgr.handle_touch_up(0, 1550);
    touch_mgr.handle_touch_down(0, core::Point{150, title_y}, 1700);
    touch_mgr.handle_touch_up(0, 1750);
    EXPECT_EQ(win->state(), WindowState::Normal);
}

// =============================================================================
// 5. Touch-to-Resize Tests
// =============================================================================

TEST(TouchInteractionTest, DragOnResizeEdgeResizesWindow) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Touch down on right edge: x = 400, y = 200
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{400, 200}, 1000));
    EXPECT_EQ(touch_mgr.state(), GestureState::ContactPending);

    // Drag to the right: dx = +50
    EXPECT_TRUE(touch_mgr.handle_touch_motion(0, core::Point{450, 200}, 1020));
    EXPECT_EQ(touch_mgr.state(), GestureState::Resizing);
    EXPECT_TRUE(touch_mgr.resize_controller().is_active());
    EXPECT_EQ(touch_mgr.resize_controller().current_geometry().width, 350);

    // Finish resize
    EXPECT_TRUE(touch_mgr.handle_touch_up(0, 1050));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);
    EXPECT_EQ(win->geometry().width, 350);
}

TEST(TouchInteractionTest, ResizeCornerCombinesDirections) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Touch down on Bottom-Right corner: (400, 400)
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{400, 400}, 1000));
    // Drag dx = +40, dy = +60
    EXPECT_TRUE(touch_mgr.handle_touch_motion(0, core::Point{440, 460}, 1020));
    EXPECT_EQ(touch_mgr.state(), GestureState::Resizing);
    EXPECT_EQ(touch_mgr.resize_controller().current_geometry().width, 340);
    EXPECT_EQ(touch_mgr.resize_controller().current_geometry().height, 360);

    touch_mgr.handle_touch_up(0, 1050);
    EXPECT_EQ(win->geometry().width, 340);
    EXPECT_EQ(win->geometry().height, 360);
}

// =============================================================================
// 6. Window Controls Touch Interaction Tests
// =============================================================================

TEST(TouchInteractionTest, TapCloseControlClosesWindow) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Close button at [100 + 300 - 48, 100] = [352, 100, 48, 44]
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{370, 120}, 1000));
    EXPECT_EQ(touch_mgr.state(), GestureState::ControlPress);
    EXPECT_TRUE(touch_mgr.control_interaction().is_pressed());

    // Release inside close button
    EXPECT_TRUE(touch_mgr.handle_touch_up(0, 1050));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);

    // Window should be removed or close requested
    EXPECT_EQ(registry.count(), 0u);
}

TEST(TouchInteractionTest, DragOutsideControlCancelsAction) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Touch down on Close button at (370, 120)
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{370, 120}, 1000));
    EXPECT_EQ(touch_mgr.state(), GestureState::ControlPress);

    // Drag away from button to (200, 200)
    touch_mgr.handle_touch_motion(0, core::Point{200, 200}, 1020);
    EXPECT_FALSE(touch_mgr.control_interaction().is_pressed());

    // Release at (200, 200) -> cancelled, window is NOT closed!
    EXPECT_TRUE(touch_mgr.handle_touch_up(0, 1050));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);
    EXPECT_EQ(registry.count(), 1u);
}

TEST(TouchInteractionTest, TapMinimizeAndMaximizeControls) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Maximize button: [100 + 300 - 96, 100] = [304, 100, 48, 44] -> center (328, 120)
    touch_mgr.handle_touch_down(0, core::Point{328, 120}, 1000);
    touch_mgr.handle_touch_up(0, 1050);
    EXPECT_EQ(win->state(), WindowState::Maximized);

    // Minimize button when maximized: [540 - 144, win->geometry().y] -> center (420, win->geometry().y + 22)
    int min_y = win->geometry().y + 22;
    touch_mgr.handle_touch_down(0, core::Point{420, min_y}, 1100);
    touch_mgr.handle_touch_up(0, 1150);
    EXPECT_EQ(win->state(), WindowState::Minimized);
}

// =============================================================================
// 7. Cancellation & Lifecycle Tests
// =============================================================================

TEST(TouchInteractionTest, TouchCancelRestoresGeometry) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Start drag
    touch_mgr.handle_touch_down(0, core::Point{150, 120}, 1000);
    touch_mgr.handle_touch_motion(0, core::Point{250, 220}, 1020);
    EXPECT_EQ(touch_mgr.state(), GestureState::Moving);

    // Protocol Cancel received (e.g. compositor cancel or phone palm rejection)
    EXPECT_TRUE(touch_mgr.handle_touch_cancel(0));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);

    // Window must be back to initial geometry
    EXPECT_EQ(win->geometry().x, 100);
    EXPECT_EQ(win->geometry().y, 100);
}

TEST(TouchInteractionTest, WindowDestroyedDuringInteractionCancelsCleanly) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Start move
    touch_mgr.handle_touch_down(0, core::Point{150, 120}, 1000);
    touch_mgr.handle_touch_motion(0, core::Point{200, 170}, 1020);
    EXPECT_EQ(touch_mgr.state(), GestureState::Moving);

    // Window is destroyed (client crash or closed externally)
    touch_mgr.handle_window_destroyed(1);

    // State machine must return to Idle cleanly without crashing
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);
    EXPECT_FALSE(touch_mgr.drag_controller().is_active());
}

TEST(TouchInteractionTest, DisplayChangeDuringInteractionCancelsCleanly) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Start move
    touch_mgr.handle_touch_down(0, core::Point{150, 120}, 1000);
    touch_mgr.handle_touch_motion(0, core::Point{200, 170}, 1020);
    EXPECT_EQ(touch_mgr.state(), GestureState::Moving);

    // Phone rotates to landscape: 1200x540
    DisplayInfo rotated = disp;
    rotated.width = 1200;
    rotated.height = 540;
    rotated.logical_width = 1200;
    rotated.logical_height = 540;
    rotated.orientation = Orientation::Landscape;

    DisplayPolicy rotated_policy(rotated);
    touch_mgr.handle_display_change(rotated_policy);

    // Must be cancelled to avoid coordinate corruption
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);
    EXPECT_FALSE(touch_mgr.drag_controller().is_active());
}

// =============================================================================
// 8. Contact Ownership and Multi-Touch
// =============================================================================

TEST(TouchInteractionTest, MultiTouchContactOwnership) {
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager disp_mgr;
    auto backend = std::make_unique<DummyBackend>();
    backend->registry = &registry;
    WindowManager wm(registry, tracker, disp_mgr, std::move(backend));
    config::Config cfg;
    wm.initialize(cfg);

    DisplayInfo disp = create_mobile_display();
    disp_mgr.register_synthetic_display(disp);

    auto win = create_test_window(1, core::Rect{100, 100, 300, 300});
    registry.add_window(win);
    win->set_geometry(core::Rect{100, 100, 300, 300});

    TouchInteractionManager touch_mgr(wm, registry, disp_mgr, cfg);

    // Touch 0 begins interaction
    EXPECT_TRUE(touch_mgr.handle_touch_down(0, core::Point{150, 120}, 1000));
    EXPECT_EQ(touch_mgr.active_touch_id(), 0);

    // Touch 1 down while Touch 0 is active -> safely ignored!
    EXPECT_FALSE(touch_mgr.handle_touch_down(1, core::Point{200, 200}, 1010));
    EXPECT_EQ(touch_mgr.active_touch_id(), 0);

    // Touch 1 motion -> ignored
    EXPECT_FALSE(touch_mgr.handle_touch_motion(1, core::Point{250, 250}, 1020));

    // Touch 0 motion -> processed
    EXPECT_TRUE(touch_mgr.handle_touch_motion(0, core::Point{180, 150}, 1025));

    // Touch 1 up -> ignored
    EXPECT_FALSE(touch_mgr.handle_touch_up(1, 1030));
    EXPECT_EQ(touch_mgr.state(), GestureState::Moving);

    // Touch 0 up -> completes interaction
    EXPECT_TRUE(touch_mgr.handle_touch_up(0, 1040));
    EXPECT_EQ(touch_mgr.state(), GestureState::Idle);
    EXPECT_FALSE(touch_mgr.active_touch_id().has_value());
}
