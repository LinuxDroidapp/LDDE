#include "ldde/system/display_status.hpp"
#include <sstream>

namespace ldde::system {

DisplayStatus::DisplayStatus(const display::DisplayPolicy& policy) {
    update_policy(policy);
}

bool DisplayStatus::update_policy(const display::DisplayPolicy& policy) {
    DisplayStatusInfo new_info;
    const auto& dinfo = policy.display_info();
    new_info.width = dinfo.pixel_width > 0 ? dinfo.pixel_width : dinfo.logical_width;
    new_info.height = dinfo.pixel_height > 0 ? dinfo.pixel_height : dinfo.logical_height;
    new_info.scale = policy.scale_policy().scale_factor();
    new_info.is_portrait = policy.is_portrait();

    new_info.orientation_name = display::orientation_name(policy.orientation());
    new_info.layout_class_name = display::layout_class_name(policy.layout_class());

    std::ostringstream oss;
    oss << new_info.width << "x" << new_info.height << " (" << new_info.orientation_name << ")";
    new_info.status_label = oss.str();

    bool changed = (new_info.width != current_info_.width ||
                    new_info.height != current_info_.height ||
                    new_info.scale != current_info_.scale ||
                    new_info.is_portrait != current_info_.is_portrait ||
                    new_info.orientation_name != current_info_.orientation_name);

    current_info_ = std::move(new_info);

    if (changed) {
        for (const auto& cb : callbacks_) {
            if (cb) cb(current_info_);
        }
    }
    return changed;
}

void DisplayStatus::on_changed(DisplayChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system
