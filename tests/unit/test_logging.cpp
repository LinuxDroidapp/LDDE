#include <gtest/gtest.h>
#include "ldde/core/logging.hpp"

using namespace ldde::core;

class TestSink : public LogSink {
public:
    struct Entry {
        LogLevel level;
        LogCategory category;
        std::string message;
        std::string file;
        int line;
    };

    std::vector<Entry> entries;

    void write(LogLevel level, LogCategory category, std::string_view message,
               std::string_view file, int line) override {
        entries.push_back(Entry{
            .level = level,
            .category = category,
            .message = std::string(message),
            .file = std::string(file),
            .line = line
        });
    }

    void flush() override {}
};

TEST(LoggingTest, LogLevelNamesAndParsing) {
    EXPECT_EQ(log_level_name(LogLevel::Trace), "TRACE");
    EXPECT_EQ(log_level_name(LogLevel::Debug), "DEBUG");
    EXPECT_EQ(log_level_name(LogLevel::Info), "INFO");
    EXPECT_EQ(log_level_name(LogLevel::Warn), "WARN");
    EXPECT_EQ(log_level_name(LogLevel::Error), "ERROR");
    EXPECT_EQ(log_level_name(LogLevel::Fatal), "FATAL");

    EXPECT_EQ(parse_log_level("debug"), LogLevel::Debug);
    EXPECT_EQ(parse_log_level("INFO"), LogLevel::Info);
    EXPECT_EQ(parse_log_level("warning"), LogLevel::Warn);
    EXPECT_FALSE(parse_log_level("invalid").has_value());
}

TEST(LoggingTest, CategoryNamesAndParsing) {
    EXPECT_EQ(log_category_name(LogCategory::Ldde), "LDDE");
    EXPECT_EQ(log_category_name(LogCategory::Wayland), "WAYLAND");
    EXPECT_EQ(log_category_name(LogCategory::Display), "DISPLAY");
    EXPECT_EQ(log_category_name(LogCategory::Input), "INPUT");

    EXPECT_EQ(parse_log_category("core"), LogCategory::Core);
    EXPECT_EQ(parse_log_category("SHELL"), LogCategory::Shell);
    EXPECT_FALSE(parse_log_category("nonexistent").has_value());
}

TEST(LoggingTest, LevelFilteringAndSink) {
    auto sink = std::make_shared<TestSink>();
    Logger& logger = Logger::instance();
    logger.clear_sinks();
    logger.add_sink(sink);
    logger.set_level(LogLevel::Warn);

    LDDE_LOG_INFO(Core, "This is info - should be ignored");
    LDDE_LOG_WARN(Core, "This is a warning - should be captured");
    LDDE_LOG_ERROR(Wayland, "This is an error - should be captured");

    ASSERT_EQ(sink->entries.size(), 2u);
    EXPECT_EQ(sink->entries[0].level, LogLevel::Warn);
    EXPECT_EQ(sink->entries[0].category, LogCategory::Core);
    EXPECT_EQ(sink->entries[0].message, "This is a warning - should be captured");

    EXPECT_EQ(sink->entries[1].level, LogLevel::Error);
    EXPECT_EQ(sink->entries[1].category, LogCategory::Wayland);
    EXPECT_EQ(sink->entries[1].message, "This is an error - should be captured");

    logger.clear_sinks();
}

