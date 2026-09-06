#pragma once

#include <vector>
#include <cstdint>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/shell_layout.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/notification/notification.hpp"

namespace ldde::notification {

struct PopupHitResult {
    NotificationId id = kInvalidNotificationId;
    bool is_dismiss = false;
    bool is_action = false;
    size_t action_index = 0;
};

struct CenterHitResult {
    bool is_inside = false;
    bool is_clear_all = false;
    bool is_close = false;
    NotificationId id = kInvalidNotificationId;
    bool is_dismiss = false;
    bool is_action = false;
    size_t action_index = 0;
};

class NotificationLayout {
public:
    NotificationLayout() = default;

    void update_popups(
        const display::DisplayPolicy& policy,
        const shell::ShellLayout& shell_layout,
        const shell::DesignTokens& tokens,
        std::vector<Notification*>& visible_popups);

    void update_notification_center(
        const display::DisplayPolicy& policy,
        const shell::ShellLayout& shell_layout,
        const shell::DesignTokens& tokens,
        const std::vector<const Notification*>& items,
        int32_t scroll_offset_y = 0);

    // Bounding boxes
    [[nodiscard]] const core::Rect& center_panel_geometry() const noexcept { return center_panel_geom_; }
    [[nodiscard]] const core::Rect& center_header_geometry() const noexcept { return center_header_geom_; }
    [[nodiscard]] const core::Rect& center_clear_all_geometry() const noexcept { return center_clear_all_geom_; }
    [[nodiscard]] const core::Rect& center_close_geometry() const noexcept { return center_close_geom_; }
    [[nodiscard]] const core::Rect& center_list_geometry() const noexcept { return center_list_geom_; }

    [[nodiscard]] const std::vector<core::Rect>& center_item_geometries() const noexcept { return center_item_geoms_; }

    // Hit testing
    [[nodiscard]] PopupHitResult hit_test_popups(int32_t x, int32_t y, const std::vector<const Notification*>& popups) const noexcept;
    [[nodiscard]] CenterHitResult hit_test_center(int32_t x, int32_t y, const std::vector<const Notification*>& items) const noexcept;

    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] bool is_portrait() const noexcept { return is_portrait_; }

private:
    double scale_ = 1.0;
    bool is_portrait_ = true;

    core::Rect center_panel_geom_{0, 0, 0, 0};
    core::Rect center_header_geom_{0, 0, 0, 0};
    core::Rect center_clear_all_geom_{0, 0, 0, 0};
    core::Rect center_close_geom_{0, 0, 0, 0};
    core::Rect center_list_geom_{0, 0, 0, 0};
    std::vector<core::Rect> center_item_geoms_;
};

} // namespace ldde::notification

