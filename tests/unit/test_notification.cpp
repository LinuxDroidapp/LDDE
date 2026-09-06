#include <gtest/gtest.h>
#include "ldde/notification/notification.hpp"
#include "ldde/notification/notification_store.hpp"
#include "ldde/notification/notification_center_state.hpp"
#include "ldde/notification/notification_layout.hpp"
#include "ldde/notification/notification_presenter.hpp"
#include "ldde/notification/notification_controller.hpp"
#include "ldde/notification/internal_notification_backend.hpp"
#include "ldde/notification/notification_manager.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shell_layout.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/config/config.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_manager.hpp"

using namespace ldde;
using namespace ldde::notification;

namespace {

display::DisplayPolicy create_test_policy(int32_t width = 720, int32_t height = 1280) {
    display::DisplayInfo info;
    info.id = 1;
    info.name = "WL-1";
    info.width = width;
    info.height = height;
    info.pixel_width = width;
    info.pixel_height = height;
    info.logical_width = width;
    info.logical_height = height;
    return display::DisplayPolicy(info);
}

} // namespace

// ============================================================================
// 1. Notification Model & Sanitization Tests
// ============================================================================

TEST(NotificationModelTest, ConstructionAndGetters) {
    Notification notif(
        101,
        "ChatApp",
        "Alice",
        "Hello there!",
        "chat-icon",
        NotificationUrgency::Critical,
        5000,
        0
    );
    notif.add_action("reply", "Reply");
    notif.add_action("archive", "Archive");
    notif.set_hint("desktop-entry", "org.example.Chat");

    EXPECT_EQ(notif.id(), 101);
    EXPECT_EQ(notif.app_name(), "ChatApp");
    EXPECT_EQ(notif.summary(), "Alice");
    EXPECT_EQ(notif.body(), "Hello there!");
    EXPECT_EQ(notif.icon(), "chat-icon");
    EXPECT_EQ(notif.urgency(), NotificationUrgency::Critical);
    EXPECT_EQ(notif.actions().size(), 2);
    EXPECT_EQ(notif.actions()[0].key, "reply");
    EXPECT_EQ(notif.actions()[1].label, "Archive");
    EXPECT_EQ(notif.hints().size(), 1);
    EXPECT_EQ(notif.get_hint("desktop-entry"), "org.example.Chat");
    EXPECT_EQ(notif.expire_timeout_ms(), 5000);
    EXPECT_EQ(notif.replaces_id(), 0);
    EXPECT_EQ(notif.state(), NotificationLifecycleState::Received);
}

TEST(NotificationModelTest, TextSanitization) {
    Notification notif(
        1,
        "<b>Web Browser</b>\n",
        "<i>Download</i> Finished!\r\n",
        "<a href='http://example.com'>Click <b>here</b></a> to view file.   ",
        "icon",
        NotificationUrgency::Normal
    );

    EXPECT_EQ(notif.app_name(), "Web Browser");
    EXPECT_EQ(notif.summary(), "Download Finished!");
    EXPECT_EQ(notif.body(), "Click here to view file.");
}

TEST(NotificationModelTest, StringClamping) {
    std::string huge_summary(300, 'A');
    std::string huge_body(3000, 'B');

    Notification notif(
        2,
        "App",
        huge_summary,
        huge_body,
        "icon",
        NotificationUrgency::Normal
    );

    EXPECT_LE(notif.summary().length(), 128u);
    EXPECT_LE(notif.body().length(), 1024u);
}

TEST(NotificationModelTest, UpdateFromPreservesTimestamp) {
    Notification original(1, "App", "Title 1", "Body 1", "icon1", NotificationUrgency::Normal);
    auto orig_timestamp = original.timestamp();

    Notification updated(2, "App", "Title 2", "Body 2", "icon2", NotificationUrgency::Critical);
    updated.set_replaces_id(1);

    original.update_from(updated);

    EXPECT_EQ(original.id(), 1);
    EXPECT_EQ(original.summary(), "Title 2");
    EXPECT_EQ(original.body(), "Body 2");
    EXPECT_EQ(original.icon(), "icon2");
    EXPECT_EQ(original.urgency(), NotificationUrgency::Critical);
    EXPECT_EQ(original.timestamp(), orig_timestamp);
}

