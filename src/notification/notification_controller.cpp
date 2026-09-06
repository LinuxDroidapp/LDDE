#include "ldde/notification/notification_controller.hpp"
#include "ldde/core/logging.hpp"
#include <cmath>

namespace ldde::notification {

namespace {
constexpr int32_t kSwipeDismissThresholdPx = 40;
}

NotificationController::NotificationController(
    NotificationStore& store,
    NotificationPresenter& presenter,
    NotificationCenterStateMachine& center_state,
    NotificationLayout& layout)
    : store_(store),
      presenter_(presenter),
      center_state_(center_state),
      layout_(layout) {}

bool NotificationController::handle_touch_down(int32_t x, int32_t y) {
    is_touch_down_ = true;
    touch_down_pos_ = {x, y};
    last_touch_pos_ = {x, y};
    drag_offset_x_ = 0;

    target_popup_id_ = kInvalidNotificationId;
    is_dismiss_target_ = false;
    is_action_target_ = false;
    target_action_index_ = 0;

    is_outside_center_tap_ = false;
    is_clear_all_target_ = false;
    is_close_target_ = false;
    target_center_item_id_ = kInvalidNotificationId;

    // 1. Check popup hits first
    if (presenter_.has_visible_popups()) {
        std::vector<const Notification*> const_popups;
        for (const auto* p : presenter_.visible_popups()) {
            const_popups.push_back(p);
        }
        auto hit = layout_.hit_test_popups(x, y, const_popups);
        if (hit.id != kInvalidNotificationId) {
            target_popup_id_ = hit.id;
            is_dismiss_target_ = hit.is_dismiss;
            is_action_target_ = hit.is_action;
            target_action_index_ = hit.action_index;
            return true;
        }
    }

    // 2. Check notification center hits if open
    if (center_state_.is_open()) {
        auto all_items = store_.all_notifications();
        auto hit = layout_.hit_test_center(x, y, all_items);
        if (!hit.is_inside) {
            is_outside_center_tap_ = true;
            return true;
        }

        if (hit.is_clear_all) {
            is_clear_all_target_ = true;
            return true;
        }

        if (hit.is_close) {
            is_close_target_ = true;
            return true;
        }

        if (hit.id != kInvalidNotificationId) {
            target_center_item_id_ = hit.id;
            return true;
        }

        return true; // Clicked inside panel
    }

    return false;
}

bool NotificationController::handle_touch_motion(int32_t x, int32_t y) {
    if (!is_touch_down_) return false;

    int32_t dx = x - last_touch_pos_.x;
    int32_t dy = y - last_touch_pos_.y;
    drag_offset_x_ += dx;

    if (center_state_.is_open() && target_popup_id_ == kInvalidNotificationId) {
        scroll_offset_y_ += dy;
        // Clamp scroll offset so we can't scroll too far down
        if (scroll_offset_y_ > 0) scroll_offset_y_ = 0;
    }

    last_touch_pos_ = {x, y};
    return (target_popup_id_ != kInvalidNotificationId || center_state_.is_open());
}

bool NotificationController::handle_touch_up(int32_t x, int32_t y) {
    (void)x;
    (void)y;
    if (!is_touch_down_) return false;
    is_touch_down_ = false;

    // 1. Popup interaction
    if (target_popup_id_ != kInvalidNotificationId) {
        // Check for horizontal swipe dismiss gesture
        if (std::abs(drag_offset_x_) >= kSwipeDismissThresholdPx) {
            LDDE_LOG_INFO(Notification, "Swipe dismiss gesture on notification [id=" << target_popup_id_ << "]");
            presenter_.dismiss(target_popup_id_);
            return true;
        }

        if (is_dismiss_target_) {
            presenter_.dismiss(target_popup_id_);
            return true;
        }

        auto* notif = store_.find(target_popup_id_);
        if (notif) {
            if (is_action_target_ && target_action_index_ < notif->actions().size()) {
                const auto& act = notif->actions()[target_action_index_];
                trigger_action(target_popup_id_, act.key);
                if (!notif->is_resident()) {
                    presenter_.dismiss(target_popup_id_);
                }
                return true;
            }

            // Tap on body -> default activation
            trigger_default(target_popup_id_);
            if (!notif->is_resident()) {
                presenter_.dismiss(target_popup_id_);
            }
            return true;
        }
    }

    // 2. Notification Center interaction
    if (center_state_.is_open()) {
        if (is_outside_center_tap_) {
            center_state_.transition_to(NotificationCenterState::Closed);
            return true;
        }

        if (is_clear_all_target_) {
            store_.clear_history();
            return true;
        }

        if (is_close_target_) {
            center_state_.transition_to(NotificationCenterState::Closed);
            return true;
        }

        if (target_center_item_id_ != kInvalidNotificationId) {
            trigger_default(target_center_item_id_);
            center_state_.transition_to(NotificationCenterState::Closed);
            return true;
        }
        return true;
    }

    return false;
}

void NotificationController::handle_touch_cancel() {
    is_touch_down_ = false;
    target_popup_id_ = kInvalidNotificationId;
    target_center_item_id_ = kInvalidNotificationId;
    is_outside_center_tap_ = false;
    drag_offset_x_ = 0;
}

bool NotificationController::handle_key(uint32_t key_symbol, uint32_t state, uint32_t /*modifiers*/) {
    if (state == 0) return false; // Key release

    // Escape
    if (key_symbol == 0xff1b) {
        if (center_state_.is_open()) {
            center_state_.transition_to(NotificationCenterState::Closed);
            return true;
        }
        if (presenter_.has_visible_popups()) {
            const auto& popups = presenter_.visible_popups();
            if (!popups.empty() && popups[0]) {
                presenter_.dismiss(popups[0]->id());
                return true;
            }
        }
    }

    // Enter / Space: activate topmost popup default action
    if ((key_symbol == 0xff0d || key_symbol == 0x0020) && presenter_.has_visible_popups()) {
        const auto& popups = presenter_.visible_popups();
        if (!popups.empty() && popups[0]) {
            NotificationId id = popups[0]->id();
            trigger_default(id);
            presenter_.dismiss(id);
            return true;
        }
    }

    return false;
}

void NotificationController::trigger_action(NotificationId id, const std::string& action_key) {
    for (const auto& cb : action_callbacks_) {
        if (cb) cb(id, action_key);
    }
}

void NotificationController::trigger_default(NotificationId id) {
    for (const auto& cb : default_callbacks_) {
        if (cb) cb(id);
    }
}

} // namespace ldde::notification

