#include <gtest/gtest.h>
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/display/orientation.hpp"
#include "ldde/display/scale_policy.hpp"
#include "ldde/display/safe_area.hpp"
#include "ldde/display/responsive_layout.hpp"
#include "ldde/display/layout_metrics.hpp"
#include "ldde/display/display_geometry.hpp"

using namespace ldde::core;
using namespace ldde::display;

// 1. Orientation Tests
TEST(DisplayPolicyTest, OrientationDerivationAndPredicates) {
    // Portrait mode (height > width)
    EXPECT_EQ(derive_orientation(DisplayTransform::Normal, 1080, 2400), Orientation::Portrait);
    EXPECT_TRUE(is_portrait(Orientation::Portrait));
    EXPECT_FALSE(is_landscape(Orientation::Portrait));
    EXPECT_EQ(orientation_name(Orientation::Portrait), "PORTRAIT");

    // Landscape mode (width >= height)
    EXPECT_EQ(derive_orientation(DisplayTransform::Normal, 2400, 1080), Orientation::Landscape);
    EXPECT_TRUE(is_landscape(Orientation::Landscape));
    EXPECT_FALSE(is_portrait(Orientation::Landscape));
    EXPECT_EQ(orientation_name(Orientation::Landscape), "LANDSCAPE");

    // Inverted 180 transforms
    EXPECT_EQ(derive_orientation(DisplayTransform::Rotate180, 1080, 2400), Orientation::PortraitReverse);
    EXPECT_TRUE(is_portrait(Orientation::PortraitReverse));
    EXPECT_EQ(orientation_name(Orientation::PortraitReverse), "PORTRAIT_REVERSE");

    EXPECT_EQ(derive_orientation(DisplayTransform::Rotate180, 2400, 1080), Orientation::LandscapeReverse);
    EXPECT_TRUE(is_landscape(Orientation::LandscapeReverse));
    EXPECT_EQ(orientation_name(Orientation::LandscapeReverse), "LANDSCAPE_REVERSE");

    // 90 and 270 degree rotation
    EXPECT_EQ(derive_orientation(DisplayTransform::Rotate90, 2400, 1080), Orientation::Landscape);
    EXPECT_EQ(derive_orientation(DisplayTransform::Rotate270, 2400, 1080), Orientation::LandscapeReverse);
}

// 2. Scale and Coordinate System Tests
TEST(DisplayPolicyTest, ScalePolicyAndCoordinateConversions) {
    ScalePolicy scale1(1);
    EXPECT_EQ(scale1.scale_factor(), 1);
    EXPECT_DOUBLE_EQ(scale1.effective_scale(), 1.0);

    ScalePolicy scale2(2);
    EXPECT_EQ(scale2.scale_factor(), 2);
    EXPECT_DOUBLE_EQ(scale2.effective_scale(), 2.0);

    // Coordinate conversions: physical <-> logical
    // Point
    Point phys_pt{.x = 1080, .y = 2400};
    Point log_pt = scale2.to_logical(phys_pt);
    EXPECT_EQ(log_pt.x, 540);
    EXPECT_EQ(log_pt.y, 1200);
    EXPECT_EQ(scale2.to_physical(log_pt), phys_pt);

    // Size
    Size phys_sz{.width = 1080, .height = 2400};
    Size log_sz = scale2.to_logical(phys_sz);
    EXPECT_EQ(log_sz.width, 540);
    EXPECT_EQ(log_sz.height, 1200);
    EXPECT_EQ(scale2.to_physical(log_sz), phys_sz);

    // Rect
    Rect phys_r{.x = 100, .y = 200, .width = 800, .height = 1200};
    Rect log_r = scale2.to_logical(phys_r);
    EXPECT_EQ(log_r.x, 50);
    EXPECT_EQ(log_r.y, 100);
    EXPECT_EQ(log_r.width, 400);
    EXPECT_EQ(log_r.height, 600);
    EXPECT_EQ(scale2.to_physical(log_r), phys_r);

    // Transform dimension swaps
    int32_t out_w = 0;
    int32_t out_h = 0;
    apply_transform_to_dimensions(DisplayTransform::Rotate90, 1080, 2400, out_w, out_h);
    EXPECT_EQ(out_w, 2400);
    EXPECT_EQ(out_h, 1080);
}

// 3. Safe Area and Cutout Insets
TEST(DisplayPolicyTest, SafeAreaRotation) {
    SafeInsets insets{.left = 0, .top = 48, .right = 0, .bottom = 32};
    EXPECT_FALSE(insets.is_empty());

    // Rotate 90 degrees clockwise (top becomes right, bottom becomes left)
    SafeInsets rot90 = insets.rotated_for(DisplayTransform::Rotate90);
    EXPECT_EQ(rot90.right, 48);
    EXPECT_EQ(rot90.left, 32);

    // Rotate 180 degrees (top becomes bottom, bottom becomes top)
    SafeInsets rot180 = insets.rotated_for(DisplayTransform::Rotate180);
    EXPECT_EQ(rot180.bottom, 48);
    EXPECT_EQ(rot180.top, 32);

    // Empty insets
    SafeInsets empty_insets;
    EXPECT_TRUE(empty_insets.is_empty());
}