// ============================================================================
// 2. NotificationStore Tests
// ============================================================================

TEST(NotificationStoreTest, AddAndRetrieve) {
    NotificationStore store(50, 5);

    Notification n1(kInvalidNotificationId, "App1", "Sum1", "Body1", "icon1", NotificationUrgency::Normal);
    NotificationId id1 = store.add_or_replace(n1);
    EXPECT_GT(id1, 0u);

    Notification n2(kInvalidNotificationId, "App2", "Sum2", "Body2", "icon2", NotificationUrgency::Low);
    NotificationId id2 = store.add_or_replace(n2);
    EXPECT_GT(id2, id1);

    EXPECT_EQ(store.active_count(), 2u);
    EXPECT_EQ(store.history_count(), 0u);
    EXPECT_EQ(store.total_count(), 2u);

    const Notification* found = store.find(id1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->summary(), "Sum1");
}

TEST(NotificationStoreTest, CloseMovesToHistory) {
    NotificationStore store(50, 5);

    Notification n(kInvalidNotificationId, "App", "Sum", "Body", "icon", NotificationUrgency::Normal);
    NotificationId id = store.add_or_replace(n);

    bool closed = store.close(id, NotificationCloseReason::Dismissed);
    EXPECT_TRUE(closed);

    EXPECT_EQ(store.active_count(), 0u);
    EXPECT_EQ(store.history_count(), 1u);

    const Notification* hist = store.find(id);
    ASSERT_NE(hist, nullptr);
    EXPECT_EQ(hist->state(), NotificationLifecycleState::Closed);
}

TEST(NotificationStoreTest, ReplaceExistingInStore) {
    NotificationStore store(50, 5);

    Notification n1(kInvalidNotificationId, "Music", "Track 1", "Artist A", "music", NotificationUrgency::Normal);
    NotificationId id1 = store.add_or_replace(n1);

    Notification n2(kInvalidNotificationId, "Music", "Track 2", "Artist B", "music", NotificationUrgency::Normal);
    n2.set_replaces_id(id1);
    NotificationId id2 = store.add_or_replace(n2);

    EXPECT_EQ(id2, id1);
    EXPECT_EQ(store.active_count(), 1u);

    const Notification* found = store.find(id1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->summary(), "Track 2");
}

TEST(NotificationStoreTest, PerAppFloodLimit) {
    size_t app_limit = 3;
    NotificationStore store(50, app_limit);

    NotificationId first_id = 0;
    for (size_t i = 0; i < 5; ++i) {
        Notification n(kInvalidNotificationId, "SpamApp", "Spam " + std::to_string(i), "Body", "", NotificationUrgency::Low);
        NotificationId id = store.add_or_replace(n);
        if (i == 0) first_id = id;
    }

    // SpamApp should have at most app_limit (3) active notifications
    auto active = store.active_notifications();
    size_t count_spam = 0;
    for (const auto* notif : active) {
        if (notif && notif->app_name() == "SpamApp") count_spam++;
    }
    EXPECT_LE(count_spam, app_limit);

    // Oldest active was pruned to history
    const auto* first_notif = store.find(first_id);
    ASSERT_NE(first_notif, nullptr);
    EXPECT_EQ(first_notif->state(), NotificationLifecycleState::Expired);
}

TEST(NotificationStoreTest, HistoryCapacityPruning) {
    size_t max_history = 5;
    NotificationStore store(max_history, 10);

    for (size_t i = 0; i < 10; ++i) {
        Notification n(kInvalidNotificationId, "App", "N " + std::to_string(i), "Body", "", NotificationUrgency::Normal);
        NotificationId id = store.add_or_replace(n);
        store.close(id, NotificationCloseReason::Dismissed);
    }

    EXPECT_EQ(store.active_count(), 0u);
    EXPECT_LE(store.history_count(), max_history);
}

