#include <gtest/gtest.h>
#include "ldde/launcher/launcher_state.hpp"
#include "ldde/launcher/launch_request.hpp"
#include "ldde/launcher/launch_result.hpp"
#include "ldde/launcher/application_launcher.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"
#include "ldde/launcher/launcher_category.hpp"
#include "ldde/launcher/launcher_item.hpp"
#include "ldde/launcher/launcher_filter.hpp"
#include "ldde/launcher/launcher_search.hpp"
#include "ldde/launcher/launcher_layout.hpp"
#include "ldde/launcher/launcher_model.hpp"
#include "ldde/launcher/launcher_controller.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/application_metadata.hpp"
#include "ldde/application/desktop_entry_reader.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/display/display_policy.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
namespace core = ldde::core;
namespace config = ldde::config;
namespace display = ldde::display;
using namespace ldde::core;
using namespace ldde::launcher;
using namespace ldde::application;
using namespace ldde::display;

namespace {

ApplicationMetadata make_app(
    std::string id_str,
    std::string name,
    std::string generic_name,
    std::string exec,
    std::vector<std::string> categories = {},
    std::vector<std::string> keywords = {},
    std::string comment = "",
    std::string icon = "application-default-icon",
    bool terminal = false) {

    std::string content = "[Desktop Entry]\nType=Application\n";
    content += "Name=" + name + "\n";
    if (!generic_name.empty()) content += "GenericName=" + generic_name + "\n";
    content += "Exec=" + exec + "\n";
    if (!icon.empty()) content += "Icon=" + icon + "\n";
    if (terminal) content += "Terminal=true\n";
    if (!comment.empty()) content += "Comment=" + comment + "\n";
    if (!categories.empty()) {
        content += "Categories=";
        for (const auto& c : categories) content += c + ";";
        content += "\n";
    }
    if (!keywords.empty()) {
        content += "Keywords=";
        for (const auto& k : keywords) content += k + ";";
        content += "\n";
    }

    auto entry = DesktopEntryParser::parse(content);
    DesktopEntrySource src{"/usr/share/applications/" + id_str, DesktopEntrySourceType::System, 10};
    auto meta = ApplicationMetadata::from_desktop_entry(ApplicationId(id_str), entry.value(), src);
    return meta.value();
}

DisplayPolicy make_phone_portrait_policy() {
    DisplayInfo info;
    info.id = 1;
    info.name = "PhoneDisplay";
    info.logical_width = 360;
    info.logical_height = 800;
    info.pixel_width = 1080;
    info.pixel_height = 2400;
    info.scale = 3;
    info.orientation = Orientation::Portrait;
    return DisplayPolicy(info);
}

DisplayPolicy make_phone_landscape_policy() {
    DisplayInfo info;
    info.id = 1;
    info.name = "PhoneDisplay";
    info.logical_width = 800;
    info.logical_height = 360;
    info.pixel_width = 2400;
    info.pixel_height = 1080;
    info.scale = 3;
    info.orientation = Orientation::Landscape;
    return DisplayPolicy(info);
}

DisplayPolicy make_tablet_policy() {
    DisplayInfo info;
    info.id = 2;
    info.name = "TabletDisplay";
    info.logical_width = 1280;
    info.logical_height = 800;
    info.scale = 2;
    info.orientation = Orientation::Landscape;
    return DisplayPolicy(info);
}

} // namespace

// =============================================================================
// 1. Launcher State Machine Tests
// =============================================================================
TEST(LauncherStateTest, InitialStateIsClosed) {
    LauncherStateMachine sm;
    EXPECT_EQ(sm.state(), LauncherState::Closed);
    EXPECT_TRUE(sm.is_closed());
    EXPECT_FALSE(sm.is_open());
    EXPECT_FALSE(sm.is_active());
}

