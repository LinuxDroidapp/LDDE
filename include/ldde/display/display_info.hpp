#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "ldde/core/types.hpp"

namespace ldde::display {

enum class DisplayTransform : int32_t {
    Normal = 0,
    Rotate90 = 1,
    Rotate180 = 2,
    Rotate270 = 3,
    Flipped = 4,
    Flipped90 = 5,
    Flipped180 = 6,
    Flipped270 = 7
};

[[nodiscard]] std::string_view display_transform_name(DisplayTransform transform) noexcept;

struct DisplayMode {
    int32_t width = 0;
    int32_t height = 0;
    int32_t refresh_rate_mhz = 0; // Millihertz (e.g. 60000 = 60Hz)
    bool is_current = false;
    bool is_preferred = false;
};

struct DisplayInfo {
    uint32_t id = 0;
    std::string name;
    std::string make;
    std::string model;
    std::string description;

    // Pixel dimensions
    int32_t width = 0;
    int32_t height = 0;

    // Physical dimensions in millimeters
    int32_t physical_width_mm = 0;
    int32_t physical_height_mm = 0;

    // Refresh rate of active mode (mHz)
    int32_t refresh_rate_mhz = 0;

    // Scale factor (Wayland wl_output scale is integer, wl_output_fractional is double)
    int32_t scale = 1;

    // Orientation / Transform
    DisplayTransform transform = DisplayTransform::Normal;

    // Available desktop geometry (within compositor space)
    core::Rect geometry;

    // Safe insets (e.g. cutouts, rounded corners, status/nav margins)
    core::Insets safe_insets;

    // Supported display modes
    std::vector<DisplayMode> modes;

    [[nodiscard]] double refresh_rate_hz() const noexcept {
        return static_cast<double>(refresh_rate_mhz) / 1000.0;
    }

    [[nodiscard]] bool is_portrait() const noexcept {
        return height > width;
    }

    [[nodiscard]] bool is_landscape() const noexcept {
        return width >= height;
    }
};

} // namespace ldde::display
