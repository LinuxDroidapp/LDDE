#pragma once

#include <memory>
#include "ldde/core/error.hpp"
#include "ldde/core/event_loop.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shell.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/notification/notification.hpp"
#include "ldde/notification/notification_store.hpp"
#include "ldde/notification/notification_backend.hpp"
#include "ldde/notification/notification_presenter.hpp"
#include "ldde/notification/notification_center_state.hpp"
#include "ldde/notification/notification_layout.hpp"
#include "ldde/notification/notification_controller.hpp"

namespace ldde::notification {

class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();

    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;

    core::Status initialize(
        shell::Shell& shell,
        window::WindowManager& window_manager,
        application::ApplicationCatalog& catalog,
        const display::DisplayPolicy& display_policy,
        const config::Config& config,
        core::EventLoop& event_loop,
        std::unique_ptr<NotificationBackend> backend = nullptr);

    void shutdown();

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    // Core notification APIs
    NotificationId post_notification(Notification notification);
    NotificationId post_system_notification(
        std::string summary,
        std::string body,
        NotificationUrgency urgency = NotificationUrgency::Normal);

    bool close_notification(NotificationId id);

    // Notification Center controls
    void open_notification_center();
    void close_notification_center();
    void toggle_notification_center();
    [[nodiscard]] bool is_notification_center_open() const noexcept;
    [[nodiscard]] bool has_visible_popups() const noexcept;

    // Display & Layout
    void update_display_policy(const display::DisplayPolicy& policy);
    void update_layout();

    // Rendering
    void render_popups(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens);

    void render_notification_center(
        shell::ShmBuffer& buffer,
        const shell::ShellTheme& theme,
        const shell::DesignTokens& tokens);

    // Input handlers
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();
    bool handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers);

    // Subsystem component accessors
    [[nodiscard]] NotificationStore& store() noexcept { return store_; }
    [[nodiscard]] const NotificationStore& store() const noexcept { return store_; }

    [[nodiscard]] NotificationBackend* backend() noexcept { return backend_.get(); }
    [[nodiscard]] const NotificationBackend* backend() const noexcept { return backend_.get(); }

    [[nodiscard]] NotificationPresenter& presenter() noexcept { return *presenter_; }
    [[nodiscard]] const NotificationPresenter& presenter() const noexcept { return *presenter_; }

    [[nodiscard]] NotificationCenterStateMachine& center_state() noexcept { return center_state_; }
    [[nodiscard]] const NotificationCenterStateMachine& center_state() const noexcept { return center_state_; }

    [[nodiscard]] NotificationLayout& layout() noexcept { return layout_; }
    [[nodiscard]] const NotificationLayout& layout() const noexcept { return layout_; }

    [[nodiscard]] NotificationController& controller() noexcept { return *controller_; }
    [[nodiscard]] const NotificationController& controller() const noexcept { return *controller_; }

    using RequestRenderCallback = std::function<void()>;
    void on_request_render(RequestRenderCallback cb) { request_render_callback_ = std::move(cb); }

private:
    void setup_backend();
    void setup_callbacks();
    void resolve_application(Notification& notif);
    void handle_action_activated(NotificationId id, const std::string& action_key);
    void handle_default_activated(NotificationId id);

    bool initialized_ = false;

    shell::Shell* shell_ = nullptr;
    window::WindowManager* window_manager_ = nullptr;
    application::ApplicationCatalog* catalog_ = nullptr;
    display::DisplayPolicy display_policy_;
    config::Config config_;
    core::EventLoop* event_loop_ = nullptr;

    NotificationStore store_;
    std::unique_ptr<NotificationBackend> backend_;
    std::unique_ptr<NotificationPresenter> presenter_;
    NotificationCenterStateMachine center_state_;
    NotificationLayout layout_;
    std::unique_ptr<NotificationController> controller_;
    RequestRenderCallback request_render_callback_;
};

} // namespace ldde::notification
