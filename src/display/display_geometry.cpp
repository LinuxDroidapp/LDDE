#include "ldde/display/display_geometry.hpp"

namespace ldde::display {

void apply_transform_to_dimensions(DisplayTransform transform,
                                   int32_t in_width, int32_t in_height,
                                   int32_t& out_width, int32_t& out_height) noexcept {
    switch (transform) {
        case DisplayTransform::Rotate90:
        case DisplayTransform::Rotate270:
        case DisplayTransform::Flipped90:
        case DisplayTransform::Flipped270:
            out_width = in_height;
            out_height = in_width;
            break;
        case DisplayTransform::Normal:
        case DisplayTransform::Rotate180:
        case DisplayTransform::Flipped:
        case DisplayTransform::Flipped180:
        default:
            out_width = in_width;
            out_height = in_height;
            break;
    }
}

} // namespace ldde::display