// 4. Responsive Layout Classes and MultiWindow Hints
TEST(DisplayPolicyTest, LayoutClassification) {
    // Narrow phone portrait: logical width 390 < 600 -> Compact
    LayoutClass cls_portrait = classify_layout(390, 844, Orientation::Portrait);
    EXPECT_EQ(cls_portrait, LayoutClass::Compact);
    EXPECT_EQ(derive_multi_window_hint(cls_portrait), MultiWindowHint::SingleDominant);
    EXPECT_EQ(layout_class_name(cls_portrait), "COMPACT");

    // Phone landscape: logical width 844, height 390 -> Standard
    LayoutClass cls_landscape = classify_layout(844, 390, Orientation::Landscape);
    EXPECT_EQ(cls_landscape, LayoutClass::Standard);
    EXPECT_EQ(derive_multi_window_hint(cls_landscape), MultiWindowHint::MultipleWindows);
    EXPECT_EQ(layout_class_name(cls_landscape), "STANDARD");

    // Tablet / Desktop / External display: logical width 1440 >= 1200 -> Expanded
    LayoutClass cls_desktop = classify_layout(1440, 900, Orientation::Landscape);
    EXPECT_EQ(cls_desktop, LayoutClass::Expanded);
    EXPECT_EQ(derive_multi_window_hint(cls_desktop), MultiWindowHint::MultipleWindowsExpanded);
    EXPECT_EQ(layout_class_name(cls_desktop), "EXPANDED");

    // Manual override
    LayoutClass cls_overridden = classify_layout(390, 844, Orientation::Portrait, 0, 0, LayoutClass::Expanded);
    EXPECT_EQ(cls_overridden, LayoutClass::Expanded);

    // Parsing helper
    EXPECT_EQ(parse_layout_class("compact"), LayoutClass::Compact);
    EXPECT_EQ(parse_layout_class("standard"), LayoutClass::Standard);
    EXPECT_EQ(parse_layout_class("expanded"), LayoutClass::Expanded);
    EXPECT_FALSE(parse_layout_class("unknown").has_value());
}

// 5. LayoutMetrics and Touch Target Sizing
TEST(DisplayPolicyTest, CentralizedMetrics) {
    ScalePolicy scale(1);
    LayoutMetrics compact = LayoutMetrics::compute(LayoutClass::Compact, scale, Orientation::Portrait);
    EXPECT_GE(compact.minimum_touch_target_px, 48);
    EXPECT_GE(compact.window_control_target_px, 48);
    EXPECT_GE(compact.title_bar_height_px, 40);

    LayoutMetrics standard = LayoutMetrics::compute(LayoutClass::Standard, scale, Orientation::Landscape);
    EXPECT_GE(standard.minimum_touch_target_px, 48);
    EXPECT_GE(standard.window_control_target_px, 44);

    LayoutMetrics expanded = LayoutMetrics::compute(LayoutClass::Expanded, scale, Orientation::Landscape);
    EXPECT_GE(expanded.minimum_touch_target_px, 40);
}

// 6. Available Geometry and Shell Reservations
TEST(DisplayPolicyTest, AvailableGeometryCalculation) {
    DisplayInfo info;
    info.id = 1;
    info.pixel_width = 1080;
    info.pixel_height = 2400;
    info.logical_width = 540;
    info.logical_height = 1200;
    info.width = 540;
    info.height = 1200;
    info.scale = 2;
    info.transform = DisplayTransform::Normal;
    info.safe_insets = SafeInsets{.left = 0, .top = 36, .right = 0, .bottom = 24};

    DisplayPolicy policy(info);
    EXPECT_TRUE(policy.is_portrait());
    EXPECT_EQ(policy.layout_class(), LayoutClass::Compact);

    // Verify Full Bounds
    EXPECT_EQ(policy.available_geometry().full_bounds, (Rect{0, 0, 540, 1200}));

    // Verify Safe Bounds
    Rect expected_safe{0, 36, 540, 1200 - 36 - 24}; // (0, 36, 540, 1140)
    EXPECT_EQ(policy.available_geometry().safe_bounds, expected_safe);

    // Provide Shell Reservations
    ShellReservations reservations{
        .status_region = Rect{0, 36, 540, 40},
        .dock_region = Rect{20, 1100, 500, 68},
        .overlay_region = Rect{0, 0, 540, 1200}
    };
    policy.set_shell_reservations(reservations);

    // Window bounds should be safe area minus status and dock and margins
    Rect win_bounds = policy.available_window_geometry();
    EXPECT_GT(win_bounds.x, 0);
    EXPECT_GE(win_bounds.y, 36 + 40);
    EXPECT_LT(win_bounds.width, 540);
    EXPECT_LT(win_bounds.height, 1140);
}

