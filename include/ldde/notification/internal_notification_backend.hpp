#pragma once

#include "ldde/notification/notification_backend.hpp"
#include <vector>

namespace ldde::notification {

class InternalNotificationBackend : public NotificationBackend {
public:
    InternalNotificationBackend() = default;
    ~InternalNotificationBackend() override = default;

    core::Status start() override;
    void stop() override;

    void emit_notification_closed(NotificationId id, NotificationCloseReason reason) override;
    void emit_action_invoked(NotificationId id, const std::string& action_key) override;

    void on_notification_received(NotificationReceivedCallback cb) override;
    void on_close_requested(NotificationCloseRequestedCallback cb) override;

    [[nodiscard]] std::string_view backend_name() const noexcept override { return "internal"; }
    [[nodiscard]] bool is_connected() const noexcept override { return is_running_; }

    // Direct injection methods for test suites & internal system events
    NotificationId post_notification(Notification notification);
    bool request_close(NotificationId id);

    // Diagnostics / test inspectability
    [[nodiscard]] NotificationId last_closed_id() const noexcept { return last_closed_id_; }
    [[nodiscard]] NotificationCloseReason last_closed_reason() const noexcept { return last_closed_reason_; }
    [[nodiscard]] NotificationId last_action_id() const noexcept { return last_action_id_; }
    [[nodiscard]] const std::string& last_action_key() const noexcept { return last_action_key_; }
    [[nodiscard]] size_t total_received_count() const noexcept { return total_received_count_; }

private:
    bool is_running_ = false;
    NotificationReceivedCallback received_callback_;
    NotificationCloseRequestedCallback close_callback_;

    NotificationId last_closed_id_ = kInvalidNotificationId;
    NotificationCloseReason last_closed_reason_ = NotificationCloseReason::Undefined;
    NotificationId last_action_id_ = kInvalidNotificationId;
    std::string last_action_key_;
    size_t total_received_count_ = 0;
};

} // namespace ldde::notification

