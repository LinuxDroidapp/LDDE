#include <gtest/gtest.h>
#include "ldde/core/application.hpp"
#include "ldde/switcher/switcher.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/application_discovery.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/config/config.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/window/window_management_backend.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace ldde::core;
using namespace ldde::switcher;
using namespace ldde::application;
using namespace ldde::window;
using namespace ldde::display;
using namespace ldde::shell;
using ldde::config::Config;

namespace {

class DummyWMBackend : public WindowManagementBackend {
public:
    WindowId last_activated = kInvalidWindowId;
    WindowId last_restored = kInvalidWindowId;

    Status activate(WindowId id) override {
        last_activated = id;
        return Status::ok();
    }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId) override { return Status::ok(); }
    Status set_geometry(WindowId, const Rect&) override { return Status::ok(); }
    Status set_maximized(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_fullscreen(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_minimized(WindowId id, bool minimized) override {
        if (!minimized) {
            last_restored = id;
        }
        return Status::ok();
    }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

std::shared_ptr<Window> create_window(WindowId id, std::string_view app_id, std::string_view title) {
    auto win = std::make_shared<Window>(id, nullptr, nullptr, nullptr);
    win->set_app_id(app_id);
    win->set_title(title);
    return win;
}

} // namespace

class SwitcherIntegrationTest : public ::testing::Test {
protected:
    std::string test_dir_;

    void SetUp() override {
        char temp[] = "/tmp/ldde_switcher_int_XXXXXX";
        char* d = mkdtemp(temp);
        ASSERT_NE(d, nullptr);
        test_dir_ = d;
        fs::create_directories(test_dir_ + "/applications");
    }

    void TearDown() override {
        if (!test_dir_.empty()) {
            fs::remove_all(test_dir_);
        }
    }

    void create_desktop_file(const std::string& filename, const std::string& name, const std::string& exec) {
        std::ofstream ofs(test_dir_ + "/applications/" + filename);
        ofs << "[Desktop Entry]\n";
        ofs << "Type=Application\n";
        ofs << "Name=" << name << "\n";
        ofs << "Exec=" << exec << "\n";
    }
};

TEST_F(SwitcherIntegrationTest, DiscoveryToRunningAppsAndMRUSwitching) {
    create_desktop_file("terminal.desktop", "Terminal App", "/bin/sh");
    create_desktop_file("editor.desktop", "Code Editor", "/usr/bin/gedit");
    create_desktop_file("browser.desktop", "Web Browser", "/usr/bin/firefox");

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy disc_policy;
    disc_policy.add_search_directory(test_dir_ + "/applications", DesktopEntrySourceType::Custom, 10);
    ApplicationDiscovery discovery(catalog, disc_policy);
    EXPECT_TRUE(discovery.scan_and_refresh().is_ok());

    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy disp_policy(disp);

    auto backend = std::make_unique<DummyWMBackend>();
    auto* raw_backend = backend.get();
    WindowManager wm(registry, tracker, display_mgr, std::move(backend));

    // Register windows for all 3 apps
    auto win_term = create_window(101, "terminal.desktop", "Terminal 1");
    auto win_edit = create_window(102, "editor.desktop", "Main.cpp - Editor");
    auto win_web  = create_window(103, "browser.desktop", "LDDE Project - Firefox");

    ASSERT_TRUE(registry.add_window(win_term).is_ok());
    ASSERT_TRUE(registry.add_window(win_edit).is_ok());
    ASSERT_TRUE(registry.add_window(win_web).is_ok());

    Config config;
    config.load_defaults();

    Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, disp_policy, config).is_ok());

    // Sequence of focus events: Terminal -> Editor -> Browser
    registry.set_active_window(101);
    registry.set_active_window(102);
    registry.set_active_window(103);

    // Open switcher
    EXPECT_TRUE(switcher.open().is_ok());
    EXPECT_TRUE(switcher.is_open());
    EXPECT_EQ(switcher.model().item_count(), 3u);

    // MRU order verification:
    // 0: Browser (current active)
    // 1: Editor (most recently focused before browser -> highlighted by default!)
    // 2: Terminal
    EXPECT_EQ(switcher.model().item_at(0)->primary_window_id(), 103u);
    EXPECT_TRUE(switcher.model().item_at(0)->is_current());

    EXPECT_EQ(switcher.model().item_at(1)->primary_window_id(), 102u);
    EXPECT_EQ(switcher.controller().selected_index(), 1u); // Default selection for fast Alt+Tab!

    EXPECT_EQ(switcher.model().item_at(2)->primary_window_id(), 101u);

