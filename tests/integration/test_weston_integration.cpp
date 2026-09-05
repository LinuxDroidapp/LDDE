#include <gtest/gtest.h>
#include "ldde/core/application.hpp"
#include "ldde/window/types.hpp"
#include "ldde/window/window.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/wayland/xdg-shell-client-protocol.h"
#include <poll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <csignal>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

using namespace ldde::core;
using namespace ldde::shell;
using namespace ldde::window;

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

    // 5. Verify display manager discovered output
    const auto& displays = app.display_manager().displays();
    EXPECT_GE(displays.size(), 1u);
    auto primary = app.display_manager().primary_display();
    ASSERT_TRUE(primary.has_value());
    EXPECT_GT(primary->width, 0);
    EXPECT_GT(primary->height, 0);

    // 6. Verify input manager handling
    if (app.wayland_registry().has_global("wl_seat")) {
        EXPECT_NE(app.input_manager().primary_seat(), nullptr);
    }

    // 7. Verify D1 Shell subsystem state
    EXPECT_TRUE(app.shell().is_ready());
    EXPECT_EQ(app.shell().state(), ShellLifecycleState::Ready);

    // 8. Verify Shell surfaces created and sized correctly
    EXPECT_TRUE(app.shell().desktop().is_created());
    EXPECT_EQ(app.shell().desktop().geometry().width, primary->width);
    EXPECT_EQ(app.shell().desktop().geometry().height, primary->height);

    EXPECT_TRUE(app.shell().status_region().is_created());
    EXPECT_EQ(app.shell().status_region().geometry().width, primary->width);
    EXPECT_GT(app.shell().status_region().geometry().height, 0);

    EXPECT_TRUE(app.shell().dock_region().is_created());
    EXPECT_GT(app.shell().dock_region().geometry().width, 0);
    EXPECT_GT(app.shell().dock_region().geometry().height, 0);

    // 9. Verify hit testing routing through shell
    // Top status bar area (x=10, y=10)
    EXPECT_EQ(app.shell().handle_pointer_motion(10, 10), ShellRegionType::Status);
    EXPECT_EQ(app.shell().focused_region(), ShellRegionType::Status);

    // Center desktop area (x = width / 2, y = height / 2)
    EXPECT_EQ(app.shell().handle_pointer_motion(primary->width / 2, primary->height / 2),
              ShellRegionType::Desktop);

    // Dock area (center bottom)
    const auto& dock_rect = app.shell().layout().dock_geometry();
    EXPECT_EQ(app.shell().handle_touch_down(dock_rect.x + dock_rect.width / 2,
                                           dock_rect.y + dock_rect.height / 2),
              ShellRegionType::Dock);

    // 10. Test running event loop and requesting clean shutdown
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

    // 11. Verify deterministic shell cleanup
    EXPECT_EQ(app.shell().state(), ShellLifecycleState::Stopped);
    EXPECT_FALSE(app.shell().desktop().is_created());
    EXPECT_FALSE(app.shell().status_region().is_created());
    EXPECT_FALSE(app.shell().dock_region().is_created());
}

