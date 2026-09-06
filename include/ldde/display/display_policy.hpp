#pragma once

#include <cstdint>
#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/orientation.hpp"
#include "ldde/display/safe_area.hpp"
#include "ldde/display/scale_policy.hpp"
#include "ldde/display/responsive_layout.hpp"
#include "ldde/display/layout_metrics.hpp"
#include "ldde/display/display_geometry.hpp"

namespace ldde::display {

class DisplayPolicy {
public:
    DisplayPolicy();
    explicit DisplayPolicy(const DisplayInfo& info);
    ~DisplayPolicy() = default;

    // Display update & configuration
    void update_display(const DisplayInfo& info);
    void set_shell_reservations(const ShellReservations& reservations);
    void set_layout_class_override(std::optional<LayoutClass> override_class);
    void set_safe_insets_override(std::optional<SafeInsets> override_insets);

    // Snapshot queries
    [[nodiscard]] const DisplayInfo& display_info() const noexcept { return info_; }
    [[nodiscard]] const ScalePolicy& scale_policy() const noexcept { return scale_policy_; }
    [[nodiscard]] Orientation orientation() const noexcept { return orientation_; }
    [[nodiscard]] LayoutClass layout_class() const noexcept { return layout_class_; }
    [[nodiscard]] MultiWindowHint multi_window_hint() const noexcept { return multi_window_hint_; }
    [[nodiscard]] const LayoutMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const AvailableGeometry& available_geometry() const noexcept { return available_geometry_; }
    [[nodiscard]] const ShellReservations& shell_reservations() const noexcept { return reservations_; }

    [[nodiscard]] bool is_portrait() const noexcept { return display::is_portrait(orientation_); }
    [[nodiscard]] bool is_landscape() const noexcept { return display::is_landscape(orientation_); }

    // Authoritative Window Geometry Calculations (consumed by D3 WindowManager)
    [[nodiscard]] core::Rect available_window_geometry() const noexcept {
        return available_geometry_.window_bounds;
    }

    [[nodiscard]] core::Rect maximized_geometry() const noexcept;
    [[nodiscard]] core::Rect fullscreen_geometry() const noexcept;

    [[nodiscard]] core::Size default_window_size(
        const core::Size& requested_size = {0, 0},
        const core::Size& min_size = {0, 0},
        const core::Size& max_size = {0, 0}) const noexcept;

    [[nodiscard]] core::Rect calculate_initial_window_geometry(
        size_t window_index,
        const core::Size& requested_size = {0, 0},
        const core::Size& min_size = {0, 0},
        const core::Size& max_size = {0, 0}) const noexcept;

    [[nodiscard]] core::Rect constrain_window_geometry(
        const core::Rect& requested,
        const core::Size& min_size = {0, 0}) const noexcept;

    [[nodiscard]] core::Rect restore_window_geometry(
        const std::optional<core::Rect>& saved_geometry,
        const core::Size& min_size = {0, 0}) const noexcept;

private:
    DisplayInfo info_;
    ScalePolicy scale_policy_;
    Orientation orientation_ = Orientation::Portrait;
    LayoutClass layout_class_ = LayoutClass::Compact;
    MultiWindowHint multi_window_hint_ = MultiWindowHint::SingleDominant;
    LayoutMetrics metrics_;
    ShellReservations reservations_;
    AvailableGeometry available_geometry_;

    std::optional<LayoutClass> layout_class_override_;
    std::optional<SafeInsets> safe_insets_override_;

    void recalculate();
};

} // namespace ldde::display