TEST(NotificationStoreTest, ClearHistory) {
    NotificationStore store(50, 5);
    for (size_t i = 0; i < 4; ++i) {
        Notification n(kInvalidNotificationId, "App", "N " + std::to_string(i), "Body", "", NotificationUrgency::Normal);
        NotificationId id = store.add_or_replace(n);
        store.close(id, NotificationCloseReason::Dismissed);
    }
    EXPECT_EQ(store.history_count(), 4u);

    store.clear_history();
    EXPECT_EQ(store.history_count(), 0u);
}

// ============================================================================
// 3. NotificationCenterStateMachine Tests
// ============================================================================

TEST(NotificationCenterStateMachineTest, ValidTransitions) {
    NotificationCenterStateMachine sm;
    EXPECT_EQ(sm.current_state(), NotificationCenterState::Closed);
    EXPECT_FALSE(sm.is_open());

    EXPECT_TRUE(sm.transition_to(NotificationCenterState::Opening));
    EXPECT_EQ(sm.current_state(), NotificationCenterState::Opening);

    EXPECT_TRUE(sm.transition_to(NotificationCenterState::Open));
    EXPECT_EQ(sm.current_state(), NotificationCenterState::Open);
    EXPECT_TRUE(sm.is_open());

    EXPECT_TRUE(sm.transition_to(NotificationCenterState::Closing));
    EXPECT_EQ(sm.current_state(), NotificationCenterState::Closing);

    EXPECT_TRUE(sm.transition_to(NotificationCenterState::Closed));
    EXPECT_EQ(sm.current_state(), NotificationCenterState::Closed);
    EXPECT_FALSE(sm.is_open());
}

TEST(NotificationCenterStateMachineTest, DirectOpenAndClose) {
    NotificationCenterStateMachine sm;
    EXPECT_TRUE(sm.transition_to(NotificationCenterState::Open));
    EXPECT_TRUE(sm.is_open());
    EXPECT_TRUE(sm.transition_to(NotificationCenterState::Closed));
    EXPECT_FALSE(sm.is_open());
}

TEST(NotificationCenterStateMachineTest, CallbackInvocation) {
    NotificationCenterStateMachine sm;
    bool called = false;
    sm.on_state_changed([&called](NotificationCenterState old_s, NotificationCenterState new_s) {
        called = true;
        EXPECT_EQ(old_s, NotificationCenterState::Closed);
        EXPECT_EQ(new_s, NotificationCenterState::Open);
    });

    sm.transition_to(NotificationCenterState::Open);
    EXPECT_TRUE(called);
}

// ============================================================================
// 4. NotificationLayout & Hit-Testing Tests
// ============================================================================

TEST(NotificationLayoutTest, PopupLayoutPortrait) {
    auto policy = create_test_policy(720, 1280);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy.display_info(), tokens, shell::DockPosition::Bottom);

    Notification n1(1, "App1", "Title 1", "Body 1", "icon", NotificationUrgency::Normal);
    Notification n2(2, "App2", "Title 2", "Body 2", "icon", NotificationUrgency::Normal);
    std::vector<Notification*> popups = {&n1, &n2};

    NotificationLayout layout;
    layout.update_popups(policy, shell_layout, tokens, popups);

    EXPECT_GT(n1.popup_geometry().width, 300);
    EXPECT_GT(n1.popup_geometry().height, 50);
    EXPECT_GE(n1.popup_geometry().y, shell_layout.status_geometry().y + shell_layout.status_geometry().height);
    EXPECT_GT(n2.popup_geometry().y, n1.popup_geometry().y); // n2 stacked below n1
}

