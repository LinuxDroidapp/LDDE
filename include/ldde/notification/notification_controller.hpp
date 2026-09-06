#pragma once

#include <functional>
#include <memory>
#include "ldde/core/types.hpp"
#include "ldde/notification/notification_store.hpp"
#include "ldde/notification/notification_presenter.hpp"
#include "ldde/notification/notification_center_state.hpp"
#include "ldde/notification/notification_layout.hpp"

namespace ldde::notification {

class NotificationController {
public:
    using ActionActivatedCallback = std::function<void(NotificationId id, const std::string& action_key)>;
    using DefaultActivatedCallback = std::function<void(NotificationId id)>;

    NotificationController(
        NotificationStore& store,
        NotificationPresenter& presenter,
        NotificationCenterStateMachine& center_state,
        NotificationLayout& layout);

    // Touch handlers
    bool handle_touch_down(int32_t x, int32_t y);
    bool handle_touch_motion(int32_t x, int32_t y);
    bool handle_touch_up(int32_t x, int32_t y);
    void handle_touch_cancel();

    // Keyboard handlers
    bool handle_key(uint32_t key_symbol, uint32_t state, uint32_t modifiers);

    void on_action_activated(ActionActivatedCallback cb) { action_callbacks_.push_back(std::move(cb)); }
    void on_default_activated(DefaultActivatedCallback cb) { default_callbacks_.push_back(std::move(cb)); }

    [[nodiscard]] int32_t scroll_offset_y() const noexcept { return scroll_offset_y_; }
    void set_scroll_offset_y(int32_t offset) noexcept { scroll_offset_y_ = offset; }

private:
    void trigger_action(NotificationId id, const std::string& action_key);
    void trigger_default(NotificationId id);

    NotificationStore& store_;
    NotificationPresenter& presenter_;
    NotificationCenterStateMachine& center_state_;
    NotificationLayout& layout_;

    bool is_touch_down_ = false;
    core::Point touch_down_pos_{0, 0};
    core::Point last_touch_pos_{0, 0};
    int32_t drag_offset_x_ = 0;
    int32_t scroll_offset_y_ = 0;

    NotificationId target_popup_id_ = kInvalidNotificationId;
    bool is_dismiss_target_ = false;
    bool is_action_target_ = false;
    size_t target_action_index_ = 0;

    bool is_outside_center_tap_ = false;
    bool is_clear_all_target_ = false;
    bool is_close_target_ = false;
    NotificationId target_center_item_id_ = kInvalidNotificationId;

    std::vector<ActionActivatedCallback> action_callbacks_;
    std::vector<DefaultActivatedCallback> default_callbacks_;
};

} // namespace ldde::notification