// 7. Window Placement, Constraints, and Sizing
TEST(DisplayPolicyTest, WindowPlacementAndConstraints) {
    DisplayInfo info;
    info.id = 1;
    info.logical_width = 720;
    info.logical_height = 1280;
    info.width = 720;
    info.height = 1280;
    info.scale = 1;
    info.transform = DisplayTransform::Normal;

    DisplayPolicy policy(info);

    // Default size is proportional to usable space
    Size def_sz = policy.default_window_size();
    EXPECT_GT(def_sz.width, 0);
    EXPECT_GT(def_sz.height, 0);
    EXPECT_LE(def_sz.width, policy.available_window_geometry().width);
    EXPECT_LE(def_sz.height, policy.available_window_geometry().height);

    // Initial placement cascades
    Rect win0 = policy.calculate_initial_window_geometry(0);
    Rect win1 = policy.calculate_initial_window_geometry(1);
    EXPECT_EQ(win0.width, def_sz.width);
    EXPECT_EQ(win0.height, def_sz.height);
    // In portrait, second window cascades vertically
    EXPECT_GT(win1.y, win0.y);

    // Maximized geometry matches available window geometry
    EXPECT_EQ(policy.maximized_geometry(), policy.available_window_geometry());

    // Fullscreen geometry matches full bounds
    EXPECT_EQ(policy.fullscreen_geometry(), (Rect{0, 0, 720, 1280}));

    // Window constraint clamping: keeps titlebar accessible
    Rect offscreen_top{-50, -100, 300, 200};
    Rect constrained = policy.constrain_window_geometry(offscreen_top);
    EXPECT_GE(constrained.y, policy.available_window_geometry().y);
    EXPECT_GE(constrained.x, policy.available_window_geometry().x - constrained.width + 48);

    // Restore geometry with saved state
    Rect saved{100, 100, 400, 300};
    Rect restored = policy.restore_window_geometry(saved);
    EXPECT_EQ(restored.width, 400);
    EXPECT_EQ(restored.height, 300);
}

// 8. DisplayManager and Structured Events
TEST(DisplayPolicyTest, DisplayManagerEventsAndMultiOutput) {
    DisplayManager manager;

    std::vector<DisplayEventType> received_events;
    manager.on_event([&received_events](const DisplayEvent& ev, const DisplayInfo&) {
        received_events.push_back(ev.type);
    });

    DisplayInfo disp1;
    disp1.id = 10;
    disp1.name = "DSI-1";
    disp1.pixel_width = 1080;
    disp1.pixel_height = 2400;
    disp1.logical_width = 540;
    disp1.logical_height = 1200;
    disp1.width = 540;
    disp1.height = 1200;
    disp1.scale = 2;

    // Register synthetic display
    manager.register_synthetic_display(disp1);
    EXPECT_EQ(manager.displays().size(), 1u);
    ASSERT_TRUE(manager.primary_display().has_value());
    EXPECT_EQ(manager.primary_display()->id, 10u);
    EXPECT_EQ(manager.primary_display_id(), 10u);
    EXPECT_NE(manager.primary_policy(), nullptr);

    // Check DisplayAdded event received
    ASSERT_FALSE(received_events.empty());
    EXPECT_EQ(received_events.front(), DisplayEventType::DisplayAdded);

    // Add second display (e.g. external HDMI)
    DisplayInfo disp2;
    disp2.id = 20;
    disp2.name = "HDMI-A-1";
    disp2.pixel_width = 1920;
    disp2.pixel_height = 1080;
    disp2.logical_width = 1920;
    disp2.logical_height = 1080;
    disp2.width = 1920;
    disp2.height = 1080;
    disp2.scale = 1;

    manager.register_synthetic_display(disp2);
    EXPECT_EQ(manager.displays().size(), 2u);

    // Query display by name and ID
    EXPECT_TRUE(manager.find_display_by_name("HDMI-A-1").has_value());
    EXPECT_TRUE(manager.find_display_by_id(20).has_value());

    // Remove primary display: should fallback to remaining display safely
    manager.remove_synthetic_display(10);
    EXPECT_EQ(manager.displays().size(), 1u);
    ASSERT_TRUE(manager.primary_display().has_value());
    EXPECT_EQ(manager.primary_display()->id, 20u);

    // Check DisplayRemoved event emitted
    bool had_removed_event = false;
    for (auto ev : received_events) {
        if (ev == DisplayEventType::DisplayRemoved) had_removed_event = true;
    }
    EXPECT_TRUE(had_removed_event);
}

// 9. Zero and Invalid Dimension Resilience
TEST(DisplayPolicyTest, InvalidDimensionsResilience) {
    DisplayInfo invalid_disp;
    invalid_disp.id = 99;
    invalid_disp.logical_width = 0;
    invalid_disp.logical_height = -100;
    invalid_disp.scale = 0;

    // Should not crash and should fall back to safe fallback bounds
    DisplayPolicy policy(invalid_disp);
    EXPECT_GT(policy.available_window_geometry().width, 0);
    EXPECT_GT(policy.available_window_geometry().height, 0);
    EXPECT_GT(policy.maximized_geometry().width, 0);
    EXPECT_GT(policy.fullscreen_geometry().width, 0);

    Size def_sz = policy.default_window_size();
    EXPECT_GT(def_sz.width, 0);
    EXPECT_GT(def_sz.height, 0);
}

