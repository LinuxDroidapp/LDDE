#pragma once

#include "ldde/shell/shell_surface.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"
#include <string>

namespace ldde::shell {

class StatusRegion : public ShellSurface {
public:
    StatusRegion();
    ~StatusRegion() override;

    void set_theme(const ShellTheme& theme) { theme_ = theme; }
    void set_tokens(const DesignTokens& tokens) { tokens_ = tokens; }
    void set_clock_text(std::string text) { clock_text_ = std::move(text); }

    void render(ShmBufferPool& pool) override;

private:
    ShellTheme theme_;
    DesignTokens tokens_;
    std::string clock_text_ = "12:00";
};

} // namespace ldde::shell