TEST(LauncherStateTest, OpenAndCloseTransitions) {
    LauncherStateMachine sm;
    ASSERT_TRUE(sm.request_open().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Opening);
    EXPECT_TRUE(sm.is_active());

    ASSERT_TRUE(sm.finish_open().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Open);
    EXPECT_TRUE(sm.is_open());

    ASSERT_TRUE(sm.request_close().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closing);

    ASSERT_TRUE(sm.finish_close().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closed);
    EXPECT_TRUE(sm.is_closed());
}

TEST(LauncherStateTest, IdempotentOpenAndClose) {
    LauncherStateMachine sm;
    EXPECT_TRUE(sm.request_close().is_ok());
    EXPECT_TRUE(sm.finish_close().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closed);

    EXPECT_TRUE(sm.request_open().is_ok());
    EXPECT_TRUE(sm.request_open().is_ok());
    EXPECT_TRUE(sm.finish_open().is_ok());
    EXPECT_TRUE(sm.finish_open().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Open);

    EXPECT_TRUE(sm.request_close().is_ok());
    EXPECT_TRUE(sm.request_close().is_ok());
    EXPECT_TRUE(sm.finish_close().is_ok());
    EXPECT_TRUE(sm.finish_close().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closed);
}

TEST(LauncherStateTest, SearchingTransitions) {
    LauncherStateMachine sm;
    EXPECT_TRUE(sm.start_searching().is_error()); // Cannot search when closed

    ASSERT_TRUE(sm.request_open().is_ok());
    ASSERT_TRUE(sm.finish_open().is_ok());

    EXPECT_TRUE(sm.start_searching().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Searching);
    EXPECT_TRUE(sm.is_open());

    EXPECT_TRUE(sm.stop_searching().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Open);
}

TEST(LauncherStateTest, LaunchingSuccessAndFailureRecovery) {
    LauncherStateMachine sm;
    ASSERT_TRUE(sm.request_open().is_ok());
    ASSERT_TRUE(sm.finish_open().is_ok());

    // Successful launch flow
    EXPECT_TRUE(sm.request_launch().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Launching);
    EXPECT_TRUE(sm.finish_launch().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closing);
    EXPECT_TRUE(sm.finish_close().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closed);

    // Failed launch flow -> remains open and usable!
    ASSERT_TRUE(sm.request_open().is_ok());
    ASSERT_TRUE(sm.finish_open().is_ok());
    EXPECT_TRUE(sm.request_launch().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Launching);
    EXPECT_TRUE(sm.fail_launch().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::LaunchFailed);
    EXPECT_TRUE(sm.is_open()); // Launcher remains open and usable

    // Can transition from LaunchFailed to Searching or Open or Close
    EXPECT_TRUE(sm.start_searching().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Searching);
    EXPECT_TRUE(sm.request_close().is_ok());
    EXPECT_TRUE(sm.finish_close().is_ok());
}

TEST(LauncherStateTest, ToggleBehavior) {
    LauncherStateMachine sm;
    EXPECT_TRUE(sm.toggle().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Open);
    EXPECT_TRUE(sm.toggle().is_ok());
    EXPECT_EQ(sm.state(), LauncherState::Closed);
}

// =============================================================================
// 2. Search & Deterministic Ranking Tests
// =============================================================================
TEST(LauncherSearchTest, DeterministicMultiTierRanking) {
    std::vector<ApplicationMetadata> apps = {
        make_app("general.desktop", "General App", "Terminal Emulator", "general"),
        make_app("xterm.desktop", "XTerm", "Terminal", "xterm"),
        make_app("term.desktop", "term", "Command Line", "term"),
        make_app("editor.desktop", "Text Editor", "Editor", "edit", {}, {"terminal", "console"}),
        make_app("calculator.desktop", "Calculator", "Math Tool", "calc", {"Utility"}, {}, "terminal based calculation")
    };

    auto results = LauncherSearch::search(apps, "term");
    ASSERT_EQ(results.size(), 5u);

    // 1. Exact name match: "term" (score 1000)
    EXPECT_EQ(results[0].application->id().value(), "term.desktop");
    EXPECT_EQ(results[0].score, 1000);

    // 2. Prefix name match: "XTerm" (word/prefix match score 700-800 or generic prefix)
    // 3. Generic name match: "Terminal Emulator" (GenericName prefix score 500)
    // 4. Keyword match: "editor.desktop" has "terminal" in keywords (score 350)
    // 5. Comment match: "calculator.desktop" has "terminal" in comment (score 100)
    EXPECT_GT(results[0].score, results[1].score);
    EXPECT_GT(results[1].score, results[4].score);
}

