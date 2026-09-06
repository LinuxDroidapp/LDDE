#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include <string>
#include <functional>

namespace ldde::shell {

class StatusRegion : public ShellSurface {
public:
    using RenderCallback = std::function<void(ShmBuffer&, const ShellTheme&, const DesignTokens&)>;

    StatusRegion();
    ~StatusRegion() override;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void set_tokens(const DesignTokens& tokens) { tokens_ = tokens; }
    void set_clock_text(std::string text) { clock_text_ = std::move(text); }
    void set_render_callback(RenderCallback callback) { render_callback_ = std::move(callback); }

    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
    DesignTokens tokens_;
    std::string clock_text_ = "12:00";
    RenderCallback render_callback_;
};

} // namespace ldde::shell
