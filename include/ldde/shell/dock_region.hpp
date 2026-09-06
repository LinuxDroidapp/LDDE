#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include <functional>

namespace ldde::shell {

class DockRegion : public ShellSurface {
public:
    DockRegion();
    ~DockRegion() override;

    using RenderCallback = std::function<void(ShmBuffer&, const ShellTheme&, const DesignTokens&)>;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void set_tokens(const DesignTokens& tokens) { tokens_ = tokens; }
    void set_slot_count(int count) { slot_count_ = count; }
    void set_render_callback(RenderCallback cb) { render_callback_ = std::move(cb); }

    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
    DesignTokens tokens_;
    int slot_count_ = 4;
    RenderCallback render_callback_;
};

} // namespace ldde::shell
