#pragma once

#include "ldde/core/types.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/shell/types.hpp"
#include "ldde/shell/design_tokens.hpp"

namespace ldde::shell {

class ShellLayout {
public:
    ShellLayout();
    ~ShellLayout() = default;

    void update(const display::DisplayInfo& display_info,
                const DesignTokens& tokens,
                DockPosition dock_pos = DockPosition::Bottom);

    [[nodiscard]] const core::Rect& screen_bounds() const noexcept { return screen_bounds_; }
    [[nodiscard]] const core::Rect& safe_area() const noexcept { return safe_area_; }
    [[nodiscard]] const core::Rect& desktop_geometry() const noexcept { return desktop_geometry_; }
    [[nodiscard]] const core::Rect& status_geometry() const noexcept { return status_geometry_; }
    [[nodiscard]] const core::Rect& dock_geometry() const noexcept { return dock_geometry_; }
    [[nodiscard]] const core::Rect& overlay_geometry() const noexcept { return overlay_geometry_; }

    [[nodiscard]] const core::Insets& safe_insets() const noexcept { return safe_insets_; }
    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] bool is_portrait() const noexcept { return screen_bounds_.height > screen_bounds_.width; }
    [[nodiscard]] bool is_landscape() const noexcept { return screen_bounds_.width >= screen_bounds_.height; }

    [[nodiscard]] ShellRegionType hit_test(const core::Point& pt, bool overlay_active = false) const noexcept;

private:
    core::Rect screen_bounds_;
    core::Rect safe_area_;
    core::Insets safe_insets_;
    double scale_ = 1.0;

    core::Rect desktop_geometry_;
    core::Rect status_geometry_;
    core::Rect dock_geometry_;
    core::Rect overlay_geometry_;
};

} // namespace ldde::shell
