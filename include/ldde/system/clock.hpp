#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <chrono>

namespace ldde::system {

enum class ClockFormat {
    Format24H,
    Format12H
};

struct ClockInfo {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    std::string formatted_time = "12:00";
    std::string formatted_date = "Jan 1";
};

class ClockProvider {
public:
    virtual ~ClockProvider() = default;
    [[nodiscard]] virtual ClockInfo query_time(ClockFormat format, bool show_seconds) = 0;
};

class SystemClockProvider : public ClockProvider {
public:
    [[nodiscard]] ClockInfo query_time(ClockFormat format, bool show_seconds) override;
};

class MockClockProvider : public ClockProvider {
public:
    void set_time(ClockInfo info) { info_ = std::move(info); }
    [[nodiscard]] ClockInfo query_time(ClockFormat /*format*/, bool /*show_seconds*/) override {
        return info_;
    }

private:
    ClockInfo info_;
};

class Clock {
public:
    using ClockChangedCallback = std::function<void(const ClockInfo&)>;

    explicit Clock(std::shared_ptr<ClockProvider> provider = nullptr,
                   ClockFormat format = ClockFormat::Format24H,
                   bool show_seconds = false);

    void set_format(ClockFormat format);
    [[nodiscard]] ClockFormat format() const noexcept { return format_; }

    void set_show_seconds(bool show);
    [[nodiscard]] bool show_seconds() const noexcept { return show_seconds_; }

    void set_provider(std::shared_ptr<ClockProvider> provider);

    bool update();
    [[nodiscard]] const ClockInfo& info() const noexcept { return current_info_; }

    void on_changed(ClockChangedCallback callback);

private:
    std::shared_ptr<ClockProvider> provider_;
    ClockFormat format_ = ClockFormat::Format24H;
    bool show_seconds_ = false;
    ClockInfo current_info_;
    std::vector<ClockChangedCallback> callbacks_;
};

} // namespace ldde::system