TEST(LauncherSearchTest, DeterministicTieBreaking) {
    std::vector<ApplicationMetadata> apps = {
        make_app("b_app.desktop", "Same Name", "", "b_app"),
        make_app("a_app.desktop", "Same Name", "", "a_app"),
        make_app("c_app.desktop", "Same Name", "", "c_app")
    };

    auto results = LauncherSearch::search(apps, "same");
    ASSERT_EQ(results.size(), 3u);

    // Tie-break should sort by ApplicationId ascending
    EXPECT_EQ(results[0].application->id().value(), "a_app.desktop");
    EXPECT_EQ(results[1].application->id().value(), "b_app.desktop");
    EXPECT_EQ(results[2].application->id().value(), "c_app.desktop");
}

TEST(LauncherSearchTest, CaseInsensitiveAndWhitespaceNormalized) {
    std::vector<ApplicationMetadata> apps = {
        make_app("files.desktop", "File Manager", "Files", "files")
    };

    auto res1 = LauncherSearch::search(apps, "  FILE   ");
    ASSERT_EQ(res1.size(), 1u);
    EXPECT_EQ(res1[0].application->id().value(), "files.desktop");

    auto res2 = LauncherSearch::search(apps, "manager");
    ASSERT_EQ(res2.size(), 1u);
}

TEST(LauncherSearchTest, EmptyQueryReturnsAllSortedAlphabetically) {
    std::vector<ApplicationMetadata> apps = {
        make_app("zebra.desktop", "Zebra", "", "zebra"),
        make_app("apple.desktop", "Apple", "", "apple"),
        make_app("banana.desktop", "Banana", "", "banana")
    };

    auto results = LauncherSearch::search(apps, "");
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].application->name(), "Apple");
    EXPECT_EQ(results[1].application->name(), "Banana");
    EXPECT_EQ(results[2].application->name(), "Zebra");
}

TEST(LauncherSearchTest, NonMatchingQueryReturnsEmpty) {
    std::vector<ApplicationMetadata> apps = {
        make_app("calc.desktop", "Calculator", "", "calc")
    };

    auto results = LauncherSearch::search(apps, "xyznonexistent");
    EXPECT_TRUE(results.empty());
}

// =============================================================================
// 3. Category Filtering Tests
// =============================================================================
TEST(LauncherCategoryTest, CanonicalizationAndStandardCategories) {
    EXPECT_EQ(LauncherCategory::canonicalize_category("WebBrowser"), "Network");
    EXPECT_EQ(LauncherCategory::canonicalize_category("Audio"), "AudioVideo");
    EXPECT_EQ(LauncherCategory::canonicalize_category("IDE"), "Development");
    EXPECT_EQ(LauncherCategory::canonicalize_category("TextEditor"), "Utility");
    EXPECT_EQ(LauncherCategory::canonicalize_category("Spreadsheet"), "Office");

    auto std_cats = LauncherCategory::standard_category_ids();
    EXPECT_FALSE(std_cats.empty());
    EXPECT_EQ(std_cats[0], "All");
}

