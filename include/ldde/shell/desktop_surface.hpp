#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"

namespace ldde::shell {

class DesktopSurface : public ShellSurface {
public:
    DesktopSurface();
    ~DesktopSurface() override;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
};

} // namespace ldde::shell
