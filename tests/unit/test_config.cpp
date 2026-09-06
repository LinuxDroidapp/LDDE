#include <gtest/gtest.h>
#include "ldde/config/config.hpp"
#include <sstream>
#include <fstream>
#include <filesystem>

using namespace ldde::config;

TEST(ConfigTest, Defaults) {
    Config cfg;
    cfg.load_defaults();

    EXPECT_EQ(cfg.version(), 1);
    EXPECT_EQ(cfg.get_string_or("general", "desktop_name", ""), "LDDE");
    EXPECT_EQ(cfg.get_string_or("logging", "level", ""), "INFO");
    EXPECT_DOUBLE_EQ(cfg.get_double_or("display", "scale_factor", 0.0), 1.0);
    EXPECT_TRUE(cfg.get_bool_or("input", "tap_to_click", false));
    EXPECT_TRUE(cfg.get_bool_or("switcher", "enabled", false));
    EXPECT_EQ(cfg.get_string_or("switcher", "presentation", ""), "application");
    EXPECT_TRUE(cfg.get_bool_or("switcher", "mru", false));
    EXPECT_EQ(cfg.get_string_or("desktop", "background_mode", ""), "gradient");
    EXPECT_EQ(cfg.get_string_or("desktop", "background_color", ""), "#121826");
    EXPECT_TRUE(cfg.get_bool_or("desktop", "ambient_glow", false));
    EXPECT_EQ(cfg.get_string_or("system", "clock_format", ""), "24h");
    EXPECT_TRUE(cfg.get_bool_or("system", "status_bar_enabled", false));
    EXPECT_TRUE(cfg.get_bool_or("system", "quick_controls_enabled", false));
    EXPECT_TRUE(cfg.validate().is_ok());
}

TEST(ConfigTest, CustomFileParsing) {
    std::string temp_path = "/tmp/test_ldde_custom.conf";
    {
        std::ofstream out(temp_path);
        out << "# Test configuration\n"
            << "[general]\n"
            << "config_version = 1\n"
            << "desktop_name = CustomLDDE\n"
            << "\n"
            << "[display]\n"
            << "scale_factor = 2.0\n"
            << "\n"
            << "[input]\n"
            << "tap_to_click = false\n";
    }

    Config cfg;
    Status s = cfg.load_with_precedence(temp_path);
    std::filesystem::remove(temp_path);

    ASSERT_TRUE(s.is_ok()) << s.to_string();
    EXPECT_EQ(cfg.get_string_or("general", "desktop_name", ""), "CustomLDDE");
    EXPECT_DOUBLE_EQ(cfg.get_double_or("display", "scale_factor", 0.0), 2.0);
    EXPECT_FALSE(cfg.get_bool_or("input", "tap_to_click", true));
}

TEST(ConfigTest, VersionValidation) {
    std::string temp_path = "/tmp/test_ldde_invalid_ver.conf";
    {
        std::ofstream out(temp_path);
        out << "[general]\n"
            << "config_version = 99\n";
    }

    Config cfg;
    Status s = cfg.load_with_precedence(temp_path);
    std::filesystem::remove(temp_path);

    EXPECT_TRUE(s.is_error());
    EXPECT_EQ(s.error().code(), ldde::core::ErrorCode::ConfigVersionMismatch);
}

TEST(ConfigTest, MalformedSyntax) {
    std::string temp_path = "/tmp/test_ldde_malformed.conf";
    {
        std::ofstream out(temp_path);
        out << "this line has no equals sign\n";
    }

    Config cfg;
    Status s = cfg.load_with_precedence(temp_path);
    std::filesystem::remove(temp_path);

    EXPECT_TRUE(s.is_error());
    EXPECT_EQ(s.error().code(), ldde::core::ErrorCode::ConfigParseError);
}

TEST(ConfigTest, TypedGetters) {
    Config cfg;
    cfg.set("test", "str", "hello");
    cfg.set("test", "num", "42");
    cfg.set("test", "flt", "3.1415");
    cfg.set("test", "b1", "true");
    cfg.set("test", "b2", "off");

    EXPECT_EQ(cfg.get_string_or("test", "str", ""), "hello");
    EXPECT_EQ(cfg.get_int_or("test", "num", 0), 42);
    EXPECT_NEAR(cfg.get_double_or("test", "flt", 0.0), 3.1415, 0.0001);
    EXPECT_TRUE(cfg.get_bool_or("test", "b1", false));
    EXPECT_FALSE(cfg.get_bool_or("test", "b2", true));
}

