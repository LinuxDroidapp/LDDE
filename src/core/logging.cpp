#include "ldde/core/logging.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace ldde::core {

namespace {

std::string format_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    gmtime_r(&now_time_t, &tm_buf);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buffer);
}

class ConsoleSink : public LogSink {
public:
    void write(LogLevel level, LogCategory category, std::string_view message,
               std::string_view file, int line) override {
        std::ostream& out = (level >= LogLevel::Warn) ? std::cerr : std::cout;

        out << "[" << format_timestamp() << "] "
            << "[" << std::left << std::setw(5) << log_level_name(level) << "] "
            << "[" << log_category_name(category) << "] "
            << message;

        if (!file.empty() && level <= LogLevel::Debug) {
            out << " (" << file << ":" << line << ")";
        }
        out << "\n";
    }

    void flush() override {
        std::cout.flush();
        std::cerr.flush();
    }
};

std::string to_upper(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

} // namespace

std::string_view log_level_name(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

std::optional<LogLevel> parse_log_level(std::string_view str) noexcept {
    std::string upper = to_upper(str);
    if (upper == "TRACE") return LogLevel::Trace;
    if (upper == "DEBUG") return LogLevel::Debug;
    if (upper == "INFO")  return LogLevel::Info;
    if (upper == "WARN" || upper == "WARNING") return LogLevel::Warn;
    if (upper == "ERROR") return LogLevel::Error;
    if (upper == "FATAL") return LogLevel::Fatal;
    return std::nullopt;
}

std::string_view log_category_name(LogCategory category) noexcept {
    switch (category) {
        case LogCategory::Ldde:         return "LDDE";
        case LogCategory::Core:         return "CORE";
        case LogCategory::Wayland:      return "WAYLAND";
        case LogCategory::Shell:        return "SHELL";
        case LogCategory::Window:       return "WINDOW";
        case LogCategory::Application:  return "APPLICATION";
        case LogCategory::Launcher:     return "LAUNCHER";
        case LogCategory::Dock:         return "DOCK";
        case LogCategory::Switcher:     return "SWITCHER";
        case LogCategory::Input:        return "INPUT";
        case LogCategory::Display:      return "DISPLAY";
        case LogCategory::Notification: return "NOTIFICATION";
        case LogCategory::System:       return "SYSTEM";
        case LogCategory::Config:       return "CONFIG";
        case LogCategory::Ipc:          return "IPC";
        case LogCategory::Session:      return "SESSION";
    }
    return "UNKNOWN";
}

std::optional<LogCategory> parse_log_category(std::string_view str) noexcept {
    std::string upper = to_upper(str);
    if (upper == "LDDE")         return LogCategory::Ldde;
    if (upper == "CORE")         return LogCategory::Core;
    if (upper == "WAYLAND")      return LogCategory::Wayland;
    if (upper == "SHELL")        return LogCategory::Shell;
    if (upper == "WINDOW")       return LogCategory::Window;
    if (upper == "APPLICATION")  return LogCategory::Application;
    if (upper == "LAUNCHER")     return LogCategory::Launcher;
    if (upper == "DOCK")         return LogCategory::Dock;
    if (upper == "SWITCHER")     return LogCategory::Switcher;
    if (upper == "INPUT")        return LogCategory::Input;
    if (upper == "DISPLAY")      return LogCategory::Display;
    if (upper == "NOTIFICATION") return LogCategory::Notification;
    if (upper == "SYSTEM")       return LogCategory::System;
    if (upper == "CONFIG")       return LogCategory::Config;
    if (upper == "IPC")          return LogCategory::Ipc;
    if (upper == "SESSION")      return LogCategory::Session;
    return std::nullopt;
}

Logger::Logger() {
    sinks_.push_back(std::make_shared<ConsoleSink>());
}

Logger::~Logger() {
    flush();
}

Logger& Logger::instance() noexcept {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) noexcept {
    min_level_ = level;
}

LogLevel Logger::level() const noexcept {
    return min_level_;
}

void Logger::add_sink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, LogCategory category, std::string_view message,
                 std::string_view file, int line) {
    if (level < min_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->write(level, category, message, file, line);
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

} // namespace ldde::core
