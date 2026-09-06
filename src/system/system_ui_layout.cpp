#include "ldde/system/system_ui_layout.hpp"
#include <algorithm>

namespace ldde::system {

void SystemUILayout::update(
    const display::DisplayPolicy& policy,
    const shell::ShellLayout& shell_layout,
    const shell::DesignTokens& tokens,
    size_t quick_control_count) {
    scale_ = tokens.scale;
    is_portrait_ = policy.is_portrait();

    // 1. Status Bar Geometry
    status_bar_geom_ = shell_layout.status_geometry();

    // Clock geometry (left side)
    int32_t clock_w = static_cast<int32_t>(90 * scale_);
    clock_geom_ = core::Rect{
        status_bar_geom_.x + tokens.spacing_md_px,
        status_bar_geom_.y,
        clock_w,
        status_bar_geom_.height
    };

    // Indicators geometry (right side)
    int32_t slot_w = static_cast<int32_t>(32 * scale_);
    int32_t slot_h = status_bar_geom_.height;
    int32_t right_edge = status_bar_geom_.x + status_bar_geom_.width - tokens.spacing_md_px;

    // From right to left: Session -> Battery -> Audio -> Network
    session_icon_geom_ = core::Rect{right_edge - slot_w, status_bar_geom_.y, slot_w, slot_h};
    right_edge -= (slot_w + tokens.spacing_xs_px);

    battery_icon_geom_ = core::Rect{right_edge - slot_w, status_bar_geom_.y, slot_w, slot_h};
    right_edge -= (slot_w + tokens.spacing_xs_px);

    audio_icon_geom_ = core::Rect{right_edge - slot_w, status_bar_geom_.y, slot_w, slot_h};
    right_edge -= (slot_w + tokens.spacing_xs_px);

    network_icon_geom_ = core::Rect{right_edge - slot_w, status_bar_geom_.y, slot_w, slot_h};

    indicators_geom_ = core::Rect{
        network_icon_geom_.x,
        status_bar_geom_.y,
        status_bar_geom_.x + status_bar_geom_.width - tokens.spacing_md_px - network_icon_geom_.x,
        status_bar_geom_.height
    };

    // 2. System Panel Geometry
    int32_t panel_w;
    if (is_portrait_) {
        panel_w = std::min(status_bar_geom_.width - tokens.spacing_md_px * 2, static_cast<int32_t>(380 * scale_));
    } else {
        panel_w = std::min(static_cast<int32_t>(360 * scale_), status_bar_geom_.width - tokens.spacing_md_px * 2);
    }
    panel_w = std::max(panel_w, static_cast<int32_t>(280 * scale_));

    int32_t panel_x = status_bar_geom_.x + status_bar_geom_.width - panel_w - tokens.spacing_md_px;
    if (panel_x < status_bar_geom_.x + tokens.spacing_sm_px) {
        panel_x = status_bar_geom_.x + (status_bar_geom_.width - panel_w) / 2;
    }

    int32_t panel_y = status_bar_geom_.y + status_bar_geom_.height + tokens.spacing_xs_px;

    // Compute tile dimensions (2 columns)
    int32_t cols = 2;
    int32_t pad = tokens.spacing_md_px;
    int32_t inner_w = panel_w - pad * 2;
    int32_t tile_spacing = tokens.spacing_sm_px;
    int32_t tile_w = (inner_w - tile_spacing * (cols - 1)) / cols;
    int32_t tile_h = std::max(tokens.min_touch_target_px, static_cast<int32_t>(52 * scale_));

    size_t rows = (quick_control_count + cols - 1) / cols;

    int32_t header_h = static_cast<int32_t>(44 * scale_);
    int32_t status_summary_h = static_cast<int32_t>(88 * scale_);
    int32_t grid_y_offset = panel_y + pad + header_h + status_summary_h + tokens.spacing_sm_px;

    control_tile_geoms_.clear();
    control_tile_geoms_.reserve(quick_control_count);

    for (size_t i = 0; i < quick_control_count; ++i) {
        size_t r = i / cols;
        size_t c = i % cols;
        int32_t tx = panel_x + pad + static_cast<int32_t>(c) * (tile_w + tile_spacing);
        int32_t ty = grid_y_offset + static_cast<int32_t>(r) * (tile_h + tile_spacing);
        control_tile_geoms_.push_back(core::Rect{tx, ty, tile_w, tile_h});
    }

    int32_t grid_h = static_cast<int32_t>(rows) * tile_h + static_cast<int32_t>(rows > 0 ? rows - 1 : 0) * tile_spacing;
    int32_t panel_h = pad * 2 + header_h + status_summary_h + tokens.spacing_sm_px + grid_h;

    // Bound panel height to available screen height minus dock
    int32_t max_h = shell_layout.screen_bounds().height - panel_y - tokens.spacing_md_px;
    panel_h = std::min(panel_h, max_h);

    panel_geom_ = core::Rect{panel_x, panel_y, panel_w, panel_h};
}

const core::Rect* SystemUILayout::control_tile_geometry(size_t index) const noexcept {
    if (index < control_tile_geoms_.size()) {
        return &control_tile_geoms_[index];
    }
    return nullptr;
}

int32_t SystemUILayout::hit_test_control(const core::Point& pt) const noexcept {
    for (size_t i = 0; i < control_tile_geoms_.size(); ++i) {
        if (control_tile_geoms_[i].contains(pt)) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

} // namespace ldde::system
