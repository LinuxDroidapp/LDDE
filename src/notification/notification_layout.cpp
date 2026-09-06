#include "ldde/notification/notification_layout.hpp"
#include <algorithm>

namespace ldde::notification {

void NotificationLayout::update_popups(
    const display::DisplayPolicy& policy,
    const shell::ShellLayout& shell_layout,
    const shell::DesignTokens& tokens,
    std::vector<Notification*>& visible_popups) {
    scale_ = tokens.scale;
    is_portrait_ = policy.is_portrait();

    const auto& status_geom = shell_layout.status_geometry();
    int32_t screen_w = policy.display_info().pixel_width > 0 ?
                       policy.display_info().pixel_width : policy.display_info().logical_width;

    int32_t popup_w;
    if (is_portrait_) {
        popup_w = std::min(screen_w - tokens.spacing_md_px * 2, static_cast<int32_t>(380 * scale_));
    } else {
        popup_w = std::min(static_cast<int32_t>(360 * scale_), screen_w - tokens.spacing_md_px * 2);
    }
    popup_w = std::max(popup_w, static_cast<int32_t>(280 * scale_));

    int32_t popup_x;
    if (is_portrait_) {
        popup_x = (screen_w - popup_w) / 2;
    } else {
        popup_x = screen_w - popup_w - tokens.spacing_md_px;
    }

    int32_t curr_y = status_geom.y + status_geom.height + tokens.spacing_sm_px;
    int32_t dismiss_size = static_cast<int32_t>(48 * scale_);

    for (auto* notif : visible_popups) {
        if (!notif) continue;

        int32_t card_h;
        if (notif->actions().empty()) {
            card_h = static_cast<int32_t>(76 * scale_);
        } else {
            card_h = static_cast<int32_t>(112 * scale_);
        }

        core::Rect card_rect{popup_x, curr_y, popup_w, card_h};
        notif->set_popup_geometry(card_rect);

        // Dismiss button in top-right
        core::Rect dismiss_rect{
            card_rect.x + card_rect.width - dismiss_size,
            card_rect.y,
            dismiss_size,
            dismiss_size
        };
        notif->set_dismiss_button_geometry(dismiss_rect);

        // Action buttons at the bottom if present
        if (!notif->actions().empty()) {
            size_t n = notif->actions().size();
            int32_t act_h = static_cast<int32_t>(40 * scale_);
            int32_t act_y = card_rect.y + card_rect.height - act_h - tokens.spacing_xs_px;
            int32_t available_w = card_rect.width - tokens.spacing_md_px * 2 - static_cast<int32_t>((n - 1) * tokens.spacing_xs_px);
            int32_t act_w = std::max(static_cast<int32_t>(48 * scale_), available_w / static_cast<int32_t>(n));

            int32_t act_x = card_rect.x + tokens.spacing_md_px;
            for (size_t i = 0; i < n; ++i) {
                notif->set_action_geometry(i, core::Rect{act_x, act_y, act_w, act_h});
                act_x += act_w + tokens.spacing_xs_px;
            }
        }

        curr_y += card_h + tokens.spacing_sm_px;
    }
}

void NotificationLayout::update_notification_center(
    const display::DisplayPolicy& policy,
    const shell::ShellLayout& shell_layout,
    const shell::DesignTokens& tokens,
    const std::vector<const Notification*>& items,
    int32_t scroll_offset_y) {
    scale_ = tokens.scale;
    is_portrait_ = policy.is_portrait();

    const auto& status_geom = shell_layout.status_geometry();
    int32_t screen_w = policy.display_info().pixel_width > 0 ?
                       policy.display_info().pixel_width : policy.display_info().logical_width;
    int32_t screen_h = policy.display_info().pixel_height > 0 ?
                       policy.display_info().pixel_height : policy.display_info().logical_height;

    int32_t panel_w;
    int32_t panel_x;
    if (is_portrait_) {
        panel_w = screen_w - tokens.spacing_sm_px * 2;
        panel_x = tokens.spacing_sm_px;
    } else {
        panel_w = std::min(static_cast<int32_t>(420 * scale_), screen_w - tokens.spacing_md_px * 2);
        panel_x = screen_w - panel_w - tokens.spacing_md_px;
    }

    int32_t panel_y = status_geom.y + status_geom.height + tokens.spacing_xs_px;
    int32_t bottom_reserved = shell_layout.dock_geometry().height > 0 ?
                              shell_layout.dock_geometry().height + tokens.spacing_sm_px : tokens.spacing_md_px;
    int32_t panel_h = std::min(screen_h - panel_y - bottom_reserved, static_cast<int32_t>(640 * scale_));
    panel_h = std::max(panel_h, static_cast<int32_t>(200 * scale_));

    center_panel_geom_ = core::Rect{panel_x, panel_y, panel_w, panel_h};

    // Header: title + clear all
    int32_t header_h = static_cast<int32_t>(48 * scale_);
    center_header_geom_ = core::Rect{panel_x, panel_y, panel_w, header_h};

    int32_t clear_w = static_cast<int32_t>(96 * scale_);
    int32_t clear_h = static_cast<int32_t>(36 * scale_);
    center_clear_all_geom_ = core::Rect{
        panel_x + panel_w - clear_w - tokens.spacing_md_px,
        panel_y + (header_h - clear_h) / 2,
        clear_w,
        clear_h
    };

    center_close_geom_ = core::Rect{
        panel_x + tokens.spacing_md_px,
        panel_y + (header_h - clear_h) / 2,
        static_cast<int32_t>(36 * scale_),
        clear_h
    };

    // List area
    center_list_geom_ = core::Rect{
        panel_x,
        panel_y + header_h,
        panel_w,
        panel_h - header_h
    };

    // Items layout inside list
    center_item_geoms_.clear();
    center_item_geoms_.reserve(items.size());

    int32_t item_y = center_list_geom_.y + scroll_offset_y + tokens.spacing_sm_px;
    int32_t item_w = panel_w - tokens.spacing_md_px * 2;
    int32_t item_x = panel_x + tokens.spacing_md_px;

    for (const auto* notif : items) {
        if (!notif) continue;
        int32_t item_h = static_cast<int32_t>(76 * scale_);
        if (!notif->actions().empty()) {
            item_h = static_cast<int32_t>(112 * scale_);
        }
        center_item_geoms_.push_back(core::Rect{item_x, item_y, item_w, item_h});
        item_y += item_h + tokens.spacing_xs_px;
    }
}

PopupHitResult NotificationLayout::hit_test_popups(
    int32_t x,
    int32_t y,
    const std::vector<const Notification*>& popups) const noexcept {
    PopupHitResult result;
    core::Point pt{x, y};

    for (const auto* notif : popups) {
        if (!notif) continue;
        if (!notif->popup_geometry().contains(pt)) continue;

        result.id = notif->id();

        // 1. Check dismiss button
        if (notif->dismiss_button_geometry().contains(pt)) {
            result.is_dismiss = true;
            return result;
        }

        // 2. Check action buttons
        for (size_t i = 0; i < notif->actions().size(); ++i) {
            if (notif->actions()[i].geometry.contains(pt)) {
                result.is_action = true;
                result.action_index = i;
                return result;
            }
        }

        // 3. Clicked card body
        return result;
    }

    return result;
}

CenterHitResult NotificationLayout::hit_test_center(
    int32_t x,
    int32_t y,
    const std::vector<const Notification*>& items) const noexcept {
    CenterHitResult result;
    core::Point pt{x, y};

    if (!center_panel_geom_.contains(pt)) {
        result.is_inside = false;
        return result;
    }

    result.is_inside = true;

    // Check clear all
    if (center_clear_all_geom_.contains(pt)) {
        result.is_clear_all = true;
        return result;
    }

    // Check close
    if (center_close_geom_.contains(pt)) {
        result.is_close = true;
        return result;
    }

    // Check items
    for (size_t i = 0; i < center_item_geoms_.size() && i < items.size(); ++i) {
        if (center_item_geoms_[i].contains(pt) && items[i]) {
            result.id = items[i]->id();
            return result;
        }
    }

    return result;
}

} // namespace ldde::notification

