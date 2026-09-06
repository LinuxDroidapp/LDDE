#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <vector>

namespace ldde::system {

enum class BatteryState {
    Discharging,
    Charging,
    Full,
    NotPresent,
    Unavailable
};

[[nodiscard]] std::string_view battery_state_name(BatteryState state) noexcept;

struct BatteryInfo {
    int percentage = 100;
    BatteryState state = BatteryState::Unavailable;
    bool is_present = false;
    bool is_charging = false;
    bool is_available = false;
};

class BatteryStatusProvider {
public:
    virtual ~BatteryStatusProvider() = default;
    [[nodiscard]] virtual BatteryInfo query_battery() = 0;
};

class LinuxSysfsBatteryProvider : public BatteryStatusProvider {
public:
    explicit LinuxSysfsBatteryProvider(std::string sysfs_path = "/sys/class/power_supply");
    [[nodiscard]] BatteryInfo query_battery() override;

private:
    std::string sysfs_path_;
};

class MockBatteryProvider : public BatteryStatusProvider {
public:
    void set_battery_info(BatteryInfo info) { info_ = std::move(info); }
    [[nodiscard]] BatteryInfo query_battery() override { return info_; }

private:
    BatteryInfo info_;
};

class BatteryStatus {
public:
    using BatteryChangedCallback = std::function<void(const BatteryInfo&)>;

    explicit BatteryStatus(std::shared_ptr<BatteryStatusProvider> provider = nullptr);

    void set_provider(std::shared_ptr<BatteryStatusProvider> provider);
    bool update();

    [[nodiscard]] const BatteryInfo& info() const noexcept { return current_info_; }

    void on_changed(BatteryChangedCallback callback);

private:
    std::shared_ptr<BatteryStatusProvider> provider_;
    BatteryInfo current_info_;
    std::vector<BatteryChangedCallback> callbacks_;
};

} // namespace ldde::system

