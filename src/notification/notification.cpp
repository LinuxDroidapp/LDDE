#include "ldde/notification/notification.hpp"
#include <algorithm>

namespace ldde::notification {

namespace {

constexpr size_t kMaxSummaryLength = 128;
constexpr size_t kMaxBodyLength = 1024;
constexpr size_t kMaxActions = 8;

std::string sanitize_text(std::string text, size_t max_len) {
    // 1. Strip HTML tags: remove everything between '<' and '>'
    std::string no_html;
    no_html.reserve(text.size());
    bool in_tag = false;
    for (char c : text) {
        if (c == '<') {
            in_tag = true;
        } else if (c == '>') {
            in_tag = false;
        } else if (!in_tag) {
            no_html += c;
        }
    }

    // 2. Remove control characters (including null chars, \r, but allow normal whitespace)
    std::string clean;
    clean.reserve(no_html.size());
    for (char c : no_html) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 32 && uc != 127) {
            clean += c;
        } else if (uc == '\n' || uc == '\t') {
            clean += ' ';
        }
    }

    // 3. Collapse multiple spaces and trim leading/trailing whitespace
    std::string trimmed;
    trimmed.reserve(clean.size());
    bool prev_space = true;
    for (char c : clean) {
        if (c == ' ') {
            if (!prev_space) {
                trimmed += ' ';
                prev_space = true;
            }
        } else {
            trimmed += c;
            prev_space = false;
        }
    }
    if (!trimmed.empty() && trimmed.back() == ' ') {
        trimmed.pop_back();
    }

    // 4. Truncate to max_len
    if (trimmed.size() > max_len) {
        trimmed.resize(max_len);
    }
    return trimmed;
}

} // namespace

Notification::Notification(
    NotificationId id,
    std::string app_name,
    std::string summary,
    std::string body,
    std::string icon,
    NotificationUrgency urgency,
    int32_t expire_timeout_ms,
    uint32_t replaces_id)
    : id_(id),
      app_name_(std::move(app_name)),
      summary_(std::move(summary)),
      body_(std::move(body)),
      icon_(std::move(icon)),
      urgency_(urgency),
      timestamp_(std::chrono::system_clock::now()),
      expire_timeout_ms_(expire_timeout_ms),
      replaces_id_(replaces_id) {
    sanitize();
}

void Notification::set_summary(std::string summary) {
    summary_ = sanitize_text(std::move(summary), kMaxSummaryLength);
}

void Notification::set_body(std::string body) {
    body_ = sanitize_text(std::move(body), kMaxBodyLength);
}

void Notification::add_action(std::string key, std::string label) {
    if (actions_.size() >= kMaxActions) {
        return;
    }
    NotificationAction action;
    action.key = sanitize_text(std::move(key), 64);
    action.label = sanitize_text(std::move(label), 64);
    actions_.push_back(std::move(action));
}

bool Notification::has_action(std::string_view key) const noexcept {
    return find_action(key) != nullptr;
}

const NotificationAction* Notification::find_action(std::string_view key) const noexcept {
    for (const auto& a : actions_) {
        if (a.key == key) {
            return &a;
        }
    }
    return nullptr;
}

void Notification::set_hint(std::string key, std::string value) {
    hints_[std::move(key)] = std::move(value);
}

std::optional<std::string> Notification::get_hint(const std::string& key) const {
    auto it = hints_.find(key);
    if (it != hints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Notification::is_persistent() const noexcept {
    return (urgency_ == NotificationUrgency::Critical || expire_timeout_ms_ == 0);
}

void Notification::set_action_geometry(size_t index, const core::Rect& geom) {
    if (index < actions_.size()) {
        actions_[index].geometry = geom;
    }
}

void Notification::update_from(const Notification& other) {
    app_name_ = other.app_name_;
    if (other.app_id_.has_value()) {
        app_id_ = other.app_id_;
    }
    summary_ = other.summary_;
    body_ = other.body_;
    icon_ = other.icon_;
    urgency_ = other.urgency_;
    // Preserve original timestamp_
    actions_ = other.actions_;
    category_ = other.category_;
    hints_ = other.hints_;
    expire_timeout_ms_ = other.expire_timeout_ms_;
    group_id_ = other.group_id_;
    is_resident_ = other.is_resident_;
    is_transient_ = other.is_transient_;
    sanitize();
}

void Notification::sanitize() {
    app_name_ = sanitize_text(std::move(app_name_), 64);
    summary_ = sanitize_text(std::move(summary_), kMaxSummaryLength);
    body_ = sanitize_text(std::move(body_), kMaxBodyLength);
    if (actions_.size() > kMaxActions) {
        actions_.resize(kMaxActions);
    }
    for (auto& a : actions_) {
        a.key = sanitize_text(std::move(a.key), 64);
        a.label = sanitize_text(std::move(a.label), 64);
    }
}

} // namespace ldde::notification

