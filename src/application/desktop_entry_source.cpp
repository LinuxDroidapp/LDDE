#include "ldde/application/desktop_entry_source.hpp"
#include <system_error>

namespace ldde::application {

std::string_view source_type_name(DesktopEntrySourceType type) noexcept {
    switch (type) {
        case DesktopEntrySourceType::User:   return "User";
        case DesktopEntrySourceType::Local:  return "Local";
        case DesktopEntrySourceType::System: return "System";
        case DesktopEntrySourceType::Custom: return "Custom";
    }
    return "Unknown";
}

DesktopEntrySource::DesktopEntrySource(
    std::filesystem::path path, DesktopEntrySourceType type, int priority)
    : path_(std::move(path)), type_(type), priority_(priority) {
    update_modified_time();
}

void DesktopEntrySource::update_modified_time() {
    std::error_code ec;
    if (!path_.empty() && std::filesystem::exists(path_, ec)) {
        modified_time_ = std::filesystem::last_write_time(path_, ec);
    }
}

} // namespace ldde::application

