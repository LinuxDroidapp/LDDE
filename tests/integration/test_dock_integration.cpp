#include <gtest/gtest.h>
#include "ldde/core/application.hpp"
#include "ldde/dock/dock.hpp"
#include "ldde/launcher/application_launcher.hpp"
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
namespace core = ldde::core;
using namespace ldde::core;
using namespace ldde::dock;
using namespace ldde::application;
using namespace ldde::window;
using namespace ldde::display;
using namespace ldde::shell;
using namespace ldde::launcher;
using ldde::config::Config;

class DummyWMBackend : public WindowManagementBackend {
public:
    Status activate(WindowId) override { return Status::ok(); }
    Status deactivate(WindowId) override { return Status::ok(); }
    Status close(WindowId) override { return Status::ok(); }
    Status set_geometry(WindowId, const Rect&) override { return Status::ok(); }
    Status set_maximized(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_fullscreen(WindowId, bool, const Size&) override { return Status::ok(); }
    Status set_minimized(WindowId, bool) override { return Status::ok(); }
    Status start_move(WindowId, uint32_t) override { return Status::ok(); }
    Status start_resize(WindowId, ResizeEdge, uint32_t) override { return Status::ok(); }
};

class DockIntegrationTest : public ::testing::Test {
protected:
    std::string test_dir_;

    void SetUp() override {
        char temp[] = "/tmp/ldde_dock_int_XXXXXX";
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

TEST_F(DockIntegrationTest, PinnedAppToLaunchToRunningIndicator) {
    create_desktop_file("test_echo.desktop", "Test Echo App", "/bin/echo dock_test");

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    policy.add_search_directory(test_dir_ + "/applications", DesktopEntrySourceType::Custom, 10);
    ApplicationDiscovery discovery(catalog, policy);
    EXPECT_TRUE(discovery.scan_and_refresh().is_ok());

    WindowRegistry registry;
    WindowTracker tracker;
    DisplayManager display_mgr;
    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy disp_policy(disp);

    WindowManager wm(registry, tracker, display_mgr, std::make_unique<DummyWMBackend>());
    auto real_launcher = std::make_shared<LinuxSessionApplicationLauncher>();
    Launcher launcher(real_launcher);

    Config config;
    config.load_defaults();
    config.set("dock", "pinned", "test_echo.desktop");

    Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, disp_policy, config, real_launcher).is_ok());
    EXPECT_EQ(dock.model().item_count(), 1u);
    EXPECT_FALSE(dock.model().item_at(0)->is_running());

    // Launch application shortcut
    dock.controller().activate_item(0);

    // Simulate genuine Wayland client window connecting
    auto win = std::make_shared<Window>(101, nullptr, nullptr, nullptr);
    win->set_app_id("test_echo.desktop");
    win->set_title("Test Echo App Window");
    registry.add_window(win);

    // Dock running indicator turns ON
    EXPECT_TRUE(dock.model().item_at(0)->is_running());
    EXPECT_EQ(dock.model().item_at(0)->window_count(), 1u);

    // Window active state
    wm.activate(101);
    dock.model().rebuild_items();
    EXPECT_TRUE(dock.model().item_at(0)->is_active());

    // Window minimized state
    wm.minimize(101);
    dock.model().rebuild_items();
    EXPECT_TRUE(dock.model().item_at(0)->is_minimized());

