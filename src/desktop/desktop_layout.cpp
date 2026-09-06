#include "ldde/desktop/desktop_layout.hpp"
#include "ldde/core/logging.hpp"
#include <algorithm>

namespace ldde::desktop {

DesktopLayout::DesktopLayout() = default;

void DesktopLayout::compute_form_factor(const display::DisplayPolicy& policy) {
    layout_class_ = policy.layout_class();

    if (layout_class_ == display::LayoutClass::Expanded) {
        form_factor_ = DesktopFormFactor::Desktop;
    } else if (layout_class_ == display::LayoutClass::Standard) {
        if (policy.is_portrait()) {
            form_factor_ = DesktopFormFactor::Tablet;
        } else {
            form_factor_ = DesktopFormFactor::PhoneLandscape;
        }
    } else {
        if (policy.is_portrait()) {
            form_factor_ = DesktopFormFactor::PhonePortrait;
        } else {
            form_factor_ = DesktopFormFactor::PhoneLandscape;
        }
    }
}

void DesktopLayout::update(const display::DisplayPolicy& policy,
                           const shell::DesignTokens& tokens,
                           shell::DockPosition dock_position) {
    scale_ = policy.scale_policy().effective_scale();
    compute_form_factor(policy);

    int32_t lw = policy.display_info().logical_width;
    int32_t lh = policy.display_info().logical_height;
    if (lw <= 0) lw = policy.display_info().width;
    if (lh <= 0) lh = policy.display_info().height;

    screen_bounds_ = core::Rect{0, 0, lw, lh};

    const auto& avail = policy.available_geometry();
    if (avail.safe_bounds.width > 0 && avail.safe_bounds.height > 0) {
        safe_bounds_ = avail.safe_bounds;
    } else {
        safe_bounds_ = screen_bounds_;
    }

    int32_t sb_h = tokens.status_height_px;
    if (sb_h <= 0) sb_h = static_cast<int32_t>(40 * scale_);

    int32_t dk_h = tokens.dock_height_px;
    if (dk_h <= 0) dk_h = static_cast<int32_t>(68 * scale_);

    status_bounds_ = core::Rect{0, 0, lw, sb_h};

    switch (dock_position) {
        case shell::DockPosition::Top:
            dock_bounds_ = core::Rect{0, sb_h, lw, dk_h};
            workspace_bounds_ = core::Rect{
                safe_bounds_.x,
                sb_h + dk_h,
                safe_bounds_.width,
                std::max(0, lh - sb_h - dk_h)
            };
            break;

        case shell::DockPosition::Left:
            dock_bounds_ = core::Rect{0, sb_h, dk_h, std::max(0, lh - sb_h)};
            workspace_bounds_ = core::Rect{
                dk_h,
                sb_h,
                std::max(0, lw - dk_h),
                std::max(0, lh - sb_h)
            };
            break;

        case shell::DockPosition::Right:
            dock_bounds_ = core::Rect{std::max(0, lw - dk_h), sb_h, dk_h, std::max(0, lh - sb_h)};
            workspace_bounds_ = core::Rect{
                0,
                sb_h,
                std::max(0, lw - dk_h),
                std::max(0, lh - sb_h)
            };
            break;

        case shell::DockPosition::Bottom:
        default:
            dock_bounds_ = core::Rect{0, std::max(0, lh - dk_h), lw, dk_h};
            workspace_bounds_ = core::Rect{
                safe_bounds_.x,
                sb_h,
                safe_bounds_.width,
                std::max(0, lh - sb_h - dk_h)
            };
            break;
    }

    LDDE_LOG_DEBUG(Desktop, "DesktopLayout updated: screen=" << screen_bounds_.width << "x" << screen_bounds_.height
                           << ", workspace=" << workspace_bounds_.width << "x" << workspace_bounds_.height
                           << ", form_factor=" << static_cast<int>(form_factor_));
}

bool DesktopLayout::contains_point(const core::Point& pt) const noexcept {
    return screen_bounds_.contains(pt);
}

bool DesktopLayout::is_in_workspace(const core::Point& pt) const noexcept {
    return workspace_bounds_.contains(pt);
}

} // namespace ldde::desktop
