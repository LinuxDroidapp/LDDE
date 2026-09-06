#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <vector>

namespace ldde::system {

enum class NetworkState {
    Disconnected,
    Connecting,
    Connected,
    Limited,
    Unavailable
};

enum class NetworkType {
    None,
    Ethernet,
    Wifi,
    Cellular,
    Other
};

[[nodiscard]] std::string_view network_state_name(NetworkState state) noexcept;
[[nodiscard]] std::string_view network_type_name(NetworkType type) noexcept;

struct NetworkInfo {
    NetworkState state = NetworkState::Unavailable;
    NetworkType type = NetworkType::None;
    std::string interface_name;
    std::string ip_address;
    std::string status_label = "Unavailable";
    bool is_connected = false;
    bool is_enabled = true;
};

class NetworkStatusProvider {
public:
    virtual ~NetworkStatusProvider() = default;
    [[nodiscard]] virtual NetworkInfo query_network() = 0;
    virtual void set_network_enabled(bool enabled) = 0;
    [[nodiscard]] virtual bool is_network_enabled() const = 0;
};

class LinuxSysfsNetworkProvider : public NetworkStatusProvider {
public:
    explicit LinuxSysfsNetworkProvider(std::string sysfs_path = "/sys/class/net");
    [[nodiscard]] NetworkInfo query_network() override;
    void set_network_enabled(bool enabled) override;
    [[nodiscard]] bool is_network_enabled() const override { return enabled_; }

private:
    std::string sysfs_path_;
    bool enabled_ = true;
};

class MockNetworkProvider : public NetworkStatusProvider {
public:
    void set_network_info(NetworkInfo info) { info_ = std::move(info); }
    [[nodiscard]] NetworkInfo query_network() override { return info_; }
    void set_network_enabled(bool enabled) override {
        enabled_ = enabled;
        info_.is_enabled = enabled;
        if (!enabled) {
            info_.state = NetworkState::Disconnected;
            info_.is_connected = false;
            info_.status_label = "Disabled";
        }
    }
    [[nodiscard]] bool is_network_enabled() const override { return enabled_; }

private:
    NetworkInfo info_;
    bool enabled_ = true;
};

class NetworkStatus {
public:
    using NetworkChangedCallback = std::function<void(const NetworkInfo&)>;

    explicit NetworkStatus(std::shared_ptr<NetworkStatusProvider> provider = nullptr);

    void set_provider(std::shared_ptr<NetworkStatusProvider> provider);
    bool update();

    [[nodiscard]] const NetworkInfo& info() const noexcept { return current_info_; }
    void set_enabled(bool enabled);
    void toggle_enabled();

    void on_changed(NetworkChangedCallback callback);

private:
    std::shared_ptr<NetworkStatusProvider> provider_;
    NetworkInfo current_info_;
    std::vector<NetworkChangedCallback> callbacks_;
};

} // namespace ldde::system
