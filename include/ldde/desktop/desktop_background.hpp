#pragma once

#include "ldde/shell/theme.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_policy.hpp"
#include <string>

namespace ldde::desktop {

enum class DesktopBackgroundMode {
    Gradient,
    Solid
};

class DesktopBackground {
public:
    DesktopBackground();
    ~DesktopBackground() = default;

    void load_config(const config::Config& config);

    [[nodiscard]] DesktopBackgroundMode mode() const noexcept { return mode_; }
    void set_mode(DesktopBackgroundMode mode) noexcept { mode_ = mode; }

    [[nodiscard]] const shell::Color& color_top() const noexcept { return color_top_; }
    void set_color_top(const shell::Color& color) noexcept { color_top_ = color; }

    [[nodiscard]] const shell::Color& color_bottom() const noexcept { return color_bottom_; }
    void set_color_bottom(const shell::Color& color) noexcept { color_bottom_ = color; }

    [[nodiscard]] bool ambient_glow_enabled() const noexcept { return ambient_glow_; }
    void set_ambient_glow_enabled(bool enabled) noexcept { ambient_glow_ = enabled; }

    void render(shell::ShmBuffer& buffer, double scale = 1.0);

private:
    DesktopBackgroundMode mode_ = DesktopBackgroundMode::Gradient;
    shell::Color color_top_{0.07, 0.09, 0.15, 1.0};      // #121826
    shell::Color color_bottom_{0.04, 0.05, 0.08, 1.0};   // #0a0d14
    bool ambient_glow_ = true;
};

} // namespace ldde::desktop
