#include "ldde/application/application_icon.hpp"

namespace ldde::application {

ApplicationIconReference::ApplicationIconReference(std::string raw_icon)
    : raw_icon_(std::move(raw_icon)) {
    if (raw_icon_.empty()) {
        type_ = IconType::None;
    } else if (raw_icon_.front() == '/' || raw_icon_.starts_with("./") || raw_icon_.starts_with("../")) {
        type_ = IconType::FilePath;
    } else {
        type_ = IconType::ThemeName;
    }
}

} // namespace ldde::application

