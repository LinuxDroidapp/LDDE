#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include "ldde/notification/notification_types.hpp"
#include "ldde/application/application_id.hpp"
#include "ldde/core/types.hpp"

namespace ldde::notification {

class Notification {
public:
    Notification() = default;

    Notification(
        NotificationId id,
        std::string app_name,
        std::string summary,
        std::string body,
        std::string icon = "",
        NotificationUrgency urgency = NotificationUrgency::Normal,
        int32_t expire_timeout_ms = -1,
        uint32_t replaces_id = 0);

    [[nodiscard]] NotificationId id() const noexcept { return id_; }
    void set_id(NotificationId id) noexcept { id_ = id; }

    [[nodiscard]] const std::string& app_name() const noexcept { return app_name_; }
    void set_app_name(std::string name) { app_name_ = std::move(name); }

    [[nodiscard]] const std::optional<application::ApplicationId>& app_id() const noexcept { return app_id_; }
    void set_app_id(std::optional<application::ApplicationId> app_id) noexcept { app_id_ = std::move(app_id); }

    [[nodiscard]] const std::string& summary() const noexcept { return summary_; }
    void set_summary(std::string summary);

    [[nodiscard]] const std::string& body() const noexcept { return body_; }
    void set_body(std::string body);

    [[nodiscard]] const std::string& icon() const noexcept { return icon_; }
    void set_icon(std::string icon) { icon_ = std::move(icon); }

    [[nodiscard]] NotificationUrgency urgency() const noexcept { return urgency_; }
    void set_urgency(NotificationUrgency urgency) noexcept { urgency_ = urgency; }

    [[nodiscard]] std::chrono::system_clock::time_point timestamp() const noexcept { return timestamp_; }
    void set_timestamp(std::chrono::system_clock::time_point tp) noexcept { timestamp_ = tp; }

    [[nodiscard]] const std::vector<NotificationAction>& actions() const noexcept { return actions_; }
    void add_action(std::string key, std::string label);
    void clear_actions() noexcept { actions_.clear(); }
    [[nodiscard]] bool has_action(std::string_view key) const noexcept;
    [[nodiscard]] const NotificationAction* find_action(std::string_view key) const noexcept;

    [[nodiscard]] const std::string& category() const noexcept { return category_; }
    void set_category(std::string cat) { category_ = std::move(cat); }

    [[nodiscard]] const std::unordered_map<std::string, std::string>& hints() const noexcept { return hints_; }
    void set_hint(std::string key, std::string value);
    [[nodiscard]] std::optional<std::string> get_hint(const std::string& key) const;

    [[nodiscard]] int32_t expire_timeout_ms() const noexcept { return expire_timeout_ms_; }
    void set_expire_timeout_ms(int32_t timeout) noexcept { expire_timeout_ms_ = timeout; }

    [[nodiscard]] uint32_t replaces_id() const noexcept { return replaces_id_; }
    void set_replaces_id(uint32_t rep_id) noexcept { replaces_id_ = rep_id; }

    [[nodiscard]] const std::string& group_id() const noexcept { return group_id_; }
    void set_group_id(std::string gid) { group_id_ = std::move(gid); }

    [[nodiscard]] NotificationLifecycleState state() const noexcept { return state_; }
    void set_state(NotificationLifecycleState state) noexcept { state_ = state; }

    [[nodiscard]] bool is_resident() const noexcept { return is_resident_; }
    void set_resident(bool res) noexcept { is_resident_ = res; }

    [[nodiscard]] bool is_transient() const noexcept { return is_transient_; }
    void set_transient(bool trans) noexcept { is_transient_ = trans; }

    [[nodiscard]] bool is_critical() const noexcept { return urgency_ == NotificationUrgency::Critical; }
    [[nodiscard]] bool is_persistent() const noexcept;

    // Hit-testing / Layout geometries
    [[nodiscard]] const core::Rect& popup_geometry() const noexcept { return popup_geometry_; }
    void set_popup_geometry(const core::Rect& geom) noexcept { popup_geometry_ = geom; }

    [[nodiscard]] const core::Rect& dismiss_button_geometry() const noexcept { return dismiss_button_geometry_; }
    void set_dismiss_button_geometry(const core::Rect& geom) noexcept { dismiss_button_geometry_ = geom; }

    void set_action_geometry(size_t index, const core::Rect& geom);

    // In-place replacement
    void update_from(const Notification& other);

    // Security sanitization
    void sanitize();

private:
    NotificationId id_ = kInvalidNotificationId;
    std::string app_name_;
    std::optional<application::ApplicationId> app_id_;
    std::string summary_;
    std::string body_;
    std::string icon_;
    NotificationUrgency urgency_ = NotificationUrgency::Normal;
    std::chrono::system_clock::time_point timestamp_ = std::chrono::system_clock::now();
    std::vector<NotificationAction> actions_;
    std::string category_;
    std::unordered_map<std::string, std::string> hints_;
    int32_t expire_timeout_ms_ = -1;
    uint32_t replaces_id_ = 0;
    std::string group_id_;
    NotificationLifecycleState state_ = NotificationLifecycleState::Received;
    bool is_resident_ = false;
    bool is_transient_ = false;

    core::Rect popup_geometry_{0, 0, 0, 0};
    core::Rect dismiss_button_geometry_{0, 0, 0, 0};
};

} // namespace ldde::notification

