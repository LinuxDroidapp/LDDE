#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include <mutex>
#include <memory>
#include <vector>
#include <optional>

namespace ldde::core {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

enum class LogCategory {
    Ldde = 0,
    Core,
    Wayland,
    Shell,
    Window,
    Application,
    Launcher,
    Dock,
    Switcher,
    Desktop,
    Input,
    Display,
    Notification,
    System,
    Config,
    Ipc,
    Session
};

[[nodiscard]] std::string_view log_level_name(LogLevel level) noexcept;
[[nodiscard]] std::optional<LogLevel> parse_log_level(std::string_view str) noexcept;

[[nodiscard]] std::string_view log_category_name(LogCategory category) noexcept;
[[nodiscard]] std::optional<LogCategory> parse_log_category(std::string_view str) noexcept;

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(LogLevel level, LogCategory category, std::string_view message,
                       std::string_view file, int line) = 0;
    virtual void flush() = 0;
};

class Logger {
public:
    static Logger& instance() noexcept;

    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void add_sink(std::shared_ptr<LogSink> sink);
    void clear_sinks();

    void log(LogLevel level, LogCategory category, std::string_view message,
             std::string_view file = {}, int line = 0);

    void flush();

    [[nodiscard]] bool is_level_enabled(LogLevel level) const noexcept {
        return level >= min_level_;
    }

private:
    Logger();
    ~Logger();

    LogLevel min_level_ = LogLevel::Info;
    std::mutex mutex_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
};

class LogMessageBuilder {
public:
    LogMessageBuilder(LogLevel level, LogCategory category, std::string_view file, int line)
        : level_(level), category_(category), file_(file), line_(line) {}

    ~LogMessageBuilder() {
        Logger::instance().log(level_, category_, stream_.str(), file_, line_);
    }

    template <typename T>
    LogMessageBuilder& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    LogCategory category_;
    std::string_view file_;
    int line_;
    std::ostringstream stream_;
};

#define LDDE_LOG_STREAM(level, category) \
    if (::ldde::core::Logger::instance().is_level_enabled(level)) \
        ::ldde::core::LogMessageBuilder(level, category, __FILE__, __LINE__)

#define LDDE_LOG_TRACE(category, msg) LDDE_LOG_STREAM(::ldde::core::LogLevel::Trace, ::ldde::core::LogCategory::category) << msg
#define LDDE_LOG_DEBUG(category, msg) LDDE_LOG_STREAM(::ldde::core::LogLevel::Debug, ::ldde::core::LogCategory::category) << msg
#define LDDE_LOG_INFO(category, msg)  LDDE_LOG_STREAM(::ldde::core::LogLevel::Info,  ::ldde::core::LogCategory::category) << msg
#define LDDE_LOG_WARN(category, msg)  LDDE_LOG_STREAM(::ldde::core::LogLevel::Warn,  ::ldde::core::LogCategory::category) << msg
#define LDDE_LOG_ERROR(category, msg) LDDE_LOG_STREAM(::ldde::core::LogLevel::Error, ::ldde::core::LogCategory::category) << msg
#define LDDE_LOG_FATAL(category, msg) LDDE_LOG_STREAM(::ldde::core::LogLevel::Fatal, ::ldde::core::LogCategory::category) << msg

} // namespace ldde::core

