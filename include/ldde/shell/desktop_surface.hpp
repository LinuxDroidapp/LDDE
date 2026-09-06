#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"
#include <functional>

namespace ldde::shell {

class DesktopSurface : public ShellSurface {
public:
    using RenderCallback = std::function<void(ShmBuffer&, const ShellTheme&)>;

    DesktopSurface();
    ~DesktopSurface() override;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void set_render_callback(RenderCallback callback) { render_callback_ = std::move(callback); }
    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
    RenderCallback render_callback_;
};

} // namespace ldde::shell
