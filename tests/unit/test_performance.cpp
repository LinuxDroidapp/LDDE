#include <gtest/gtest.h>
#include "ldde/shell/shell.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_stacking.hpp"
#include "ldde/launcher/launcher_search.hpp"
#include "ldde/notification/notification_store.hpp"
#include "ldde/settings/settings_store.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include <filesystem>

using namespace ldde;

static application::ApplicationMetadata create_test_app(const std::string& name,
                                                        const std::string& app_id,
                                                        const std::string& keywords) {
    std::string content = "[Desktop Entry]\nType=Application\n";
    content += "Name=" + name + "\n";
    content += "Exec=" + app_id + "\n";
    content += "Categories=Utility;\n";
    content += "Keywords=" + keywords + "\n";

    auto parsed = application::DesktopEntryParser::parse(content);
    application::DesktopEntrySource src(
        std::filesystem::path("/usr/share/applications/" + app_id + ".desktop"),
        application::DesktopEntrySourceType::System, 10);
    auto meta_res = application::ApplicationMetadata::from_desktop_entry(
        application::ApplicationId(app_id),
        parsed.value(),
        src
    );
    return meta_res.value();
}

TEST(PerformanceTest, WindowRegistryFastLookups) {
    window::WindowRegistry reg;

    // Use distinct non-null dummy pointers
    auto* s1 = reinterpret_cast<wl_surface*>(0x1000);
    auto* s2 = reinterpret_cast<wl_surface*>(0x2000);
    auto* t1 = reinterpret_cast<xdg_toplevel*>(0x3000);
    auto* t2 = reinterpret_cast<xdg_toplevel*>(0x4000);

    auto win1 = std::make_shared<window::Window>(101, s1, nullptr, t1);
    win1->set_title("App 1");
    auto win2 = std::make_shared<window::Window>(102, s2, nullptr, t2);
    win2->set_title("App 2");

    EXPECT_TRUE(reg.add_window(win1).is_ok());
    EXPECT_TRUE(reg.add_window(win2).is_ok());

    // Verify O(1) lookup by surface
    auto found_s1 = reg.find_by_surface(s1);
    ASSERT_NE(found_s1, nullptr);
    EXPECT_EQ(found_s1->id(), 101u);

    auto found_s2 = reg.find_by_surface(s2);
    ASSERT_NE(found_s2, nullptr);
    EXPECT_EQ(found_s2->id(), 102u);

    // Verify O(1) lookup by toplevel
    auto found_t1 = reg.find_by_toplevel(t1);
    ASSERT_NE(found_t1, nullptr);
    EXPECT_EQ(found_t1->id(), 101u);

    auto found_t2 = reg.find_by_toplevel(t2);
    ASSERT_NE(found_t2, nullptr);
    EXPECT_EQ(found_t2->id(), 102u);

    // Unknown pointers return nullptr
    auto* s_unknown = reinterpret_cast<wl_surface*>(0x9999);
    EXPECT_EQ(reg.find_by_surface(s_unknown), nullptr);
    auto* t_unknown = reinterpret_cast<xdg_toplevel*>(0x9999);
    EXPECT_EQ(reg.find_by_toplevel(t_unknown), nullptr);

    // After remove, lookups must return nullptr
    EXPECT_TRUE(reg.remove_window(101).is_ok());
    EXPECT_EQ(reg.find_by_surface(s1), nullptr);
    EXPECT_EQ(reg.find_by_toplevel(t1), nullptr);

    // Second window remains intact
    EXPECT_EQ(reg.find_by_surface(s2), win2);
}

TEST(PerformanceTest, WindowStackingCacheReused) {
    window::WindowRegistry reg;
    window::WindowStacking stacking;

    auto win1 = std::make_shared<window::Window>(1, nullptr, nullptr, nullptr);
    auto win2 = std::make_shared<window::Window>(2, nullptr, nullptr, nullptr);
    auto win3 = std::make_shared<window::Window>(3, nullptr, nullptr, nullptr);

    win1->set_state(window::WindowState::Normal);
    win1->set_visible(true);
    win2->set_state(window::WindowState::Normal);
    win2->set_visible(true);
    win3->set_state(window::WindowState::Minimized);
    win3->set_visible(true);

    EXPECT_TRUE(reg.add_window(win1).is_ok());
    EXPECT_TRUE(reg.add_window(win2).is_ok());
    EXPECT_TRUE(reg.add_window(win3).is_ok());

    stacking.add(1);
    stacking.add(2);
    stacking.add(3);

    const auto& v1 = stacking.visible_stack(reg);
    EXPECT_EQ(v1.size(), 2u);

    // Unminimize win3
    win3->set_state(window::WindowState::Normal);
    const auto& v2 = stacking.visible_stack(reg);
    EXPECT_EQ(v2.size(), 3u);

    // Reorder
    stacking.raise(1);
    const auto& v3 = stacking.visible_stack(reg);
    ASSERT_EQ(v3.size(), 3u);
    EXPECT_EQ(v3.back(), 1u);
}

