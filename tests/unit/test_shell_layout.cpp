#include <gtest/gtest.h>
#include "ldde/shell/shell_layout.hpp"

using namespace ldde::core;
using namespace ldde::display;
using namespace ldde::shell;

TEST(ShellLayoutTest, PortraitLayoutCalculations) {
    ShellLayout layout;
    DisplayInfo info;
    info.id = 1;
    info.name = "MobileScreen";
    info.width = 1080;
    info.height = 2400;
    info.scale = 1;

    DesignTokens tokens = DesignTokens::create_scaled(1.0);

    layout.update(info, tokens, DockPosition::Bottom);

    EXPECT_TRUE(layout.is_portrait());
    EXPECT_FALSE(layout.is_landscape());

    // Screen bounds
    EXPECT_EQ(layout.screen_bounds().x, 0);
    EXPECT_EQ(layout.screen_bounds().y, 0);
    EXPECT_EQ(layout.screen_bounds().width, 1080);
    EXPECT_EQ(layout.screen_bounds().height, 2400);

    // Desktop geometry covers entire screen
    EXPECT_EQ(layout.desktop_geometry().x, 0);
    EXPECT_EQ(layout.desktop_geometry().y, 0);
    EXPECT_EQ(layout.desktop_geometry().width, 1080);
    EXPECT_EQ(layout.desktop_geometry().height, 2400);

    // Status bar at top
    EXPECT_EQ(layout.status_geometry().x, 0);
    EXPECT_EQ(layout.status_geometry().y, 0);
    EXPECT_EQ(layout.status_geometry().width, 1080);
    EXPECT_EQ(layout.status_geometry().height, tokens.status_height_px);

    // Dock at bottom: centered with portrait ratio (90%)
    EXPECT_EQ(layout.dock_geometry().height, tokens.dock_height_px);
    int expected_dock_w = static_cast<int>(1080 * DesignTokens::kDockWidthPortraitRatio);
    EXPECT_EQ(layout.dock_geometry().width, expected_dock_w);
    EXPECT_EQ(layout.dock_geometry().x, (1080 - expected_dock_w) / 2);
    EXPECT_EQ(layout.dock_geometry().y, 2400 - tokens.dock_height_px - tokens.dock_margin_bottom_px);
}

TEST(ShellLayoutTest, LandscapeLayoutCalculations) {
    ShellLayout layout;
    DisplayInfo info;
    info.id = 1;
    info.name = "LandscapeScreen";
    info.width = 1920;
    info.height = 1080;
    info.scale = 1;

    DesignTokens tokens = DesignTokens::create_scaled(1.0);

    layout.update(info, tokens, DockPosition::Bottom);

    EXPECT_FALSE(layout.is_portrait());
    EXPECT_TRUE(layout.is_landscape());

    // Screen bounds
    EXPECT_EQ(layout.screen_bounds().width, 1920);
    EXPECT_EQ(layout.screen_bounds().height, 1080);

    // Status bar spans top edge
    EXPECT_EQ(layout.status_geometry().width, 1920);
    EXPECT_EQ(layout.status_geometry().height, tokens.status_height_px);

    // Dock at bottom: centered with landscape ratio (60%)
    int expected_dock_w = static_cast<int>(1920 * DesignTokens::kDockWidthLandscapeRatio);
    EXPECT_EQ(layout.dock_geometry().width, expected_dock_w);
    EXPECT_EQ(layout.dock_geometry().x, (1920 - expected_dock_w) / 2);
    EXPECT_EQ(layout.dock_geometry().y, 1080 - tokens.dock_height_px - tokens.dock_margin_bottom_px);
}

TEST(ShellLayoutTest, DockPositioningOptions) {
    ShellLayout layout;
    DisplayInfo info;
    info.width = 1920;
    info.height = 1080;
    DesignTokens tokens = DesignTokens::create_scaled(1.0);

    // Left dock
    layout.update(info, tokens, DockPosition::Left);
    EXPECT_EQ(layout.dock_geometry().x, tokens.dock_margin_bottom_px);
    EXPECT_GT(layout.dock_geometry().y, 0);

    // Right dock
    layout.update(info, tokens, DockPosition::Right);
    EXPECT_EQ(layout.dock_geometry().x, 1920 - layout.dock_geometry().width - tokens.dock_margin_bottom_px);
}

TEST(ShellLayoutTest, HitTestingHierarchy) {
    ShellLayout layout;
    DisplayInfo info;
    info.width = 1000;
    info.height = 2000;
    DesignTokens tokens = DesignTokens::create_scaled(1.0);

    layout.update(info, tokens, DockPosition::Bottom);

    // Point in status bar (x=100, y=20)
    EXPECT_EQ(layout.hit_test(Point{100, 20}, false), ShellRegionType::Status);

    // Point in dock
    const auto& dock = layout.dock_geometry();
    EXPECT_EQ(layout.hit_test(Point{dock.x + 50, dock.y + 20}, false), ShellRegionType::Dock);

    // Point in central background
    EXPECT_EQ(layout.hit_test(Point{500, 1000}, false), ShellRegionType::Desktop);

    // Point outside screen
    EXPECT_EQ(layout.hit_test(Point{-10, -10}, false), ShellRegionType::None);
    EXPECT_EQ(layout.hit_test(Point{1500, 2500}, false), ShellRegionType::None);

    // When overlay is active, modal intercepts
    EXPECT_EQ(layout.hit_test(Point{500, 1000}, true), ShellRegionType::Overlay);
}