TEST_F(WestonIntegrationTest, WestonTrackedWindowLifecycle) {
    Application app;

    char arg0[] = "ldde";
    char arg1[] = "--wayland-display";
    char* arg2 = const_cast<char*>(socket_name_.c_str());
    char arg3[] = "--log-level";
    char arg4[] = "DEBUG";

    char* argv[] = {arg0, arg1, arg2, arg3, arg4};
    int argc = 5;

    Status init_status = app.initialize(argc, argv);
    ASSERT_TRUE(init_status.is_ok()) << init_status.to_string();

    // 1. Verify xdg_wm_base discovery and version
    EXPECT_TRUE(app.wayland_registry().has_global("xdg_wm_base"));
    EXPECT_TRUE(app.window_tracker().is_initialized());
    EXPECT_NE(app.window_tracker().wm_base(), nullptr);
    EXPECT_GE(app.window_tracker().xdg_wm_base_version(), 1u);

    // 2. Create real tracked Wayland client window under Weston
    auto comp_info = app.wayland_registry().get_global("wl_compositor");
    ASSERT_TRUE(comp_info.has_value());
    wl_compositor* comp = app.wayland_registry().bind<wl_compositor>(
        comp_info->name, &wl_compositor_interface, comp_info->version);
    ASSERT_NE(comp, nullptr);

    wl_surface* client_surf = wl_compositor_create_surface(comp);
    ASSERT_NE(client_surf, nullptr);

    auto tracked = app.window_tracker().create_tracked_window(client_surf, "Weston Client Window", "org.test.WestonApp");
    ASSERT_NE(tracked, nullptr);
    EXPECT_EQ(tracked->title(), "Weston Client Window");
    EXPECT_EQ(tracked->app_id(), "org.test.WestonApp");
    EXPECT_EQ(tracked->lifecycle_state(), WindowLifecycleState::Initializing);
    EXPECT_EQ(app.window_registry().count(), 1u);
    EXPECT_EQ(app.window_registry().lookup(tracked->id()), tracked);

    // 3. Commit surface to trigger Weston's initial configure sequence
    wl_surface_commit(client_surf);
    app.wayland_connection().flush();
    app.wayland_connection().roundtrip();

    EXPECT_TRUE(tracked->is_visible());
    EXPECT_EQ(tracked->lifecycle_state(), WindowLifecycleState::Visible);

    // 4. Destroy tracked window
    app.window_tracker().destroy_window(tracked->id());
    EXPECT_EQ(app.window_registry().count(), 0u);
    EXPECT_EQ(tracked->lifecycle_state(), WindowLifecycleState::Destroyed);

    wl_surface_destroy(client_surf);
    wl_compositor_destroy(comp);

    app.request_shutdown(0);
}

namespace {

struct ExtClientData {
    wl_compositor* compositor = nullptr;
    xdg_wm_base* wm_base = nullptr;
};

const wl_registry_listener ext_reg_listener = {
    .global = [](void* data, wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
        auto* d = static_cast<ExtClientData*>(data);
        if (strcmp(interface, "wl_compositor") == 0) {
            d->compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, id, &wl_compositor_interface, std::min(version, 4u)));
        } else if (strcmp(interface, "xdg_wm_base") == 0) {
            d->wm_base = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, id, &xdg_wm_base_interface, std::min(version, 5u)));
        }
    },
    .global_remove = [](void*, wl_registry*, uint32_t) {}
};

} // namespace

