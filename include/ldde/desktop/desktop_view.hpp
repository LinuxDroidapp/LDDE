#pragma once

#include "ldde/shell/shm_buffer.hpp"
#include "ldde/desktop/desktop_background.hpp"
#include "ldde/desktop/desktop_layout.hpp"
#include "ldde/desktop/desktop_model.hpp"
#include "ldde/config/config.hpp"

namespace ldde::desktop {

class DesktopView {
public:
    DesktopView();
    ~DesktopView() = default;

    void load_config(const config::Config& config);

    [[nodiscard]] bool show_empty_hint() const noexcept { return show_empty_hint_; }
    void set_show_empty_hint(bool show) noexcept { show_empty_hint_ = show; }

    void render(shell::ShmBuffer& buffer,
                DesktopBackground& background,
                const DesktopLayout& layout,
                const DesktopModel& model);

private:
    bool show_empty_hint_ = true;
};

} // namespace ldde::desktop
