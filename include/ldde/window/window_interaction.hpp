#pragma once

#include <cstdint>
#include <optional>
#include "ldde/core/types.hpp"
#include "ldde/window/types.hpp"

namespace ldde::window {

enum class InteractionType {
    None = 0,
    Moving,
    Resizing
};

struct InteractionConfig {
    int32_t pointer_edge_margin = 6;
    int32_t touch_edge_margin = 24;
    int32_t min_visible_titlebar = 36;
};

class WindowInteraction {
public:
    explicit WindowInteraction(InteractionConfig config = {});

    [[nodiscard]] ResizeEdge detect_resize_edge(
        const core::Rect& window_geom,
        const core::Point& point,
        bool is_touch = false) const noexcept;

    bool start_move(
        WindowId id,
        const core::Point& start_pos,
        const core::Rect& initial_geom,
        const core::Rect& usable_area,
        bool is_touch = false);

    [[nodiscard]] core::Rect update_move(const core::Point& current_pos) noexcept;
    [[nodiscard]] core::Rect end_move() noexcept;
    [[nodiscard]] core::Rect cancel_move() noexcept;

    bool start_resize(
        WindowId id,
        ResizeEdge edge,
        const core::Point& start_pos,
        const core::Rect& initial_geom,
        const core::Rect& usable_area,
        const core::Size& min_size = {200, 150},
        const core::Size& max_size = {0, 0},
        bool is_touch = false);

    [[nodiscard]] core::Rect update_resize(const core::Point& current_pos) noexcept;
    [[nodiscard]] core::Rect end_resize() noexcept;
    [[nodiscard]] core::Rect cancel_resize() noexcept;

    [[nodiscard]] bool is_active() const noexcept { return type_ != InteractionType::None; }
    [[nodiscard]] InteractionType interaction_type() const noexcept { return type_; }
    [[nodiscard]] std::optional<WindowId> active_window_id() const noexcept { return window_id_; }
    [[nodiscard]] ResizeEdge active_resize_edge() const noexcept { return resize_edge_; }
    [[nodiscard]] const core::Rect& current_geometry() const noexcept { return current_geom_; }
    [[nodiscard]] const core::Rect& initial_geometry() const noexcept { return initial_geom_; }

    void reset() noexcept;

private:
    InteractionConfig config_;
    InteractionType type_ = InteractionType::None;
    std::optional<WindowId> window_id_;
    ResizeEdge resize_edge_ = ResizeEdge::None;

    core::Point start_point_{0, 0};
    core::Rect initial_geom_{0, 0, 0, 0};
    core::Rect current_geom_{0, 0, 0, 0};
    core::Rect usable_area_{0, 0, 0, 0};
    core::Size min_size_{200, 150};
    core::Size max_size_{0, 0};
    bool is_touch_ = false;

    [[nodiscard]] core::Rect clamp_moved_geometry(const core::Rect& geom) const noexcept;
    [[nodiscard]] core::Rect apply_resize_delta(int32_t dx, int32_t dy) const noexcept;
};

} // namespace ldde::window

