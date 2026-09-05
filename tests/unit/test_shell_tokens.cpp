#include <gtest/gtest.h>
#include "ldde/shell/design_tokens.hpp"
#include "ldde/shell/theme.hpp"

using namespace ldde::shell;

TEST(ShellTokensTest, BaselineValues) {
    DesignTokens t = DesignTokens::create_scaled(1.0);
    EXPECT_DOUBLE_EQ(t.scale, 1.0);
    EXPECT_EQ(t.status_height_px, 40);
    EXPECT_EQ(t.dock_height_px, 68);
    EXPECT_EQ(t.dock_margin_bottom_px, 16);
    EXPECT_EQ(t.dock_corner_radius_px, 24);
    EXPECT_EQ(t.min_touch_target_px, 48);
}

TEST(ShellTokensTest, ScaleFactorScaling) {
    DesignTokens t15 = DesignTokens::create_scaled(1.5);
    EXPECT_DOUBLE_EQ(t15.scale, 1.5);
    EXPECT_EQ(t15.status_height_px, 60); // 40 * 1.5
    EXPECT_EQ(t15.dock_height_px, 102);  // 68 * 1.5

    DesignTokens t20 = DesignTokens::create_scaled(2.0);
    EXPECT_DOUBLE_EQ(t20.scale, 2.0);
    EXPECT_EQ(t20.status_height_px, 80);
    EXPECT_EQ(t20.dock_height_px, 136);
    EXPECT_EQ(t20.dock_corner_radius_px, 48);
}

TEST(ShellThemeTest, ColorHexParsing) {
    // 6-character hex #RRGGBB
    auto c1 = Color::from_hex("#FF8000");
    ASSERT_TRUE(c1.has_value());
    EXPECT_NEAR(c1->r, 1.0, 0.01);
    EXPECT_NEAR(c1->g, 0.5, 0.01);
    EXPECT_NEAR(c1->b, 0.0, 0.01);
    EXPECT_DOUBLE_EQ(c1->a, 1.0);

    // 8-character hex #RRGGBBAA
    auto c2 = Color::from_hex("#00FF0080");
    ASSERT_TRUE(c2.has_value());
    EXPECT_DOUBLE_EQ(c2->r, 0.0);
    EXPECT_DOUBLE_EQ(c2->g, 1.0);
    EXPECT_DOUBLE_EQ(c2->b, 0.0);
    EXPECT_NEAR(c2->a, 0.5, 0.01);

    // Invalid strings
    EXPECT_FALSE(Color::from_hex("").has_value());
    EXPECT_FALSE(Color::from_hex("invalid").has_value());
    EXPECT_FALSE(Color::from_hex("#XYZ").has_value());
    EXPECT_FALSE(Color::from_hex("#12345").has_value());
}

TEST(ShellThemeTest, DefaultDarkTheme) {
    ShellTheme dark = ShellTheme::default_dark();
    EXPECT_GT(dark.desktop_bg_top.a, 0.0);
    EXPECT_GT(dark.dock_bg.a, 0.0);
    EXPECT_GT(dark.status_text.a, 0.0);
    EXPECT_GT(dark.overlay_scrim.a, 0.0);
}
