#include "ldde/notification/notification_store.hpp"
#include <algorithm>

namespace ldde::notification {

NotificationStore::NotificationStore(size_t max_history_entries, size_t max_active_per_app)
    : max_history_entries_(max_history_entries),
      max_active_per_app_(max_active_per_app) {}

NotificationId NotificationStore::next_id() noexcept {
    if (next_id_ == 0) {
        next_id_ = 1;
    }
    return next_id_++;
}

NotificationId NotificationStore::add_or_replace(Notification notification) {
    notification.sanitize();

    // 1. Check for replaces_id
    if (notification.replaces_id() != 0) {
        auto* existing = find(notification.replaces_id());
        if (existing) {
            existing->update_from(notification);
            existing->set_state(NotificationLifecycleState::Active);
            for (const auto& cb : updated_callbacks_) {
                if (cb) cb(*existing);
            }
            return existing->id();
        }
    }

    // 2. Assign ID if not set
    if (notification.id() == kInvalidNotificationId) {
        notification.set_id(next_id());
    } else if (notification.id() >= next_id_) {
        next_id_ = notification.id() + 1;
    }

    // 3. Enforce flood limits per app
    enforce_per_app_limit(notification.app_name());

    notification.set_state(NotificationLifecycleState::Active);
    auto ptr = std::make_shared<Notification>(std::move(notification));
    // Insert at front so newest is first
    notifications_.insert(notifications_.begin(), ptr);

    // 4. Prune history if total exceeds bound
    prune_history();

    for (const auto& cb : added_callbacks_) {
        if (cb) cb(*ptr);
    }

    return ptr->id();
}

Notification* NotificationStore::find(NotificationId id) noexcept {
    for (auto& ptr : notifications_) {
        if (ptr && ptr->id() == id) {
            return ptr.get();
        }
    }
    return nullptr;
}

const Notification* NotificationStore::find(NotificationId id) const noexcept {
    for (const auto& ptr : notifications_) {
        if (ptr && ptr->id() == id) {
            return ptr.get();
        }
    }
    return nullptr;
}

bool NotificationStore::mark_displayed(NotificationId id) {
    auto* notif = find(id);
    if (notif && notif->state() == NotificationLifecycleState::Active) {
        notif->set_state(NotificationLifecycleState::Displayed);
        for (const auto& cb : updated_callbacks_) {
            if (cb) cb(*notif);
        }
        return true;
    }
    return false;
}

bool NotificationStore::dismiss(NotificationId id) {
    auto* notif = find(id);
    if (notif && (notif->state() == NotificationLifecycleState::Active ||
                  notif->state() == NotificationLifecycleState::Displayed)) {
        notif->set_state(NotificationLifecycleState::Dismissed);
        for (const auto& cb : removed_callbacks_) {
            if (cb) cb(id, NotificationCloseReason::Dismissed);
        }
        prune_history();
        return true;
    }
    return false;
}

bool NotificationStore::expire(NotificationId id) {
    auto* notif = find(id);
    if (notif && (notif->state() == NotificationLifecycleState::Active ||
                  notif->state() == NotificationLifecycleState::Displayed)) {
        notif->set_state(NotificationLifecycleState::Expired);
        for (const auto& cb : removed_callbacks_) {
            if (cb) cb(id, NotificationCloseReason::Expired);
        }
        prune_history();
        return true;
    }
    return false;
}

bool NotificationStore::close(NotificationId id, NotificationCloseReason reason) {
    auto* notif = find(id);
    if (notif) {
        notif->set_state(NotificationLifecycleState::Closed);
        for (const auto& cb : removed_callbacks_) {
            if (cb) cb(id, reason);
        }
        prune_history();
        return true;
    }
    return false;
}

size_t NotificationStore::clear_history() {
    size_t removed = 0;
    auto it = notifications_.begin();
    while (it != notifications_.end()) {
        if (*it && ((*it)->state() == NotificationLifecycleState::Dismissed ||
                    (*it)->state() == NotificationLifecycleState::Expired ||
                    (*it)->state() == NotificationLifecycleState::Closed)) {
            // Keep critical or resident notifications if desired, or clear
            if (!(*it)->is_critical()) {
                it = notifications_.erase(it);
                ++removed;
                continue;
            }
        }
        ++it;
    }
    return removed;
}

std::vector<const Notification*> NotificationStore::active_notifications() const {
    std::vector<const Notification*> result;
    for (const auto& ptr : notifications_) {
        if (ptr && (ptr->state() == NotificationLifecycleState::Active ||
                    ptr->state() == NotificationLifecycleState::Displayed)) {
            result.push_back(ptr.get());
        }
    }
    return result;
}

std::vector<const Notification*> NotificationStore::history_notifications() const {
    std::vector<const Notification*> result;
    for (const auto& ptr : notifications_) {
        if (ptr && (ptr->state() == NotificationLifecycleState::Dismissed ||
                    ptr->state() == NotificationLifecycleState::Expired ||
                    ptr->state() == NotificationLifecycleState::Closed)) {
            result.push_back(ptr.get());
        }
    }
    return result;
}

std::vector<const Notification*> NotificationStore::all_notifications() const {
    std::vector<const Notification*> result;
    result.reserve(notifications_.size());
    for (const auto& ptr : notifications_) {
        if (ptr) {
            result.push_back(ptr.get());
        }
    }
    return result;
}

std::unordered_map<std::string, std::vector<const Notification*>> NotificationStore::grouped_notifications() const {
    std::unordered_map<std::string, std::vector<const Notification*>> groups;
    for (const auto& ptr : notifications_) {
        if (ptr) {
            std::string key = ptr->group_id().empty() ? ptr->app_name() : ptr->group_id();
            if (key.empty()) key = "Other";
            groups[key].push_back(ptr.get());
        }
    }
    return groups;
}

size_t NotificationStore::active_count() const noexcept {
    size_t count = 0;
    for (const auto& ptr : notifications_) {
        if (ptr && (ptr->state() == NotificationLifecycleState::Active ||
                    ptr->state() == NotificationLifecycleState::Displayed)) {
            ++count;
        }
    }
    return count;
}

size_t NotificationStore::history_count() const noexcept {
    size_t count = 0;
    for (const auto& ptr : notifications_) {
        if (ptr && (ptr->state() == NotificationLifecycleState::Dismissed ||
                    ptr->state() == NotificationLifecycleState::Expired ||
                    ptr->state() == NotificationLifecycleState::Closed)) {
            ++count;
        }
    }
    return count;
}

void NotificationStore::prune_history() {
    // Drop transient items that are no longer active
    notifications_.erase(
        std::remove_if(notifications_.begin(), notifications_.end(), [](const std::shared_ptr<Notification>& n) {
            return n && n->is_transient() &&
                   (n->state() == NotificationLifecycleState::Dismissed ||
                    n->state() == NotificationLifecycleState::Expired ||
                    n->state() == NotificationLifecycleState::Closed);
        }),
        notifications_.end());

    // Count history items from back to front
    size_t history = 0;
    for (auto it = notifications_.rbegin(); it != notifications_.rend();) {
        if (*it && ((*it)->state() == NotificationLifecycleState::Dismissed ||
                    (*it)->state() == NotificationLifecycleState::Expired ||
                    (*it)->state() == NotificationLifecycleState::Closed)) {
            if (history >= max_history_entries_ && !(*it)->is_critical()) {
                // Erase from vector
                auto forward_it = it.base() - 1;
                it = decltype(it)(notifications_.erase(forward_it));
                continue;
            }
            ++history;
        }
        ++it;
    }
}

void NotificationStore::enforce_per_app_limit(const std::string& app_name) {
    if (max_active_per_app_ == 0) return;

    size_t active = 0;
    for (const auto& ptr : notifications_) {
        if (ptr && ptr->app_name() == app_name &&
            (ptr->state() == NotificationLifecycleState::Active ||
             ptr->state() == NotificationLifecycleState::Displayed)) {
            ++active;
        }
    }

    if (active >= max_active_per_app_) {
        // Expire the oldest active notification for this app
        for (auto it = notifications_.rbegin(); it != notifications_.rend(); ++it) {
            if (*it && (*it)->app_name() == app_name &&
                ((*it)->state() == NotificationLifecycleState::Active ||
                 (*it)->state() == NotificationLifecycleState::Displayed) &&
                !(*it)->is_critical()) {
                (*it)->set_state(NotificationLifecycleState::Expired);
                break;
            }
        }
    }
}

} // namespace ldde::notification

