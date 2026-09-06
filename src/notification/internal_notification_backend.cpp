#include "ldde/notification/internal_notification_backend.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::notification {

core::Status InternalNotificationBackend::start() {
    is_running_ = true;
    LDDE_LOG_INFO(Notification, "Internal notification backend started");
    return core::Status::ok();
}

void InternalNotificationBackend::stop() {
    is_running_ = false;
    LDDE_LOG_INFO(Notification, "Internal notification backend stopped");
}

void InternalNotificationBackend::emit_notification_closed(NotificationId id, NotificationCloseReason reason) {
    last_closed_id_ = id;
    last_closed_reason_ = reason;
    LDDE_LOG_DEBUG(Notification, "NotificationClosed signal emitted [id=" << id
                   << ", reason=" << notification_close_reason_name(reason) << "]");
}

void InternalNotificationBackend::emit_action_invoked(NotificationId id, const std::string& action_key) {
    last_action_id_ = id;
    last_action_key_ = action_key;
    LDDE_LOG_DEBUG(Notification, "ActionInvoked signal emitted [id=" << id
                   << ", action=" << action_key << "]");
}

void InternalNotificationBackend::on_notification_received(NotificationReceivedCallback cb) {
    received_callback_ = std::move(cb);
}

void InternalNotificationBackend::on_close_requested(NotificationCloseRequestedCallback cb) {
    close_callback_ = std::move(cb);
}

NotificationId InternalNotificationBackend::post_notification(Notification notification) {
    ++total_received_count_;
    if (received_callback_) {
        return received_callback_(std::move(notification));
    }
    return kInvalidNotificationId;
}

bool InternalNotificationBackend::request_close(NotificationId id) {
    if (close_callback_) {
        return close_callback_(id);
    }
    return false;
}

} // namespace ldde::notification

