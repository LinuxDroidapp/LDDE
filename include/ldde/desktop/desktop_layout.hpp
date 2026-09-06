#pragma once

#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/shell/types.hpp"

namespace ldde::desktop {

enum class DesktopFormFactor {
    PhonePortrait,
    PhoneLandscape,
    Tablet,
    Desktop
};

class DesktopLayout {
public:
    DesktopLayout();
    ~DesktopLayout() = default;

    void update(const display::DisplayPolicy& policy,
                const shell::DesignTokens& tokens,
                shell::DockPosition dock_position = shell::DockPosition::Bottom);

    [[nodiscard]] DesktopFormFactor form_factor() const noexcept { return form_factor_; }
    [[nodiscard]] display::LayoutClass layout_class() const noexcept { return layout_class_; }

    [[nodiscard]] const core::Rect& screen_bounds() const noexcept { return screen_bounds_; }
    [[nodiscard]] const core::Rect& safe_bounds() const noexcept { return safe_bounds_; }
    [[nodiscard]] const core::Rect& workspace_bounds() const noexcept { return workspace_bounds_; }
    [[nodiscard]] const core::Rect& status_bounds() const noexcept { return status_bounds_; }
    [[nodiscard]] const core::Rect& dock_bounds() const noexcept { return dock_bounds_; }

    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] bool contains_point(const core::Point& pt) const noexcept;
    [[nodiscard]] bool is_in_workspace(const core::Point& pt) const noexcept;

private:
    DesktopFormFactor form_factor_ = DesktopFormFactor::PhonePortrait;
    display::LayoutClass layout_class_ = display::LayoutClass::Compact;

    core::Rect screen_bounds_{0, 0, 0, 0};
    core::Rect safe_bounds_{0, 0, 0, 0};
    core::Rect workspace_bounds_{0, 0, 0, 0};
    core::Rect status_bounds_{0, 0, 0, 0};
    core::Rect dock_bounds_{0, 0, 0, 0};

    double scale_ = 1.0;

    void compute_form_factor(const display::DisplayPolicy& policy);
};

} // namespace ldde::desktop
