#include <gtest/gtest.h>
#include "ldde/window/window_stacking.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window.hpp"

using namespace ldde::core;
using namespace ldde::window;

class WindowStackingTest : public ::testing::Test {
protected:
    WindowStacking stacking_;
    WindowRegistry registry_;

    std::shared_ptr<Window> add_window(WindowId id) {
        auto win = std::make_shared<Window>(id, nullptr, nullptr, nullptr);
        win->set_visible(true);
        win->set_state(WindowState::Normal);
        static_cast<void>(registry_.add_window(win));
        return win;
    }
};

TEST_F(WindowStackingTest, AddAndContains) {
    stacking_.add(1);
    stacking_.add(2);
    stacking_.add(3);

    EXPECT_EQ(stacking_.size(), 3u);
    EXPECT_TRUE(stacking_.contains(1));
    EXPECT_TRUE(stacking_.contains(2));
    EXPECT_TRUE(stacking_.contains(3));
    EXPECT_FALSE(stacking_.contains(4));

    EXPECT_EQ(stacking_.bottom(), 1);
    EXPECT_EQ(stacking_.top(), 3);
}

TEST_F(WindowStackingTest, RaiseWindow) {
    stacking_.add(1);
    stacking_.add(2);
    stacking_.add(3);

    // Raise 1 to top
    stacking_.raise(1);
    EXPECT_EQ(stacking_.top(), 1);
    const auto& s = stacking_.stack();
    std::vector<WindowId> expected = {2, 3, 1};
    EXPECT_EQ(s, expected);

    // Raise 2 to top
    stacking_.raise(2);
    EXPECT_EQ(stacking_.top(), 2);
    expected = {3, 1, 2};
    EXPECT_EQ(stacking_.stack(), expected);
}

TEST_F(WindowStackingTest, LowerWindow) {
    stacking_.add(1);
    stacking_.add(2);
    stacking_.add(3);

    // Lower 3 to bottom
    stacking_.lower(3);
    EXPECT_EQ(stacking_.bottom(), 3);
    std::vector<WindowId> expected = {3, 1, 2};
    EXPECT_EQ(stacking_.stack(), expected);
}

TEST_F(WindowStackingTest, RemoveWindow) {
    stacking_.add(1);
    stacking_.add(2);
    stacking_.add(3);

    stacking_.remove(2);
    EXPECT_EQ(stacking_.size(), 2u);
    EXPECT_FALSE(stacking_.contains(2));
    std::vector<WindowId> expected = {1, 3};
    EXPECT_EQ(stacking_.stack(), expected);
}

TEST_F(WindowStackingTest, TransientDialogAboveParent) {
    // Window 1 is parent, Window 2 is transient child of 1
    stacking_.add(1);
    stacking_.add(2, 1);
    stacking_.add(3); // unrelated window

    // Initial order: [1, 2, 3]
    EXPECT_EQ(stacking_.top(), 3);

    // When parent 1 is raised, child 2 should be raised along with it and stay above 1
    stacking_.raise(1);
    const auto& s = stacking_.stack();
    // 3 should now be at the bottom, followed by 1, with child 2 at the top
    std::vector<WindowId> expected = {3, 1, 2};
    EXPECT_EQ(s, expected);
    EXPECT_EQ(stacking_.top(), 2);
}

TEST_F(WindowStackingTest, VisibleStackFiltersHiddenOrMinimized) {
    auto w1 = add_window(1);
    auto w2 = add_window(2);
    auto w3 = add_window(3);

    stacking_.add(1);
    stacking_.add(2);
    stacking_.add(3);

    // All visible initially
    auto visible = stacking_.visible_stack(registry_);
    EXPECT_EQ(visible.size(), 3u);

    // Minimize w2
    w2->set_state(WindowState::Minimized);
    w2->set_visible(false);

    visible = stacking_.visible_stack(registry_);
    ASSERT_EQ(visible.size(), 2u);
    EXPECT_EQ(visible[0], 1);
    EXPECT_EQ(visible[1], 3);
}
