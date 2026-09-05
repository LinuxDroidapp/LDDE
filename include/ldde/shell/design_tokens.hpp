#pragma once

#include <cstdint>
#include <cmath>

namespace ldde::shell {

struct DesignTokens {
    // DP baseline metrics
    static constexpr int32_t kStatusHeightDp = 40;
    static constexpr int32_t kDockHeightDp = 68;
    static constexpr int32_t kDockMarginBottomDp = 16;
    static constexpr int32_t kDockCornerRadiusDp = 24;
    static constexpr int32_t kMinTouchTargetDp = 48;

    static constexpr int32_t kSpacingXsDp = 4;
    static constexpr int32_t kSpacingSmDp = 8;
    static constexpr int32_t kSpacingMdDp = 16;
    static constexpr int32_t kSpacingLgDp = 24;

    static constexpr double kDockWidthPortraitRatio = 0.90;
    static constexpr double kDockWidthLandscapeRatio = 0.60;

    // Scaled pixel values
    int32_t status_height_px = kStatusHeightDp;
    int32_t dock_height_px = kDockHeightDp;
    int32_t dock_margin_bottom_px = kDockMarginBottomDp;
    int32_t dock_corner_radius_px = kDockCornerRadiusDp;
    int32_t min_touch_target_px = kMinTouchTargetDp;

    int32_t spacing_xs_px = kSpacingXsDp;
    int32_t spacing_sm_px = kSpacingSmDp;
    int32_t spacing_md_px = kSpacingMdDp;
    int32_t spacing_lg_px = kSpacingLgDp;

    double scale = 1.0;

    static int32_t scale_dp(int32_t dp, double scale_factor) noexcept {
        return static_cast<int32_t>(std::round(static_cast<double>(dp) * scale_factor));
    }

    static DesignTokens create_scaled(double scale_factor) noexcept {
        if (scale_factor <= 0.0) scale_factor = 1.0;
        DesignTokens t;
        t.scale = scale_factor;
        t.status_height_px = scale_dp(kStatusHeightDp, scale_factor);
        t.dock_height_px = scale_dp(kDockHeightDp, scale_factor);
        t.dock_margin_bottom_px = scale_dp(kDockMarginBottomDp, scale_factor);
        t.dock_corner_radius_px = scale_dp(kDockCornerRadiusDp, scale_factor);
        t.min_touch_target_px = scale_dp(kMinTouchTargetDp, scale_factor);
        t.spacing_xs_px = scale_dp(kSpacingXsDp, scale_factor);
        t.spacing_sm_px = scale_dp(kSpacingSmDp, scale_factor);
        t.spacing_md_px = scale_dp(kSpacingMdDp, scale_factor);
        t.spacing_lg_px = scale_dp(kSpacingLgDp, scale_factor);
        return t;
    }
};

} // namespace ldde::shell
