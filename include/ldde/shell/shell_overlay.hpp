#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"

namespace ldde::shell {

class ShellOverlay : public ShellSurface {
public:
    ShellOverlay();
    ~ShellOverlay() override;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void set_active(bool active) { is_active_ = active; }
    [[nodiscard]] bool is_active() const noexcept { return is_active_; }

    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
    bool is_active_ = false;
};

} // namespace ldde::shell
