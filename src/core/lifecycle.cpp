#include "ldde/core/lifecycle.hpp"

namespace ldde::core {

std::string_view lifecycle_state_name(LifecycleState state) noexcept {
    switch (state) {
        case LifecycleState::Starting:               return "STARTING";
        case LifecycleState::Initializing:           return "INITIALIZING";
        case LifecycleState::ConnectingWayland:       return "CONNECTING_WAYLAND";
        case LifecycleState::InitializingComponents: return "INITIALIZING_COMPONENTS";
        case LifecycleState::Ready:                  return "READY";
        case LifecycleState::Running:                return "RUNNING";
        case LifecycleState::Stopping:               return "STOPPING";
        case LifecycleState::Stopped:                return "STOPPED";
        case LifecycleState::Failed:                 return "FAILED";
    }
    return "UNKNOWN";
}

LifecycleManager::LifecycleManager()
    : state_(LifecycleState::Starting) {}

LifecycleState LifecycleManager::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

bool LifecycleManager::is_ready() const noexcept {
    auto s = state();
    return s == LifecycleState::Ready || s == LifecycleState::Running;
}

bool LifecycleManager::is_running() const noexcept {
    return state() == LifecycleState::Running;
}

bool LifecycleManager::is_stopped() const noexcept {
    return state() == LifecycleState::Stopped;
}

bool LifecycleManager::is_failed() const noexcept {
    return state() == LifecycleState::Failed;
}

bool LifecycleManager::is_valid_transition(LifecycleState from, LifecycleState to) noexcept {
    if (from == to) {
        return true;
    }

    // Stopped is terminal
    if (from == LifecycleState::Stopped) {
        return false;
    }

    // Any non-stopped state can fail
    if (to == LifecycleState::Failed) {
        return true;
    }

    // Any state can transition to Stopping, except Failed (which goes to Stopped) and Stopped
    if (to == LifecycleState::Stopping) {
        return from != LifecycleState::Failed && from != LifecycleState::Stopped;
    }

    // Stopped can be reached from Stopping or Failed
    if (to == LifecycleState::Stopped) {
        return from == LifecycleState::Stopping || from == LifecycleState::Failed;
    }

    // Normal forward progression
    switch (from) {
        case LifecycleState::Starting:
            return to == LifecycleState::Initializing;
        case LifecycleState::Initializing:
            return to == LifecycleState::ConnectingWayland;
        case LifecycleState::ConnectingWayland:
            return to == LifecycleState::InitializingComponents;
        case LifecycleState::InitializingComponents:
            return to == LifecycleState::Ready;
        case LifecycleState::Ready:
            return to == LifecycleState::Running;
        case LifecycleState::Running:
        case LifecycleState::Stopping:
        case LifecycleState::Stopped:
        case LifecycleState::Failed:
            return false;
    }

    return false;
}

Status LifecycleManager::transition_to(LifecycleState next_state) {
    LifecycleState current = state_.load(std::memory_order_acquire);

    if (current == next_state) {
        return Status::ok();
    }

    if (!is_valid_transition(current, next_state)) {
        return LDDE_STATUS_ERROR(
            ErrorCategory::Application,
            ErrorCode::InvalidLifecycleTransition,
            std::string("Illegal lifecycle transition from ") +
                std::string(lifecycle_state_name(current)) + " to " +
                std::string(lifecycle_state_name(next_state)));
    }

    state_.store(next_state, std::memory_order_release);

    std::vector<StateChangeCallback> callbacks_copy;
    {
        std::lock_guard<std::mutex> lock(observer_mutex_);
        callbacks_copy = observers_;
    }

    for (const auto& cb : callbacks_copy) {
        if (cb) {
            cb(current, next_state);
        }
    }

    return Status::ok();
}

void LifecycleManager::add_observer(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(observer_mutex_);
    observers_.push_back(std::move(callback));
}

} // namespace ldde::core

