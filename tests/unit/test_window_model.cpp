#include <gtest/gtest.h>
#include "ldde/window/window.hpp"
#include "ldde/window/types.hpp"

using namespace ldde::window;
using ldde::core::Rect;
using ldde::core::Size;

TEST(WindowModelTest, InitialProperties) {
    Window win(42, nullptr, nullptr, nullptr);

    EXPECT_EQ(win.id(), 42u);
    EXPECT_EQ(win.surface(), nullptr);
    EXPECT_EQ(win.xdg_surf(), nullptr);
    EXPECT_EQ(win.toplevel(), nullptr);
    EXPECT_TRUE(win.title().empty());
    EXPECT_TRUE(win.app_id().empty());
    EXPECT_EQ(win.state(), WindowState::Normal);
    EXPECT_EQ(win.requested_state(), WindowState::Normal);
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Discovered);
    EXPECT_FALSE(win.is_active());
    EXPECT_FALSE(win.is_visible());
    EXPECT_FALSE(win.parent_id().has_value());
    EXPECT_EQ(win.geometry(), (Rect{0, 0, 0, 0}));
    EXPECT_EQ(win.surface_size(), (Size{0, 0}));
    EXPECT_EQ(win.last_configure_serial(), 0u);
}

TEST(WindowModelTest, TitleAndAppIdMutations) {
    Window win(1, nullptr, nullptr, nullptr);

    win.set_title("Terminal");
    EXPECT_EQ(win.title(), "Terminal");

    win.set_app_id("org.example.Terminal");
    EXPECT_EQ(win.app_id(), "org.example.Terminal");

    // Overwrite
    win.set_title("Editor - Untitled");
    EXPECT_EQ(win.title(), "Editor - Untitled");
    win.set_app_id("org.example.Editor");
    EXPECT_EQ(win.app_id(), "org.example.Editor");
}

TEST(WindowModelTest, GeometryAndSizeMutations) {
    Window win(2, nullptr, nullptr, nullptr);

    Rect r{100, 150, 800, 600};
    win.set_geometry(r);
    EXPECT_EQ(win.geometry(), r);

    Size s{800, 600};
    win.set_surface_size(s);
    EXPECT_EQ(win.surface_size(), s);
}

TEST(WindowModelTest, StateTransitions) {
    Window win(3, nullptr, nullptr, nullptr);

    EXPECT_EQ(win.state(), WindowState::Normal);

    win.set_state(WindowState::Maximized);
    EXPECT_EQ(win.state(), WindowState::Maximized);

    win.set_state(WindowState::Fullscreen);
    EXPECT_EQ(win.state(), WindowState::Fullscreen);

    win.set_state(WindowState::Minimized);
    EXPECT_EQ(win.state(), WindowState::Minimized);

    win.set_state(WindowState::Normal);
    EXPECT_EQ(win.state(), WindowState::Normal);

    win.set_requested_state(WindowState::Maximized);
    EXPECT_EQ(win.requested_state(), WindowState::Maximized);
}

TEST(WindowModelTest, ActiveAndVisibilityTracking) {
    Window win(4, nullptr, nullptr, nullptr);

    EXPECT_FALSE(win.is_active());
    win.set_active(true);
    EXPECT_TRUE(win.is_active());
    win.set_active(false);
    EXPECT_FALSE(win.is_active());

    EXPECT_FALSE(win.is_visible());
    win.set_visible(true);
    EXPECT_TRUE(win.is_visible());
    win.set_visible(false);
    EXPECT_FALSE(win.is_visible());
}

TEST(WindowModelTest, ParentAttachment) {
    Window win(5, nullptr, nullptr, nullptr);

    EXPECT_FALSE(win.parent_id().has_value());

    win.set_parent_id(100);
    ASSERT_TRUE(win.parent_id().has_value());
    EXPECT_EQ(*win.parent_id(), 100u);

    win.set_parent_id(std::nullopt);
    EXPECT_FALSE(win.parent_id().has_value());
}

TEST(WindowModelTest, LifecycleTransitions) {
    Window win(10, nullptr, nullptr, nullptr);
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Discovered);

    // Discovered -> Initializing: OK
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Initializing).is_ok());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Initializing);

    // Initializing -> Visible is invalid (must go through Ready)
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Visible).is_error());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Initializing);

    // Initializing -> Ready: OK
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Ready).is_ok());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Ready);

    // Ready -> Visible: OK
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Visible).is_ok());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Visible);

    // Visible -> Ready: OK (e.g. unmapped / hidden)
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Ready).is_ok());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Ready);

    // Ready -> Closing: OK
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Closing).is_ok());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Closing);

    // Closing -> Destroyed: OK
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Destroyed).is_ok());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Destroyed);

    // Destroyed -> anything else is invalid
    EXPECT_TRUE(win.transition_to(WindowLifecycleState::Visible).is_error());
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Destroyed);
}

TEST(WindowModelTest, RequestCloseAndAckConfigure) {
    Window win(11, nullptr, nullptr, nullptr);
    static_cast<void>(win.transition_to(WindowLifecycleState::Initializing));
    static_cast<void>(win.transition_to(WindowLifecycleState::Ready));

    win.ack_configure(42);
    EXPECT_EQ(win.last_configure_serial(), 42u);

    win.request_close();
    EXPECT_EQ(win.lifecycle_state(), WindowLifecycleState::Closing);
}

