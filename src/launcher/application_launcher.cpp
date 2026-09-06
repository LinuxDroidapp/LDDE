#include "ldde/launcher/application_launcher.hpp"
#include "ldde/core/logging.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>

namespace ldde::launcher {

std::string_view launch_status_name(LaunchStatus status) noexcept {
    switch (status) {
        case LaunchStatus::Success:                  return "Success";
        case LaunchStatus::NotFound:                 return "NotFound";
        case LaunchStatus::InvalidMetadata:          return "InvalidMetadata";
        case LaunchStatus::BackendUnavailable:      return "BackendUnavailable";
        case LaunchStatus::ExecutionFailed:          return "ExecutionFailed";
        case LaunchStatus::PermissionFailure:        return "PermissionFailure";
        case LaunchStatus::EnvironmentFailure:       return "EnvironmentFailure";
        case LaunchStatus::UnknownFailure:           return "UnknownFailure";
    }
    return "Unknown";
}

LinuxSessionApplicationLauncher::LinuxSessionApplicationLauncher(std::string terminal_emulator)
    : terminal_emulator_(std::move(terminal_emulator)) {}

std::optional<std::string> LinuxSessionApplicationLauncher::resolve_binary_in_path(std::string_view binary_name) {
    if (binary_name.empty()) return std::nullopt;

    if (binary_name.find('/') != std::string_view::npos) {
        std::string s(binary_name);
        if (access(s.c_str(), X_OK) == 0) {
            return s;
        }
        return std::nullopt;
    }

    const char* path_env = std::getenv("PATH");
    if (!path_env || !*path_env) {
        path_env = "/usr/local/bin:/usr/bin:/bin";
    }

    std::stringstream ss(path_env);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        std::string full_path = dir + "/" + std::string(binary_name);
        if (access(full_path.c_str(), X_OK) == 0) {
            return full_path;
        }
    }

    return std::nullopt;
}

LaunchResult LinuxSessionApplicationLauncher::launch(const LaunchRequest& request) {
    if (request.executable.empty()) {
        LDDE_LOG_WARN(Launcher, "Launch request failed: empty executable name");
        return LaunchResult::failure(LaunchStatus::InvalidMetadata, "Empty executable name");
    }

    std::string binary_to_run;
    auto resolved = resolve_binary_in_path(request.executable);
    if (!resolved) {
        LDDE_LOG_WARN(Launcher, "Executable not found or not executable: " << request.executable);
        return LaunchResult::failure(LaunchStatus::NotFound,
                                     "Executable not found: " + request.executable);
    }
    binary_to_run = *resolved;

    std::vector<std::string> exec_argv;
    if (request.terminal) {
        std::string term = terminal_emulator_;
        auto term_resolved = resolve_binary_in_path(term);
        if (!term_resolved) {
            // Fallback terminal options
            for (const char* candidate : {"xterm", "weston-terminal", "gnome-terminal", "alacritty", "foot"}) {
                term_resolved = resolve_binary_in_path(candidate);
                if (term_resolved) {
                    term = *term_resolved;
                    break;
                }
            }
        }
        if (!term_resolved) {
            LDDE_LOG_WARN(Launcher, "Terminal application requested but no terminal emulator found");
            return LaunchResult::failure(LaunchStatus::EnvironmentFailure,
                                         "No terminal emulator available");
        }

        exec_argv.push_back(term);
        exec_argv.push_back("-e");
        exec_argv.push_back(binary_to_run);
        for (const auto& arg : request.arguments) {
            exec_argv.push_back(arg);
        }
    } else {
        exec_argv.push_back(binary_to_run);
        for (const auto& arg : request.arguments) {
            exec_argv.push_back(arg);
        }
    }

    // Pipe for reporting execvp errors from child to parent
    int pipe_fds[2];
#if defined(__linux__) && defined(O_CLOEXEC)
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
        return LaunchResult::failure(LaunchStatus::ExecutionFailed, "Failed to create error pipe");
    }
#else
    if (pipe(pipe_fds) != 0) {
        return LaunchResult::failure(LaunchStatus::ExecutionFailed, "Failed to create error pipe");
    }
    fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC);
#endif

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        LDDE_LOG_ERROR(Launcher, "fork() failed: " << strerror(errno));
        return LaunchResult::failure(LaunchStatus::ExecutionFailed,
                                     std::string("fork() failed: ") + strerror(errno));
    }

    if (pid == 0) {
        // Child process
        close(pipe_fds[0]);

        // Change directory if requested
        if (!request.working_directory.empty()) {
            if (chdir(request.working_directory.c_str()) != 0) {
                // Not fatal, but logged if possible
            }
        }

        // Set environment
        for (const auto& [k, v] : request.environment) {
            setenv(k.c_str(), v.c_str(), 1);
        }

        // Prepare raw argv
        std::vector<char*> raw_argv;
        raw_argv.reserve(exec_argv.size() + 1);
        for (auto& s : exec_argv) {
            raw_argv.push_back(s.data());
        }
        raw_argv.push_back(nullptr);

        execvp(raw_argv[0], raw_argv.data());

        // If execvp returns, it failed
        int err = errno;
        ssize_t written = write(pipe_fds[1], &err, sizeof(err));
        (void)written;
        close(pipe_fds[1]);
        _exit(127);
    }

    // Parent process
    close(pipe_fds[1]);

    int child_err = 0;
    ssize_t bytes_read = read(pipe_fds[0], &child_err, sizeof(child_err));
    close(pipe_fds[0]);

    if (bytes_read == sizeof(child_err)) {
        // Child failed execvp
        waitpid(pid, nullptr, 0);
        LaunchStatus st = LaunchStatus::ExecutionFailed;
        if (child_err == EACCES) {
            st = LaunchStatus::PermissionFailure;
        } else if (child_err == ENOENT) {
            st = LaunchStatus::NotFound;
        }
        LDDE_LOG_WARN(Launcher, "execvp failed in child with errno=" << child_err
                               << " (" << strerror(child_err) << ")");
        return LaunchResult::failure(st, std::string("Execution failed: ") + strerror(child_err));
    }

    LDDE_LOG_INFO(Launcher, "Launched application '" << request.name << "' (PID " << pid << ")");
    return LaunchResult::success(pid);
}

LaunchResult MockApplicationLauncher::launch(const LaunchRequest& request) {
    launched_requests_.push_back(request);
    if (next_result_) {
        LaunchResult r = *next_result_;
        next_result_.reset();
        return r;
    }
    return LaunchResult::success(static_cast<pid_t>(1000 + launched_requests_.size()));
}

} // namespace ldde::launcher

