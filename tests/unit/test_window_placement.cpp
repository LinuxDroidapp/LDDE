#include <gtest/gtest.h>
#include "ldde/window/window_placement.hpp"
#include "ldde/window/window.hpp"

using namespace ldde::core;
using namespace ldde::display;
using namespace ldde::window;

class WindowPlacementTest : public ::testing::Test {
protected:
    WindowPlacement placement_;
    DisplayInfo portrait_disp_;
    DisplayInfo landscape_disp_;

    void SetUp() override {
        portrait_disp_.width = 1080;
        portrait_disp_.height = 2400;

        landscape_disp_.width = 2400;
        landscape_disp_.height = 1080;

        PlacementConstraints constraints;
        constraints.status_bar_height = 40;
        constraints.dock_height = 68;
        constraints.margin_top = 8;
        constraints.margin_bottom = 8;
        constraints.margin_horizontal = 8;
        constraints.cascade_step = 32;
        placement_.set_constraints(constraints);
    }
};

TEST_F(WindowPlacementTest, UsableAreaCalculation) {
    Rect usable_portrait = placement_.get_usable_area(portrait_disp_);
    EXPECT_EQ(usable_portrait.x, 8);
    EXPECT_EQ(usable_portrait.y, 48); // 40 + 8
    EXPECT_EQ(usable_portrait.width, 1080 - 16); // 1064
    EXPECT_EQ(usable_portrait.height, 2400 - 48 - 68 - 8); // 2276

    Rect usable_landscape = placement_.get_usable_area(landscape_disp_);
    EXPECT_EQ(usable_landscape.x, 8);
    EXPECT_EQ(usable_landscape.y, 48);
    EXPECT_EQ(usable_landscape.width, 2400 - 16); // 2384
    EXPECT_EQ(usable_landscape.height, 1080 - 48 - 68 - 8); // 956
}

TEST_F(WindowPlacementTest, PortraitInitialPlacementAndCascade) {
    // First window in portrait
    Rect r1 = placement_.calculate_initial_geometry(portrait_disp_, 0);
    Rect usable = placement_.get_usable_area(portrait_disp_);

    // Width should be ~88% of usable width
    int32_t expected_w = (usable.width * 88) / 100;
    int32_t expected_h = (usable.height * 60) / 100;
    EXPECT_EQ(r1.width, expected_w);
    EXPECT_EQ(r1.height, expected_h);
    // Horizontally centered
    EXPECT_EQ(r1.x, usable.x + (usable.width - expected_w) / 2);
    EXPECT_EQ(r1.y, usable.y);

    // Second window cascades downwards (24px in portrait)
    Rect r2 = placement_.calculate_initial_geometry(portrait_disp_, 1);
    EXPECT_EQ(r2.width, expected_w);
    EXPECT_EQ(r2.height, expected_h);
    EXPECT_EQ(r2.x, r1.x);
    EXPECT_EQ(r2.y, usable.y + 24);

    // Third window cascades downwards
    Rect r3 = placement_.calculate_initial_geometry(portrait_disp_, 2);
    EXPECT_EQ(r3.y, usable.y + 48);
}

TEST_F(WindowPlacementTest, LandscapeInitialPlacementAndCascade) {
    // First window in landscape
    Rect r1 = placement_.calculate_initial_geometry(landscape_disp_, 0);
    Rect usable = placement_.get_usable_area(landscape_disp_);

    int32_t expected_w = (usable.width * 65) / 100;
    int32_t expected_h = (usable.height * 70) / 100;
    EXPECT_EQ(r1.width, expected_w);
    EXPECT_EQ(r1.height, expected_h);
    EXPECT_EQ(r1.x, usable.x);
    EXPECT_EQ(r1.y, usable.y);

    // Second window cascades diagonally
    Rect r2 = placement_.calculate_initial_geometry(landscape_disp_, 1);
    EXPECT_EQ(r2.width, expected_w);
    EXPECT_EQ(r2.height, expected_h);
    EXPECT_EQ(r2.x, usable.x + 32);
    EXPECT_EQ(r2.y, usable.y + 32);
}

TEST_F(WindowPlacementTest, ClientRequestedSize) {
    Size requested{600, 400};
    Rect r = placement_.calculate_initial_geometry(landscape_disp_, 0, requested);
    EXPECT_EQ(r.width, 600);
    EXPECT_EQ(r.height, 400);
}

TEST_F(WindowPlacementTest, ClampingKeepsHeaderAccessible) {
    Rect usable = placement_.get_usable_area(portrait_disp_);

    // Test window positioned way off to the top-left
    Rect off_top_left{-200, -100, 500, 400};
    Rect clamped1 = placement_.clamp_to_usable(off_top_left, usable);
    EXPECT_GE(clamped1.y, usable.y); // header must not be hidden under status bar
    EXPECT_GT(clamped1.x + clamped1.width, usable.x);

    // Test window positioned way off to the bottom-right
    Rect off_bottom_right{1200, 2500, 500, 400};
    Rect clamped2 = placement_.clamp_to_usable(off_bottom_right, usable);
    EXPECT_LE(clamped2.y, usable.y + usable.height - 36); // at least titlebar visible
    EXPECT_GE(clamped2.x, usable.x - clamped2.width + 60);

    // Test window larger than usable area is downscaled to fit
    Rect oversize{0, 0, 2000, 3000};
    Rect clamped3 = placement_.clamp_to_usable(oversize, usable);
    EXPECT_LE(clamped3.width, usable.width);
    EXPECT_LE(clamped3.height, usable.height);
}

