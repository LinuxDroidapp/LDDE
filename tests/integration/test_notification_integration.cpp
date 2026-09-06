#include <gtest/gtest.h>
#include "ldde/notification/notification_manager.hpp"
#include "ldde/notification/internal_notification_backend.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/desktop/desktop.hpp"
#include "ldde/system/system_ui.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/config/config.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_manager.hpp"

using namespace ldde;
using namespace ldde::notification;

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
    return display::DisplayPolicy(info);
}

} // namespace

class NotificationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy_ = make_policy(720, 1280);
        config_.load_defaults();
        loop_.initialize();

        wm_ = std::make_unique<window::WindowManager>(
            win_reg_, win_tracker_, disp_mgr_, std::make_unique<DummyWMBackend>());
        wm_->initialize(config_);

        launcher_.initialize(catalog_, policy_, config_);
        dock_.initialize(catalog_, win_reg_, *wm_, launcher_, policy_, config_);
        switcher_.initialize(catalog_, win_reg_, *wm_, policy_, config_);
        system_ui_.initialize(shell_, policy_, config_);

        auto backend = std::make_unique<InternalNotificationBackend>();
        backend_ptr_ = backend.get();

        notif_mgr_.initialize(
            shell_,
            *wm_,
            catalog_,
            policy_,
            config_,
            loop_,
            std::move(backend));

        // Connect mutual exclusion as in Application
        notif_mgr_.center_state().on_state_changed([this](NotificationCenterState /*old_state*/, NotificationCenterState new_state) {
            if (new_state == NotificationCenterState::Opening || new_state == NotificationCenterState::Open) {
                if (launcher_.is_open()) launcher_.close();
                if (switcher_.is_open()) switcher_.close();
                if (system_ui_.is_panel_open()) system_ui_.close_panel();
            }
        });

        launcher_.controller().state_machine().on_state_changed([this](launcher::LauncherState /*old_state*/, launcher::LauncherState new_state) {
            if (new_state == launcher::LauncherState::Opening || new_state == launcher::LauncherState::Open) {
                if (notif_mgr_.is_notification_center_open()) {
                    notif_mgr_.close_notification_center();
                }
            }
        });

        switcher_.controller().state_machine().on_state_changed([this](switcher::SwitcherState /*old_state*/, switcher::SwitcherState new_state) {
            if (new_state == switcher::SwitcherState::Opening || new_state == switcher::SwitcherState::Open) {
                if (launcher_.is_open()) launcher_.close();
                if (system_ui_.is_panel_open()) system_ui_.close_panel();
                if (notif_mgr_.is_notification_center_open()) {
                    notif_mgr_.close_notification_center();
                }
            }
        });

        system_ui_.state_machine().on_state_changed([this](system::SystemPanelState /*old_state*/, system::SystemPanelState new_state) {
            if (new_state == system::SystemPanelState::Opening || new_state == system::SystemPanelState::Open) {
                if (launcher_.is_open()) launcher_.close();
                if (switcher_.is_open()) switcher_.close();
                if (notif_mgr_.is_notification_center_open()) {
                    notif_mgr_.close_notification_center();
                }
            }
        });

        // Quick Controls callback
        system_ui_.controls_manager().on_open_notifications([this]() {
            system_ui_.close_panel();
            notif_mgr_.open_notification_center();
        });
    }

    void TearDown() override {
        notif_mgr_.shutdown();
        system_ui_.shutdown();
        switcher_.shutdown();
        dock_.shutdown();
        launcher_.shutdown();
        if (wm_) {
            wm_->shutdown();
        }
    }

    display::DisplayPolicy policy_;
    config::Config config_;
    core::EventLoop loop_;
    shell::Shell shell_;
    application::ApplicationCatalog catalog_;
    window::WindowRegistry win_reg_;
    window::WindowTracker win_tracker_;
    display::DisplayManager disp_mgr_;
    std::unique_ptr<window::WindowManager> wm_;
    launcher::Launcher launcher_;
    dock::Dock dock_;
    switcher::Switcher switcher_;
    system::SystemUI system_ui_;
    NotificationManager notif_mgr_;
    InternalNotificationBackend* backend_ptr_ = nullptr;
};

TEST_F(NotificationIntegrationTest, PopupLifecycleAndRendering) {
    Notification notif(
        kInvalidNotificationId,
        "SystemUpdate",
        "Update Available",
        "Version 2.0 is ready to install.",
        "software-update",
        NotificationUrgency::Normal
    );
    notif.add_action("install", "Install Now");
    notif.add_action("later", "Later");

    NotificationId id = notif_mgr_.post_notification(std::move(notif));
    EXPECT_GT(id, 0u);
    EXPECT_TRUE(notif_mgr_.has_visible_popups());

    // Verify layout created popup item
    const auto* p = notif_mgr_.store().find(id);
    ASSERT_NE(p, nullptr);
    EXPECT_GT(p->popup_geometry().width, 300);

    // Verify ShmBuffer rendering
    int32_t w = 720, h = 1280;
    std::vector<uint8_t> mem(w * h * 4, 0);
    shell::ShmBuffer buffer(w, h, w * 4, mem.size(), -1, mem.data(), nullptr);
    shell::ShellTheme theme;
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);

    EXPECT_NO_THROW({
        notif_mgr_.render_popups(buffer, theme, tokens);
    });

    // Dismiss popup
    notif_mgr_.close_notification(id);
    EXPECT_FALSE(notif_mgr_.has_visible_popups());
}