TEST(NotificationLayoutTest, HitTestingPopup) {
    auto policy = create_test_policy(720, 1280);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy.display_info(), tokens, shell::DockPosition::Bottom);

    Notification n(
        10,
        "App",
        "Title",
        "Body",
        "icon",
        NotificationUrgency::Normal
    );
    n.add_action("btn1", "Action1");
    std::vector<Notification*> popups = {&n};

    NotificationLayout layout;
    layout.update_popups(policy, shell_layout, tokens, popups);

    std::vector<const Notification*> const_popups = {&n};

    // Tap in card body
    int32_t body_x = n.popup_geometry().x + 30;
    int32_t body_y = n.popup_geometry().y + 20;
    auto hit_body = layout.hit_test_popups(body_x, body_y, const_popups);
    EXPECT_EQ(hit_body.id, 10u);
    EXPECT_FALSE(hit_body.is_dismiss);
    EXPECT_FALSE(hit_body.is_action);

    // Tap on dismiss button
    int32_t close_x = n.dismiss_button_geometry().x + n.dismiss_button_geometry().width / 2;
    int32_t close_y = n.dismiss_button_geometry().y + n.dismiss_button_geometry().height / 2;
    auto hit_close = layout.hit_test_popups(close_x, close_y, const_popups);
    EXPECT_EQ(hit_close.id, 10u);
    EXPECT_TRUE(hit_close.is_dismiss);

    // Tap outside
    auto hit_outside = layout.hit_test_popups(0, 0, const_popups);
    EXPECT_EQ(hit_outside.id, kInvalidNotificationId);
}

TEST(NotificationLayoutTest, NotificationCenterLayoutAndHitTest) {
    auto policy = create_test_policy(720, 1280);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy.display_info(), tokens, shell::DockPosition::Bottom);

    Notification n1(1, "App1", "Title 1", "Body 1", "", NotificationUrgency::Normal);
    Notification n2(2, "App2", "Title 2", "Body 2", "", NotificationUrgency::Normal);
    std::vector<const Notification*> all_notifs = {&n1, &n2};

    NotificationLayout layout;
    layout.update_notification_center(policy, shell_layout, tokens, all_notifs, 0);

    const auto& center_geom = layout.center_panel_geometry();
    EXPECT_GT(center_geom.width, 300);
    EXPECT_GT(center_geom.height, 400);

    // Outside tap
    auto hit_outside = layout.hit_test_center(10, 10, all_notifs);
    EXPECT_FALSE(hit_outside.is_inside);

    // Close button tap
    int32_t cx = layout.center_close_geometry().x + 5;
    int32_t cy = layout.center_close_geometry().y + 5;
    auto hit_close = layout.hit_test_center(cx, cy, all_notifs);
    EXPECT_TRUE(hit_close.is_close);

    // Clear all button tap
    int32_t clr_x = layout.center_clear_all_geometry().x + 5;
    int32_t clr_y = layout.center_clear_all_geometry().y + 5;
    auto hit_clear = layout.hit_test_center(clr_x, clr_y, all_notifs);
    EXPECT_TRUE(hit_clear.is_clear_all);
}

// ============================================================================
// 5. NotificationPresenter Tests
// ============================================================================

TEST(NotificationPresenterTest, ShowAndDismissQueue) {
    NotificationStore store(50, 5);
    core::EventLoop loop;
    loop.initialize();

    NotificationPresenter presenter(store, &loop, 2, 5000);

    Notification n1(kInvalidNotificationId, "App1", "T1", "B1", "", NotificationUrgency::Normal);
    Notification n2(kInvalidNotificationId, "App2", "T2", "B2", "", NotificationUrgency::Normal);
    Notification n3(kInvalidNotificationId, "App3", "T3", "B3", "", NotificationUrgency::Normal);

    NotificationId id1 = store.add_or_replace(n1);
    NotificationId id2 = store.add_or_replace(n2);
    NotificationId id3 = store.add_or_replace(n3);

    presenter.show(id1);
    presenter.show(id2);
    EXPECT_EQ(presenter.visible_popups().size(), 2u);

    // Exceeding max_visible_popups (2) keeps count at 2, replacing oldest
    presenter.show(id3);
    EXPECT_EQ(presenter.visible_popups().size(), 2u);

    // Dismiss id3
    presenter.dismiss(id3);
    EXPECT_FALSE(std::any_of(presenter.visible_popups().begin(), presenter.visible_popups().end(),
                             [id3](const Notification* n) { return n && n->id() == id3; }));

    while (!presenter.visible_popups().empty()) {
        presenter.dismiss(presenter.visible_popups().front()->id());
    }
    EXPECT_EQ(presenter.visible_popups().size(), 0u);
}