TEST(LauncherCategoryTest, DynamicCategoryCountsAndFiltering) {
    std::vector<ApplicationMetadata> apps = {
        make_app("code.desktop", "Code Editor", "", "code", {"Development", "Utility"}),
        make_app("browser.desktop", "Browser", "", "browse", {"Network"}),
        make_app("music.desktop", "Music Player", "", "music", {"AudioVideo"}),
        make_app("notes.desktop", "Notes", "", "notes", {"Office"}),
        make_app("custom.desktop", "Custom Tool", "", "cust", {"SpecialUnknown"})
    };

    auto cat_infos = LauncherCategory::compute_categories(apps, false);

    // Verify "All" category has count 5
    auto it_all = std::find_if(cat_infos.begin(), cat_infos.end(), [](const auto& c) { return c.id == "All"; });
    ASSERT_NE(it_all, cat_infos.end());
    EXPECT_EQ(it_all->count, 5u);

    // Multi-category: Code Editor is in Development and Utility
    EXPECT_TRUE(LauncherCategory::matches_category(apps[0], "Development"));
    EXPECT_TRUE(LauncherCategory::matches_category(apps[0], "Utility"));
    EXPECT_FALSE(LauncherCategory::matches_category(apps[0], "Network"));

    // Unknown category matches "Other"
    EXPECT_TRUE(LauncherCategory::matches_category(apps[4], "Other"));
}

TEST(LauncherCategoryTest, CombinedCategoryAndSearchQuery) {
    std::vector<ApplicationMetadata> apps = {
        make_app("terminal_dev.desktop", "Terminal Pro", "", "term-dev", {"Development"}),
        make_app("terminal_sys.desktop", "Terminal Sys", "", "term-sys", {"System"}),
        make_app("other_dev.desktop", "Compiler", "", "cc", {"Development"})
    };

    LauncherModel model;
    model.set_applications(apps);

    // Filter by Development category only
    model.set_category("Development");
    EXPECT_EQ(model.item_count(), 2u);

    // Combine with search "terminal"
    model.set_search_query("terminal");
    EXPECT_EQ(model.item_count(), 1u);
    EXPECT_EQ(model.item_at(0)->id().value(), "terminal_dev.desktop");

    // Clear search preserves category
    model.clear_search();
    EXPECT_EQ(model.item_count(), 2u);
    EXPECT_EQ(model.filter().category, "Development");

    // Clear filter resets to All
    model.clear_filter();
    EXPECT_EQ(model.item_count(), 3u);
    EXPECT_EQ(model.filter().category, "All");
}

// =============================================================================
// 4. Icon Resolution Tests
// =============================================================================
TEST(LauncherIconResolverTest, DirectFilePathAndThemeLookup) {
    LauncherIconResolver resolver("hicolor");

    // Test with direct file path that exists
    ApplicationIconReference direct_ref("/bin/sh");
    auto res_direct = resolver.resolve(direct_ref);
    ASSERT_TRUE(res_direct.has_value());
    EXPECT_EQ(*res_direct, "/bin/sh");

    // Test non-existent icon returns nullopt
    ApplicationIconReference missing_ref("totally-missing-icon-name-xyz");
    auto res_missing = resolver.resolve(missing_ref);
    EXPECT_FALSE(res_missing.has_value());

    // In-memory cache records lookup
    EXPECT_GT(resolver.cached_count(), 0u);
}

TEST(LauncherIconResolverTest, CustomSearchPathLookup) {
    std::string tmp_dir = "/tmp/ldde_test_icons_" + std::to_string(getpid());
    fs::create_directories(tmp_dir + "/hicolor/48x48/apps");
    std::string icon_file = tmp_dir + "/hicolor/48x48/apps/my-app.png";

    std::ofstream ofs(icon_file);
    ofs << "fake-png-data";
    ofs.close();

    LauncherIconResolver resolver("hicolor");
    resolver.add_search_path(tmp_dir);

    ApplicationIconReference icon_ref("my-app");
    auto res = resolver.resolve(icon_ref, 48);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(*res, icon_file);

    fs::remove_all(tmp_dir);
}

