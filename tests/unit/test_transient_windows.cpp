#include <gtest/gtest.h>
#include "ldde/window/window_registry.hpp"

using namespace ldde::window;

TEST(TransientWindowsTest, ParentChildHierarchy) {
    WindowRegistry reg;

    auto parent = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    parent->set_title("Main Application");
    parent->set_app_id("org.example.App");

    auto dialog1 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);
    dialog1->set_title("Open File");
    dialog1->set_app_id("org.example.App");
    dialog1->set_parent_id(1);

    auto dialog2 = std::make_shared<Window>(3, nullptr, nullptr, nullptr);
    dialog2->set_title("Preferences");
    dialog2->set_app_id("org.example.App");
    dialog2->set_parent_id(1);

    static_cast<void>(reg.add_window(parent));
    static_cast<void>(reg.add_window(dialog1));
    static_cast<void>(reg.add_window(dialog2));

    EXPECT_FALSE(parent->parent_id().has_value());
    ASSERT_TRUE(dialog1->parent_id().has_value());
    EXPECT_EQ(*dialog1->parent_id(), 1u);
    ASSERT_TRUE(dialog2->parent_id().has_value());
    EXPECT_EQ(*dialog2->parent_id(), 1u);

    // Query all windows for app
    auto app_windows = reg.windows_for_app("org.example.App");
    EXPECT_EQ(app_windows.size(), 3u);
}

TEST(TransientWindowsTest, ReparentingAndDetaching) {
    auto child = std::make_shared<Window>(10, nullptr, nullptr, nullptr);
    EXPECT_FALSE(child->parent_id().has_value());

    child->set_parent_id(1);
    ASSERT_TRUE(child->parent_id().has_value());
    EXPECT_EQ(*child->parent_id(), 1u);

    // Reparent to another window
    child->set_parent_id(2);
    ASSERT_TRUE(child->parent_id().has_value());
    EXPECT_EQ(*child->parent_id(), 2u);

    // Detach parent
    child->set_parent_id(std::nullopt);
    EXPECT_FALSE(child->parent_id().has_value());
}

TEST(TransientWindowsTest, DestructionWithChildren) {
    WindowRegistry reg;

    auto parent = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto child = std::make_shared<Window>(2, nullptr, nullptr, nullptr);
    child->set_parent_id(1);

    static_cast<void>(reg.add_window(parent));
    static_cast<void>(reg.add_window(child));

    // When parent is destroyed, child remains in registry with parent_id or can be detached
    static_cast<void>(reg.remove_window(1));
    EXPECT_EQ(reg.lookup(1), nullptr);

    auto retrieved_child = reg.lookup(2);
    ASSERT_NE(retrieved_child, nullptr);
    EXPECT_EQ(retrieved_child->parent_id(), 1u);

    static_cast<void>(reg.remove_window(2));
    EXPECT_TRUE(reg.empty());
}

