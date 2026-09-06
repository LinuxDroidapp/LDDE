#include "ldde/system/clock.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ldde::system {

ClockInfo SystemClockProvider::query_time(ClockFormat format, bool show_seconds) {
    ClockInfo info;
    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&now, &tm_buf);

    info.hours = tm_buf.tm_hour;
    info.minutes = tm_buf.tm_min;
    info.seconds = tm_buf.tm_sec;

    char time_str[64];
    if (format == ClockFormat::Format24H) {
        if (show_seconds) {
            std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);
        } else {
            std::strftime(time_str, sizeof(time_str), "%H:%M", &tm_buf);
        }
    } else {
        if (show_seconds) {
            std::strftime(time_str, sizeof(time_str), "%I:%M:%S %p", &tm_buf);
        } else {
            std::strftime(time_str, sizeof(time_str), "%I:%M %p", &tm_buf);
        }
    }
    // Trim leading zero in 12-hour format (e.g., "01:42 PM" -> "1:42 PM")
    if (format == ClockFormat::Format12H && time_str[0] == '0') {
        info.formatted_time = std::string(time_str + 1);
    } else {
        info.formatted_time = std::string(time_str);
    }

    char date_str[64];
    std::strftime(date_str, sizeof(date_str), "%a, %b %e", &tm_buf);
    info.formatted_date = std::string(date_str);

    return info;
}

Clock::Clock(std::shared_ptr<ClockProvider> provider, ClockFormat format, bool show_seconds)
    : provider_(std::move(provider)), format_(format), show_seconds_(show_seconds) {
    if (!provider_) {
        provider_ = std::make_shared<SystemClockProvider>();
    }
    update();
}

void Clock::set_format(ClockFormat format) {
    if (format_ != format) {
        format_ = format;
        update();
    }
}

void Clock::set_show_seconds(bool show) {
    if (show_seconds_ != show) {
        show_seconds_ = show;
        update();
    }
}

void Clock::set_provider(std::shared_ptr<ClockProvider> provider) {
    provider_ = std::move(provider);
    if (!provider_) {
        provider_ = std::make_shared<SystemClockProvider>();
    }
    update();
}

bool Clock::update() {
    if (!provider_) return false;
    ClockInfo new_info = provider_->query_time(format_, show_seconds_);

    bool changed = (new_info.formatted_time != current_info_.formatted_time ||
                    new_info.formatted_date != current_info_.formatted_date);

    current_info_ = std::move(new_info);

    if (changed) {
        for (const auto& cb : callbacks_) {
            if (cb) cb(current_info_);
        }
    }
    return changed;
}

void Clock::on_changed(ClockChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system
