#pragma once

#include "ldde/core/event_loop.hpp"
#include "ldde/core/error.hpp"
#include "ldde/application/application_discovery.hpp"
#include <unordered_map>
#include <filesystem>

namespace ldde::application {

class ApplicationChangeMonitor {
public:
    ApplicationChangeMonitor(ApplicationDiscovery& discovery, core::EventLoop* event_loop = nullptr);
    ~ApplicationChangeMonitor();

    ApplicationChangeMonitor(const ApplicationChangeMonitor&) = delete;
    ApplicationChangeMonitor& operator=(const ApplicationChangeMonitor&) = delete;

    core::Status start();
    void stop();

    [[nodiscard]] bool is_running() const noexcept { return inotify_fd_ >= 0; }

private:
    void handle_inotify_event();

    ApplicationDiscovery& discovery_;
    core::EventLoop* event_loop_ = nullptr;
    int inotify_fd_ = -1;
    std::unordered_map<int, std::filesystem::path> watch_descriptors_;
};

} // namespace ldde::application

