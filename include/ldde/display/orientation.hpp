#pragma once

#include <cstdint>
#include <string_view>

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

enum class Orientation : uint8_t {
    Portrait = 0,
    Landscape = 1,
    PortraitReverse = 2,
    LandscapeReverse = 3
};

[[nodiscard]] std::string_view orientation_name(Orientation orientation) noexcept;

[[nodiscard]] constexpr bool is_portrait(Orientation orientation) noexcept {
    return orientation == Orientation::Portrait || orientation == Orientation::PortraitReverse;
}

[[nodiscard]] constexpr bool is_landscape(Orientation orientation) noexcept {
    return orientation == Orientation::Landscape || orientation == Orientation::LandscapeReverse;
}

[[nodiscard]] Orientation derive_orientation(DisplayTransform transform, int32_t width, int32_t height) noexcept;

} // namespace ldde::display

