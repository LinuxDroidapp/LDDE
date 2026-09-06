#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ldde::core {

inline constexpr std::string_view kVersion = "1.0.0";
inline constexpr int kConfigVersion = 1;
inline constexpr std::string_view kDefaultDesktopName = "LDDE";

struct Point {
    int32_t x = 0;
    int32_t y = 0;

    constexpr bool operator==(const Point& other) const = default;
};

struct Size {
    int32_t width = 0;
    int32_t height = 0;

    constexpr bool operator==(const Size& other) const = default;
    constexpr bool is_empty() const noexcept { return width <= 0 || height <= 0; }
};

struct Rect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;

    constexpr bool operator==(const Rect& other) const = default;
    constexpr bool contains(const Point& pt) const noexcept {
        return pt.x >= x && pt.x < (x + width) && pt.y >= y && pt.y < (y + height);
    }
};

struct Insets {
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
    int32_t left = 0;

    constexpr bool operator==(const Insets& other) const = default;
};

} // namespace ldde::core

