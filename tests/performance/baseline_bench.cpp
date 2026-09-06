#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

#include "ldde/core/application.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/window/window_tracker.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/launcher/launcher_search.hpp"
#include "ldde/notification/notification_manager.hpp"
#include "ldde/notification/notification_store.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_store.hpp"
#include "ldde/config/config.hpp"

using namespace ldde;
using Clock = std::chrono::steady_clock;

struct ResourceStats {
    long rss_kb = 0;
    long vm_kb = 0;
    int fd_count = 0;
    double user_cpu_ms = 0.0;
    double sys_cpu_ms = 0.0;
};

ResourceStats get_current_resources() {
    ResourceStats stats;
    // 1. Memory from /proc/self/statm
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        long pages_vm = 0, pages_rss = 0;
        statm >> pages_vm >> pages_rss;
        long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
        stats.vm_kb = pages_vm * page_size_kb;
        stats.rss_kb = pages_rss * page_size_kb;
    }

    // 2. FDs from /proc/self/fd
    DIR* dir = opendir("/proc/self/fd");
    if (dir) {
        int count = 0;
        while (readdir(dir)) count++;
        stats.fd_count = count - 2; // exclude . and ..
        closedir(dir);
    }

    // 3. CPU from getrusage
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        stats.user_cpu_ms = static_cast<double>(ru.ru_utime.tv_sec) * 1000.0 + static_cast<double>(ru.ru_utime.tv_usec) / 1000.0;
        stats.sys_cpu_ms = static_cast<double>(ru.ru_stime.tv_sec) * 1000.0 + static_cast<double>(ru.ru_stime.tv_usec) / 1000.0;
    }

    return stats;
}

