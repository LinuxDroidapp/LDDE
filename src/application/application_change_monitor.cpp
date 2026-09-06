#include "ldde/application/application_change_monitor.hpp"
#include "ldde/core/logging.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <system_error>

namespace ldde::application {

ApplicationChangeMonitor::ApplicationChangeMonitor(
    ApplicationDiscovery& discovery, core::EventLoop* event_loop)
    : discovery_(discovery), event_loop_(event_loop) {
}

ApplicationChangeMonitor::~ApplicationChangeMonitor() {
    stop();
}

core::Status ApplicationChangeMonitor::start() {
    if (inotify_fd_ >= 0) {
        return core::Status::ok();
    }

    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd_ < 0) {
        LDDE_LOG_WARN(Application, "Failed to initialize inotify for application monitoring");
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::FilesystemError,
                           "inotify_init1 failed", __FILE__, __LINE__);
    }

    std::error_code ec;
    for (const auto& sdir : discovery_.policy().search_directories()) {
        if (std::filesystem::exists(sdir.path, ec) && std::filesystem::is_directory(sdir.path, ec)) {
            int wd = inotify_add_watch(inotify_fd_, sdir.path.string().c_str(),
                                       IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM | IN_ONLYDIR);
            if (wd >= 0) {
                watch_descriptors_[wd] = sdir.path;
                LDDE_LOG_DEBUG(Application, "Monitoring application directory: " << sdir.path.string());
            } else {
                LDDE_LOG_DEBUG(Application, "Could not watch directory: " << sdir.path.string());
            }
        }
    }

    if (event_loop_) {
        core::Status s = event_loop_->add_fd(
            inotify_fd_, core::FdEvent::Readable,
            [this](int, core::FdEvent) { handle_inotify_event(); });
        if (s.is_error()) {
            stop();
            return s;
        }
    }

    LDDE_LOG_INFO(Application, "ApplicationChangeMonitor started watching " << watch_descriptors_.size() << " directories");
    return core::Status::ok();
}

void ApplicationChangeMonitor::stop() {
    if (inotify_fd_ < 0) return;

    if (event_loop_) {
        event_loop_->remove_fd(inotify_fd_);
    }

    for (const auto& [wd, _] : watch_descriptors_) {
        inotify_rm_watch(inotify_fd_, wd);
    }
    watch_descriptors_.clear();

    close(inotify_fd_);
    inotify_fd_ = -1;
    LDDE_LOG_INFO(Application, "ApplicationChangeMonitor stopped");
}

void ApplicationChangeMonitor::handle_inotify_event() {
    if (inotify_fd_ < 0) return;

    alignas(struct inotify_event) char buffer[4096];
    bool should_refresh = false;

    while (true) {
        ssize_t bytes_read = read(inotify_fd_, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            break;
        }

        ssize_t i = 0;
        while (i < bytes_read) {
            auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            if (event->len > 0) {
                std::string_view name(event->name);
                if (name.ends_with(".desktop")) {
                    should_refresh = true;
                }
            } else {
                should_refresh = true;
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    if (should_refresh) {
        LDDE_LOG_INFO(Application, "Filesystem changes detected in application directories; refreshing catalog");
        core::Status s = discovery_.scan_and_refresh();
        if (s.is_error()) {
            LDDE_LOG_WARN(Application, "Refresh failed: " << s.to_string());
        }
    }
}

} // namespace ldde::application
