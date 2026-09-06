#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"
#include <functional>

namespace ldde::shell {

class ShellOverlay : public ShellSurface {
public:
    ShellOverlay();
    ~ShellOverlay() override;

    using RenderCallback = std::function<void(ShmBuffer&, const ShellTheme&)>;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void set_active(bool active) { is_active_ = active; }
    [[nodiscard]] bool is_active() const noexcept { return is_active_; }
    void set_render_callback(RenderCallback cb) { render_callback_ = std::move(cb); }

    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
    bool is_active_ = false;
    RenderCallback render_callback_;
};

} // namespace ldde::shell
