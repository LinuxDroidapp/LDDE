#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <vector>

namespace ldde::system {

enum class SessionState {
    Active,
    Idle,
    Locked,
    Terminating
};

[[nodiscard]] std::string_view session_state_name(SessionState state) noexcept;

struct SessionInfo {
    SessionState state = SessionState::Active;
    std::string compositor_name = "Weston";
    std::string desktop_identity = "LinuxDroid LDDE";
    std::string status_label = "Active";
    bool wayland_connected = true;
};

class SessionStatusProvider {
public:
    virtual ~SessionStatusProvider() = default;
    [[nodiscard]] virtual SessionInfo query_session() = 0;
};

class DesktopSessionStatusProvider : public SessionStatusProvider {
public:
    explicit DesktopSessionStatusProvider(std::string compositor_name = "Weston",
                                          std::string desktop_identity = "LinuxDroid LDDE");
    [[nodiscard]] SessionInfo query_session() override;

    void set_state(SessionState state);
    void set_wayland_connected(bool connected);

private:
    std::string compositor_name_;
    std::string desktop_identity_;
    SessionState state_ = SessionState::Active;
    bool wayland_connected_ = true;
};

class SessionStatus {
public:
    using SessionChangedCallback = std::function<void(const SessionInfo&)>;

    explicit SessionStatus(std::shared_ptr<SessionStatusProvider> provider = nullptr);

    void set_provider(std::shared_ptr<SessionStatusProvider> provider);
    bool update();

    [[nodiscard]] const SessionInfo& info() const noexcept { return current_info_; }

    void on_changed(SessionChangedCallback callback);

private:
    std::shared_ptr<SessionStatusProvider> provider_;
    SessionInfo current_info_;
    std::vector<SessionChangedCallback> callbacks_;
};

} // namespace ldde::system