static application::ApplicationMetadata make_test_app(int index) {
    std::string content = "[Desktop Entry]\nType=Application\n";
    content += "Name=Application " + std::to_string(index) + "\n";
    content += "GenericName=Generic Utility " + std::to_string(index) + "\n";
    content += "Exec=app" + std::to_string(index) + "\n";
    content += "Icon=utilities-terminal\n";
    content += "Comment=A powerful mobile tool for testing performance and responsiveness " + std::to_string(index) + "\n";
    content += "Categories=Utility;Development;System;\n";
    content += "Keywords=terminal;console;command;tool;editor;\n";

    auto parsed = application::DesktopEntryParser::parse(content);
    application::DesktopEntrySource src(std::filesystem::path("/usr/share/applications/app" + std::to_string(index) + ".desktop"),
                                        application::DesktopEntrySourceType::System, 10);
    auto meta_res = application::ApplicationMetadata::from_desktop_entry(
        application::ApplicationId("org.example.app" + std::to_string(index)),
        parsed.value(),
        src
    );
    return meta_res.value();
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "         LDDE Performance & UX Benchmark Harness        \n";
    std::cout << "========================================================\n";

    ResourceStats initial_res = get_current_resources();
    std::cout << "Initial Memory RSS: " << initial_res.rss_kb << " KB, VM: " << initial_res.vm_kb << " KB\n";
    std::cout << "Initial Open FDs:   " << initial_res.fd_count << "\n";
    std::cout << "--------------------------------------------------------\n";

    // 1. Display Policy Recalculation & Layout Responsiveness
    {
        display::DisplayInfo portrait_info;
        portrait_info.id = 1;
        portrait_info.name = "DSI-1";
        portrait_info.width = 1080;
        portrait_info.height = 2400;
        portrait_info.pixel_width = 1080;
        portrait_info.pixel_height = 2400;
        portrait_info.logical_width = 1080;
        portrait_info.logical_height = 2400;
        portrait_info.physical_width_mm = 68;
        portrait_info.physical_height_mm = 152;
        portrait_info.safe_insets = core::Insets{48, 0, 96, 0};

        display::DisplayInfo landscape_info;
        landscape_info.id = 1;
        landscape_info.name = "DSI-1";
        landscape_info.width = 2400;
        landscape_info.height = 1080;
        landscape_info.pixel_width = 2400;
        landscape_info.pixel_height = 1080;
        landscape_info.logical_width = 2400;
        landscape_info.logical_height = 1080;
        landscape_info.physical_width_mm = 152;
        landscape_info.physical_height_mm = 68;
        landscape_info.safe_insets = core::Insets{0, 48, 0, 96};

        constexpr int kPolicyIterations = 10000;
        auto t0 = Clock::now();
        for (int i = 0; i < kPolicyIterations; ++i) {
            display::DisplayPolicy policy((i % 2 == 0) ? portrait_info : landscape_info);
            volatile auto orient = policy.orientation();
            volatile auto scale = policy.scale_policy().effective_scale();
            volatile auto work = policy.available_window_geometry();
            (void)orient; (void)scale; (void)work;
        }
        auto t1 = Clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kPolicyIterations;
        std::cout << "[DisplayPolicy] Recalculation & Work Area: " << std::fixed << std::setprecision(3)
                  << elapsed_us << " µs per switch (" << kPolicyIterations << " iterations)\n";
    }

    // 2. Application Catalog & Launcher Search Benchmark
    {
        std::vector<application::ApplicationMetadata> apps;
        apps.reserve(200);
        for (int i = 0; i < 200; ++i) {
            apps.push_back(make_test_app(i));
        }

        // Benchmark empty search
        constexpr int kSearchIters = 1000;
        auto t0 = Clock::now();
        for (int i = 0; i < kSearchIters; ++i) {
            auto res = launcher::LauncherSearch::search(apps, "");
            (void)res;
        }
        auto t1 = Clock::now();
        double empty_search_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kSearchIters;

        // Benchmark prefix search
        t0 = Clock::now();
        for (int i = 0; i < kSearchIters; ++i) {
            auto res = launcher::LauncherSearch::search(apps, "app");
            (void)res;
        }
        t1 = Clock::now();
        double prefix_search_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kSearchIters;

        // Benchmark keyword search
        t0 = Clock::now();
        for (int i = 0; i < kSearchIters; ++i) {
            auto res = launcher::LauncherSearch::search(apps, "editor");
            (void)res;
        }
        t1 = Clock::now();
        double keyword_search_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kSearchIters;

        std::cout << "[LauncherSearch] Empty query (200 apps):    " << empty_search_us << " µs\n";
        std::cout << "[LauncherSearch] Prefix query 'app':        " << prefix_search_us << " µs\n";
        std::cout << "[LauncherSearch] Keyword query 'editor':    " << keyword_search_us << " µs\n";
    }

    // 3. Window Manager & Registry Hit Testing and Interaction
    {
        window::WindowRegistry registry;
        window::WindowTracker tracker;
        display::DisplayManager display_mgr;
        display::DisplayInfo dinfo;
        dinfo.id = 1;
        dinfo.name = "WL-1";
        dinfo.width = 1080;
        dinfo.height = 2400;
        dinfo.pixel_width = 1080;
        dinfo.pixel_height = 2400;
        dinfo.logical_width = 1080;
        dinfo.logical_height = 2400;
        display_mgr.register_synthetic_display(dinfo);

        config::Config cfg;
        window::WindowManager wm(registry, tracker, display_mgr, nullptr);
        wm.initialize(cfg);

        // Create 20 windows
        for (uint32_t i = 1; i <= 20; ++i) {
            auto win = std::make_shared<window::Window>(i, nullptr, nullptr, nullptr);
            win->set_app_id("app.test." + std::to_string(i));
            win->set_title("Window " + std::to_string(i));
            win->set_geometry(core::Rect{static_cast<int32_t>(i * 20), static_cast<int32_t>(i * 30), 600, 800});
            registry.add_window(win);
            win->set_visible(true);
        }

        // Benchmark hit testing
        constexpr int kHitIters = 10000;
        auto t0 = Clock::now();
        for (int i = 0; i < kHitIters; ++i) {
            core::Point pt{100 + (i % 500), 200 + (i % 600)};
            wm.handle_touch_tap(pt, static_cast<uint32_t>(i));
        }
        auto t1 = Clock::now();
        double hit_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kHitIters;
        std::cout << "[WindowManager] Touch Tap & Hit Test (20 wins): " << hit_us << " µs per event\n";

        // Benchmark window placement
        constexpr int kPlaceIters = 5000;
        t0 = Clock::now();
        for (int i = 0; i < kPlaceIters; ++i) {
            auto w = registry.lookup(20);
            if (w) {
                w->set_geometry(core::Rect{100 + (i % 200), 200 + (i % 200), 600, 800});
            }
        }
        t1 = Clock::now();
        double place_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kPlaceIters;
        std::cout << "[WindowManager] Geometry Update Responsiveness: " << place_us << " µs per update\n";

        wm.shutdown();
    }

    // 4. Notification Store & Flood Throughput
    {
        notification::NotificationStore store(50, 100);
        constexpr int kNotifBurst = 500;
        auto t0 = Clock::now();
        for (uint32_t i = 1; i <= kNotifBurst; ++i) {
            notification::Notification n(
                i,
                "app.flood",
                "Flood Title " + std::to_string(i),
                "Flood Body content for stress testing notifications " + std::to_string(i),
                "dialog-information",
                notification::NotificationUrgency::Normal,
                5000,
                0
            );
            store.add_or_replace(n);
        }
        auto t1 = Clock::now();
        double notif_add_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kNotifBurst;
        std::cout << "[NotificationStore] Flood Insertion (500 burst): " << notif_add_us << " µs per notification\n";
        std::cout << "[NotificationStore] Bounded Active Count: " << store.active_count()
                  << " (max 50), History: " << store.history_count() << " (max 100)\n";
    }

    // 5. Settings Store Transactions & Query Throughput
    {
        config::Config cfg;
        settings::SettingsSchema schema = settings::SettingsSchema::create_default_schema();
        settings::SettingsStore store(cfg, schema);

        // Benchmark in-memory set & notify (no disk I/O)
        constexpr int kSettingsIters = 10000;
        auto t0 = Clock::now();
        for (int i = 0; i < kSettingsIters; ++i) {
            store.set("dock.item_size", settings::SettingsValue(static_cast<int64_t>(48 + (i % 32))), false);
        }
        auto t1 = Clock::now();
        double settings_set_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kSettingsIters;
        std::cout << "[SettingsStore] Value Set & Notify (In-Memory):" << settings_set_us << " µs per update\n";

        // Benchmark persistent write & fsync latency (50 iters)
        constexpr int kPersistIters = 50;
        t0 = Clock::now();
        for (int i = 0; i < kPersistIters; ++i) {
            store.set("dock.item_size", settings::SettingsValue(static_cast<int64_t>(48 + (i % 32))), true);
        }
        t1 = Clock::now();
        double persist_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / kPersistIters;
        std::cout << "[SettingsStore] Value Set & Disk Sync (Atomic):" << persist_ms << " ms per sync\n";

        // Transaction benchmark with disk sync (50 iters)
        constexpr int kTxIters = 50;
        t0 = Clock::now();
        for (int i = 0; i < kTxIters; ++i) {
            store.begin_transaction();
            store.set("dock.enabled", settings::SettingsValue(i % 2 == 0));
            store.set("dock.item_size", settings::SettingsValue(static_cast<int64_t>(64)));
            store.commit(true);
        }
        t1 = Clock::now();
        double tx_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / kTxIters;
        std::cout << "[SettingsStore] 2-Key Transaction Commit (Sync):" << tx_ms << " ms per transaction\n";
    }

    // 6. Real Weston Headless Integration (Startup, Surface Creation, Presentation, Shutdown)
    {
        char template_dir[] = "/tmp/ldde_perf_XXXXXX";
        char* dir = mkdtemp(template_dir);
        if (dir) {
            std::string runtime_dir = dir;
            chmod(runtime_dir.c_str(), 0700);
            std::string socket_name = "wayland-ldde-perf-0";

            setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);

            pid_t weston_pid = fork();
            if (weston_pid == 0) {
                std::string sock_arg = "--socket=" + socket_name;
                char* const argv[] = {
                    const_cast<char*>("weston"),
                    const_cast<char*>("--backend=headless"),
                    const_cast<char*>(sock_arg.c_str()),
                    const_cast<char*>("--idle-time=0"),
                    nullptr
                };
                execvp("weston", argv);
                _exit(127);
            }

            std::string sock_path = runtime_dir + "/" + socket_name;
            bool socket_ready = false;
            for (int i = 0; i < 50; ++i) {
                struct stat st{};
                if (stat(sock_path.c_str(), &st) == 0 && S_ISSOCK(st.st_mode)) {
                    socket_ready = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (socket_ready) {
                std::cout << "--------------------------------------------------------\n";
                std::cout << "Running Weston Headless Client Benchmark...\n";

                core::Application app;
                char arg0[] = "ldde";
                char arg1[] = "--wayland-display";
                char* arg2 = const_cast<char*>(socket_name.c_str());
                char* argv[] = {arg0, arg1, arg2};
                int argc = 3;

                auto t_init_start = Clock::now();
                core::Status s = app.initialize(argc, argv);
                auto t_init_end = Clock::now();

                double init_ms = std::chrono::duration<double, std::milli>(t_init_end - t_init_start).count();
                std::cout << "[Wayland/Shell] App & Wayland Init Latency: " << init_ms << " ms (Status: "
                          << (s.is_ok() ? "OK" : s.to_string()) << ")\n";

                // Measure memory after shell initialized
                ResourceStats post_init_res = get_current_resources();
                std::cout << "[Wayland/Shell] Post-Init RSS Memory:       " << post_init_res.rss_kb << " KB\n";
                std::cout << "[Wayland/Shell] Post-Init Open FDs:         " << post_init_res.fd_count << "\n";

                // Measure render_all / presentation latency
                auto t_render_start = Clock::now();
                app.shell().render_all();
                auto t_render_end = Clock::now();
                double render_all_ms = std::chrono::duration<double, std::milli>(t_render_end - t_render_start).count();
                std::cout << "[Wayland/Shell] Full render_all() Latency:  " << render_all_ms << " ms\n";

                // Measure 50 repeated render_all calls to observe memory and time
                auto t_rep_start = Clock::now();
                for (int i = 0; i < 50; ++i) {
                    app.shell().render_all();
                }
                auto t_rep_end = Clock::now();
                double avg_render_ms = std::chrono::duration<double, std::milli>(t_rep_end - t_rep_start).count() / 50.0;
                std::cout << "[Wayland/Shell] 50x render_all() Average:   " << avg_render_ms << " ms per redraw\n";

                ResourceStats post_render_res = get_current_resources();
                std::cout << "[Wayland/Shell] Post-Render RSS Memory:     " << post_render_res.rss_kb << " KB\n";

                // Measure clean shutdown latency
                auto t_shut_start = Clock::now();
                app.request_shutdown(0);
                auto t_shut_end = Clock::now();
                double shut_ms = std::chrono::duration<double, std::milli>(t_shut_end - t_shut_start).count();
                std::cout << "[Wayland/Shell] Clean Shutdown Latency:     " << shut_ms << " ms\n";

                ResourceStats post_shut_res = get_current_resources();
                std::cout << "[Wayland/Shell] Post-Shutdown Open FDs:     " << post_shut_res.fd_count << "\n";
            }

            if (weston_pid > 0) {
                kill(weston_pid, SIGTERM);
                int status = 0;
                waitpid(weston_pid, &status, 0);
            }
            std::filesystem::remove_all(runtime_dir);
        }
    }

    ResourceStats final_res = get_current_resources();
    std::cout << "--------------------------------------------------------\n";
    std::cout << "Final Total RSS Memory: " << final_res.rss_kb << " KB (Delta: +" << (final_res.rss_kb - initial_res.rss_kb) << " KB)\n";
    std::cout << "Final Total Open FDs:   " << final_res.fd_count << " (Initial: " << initial_res.fd_count << ")\n";
    std::cout << "Total CPU User: " << final_res.user_cpu_ms << " ms, Sys: " << final_res.sys_cpu_ms << " ms\n";
    std::cout << "========================================================\n";

    return 0;
}