TEST(NotificationPresenterTest, RenderRequestTriggeredOnShowDismiss) {
    NotificationStore store(50, 5);
    core::EventLoop loop;
    loop.initialize();

    NotificationPresenter presenter(store, &loop, 3, 5000);
    int render_requests = 0;
    presenter.on_request_render([&render_requests]() {
        render_requests++;
    });

    Notification n(kInvalidNotificationId, "App", "T", "B", "", NotificationUrgency::Normal);
    NotificationId id = store.add_or_replace(n);

    presenter.show(id);
    EXPECT_EQ(render_requests, 1);

    presenter.dismiss(id);
    EXPECT_EQ(render_requests, 2);
}

// ============================================================================
// 6. NotificationController Tests
// ============================================================================

TEST(NotificationControllerTest, TouchDownAndUpDefaultActivation) {
    NotificationStore store(50, 5);
    core::EventLoop loop;
    loop.initialize();
    NotificationPresenter presenter(store, &loop, 3, 5000);
    NotificationCenterStateMachine center_state;
    NotificationLayout layout;

    NotificationController controller(store, presenter, center_state, layout);

    Notification n(
        kInvalidNotificationId,
        "Email",
        "New mail",
        "Meeting at 2pm",
        "mail-icon",
        NotificationUrgency::Normal
    );
    NotificationId id = store.add_or_replace(std::move(n));
    presenter.show(id);

    auto policy = create_test_policy(720, 1280);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy.display_info(), tokens, shell::DockPosition::Bottom);
    layout.update_popups(policy, shell_layout, tokens, presenter.visible_popups());

    ASSERT_FALSE(presenter.visible_popups().empty());
    const auto* vis = presenter.visible_popups()[0];
    int32_t tap_x = vis->popup_geometry().x + 20;
    int32_t tap_y = vis->popup_geometry().y + 20;

    NotificationId default_activated_id = kInvalidNotificationId;
    controller.on_default_activated([&default_activated_id](NotificationId act_id) {
        default_activated_id = act_id;
    });

    EXPECT_TRUE(controller.handle_touch_down(tap_x, tap_y));
    EXPECT_TRUE(controller.handle_touch_up(tap_x, tap_y));

    EXPECT_EQ(default_activated_id, id);
    // Transient popup was dismissed on activation
    EXPECT_EQ(presenter.visible_popups().size(), 0u);
}

TEST(NotificationControllerTest, SwipeDismissGesture) {
    NotificationStore store(50, 5);
    core::EventLoop loop;
    loop.initialize();
    NotificationPresenter presenter(store, &loop, 3, 5000);
    NotificationCenterStateMachine center_state;
    NotificationLayout layout;

    NotificationController controller(store, presenter, center_state, layout);

    Notification n(kInvalidNotificationId, "App", "Title", "Body", "", NotificationUrgency::Normal);
    NotificationId id = store.add_or_replace(std::move(n));
    presenter.show(id);

    auto policy = create_test_policy(720, 1280);
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);
    shell::ShellLayout shell_layout;
    shell_layout.update(policy.display_info(), tokens, shell::DockPosition::Bottom);
    layout.update_popups(policy, shell_layout, tokens, presenter.visible_popups());

    ASSERT_FALSE(presenter.visible_popups().empty());
    const auto* vis = presenter.visible_popups()[0];
    int32_t start_x = vis->popup_geometry().x + 20;
    int32_t start_y = vis->popup_geometry().y + 20;

    EXPECT_TRUE(controller.handle_touch_down(start_x, start_y));
    // Swipe horizontally > 40px
    EXPECT_TRUE(controller.handle_touch_motion(start_x + 60, start_y));
    EXPECT_TRUE(controller.handle_touch_up(start_x + 60, start_y));

    // Successfully dismissed via swipe gesture
    EXPECT_EQ(presenter.visible_popups().size(), 0u);
}

