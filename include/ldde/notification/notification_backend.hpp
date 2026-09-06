#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include "ldde/notification/notification.hpp"
#include "ldde/core/error.hpp"

namespace ldde::notification {

class NotificationBackend {
public:
    virtual ~NotificationBackend() = default;

    using NotificationReceivedCallback = std::function<NotificationId(Notification)>;
    using NotificationCloseRequestedCallback = std::function<bool(NotificationId)>;

    virtual core::Status start() = 0;
    virtual void stop() = 0;

    virtual void emit_notification_closed(NotificationId id, NotificationCloseReason reason) = 0;
    virtual void emit_action_invoked(NotificationId id, const std::string& action_key) = 0;

    virtual void on_notification_received(NotificationReceivedCallback cb) = 0;
    virtual void on_close_requested(NotificationCloseRequestedCallback cb) = 0;

    [[nodiscard]] virtual std::string_view backend_name() const noexcept = 0;
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;
};

} // namespace ldde::notification

