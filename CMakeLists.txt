cmake_minimum_required(VERSION 3.20)
project(LDDE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(GNUInstallDirs)
include(CTest)

# GNUInstallDirs sets SYSCONFDIR to ${prefix}/etc, but when prefix is /usr
# the config must go to /etc (not /usr/etc). Override to follow Debian FHS conventions.
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr" AND NOT CMAKE_INSTALL_SYSCONFDIR)
    set(CMAKE_INSTALL_SYSCONFDIR "/etc"
        CACHE PATH "System configuration directory (override for /usr prefix)" FORCE)
endif()

# Version variables derived from project() — single source of truth
set(LDDE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(LDDE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(LDDE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# Generate version header from template (avoids version string drift)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LDDEVersion.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/ldde/version.hpp
    @ONLY
)


# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the build type" FORCE)
endif()

# Compiler warnings and optimization options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
        -Werror=return-type
        -ffunction-sections
        -fdata-sections
    )
    add_link_options(-Wl,--gc-sections)
endif()

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client>=1.20)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server>=1.20)
pkg_check_modules(CAIRO REQUIRED cairo>=1.16)
pkg_check_modules(GIO REQUIRED gio-2.0 gio-unix-2.0)
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
    src/desktop/desktop_state.cpp
    src/desktop/desktop_background.cpp
    src/desktop/desktop_layout.cpp
    src/desktop/desktop_model.cpp
    src/desktop/desktop_view.cpp
    src/desktop/desktop_controller.cpp
    src/desktop/desktop.cpp
    src/system/clock.cpp
    src/system/network_status.cpp
    src/system/audio_status.cpp
    src/system/battery_status.cpp
    src/system/display_status.cpp
    src/system/session_status.cpp
    src/system/system_data_provider.cpp
    src/system/quick_controls.cpp
    src/system/system_ui_state.cpp
    src/system/system_ui_layout.cpp
    src/system/system_ui_view.cpp
    src/system/system_ui_controller.cpp
    src/system/system_ui.cpp
    src/notification/notification.cpp
    src/notification/notification_store.cpp
    src/notification/internal_notification_backend.cpp
    src/notification/dbus_notification_backend.cpp
    src/notification/notification_center_state.cpp
    src/notification/notification_layout.cpp
    src/notification/notification_view.cpp
    src/notification/notification_presenter.cpp
    src/notification/notification_controller.cpp
    src/notification/notification_manager.cpp
    src/settings/settings_types.cpp
    src/settings/settings_value.cpp
    src/settings/settings_schema.cpp
    src/settings/settings_store.cpp
    src/settings/settings_search.cpp
    src/settings/settings_navigation.cpp
    src/settings/settings_layout.cpp
    src/settings/settings_view.cpp
    src/settings/settings_controller.cpp
    src/settings/settings_manager.cpp
    src/core/application.cpp
)

add_library(ldde_core STATIC ${LDDE_CORE_SOURCES})

target_include_directories(ldde_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
)

target_link_libraries(ldde_core PUBLIC
    ${WAYLAND_CLIENT_LIBRARIES}
    ${WAYLAND_SERVER_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GIO_LIBRARIES}
    Threads::Threads
)

# Executable
add_executable(ldde src/main/main.cpp)
target_link_libraries(ldde PRIVATE ldde_core)

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()


# =============================================================================
# Installation Rules
# =============================================================================

# Binary
install(TARGETS ldde
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Static library (for SDK consumers)
install(TARGETS ldde_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/ldde
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/ldde
)

# Public headers (source tree)
install(DIRECTORY include/ldde
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Generated version header
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/ldde/version.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ldde
)

# Wayland session entry (discovered by LDDM and other display managers)
install(FILES data/ldde.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions
)

# System default configuration (marked as conffile — dpkg preserves admin edits on upgrade)
# Installs as desktop.conf (the real default); package preinst creates it only if absent
install(FILES config/desktop.conf.example
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linuxdroid
    RENAME desktop.conf
)

# Documentation — use Debian-standard path with package name, not project name
set(LDDE_DOCDIR "share/doc/linuxdroid-desktop-environment")
install(FILES README.md
    DESTINATION ${LDDE_DOCDIR}
    RENAME README.md
)
install(FILES LICENSE
    DESTINATION ${LDDE_DOCDIR}
)

# Version manifest (machine-readable, installed alongside binary)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version
    "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}\n"
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version
    DESTINATION ${CMAKE_INSTALL_DATADIR}/linuxdroid/ldde
)

# =============================================================================
# CPack — Debian .deb packaging
# =============================================================================

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "linuxdroid-desktop-environment")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "LinuxDroid Desktop Environment — Wayland shell for LinuxDroid")
set(CPACK_PACKAGE_DESCRIPTION
    "LDDE is a production-grade, minimal, Linux-native Wayland desktop environment \
designed mobile-first for Linux running through LinuxDroid. It provides a complete \
Wayland compositor shell including application launcher, dock, window manager, \
notification center, and settings subsystem.")
set(CPACK_PACKAGE_VENDOR "LinuxDroid")
set(CPACK_PACKAGE_CONTACT "LinuxDroid Team <hello@linuxdroid.dev>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/linuxdroid/ldde")

# Architecture: set to native unless cross-compiling
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
else()
    execute_process(
        COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE _DEB_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_DEB_ARCH)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_DEB_ARCH}")
    else()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    endif()
endif()

set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libwayland-client0 (>= 1.20), libcairo2 (>= 1.16.0), libglib2.0-0 (>= 2.60)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "weston (>= 10.0)")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "linuxdroid-lddm")
set(CPACK_DEBIAN_PACKAGE_SECTION "x11")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Conffiles: dpkg will prompt before overwriting admin-modified config
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/conffiles"
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst"
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm"
)

# Output filename: linuxdroid-desktop-environment_1.0.0_arm64.deb
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

set(CPACK_STRIP_FILES TRUE)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# include(CPack) must come LAST — it reads all CPACK_* variables at include time
include(CPack)