// =============================================================================
// 5. Responsive Layout Tests
// =============================================================================
TEST(LauncherLayoutTest, DynamicGridColumnsPhoneVsTablet) {
    LauncherLayout layout;

    // Phone Portrait: 360px wide -> 4 columns (or dynamically derived from available width)
    auto phone_pol = make_phone_portrait_policy();
    layout.update(phone_pol, 12, 5, 80);
    EXPECT_GE(layout.columns(), 2);
    EXPECT_LE(layout.columns(), 5);
    EXPECT_GE(layout.search_bar_rect().height, 48); // Minimum 48dp touch target
    EXPECT_GE(layout.item_width(), 48);
    EXPECT_GE(layout.item_height(), 48);

    int portrait_cols = layout.columns();

    // Phone Landscape: 800px wide -> significantly more columns
    auto land_pol = make_phone_landscape_policy();
    layout.update(land_pol, 12, 5, 80);
    EXPECT_GT(layout.columns(), portrait_cols);

    // Tablet: 1280px wide -> centered modal card
    auto tab_pol = make_tablet_policy();
    layout.update(tab_pol, 20, 5, 80);
    EXPECT_GT(layout.container_rect().x, 0); // Centered card
    EXPECT_GT(layout.container_rect().y, 0);
}

TEST(LauncherLayoutTest, ItemRectAndHitTesting) {
    LauncherLayout layout;
    auto phone_pol = make_phone_portrait_policy();
    layout.update(phone_pol, 8, 3, 80);

    // Search bar hit test
    core::Point in_search{layout.search_bar_rect().x + 10, layout.search_bar_rect().y + 10};
    auto hit_search = layout.hit_test(in_search, 0, 8, 3);
    EXPECT_EQ(hit_search.type, LauncherHitAreaType::SearchBar);

    // Clear button hit test
    core::Point in_clear{layout.clear_button_rect().x + 5, layout.clear_button_rect().y + 5};
    auto hit_clear = layout.hit_test(in_clear, 0, 8, 3);
    EXPECT_EQ(hit_clear.type, LauncherHitAreaType::ClearSearchButton);

    // Grid item hit test
    core::Rect item0 = layout.item_rect(0, 0);
    core::Point in_item0{item0.x + 10, item0.y + 10};
    auto hit_item0 = layout.hit_test(in_item0, 0, 8, 3);
    EXPECT_EQ(hit_item0.type, LauncherHitAreaType::GridItem);
    EXPECT_EQ(hit_item0.index, 0u);

    // Scrim dismiss hit test (outside container)
    core::Point out_point{-10, -10};
    auto hit_scrim = layout.hit_test(out_point, 0, 8, 3);
    EXPECT_EQ(hit_scrim.type, LauncherHitAreaType::DismissScrim);
}

TEST(LauncherLayoutTest, ScrollCalculationWhenContentOverflows) {
    LauncherLayout layout;
    auto phone_pol = make_phone_portrait_policy();
    // 50 items will definitely overflow 800px screen
    layout.update(phone_pol, 50, 4, 80);
    EXPECT_GT(layout.total_content_height(), layout.grid_rect().height);
    EXPECT_GT(layout.max_scroll_y(), 0);

    // Scrolling moves item rect upwards by scroll_y
    core::Rect r_unscrolled = layout.item_rect(10, 0);
    core::Rect r_scrolled = layout.item_rect(10, 50);
    EXPECT_EQ(r_scrolled.y, r_unscrolled.y - 50);
}

