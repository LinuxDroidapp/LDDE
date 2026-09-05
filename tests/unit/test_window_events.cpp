#include <gtest/gtest.h>
#include "ldde/window/window_registry.hpp"

using namespace ldde::window;
using ldde::core::Rect;

TEST(WindowEventsTest, CreatedAndDestroyedEvents) {
    WindowRegistry reg;
    std::vector<WindowEvent> events;

    auto listener_id = reg.add_listener([&](const WindowEvent& ev) {
        events.push_back(ev);
    });

    auto win = std::make_shared<Window>(10, nullptr, nullptr, nullptr);
    static_cast<void>(reg.add_window(win));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, WindowEventType::Created);
    EXPECT_EQ(events[0].window_id, 10u);
    EXPECT_EQ(events[0].property_name, "created");

    static_cast<void>(reg.remove_window(10));
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[1].type, WindowEventType::Destroyed);
    EXPECT_EQ(events[1].window_id, 10u);
    EXPECT_EQ(events[1].property_name, "destroyed");

    reg.remove_listener(listener_id);
}

TEST(WindowEventsTest, FocusChangedEvents) {
    WindowRegistry reg;
    std::vector<WindowEvent> events;

    reg.add_listener([&](const WindowEvent& ev) {
        if (ev.type == WindowEventType::FocusChanged) {
            events.push_back(ev);
        }
    });

    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);
    static_cast<void>(reg.add_window(win1));
    static_cast<void>(reg.add_window(win2));

    reg.set_active_window(1);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].window_id, 1u);
    EXPECT_EQ(events[0].property_name, "focused");

    // Switching focus triggers unfocused on win1 and focused on win2
    reg.set_active_window(2);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[1].window_id, 1u);
    EXPECT_EQ(events[1].property_name, "unfocused");
    EXPECT_EQ(events[2].window_id, 2u);
    EXPECT_EQ(events[2].property_name, "focused");
}

TEST(WindowEventsTest, DispatchAllEventTypes) {
    WindowRegistry reg;
    std::vector<WindowEventType> captured_types;

    reg.add_listener([&](const WindowEvent& ev) {
        captured_types.push_back(ev.type);
    });

    auto win = std::make_shared<Window>(1, nullptr, nullptr, nullptr);

    std::vector<WindowEventType> expected = {
        WindowEventType::Created,
        WindowEventType::TitleChanged,
        WindowEventType::AppIdChanged,
        WindowEventType::GeometryChanged,
        WindowEventType::StateChanged,
        WindowEventType::FocusChanged,
        WindowEventType::VisibilityChanged,
        WindowEventType::ParentChanged,
        WindowEventType::Closed,
        WindowEventType::Destroyed
    };

    for (auto type : expected) {
        reg.dispatch_event(WindowEvent{
            .type = type,
            .window_id = win->id(),
            .window = win,
            .property_name = "test"
        });
    }

    EXPECT_EQ(captured_types, expected);
}

TEST(WindowEventsTest, MultipleListeners) {
    WindowRegistry reg;
    int count1 = 0;
    int count2 = 0;

    auto id1 = reg.add_listener([&](const WindowEvent&) { count1++; });
    auto id2 = reg.add_listener([&](const WindowEvent&) { count2++; });

    auto win = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    static_cast<void>(reg.add_window(win));

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);

    reg.remove_listener(id1);

    static_cast<void>(reg.remove_window(1));
    EXPECT_EQ(count1, 1); // Not called after removal
    EXPECT_EQ(count2, 2); // Still called

    reg.remove_listener(id2);
}

