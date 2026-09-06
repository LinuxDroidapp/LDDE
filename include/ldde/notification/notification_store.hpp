#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>
#include "ldde/notification/notification.hpp"
#include "ldde/core/error.hpp"

namespace ldde::notification {

class NotificationStore {
public:
    using NotificationAddedCallback = std::function<void(const Notification&)>;
    using NotificationUpdatedCallback = std::function<void(const Notification&)>;
    using NotificationRemovedCallback = std::function<void(NotificationId, NotificationCloseReason)>;

    explicit NotificationStore(size_t max_history_entries = 50, size_t max_active_per_app = 10);

    void set_max_history_entries(size_t max_entries) noexcept { max_history_entries_ = max_entries; }
    [[nodiscard]] size_t max_history_entries() const noexcept { return max_history_entries_; }

    [[nodiscard]] NotificationId next_id() noexcept;

    // Adding & Updating
    NotificationId add_or_replace(Notification notification);

    // Lookup
    [[nodiscard]] Notification* find(NotificationId id) noexcept;
    [[nodiscard]] const Notification* find(NotificationId id) const noexcept;

    // State transitions
    bool mark_displayed(NotificationId id);
    bool dismiss(NotificationId id);
    bool expire(NotificationId id);
    bool close(NotificationId id, NotificationCloseReason reason = NotificationCloseReason::ClosedByCall);

    // History clearing
    size_t clear_history();

    // Query collections
    [[nodiscard]] std::vector<const Notification*> active_notifications() const;
    [[nodiscard]] std::vector<const Notification*> history_notifications() const;
    [[nodiscard]] std::vector<const Notification*> all_notifications() const;
    [[nodiscard]] std::unordered_map<std::string, std::vector<const Notification*>> grouped_notifications() const;

    [[nodiscard]] size_t total_count() const noexcept { return notifications_.size(); }
    [[nodiscard]] size_t active_count() const noexcept;
    [[nodiscard]] size_t history_count() const noexcept;

    // Callbacks
    void on_added(NotificationAddedCallback cb) { added_callbacks_.push_back(std::move(cb)); }
    void on_updated(NotificationUpdatedCallback cb) { updated_callbacks_.push_back(std::move(cb)); }
    void on_removed(NotificationRemovedCallback cb) { removed_callbacks_.push_back(std::move(cb)); }

private:
    void prune_history();
    void enforce_per_app_limit(const std::string& app_name);

    size_t max_history_entries_;
    size_t max_active_per_app_;
    NotificationId next_id_ = 1;

    std::vector<std::shared_ptr<Notification>> notifications_;

    std::vector<NotificationAddedCallback> added_callbacks_;
    std::vector<NotificationUpdatedCallback> updated_callbacks_;
    std::vector<NotificationRemovedCallback> removed_callbacks_;
};

} // namespace ldde::notification

