#include "ldde/notification/dbus_notification_backend.hpp"
#include "ldde/core/logging.hpp"
#include <gio/gio.h>

namespace ldde::notification {

namespace {

const gchar kIntrospectionXml[] =
    "<node>"
    "  <interface name='org.freedesktop.Notifications'>"
    "    <method name='Notify'>"
    "      <arg type='s' name='app_name' direction='in'/>"
    "      <arg type='u' name='replaces_id' direction='in'/>"
    "      <arg type='s' name='app_icon' direction='in'/>"
    "      <arg type='s' name='summary' direction='in'/>"
    "      <arg type='s' name='body' direction='in'/>"
    "      <arg type='as' name='actions' direction='in'/>"
    "      <arg type='a{sv}' name='hints' direction='in'/>"
    "      <arg type='i' name='expire_timeout' direction='in'/>"
    "      <arg type='u' name='id' direction='out'/>"
    "    </method>"
    "    <method name='CloseNotification'>"
    "      <arg type='u' name='id' direction='in'/>"
    "    </method>"
    "    <method name='GetCapabilities'>"
    "      <arg type='as' name='capabilities' direction='out'/>"
    "    </method>"
    "    <method name='GetServerInformation'>"
    "      <arg type='s' name='name' direction='out'/>"
    "      <arg type='s' name='vendor' direction='out'/>"
    "      <arg type='s' name='version' direction='out'/>"
    "      <arg type='s' name='spec_version' direction='out'/>"
    "    </method>"
    "    <signal name='NotificationClosed'>"
    "      <arg type='u' name='id'/>"
    "      <arg type='u' name='reason'/>"
    "    </signal>"
    "    <signal name='ActionInvoked'>"
    "      <arg type='u' name='id'/>"
    "      <arg type='s' name='action_key'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

const GDBusInterfaceVTable kVTable = {
    &DBusNotificationBackend::handle_method_call,
    nullptr, // get_property
    nullptr, // set_property
    {nullptr}
};

} // namespace

DBusNotificationBackend::DBusNotificationBackend(core::EventLoop* event_loop)
    : event_loop_(event_loop) {}

DBusNotificationBackend::~DBusNotificationBackend() {
    stop();
}

core::Status DBusNotificationBackend::start() {
    if (is_running_) {
        return core::Status::ok();
    }

    is_running_ = true;
    worker_thread_ = std::thread(&DBusNotificationBackend::run_worker, this);
    LDDE_LOG_INFO(Notification, "DBusNotificationBackend started worker thread");
    return core::Status::ok();
}

void DBusNotificationBackend::stop() {
    if (!is_running_) return;
    is_running_ = false;

    if (main_loop_ && g_main_loop_is_running(main_loop_)) {
        g_main_loop_quit(main_loop_);
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (owner_id_ > 0) {
        g_bus_unown_name(owner_id_);
        owner_id_ = 0;
    }

    if (introspection_data_) {
        g_dbus_node_info_unref(introspection_data_);
        introspection_data_ = nullptr;
    }

    if (main_loop_) {
        g_main_loop_unref(main_loop_);
        main_loop_ = nullptr;
    }

    if (main_context_) {
        g_main_context_unref(main_context_);
        main_context_ = nullptr;
    }

    connection_ = nullptr;
    is_connected_ = false;
    LDDE_LOG_INFO(Notification, "DBusNotificationBackend stopped");
}

void DBusNotificationBackend::run_worker() {
    main_context_ = g_main_context_new();
    g_main_context_push_thread_default(main_context_);
    main_loop_ = g_main_loop_new(main_context_, FALSE);

    GError* error = nullptr;
    introspection_data_ = g_dbus_node_info_new_for_xml(kIntrospectionXml, &error);
    if (!introspection_data_) {
        LDDE_LOG_WARN(Notification, "Failed to parse D-Bus introspection XML: "
                      << (error ? error->message : "unknown"));
        if (error) g_error_free(error);
        g_main_context_pop_thread_default(main_context_);
        return;
    }

    owner_id_ = g_bus_own_name_on_connection(
        nullptr, // We use standard session bus acquisition via g_bus_own_name
        "org.freedesktop.Notifications",
        G_BUS_NAME_OWNER_FLAGS_REPLACE,
        &DBusNotificationBackend::on_name_acquired,
        &DBusNotificationBackend::on_name_lost,
        this,
        nullptr);

    // Alternative: own name with connection callbacks
    g_bus_unown_name(owner_id_);
    owner_id_ = g_bus_own_name(
        G_BUS_TYPE_SESSION,
        "org.freedesktop.Notifications",
        G_BUS_NAME_OWNER_FLAGS_REPLACE,
        &DBusNotificationBackend::on_bus_acquired,
        &DBusNotificationBackend::on_name_acquired,
        &DBusNotificationBackend::on_name_lost,
        this,
        nullptr);

    LDDE_LOG_INFO(Notification, "Entering GDBus worker main loop");
    g_main_loop_run(main_loop_);

    g_main_context_pop_thread_default(main_context_);
}

void DBusNotificationBackend::on_bus_acquired(
    GDBusConnection* connection,
    const gchar* /*name*/,
    gpointer user_data) {
    auto* self = static_cast<DBusNotificationBackend*>(user_data);
    self->connection_ = connection;
    self->is_connected_ = true;
    LDDE_LOG_INFO(Notification, "D-Bus session bus acquired");

    GError* error = nullptr;
    self->registration_id_ = g_dbus_connection_register_object(
        connection,
        "/org/freedesktop/Notifications",
        self->introspection_data_->interfaces[0],
        &kVTable,
        self,
        nullptr, // user_data_free_func
        &error);

    if (self->registration_id_ == 0) {
        LDDE_LOG_WARN(Notification, "Failed to register /org/freedesktop/Notifications: "
                      << (error ? error->message : "unknown"));
        if (error) g_error_free(error);
    } else {
        LDDE_LOG_INFO(Notification, "Successfully registered D-Bus object /org/freedesktop/Notifications");
    }
}

void DBusNotificationBackend::on_name_acquired(
    GDBusConnection* /*connection*/,
    const gchar* name,
    gpointer /*user_data*/) {
    LDDE_LOG_INFO(Notification, "Acquired D-Bus service name: " << (name ? name : ""));
}

void DBusNotificationBackend::on_name_lost(
    GDBusConnection* /*connection*/,
    const gchar* name,
    gpointer user_data) {
    auto* self = static_cast<DBusNotificationBackend*>(user_data);
    self->is_connected_ = false;
    LDDE_LOG_WARN(Notification, "Lost or could not acquire D-Bus service name: " << (name ? name : ""));
}

void DBusNotificationBackend::handle_method_call(
    GDBusConnection* /*connection*/,
    const gchar* /*sender*/,
    const gchar* /*object_path*/,
    const gchar* /*interface_name*/,
    const gchar* method_name,
    GVariant* parameters,
    GDBusMethodInvocation* invocation,
    gpointer user_data) {
    auto* self = static_cast<DBusNotificationBackend*>(user_data);

    if (g_strcmp0(method_name, "Notify") == 0) {
        const gchar* app_name = "";
        guint32 replaces_id = 0;
        const gchar* app_icon = "";
        const gchar* summary = "";
        const gchar* body = "";
        GVariant* actions_var = nullptr;
        GVariant* hints_var = nullptr;
        gint32 expire_timeout = -1;

        g_variant_get(parameters, "(&su&s&s&s@as@a{sv}i)",
                      &app_name, &replaces_id, &app_icon,
                      &summary, &body, &actions_var, &hints_var, &expire_timeout);

        Notification notif(
            kInvalidNotificationId,
            app_name ? app_name : "",
            summary ? summary : "",
            body ? body : "",
            app_icon ? app_icon : "",
            NotificationUrgency::Normal,
            expire_timeout,
            replaces_id);

        if (actions_var) {
            GVariantIter iter;
            g_variant_iter_init(&iter, actions_var);
            const gchar* key = nullptr;
            const gchar* label = nullptr;
            while (g_variant_iter_next(&iter, "&s", &key)) {
                if (g_variant_iter_next(&iter, "&s", &label)) {
                    notif.add_action(key ? key : "", label ? label : "");
                } else {
                    notif.add_action(key ? key : "", key ? key : "");
                    break;
                }
            }
            g_variant_unref(actions_var);
        }

        if (hints_var) {
            GVariantIter iter;
            g_variant_iter_init(&iter, hints_var);
            const gchar* hint_key = nullptr;
            GVariant* hint_val = nullptr;
            while (g_variant_iter_next(&iter, "{&sv}", &hint_key, &hint_val)) {
                if (hint_key && hint_val) {
                    if (g_strcmp0(hint_key, "urgency") == 0) {
                        if (g_variant_is_of_type(hint_val, G_VARIANT_TYPE_BYTE)) {
                            guchar u = g_variant_get_byte(hint_val);
                            if (u == 0) notif.set_urgency(NotificationUrgency::Low);
                            else if (u == 1) notif.set_urgency(NotificationUrgency::Normal);
                            else if (u >= 2) notif.set_urgency(NotificationUrgency::Critical);
                        } else if (g_variant_is_of_type(hint_val, G_VARIANT_TYPE_INT32)) {
                            gint32 u = g_variant_get_int32(hint_val);
                            if (u == 0) notif.set_urgency(NotificationUrgency::Low);
                            else if (u == 1) notif.set_urgency(NotificationUrgency::Normal);
                            else if (u >= 2) notif.set_urgency(NotificationUrgency::Critical);
                        }
                    } else if (g_strcmp0(hint_key, "category") == 0) {
                        if (g_variant_is_of_type(hint_val, G_VARIANT_TYPE_STRING)) {
                            notif.set_category(g_variant_get_string(hint_val, nullptr));
                        }
                    } else if (g_strcmp0(hint_key, "resident") == 0) {
                        if (g_variant_is_of_type(hint_val, G_VARIANT_TYPE_BOOLEAN)) {
                            notif.set_resident(g_variant_get_boolean(hint_val));
                        }
                    } else if (g_strcmp0(hint_key, "transient") == 0) {
                        if (g_variant_is_of_type(hint_val, G_VARIANT_TYPE_BOOLEAN)) {
                            notif.set_transient(g_variant_get_boolean(hint_val));
                        }
                    } else if (g_variant_is_of_type(hint_val, G_VARIANT_TYPE_STRING)) {
                        notif.set_hint(hint_key, g_variant_get_string(hint_val, nullptr));
                    }
                    g_variant_unref(hint_val);
                }
            }
            g_variant_unref(hints_var);
        }

        NotificationId assigned_id = 0;
        {
            std::lock_guard<std::mutex> lock(self->callback_mutex_);
            if (self->received_callback_) {
                assigned_id = self->received_callback_(std::move(notif));
            }
        }

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(u)", assigned_id));
        return;
    }

    if (g_strcmp0(method_name, "CloseNotification") == 0) {
        guint32 id = 0;
        g_variant_get(parameters, "(u)", &id);
        {
            std::lock_guard<std::mutex> lock(self->callback_mutex_);
            if (self->close_callback_) {
                self->close_callback_(id);
            }
        }
        g_dbus_method_invocation_return_value(invocation, nullptr);
        return;
    }

    if (g_strcmp0(method_name, "GetCapabilities") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        g_variant_builder_add(&builder, "s", "actions");
        g_variant_builder_add(&builder, "s", "body");
        g_variant_builder_add(&builder, "s", "body-markup");
        g_variant_builder_add(&builder, "s", "icon-static");
        g_variant_builder_add(&builder, "s", "persistence");

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(as)", &builder));
        return;
    }

    if (g_strcmp0(method_name, "GetServerInformation") == 0) {
        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(ssss)",
                          "LinuxDroid LDDE Notification Daemon",
                          "LinuxDroid",
                          "1.0.0",
                          "1.2"));
        return;
    }

