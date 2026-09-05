#pragma once

#include <string_view>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include "ldde/core/error.hpp"

namespace ldde::core {

enum class LifecycleState {
    Starting,
    Initializing,
    ConnectingWayland,
    InitializingComponents,
    Ready,
    Running,
    Stopping,
    Stopped,
    Failed
};

[[nodiscard]] std::string_view lifecycle_state_name(LifecycleState state) noexcept;

class LifecycleManager {
public:
    using StateChangeCallback = std::function<void(LifecycleState from, LifecycleState to)>;

    LifecycleManager();
    ~LifecycleManager() = default;

    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;

    [[nodiscard]] LifecycleState state() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] bool is_stopped() const noexcept;
    [[nodiscard]] bool is_failed() const noexcept;

    Status transition_to(LifecycleState next_state);

    void add_observer(StateChangeCallback callback);

    [[nodiscard]] static bool is_valid_transition(LifecycleState from, LifecycleState to) noexcept;

private:
    std::atomic<LifecycleState> state_{LifecycleState::Starting};
    mutable std::mutex observer_mutex_;
    std::vector<StateChangeCallback> observers_;
};

} // namespace ldde::core

