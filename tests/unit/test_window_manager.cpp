#include <gtest/gtest.h>
#include "ldde/window/window_manager.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/config/config.hpp"

using namespace ldde::core;
using namespace ldde::window;
using namespace ldde::display;

class TestWMBackend : public WindowManagementBackend {
public:
    WindowRegistry* registry = nullptr;
    std::vector<WindowId> closed_windows;
    std::vector<WindowId> activated_windows;
    std::vector<std::pair<WindowId, Rect>> geometry_updates;
    std::vector<std::pair<WindowId, bool>> maximize_updates;

    Status activate(WindowId id) override {
        activated_windows.push_back(id);
        return Status::ok();
    }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId id) override {
        closed_windows.push_back(id);
        return Status::ok();
    }
    Status set_geometry(WindowId id, const Rect& geom) override {
        geometry_updates.push_back({id, geom});
        if (registry) {
            auto win = registry->lookup(id);
            if (win) win->set_geometry(geom);
        }
        return Status::ok();
    }
    Status set_maximized(WindowId id, bool maximized, const Size&) override {
        maximize_updates.push_back({id, maximized});
        return Status::ok();
    }
    Status set_fullscreen(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_minimized(WindowId, bool) override { return Status::ok(); }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

class WindowManagerTest : public ::testing::Test {
protected:
    WindowRegistry registry_;
    WindowTracker tracker_;
    DisplayManager display_mgr_;
    std::unique_ptr<WindowManager> wm_;
    TestWMBackend* backend_ptr_ = nullptr;

    void SetUp() override {
        auto backend = std::make_unique<TestWMBackend>();
        backend_ptr_ = backend.get();
        backend_ptr_->registry = &registry_;
        wm_ = std::make_unique<WindowManager>(registry_, tracker_, display_mgr_, std::move(backend));

        ldde::config::Config cfg;
        cfg.load_defaults();
        Status s = wm_->initialize(cfg);
        ASSERT_TRUE(s.is_ok());
    }

    void TearDown() override {
        wm_->shutdown();
    }

    std::shared_ptr<Window> create_and_register_window(WindowId id) {
        auto win = std::make_shared<Window>(id, nullptr, nullptr, nullptr);
        win->set_title("Test App");
        win->set_app_id("org.test.app");
        win->set_visible(true);
        static_cast<void>(win->transition_to(WindowLifecycleState::Visible));
        static_cast<void>(registry_.add_window(win));
        return win;
    }
};

TEST_F(WindowManagerTest, WindowCreationAutoStackAndFocus) {
    auto w1 = create_and_register_window(1);

    EXPECT_EQ(wm_->stacking_order().size(), 1u);
    EXPECT_EQ(wm_->active_window_id(), 1);
    EXPECT_EQ(wm_->top_window_id(), 1);

    auto w2 = create_and_register_window(2);
    EXPECT_EQ(wm_->stacking_order().size(), 2u);
    EXPECT_EQ(wm_->active_window_id(), 2);
    EXPECT_EQ(wm_->top_window_id(), 2);
}

TEST_F(WindowManagerTest, MaximizeAndRestoreWindow) {
    auto w1 = create_and_register_window(1);

    Status s = wm_->maximize(1);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(w1->state(), WindowState::Maximized);

    s = wm_->restore(1);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(w1->state(), WindowState::Normal);
}

TEST_F(WindowManagerTest, MinimizeAndRestoreWindow) {
    auto w1 = create_and_register_window(1);
    auto w2 = create_and_register_window(2);

    EXPECT_EQ(wm_->active_window_id(), 2);

    // Minimize active window w2
    Status s = wm_->minimize(2);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(w2->state(), WindowState::Minimized);
    EXPECT_FALSE(w2->is_visible());

    // Active window should automatically fall back to w1
    EXPECT_EQ(wm_->active_window_id(), 1);

    // Minimized windows query
    auto min_list = wm_->minimized_windows();
    EXPECT_EQ(min_list.size(), 1u);
    EXPECT_EQ(min_list[0]->id(), 2);

    // Restore w2
    s = wm_->restore(2);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(w2->state(), WindowState::Normal);
    EXPECT_TRUE(w2->is_visible());
}

TEST_F(WindowManagerTest, CloseWindowAndFallbackFocus) {
    create_and_register_window(1);
    create_and_register_window(2);

    EXPECT_EQ(wm_->active_window_id(), 2);

    Status s = wm_->close(2);
    EXPECT_TRUE(s.is_ok());
    ASSERT_EQ(backend_ptr_->closed_windows.size(), 1u);
    EXPECT_EQ(backend_ptr_->closed_windows[0], 2);

    // Simulate compositor destroy event
    static_cast<void>(registry_.remove_window(2));
    EXPECT_EQ(wm_->active_window_id(), 1);
    EXPECT_EQ(wm_->top_window_id(), 1);
}

TEST_F(WindowManagerTest, InteractiveMoveWindow) {
    auto w1 = create_and_register_window(1);
    Rect initial = w1->geometry();

    Point start_pt{initial.x + 50, initial.y + 15};
    bool started = wm_->start_move(1, start_pt, false);
    EXPECT_TRUE(started);

    wm_->update_move(Point{start_pt.x + 40, start_pt.y + 20});
    Rect final_geom = wm_->end_move();

    EXPECT_EQ(final_geom.x, initial.x + 40);
    EXPECT_EQ(final_geom.y, initial.y + 20);
    EXPECT_EQ(w1->geometry(), final_geom);
}

TEST_F(WindowManagerTest, HeaderPointerClickActions) {
    auto w1 = create_and_register_window(1);
    Rect geom = w1->geometry();

    // Click Close Button
    Rect close_rect = wm_->controls().get_close_button_rect(geom, false);
    Point close_click{close_rect.x + 5, close_rect.y + 5};

    bool handled = wm_->handle_pointer_click(close_click, 1000);
    EXPECT_TRUE(handled);
    ASSERT_EQ(backend_ptr_->closed_windows.size(), 1u);
    EXPECT_EQ(backend_ptr_->closed_windows[0], 1);
}

TEST_F(WindowManagerTest, TouchHeaderHitTestMobile) {
    auto w1 = create_and_register_window(1);
    Rect geom = w1->geometry();

    // Click Maximize Button with touch
    Rect max_rect = wm_->controls().get_maximize_button_rect(geom, true);
    Point max_touch{max_rect.x + 10, max_rect.y + 10};

    bool handled = wm_->handle_touch_tap(max_touch, 1000);
    EXPECT_TRUE(handled);
    EXPECT_EQ(w1->state(), WindowState::Maximized);
}
