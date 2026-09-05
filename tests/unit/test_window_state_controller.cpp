#include <gtest/gtest.h>
#include "ldde/window/window_state_controller.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_placement.hpp"
#include "ldde/window/window_management_backend.hpp"

using namespace ldde::core;
using namespace ldde::display;
using namespace ldde::window;

class StateMockBackend : public WindowManagementBackend {
public:
    struct MaximizedCall {
        WindowId id;
        bool maximized;
        Size target_size;
    };
    struct FullscreenCall {
        WindowId id;
        bool fullscreen;
        Size target_size;
    };

    std::vector<MaximizedCall> maximized_calls;
    std::vector<FullscreenCall> fullscreen_calls;
    std::vector<std::pair<WindowId, bool>> minimized_calls;
    std::vector<std::pair<WindowId, Rect>> geometry_calls;

    Status activate(WindowId) override { return Status::ok(); }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId) override { return Status::ok(); }

    Status set_geometry(WindowId id, const Rect& geom) override {
        geometry_calls.push_back({id, geom});
        return Status::ok();
    }

    Status set_maximized(WindowId id, bool maximized, const Size& target_size) override {
        maximized_calls.push_back({id, maximized, target_size});
        return Status::ok();
    }

    Status set_fullscreen(WindowId id, bool fullscreen, const Size& target_size) override {
        fullscreen_calls.push_back({id, fullscreen, target_size});
        return Status::ok();
    }

    Status set_minimized(WindowId id, bool minimized) override {
        minimized_calls.push_back({id, minimized});
        return Status::ok();
    }

    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

class WindowStateControllerTest : public ::testing::Test {
protected:
    StateMockBackend backend_;
    WindowPlacement placement_;
    std::unique_ptr<WindowStateController> controller_;
    DisplayInfo display_;

    void SetUp() override {
        display_.width = 1920;
        display_.height = 1080;

        PlacementConstraints constraints;
        constraints.status_bar_height = 40;
        constraints.dock_height = 68;
        constraints.margin_top = 8;
        constraints.margin_bottom = 8;
        constraints.margin_horizontal = 8;
        placement_.set_constraints(constraints);

        controller_ = std::make_unique<WindowStateController>(backend_, placement_);
    }
};

TEST_F(WindowStateControllerTest, MaximizeAndRestore) {
    auto win = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    win->set_geometry(Rect{200, 150, 800, 600});
    win->set_state(WindowState::Normal);

    // Maximize
    Status s = controller_->maximize(win, display_);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(win->state(), WindowState::Maximized);
    EXPECT_TRUE(win->saved_geometry().has_value());
    EXPECT_EQ(win->saved_geometry()->x, 200);
    EXPECT_EQ(win->saved_geometry()->y, 150);
    EXPECT_EQ(win->saved_geometry()->width, 800);
    EXPECT_EQ(win->saved_geometry()->height, 600);

    Rect usable = placement_.get_usable_area(display_);
    EXPECT_EQ(win->geometry(), usable);
    ASSERT_EQ(backend_.maximized_calls.size(), 1u);
    EXPECT_EQ(backend_.maximized_calls[0].id, 1);
    EXPECT_TRUE(backend_.maximized_calls[0].maximized);
    EXPECT_EQ(backend_.maximized_calls[0].target_size.width, usable.width);
    EXPECT_EQ(backend_.maximized_calls[0].target_size.height, usable.height);

    // Restore
    s = controller_->restore(win, display_);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(win->state(), WindowState::Normal);
    EXPECT_FALSE(win->saved_geometry().has_value());
    EXPECT_EQ(win->geometry(), (Rect{200, 150, 800, 600}));

    ASSERT_EQ(backend_.maximized_calls.size(), 2u);
    EXPECT_FALSE(backend_.maximized_calls[1].maximized);
    EXPECT_EQ(backend_.maximized_calls[1].target_size.width, 800);
    EXPECT_EQ(backend_.maximized_calls[1].target_size.height, 600);
}

TEST_F(WindowStateControllerTest, FullscreenAndRestore) {
    auto win = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    win->set_geometry(Rect{300, 200, 640, 480});
    win->set_state(WindowState::Normal);

    // Fullscreen takes entire display (0, 0, width, height)
    Status s = controller_->fullscreen(win, display_);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(win->state(), WindowState::Fullscreen);
    EXPECT_EQ(win->geometry(), (Rect{0, 0, 1920, 1080}));
    EXPECT_TRUE(win->saved_geometry().has_value());
    EXPECT_EQ(win->saved_geometry()->width, 640);

    // Restore from Fullscreen
    s = controller_->restore(win, display_);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(win->state(), WindowState::Normal);
    EXPECT_EQ(win->geometry(), (Rect{300, 200, 640, 480}));
}

TEST_F(WindowStateControllerTest, MinimizeAndRestore) {
    auto win = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    win->set_geometry(Rect{100, 100, 500, 400});
    win->set_state(WindowState::Normal);
    win->set_visible(true);

    Status s = controller_->minimize(win);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(win->state(), WindowState::Minimized);
    EXPECT_FALSE(win->is_visible());
    ASSERT_EQ(backend_.minimized_calls.size(), 1u);
    EXPECT_TRUE(backend_.minimized_calls[0].second);

    // Restore
    s = controller_->restore_minimized(win);
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(win->state(), WindowState::Normal);
    EXPECT_TRUE(win->is_visible());
    EXPECT_EQ(win->geometry(), (Rect{100, 100, 500, 400}));
    ASSERT_EQ(backend_.minimized_calls.size(), 2u);
    EXPECT_FALSE(backend_.minimized_calls[1].second);
}

TEST_F(WindowStateControllerTest, AdaptToDisplayChange) {
    auto win_max = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    win_max->set_state(WindowState::Maximized);
    win_max->set_geometry(placement_.get_usable_area(display_));

    // Screen rotates to portrait (1080x1920)
    DisplayInfo new_disp;
    new_disp.width = 1080;
    new_disp.height = 1920;

    controller_->adapt_to_display_change(win_max, new_disp);
    Rect new_usable = placement_.get_usable_area(new_disp);
    EXPECT_EQ(win_max->geometry(), new_usable);
}
