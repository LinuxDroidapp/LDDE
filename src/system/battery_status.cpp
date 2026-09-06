#include "ldde/system/battery_status.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace ldde::system {

std::string_view battery_state_name(BatteryState state) noexcept {
    switch (state) {
        case BatteryState::Discharging: return "Discharging";
        case BatteryState::Charging:    return "Charging";
        case BatteryState::Full:        return "Full";
        case BatteryState::NotPresent:  return "NotPresent";
        case BatteryState::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

LinuxSysfsBatteryProvider::LinuxSysfsBatteryProvider(std::string sysfs_path)
    : sysfs_path_(std::move(sysfs_path)) {}

BatteryInfo LinuxSysfsBatteryProvider::query_battery() {
    BatteryInfo info;
    std::error_code ec;

    if (!std::filesystem::exists(sysfs_path_, ec) || !std::filesystem::is_directory(sysfs_path_, ec)) {
        info.state = BatteryState::Unavailable;
        info.is_available = false;
        info.is_present = false;
        return info;
    }

    bool found_battery = false;

    for (const auto& entry : std::filesystem::directory_iterator(sysfs_path_, ec)) {
        if (ec) break;
        std::string name = entry.path().filename().string();

        std::string type_str;
        std::ifstream type_file(entry.path() / "type");
        if (type_file.is_open()) {
            type_file >> type_str;
        }

        if (type_str == "Battery" || name.rfind("BAT", 0) == 0) {
            found_battery = true;

            int cap = 100;
            std::ifstream cap_file(entry.path() / "capacity");
            if (cap_file.is_open()) {
                cap_file >> cap;
            }

            std::string status_str;
            std::ifstream status_file(entry.path() / "status");
            if (status_file.is_open()) {
                status_file >> status_str;
            }

            info.percentage = std::clamp(cap, 0, 100);
            info.is_present = true;
            info.is_available = true;

            if (status_str == "Charging") {
                info.state = BatteryState::Charging;
                info.is_charging = true;
            } else if (status_str == "Full") {
                info.state = BatteryState::Full;
                info.is_charging = false;
            } else if (status_str == "Discharging" || status_str == "Not charging") {
                info.state = BatteryState::Discharging;
                info.is_charging = false;
            } else {
                info.state = BatteryState::Discharging;
                info.is_charging = false;
            }
            break;
        }
    }

    if (!found_battery) {
        info.state = BatteryState::Unavailable;
        info.is_available = false;
        info.is_present = false;
    }

    return info;
}

BatteryStatus::BatteryStatus(std::shared_ptr<BatteryStatusProvider> provider)
    : provider_(std::move(provider)) {
    if (!provider_) {
        provider_ = std::make_shared<LinuxSysfsBatteryProvider>();
    }
    update();
}

void BatteryStatus::set_provider(std::shared_ptr<BatteryStatusProvider> provider) {
    provider_ = std::move(provider);
    if (!provider_) {
        provider_ = std::make_shared<LinuxSysfsBatteryProvider>();
    }
    update();
}

bool BatteryStatus::update() {
    if (!provider_) return false;
    BatteryInfo new_info = provider_->query_battery();

    bool changed = (new_info.percentage != current_info_.percentage ||
                    new_info.state != current_info_.state ||
                    new_info.is_charging != current_info_.is_charging ||
                    new_info.is_available != current_info_.is_available ||
                    new_info.is_present != current_info_.is_present);

    current_info_ = std::move(new_info);

    if (changed) {
        for (const auto& cb : callbacks_) {
            if (cb) cb(current_info_);
        }
    }
    return changed;
}

void BatteryStatus::on_changed(BatteryChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system
