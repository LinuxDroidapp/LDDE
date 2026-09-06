#include "ldde/application/application_id.hpp"
#include <algorithm>

namespace ldde::application {

ApplicationId::ApplicationId(std::string id)
    : id_(std::move(id)) {
}

ApplicationId ApplicationId::from_desktop_filename(std::string_view filename) {
    return ApplicationId(std::string(filename));
}

ApplicationId ApplicationId::from_relative_path(const std::filesystem::path& relative_path) {
    // According to Desktop Entry Specification:
    // If the desktop file is in a subdirectory of $XDG_DATA_DIRS/applications/,
    // the desktop file ID is constructed by joining the subdirectory path elements
    // and the file name with a hyphen (-).
    // e.g. "foo/bar.desktop" -> "foo-bar.desktop"
    std::string result;
    for (auto it = relative_path.begin(); it != relative_path.end(); ++it) {
        if (!result.empty()) {
            result.push_back('-');
        }
        result += it->string();
    }
    return ApplicationId(std::move(result));
}

bool ApplicationId::is_valid() const noexcept {
    if (id_.size() <= 8) return false; // Must be longer than ".desktop"
    if (!id_.ends_with(".desktop")) return false;
    // Cannot contain path separators
    if (id_.find('/') != std::string::npos || id_.find('\\') != std::string::npos) {
        return false;
    }
    return true;
}

std::string ApplicationId::basename_without_extension() const {
    if (id_.ends_with(".desktop")) {
        return id_.substr(0, id_.size() - 8);
    }
    return id_;
}

} // namespace ldde::application

