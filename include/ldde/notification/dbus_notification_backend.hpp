#pragma once

#include "ldde/notification/notification_backend.hpp"
#include "ldde/core/event_loop.hpp"
#include <thread>
#include <atomic>
#include <mutex>

#include <gio/gio.h>

namespace ldde::notification {

class DBusNotificationBackend : public NotificationBackend {
public:
    explicit DBusNotificationBackend(core::EventLoop* event_loop = nullptr);
    ~DBusNotificationBackend() override;

    core::Status start() override;
    void stop() override;

    void emit_notification_closed(NotificationId id, NotificationCloseReason reason) override;
    void emit_action_invoked(NotificationId id, const std::string& action_key) override;

    void on_notification_received(NotificationReceivedCallback cb) override;
    void on_close_requested(NotificationCloseRequestedCallback cb) override;

    [[nodiscard]] std::string_view backend_name() const noexcept override { return "dbus"; }
    [[nodiscard]] bool is_connected() const noexcept override { return is_connected_; }

    void set_event_loop(core::EventLoop* event_loop) noexcept { event_loop_ = event_loop; }

    // Static C callbacks for GDBus
    static void on_bus_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_lost(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void handle_method_call(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* object_path,
        const gchar* interface_name,
        const gchar* method_name,
        GVariant* parameters,
        GDBusMethodInvocation* invocation,
        gpointer user_data);

private:
    void run_worker();

    core::EventLoop* event_loop_ = nullptr;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_connected_{false};

    NotificationReceivedCallback received_callback_;
    NotificationCloseRequestedCallback close_callback_;
    std::mutex callback_mutex_;

    std::thread worker_thread_;
    GMainLoop* main_loop_ = nullptr;
    GMainContext* main_context_ = nullptr;
    GDBusConnection* connection_ = nullptr;
    guint owner_id_ = 0;
    guint registration_id_ = 0;
    GDBusNodeInfo* introspection_data_ = nullptr;
};

} // namespace ldde::notification

