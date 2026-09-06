cmake_minimum_required(VERSION 3.20)
project(LDDE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CTest)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
        -Werror=return-type
    )
endif()

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client>=1.20)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server>=1.20)
pkg_check_modules(CAIRO REQUIRED cairo>=1.16)
find_package(Threads REQUIRED)

# Core Library
set(LDDE_CORE_SOURCES
    src/core/error.cpp
    src/core/lifecycle.cpp
    src/core/logging.cpp
    src/core/event_loop.cpp
    src/core/readiness.cpp
    src/config/config.cpp
    src/wayland/connection.cpp
    src/wayland/registry.cpp
    src/wayland/xdg-shell-protocol.c
    src/display/orientation.cpp
    src/display/display_geometry.cpp
    src/display/scale_policy.cpp
    src/display/responsive_layout.cpp
    src/display/layout_metrics.cpp
    src/display/display_policy.cpp
    src/display/display_info.cpp
    src/display/display_manager.cpp
    src/input/seat.cpp
    src/input/input_manager.cpp
    src/input/touch_interaction_policy.cpp
    src/input/touch_gesture_state.cpp
    src/input/touch_hit_testing.cpp
    src/input/window_drag_controller.cpp
    src/input/window_resize_controller.cpp
    src/input/window_control_interaction.cpp
    src/input/touch_interaction_manager.cpp
    src/shell/types.cpp
    src/shell/design_tokens.cpp
    src/shell/theme.cpp
    src/shell/shell_layout.cpp
    src/shell/shm_buffer.cpp
    src/shell/cairo_renderer.cpp
    src/shell/shell_surface.cpp
    src/shell/desktop_surface.cpp
    src/shell/status_region.cpp
    src/shell/dock_region.cpp
    src/shell/shell_overlay.cpp
    src/shell/shell.cpp
    src/window/types.cpp
    src/window/window.cpp
    src/window/window_registry.cpp
    src/window/window_tracker.cpp
    src/window/window_management_backend.cpp
    src/window/window_placement.cpp
    src/window/window_state_controller.cpp
    src/window/window_focus.cpp
    src/window/window_stacking.cpp
    src/window/window_interaction.cpp
    src/window/window_controls.cpp
    src/window/window_manager.cpp
    src/application/application_id.cpp
    src/application/application_icon.cpp
    src/application/desktop_entry_source.cpp
    src/application/desktop_entry.cpp
    src/application/desktop_entry_reader.cpp
    src/application/desktop_entry_parser.cpp
    src/application/application_metadata.cpp
    src/application/application_discovery_policy.cpp
    src/application/application_catalog.cpp
    src/application/application_discovery.cpp
    src/application/application_change_monitor.cpp
    src/launcher/launcher_state.cpp
    src/launcher/application_launcher.cpp
    src/launcher/launcher_icon_resolver.cpp
    src/launcher/launcher_category.cpp
    src/launcher/launcher_item.cpp
    src/launcher/launcher_search.cpp
    src/launcher/launcher_layout.cpp
    src/launcher/launcher_model.cpp
    src/launcher/launcher_view.cpp
    src/launcher/launcher_controller.cpp
    src/launcher/launcher.cpp
    src/dock/dock_state.cpp
    src/dock/dock_item.cpp
    src/dock/dock_layout.cpp
    src/dock/dock_model.cpp
    src/dock/dock_view.cpp
    src/dock/dock_controller.cpp
    src/dock/dock.cpp
    src/switcher/switcher_state.cpp
    src/switcher/switcher_item.cpp
    src/switcher/switcher_mru.cpp
    src/switcher/switcher_model.cpp
    src/switcher/switcher_layout.cpp
    src/switcher/switcher_view.cpp
    src/switcher/switcher_controller.cpp
    src/switcher/switcher.cpp
    src/core/application.cpp
)

add_library(ldde_core STATIC ${LDDE_CORE_SOURCES})

target_include_directories(ldde_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(ldde_core PUBLIC
    ${WAYLAND_CLIENT_LIBRARIES}
    ${WAYLAND_SERVER_LIBRARIES}
    ${CAIRO_LIBRARIES}
    Threads::Threads
)

# Executable
add_executable(ldde src/main/main.cpp)
target_link_libraries(ldde PRIVATE ldde_core)

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Installation rules
install(TARGETS ldde
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS ldde_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ldde
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES config/desktop.conf.example
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linuxdroid
    RENAME desktop.conf.example
)

