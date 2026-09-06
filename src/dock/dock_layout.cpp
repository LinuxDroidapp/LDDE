#include "ldde/dock/dock_layout.hpp"

namespace ldde::dock {

void DockLayout::update(const display::DisplayPolicy& policy,
                        const shell::DesignTokens& tokens,
                        const core::Rect& dock_geometry,
                        size_t item_count,
                        int32_t custom_item_size,
                        int32_t custom_spacing) {
    (void)policy;
    dock_geometry_ = dock_geometry;
    double scale = tokens.scale > 0.0 ? tokens.scale : 1.0;

    padding_v_ = std::max(4, static_cast<int32_t>(6 * scale));
    padding_h_ = std::max(8, static_cast<int32_t>(12 * scale));

    if (custom_item_size > 0) {
        item_size_ = static_cast<int32_t>(custom_item_size * scale);
    } else {
        int32_t max_item_h = std::max(24, dock_geometry_.height - padding_v_ * 2);
        item_size_ = std::min(max_item_h, std::max(tokens.min_touch_target_px, static_cast<int32_t>(48 * scale)));
    }

    if (custom_spacing > 0) {
        item_spacing_ = static_cast<int32_t>(custom_spacing * scale);
    } else {
        item_spacing_ = tokens.spacing_sm_px > 0 ? tokens.spacing_sm_px : static_cast<int32_t>(8 * scale);
    }

    separator_w_ = std::max(4, static_cast<int32_t>(8 * scale));

    int32_t item_y = (dock_geometry_.height - item_size_) / 2;

    // 1. Launcher button at the start
    launcher_button_rect_ = core::Rect{padding_h_, item_y, item_size_, item_size_};

    // 2. Items start after launcher button and separator
    int32_t current_x = padding_h_ + item_size_ + separator_w_;

    item_rects_.clear();
    item_rects_.reserve(item_count);

    for (size_t i = 0; i < item_count; ++i) {
        item_rects_.push_back(core::Rect{current_x, item_y, item_size_, item_size_});
        current_x += item_size_ + item_spacing_;
    }

    if (item_count > 0) {
        total_content_width_ = current_x - item_spacing_ + padding_h_;
    } else {
        total_content_width_ = padding_h_ + item_size_ + padding_h_;
    }

    int32_t available_w = dock_geometry_.width;
    if (total_content_width_ > available_w) {
        max_scroll_x_ = total_content_width_ - available_w;
    } else {
        max_scroll_x_ = 0;
        // If content fits and does not overflow, center the content inside the dock pill
        int32_t center_offset = (available_w - total_content_width_) / 2;
        if (center_offset > 0) {
            launcher_button_rect_.x += center_offset;
            for (auto& r : item_rects_) {
                r.x += center_offset;
            }
        }
    }

    scroll_offset_x_ = std::clamp(scroll_offset_x_, 0, max_scroll_x_);
}

DockHitResult DockLayout::hit_test(int32_t local_x, int32_t local_y) const noexcept {
    core::Point pt{local_x, local_y};

    // Check if inside dock bounds
    if (local_x < 0 || local_y < 0 || local_x > dock_geometry_.width || local_y > dock_geometry_.height) {
        return DockHitResult{DockHitType::None, -1};
    }

    // Check launcher button (launcher button stays at fixed position or scrolls with content)
    // Here, launcher button is positioned at launcher_button_rect_ (shifted by scroll if scrolled)
    core::Rect visible_launcher = launcher_button_rect_;
    visible_launcher.x -= scroll_offset_x_;
    if (visible_launcher.contains(pt)) {
        return DockHitResult{DockHitType::LauncherButton, -1};
    }

    // Check items
    for (size_t i = 0; i < item_rects_.size(); ++i) {
        core::Rect r = visible_item_rect(i);
        if (r.contains(pt)) {
            return DockHitResult{DockHitType::Item, static_cast<int32_t>(i)};
        }
    }

    return DockHitResult{DockHitType::Background, -1};
}

core::Rect DockLayout::visible_item_rect(size_t index) const noexcept {
    if (index >= item_rects_.size()) {
        return core::Rect{0, 0, 0, 0};
    }
    core::Rect r = item_rects_[index];
    r.x -= scroll_offset_x_;
    return r;
}

} // namespace ldde::dock
