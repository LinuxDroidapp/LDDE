#include <gtest/gtest.h>
#include "ldde/core/application.hpp"
#include "ldde/config/config.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/application_discovery.hpp"
#include "ldde/application/application_change_monitor.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace ldde::application;
using namespace ldde::core;
namespace config = ldde::config;

class ApplicationIntegrationTest : public ::testing::Test {
protected:
    std::string test_root_;
    std::filesystem::path sys_apps_;
    std::filesystem::path local_apps_;
    std::filesystem::path user_apps_;

    void SetUp() override {
        char template_dir[] = "/tmp/ldde_app_int_XXXXXX";
        char* dir = mkdtemp(template_dir);
        ASSERT_NE(dir, nullptr);
        test_root_ = dir;

        sys_apps_ = std::filesystem::path(test_root_) / "usr" / "share" / "applications";
        local_apps_ = std::filesystem::path(test_root_) / "usr" / "local" / "share" / "applications";
        user_apps_ = std::filesystem::path(test_root_) / "home" / "user" / ".local" / "share" / "applications";

        std::filesystem::create_directories(sys_apps_);
        std::filesystem::create_directories(local_apps_);
        std::filesystem::create_directories(user_apps_);
    }

    void TearDown() override {
        if (!test_root_.empty()) {
            std::filesystem::remove_all(test_root_);
        }
    }
};

TEST_F(ApplicationIntegrationTest, MultiTierXdgDiscoveryAndRefresh) {
    // 1. Create system applications
    {
        std::ofstream ofs(sys_apps_ / "calc.desktop");
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Calculator\n"
            << "Exec=gnome-calculator\n"
            << "Icon=org.gnome.Calculator\n"
            << "Categories=Utility;Calculator;\n";
    }
    {
        std::ofstream ofs(sys_apps_ / "terminal.desktop");
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Terminal\n"
            << "Exec=kgx\n"
            << "Icon=org.gnome.Console\n"
            << "Categories=System;TerminalEmulator;\n";
    }

    // 2. Create local application overriding system
    {
        std::ofstream ofs(local_apps_ / "editor.desktop");
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Text Editor\n"
            << "Exec=gedit %U\n"
            << "Categories=Utility;TextEditor;\n";
    }

    // 3. Create user application
    {
        std::ofstream ofs(user_apps_ / "custom-tool.desktop");
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=My Custom Tool\n"
            << "Exec=custom-tool\n"
            << "Categories=Development;\n";
    }

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    policy.add_search_directory(user_apps_, DesktopEntrySourceType::User, 0);
    policy.add_search_directory(local_apps_, DesktopEntrySourceType::Local, 1);
    policy.add_search_directory(sys_apps_, DesktopEntrySourceType::System, 2);

    ApplicationDiscovery discovery(catalog, policy);
    Status s = discovery.scan_and_refresh();
    ASSERT_TRUE(s.is_ok());

    EXPECT_EQ(catalog.count(), 4u);
    EXPECT_EQ(catalog.visible_count("LinuxDroid"), 4u);

    // Verify all applications are discoverable
    EXPECT_TRUE(catalog.contains(ApplicationId("calc.desktop")));
    EXPECT_TRUE(catalog.contains(ApplicationId("terminal.desktop")));
    EXPECT_TRUE(catalog.contains(ApplicationId("editor.desktop")));
    EXPECT_TRUE(catalog.contains(ApplicationId("custom-tool.desktop")));

    // Search query test
    auto dev_apps = catalog.search("Custom");
    ASSERT_EQ(dev_apps.size(), 1u);
    EXPECT_EQ(dev_apps[0].id().value(), "custom-tool.desktop");

    // 4. Dynamic addition of a new application
    {
        std::ofstream ofs(user_apps_ / "browser.desktop");
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Web Browser\n"
            << "Exec=firefox %u\n"
            << "Categories=Network;WebBrowser;\n";
    }

    // Explicit refresh
    s = discovery.scan_and_refresh();
    ASSERT_TRUE(s.is_ok());
    EXPECT_EQ(catalog.count(), 5u);
    EXPECT_TRUE(catalog.contains(ApplicationId("browser.desktop")));

    // 5. Dynamic removal of an application
    std::filesystem::remove(user_apps_ / "custom-tool.desktop");
    s = discovery.scan_and_refresh();
    ASSERT_TRUE(s.is_ok());
    EXPECT_EQ(catalog.count(), 4u);
    EXPECT_FALSE(catalog.contains(ApplicationId("custom-tool.desktop")));
}

TEST_F(ApplicationIntegrationTest, HostSystemApplicationsDiscovery) {
    // Test discovering actual installed system applications from /usr/share/applications
    if (!std::filesystem::exists("/usr/share/applications")) {
        GTEST_SKIP() << "/usr/share/applications not present on host";
    }

    ApplicationCatalog catalog;
    config::Config cfg;
    ApplicationDiscoveryPolicy policy = ApplicationDiscoveryPolicy::from_config_and_env(cfg);

    ApplicationDiscovery discovery(catalog, policy);
    Status s = discovery.scan_and_refresh();
    ASSERT_TRUE(s.is_ok());

    EXPECT_GT(catalog.count(), 0u);

    // If htop or vim is installed, verify their discovery
    if (std::filesystem::exists("/usr/share/applications/htop.desktop")) {
        const auto* htop_meta = catalog.find(ApplicationId("htop.desktop"));
        ASSERT_NE(htop_meta, nullptr);
        EXPECT_EQ(htop_meta->name(), "Htop");
        EXPECT_EQ(htop_meta->executable(), "htop");
        EXPECT_TRUE(htop_meta->terminal());
        EXPECT_TRUE(htop_meta->has_category("System"));
    }

    // Verify deterministic ordering of visible applications
    auto visible = catalog.visible_applications();
    for (size_t i = 1; i < visible.size(); ++i) {
        std::string prev = visible[i - 1].name();
        std::string curr = visible[i].name();
        for (char& c : prev) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (char& c : curr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        EXPECT_LE(prev, curr);
    }
}

