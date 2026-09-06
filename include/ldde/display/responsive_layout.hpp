#pragma once

#include <cstdint>
#include <string_view>
#include <optional>
#include "ldde/display/orientation.hpp"

namespace ldde::display {

enum class LayoutClass : uint8_t {
    Compact = 0,    // Typical narrow phone portrait
    Standard = 1,   // Phone landscape / large phone / foldable
    Expanded = 2    // Tablet / desktop / external display
};

[[nodiscard]] std::string_view layout_class_name(LayoutClass layout_class) noexcept;
[[nodiscard]] std::optional<LayoutClass> parse_layout_class(std::string_view str) noexcept;

enum class MultiWindowHint : uint8_t {
    SingleDominant = 0,           // Prioritize one dominant window
    MultipleWindows = 1,          // Support multiple simultaneous floating windows
    MultipleWindowsExpanded = 2   // Encourage multiple visible windows with tiling/freeform
};

[[nodiscard]] std::string_view multi_window_hint_name(MultiWindowHint hint) noexcept;

[[nodiscard]] LayoutClass classify_layout(
    int32_t logical_width,
    int32_t logical_height,
    Orientation orientation,
    int32_t physical_width_mm = 0,
    int32_t physical_height_mm = 0,
    std::optional<LayoutClass> override_class = std::nullopt) noexcept;

[[nodiscard]] constexpr MultiWindowHint derive_multi_window_hint(LayoutClass layout_class) noexcept {
    switch (layout_class) {
        case LayoutClass::Compact:
            return MultiWindowHint::SingleDominant;
        case LayoutClass::Standard:
            return MultiWindowHint::MultipleWindows;
        case LayoutClass::Expanded:
            return MultiWindowHint::MultipleWindowsExpanded;
    }
    return MultiWindowHint::SingleDominant;
}

} // namespace ldde::display

