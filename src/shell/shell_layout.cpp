#include "ldde/shell/shell_layout.hpp"
#include <algorithm>

namespace ldde::shell {

ShellLayout::ShellLayout() {
    screen_bounds_ = core::Rect{0, 0, 720, 1280};
    safe_area_ = screen_bounds_;
    desktop_geometry_ = screen_bounds_;
    overlay_geometry_ = screen_bounds_;
}

void ShellLayout::update(const display::DisplayPolicy& policy,
                         const DesignTokens& tokens,
                         DockPosition dock_pos) {
    screen_bounds_ = policy.available_geometry().full_bounds;
    safe_area_ = policy.available_geometry().safe_bounds;
    safe_insets_ = policy.display_info().safe_insets.to_core_insets();
    scale_ = policy.scale_policy().effective_scale();

    // Desktop Area: covers full screen
    desktop_geometry_ = screen_bounds_;

    // Overlay Area: covers full screen
    overlay_geometry_ = screen_bounds_;

    // Status Area: anchored to top of safe area, height from tokens or centralized LayoutMetrics
    status_geometry_.x = safe_area_.x;
    status_geometry_.y = safe_area_.y;
    status_geometry_.width = safe_area_.width;
    status_geometry_.height = tokens.status_height_px > 0 ? tokens.status_height_px : policy.metrics().status_bar_height_px;

    // Dock Area: responsive width, anchored to bottom of safe area
    bool portrait = policy.is_portrait();
    double width_ratio = portrait ? DesignTokens::kDockWidthPortraitRatio : DesignTokens::kDockWidthLandscapeRatio;
    int32_t dock_w = std::min(safe_area_.width, static_cast<int32_t>(safe_area_.width * width_ratio));
    int32_t dock_h = tokens.dock_height_px > 0 ? tokens.dock_height_px : policy.metrics().dock_height_px;
    int32_t margin_bot = tokens.dock_margin_bottom_px;

    if (dock_pos == DockPosition::Bottom) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + (safe_area_.width - dock_w) / 2;
        dock_geometry_.y = safe_area_.y + safe_area_.height - dock_h - margin_bot;
    } else if (dock_pos == DockPosition::Top) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + (safe_area_.width - dock_w) / 2;
        dock_geometry_.y = safe_area_.y + status_geometry_.height + tokens.spacing_sm_px;
    } else if (dock_pos == DockPosition::Left) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + margin_bot;
        dock_geometry_.y = safe_area_.y + safe_area_.height - dock_h - margin_bot;
    } else if (dock_pos == DockPosition::Right) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + safe_area_.width - dock_w - margin_bot;
        dock_geometry_.y = safe_area_.y + safe_area_.height - dock_h - margin_bot;
    }
}

void ShellLayout::update(const display::DisplayInfo& display_info,
                         const DesignTokens& tokens,
                         DockPosition dock_pos) {
    int32_t w = display_info.width > 0 ? display_info.width : 720;
    int32_t h = display_info.height > 0 ? display_info.height : 1280;

    screen_bounds_ = core::Rect{0, 0, w, h};
    safe_insets_ = display_info.safe_insets.to_core_insets();
    scale_ = tokens.scale;

    // Safe Area
    safe_area_.x = safe_insets_.left;
    safe_area_.y = safe_insets_.top;
    safe_area_.width = std::max(0, screen_bounds_.width - safe_insets_.left - safe_insets_.right);
    safe_area_.height = std::max(0, screen_bounds_.height - safe_insets_.top - safe_insets_.bottom);

    // Desktop Area: covers full screen
    desktop_geometry_ = screen_bounds_;

    // Overlay Area: covers full screen
    overlay_geometry_ = screen_bounds_;

    // Status Area: anchored to top of safe area
    status_geometry_.x = safe_area_.x;
    status_geometry_.y = safe_area_.y;
    status_geometry_.width = safe_area_.width;
    status_geometry_.height = tokens.status_height_px;

    // Dock Area: responsive width, anchored to bottom of safe area
    bool portrait = is_portrait();
    double width_ratio = portrait ? DesignTokens::kDockWidthPortraitRatio : DesignTokens::kDockWidthLandscapeRatio;
    int32_t dock_w = std::min(safe_area_.width, static_cast<int32_t>(safe_area_.width * width_ratio));
    int32_t dock_h = tokens.dock_height_px;

    if (dock_pos == DockPosition::Bottom) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + (safe_area_.width - dock_w) / 2;
        dock_geometry_.y = safe_area_.y + safe_area_.height - dock_h - tokens.dock_margin_bottom_px;
    } else if (dock_pos == DockPosition::Top) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + (safe_area_.width - dock_w) / 2;
        dock_geometry_.y = safe_area_.y + status_geometry_.height + tokens.spacing_sm_px;
    } else if (dock_pos == DockPosition::Left) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + tokens.dock_margin_bottom_px;
        dock_geometry_.y = safe_area_.y + safe_area_.height - dock_h - tokens.dock_margin_bottom_px;
    } else if (dock_pos == DockPosition::Right) {
        dock_geometry_.width = dock_w;
        dock_geometry_.height = dock_h;
        dock_geometry_.x = safe_area_.x + safe_area_.width - dock_w - tokens.dock_margin_bottom_px;
        dock_geometry_.y = safe_area_.y + safe_area_.height - dock_h - tokens.dock_margin_bottom_px;
    }
}

ShellRegionType ShellLayout::hit_test(const core::Point& pt, bool overlay_active) const noexcept {
    if (overlay_active && overlay_geometry_.contains(pt)) {
        return ShellRegionType::Overlay;
    }
    if (dock_geometry_.contains(pt)) {
        return ShellRegionType::Dock;
    }
    if (status_geometry_.contains(pt)) {
        return ShellRegionType::Status;
    }
    if (desktop_geometry_.contains(pt)) {
        return ShellRegionType::Desktop;
    }
    return ShellRegionType::None;
}

} // namespace ldde::shell