// =============================================================================
// 6. Controller & Keyboard Navigation Tests
// =============================================================================
TEST(LauncherControllerTest, KeyboardArrowAndEnterLaunch) {
    auto mock_backend = std::make_shared<MockApplicationLauncher>();
    LauncherController controller(mock_backend);

    std::vector<ApplicationMetadata> apps = {
        make_app("app1.desktop", "App A", "", "app1"),
        make_app("app2.desktop", "App B", "", "app2"),
        make_app("app3.desktop", "App C", "", "app3"),
        make_app("app4.desktop", "App D", "", "app4")
    };
    controller.model().set_applications(apps);
    auto phone_pol = make_phone_portrait_policy();
    controller.update_layout(phone_pol);

    ASSERT_TRUE(controller.open().is_ok());
    EXPECT_EQ(controller.model().selected_index(), 0u);

    // Arrow Right navigates to index 1
    EXPECT_TRUE(controller.handle_key_down(0xff53)); // Right
    EXPECT_EQ(controller.model().selected_index(), 1u);

    // Arrow Left navigates back to index 0
    EXPECT_TRUE(controller.handle_key_down(0xff51)); // Left
    EXPECT_EQ(controller.model().selected_index(), 0u);

    // Enter launches selected item
    EXPECT_TRUE(controller.handle_key_down(0xff0d)); // Enter
    EXPECT_EQ(mock_backend->launch_count(), 1u);
    EXPECT_EQ(mock_backend->last_request()->id.value(), "app1.desktop");
    EXPECT_EQ(controller.state(), LauncherState::Closed); // Closes upon launch
}

TEST(LauncherControllerTest, EscapeClearsSearchThenCloses) {
    auto mock_backend = std::make_shared<MockApplicationLauncher>();
    LauncherController controller(mock_backend);

    std::vector<ApplicationMetadata> apps = {
        make_app("app1.desktop", "App One", "", "app1")
    };
    controller.model().set_applications(apps);
    ASSERT_TRUE(controller.open().is_ok());

    // Type query "foo"
    controller.handle_key_down('f');
    controller.handle_key_down('o');
    controller.handle_key_down('o');
    EXPECT_EQ(controller.model().filter().search_query, "foo");
    EXPECT_EQ(controller.state(), LauncherState::Searching);

    // First Escape clears search query and returns to Open state
    EXPECT_TRUE(controller.handle_key_down(0xff1b)); // Esc
    EXPECT_TRUE(controller.model().filter().search_query.empty());
    EXPECT_EQ(controller.state(), LauncherState::Open);

    // Second Escape closes launcher
    EXPECT_TRUE(controller.handle_key_down(0xff1b)); // Esc
    EXPECT_EQ(controller.state(), LauncherState::Closed);
}

// =============================================================================
// 7. Touch Interaction Tests
// =============================================================================
TEST(LauncherControllerTest, TouchTapToLaunch) {
    auto mock_backend = std::make_shared<MockApplicationLauncher>();
    LauncherController controller(mock_backend);

    std::vector<ApplicationMetadata> apps = {
        make_app("app1.desktop", "App One", "", "app1"),
        make_app("app2.desktop", "App Two", "", "app2")
    };
    controller.model().set_applications(apps);
    auto phone_pol = make_phone_portrait_policy();
    controller.update_layout(phone_pol);
    ASSERT_TRUE(controller.open().is_ok());

    core::Rect item1_rect = controller.layout().item_rect(1, 0);
    int tap_x = item1_rect.x + item1_rect.width / 2;
    int tap_y = item1_rect.y + item1_rect.height / 2;

    EXPECT_TRUE(controller.handle_touch_down(tap_x, tap_y));
    EXPECT_TRUE(controller.handle_touch_up(tap_x, tap_y));

    EXPECT_EQ(mock_backend->launch_count(), 1u);
    EXPECT_EQ(mock_backend->last_request()->id.value(), "app2.desktop");
    EXPECT_EQ(controller.state(), LauncherState::Closed);
}

