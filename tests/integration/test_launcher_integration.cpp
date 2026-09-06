#include <gtest/gtest.h>
#include "ldde/core/application.hpp"
#include "ldde/launcher/launcher.hpp"
#include "ldde/launcher/application_launcher.hpp"
#include "ldde/application/application_discovery.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/display/display_policy.hpp"

#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;
namespace core = ldde::core;
namespace config = ldde::config;
namespace display = ldde::display;
using namespace ldde::core;
using namespace ldde::launcher;
using namespace ldde::application;
using namespace ldde::display;

class LauncherIntegrationTest : public ::testing::Test {
protected:
    std::string test_dir_;

    void SetUp() override {
        char temp[] = "/tmp/ldde_launcher_int_XXXXXX";
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

    void create_desktop_file(const std::string& filename, const std::string& name, const std::string& exec, const std::string& cat = "") {
        std::ofstream ofs(test_dir_ + "/applications/" + filename);
        ofs << "[Desktop Entry]\n";
        ofs << "Type=Application\n";
        ofs << "Name=" << name << "\n";
        ofs << "Exec=" << exec << "\n";
        if (!cat.empty()) {
            ofs << "Categories=" << cat << ";\n";
        }
    }
};

TEST_F(LauncherIntegrationTest, DiscoveryToLauncherPresentationAndLaunch) {
    create_desktop_file("test_true.desktop", "Test True", "/bin/true", "Utility");
    create_desktop_file("test_echo.desktop", "Test Echo", "/bin/echo hello", "Development");

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    policy.add_search_directory(test_dir_ + "/applications", DesktopEntrySourceType::Custom, 10);
    ApplicationDiscovery discovery(catalog, policy);

    ASSERT_TRUE(discovery.scan_and_refresh().is_ok());
    EXPECT_EQ(catalog.visible_count(), 2u);

    DisplayInfo disp;
    disp.id = 1;
    disp.logical_width = 360;
    disp.logical_height = 800;
    disp.orientation = Orientation::Portrait;
    DisplayPolicy disp_pol(disp);

    config::Config cfg;
    auto real_backend = std::make_shared<LinuxSessionApplicationLauncher>();
    Launcher launcher(real_backend);

    ASSERT_TRUE(launcher.initialize(catalog, disp_pol, cfg, real_backend).is_ok());
    EXPECT_EQ(launcher.model().item_count(), 2u);

    // Open launcher
    ASSERT_TRUE(launcher.open().is_ok());
    EXPECT_TRUE(launcher.is_open());

    // Search for "echo"
    launcher.handle_key('e');
    launcher.handle_key('c');
    launcher.handle_key('h');
    launcher.handle_key('o');
    EXPECT_EQ(launcher.model().item_count(), 1u);
    EXPECT_EQ(launcher.model().item_at(0)->name(), "Test Echo");

    // Launch via real Linux session launcher
    auto launch_res = launcher.controller().launch_selected();
    EXPECT_TRUE(launch_res.is_success());
    EXPECT_GT(launch_res.pid, 0);

    // Reap child
    int status = 0;
    waitpid(launch_res.pid, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));

    // Launcher automatically closed
    EXPECT_FALSE(launcher.is_open());
}

TEST_F(LauncherIntegrationTest, RealLinuxSessionLauncherExecutionAndFailure) {
    LinuxSessionApplicationLauncher launcher;

    // 1. Success launch: /bin/true
    LaunchRequest req_true;
    req_true.executable = "/bin/true";
    req_true.name = "True";
    auto res_true = launcher.launch(req_true);
    EXPECT_TRUE(res_true.is_success());
    EXPECT_GT(res_true.pid, 0);
    waitpid(res_true.pid, nullptr, 0);

    // 2. Binary in PATH: echo
    LaunchRequest req_echo;
    req_echo.executable = "echo";
    req_echo.arguments = {"hello", "world"};
    req_echo.name = "Echo";
    auto res_echo = launcher.launch(req_echo);
    EXPECT_TRUE(res_echo.is_success());
    EXPECT_GT(res_echo.pid, 0);
    waitpid(res_echo.pid, nullptr, 0);

    // 3. Not found binary
    LaunchRequest req_bad;
    req_bad.executable = "/bin/non_existent_binary_xyz_123";
    req_bad.name = "NonExistent";
    auto res_bad = launcher.launch(req_bad);
    EXPECT_FALSE(res_bad.is_success());
    EXPECT_EQ(res_bad.status, LaunchStatus::NotFound);

    // 4. Empty executable
    LaunchRequest req_empty;
    auto res_empty = launcher.launch(req_empty);
    EXPECT_FALSE(res_empty.is_success());
    EXPECT_EQ(res_empty.status, LaunchStatus::InvalidMetadata);
}

TEST_F(LauncherIntegrationTest, ResponsiveDisplayOrientationAdaptation) {
    create_desktop_file("app1.desktop", "App 1", "/bin/true");
    create_desktop_file("app2.desktop", "App 2", "/bin/true");
    create_desktop_file("app3.desktop", "App 3", "/bin/true");
    create_desktop_file("app4.desktop", "App 4", "/bin/true");

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    policy.add_search_directory(test_dir_ + "/applications", DesktopEntrySourceType::Custom, 10);
    ApplicationDiscovery discovery(catalog, policy);
    ASSERT_TRUE(discovery.scan_and_refresh().is_ok());

    DisplayInfo portrait_info;
    portrait_info.id = 1;
    portrait_info.logical_width = 360;
    portrait_info.logical_height = 800;
    portrait_info.orientation = Orientation::Portrait;
    DisplayPolicy port_pol(portrait_info);

    Launcher launcher(std::make_shared<MockApplicationLauncher>());
    config::Config cfg;
    launcher.initialize(catalog, port_pol, cfg);
    launcher.open();

    int portrait_columns = launcher.layout().columns();
    EXPECT_GE(portrait_columns, 2);

    // Rotate to landscape
    DisplayInfo land_info;
    land_info.id = 1;
    land_info.logical_width = 800;
    land_info.logical_height = 360;
    land_info.orientation = Orientation::Landscape;
    DisplayPolicy land_pol(land_info);

    launcher.update_display_policy(land_pol);
    int landscape_columns = launcher.layout().columns();
    EXPECT_GT(landscape_columns, portrait_columns);
}
