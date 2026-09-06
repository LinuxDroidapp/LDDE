#pragma once

#include <memory>
#include <functional>
#include <vector>
#include "ldde/system/clock.hpp"
#include "ldde/system/network_status.hpp"
#include "ldde/system/audio_status.hpp"
#include "ldde/system/battery_status.hpp"
#include "ldde/system/display_status.hpp"
#include "ldde/system/session_status.hpp"

namespace ldde::system {

class SystemDataProvider {
public:
    using ChangedCallback = std::function<void()>;

    SystemDataProvider(
        std::shared_ptr<ClockProvider> clock_provider = nullptr,
        std::shared_ptr<NetworkStatusProvider> net_provider = nullptr,
        std::shared_ptr<AudioStatusProvider> audio_provider = nullptr,
        std::shared_ptr<BatteryStatusProvider> batt_provider = nullptr,
        std::shared_ptr<SessionStatusProvider> sess_provider = nullptr);

    [[nodiscard]] Clock& clock() noexcept { return clock_; }
    [[nodiscard]] const Clock& clock() const noexcept { return clock_; }

    [[nodiscard]] NetworkStatus& network() noexcept { return network_; }
    [[nodiscard]] const NetworkStatus& network() const noexcept { return network_; }

    [[nodiscard]] AudioStatus& audio() noexcept { return audio_; }
    [[nodiscard]] const AudioStatus& audio() const noexcept { return audio_; }

    [[nodiscard]] BatteryStatus& battery() noexcept { return battery_; }
    [[nodiscard]] const BatteryStatus& battery() const noexcept { return battery_; }

    [[nodiscard]] DisplayStatus& display() noexcept { return display_; }
    [[nodiscard]] const DisplayStatus& display() const noexcept { return display_; }

    [[nodiscard]] SessionStatus& session() noexcept { return session_; }
    [[nodiscard]] const SessionStatus& session() const noexcept { return session_; }

    void refresh_all();
    void on_changed(ChangedCallback callback);

private:
    void notify_changed();

    Clock clock_;
    NetworkStatus network_;
    AudioStatus audio_;
    BatteryStatus battery_;
    DisplayStatus display_;
    SessionStatus session_;

    std::vector<ChangedCallback> callbacks_;
};

} // namespace ldde::system
