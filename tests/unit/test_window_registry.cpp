#include <gtest/gtest.h>
#include "ldde/window/window_registry.hpp"

using namespace ldde::window;

TEST(WindowRegistryTest, EmptyInitially) {
    WindowRegistry reg;
    EXPECT_TRUE(reg.empty());
    EXPECT_EQ(reg.count(), 0u);
    EXPECT_FALSE(reg.active_window_id().has_value());
    EXPECT_TRUE(reg.windows().empty());
}

TEST(WindowRegistryTest, AddAndLookupWindow) {
    WindowRegistry reg;
    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    win1->set_title("Calculator");
    win1->set_app_id("org.example.Calculator");

    EXPECT_TRUE(reg.add_window(win1).is_ok());
    EXPECT_FALSE(reg.empty());
    EXPECT_EQ(reg.count(), 1u);

    auto found = reg.lookup(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id(), 1u);
    EXPECT_EQ(found->title(), "Calculator");
    EXPECT_EQ(found->app_id(), "org.example.Calculator");

    EXPECT_EQ(reg.lookup(999), nullptr);
}

TEST(WindowRegistryTest, RejectDuplicateIdOrNull) {
    WindowRegistry reg;
    EXPECT_TRUE(reg.add_window(nullptr).is_error());

    auto win1 = std::make_shared<Window>(10, nullptr, nullptr, nullptr);
    EXPECT_TRUE(reg.add_window(win1).is_ok());

    auto duplicate = std::make_shared<Window>(10, nullptr, nullptr, nullptr);
    EXPECT_TRUE(reg.add_window(duplicate).is_error());
    EXPECT_EQ(reg.count(), 1u);
}

TEST(WindowRegistryTest, RemoveWindow) {
    WindowRegistry reg;
    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);

    EXPECT_TRUE(reg.add_window(win1).is_ok());
    EXPECT_TRUE(reg.add_window(win2).is_ok());
    EXPECT_EQ(reg.count(), 2u);

    EXPECT_TRUE(reg.remove_window(1).is_ok());
    EXPECT_EQ(reg.count(), 1u);
    EXPECT_EQ(reg.lookup(1), nullptr);
    EXPECT_NE(reg.lookup(2), nullptr);

    // Removing non-existent window fails
    EXPECT_TRUE(reg.remove_window(999).is_error());
}

TEST(WindowRegistryTest, WindowsOrderPreserved) {
    WindowRegistry reg;
    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);
    auto win3 = std::make_shared<Window>(3, nullptr, nullptr, nullptr);

    EXPECT_TRUE(reg.add_window(win1).is_ok());
    EXPECT_TRUE(reg.add_window(win2).is_ok());
    EXPECT_TRUE(reg.add_window(win3).is_ok());

    auto list = reg.windows();
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0]->id(), 1u);
    EXPECT_EQ(list[1]->id(), 2u);
    EXPECT_EQ(list[2]->id(), 3u);

    // Remove middle window
    EXPECT_TRUE(reg.remove_window(2).is_ok());
    list = reg.windows();
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0]->id(), 1u);
    EXPECT_EQ(list[1]->id(), 3u);
}

TEST(WindowRegistryTest, WindowsForApp) {
    WindowRegistry reg;
    auto w1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    w1->set_app_id("org.example.App1");

    auto w2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);
    w2->set_app_id("org.example.App2");

    auto w3 = std::make_shared<Window>(3, nullptr, nullptr, nullptr);
    w3->set_app_id("org.example.App1");

    static_cast<void>(reg.add_window(w1));
    static_cast<void>(reg.add_window(w2));
    static_cast<void>(reg.add_window(w3));

    auto app1_wins = reg.windows_for_app("org.example.App1");
    ASSERT_EQ(app1_wins.size(), 2u);
    EXPECT_EQ(app1_wins[0]->id(), 1u);
    EXPECT_EQ(app1_wins[1]->id(), 3u);

    auto app2_wins = reg.windows_for_app("org.example.App2");
    ASSERT_EQ(app2_wins.size(), 1u);
    EXPECT_EQ(app2_wins[0]->id(), 2u);

    auto app3_wins = reg.windows_for_app("org.example.NonExistent");
    EXPECT_TRUE(app3_wins.empty());
}

TEST(WindowRegistryTest, ActiveWindowTracking) {
    WindowRegistry reg;
    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);

    static_cast<void>(reg.add_window(win1));
    static_cast<void>(reg.add_window(win2));

    EXPECT_FALSE(reg.active_window_id().has_value());
    EXPECT_FALSE(win1->is_active());
    EXPECT_FALSE(win2->is_active());

    reg.set_active_window(1);
    ASSERT_TRUE(reg.active_window_id().has_value());
    EXPECT_EQ(*reg.active_window_id(), 1u);
    EXPECT_TRUE(win1->is_active());
    EXPECT_FALSE(win2->is_active());

    // Switch active window
    reg.set_active_window(2);
    ASSERT_TRUE(reg.active_window_id().has_value());
    EXPECT_EQ(*reg.active_window_id(), 2u);
    EXPECT_FALSE(win1->is_active());
    EXPECT_TRUE(win2->is_active());

    // Clear active window
    reg.set_active_window(std::nullopt);
    EXPECT_FALSE(reg.active_window_id().has_value());
    EXPECT_FALSE(win1->is_active());
    EXPECT_FALSE(win2->is_active());
}

TEST(WindowRegistryTest, ActiveWindowRemovalFallback) {
    WindowRegistry reg;
    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);

    static_cast<void>(reg.add_window(win1));
    static_cast<void>(reg.add_window(win2));

    reg.set_active_window(2);
    EXPECT_EQ(reg.active_window_id(), 2u);

    // When active window 2 is removed, fallback should focus remaining window 1
    EXPECT_TRUE(reg.remove_window(2).is_ok());
    ASSERT_TRUE(reg.active_window_id().has_value());
    EXPECT_EQ(*reg.active_window_id(), 1u);
    EXPECT_TRUE(win1->is_active());

    // Remove last window
    EXPECT_TRUE(reg.remove_window(1).is_ok());
    EXPECT_FALSE(reg.active_window_id().has_value());
}

TEST(WindowRegistryTest, ForEachAndClear) {
    WindowRegistry reg;
    auto win1 = std::make_shared<Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<Window>(2, nullptr, nullptr, nullptr);

    static_cast<void>(reg.add_window(win1));
    static_cast<void>(reg.add_window(win2));

    size_t iterated = 0;
    reg.for_each([&](const std::shared_ptr<Window>& w) {
        ASSERT_NE(w, nullptr);
        iterated++;
    });
    EXPECT_EQ(iterated, 2u);

    reg.clear();
    EXPECT_TRUE(reg.empty());
    EXPECT_EQ(reg.count(), 0u);
}

