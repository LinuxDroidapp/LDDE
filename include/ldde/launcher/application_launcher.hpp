#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "ldde/launcher/launch_request.hpp"
#include "ldde/launcher/launch_result.hpp"

namespace ldde::launcher {

class ApplicationLauncher {
public:
    virtual ~ApplicationLauncher() = default;
    virtual LaunchResult launch(const LaunchRequest& request) = 0;
};

class LinuxSessionApplicationLauncher : public ApplicationLauncher {
public:
    explicit LinuxSessionApplicationLauncher(std::string terminal_emulator = "x-terminal-emulator");
    ~LinuxSessionApplicationLauncher() override = default;

    using BuiltInLaunchHandler = std::function<bool(const LaunchRequest&)>;

    void set_terminal_emulator(std::string terminal_emulator) {
        terminal_emulator_ = std::move(terminal_emulator);
    }

    [[nodiscard]] const std::string& terminal_emulator() const noexcept {
        return terminal_emulator_;
    }

    void register_built_in_handler(std::string app_id, BuiltInLaunchHandler handler) {
        built_in_handlers_[std::move(app_id)] = std::move(handler);
    }

    LaunchResult launch(const LaunchRequest& request) override;

    [[nodiscard]] static std::optional<std::string> resolve_binary_in_path(std::string_view binary_name);

private:
    std::string terminal_emulator_;
    std::unordered_map<std::string, BuiltInLaunchHandler> built_in_handlers_;
};

class MockApplicationLauncher : public ApplicationLauncher {
public:
    MockApplicationLauncher() = default;
    ~MockApplicationLauncher() override = default;

    void set_next_result(LaunchResult result) {
        next_result_ = std::move(result);
    }

    [[nodiscard]] const std::vector<LaunchRequest>& launched_requests() const noexcept {
        return launched_requests_;
    }

    [[nodiscard]] const LaunchRequest* last_request() const noexcept {
        return launched_requests_.empty() ? nullptr : &launched_requests_.back();
    }

    [[nodiscard]] size_t launch_count() const noexcept {
        return launched_requests_.size();
    }

    void clear() {
        launched_requests_.clear();
        next_result_.reset();
    }

    LaunchResult launch(const LaunchRequest& request) override;

private:
    std::vector<LaunchRequest> launched_requests_;
    std::optional<LaunchResult> next_result_;
};

} // namespace ldde::launcher

