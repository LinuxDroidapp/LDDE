#pragma once

#include <cstdint>
#include "ldde/core/types.hpp"
#include "ldde/display/orientation.hpp"

namespace ldde::display {

struct SafeInsets {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;

    constexpr SafeInsets& operator=(const core::Insets& insets) noexcept {
        left = insets.left;
        top = insets.top;
        right = insets.right;
        bottom = insets.bottom;
        return *this;
    }

    constexpr bool operator==(const SafeInsets& other) const = default;

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return left == 0 && top == 0 && right == 0 && bottom == 0;
    }

    [[nodiscard]] constexpr core::Insets to_core_insets() const noexcept {
        return core::Insets{
            .top = top,
            .right = right,
            .bottom = bottom,
            .left = left
        };
    }

    [[nodiscard]] static constexpr SafeInsets from_core_insets(const core::Insets& insets) noexcept {
        return SafeInsets{
            .left = insets.left,
            .top = insets.top,
            .right = insets.right,
            .bottom = insets.bottom
        };
    }

    [[nodiscard]] SafeInsets rotated_for(DisplayTransform transform) const noexcept {
        switch (transform) {
            case DisplayTransform::Rotate90:
                // 90 deg clockwise: top->right, right->bottom, bottom->left, left->top
                return SafeInsets{.left = bottom, .top = left, .right = top, .bottom = right};
            case DisplayTransform::Rotate180:
                // 180 deg: invert both axes
                return SafeInsets{.left = right, .top = bottom, .right = left, .bottom = top};
            case DisplayTransform::Rotate270:
                // 270 deg clockwise: top->left, left->bottom, bottom->right, right->top
                return SafeInsets{.left = top, .top = right, .right = bottom, .bottom = left};
            case DisplayTransform::Normal:
            default:
                return *this;
        }
    }
};

} // namespace ldde::display

