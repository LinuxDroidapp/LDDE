#include <gtest/gtest.h>
#include "ldde/core/error.hpp"

using namespace ldde::core;

TEST(ErrorTest, CategoryAndCodeNames) {
    EXPECT_EQ(error_category_name(ErrorCategory::Configuration), "Configuration");
    EXPECT_EQ(error_category_name(ErrorCategory::Wayland), "Wayland");
    EXPECT_EQ(error_category_name(ErrorCategory::Input), "Input");
    EXPECT_EQ(error_category_name(ErrorCategory::Display), "Display");
    EXPECT_EQ(error_category_name(ErrorCategory::Application), "Application");
    EXPECT_EQ(error_category_name(ErrorCategory::Dock), "Dock");
    EXPECT_EQ(error_category_name(ErrorCategory::Switcher), "Switcher");
    EXPECT_EQ(error_category_name(ErrorCategory::Session), "Session");
    EXPECT_EQ(error_category_name(ErrorCategory::Resource), "Resource");
    EXPECT_EQ(error_category_name(ErrorCategory::Internal), "Internal");

    EXPECT_EQ(error_code_name(ErrorCode::Ok), "Ok");
    EXPECT_EQ(error_code_name(ErrorCode::ConfigNotFound), "ConfigNotFound");
    EXPECT_EQ(error_code_name(ErrorCode::WaylandConnectionFailed), "WaylandConnectionFailed");
    EXPECT_EQ(error_code_name(ErrorCode::InvalidLifecycleTransition), "InvalidLifecycleTransition");
    EXPECT_EQ(error_code_name(ErrorCode::SwitcherNotOpen), "SwitcherNotOpen");
    EXPECT_EQ(error_code_name(ErrorCode::SwitcherActivationFailed), "SwitcherActivationFailed");
}

TEST(ErrorTest, StatusOkAndError) {
    Status ok = Status::ok();
    EXPECT_TRUE(ok.is_ok());
    EXPECT_FALSE(ok.is_error());
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_EQ(ok.to_string(), "OK");

    Status err = Status::error(ErrorCategory::Wayland, ErrorCode::WaylandConnectionFailed,
                               "Connection refused", "test.cpp", 42);
    EXPECT_FALSE(err.is_ok());
    EXPECT_TRUE(err.is_error());
    EXPECT_FALSE(static_cast<bool>(err));
    EXPECT_EQ(err.error().category(), ErrorCategory::Wayland);
    EXPECT_EQ(err.error().code(), ErrorCode::WaylandConnectionFailed);
    EXPECT_EQ(err.error().message(), "Connection refused");
    EXPECT_EQ(err.error().file(), "test.cpp");
    EXPECT_EQ(err.error().line(), 42);

    std::string str = err.to_string();
    EXPECT_NE(str.find("Wayland:WaylandConnectionFailed"), std::string::npos);
    EXPECT_NE(str.find("Connection refused"), std::string::npos);
    EXPECT_NE(str.find("test.cpp:42"), std::string::npos);
}

TEST(ErrorTest, ResultTemplate) {
    Result<int> res_val(123);
    EXPECT_TRUE(res_val.is_ok());
    EXPECT_FALSE(res_val.is_error());
    EXPECT_EQ(res_val.value(), 123);

    Result<int> res_err(Error(ErrorCategory::Configuration, ErrorCode::ConfigNotFound, "file missing"));
    EXPECT_FALSE(res_err.is_ok());
    EXPECT_TRUE(res_err.is_error());
    EXPECT_EQ(res_err.error().code(), ErrorCode::ConfigNotFound);
}

