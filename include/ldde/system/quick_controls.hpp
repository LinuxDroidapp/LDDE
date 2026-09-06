#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include "ldde/core/types.hpp"
#include "ldde/system/system_data_provider.hpp"

namespace ldde::system {

enum class ControlCapability {
    Available,
    Unavailable,
    Unsupported,
    Error
};

[[nodiscard]] std::string_view control_capability_name(ControlCapability cap) noexcept;

enum class ControlType {
    AudioMute,
    NetworkToggle,
    DisplayInfo,
    SessionAction
};

[[nodiscard]] std::string_view control_type_name(ControlType type) noexcept;

struct QuickControl {
    ControlType type = ControlType::AudioMute;
    std::string id = "audio_mute";
    std::string label = "Audio";
    std::string status_text = "Unmuted";
    bool is_active = false;
    ControlCapability capability = ControlCapability::Available;
    core::Rect geometry{0, 0, 0, 0};
    std::function<void()> action;
};

class QuickControlsManager {
public:
    using ControlsChangedCallback = std::function<void()>;

    explicit QuickControlsManager(SystemDataProvider& data_provider);

    void refresh_controls();

    [[nodiscard]] const std::vector<QuickControl>& controls() const noexcept { return controls_; }
    [[nodiscard]] size_t control_count() const noexcept { return controls_.size(); }

    [[nodiscard]] const QuickControl* control_at(size_t index) const noexcept;
    [[nodiscard]] QuickControl* control_at(size_t index) noexcept;

    // Hit testing
    [[nodiscard]] int32_t hit_test(int32_t x, int32_t y) const noexcept;

    // Selection & Navigation for keyboard
    [[nodiscard]] int32_t selected_index() const noexcept { return selected_index_; }
    void select_next();
    void select_prev();
    void set_selected_index(int32_t index);
    bool activate_selected();
    bool activate_index(size_t index);

    void set_control_geometry(size_t index, const core::Rect& geom);

    void on_changed(ControlsChangedCallback callback);

private:
    void notify_changed();

    SystemDataProvider& data_provider_;
    std::vector<QuickControl> controls_;
    int32_t selected_index_ = 0;
    std::vector<ControlsChangedCallback> callbacks_;
};

} // namespace ldde::system
