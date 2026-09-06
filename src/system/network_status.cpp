#include "ldde/system/network_status.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace ldde::system {

std::string_view network_state_name(NetworkState state) noexcept {
    switch (state) {
        case NetworkState::Disconnected: return "Disconnected";
        case NetworkState::Connecting:   return "Connecting";
        case NetworkState::Connected:    return "Connected";
        case NetworkState::Limited:      return "Limited";
        case NetworkState::Unavailable:  return "Unavailable";
    }
    return "Unknown";
}

std::string_view network_type_name(NetworkType type) noexcept {
    switch (type) {
        case NetworkType::None:     return "None";
        case NetworkType::Ethernet: return "Ethernet";
        case NetworkType::Wifi:     return "Wi-Fi";
        case NetworkType::Cellular: return "Cellular";
        case NetworkType::Other:    return "Other";
    }
    return "Unknown";
}

LinuxSysfsNetworkProvider::LinuxSysfsNetworkProvider(std::string sysfs_path)
    : sysfs_path_(std::move(sysfs_path)) {}

NetworkInfo LinuxSysfsNetworkProvider::query_network() {
    NetworkInfo info;
    info.is_enabled = enabled_;

    if (!enabled_) {
        info.state = NetworkState::Disconnected;
        info.type = NetworkType::None;
        info.is_connected = false;
        info.status_label = "Disabled";
        return info;
    }

    std::error_code ec;
    if (!std::filesystem::exists(sysfs_path_, ec) || !std::filesystem::is_directory(sysfs_path_, ec)) {
        info.state = NetworkState::Unavailable;
        info.type = NetworkType::None;
        info.status_label = "Unavailable";
        return info;
    }

    bool has_interfaces = false;
    std::string connected_iface;
    NetworkType detected_type = NetworkType::None;

    for (const auto& entry : std::filesystem::directory_iterator(sysfs_path_, ec)) {
        if (ec) break;
        std::string ifname = entry.path().filename().string();
        if (ifname == "lo") continue;

        has_interfaces = true;

        // Check operstate
        std::string operstate;
        std::ifstream state_file(entry.path() / "operstate");
        if (state_file.is_open()) {
            state_file >> operstate;
        }

        // Check carrier
        int carrier = 0;
        std::ifstream carrier_file(entry.path() / "carrier");
        if (carrier_file.is_open()) {
            carrier_file >> carrier;
        }

        if (operstate == "up" || carrier == 1) {
            connected_iface = ifname;
            if (ifname.rfind("wl", 0) == 0 || ifname.rfind("wlan", 0) == 0) {
                detected_type = NetworkType::Wifi;
            } else if (ifname.rfind("eth", 0) == 0 || ifname.rfind("en", 0) == 0) {
                detected_type = NetworkType::Ethernet;
            } else if (ifname.rfind("rmnet", 0) == 0 || ifname.rfind("wwan", 0) == 0) {
                detected_type = NetworkType::Cellular;
            } else {
                detected_type = NetworkType::Other;
            }
            break;
        }
    }

    if (!connected_iface.empty()) {
        info.state = NetworkState::Connected;
        info.type = detected_type;
        info.interface_name = connected_iface;
        info.is_connected = true;
        info.status_label = "Connected (" + connected_iface + ")";
    } else if (has_interfaces) {
        info.state = NetworkState::Disconnected;
        info.type = NetworkType::None;
        info.is_connected = false;
        info.status_label = "Disconnected";
    } else {
        info.state = NetworkState::Unavailable;
        info.type = NetworkType::None;
        info.is_connected = false;
        info.status_label = "Unavailable";
    }

    return info;
}

void LinuxSysfsNetworkProvider::set_network_enabled(bool enabled) {
    enabled_ = enabled;
}

NetworkStatus::NetworkStatus(std::shared_ptr<NetworkStatusProvider> provider)
    : provider_(std::move(provider)) {
    if (!provider_) {
        provider_ = std::make_shared<LinuxSysfsNetworkProvider>();
    }
    update();
}

void NetworkStatus::set_provider(std::shared_ptr<NetworkStatusProvider> provider) {
    provider_ = std::move(provider);
    if (!provider_) {
        provider_ = std::make_shared<LinuxSysfsNetworkProvider>();
    }
    update();
}

bool NetworkStatus::update() {
    if (!provider_) return false;
    NetworkInfo new_info = provider_->query_network();

    bool changed = (new_info.state != current_info_.state ||
                    new_info.type != current_info_.type ||
                    new_info.interface_name != current_info_.interface_name ||
                    new_info.is_connected != current_info_.is_connected ||
                    new_info.is_enabled != current_info_.is_enabled);

    current_info_ = std::move(new_info);

    if (changed) {
        for (const auto& cb : callbacks_) {
            if (cb) cb(current_info_);
        }
    }
    return changed;
}

void NetworkStatus::set_enabled(bool enabled) {
    if (provider_) {
        provider_->set_network_enabled(enabled);
        update();
    }
}

void NetworkStatus::toggle_enabled() {
    set_enabled(!current_info_.is_enabled);
}

void NetworkStatus::on_changed(NetworkChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system

