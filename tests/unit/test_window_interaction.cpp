#include <gtest/gtest.h>
#include "ldde/window/window_interaction.hpp"

using namespace ldde::core;
using namespace ldde::window;

class WindowInteractionTest : public ::testing::Test {
protected:
    WindowInteraction interaction_;
    Rect initial_geom_{100, 100, 600, 400};
    Rect usable_area_{0, 40, 1920, 1000};
};

TEST_F(WindowInteractionTest, DetectResizeEdgesPointer) {
    // Top edge
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{400, 102}, false), ResizeEdge::Top);
    // Bottom edge
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{400, 498}, false), ResizeEdge::Bottom);
    // Left edge
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{102, 300}, false), ResizeEdge::Left);
    // Right edge
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{698, 300}, false), ResizeEdge::Right);
    // Top-Left corner
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{102, 102}, false), ResizeEdge::TopLeft);
    // Bottom-Right corner
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{698, 498}, false), ResizeEdge::BottomRight);
    // Center (no edge)
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{400, 300}, false), ResizeEdge::None);
}

TEST_F(WindowInteractionTest, DetectResizeEdgesTouch) {
    // Touch has larger hit margin (24px vs 6px pointer)
    // Point at distance 15px from right edge (x = 700 - 15 = 685)
    // For pointer (margin 6px): should be None
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{685, 300}, false), ResizeEdge::None);
    // For touch (margin 24px): should be Right
    EXPECT_EQ(interaction_.detect_resize_edge(initial_geom_, Point{685, 300}, true), ResizeEdge::Right);
}

TEST_F(WindowInteractionTest, InteractiveMoveAndClamp) {
    Point start_pos{200, 120};
    bool started = interaction_.start_move(1, start_pos, initial_geom_, usable_area_, false);
    EXPECT_TRUE(started);
    EXPECT_TRUE(interaction_.is_active());
    EXPECT_EQ(interaction_.interaction_type(), InteractionType::Moving);
    EXPECT_EQ(interaction_.active_window_id(), 1);

    // Drag +50px right, +30px down
    Point cur{250, 150};
    Rect updated = interaction_.update_move(cur);
    EXPECT_EQ(updated.x, 150);
    EXPECT_EQ(updated.y, 130);
    EXPECT_EQ(updated.width, 600);
    EXPECT_EQ(updated.height, 400);

    // End move
    Rect final_geom = interaction_.end_move();
    EXPECT_EQ(final_geom, updated);
    EXPECT_FALSE(interaction_.is_active());
}

TEST_F(WindowInteractionTest, InteractiveMoveCancel) {
    Point start_pos{200, 120};
    interaction_.start_move(1, start_pos, initial_geom_, usable_area_, false);

    static_cast<void>(interaction_.update_move(Point{500, 500}));
    EXPECT_NE(interaction_.current_geometry(), initial_geom_);

    Rect reverted = interaction_.cancel_move();
    EXPECT_EQ(reverted, initial_geom_);
    EXPECT_FALSE(interaction_.is_active());
}

TEST_F(WindowInteractionTest, InteractiveResizeBottomRight) {
    Point start_pos{700, 500};
    bool started = interaction_.start_resize(1, ResizeEdge::BottomRight, start_pos, initial_geom_, usable_area_);
    EXPECT_TRUE(started);
    EXPECT_EQ(interaction_.interaction_type(), InteractionType::Resizing);

    // Drag +100px right, +50px down
    Rect updated = interaction_.update_resize(Point{800, 550});
    EXPECT_EQ(updated.x, 100);
    EXPECT_EQ(updated.y, 100);
    EXPECT_EQ(updated.width, 700);
    EXPECT_EQ(updated.height, 450);

    Rect final_geom = interaction_.end_resize();
    EXPECT_EQ(final_geom, updated);
}

TEST_F(WindowInteractionTest, InteractiveResizeRespectsMinSize) {
    Point start_pos{700, 500};
    Size min_sz{300, 200};
    interaction_.start_resize(1, ResizeEdge::BottomRight, start_pos, initial_geom_, usable_area_, min_sz);

    // Try to shrink window to negative or tiny size
    Rect updated = interaction_.update_resize(Point{150, 150});
    EXPECT_GE(updated.width, min_sz.width);
    EXPECT_GE(updated.height, min_sz.height);
}

TEST_F(WindowInteractionTest, InteractiveResizeTopLeft) {
    Point start_pos{100, 100};
    interaction_.start_resize(1, ResizeEdge::TopLeft, start_pos, initial_geom_, usable_area_);

    // Drag TopLeft +20px right, +30px down (shrinking window from top-left)
    Rect updated = interaction_.update_resize(Point{120, 130});
    EXPECT_EQ(updated.x, 120);
    EXPECT_EQ(updated.y, 130);
    EXPECT_EQ(updated.width, 580);
    EXPECT_EQ(updated.height, 370);
}
