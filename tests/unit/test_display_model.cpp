#include <gtest/gtest.h>
#include "ldde/display/display_info.hpp"

using namespace ldde::display;
using namespace ldde::core;

TEST(DisplayModelTest, DisplayInfoCalculations) {
    DisplayInfo info;
    info.id = 1;
    info.name = "WL-1";
    info.width = 1080;
    info.height = 2400;
    info.physical_width_mm = 70;
    info.physical_height_mm = 155;
    info.refresh_rate_mhz = 120000;
    info.scale = 2;
    info.transform = DisplayTransform::Normal;
    info.geometry = Rect{.x = 0, .y = 0, .width = 1080, .height = 2400};
    info.safe_insets = Insets{.top = 48, .right = 0, .bottom = 32, .left = 0};

    EXPECT_DOUBLE_EQ(info.refresh_rate_hz(), 120.0);
    EXPECT_TRUE(info.is_portrait());
    EXPECT_FALSE(info.is_landscape());

    // Switch to landscape
    info.width = 2400;
    info.height = 1080;
    EXPECT_FALSE(info.is_portrait());
    EXPECT_TRUE(info.is_landscape());
}

TEST(DisplayModelTest, TransformNames) {
    EXPECT_EQ(display_transform_name(DisplayTransform::Normal), "Normal");
    EXPECT_EQ(display_transform_name(DisplayTransform::Rotate90), "Rotate90");
    EXPECT_EQ(display_transform_name(DisplayTransform::Rotate180), "Rotate180");
    EXPECT_EQ(display_transform_name(DisplayTransform::Rotate270), "Rotate270");
    EXPECT_EQ(display_transform_name(DisplayTransform::Flipped), "Flipped");
    EXPECT_EQ(display_transform_name(DisplayTransform::Flipped90), "Flipped90");
    EXPECT_EQ(display_transform_name(DisplayTransform::Flipped180), "Flipped180");
    EXPECT_EQ(display_transform_name(DisplayTransform::Flipped270), "Flipped270");
}

TEST(DisplayModelTest, GeometryPrimitives) {
    Rect rect{.x = 10, .y = 20, .width = 100, .height = 200};
    EXPECT_TRUE(rect.contains(Point{.x = 10, .y = 20}));
    EXPECT_TRUE(rect.contains(Point{.x = 50, .y = 100}));
    EXPECT_FALSE(rect.contains(Point{.x = 9, .y = 20}));
    EXPECT_FALSE(rect.contains(Point{.x = 110, .y = 20}));
    EXPECT_FALSE(rect.contains(Point{.x = 10, .y = 220}));

    Size sz{.width = 100, .height = 50};
    EXPECT_FALSE(sz.is_empty());

    Size empty_sz{.width = 0, .height = 50};
    EXPECT_TRUE(empty_sz.is_empty());
}