    // Remove window (application exit)
    registry.remove_window(101);
    EXPECT_FALSE(dock.model().item_at(0)->is_running());
    EXPECT_FALSE(dock.model().item_at(0)->is_active());
}

TEST_F(DockIntegrationTest, UnpinnedAppLifecycleInDock) {
    ApplicationCatalog catalog;
    WindowRegistry registry;
    DisplayManager display_mgr;
    DisplayInfo disp;
    disp.width = 720;
    disp.height = 1280;
    DisplayPolicy disp_policy(disp);
    WindowTracker tracker;
    WindowManager wm(registry, tracker, display_mgr, std::make_unique<DummyWMBackend>());
    Launcher launcher;
    Config config;
    config.load_defaults();

    Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, disp_policy, config).is_ok());
    EXPECT_EQ(dock.model().item_count(), 0u);

    // 1. Client connects without pinned entry
    auto win1 = std::make_shared<Window>(301, nullptr, nullptr, nullptr);
    win1->set_app_id("calculator");
    win1->set_title("Calculator");
    registry.add_window(win1);

    // Automatically added to dock as unpinned running application
    EXPECT_EQ(dock.model().item_count(), 1u);
    EXPECT_FALSE(dock.model().item_at(0)->is_pinned());
    EXPECT_TRUE(dock.model().item_at(0)->is_running());
    EXPECT_EQ(dock.model().item_at(0)->window_count(), 1u);

    // 2. Client opens second window
    auto win2 = std::make_shared<Window>(302, nullptr, nullptr, nullptr);
    win2->set_app_id("calculator");
    win2->set_title("Calculator History");
    registry.add_window(win2);

    EXPECT_EQ(dock.model().item_count(), 1u);
    EXPECT_EQ(dock.model().item_at(0)->window_count(), 2u);

    // 3. Client closes one window
    registry.remove_window(301);
    EXPECT_EQ(dock.model().item_count(), 1u);
    EXPECT_EQ(dock.model().item_at(0)->window_count(), 1u);

    // 4. Client closes final window -> Removed from dock!
    registry.remove_window(302);
    EXPECT_EQ(dock.model().item_count(), 0u);
}

TEST_F(DockIntegrationTest, ResponsiveDisplayOrientationAdaptation) {
    ApplicationCatalog catalog;
    WindowRegistry registry;
    DisplayManager display_mgr;
    Config config;
    config.load_defaults();
    config.set("dock", "pinned", "app1.desktop,app2.desktop,app3.desktop");

    DisplayInfo portrait_disp;
    portrait_disp.width = 360;
    portrait_disp.height = 800;
    portrait_disp.logical_width = 360;
    portrait_disp.logical_height = 800;
    DisplayPolicy portrait_policy(portrait_disp);

    WindowTracker tracker;
    WindowManager wm(registry, tracker, display_mgr, std::make_unique<DummyWMBackend>());
    Launcher launcher;

    Dock dock;
    ASSERT_TRUE(dock.initialize(catalog, registry, wm, launcher, portrait_policy, config).is_ok());

    std::vector<uint8_t> mem_portrait(324 * 56 * 4, 0);
    ShmBuffer buf_portrait(324, 56, 324 * 4, mem_portrait.size(), -1, mem_portrait.data(), nullptr);
    ShellTheme theme;
    DesignTokens tokens_portrait = DesignTokens::create_scaled(1.0);
    dock.render(buf_portrait, theme, tokens_portrait);

    core::Rect portrait_launcher_rect = dock.layout().launcher_button_rect();
    EXPECT_GT(portrait_launcher_rect.width, 0);

    // Switch to Landscape orientation
    DisplayInfo landscape_disp;
    landscape_disp.width = 800;
    landscape_disp.height = 360;
    landscape_disp.logical_width = 800;
    landscape_disp.logical_height = 360;
    DisplayPolicy landscape_policy(landscape_disp);

    dock.update_display_policy(landscape_policy);
    std::vector<uint8_t> mem_landscape(480 * 56 * 4, 0);
    ShmBuffer buf_landscape(480, 56, 480 * 4, mem_landscape.size(), -1, mem_landscape.data(), nullptr);
    DesignTokens tokens_landscape = DesignTokens::create_scaled(1.0);
    dock.render(buf_landscape, theme, tokens_landscape);

    core::Rect landscape_launcher_rect = dock.layout().launcher_button_rect();
    EXPECT_GT(landscape_launcher_rect.width, 0);
    EXPECT_EQ(dock.model().item_count(), 3u);
}
