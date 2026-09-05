#include "ldde/display/display_info.hpp"

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

} // namespace ldde::display