TEST_F(WestonIntegrationTest, RealExternalWaylandAppTracking) {
    Application app;

    char arg0[] = "ldde";
    char arg1[] = "--wayland-display";
    char* arg2 = const_cast<char*>(socket_name_.c_str());
    char arg3[] = "--log-level";
    char arg4[] = "DEBUG";

    char* argv[] = {arg0, arg1, arg2, arg3, arg4};
    int argc = 5;

    Status init_status = app.initialize(argc, argv);
    ASSERT_TRUE(init_status.is_ok()) << init_status.to_string();

    EXPECT_GE(app.window_tracker().server_fd(), 0);
    std::string app_sock = app.window_tracker().application_socket_name();
    EXPECT_FALSE(app_sock.empty());

    std::atomic<bool> client_done = false;
    std::atomic<bool> step_created = false;
    std::atomic<bool> step_unmaximized = false;

    // Run real external Wayland client in separate thread to communicate over Wayland socket
    std::thread client_thread([&]() {
        wl_display* ext_display = wl_display_connect(app_sock.c_str());
        if (!ext_display) return;

        wl_registry* ext_registry = wl_display_get_registry(ext_display);
        ExtClientData ext_data;
        wl_registry_add_listener(ext_registry, &ext_reg_listener, &ext_data);

        wl_display_roundtrip(ext_display);
        wl_display_roundtrip(ext_display);

        if (!ext_data.compositor || !ext_data.wm_base) {
            wl_registry_destroy(ext_registry);
            wl_display_disconnect(ext_display);
            client_done = true;
            return;
        }

        wl_surface* surf = wl_compositor_create_surface(ext_data.compositor);
        xdg_surface* xdg_surf = xdg_wm_base_get_xdg_surface(ext_data.wm_base, surf);
        xdg_toplevel* toplevel = xdg_surface_get_toplevel(xdg_surf);

        xdg_toplevel_set_title(toplevel, "External Wayland Editor");
        xdg_toplevel_set_app_id(toplevel, "org.gnome.TextEditor");
        xdg_toplevel_set_maximized(toplevel);
        xdg_surface_set_window_geometry(xdg_surf, 10, 20, 800, 600);
        wl_surface_commit(surf);
        wl_display_roundtrip(ext_display);

        step_created = true;

        // Wait for main thread check
        for (int i = 0; i < 50 && step_created; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        xdg_toplevel_unset_maximized(toplevel);
        wl_display_roundtrip(ext_display);
        step_unmaximized = true;

        for (int i = 0; i < 50 && step_unmaximized; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        xdg_toplevel_destroy(toplevel);
        xdg_surface_destroy(xdg_surf);
        wl_surface_destroy(surf);
        xdg_wm_base_destroy(ext_data.wm_base);
        wl_compositor_destroy(ext_data.compositor);
        wl_registry_destroy(ext_registry);
        wl_display_roundtrip(ext_display);
        wl_display_disconnect(ext_display);

        client_done = true;
    });

    // 1. Wait for external client creation
    auto start_t = std::chrono::steady_clock::now();
    while (!step_created && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    app.window_tracker().dispatch_server();

    // Verify tracked window properties in registry
    EXPECT_EQ(app.window_registry().count(), 1u);
    auto windows = app.window_registry().windows();
    ASSERT_EQ(windows.size(), 1u);
    auto tracked_win = windows[0];

    EXPECT_EQ(tracked_win->title(), "External Wayland Editor");
    EXPECT_EQ(tracked_win->app_id(), "org.gnome.TextEditor");
    EXPECT_EQ(tracked_win->state(), WindowState::Maximized);
    EXPECT_EQ(tracked_win->geometry(), (ldde::core::Rect{10, 20, 800, 600}));
    EXPECT_TRUE(tracked_win->is_visible());
    EXPECT_EQ(tracked_win->lifecycle_state(), WindowLifecycleState::Visible);

    // 2. Allow unmaximize step
    step_created = false;
    start_t = std::chrono::steady_clock::now();
    while (!step_unmaximized && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    app.window_tracker().dispatch_server();
    EXPECT_EQ(tracked_win->state(), WindowState::Normal);

    // 3. Allow destruction step
    step_unmaximized = false;
    start_t = std::chrono::steady_clock::now();
    while (!client_done && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    app.window_tracker().dispatch_server();

    EXPECT_EQ(app.window_registry().count(), 0u);
    EXPECT_EQ(tracked_win->lifecycle_state(), WindowLifecycleState::Destroyed);

    client_thread.join();
    app.request_shutdown(0);
}

TEST_F(WestonIntegrationTest, RealMultiWindowManagement) {
    Application app;

    char arg0[] = "ldde";
    char arg1[] = "--wayland-display";
    char* arg2 = const_cast<char*>(socket_name_.c_str());
    char arg3[] = "--log-level";
    char arg4[] = "DEBUG";

    char* argv[] = {arg0, arg1, arg2, arg3, arg4};
    int argc = 5;

    Status init_status = app.initialize(argc, argv);
    ASSERT_TRUE(init_status.is_ok()) << init_status.to_string();

    std::string app_sock = app.window_tracker().application_socket_name();
    EXPECT_FALSE(app_sock.empty());

    EXPECT_TRUE(app.window_manager().is_initialized());
    EXPECT_EQ(app.window_registry().count(), 0u);

    // Client context structure
    struct ManagedClientContext {
        wl_display* display = nullptr;
        wl_registry* registry = nullptr;
        ExtClientData ext_data;
        wl_surface* surface = nullptr;
        xdg_surface* xdg_surf = nullptr;
        xdg_toplevel* toplevel = nullptr;
        std::atomic<bool> closed = false;
    };

    const xdg_surface_listener xdg_surf_listener = {
        .configure = [](void*, xdg_surface* surf, uint32_t serial) {
            xdg_surface_ack_configure(surf, serial);
        }
    };

    const xdg_toplevel_listener xdg_top_listener = {
        .configure = [](void*, xdg_toplevel*, int32_t, int32_t, wl_array*) {},
        .close = [](void* data, xdg_toplevel*) {
            auto* ctx = static_cast<ManagedClientContext*>(data);
            ctx->closed = true;
        },
        .configure_bounds = nullptr,
        .wm_capabilities = nullptr,
    };

    std::atomic<bool> client1_ready = false;
    std::atomic<bool> client2_ready = false;
    std::atomic<bool> finish_clients = false;
    std::atomic<bool> c1_done = false;
    std::atomic<bool> c2_done = false;

    // Run two real application clients
    ManagedClientContext c1;
    ManagedClientContext c2;

    std::thread c1_thread([&]() {
        c1.display = wl_display_connect(app_sock.c_str());
        if (!c1.display) return;
        c1.registry = wl_display_get_registry(c1.display);
        wl_registry_add_listener(c1.registry, &ext_reg_listener, &c1.ext_data);
        wl_display_roundtrip(c1.display);
        wl_display_roundtrip(c1.display);

        if (!c1.ext_data.compositor || !c1.ext_data.wm_base) return;

        c1.surface = wl_compositor_create_surface(c1.ext_data.compositor);
        c1.xdg_surf = xdg_wm_base_get_xdg_surface(c1.ext_data.wm_base, c1.surface);
        xdg_surface_add_listener(c1.xdg_surf, &xdg_surf_listener, &c1);

        c1.toplevel = xdg_surface_get_toplevel(c1.xdg_surf);
        xdg_toplevel_add_listener(c1.toplevel, &xdg_top_listener, &c1);
        xdg_toplevel_set_title(c1.toplevel, "Calculator App");
        xdg_toplevel_set_app_id(c1.toplevel, "org.gnome.Calculator");
        wl_surface_commit(c1.surface);
        wl_display_roundtrip(c1.display);

        client1_ready = true;

        while (!finish_clients && !c1.closed) {
            struct pollfd pfd = { wl_display_get_fd(c1.display), POLLIN, 0 };
            if (poll(&pfd, 1, 10) > 0 && (pfd.revents & POLLIN)) {
                wl_display_dispatch(c1.display);
            }
        }

        xdg_toplevel_destroy(c1.toplevel);
        xdg_surface_destroy(c1.xdg_surf);
        wl_surface_destroy(c1.surface);
        xdg_wm_base_destroy(c1.ext_data.wm_base);
        wl_compositor_destroy(c1.ext_data.compositor);
        wl_registry_destroy(c1.registry);
        wl_display_flush(c1.display);
        wl_display_disconnect(c1.display);
        c1_done = true;
    });

    // Wait for client 1
    auto start_t = std::chrono::steady_clock::now();
    while (!client1_ready && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    app.window_tracker().dispatch_server();

    ASSERT_EQ(app.window_registry().count(), 1u);
    auto win1 = app.window_registry().windows()[0];
    EXPECT_EQ(win1->title(), "Calculator App");
    EXPECT_EQ(win1->app_id(), "org.gnome.Calculator");
    EXPECT_EQ(app.window_manager().active_window_id(), win1->id());
    EXPECT_EQ(app.window_manager().top_window_id(), win1->id());

    // Connect client 2
    std::thread c2_thread([&]() {
        c2.display = wl_display_connect(app_sock.c_str());
        if (!c2.display) return;
        c2.registry = wl_display_get_registry(c2.display);
        wl_registry_add_listener(c2.registry, &ext_reg_listener, &c2.ext_data);
        wl_display_roundtrip(c2.display);
        wl_display_roundtrip(c2.display);

        if (!c2.ext_data.compositor || !c2.ext_data.wm_base) return;

        c2.surface = wl_compositor_create_surface(c2.ext_data.compositor);
        c2.xdg_surf = xdg_wm_base_get_xdg_surface(c2.ext_data.wm_base, c2.surface);
        xdg_surface_add_listener(c2.xdg_surf, &xdg_surf_listener, &c2);

        c2.toplevel = xdg_surface_get_toplevel(c2.xdg_surf);
        xdg_toplevel_add_listener(c2.toplevel, &xdg_top_listener, &c2);
        xdg_toplevel_set_title(c2.toplevel, "Calendar App");
        xdg_toplevel_set_app_id(c2.toplevel, "org.gnome.Calendar");
        wl_surface_commit(c2.surface);
        wl_display_roundtrip(c2.display);

        client2_ready = true;

        while (!finish_clients && !c2.closed) {
            struct pollfd pfd = { wl_display_get_fd(c2.display), POLLIN, 0 };
            if (poll(&pfd, 1, 10) > 0 && (pfd.revents & POLLIN)) {
                wl_display_dispatch(c2.display);
            }
        }

        xdg_toplevel_destroy(c2.toplevel);
        xdg_surface_destroy(c2.xdg_surf);
        wl_surface_destroy(c2.surface);
        xdg_wm_base_destroy(c2.ext_data.wm_base);
        wl_compositor_destroy(c2.ext_data.compositor);
        wl_registry_destroy(c2.registry);
        wl_display_flush(c2.display);
        wl_display_disconnect(c2.display);
        c2_done = true;
    });

    // Wait for client 2
    start_t = std::chrono::steady_clock::now();
    while (!client2_ready && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    app.window_tracker().dispatch_server();

    ASSERT_EQ(app.window_registry().count(), 2u);
    auto all_wins = app.window_registry().windows();
    auto win2 = (all_wins[0]->id() == win1->id()) ? all_wins[1] : all_wins[0];
    EXPECT_EQ(win2->title(), "Calendar App");

    // 1. Verify multi-window stacking and focus: win2 should be active and on top
    EXPECT_EQ(app.window_manager().active_window_id(), win2->id());
    EXPECT_EQ(app.window_manager().top_window_id(), win2->id());
    EXPECT_FALSE(win1->is_active());
    EXPECT_TRUE(win2->is_active());

    // 2. Test WindowManager Maximize on win1
    Status max_status = app.window_manager().maximize(win1->id());
    EXPECT_TRUE(max_status.is_ok());
    EXPECT_EQ(win1->state(), WindowState::Maximized);

    // 3. Test WindowManager Minimize on win2 -> Focus automatically falls back to win1
    Status min_status = app.window_manager().minimize(win2->id());
    EXPECT_TRUE(min_status.is_ok());
    EXPECT_EQ(win2->state(), WindowState::Minimized);
    EXPECT_EQ(app.window_manager().active_window_id(), win1->id());

    // 4. Test WindowManager Restore on win2
    Status res_status = app.window_manager().restore(win2->id());
    EXPECT_TRUE(res_status.is_ok());
    EXPECT_EQ(win2->state(), WindowState::Normal);
    EXPECT_TRUE(win2->is_visible());

    // 5. Test WindowManager Close on win1
    Status close_status = app.window_manager().close(win1->id());
    EXPECT_TRUE(close_status.is_ok());

    // Wait for Client 1 to acknowledge close
    start_t = std::chrono::steady_clock::now();
    while (!c1.closed && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_TRUE(c1.closed);

    // Clean finish
    finish_clients = true;
    start_t = std::chrono::steady_clock::now();
    while ((!c1_done || !c2_done) && (std::chrono::steady_clock::now() - start_t < std::chrono::seconds(3))) {
        app.window_tracker().dispatch_server();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    c1_thread.join();
    c2_thread.join();

    app.request_shutdown(0);
}

