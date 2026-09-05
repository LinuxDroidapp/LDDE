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
        -Wnon-virtual-dtor
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
    src/display/display_info.cpp
    src/display/display_manager.cpp
    src/input/seat.cpp
    src/input/input_manager.cpp
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