TEST(NotificationControllerTest, KeyboardEscapeCloses) {
    NotificationStore store(50, 5);
    core::EventLoop loop;
    loop.initialize();
    NotificationPresenter presenter(store, &loop, 3, 5000);
    NotificationCenterStateMachine center_state;
    NotificationLayout layout;

    NotificationController controller(store, presenter, center_state, layout);

    center_state.transition_to(NotificationCenterState::Open);
    EXPECT_TRUE(center_state.is_open());

    // Press Escape (0xff1b)
    EXPECT_TRUE(controller.handle_key(0xff1b, 1, 0));
    EXPECT_FALSE(center_state.is_open());
}

// ============================================================================
// 7. InternalNotificationBackend Tests
// ============================================================================

TEST(InternalNotificationBackendTest, ProtocolSimulation) {
    InternalNotificationBackend backend;
    auto s = backend.start();
    EXPECT_TRUE(s.is_ok());

    EXPECT_EQ(backend.backend_name(), "internal");
    EXPECT_TRUE(backend.is_connected());

    bool received = false;
    backend.on_notification_received([&received](Notification notif) {
        received = true;
        EXPECT_EQ(notif.app_name(), "TestApp");
        EXPECT_EQ(notif.summary(), "Test Summary");
        return 42u;
    });

    Notification notif(0, "TestApp", "Test Summary", "Test Body", "", NotificationUrgency::Normal);
    NotificationId res_id = backend.post_notification(std::move(notif));
    EXPECT_EQ(res_id, 42u);
    EXPECT_TRUE(received);

    backend.emit_notification_closed(42u, NotificationCloseReason::Dismissed);
    EXPECT_EQ(backend.last_closed_id(), 42u);
    EXPECT_EQ(backend.last_closed_reason(), NotificationCloseReason::Dismissed);

    backend.emit_action_invoked(42u, "default");
    EXPECT_EQ(backend.last_action_id(), 42u);
    EXPECT_EQ(backend.last_action_key(), "default");

    backend.stop();
    EXPECT_FALSE(backend.is_connected());
}

// ============================================================================
// 8. NotificationManager Facade Tests
// ============================================================================

TEST(NotificationManagerTest, LifecycleAndSystemNotifications) {
    NotificationManager manager;
    shell::Shell shell;
    window::WindowRegistry reg;
    window::WindowTracker tracker;
    display::DisplayManager dm;
    window::WindowManager wm(reg, tracker, dm, nullptr);
    application::ApplicationCatalog catalog;
    auto policy = create_test_policy(720, 1280);
    config::Config config;
    config.load_defaults();
    core::EventLoop loop;
    loop.initialize();

    auto backend = std::make_unique<InternalNotificationBackend>();

    auto s = manager.initialize(shell, wm, catalog, policy, config, loop, std::move(backend));
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(manager.is_initialized());

    NotificationId sys_id = manager.post_system_notification("Low Battery", "15% remaining", NotificationUrgency::Critical);
    EXPECT_GT(sys_id, 0u);
    EXPECT_TRUE(manager.has_visible_popups());

    const auto* found = manager.store().find(sys_id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->summary(), "Low Battery");
    EXPECT_EQ(found->urgency(), NotificationUrgency::Critical);

    manager.open_notification_center();
    EXPECT_TRUE(manager.is_notification_center_open());

    manager.toggle_notification_center();
    EXPECT_FALSE(manager.is_notification_center_open());

    manager.close_notification(sys_id);
    EXPECT_FALSE(manager.has_visible_popups());

    manager.shutdown();
    EXPECT_FALSE(manager.is_initialized());
}
