#include <gtest/gtest.h>
#include "ldde/window/window_focus.hpp"
#include "ldde/window/window_stacking.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_management_backend.hpp"

using namespace ldde::core;
using namespace ldde::window;

class FocusMockBackend : public WindowManagementBackend {
public:
    std::vector<std::pair<WindowId, bool>> activate_events;

    Status activate(WindowId id) override {
        activate_events.push_back({id, true});
        return Status::ok();
    }

    Status deactivate(WindowId id) override {
        activate_events.push_back({id, false});
        return Status::ok();
    }

    Status close(WindowId) override { return Status::ok(); }
    Status set_geometry(WindowId, const Rect&) override { return Status::ok(); }
    Status set_maximized(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_fullscreen(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_minimized(WindowId, bool) override { return Status::ok(); }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

class WindowFocusTest : public ::testing::Test {
protected:
    WindowRegistry registry_;
    WindowStacking stacking_;
    FocusMockBackend backend_;
    std::unique_ptr<WindowFocus> focus_;

    void SetUp() override {
        focus_ = std::make_unique<WindowFocus>(registry_, stacking_, backend_);
    }

    std::shared_ptr<Window> add_test_window(WindowId id) {
        auto win = std::make_shared<Window>(id, nullptr, nullptr, nullptr);
        win->set_visible(true);
        static_cast<void>(win->transition_to(WindowLifecycleState::Visible));
        static_cast<void>(registry_.add_window(win));
        stacking_.add(id);
        return win;
    }
};

TEST_F(WindowFocusTest, InitiallyNoActiveWindow) {
    EXPECT_FALSE(focus_->active_window_id().has_value());
    EXPECT_FALSE(focus_->is_active(1));
}

TEST_F(WindowFocusTest, ActivateWindow) {
    auto w1 = add_test_window(1);
    auto w2 = add_test_window(2);

    Status s = focus_->activate(1);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(focus_->active_window_id(), 1);
    EXPECT_TRUE(focus_->is_active(1));
    EXPECT_FALSE(focus_->is_active(2));
    EXPECT_TRUE(w1->is_active());
    EXPECT_FALSE(w2->is_active());

    // Switch focus to w2
    s = focus_->activate(2);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(focus_->active_window_id(), 2);
    EXPECT_FALSE(focus_->is_active(1));
    EXPECT_TRUE(focus_->is_active(2));
    EXPECT_FALSE(w1->is_active());
    EXPECT_TRUE(w2->is_active());
}

TEST_F(WindowFocusTest, ActivatingRaisesWindowToTop) {
    add_test_window(1);
    add_test_window(2);
    add_test_window(3);

    // Initial stacking: [1, 2, 3] (3 is top)
    EXPECT_EQ(stacking_.top(), 3);

    // Focus 1 -> should raise 1 to top
    focus_->activate(1);
    EXPECT_EQ(stacking_.top(), 1);
}

TEST_F(WindowFocusTest, FallbackOnActiveWindowRemoved) {
    add_test_window(1);
    add_test_window(2);
    add_test_window(3);

    focus_->activate(3);
    EXPECT_EQ(focus_->active_window_id(), 3);

    // Window 3 is closed / removed
    static_cast<void>(registry_.remove_window(3));
    stacking_.remove(3);
    focus_->handle_window_removed_or_hidden(3);

    // Should automatically fall back to topmost remaining visible window (w2)
    EXPECT_EQ(focus_->active_window_id(), 2);
    EXPECT_EQ(stacking_.top(), 2);
}

TEST_F(WindowFocusTest, FallbackOnActiveWindowMinimized) {
    auto w1 = add_test_window(1);
    auto w2 = add_test_window(2);

    focus_->activate(2);
    EXPECT_EQ(focus_->active_window_id(), 2);

    // w2 is minimized
    w2->set_state(WindowState::Minimized);
    w2->set_visible(false);
    focus_->handle_window_removed_or_hidden(2);

    // Topmost visible window should now be w1
    EXPECT_EQ(focus_->active_window_id(), 1);
    EXPECT_FALSE(w2->is_active());
    EXPECT_TRUE(w1->is_active());
}

TEST_F(WindowFocusTest, DeactivateClearsActiveWindow) {
    auto w1 = add_test_window(1);
    focus_->activate(1);
    EXPECT_TRUE(w1->is_active());

    focus_->deactivate();
    EXPECT_FALSE(focus_->active_window_id().has_value());
    EXPECT_FALSE(w1->is_active());
}

