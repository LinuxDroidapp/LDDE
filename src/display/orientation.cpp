#include "ldde/display/orientation.hpp"

namespace ldde::display {

std::string_view display_transform_name(DisplayTransform transform) noexcept {
    switch (transform) {
        case DisplayTransform::Normal:     return "Normal";
        case DisplayTransform::Rotate90:   return "Rotate90";
        case DisplayTransform::Rotate180:  return "Rotate180";
        case DisplayTransform::Rotate270:  return "Rotate270";
        case DisplayTransform::Flipped:    return "Flipped";
        case DisplayTransform::Flipped90:  return "Flipped90";
        case DisplayTransform::Flipped180: return "Flipped180";
        case DisplayTransform::Flipped270: return "Flipped270";
    }
    return "Unknown";
}

std::string_view orientation_name(Orientation orientation) noexcept {
    switch (orientation) {
        case Orientation::Portrait:         return "PORTRAIT";
        case Orientation::Landscape:        return "LANDSCAPE";
        case Orientation::PortraitReverse:  return "PORTRAIT_REVERSE";
        case Orientation::LandscapeReverse: return "LANDSCAPE_REVERSE";
    }
    return "UNKNOWN";
}

Orientation derive_orientation(DisplayTransform transform, int32_t width, int32_t height) noexcept {
    bool aspect_portrait = (height > width);

    switch (transform) {
        case DisplayTransform::Rotate180:
        case DisplayTransform::Flipped180:
            return aspect_portrait ? Orientation::PortraitReverse : Orientation::LandscapeReverse;

        case DisplayTransform::Rotate270:
        case DisplayTransform::Flipped270:
            return aspect_portrait ? Orientation::PortraitReverse : Orientation::LandscapeReverse;

        case DisplayTransform::Rotate90:
        case DisplayTransform::Flipped90:
            return aspect_portrait ? Orientation::Portrait : Orientation::Landscape;

        case DisplayTransform::Normal:
        case DisplayTransform::Flipped:
        default:
            return aspect_portrait ? Orientation::Portrait : Orientation::Landscape;
    }
}

} // namespace ldde::display