TEST(LauncherControllerTest, TouchScrollClamping) {
    auto mock_backend = std::make_shared<MockApplicationLauncher>();
    LauncherController controller(mock_backend);

    std::vector<ApplicationMetadata> apps;
    for (int i = 0; i < 40; ++i) {
        apps.push_back(make_app("app" + std::to_string(i) + ".desktop", "App " + std::to_string(i), "", "app"));
    }
    controller.model().set_applications(apps);
    auto phone_pol = make_phone_portrait_policy();
    controller.update_layout(phone_pol);
    ASSERT_TRUE(controller.open().is_ok());

    int start_scroll = controller.scroll_y();
    EXPECT_EQ(start_scroll, 0);

    // Touch down in grid and drag up
    int gx = controller.layout().grid_rect().x + 50;
    int gy = controller.layout().grid_rect().y + 200;
    controller.handle_touch_down(gx, gy);
    controller.handle_touch_motion(gx, gy - 100); // Drag up 100px

    EXPECT_EQ(controller.scroll_y(), 100);

    // Drag past bottom: clamped to max_scroll_y
    controller.handle_touch_motion(gx, gy - 5000);
    EXPECT_EQ(controller.scroll_y(), controller.layout().max_scroll_y());

    controller.handle_touch_up(gx, gy - 5000);
    EXPECT_FALSE(mock_backend->launch_count() > 0); // Did not launch on scroll!
}

// =============================================================================
// 8. Launch Failure UX Tests
// =============================================================================
TEST(LauncherControllerTest, LaunchFailureMaintainsUsableLauncher) {
    auto mock_backend = std::make_shared<MockApplicationLauncher>();
    mock_backend->set_next_result(LaunchResult::failure(LaunchStatus::ExecutionFailed, "Process crashed"));
    LauncherController controller(mock_backend);

    std::vector<ApplicationMetadata> apps = {
        make_app("bad_app.desktop", "Failing App", "", "fail")
    };
    controller.model().set_applications(apps);
    ASSERT_TRUE(controller.open().is_ok());

    auto result = controller.launch_item(0);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, LaunchStatus::ExecutionFailed);
    EXPECT_EQ(controller.state(), LauncherState::LaunchFailed);
    EXPECT_TRUE(controller.is_open());
    EXPECT_FALSE(controller.last_error_message().empty());

    // User can still search or close or retry
    controller.handle_key_down('a');
    EXPECT_EQ(controller.state(), LauncherState::Searching);
    EXPECT_TRUE(controller.close().is_ok());
    EXPECT_EQ(controller.state(), LauncherState::Closed);
}

// =============================================================================
// 9. Master Facade & Catalog Synchronization Tests
// =============================================================================
TEST(LauncherFacadeTest, FullLifecycleAndCatalogSync) {
    ApplicationCatalog catalog;
    config::Config config;
    auto mock_backend = std::make_shared<MockApplicationLauncher>();

    Launcher launcher(mock_backend);
    auto phone_pol = make_phone_portrait_policy();

    ASSERT_TRUE(launcher.initialize(catalog, phone_pol, config, mock_backend).is_ok());
    EXPECT_EQ(launcher.model().item_count(), 0u);

    // Dynamically add applications to catalog
    std::vector<ApplicationMetadata> new_apps = {
        make_app("calc.desktop", "Calculator", "", "calc"),
        make_app("term.desktop", "Terminal", "", "term")
    };
    catalog.update_applications(new_apps);

    // Launcher immediately receives catalog update!
    EXPECT_EQ(launcher.model().item_count(), 2u);

    // Open launcher
    ASSERT_TRUE(launcher.open().is_ok());
    EXPECT_TRUE(launcher.is_open());

    // Type "term"
    launcher.handle_key('t');
    launcher.handle_key('e');
    launcher.handle_key('r');
    launcher.handle_key('m');
    EXPECT_EQ(launcher.model().item_count(), 1u);
    EXPECT_EQ(launcher.model().item_at(0)->name(), "Terminal");

    // Launch via Enter
    launcher.handle_key(0xff0d); // Enter
    EXPECT_EQ(mock_backend->launch_count(), 1u);
    EXPECT_EQ(mock_backend->last_request()->name, "Terminal");
    EXPECT_FALSE(launcher.is_open());

    launcher.shutdown();
    EXPECT_FALSE(launcher.is_open());
}
