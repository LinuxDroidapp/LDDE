#include "ldde/system/system_data_provider.hpp"

namespace ldde::system {

SystemDataProvider::SystemDataProvider(
    std::shared_ptr<ClockProvider> clock_provider,
    std::shared_ptr<NetworkStatusProvider> net_provider,
    std::shared_ptr<AudioStatusProvider> audio_provider,
    std::shared_ptr<BatteryStatusProvider> batt_provider,
    std::shared_ptr<SessionStatusProvider> sess_provider)
    : clock_(std::move(clock_provider)),
      network_(std::move(net_provider)),
      audio_(std::move(audio_provider)),
      battery_(std::move(batt_provider)),
      session_(std::move(sess_provider)) {

    clock_.on_changed([this](const ClockInfo&) { notify_changed(); });
    network_.on_changed([this](const NetworkInfo&) { notify_changed(); });
    audio_.on_changed([this](const AudioInfo&) { notify_changed(); });
    battery_.on_changed([this](const BatteryInfo&) { notify_changed(); });
    display_.on_changed([this](const DisplayStatusInfo&) { notify_changed(); });
    session_.on_changed([this](const SessionInfo&) { notify_changed(); });
}

void SystemDataProvider::refresh_all() {
    bool c1 = clock_.update();
    bool c2 = network_.update();
    bool c3 = audio_.update();
    bool c4 = battery_.update();
    bool c5 = session_.update();
    if (c1 || c2 || c3 || c4 || c5) {
        notify_changed();
    }
}

void SystemDataProvider::on_changed(ChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

void SystemDataProvider::notify_changed() {
    for (const auto& cb : callbacks_) {
        if (cb) cb();
    }
}

} // namespace ldde::system
