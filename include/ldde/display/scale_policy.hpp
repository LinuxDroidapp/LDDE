#pragma once

#include <cstdint>
#include "ldde/core/types.hpp"
#include "ldde/display/display_geometry.hpp"

namespace ldde::display {

class ScalePolicy {
public:
    constexpr ScalePolicy() = default;
    explicit constexpr ScalePolicy(int32_t scale_factor, double fractional_scale = 0.0) noexcept
        : scale_factor_((scale_factor > 0) ? scale_factor : 1),
          fractional_scale_(fractional_scale > 0.0 ? fractional_scale : static_cast<double>((scale_factor > 0) ? scale_factor : 1)) {}

    [[nodiscard]] constexpr int32_t scale_factor() const noexcept { return scale_factor_; }
    [[nodiscard]] constexpr double effective_scale() const noexcept { return fractional_scale_; }

    [[nodiscard]] constexpr int32_t to_logical(int32_t physical) const noexcept {
        return physical_to_logical(physical, scale_factor_);
    }

    [[nodiscard]] constexpr int32_t to_physical(int32_t logical) const noexcept {
        return logical_to_physical(logical, scale_factor_);
    }

    [[nodiscard]] constexpr core::Point to_logical(core::Point pt) const noexcept {
        return physical_to_logical(pt, scale_factor_);
    }

    [[nodiscard]] constexpr core::Point to_physical(core::Point pt) const noexcept {
        return logical_to_physical(pt, scale_factor_);
    }

    [[nodiscard]] constexpr core::Size to_logical(core::Size sz) const noexcept {
        return physical_to_logical(sz, scale_factor_);
    }

    [[nodiscard]] constexpr core::Size to_physical(core::Size sz) const noexcept {
        return logical_to_physical(sz, scale_factor_);
    }

    [[nodiscard]] constexpr core::Rect to_logical(core::Rect r) const noexcept {
        return physical_to_logical(r, scale_factor_);
    }

    [[nodiscard]] constexpr core::Rect to_physical(core::Rect r) const noexcept {
        return logical_to_physical(r, scale_factor_);
    }

    [[nodiscard]] constexpr bool operator==(const ScalePolicy& other) const = default;

private:
    int32_t scale_factor_ = 1;
    double fractional_scale_ = 1.0;
};

} // namespace ldde::display

