#pragma once

#include <memory>
#include <string>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/wayland/connection.hpp"
#include "ldde/wayland/registry.hpp"
#include "ldde/display/display_manager.hpp"
#include "ldde/shell/types.hpp"
#include "ldde/shell/design_tokens.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/shell_layout.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/desktop_surface.hpp"
#include "ldde/shell/status_region.hpp"
#include "ldde/shell/dock_region.hpp"
#include "ldde/shell/shell_overlay.hpp"

namespace ldde::shell {

using core::Status;

enum class ShellDirtyFlag : uint32_t {
    None = 0,
    Desktop = 1 << 0,
    StatusBar = 1 << 1,
    Dock = 1 << 2,
    Overlay = 1 << 3,
    All = Desktop | StatusBar | Dock | Overlay
};

[[nodiscard]] constexpr ShellDirtyFlag operator|(ShellDirtyFlag a, ShellDirtyFlag b) noexcept {
    return static_cast<ShellDirtyFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

[[nodiscard]] constexpr ShellDirtyFlag operator&(ShellDirtyFlag a, ShellDirtyFlag b) noexcept {
    return static_cast<ShellDirtyFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr ShellDirtyFlag& operator|=(ShellDirtyFlag& a, ShellDirtyFlag b) noexcept {
    a = a | b;
    return a;
}

class Shell {
public:
    Shell();
    ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    Status initialize(wayland::WaylandConnection& connection,
                      wayland::WaylandRegistry& registry,
                      display::DisplayManager& display_manager,
                      const config::Config& config);

    void shutdown() noexcept;

    void update_display(const display::DisplayInfo& info);
    void update_display_policy(const display::DisplayPolicy& policy);

    // Fine-grained dirty rendering
    void mark_dirty(ShellDirtyFlag flags) noexcept;
    void render_dirty();
    void render_desktop();
    void render_status_bar();
    void render_dock();
    void render_overlay();
    void render_all();

    // Input routing & hit testing
    ShellRegionType handle_pointer_motion(int32_t x, int32_t y);
    ShellRegionType handle_touch_down(int32_t x, int32_t y);
    void handle_key(uint32_t key, uint32_t state);

    [[nodiscard]] ShellLifecycleState state() const noexcept { return state_; }
    [[nodiscard]] bool is_ready() const noexcept { return state_ == ShellLifecycleState::Ready || state_ == ShellLifecycleState::Running; }
    [[nodiscard]] const ShellLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] ShellRegionType focused_region() const noexcept { return focused_region_; }

    [[nodiscard]] DesktopSurface& desktop() noexcept { return desktop_; }
    [[nodiscard]] const DesktopSurface& desktop() const noexcept { return desktop_; }
    [[nodiscard]] StatusRegion& status_region() noexcept { return status_region_; }
    [[nodiscard]] const StatusRegion& status_region() const noexcept { return status_region_; }
    [[nodiscard]] DockRegion& dock_region() noexcept { return dock_region_; }
    [[nodiscard]] const DockRegion& dock_region() const noexcept { return dock_region_; }
    [[nodiscard]] ShellOverlay& overlay() noexcept { return overlay_; }
    [[nodiscard]] const ShellOverlay& overlay() const noexcept { return overlay_; }
    [[nodiscard]] const ShellTheme& theme() const noexcept { return theme_; }
    [[nodiscard]] const DesignTokens& tokens() const noexcept { return tokens_; }

private:
    ShellLifecycleState state_ = ShellLifecycleState::Created;
    ShellTheme theme_;
    DesignTokens tokens_;
    ShellLayout layout_;
    ShellRegionType focused_region_ = ShellRegionType::None;

    display::DisplayManager* display_manager_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    wl_subcompositor* subcompositor_ = nullptr;
    wl_shm* shm_ = nullptr;

    std::unique_ptr<ShmBufferPool> buffer_pool_;

    DesktopSurface desktop_;
    StatusRegion status_region_;
    DockRegion dock_region_;
    ShellOverlay overlay_;

    bool status_bar_enabled_ = true;
    bool dock_enabled_ = true;
    DockPosition dock_position_ = DockPosition::Bottom;
    ShellDirtyFlag dirty_flags_ = ShellDirtyFlag::All;

    Status transition_to(ShellLifecycleState next_state);
    Status create_surfaces();
};

} // namespace ldde::shell