TEST_F(NotificationIntegrationTest, ActionInvocationDispatchesToBackend) {
    Notification notif(
        kInvalidNotificationId,
        "Calendar",
        "Team Standup",
        "Starting in 5 minutes",
        "calendar",
        NotificationUrgency::Normal
    );
    notif.add_action("join", "Join Meeting");

    NotificationId id = notif_mgr_.post_notification(std::move(notif));
    const auto* p = notif_mgr_.store().find(id);
    ASSERT_NE(p, nullptr);
    ASSERT_FALSE(p->actions().empty());
    const auto& act_rect = p->actions()[0].geometry;

    int32_t tap_x = act_rect.x + act_rect.width / 2;
    int32_t tap_y = act_rect.y + act_rect.height / 2;

    EXPECT_TRUE(notif_mgr_.handle_touch_down(tap_x, tap_y));
    EXPECT_TRUE(notif_mgr_.handle_touch_up(tap_x, tap_y));

    EXPECT_EQ(backend_ptr_->last_action_id(), id);
    EXPECT_EQ(backend_ptr_->last_action_key(), "join");
}

TEST_F(NotificationIntegrationTest, MutualExclusionWithOtherSurfaces) {
    // 1. Notification center open closes Launcher
    launcher_.open();
    EXPECT_TRUE(launcher_.is_open());
    EXPECT_FALSE(notif_mgr_.is_notification_center_open());

    notif_mgr_.open_notification_center();
    EXPECT_TRUE(notif_mgr_.is_notification_center_open());
    EXPECT_FALSE(launcher_.is_open());

    // 2. Launcher open closes Notification center
    launcher_.open();
    EXPECT_TRUE(launcher_.is_open());
    EXPECT_FALSE(notif_mgr_.is_notification_center_open());

    // 3. Notification center open closes Switcher
    switcher_.open();
    EXPECT_TRUE(switcher_.is_open());
    notif_mgr_.open_notification_center();
    EXPECT_TRUE(notif_mgr_.is_notification_center_open());
    EXPECT_FALSE(switcher_.is_open());

    // 4. Notification center open closes System UI Panel
    system_ui_.open_panel();
    EXPECT_TRUE(system_ui_.is_panel_open());
    notif_mgr_.open_notification_center();
    EXPECT_TRUE(notif_mgr_.is_notification_center_open());
    EXPECT_FALSE(system_ui_.is_panel_open());

    // 5. System UI Quick Control "Notifications" opens Notification Center
    system_ui_.open_panel();
    EXPECT_TRUE(system_ui_.is_panel_open());
    EXPECT_FALSE(notif_mgr_.is_notification_center_open());

    // Find and activate Notifications quick control tile
    bool tile_found = false;
    for (size_t i = 0; i < system_ui_.controls_manager().control_count(); ++i) {
        const auto* c = system_ui_.controls_manager().control_at(i);
        if (c && c->type == system::ControlType::Notifications) {
            system_ui_.controls_manager().activate_index(i);
            tile_found = true;
            break;
        }
    }
    EXPECT_TRUE(tile_found);
    EXPECT_FALSE(system_ui_.is_panel_open());
    EXPECT_TRUE(notif_mgr_.is_notification_center_open());
}

TEST_F(NotificationIntegrationTest, OrientationChangeAdaptsLayout) {
    Notification notif(kInvalidNotificationId, "News", "Breaking News", "Headline text", "", NotificationUrgency::Normal);
    NotificationId id = notif_mgr_.post_notification(std::move(notif));

    // Portrait
    const auto* p_port = notif_mgr_.store().find(id);
    ASSERT_NE(p_port, nullptr);
    int32_t portrait_width = p_port->popup_geometry().width;
    EXPECT_GT(portrait_width, 200);

    // Switch to Landscape
    auto landscape_policy = make_policy(1280, 720);
    shell_.update_display(landscape_policy.display_info());
    notif_mgr_.update_display_policy(landscape_policy);

    const auto* p_land = notif_mgr_.store().find(id);
    ASSERT_NE(p_land, nullptr);
    int32_t landscape_width = p_land->popup_geometry().width;

    EXPECT_LE(landscape_width, 1280);
    EXPECT_GT(landscape_width, 200);
    EXPECT_LE(p_land->popup_geometry().x + landscape_width, 1280);
}
