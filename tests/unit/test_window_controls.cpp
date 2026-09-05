#include <gtest/gtest.h>
#include "ldde/window/window_controls.hpp"

using namespace ldde::core;
using namespace ldde::window;

class WindowControlsTest : public ::testing::Test {
protected:
    WindowControls controls_;
    Rect win_geom_{100, 100, 800, 500};
};

TEST_F(WindowControlsTest, GeometryHeaderAndButtonsPointer) {
    int32_t hh = controls_.header_height(false);
    EXPECT_EQ(hh, 36);

    Rect header = controls_.get_header_rect(win_geom_, false);
    EXPECT_EQ(header.x, 100);
    EXPECT_EQ(header.y, 100);
    EXPECT_EQ(header.width, 800);
    EXPECT_EQ(header.height, 36);

    Rect close_btn = controls_.get_close_button_rect(win_geom_, false);
    EXPECT_EQ(close_btn.x, 100 + 800 - 36);
    EXPECT_EQ(close_btn.y, 100);
    EXPECT_EQ(close_btn.width, 36);
    EXPECT_EQ(close_btn.height, 36);

    Rect max_btn = controls_.get_maximize_button_rect(win_geom_, false);
    EXPECT_EQ(max_btn.x, close_btn.x - 36);
    EXPECT_EQ(max_btn.y, 100);

    Rect min_btn = controls_.get_minimize_button_rect(win_geom_, false);
    EXPECT_EQ(min_btn.x, max_btn.x - 36);
    EXPECT_EQ(min_btn.y, 100);
}

TEST_F(WindowControlsTest, GeometryHeaderAndButtonsTouchMobile) {
    // Touch targets must be at least 48dp
    int32_t hh = controls_.header_height(true);
    EXPECT_GE(hh, 48);

    Rect close_btn = controls_.get_close_button_rect(win_geom_, true);
    EXPECT_GE(close_btn.width, 48);
    EXPECT_GE(close_btn.height, 48);
}

TEST_F(WindowControlsTest, HitTestButtons) {
    // Click Close button
    Point close_pt{100 + 800 - 10, 100 + 10};
    HeaderHitResult res = controls_.hit_test(win_geom_, close_pt, 1000, false);
    EXPECT_EQ(res.button, WindowControlButton::Close);
    EXPECT_FALSE(res.is_double_tap);

    // Click Maximize button
    Rect max_rect = controls_.get_maximize_button_rect(win_geom_, false);
    Point max_pt{max_rect.x + 5, max_rect.y + 5};
    res = controls_.hit_test(win_geom_, max_pt, 1000, false);
    EXPECT_EQ(res.button, WindowControlButton::MaximizeRestore);

    // Click Minimize button
    Rect min_rect = controls_.get_minimize_button_rect(win_geom_, false);
    Point min_pt{min_rect.x + 5, min_rect.y + 5};
    res = controls_.hit_test(win_geom_, min_pt, 1000, false);
    EXPECT_EQ(res.button, WindowControlButton::Minimize);

    // Click Titlebar Drag Area
    Point drag_pt{100 + 50, 100 + 15};
    res = controls_.hit_test(win_geom_, drag_pt, 1000, false);
    EXPECT_EQ(res.button, WindowControlButton::TitleDragArea);
    EXPECT_FALSE(res.is_double_tap);

    // Click outside header (content area)
    Point content_pt{100 + 50, 100 + 100};
    res = controls_.hit_test(win_geom_, content_pt, 1000, false);
    EXPECT_EQ(res.button, WindowControlButton::None);
}

TEST_F(WindowControlsTest, DoubleTapMaximizeDetection) {
    Point drag_pt{100 + 50, 100 + 15};

    // First tap at t=1000ms
    HeaderHitResult r1 = controls_.hit_test(win_geom_, drag_pt, 1000, false);
    EXPECT_EQ(r1.button, WindowControlButton::TitleDragArea);
    EXPECT_FALSE(r1.is_double_tap);

    // Second tap at t=1200ms (< 350ms)
    HeaderHitResult r2 = controls_.hit_test(win_geom_, drag_pt, 1200, false);
    EXPECT_EQ(r2.button, WindowControlButton::TitleDragArea);
    EXPECT_TRUE(r2.is_double_tap);

    // Third tap at t=2000ms (> 350ms) - not double tap
    HeaderHitResult r3 = controls_.hit_test(win_geom_, drag_pt, 2000, false);
    EXPECT_EQ(r3.button, WindowControlButton::TitleDragArea);
    EXPECT_FALSE(r3.is_double_tap);
}