    g_dbus_method_invocation_return_error(
        invocation,
        G_DBUS_ERROR,
        G_DBUS_ERROR_UNKNOWN_METHOD,
        "Unknown method %s on interface org.freedesktop.Notifications",
        method_name);
}

void DBusNotificationBackend::emit_notification_closed(NotificationId id, NotificationCloseReason reason) {
    if (!connection_ || !is_connected_) return;

    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_,
        nullptr, // destination_bus_name (broadcast)
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "NotificationClosed",
        g_variant_new("(uu)", id, static_cast<guint32>(reason)),
        &error);

    if (error) {
        LDDE_LOG_WARN(Notification, "Failed to emit NotificationClosed: " << error->message);
        g_error_free(error);
    }
}

void DBusNotificationBackend::emit_action_invoked(NotificationId id, const std::string& action_key) {
    if (!connection_ || !is_connected_) return;

    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection_,
        nullptr,
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "ActionInvoked",
        g_variant_new("(us)", id, action_key.c_str()),
        &error);

    if (error) {
        LDDE_LOG_WARN(Notification, "Failed to emit ActionInvoked: " << error->message);
        g_error_free(error);
    }
}

void DBusNotificationBackend::on_notification_received(NotificationReceivedCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    received_callback_ = std::move(cb);
}

void DBusNotificationBackend::on_close_requested(NotificationCloseRequestedCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    close_callback_ = std::move(cb);
}

} // namespace ldde::notification