    // Activate selected item (Editor)
    EXPECT_TRUE(switcher.controller().activate_selected().is_ok());
    EXPECT_FALSE(switcher.is_open());
    EXPECT_EQ(raw_backend->last_activated, 102u);

    switcher.shutdown();
}

TEST_F(SwitcherIntegrationTest, MinimizedAppRestoreAndSwitch) {
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy disp_policy(disp);

    auto backend = std::make_unique<DummyWMBackend>();
    auto* raw_backend = backend.get();
    WindowManager wm(registry, tracker, display_mgr, std::move(backend));

    auto win = create_window(201, "calculator", "Calculator");
    win->set_state(WindowState::Minimized);
    ASSERT_TRUE(registry.add_window(win).is_ok());

    Config config;
    config.load_defaults();

    Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, disp_policy, config).is_ok());

    EXPECT_TRUE(switcher.open().is_ok());
    EXPECT_EQ(switcher.model().item_count(), 1u);
    EXPECT_TRUE(switcher.model().item_at(0)->is_minimized());

    EXPECT_TRUE(switcher.controller().activate_selected().is_ok());
    EXPECT_EQ(raw_backend->last_restored, 201u);
    EXPECT_EQ(raw_backend->last_activated, 201u);
    EXPECT_FALSE(switcher.is_open());

    switcher.shutdown();
}

TEST_F(SwitcherIntegrationTest, WindowDestructionWhileSwitcherOpen) {
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy disp_policy(disp);

    auto backend = std::make_unique<DummyWMBackend>();
    WindowManager wm(registry, tracker, display_mgr, std::move(backend));

    auto win1 = create_window(301, "app1", "App 1");
    auto win2 = create_window(302, "app2", "App 2");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());

    Config config;
    config.load_defaults();

    Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, disp_policy, config).is_ok());

    EXPECT_TRUE(switcher.open().is_ok());
    EXPECT_EQ(switcher.model().item_count(), 2u);
    EXPECT_EQ(switcher.controller().selected_index(), 1u);

    // Destroy window 2 while switcher is open
    win2->mark_destroyed();
    ASSERT_TRUE(registry.remove_window(302).is_ok());

    // Switcher should auto-update and survive
    EXPECT_TRUE(switcher.is_open());
    EXPECT_EQ(switcher.model().item_count(), 1u);
    EXPECT_EQ(switcher.controller().selected_index(), 0u);

    // Destroy remaining window
    win1->mark_destroyed();
    ASSERT_TRUE(registry.remove_window(301).is_ok());

    EXPECT_TRUE(switcher.is_open());
    EXPECT_TRUE(switcher.model().empty());

    switcher.shutdown();
}

TEST_F(SwitcherIntegrationTest, ResponsiveDisplayOrientationAdaptation) {
    ApplicationCatalog catalog;
    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;

    DisplayInfo portrait_disp;
    portrait_disp.id = 1;
    portrait_disp.width = 1080;
    portrait_disp.height = 2400;
    portrait_disp.logical_width = 360;
    portrait_disp.logical_height = 800;
    portrait_disp.geometry = {0, 0, 360, 800};
    DisplayPolicy portrait_policy(portrait_disp);

    auto backend = std::make_unique<DummyWMBackend>();
    WindowManager wm(registry, tracker, display_mgr, std::move(backend));

    auto win1 = create_window(401, "app1", "App 1");
    auto win2 = create_window(402, "app2", "App 2");
    ASSERT_TRUE(registry.add_window(win1).is_ok());
    ASSERT_TRUE(registry.add_window(win2).is_ok());

    Config config;
    config.load_defaults();

    Switcher switcher;
    ASSERT_TRUE(switcher.initialize(catalog, registry, wm, portrait_policy, config).is_ok());
    EXPECT_TRUE(switcher.open().is_ok());

    // Portrait mode verification
    EXPECT_FALSE(switcher.layout().is_horizontal());
    EXPECT_GE(switcher.layout().card_height(), 48);

    // Rotate to landscape
    DisplayInfo landscape_disp;
    landscape_disp.id = 1;
    landscape_disp.width = 2400;
    landscape_disp.height = 1080;
    landscape_disp.logical_width = 800;
    landscape_disp.logical_height = 360;
    landscape_disp.geometry = {0, 0, 800, 360};
    DisplayPolicy landscape_policy(landscape_disp);

    switcher.update_display_policy(landscape_policy);

    // Landscape mode verification
    EXPECT_TRUE(switcher.layout().is_horizontal());
    EXPECT_GE(switcher.layout().card_width(), 48);
    EXPECT_GE(switcher.layout().card_height(), 48);
    EXPECT_TRUE(switcher.is_open());

    switcher.shutdown();
}
