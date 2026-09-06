#pragma once

#include <cstdint>
#include <vector>
#include "ldde/core/types.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/shell/shell_layout.hpp"

namespace ldde::system {

class SystemUILayout {
public:
    SystemUILayout() = default;

    void update(const display::DisplayPolicy& policy,
                const shell::ShellLayout& shell_layout,
                const shell::DesignTokens& tokens,
                size_t quick_control_count);

    [[nodiscard]] const core::Rect& status_bar_geometry() const noexcept { return status_bar_geom_; }
    [[nodiscard]] const core::Rect& clock_geometry() const noexcept { return clock_geom_; }
    [[nodiscard]] const core::Rect& indicators_geometry() const noexcept { return indicators_geom_; }

    [[nodiscard]] const core::Rect& network_icon_geometry() const noexcept { return network_icon_geom_; }
    [[nodiscard]] const core::Rect& audio_icon_geometry() const noexcept { return audio_icon_geom_; }
    [[nodiscard]] const core::Rect& battery_icon_geometry() const noexcept { return battery_icon_geom_; }
    [[nodiscard]] const core::Rect& session_icon_geometry() const noexcept { return session_icon_geom_; }

    [[nodiscard]] const core::Rect& panel_geometry() const noexcept { return panel_geom_; }
    [[nodiscard]] const std::vector<core::Rect>& control_tile_geometries() const noexcept {
        return control_tile_geoms_;
    }
    [[nodiscard]] const core::Rect* control_tile_geometry(size_t index) const noexcept;

    [[nodiscard]] bool is_point_in_panel(const core::Point& pt) const noexcept {
        return panel_geom_.contains(pt);
    }
    [[nodiscard]] bool is_point_in_status_bar(const core::Point& pt) const noexcept {
        return status_bar_geom_.contains(pt);
    }
    [[nodiscard]] int32_t hit_test_control(const core::Point& pt) const noexcept;

    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] bool is_portrait() const noexcept { return is_portrait_; }

private:
    double scale_ = 1.0;
    bool is_portrait_ = true;

    core::Rect status_bar_geom_;
    core::Rect clock_geom_;
    core::Rect indicators_geom_;

    core::Rect network_icon_geom_;
    core::Rect audio_icon_geom_;
    core::Rect battery_icon_geom_;
    core::Rect session_icon_geom_;

    core::Rect panel_geom_;
    std::vector<core::Rect> control_tile_geoms_;
};

} // namespace ldde::system
