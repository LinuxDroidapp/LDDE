#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "ldde/core/types.hpp"

namespace ldde::notification {

using NotificationId = uint32_t;
inline constexpr NotificationId kInvalidNotificationId = 0;

enum class NotificationUrgency : uint8_t {
    Low = 0,
    Normal = 1,
    Critical = 2
};

[[nodiscard]] constexpr std::string_view notification_urgency_name(NotificationUrgency urgency) noexcept {
    switch (urgency) {
        case NotificationUrgency::Low:      return "Low";
        case NotificationUrgency::Normal:   return "Normal";
        case NotificationUrgency::Critical: return "Critical";
    }
    return "Unknown";
}

enum class NotificationLifecycleState : uint8_t {
    Received,
    Active,
    Displayed,
    Dismissing,
    Dismissed,
    Expired,
    Closed,
    Replaced
};

[[nodiscard]] constexpr std::string_view notification_state_name(NotificationLifecycleState state) noexcept {
    switch (state) {
        case NotificationLifecycleState::Received:   return "Received";
        case NotificationLifecycleState::Active:     return "Active";
        case NotificationLifecycleState::Displayed:  return "Displayed";
        case NotificationLifecycleState::Dismissing: return "Dismissing";
        case NotificationLifecycleState::Dismissed:  return "Dismissed";
        case NotificationLifecycleState::Expired:    return "Expired";
        case NotificationLifecycleState::Closed:     return "Closed";
        case NotificationLifecycleState::Replaced:   return "Replaced";
    }
    return "Unknown";
}

enum class NotificationCloseReason : uint32_t {
    Expired = 1,
    Dismissed = 2,
    ClosedByCall = 3,
    Undefined = 4
};

[[nodiscard]] constexpr std::string_view notification_close_reason_name(NotificationCloseReason reason) noexcept {
    switch (reason) {
        case NotificationCloseReason::Expired:      return "Expired";
        case NotificationCloseReason::Dismissed:    return "Dismissed";
        case NotificationCloseReason::ClosedByCall: return "ClosedByCall";
        case NotificationCloseReason::Undefined:    return "Undefined";
    }
    return "Unknown";
}

struct NotificationAction {
    std::string key;
    std::string label;
    core::Rect geometry{0, 0, 0, 0};
};

} // namespace ldde::notification

