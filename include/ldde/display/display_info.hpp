#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <cmath>
#include "ldde/core/types.hpp"
#include "ldde/display/orientation.hpp"
#include "ldde/display/safe_area.hpp"
#include "ldde/display/display_geometry.hpp"

namespace ldde::display {

using DisplayId = uint32_t;

struct DisplayMode {
    int32_t width = 0;             // Pixel width
    int32_t height = 0;            // Pixel height
    int32_t refresh_rate_mhz = 0;  // Millihertz (e.g. 60000 = 60Hz)
    bool is_current = false;
    bool is_preferred = false;

    constexpr bool operator==(const DisplayMode& other) const = default;
};

struct DisplayInfo {
    DisplayId id = 0;
    std::string name;
    std::string make;
    std::string model;
    std::string description;

    // Physical pixels (mode dimensions)
    int32_t pixel_width = 0;
    int32_t pixel_height = 0;

    // Logical dimensions (Wayland compositor space)
    int32_t logical_x = 0;
    int32_t logical_y = 0;
    int32_t logical_width = 0;
    int32_t logical_height = 0;

    // Backward-compatibility aliases for logical width and height
    int32_t width = 0;
    int32_t height = 0;

    // Physical dimensions in millimeters
    int32_t physical_width_mm = 0;
    int32_t physical_height_mm = 0;

    // Refresh rate of active mode (mHz)
    int32_t refresh_rate_mhz = 0;

    // Scale factor
    int32_t scale = 1;

    // Orientation / Transform
    DisplayTransform transform = DisplayTransform::Normal;
    Orientation orientation = Orientation::Portrait;

    // Available desktop geometry (within compositor space)
    core::Rect geometry;

    // Safe insets (e.g. cutouts, rounded corners, status/nav margins)
    SafeInsets safe_insets;

    // Central available geometry calculation snapshot
    AvailableGeometry available_geometry;

    // Supported display modes
    std::vector<DisplayMode> modes;

    [[nodiscard]] double refresh_rate_hz() const noexcept {
        return static_cast<double>(refresh_rate_mhz) / 1000.0;
    }

    [[nodiscard]] bool is_portrait() const noexcept {
        if (logical_width > 0 && logical_height > 0) {
            return logical_height > logical_width;
        }
        if (width > 0 && height > 0) {
            return height > width;
        }
        return display::is_portrait(orientation);
    }

    [[nodiscard]] bool is_landscape() const noexcept {
        if (logical_width > 0 && logical_height > 0) {
            return logical_width >= logical_height;
        }
        if (width > 0 && height > 0) {
            return width >= height;
        }
        return display::is_landscape(orientation);
    }

    [[nodiscard]] double physical_diagonal_mm() const noexcept {
        if (physical_width_mm <= 0 || physical_height_mm <= 0) return 0.0;
        return std::hypot(static_cast<double>(physical_width_mm), static_cast<double>(physical_height_mm));
    }

    [[nodiscard]] double physical_diagonal_inches() const noexcept {
        return physical_diagonal_mm() / 25.4;
    }

    [[nodiscard]] double dpi() const noexcept {
        if (physical_width_mm <= 0 || pixel_width <= 0) return 0.0;
        return (static_cast<double>(pixel_width) / static_cast<double>(physical_width_mm)) * 25.4;
    }
};

} // namespace ldde::display
