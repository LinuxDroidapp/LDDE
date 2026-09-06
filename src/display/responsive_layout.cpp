#include "ldde/display/responsive_layout.hpp"
#include <cmath>

namespace ldde::display {

std::string_view layout_class_name(LayoutClass layout_class) noexcept {
    switch (layout_class) {
        case LayoutClass::Compact:  return "COMPACT";
        case LayoutClass::Standard: return "STANDARD";
        case LayoutClass::Expanded: return "EXPANDED";
    }
    return "UNKNOWN";
}

std::optional<LayoutClass> parse_layout_class(std::string_view str) noexcept {
    if (str == "compact" || str == "COMPACT") return LayoutClass::Compact;
    if (str == "standard" || str == "STANDARD") return LayoutClass::Standard;
    if (str == "expanded" || str == "EXPANDED") return LayoutClass::Expanded;
    return std::nullopt;
}

std::string_view multi_window_hint_name(MultiWindowHint hint) noexcept {
    switch (hint) {
        case MultiWindowHint::SingleDominant:          return "SINGLE_DOMINANT";
        case MultiWindowHint::MultipleWindows:         return "MULTIPLE_WINDOWS";
        case MultiWindowHint::MultipleWindowsExpanded: return "MULTIPLE_WINDOWS_EXPANDED";
    }
    return "UNKNOWN";
}

LayoutClass classify_layout(
    int32_t logical_width,
    int32_t logical_height,
    Orientation orientation,
    int32_t physical_width_mm,
    int32_t physical_height_mm,
    std::optional<LayoutClass> override_class) noexcept {

    if (override_class.has_value()) {
        return override_class.value();
    }

    // Physical diagonal check if physical dimensions are known (mm)
    if (physical_width_mm > 0 && physical_height_mm > 0) {
        double diag_mm = std::hypot(static_cast<double>(physical_width_mm),
                                    static_cast<double>(physical_height_mm));
        // >= 250mm is roughly >= 10-inch screen (tablet / external monitor)
        if (diag_mm >= 250.0 && logical_width >= 900) {
            return LayoutClass::Expanded;
        }
    }

    // Logical dimensions check
    if (logical_width >= 1200) {
        return LayoutClass::Expanded;
    }

    if (is_portrait(orientation)) {
        // In portrait mode:
        // Narrow phone portrait: width < 600
        if (logical_width < 600) {
            return LayoutClass::Compact;
        }
        // Tablet / foldable in portrait (600 <= width < 1000)
        if (logical_width < 1000) {
            return LayoutClass::Standard;
        }
        return LayoutClass::Expanded;
    } else {
        // In landscape mode:
        // Small landscape phone: width < 720 and height < 480
        if (logical_width < 720 && logical_height < 480) {
            return LayoutClass::Compact;
        }
        // Phone landscape / medium screen (width < 1200)
        if (logical_width < 1200) {
            return LayoutClass::Standard;
        }
        return LayoutClass::Expanded;
    }
}

} // namespace ldde::display

