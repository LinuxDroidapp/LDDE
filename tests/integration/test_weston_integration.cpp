#include <gtest/gtest.h>
#include "ldde/core/application.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <csignal>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

using namespace ldde::core;

class WestonIntegrationTest : public ::testing::Test {
protected:
    std::string runtime_dir_;
    std::string socket_name_ = "wayland-ldde-int-0";
    pid_t weston_pid_ = -1;

    void SetUp() override {
        // Verify weston binary exists
        if (system("which weston > /dev/null 2>&1") != 0) {
            GTEST_SKIP() << "weston binary not found in PATH; skipping real Weston integration test";
        }

        char template_dir[] = "/tmp/ldde_weston_XXXXXX";
        char* dir = mkdtemp(template_dir);
        ASSERT_NE(dir, nullptr);
        runtime_dir_ = dir;
        chmod(runtime_dir_.c_str(), 0700);

        setenv("XDG_RUNTIME_DIR", runtime_dir_.c_str(), 1);

        // Spawn weston headless
        weston_pid_ = fork();
        if (weston_pid_ == 0) {
            // Child process
            std::string socket_arg = "--socket=" + socket_name_;
            char* const argv[] = {
                const_cast<char*>("weston"),
                const_cast<char*>("--backend=headless"),
                const_cast<char*>(socket_arg.c_str()),
                const_cast<char*>("--idle-time=0"),
                nullptr
            };
            execvp("weston", argv);
            _exit(127);
        }

        ASSERT_GT(weston_pid_, 0);

        // Wait for Wayland socket to appear (up to 5 seconds)
        std::string sock_path = runtime_dir_ + "/" + socket_name_;
        bool socket_ready = false;
        for (int i = 0; i < 50; ++i) {
            struct stat st{};
            if (stat(sock_path.c_str(), &st) == 0 && S_ISSOCK(st.st_mode)) {
                socket_ready = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!socket_ready) {
            kill(weston_pid_, SIGTERM);
            waitpid(weston_pid_, nullptr, 0);
            FAIL() << "Weston socket did not become ready at " << sock_path;
        }
    }

    void TearDown() override {
        if (weston_pid_ > 0) {
            kill(weston_pid_, SIGTERM);
            int status = 0;
            waitpid(weston_pid_, &status, 0);
            weston_pid_ = -1;
        }

        if (!runtime_dir_.empty()) {
            std::filesystem::remove_all(runtime_dir_);
        }
    }
};

TEST_F(WestonIntegrationTest, ConnectDiscoverAndShutdown) {
    int pipefds[2];
    ASSERT_EQ(pipe(pipefds), 0);

    std::string ready_fd_str = std::to_string(pipefds[1]);

    Application app;

    char arg0[] = "ldde";
    char arg1[] = "--wayland-display";
    char* arg2 = const_cast<char*>(socket_name_.c_str());
    char arg3[] = "--ready-fd";
    char* arg4 = const_cast<char*>(ready_fd_str.c_str());
    char arg5[] = "--log-level";
    char arg6[] = "DEBUG";

    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6};
    int argc = 7;

    Status init_status = app.initialize(argc, argv);
    ASSERT_TRUE(init_status.is_ok()) << init_status.to_string();

    // 1. Verify readiness pipe signaled
    char read_buf[8] = {0};
    ssize_t n = read(pipefds[0], read_buf, sizeof(read_buf));
    close(pipefds[0]);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(read_buf[0], '\n');

    // 2. Verify state is Ready
    EXPECT_EQ(app.lifecycle().state(), LifecycleState::Ready);
    EXPECT_TRUE(app.lifecycle().is_ready());

    // 3. Verify Wayland connection
    EXPECT_TRUE(app.wayland_connection().is_connected());
    EXPECT_GE(app.wayland_connection().fd(), 0);

    // 4. Verify registry globals discovery
    EXPECT_TRUE(app.wayland_registry().has_global("wl_compositor"));
    EXPECT_TRUE(app.wayland_registry().has_global("wl_shm"));
    EXPECT_TRUE(app.wayland_registry().has_global("wl_output"));
    EXPECT_TRUE(app.wayland_registry().has_global("wl_seat"));

    // 5. Verify display manager discovered output
    const auto& displays = app.display_manager().displays();
    EXPECT_GE(displays.size(), 1u);
    auto primary = app.display_manager().primary_display();
    ASSERT_TRUE(primary.has_value());
    EXPECT_GT(primary->width, 0);
    EXPECT_GT(primary->height, 0);

    // 6. Verify input manager discovered seat
    EXPECT_NE(app.input_manager().primary_seat(), nullptr);

    // 7. Test running event loop and requesting clean shutdown
    std::thread runner([&app]() {
        // Allow event loop to enter running state
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        app.request_shutdown(0);
    });

    int exit_code = app.run();
    runner.join();

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(app.lifecycle().state(), LifecycleState::Stopped);
    EXPECT_FALSE(app.wayland_connection().is_connected());
}

