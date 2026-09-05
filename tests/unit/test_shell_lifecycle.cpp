#include <gtest/gtest.h>
#include "ldde/shell/types.hpp"
#include "ldde/shell/shell.hpp"

using namespace ldde::shell;

TEST(ShellTypesTest, EnumToStringNames) {
    EXPECT_EQ(shell_region_name(ShellRegionType::None), "None");
    EXPECT_EQ(shell_region_name(ShellRegionType::Desktop), "Desktop");
    EXPECT_EQ(shell_region_name(ShellRegionType::Status), "Status");
    EXPECT_EQ(shell_region_name(ShellRegionType::Dock), "Dock");
    EXPECT_EQ(shell_region_name(ShellRegionType::Overlay), "Overlay");

    EXPECT_EQ(shell_layer_name(ShellLayer::Background), "Background");
    EXPECT_EQ(shell_layer_name(ShellLayer::Bottom), "Bottom");
    EXPECT_EQ(shell_layer_name(ShellLayer::Top), "Top");
    EXPECT_EQ(shell_layer_name(ShellLayer::Overlay), "Overlay");

    EXPECT_EQ(shell_state_name(ShellLifecycleState::Created), "CREATED");
    EXPECT_EQ(shell_state_name(ShellLifecycleState::Initializing), "INITIALIZING");
    EXPECT_EQ(shell_state_name(ShellLifecycleState::Ready), "READY");
    EXPECT_EQ(shell_state_name(ShellLifecycleState::Running), "RUNNING");
    EXPECT_EQ(shell_state_name(ShellLifecycleState::Stopped), "STOPPED");
    EXPECT_EQ(shell_state_name(ShellLifecycleState::Failed), "FAILED");

    EXPECT_EQ(dock_position_name(DockPosition::Bottom), "bottom");
    EXPECT_EQ(dock_position_name(DockPosition::Left), "left");
    EXPECT_EQ(dock_position_name(DockPosition::Right), "right");
}

TEST(ShellLifecycleTest, InitialStateAndShutdown) {
    Shell shell;
    EXPECT_EQ(shell.state(), ShellLifecycleState::Created);
    EXPECT_FALSE(shell.is_ready());

    shell.shutdown();
    EXPECT_EQ(shell.state(), ShellLifecycleState::Stopped);
}

TEST(ShellLifecycleTest, UninitializedSurfaceAccess) {
    Shell shell;
    EXPECT_FALSE(shell.desktop().is_created());
    EXPECT_FALSE(shell.status_region().is_created());
    EXPECT_FALSE(shell.dock_region().is_created());
    EXPECT_FALSE(shell.overlay().is_created());
    EXPECT_EQ(shell.focused_region(), ShellRegionType::None);
}