TEST(PerformanceTest, LauncherSearchZeroAllocationPrecision) {
    std::vector<application::ApplicationMetadata> apps;
    apps.push_back(create_test_app("Terminal", "org.example.term", "console;shell;bash;"));
    apps.push_back(create_test_app("File Manager", "org.example.files", "explorer;browser;directory;"));
    apps.push_back(create_test_app("Settings", "org.example.settings", "preferences;configuration;display;"));

    // Empty search returns all
    auto res_empty = launcher::LauncherSearch::search(apps, "");
    EXPECT_EQ(res_empty.size(), 3u);

    // Prefix match on Name (case-insensitive)
    auto res_term = launcher::LauncherSearch::search(apps, "term");
    ASSERT_EQ(res_term.size(), 1u);
    EXPECT_EQ(res_term[0].application->name(), "Terminal");

    // Word start match in Name ("Manager" in "File Manager")
    auto res_mgr = launcher::LauncherSearch::search(apps, "man");
    ASSERT_EQ(res_mgr.size(), 1u);
    EXPECT_EQ(res_mgr[0].application->name(), "File Manager");

    // Keyword match ("bash")
    auto res_kw = launcher::LauncherSearch::search(apps, "bash");
    ASSERT_EQ(res_kw.size(), 1u);
    EXPECT_EQ(res_kw[0].application->name(), "Terminal");

    // Case insensitive with leading/trailing spaces
    auto res_spaces = launcher::LauncherSearch::search(apps, "  SeTtInGs  ");
    ASSERT_EQ(res_spaces.size(), 1u);
    EXPECT_EQ(res_spaces[0].application->name(), "Settings");

    // Non-matching query
    auto res_none = launcher::LauncherSearch::search(apps, "xyznonexistent");
    EXPECT_TRUE(res_none.empty());
}

TEST(PerformanceTest, ShellDirtyFlagsBitmaskOperations) {
    using shell::ShellDirtyFlag;

    auto flags = ShellDirtyFlag::Desktop | ShellDirtyFlag::StatusBar;
    EXPECT_TRUE((flags & ShellDirtyFlag::Desktop) != ShellDirtyFlag::None);
    EXPECT_TRUE((flags & ShellDirtyFlag::StatusBar) != ShellDirtyFlag::None);
    EXPECT_TRUE((flags & ShellDirtyFlag::Dock) == ShellDirtyFlag::None);
    EXPECT_TRUE((flags & ShellDirtyFlag::Overlay) == ShellDirtyFlag::None);

    flags |= ShellDirtyFlag::Dock;
    EXPECT_TRUE((flags & ShellDirtyFlag::Dock) != ShellDirtyFlag::None);

    // Clear Desktop
    flags = static_cast<ShellDirtyFlag>(
        static_cast<uint32_t>(flags) & ~static_cast<uint32_t>(ShellDirtyFlag::Desktop)
    );
    EXPECT_TRUE((flags & ShellDirtyFlag::Desktop) == ShellDirtyFlag::None);
    EXPECT_TRUE((flags & ShellDirtyFlag::StatusBar) != ShellDirtyFlag::None);
    EXPECT_TRUE((flags & ShellDirtyFlag::Dock) != ShellDirtyFlag::None);
}

TEST(PerformanceTest, NotificationStoreFloodBoundsUnderStress) {
    constexpr size_t kMaxHistory = 50;
    constexpr size_t kMaxActivePerApp = 10;
    notification::NotificationStore store(kMaxHistory, kMaxActivePerApp);

    // Flood with 100 notifications for a single application
    for (uint32_t i = 1; i <= 100; ++i) {
        notification::Notification n(
            i,
            "app.flood",
            "Stress Notification " + std::to_string(i),
            "Notification body text to test bounded memory growth under stress",
            "dialog-information",
            notification::NotificationUrgency::Normal,
            5000,
            0
        );
        store.add_or_replace(n);
    }

    // Active count for this application must strictly not exceed kMaxActivePerApp
    EXPECT_LE(store.active_count(), kMaxActivePerApp);

    // Close all remaining notifications to push into history
    for (uint32_t i = 1; i <= 100; ++i) {
        store.close(i);
    }

    // History count must strictly not exceed kMaxHistory
    EXPECT_LE(store.history_count(), kMaxHistory);
}

TEST(PerformanceTest, SettingsStoreTransactionRollbackAndCommit) {
    config::Config cfg;
    settings::SettingsSchema schema = settings::SettingsSchema::create_default_schema();
    settings::SettingsStore store(cfg, schema);

    int change_notifications = 0;
    store.on_setting_changed([&](const std::string&, const settings::SettingsValue&) {
        ++change_notifications;
    });

    // Test rollback: changes must NOT take effect and notifications must NOT fire
    store.begin_transaction();
    EXPECT_TRUE(store.in_transaction());
    store.set("dock.enabled", settings::SettingsValue(false), false);
    store.set("dock.item_size", settings::SettingsValue(static_cast<int64_t>(72)), false);
    store.rollback();

    EXPECT_FALSE(store.in_transaction());
    EXPECT_EQ(change_notifications, 0);
    EXPECT_EQ(store.get_or("dock.enabled", settings::SettingsValue(true)).as_bool(), true);

    // Test commit: changes must take effect and fire notifications
    store.begin_transaction();
    store.set("dock.enabled", settings::SettingsValue(false), false);
    store.set("dock.item_size", settings::SettingsValue(static_cast<int64_t>(72)), false);
    EXPECT_TRUE(store.commit(false).is_ok());

    EXPECT_FALSE(store.in_transaction());
    EXPECT_EQ(change_notifications, 2);
    EXPECT_EQ(store.get_or("dock.enabled", settings::SettingsValue(true)).as_bool(), false);
    EXPECT_EQ(store.get_or("dock.item_size", settings::SettingsValue(static_cast<int64_t>(48))).as_int(), 72);
}
