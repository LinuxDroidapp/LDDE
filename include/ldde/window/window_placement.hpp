#pragma once

#include <cstdint>
#include <vector>
#include "ldde/core/types.hpp"
#include "ldde/display/display_info.hpp"

namespace ldde::window {

struct PlacementConstraints {
    int32_t status_bar_height = 40;
    int32_t dock_height = 68;
    int32_t margin_top = 8;
    int32_t margin_bottom = 8;
    int32_t margin_horizontal = 8;
    int32_t cascade_step = 32;
};

class WindowPlacement {
public:
    explicit WindowPlacement(PlacementConstraints constraints = {});

    [[nodiscard]] core::Rect calculate_initial_geometry(
        const display::DisplayInfo& display,
        size_t existing_window_count,
        const core::Size& requested_size = {0, 0},
        const core::Size& min_size = {200, 150},
        const core::Size& max_size = {0, 0}) const noexcept;

    [[nodiscard]] core::Rect get_usable_area(const display::DisplayInfo& display) const noexcept;

    [[nodiscard]] core::Rect clamp_to_usable(
        const core::Rect& geom,
        const core::Rect& usable_area,
        int32_t min_visible_titlebar = 36) const noexcept;

    [[nodiscard]] const PlacementConstraints& constraints() const noexcept { return constraints_; }
    void set_constraints(const PlacementConstraints& constraints) noexcept { constraints_ = constraints; }

private:
    PlacementConstraints constraints_;
};

} // namespace ldde::window
