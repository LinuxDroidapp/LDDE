#pragma once

#include <cstdint>
#include <string_view>

namespace ldde::display {

enum class DisplayEventType : uint8_t {
    DisplayAdded,
    DisplayRemoved,
    DisplayChanged,
    OrientationChanged,
    ScaleChanged,
    GeometryChanged,
    SafeAreaChanged,
    LayoutClassChanged
};

[[nodiscard]] constexpr std::string_view display_event_type_name(DisplayEventType type) noexcept {
    switch (type) {
        case DisplayEventType::DisplayAdded:        return "DisplayAdded";
        case DisplayEventType::DisplayRemoved:      return "DisplayRemoved";
        case DisplayEventType::DisplayChanged:      return "DisplayChanged";
        case DisplayEventType::OrientationChanged:  return "OrientationChanged";
        case DisplayEventType::ScaleChanged:        return "ScaleChanged";
        case DisplayEventType::GeometryChanged:     return "GeometryChanged";
        case DisplayEventType::SafeAreaChanged:     return "SafeAreaChanged";
        case DisplayEventType::LayoutClassChanged:  return "LayoutClassChanged";
    }
    return "Unknown";
}

struct DisplayEvent {
    DisplayEventType type;
    uint32_t display_id = 0;
};

} // namespace ldde::display

