#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include "ldde/display/display_policy.hpp"

namespace ldde::system {

struct DisplayStatusInfo {
    int32_t width = 0;
    int32_t height = 0;
    double scale = 1.0;
    bool is_portrait = true;
    std::string orientation_name = "Portrait";
    std::string layout_class_name = "PhonePortrait";
    std::string status_label = "0x0";
};

class DisplayStatus {
public:
    using DisplayChangedCallback = std::function<void(const DisplayStatusInfo&)>;

    DisplayStatus() = default;
    explicit DisplayStatus(const display::DisplayPolicy& policy);

    bool update_policy(const display::DisplayPolicy& policy);
    [[nodiscard]] const DisplayStatusInfo& info() const noexcept { return current_info_; }

    void on_changed(DisplayChangedCallback callback);

private:
    DisplayStatusInfo current_info_;
    std::vector<DisplayChangedCallback> callbacks_;
};

} // namespace ldde::system

