#include "ldde/application/application_metadata.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include <algorithm>
#include <cctype>

namespace ldde::application {

namespace {

std::string to_lower(std::string_view sv) {
    std::string result;
    result.reserve(sv.size());
    for (char c : sv) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

} // namespace

core::Result<ApplicationMetadata> ApplicationMetadata::from_desktop_entry(
    const ApplicationId& id,
    const DesktopEntry& entry,
    const DesktopEntrySource& source,
    std::string_view locale) {

    if (!entry.is_valid_application()) {
        return core::Error(core::ErrorCategory::Application, core::ErrorCode::DesktopEntryInvalid,
                           "Desktop entry is not a valid application: " + id.value(), __FILE__, __LINE__);
    }

    ApplicationMetadata meta;
    meta.id_ = id;
    meta.source_ = source;

    // Localized strings
    meta.name_ = entry.get_localized_string("Desktop Entry", "Name", locale);
    meta.generic_name_ = entry.get_localized_string("Desktop Entry", "GenericName", locale);
    meta.comment_ = entry.get_localized_string("Desktop Entry", "Comment", locale);

    // Exec field
    meta.exec_ = entry.get_string("Desktop Entry", "Exec");
    ParsedExec pe = DesktopEntryParser::parse_exec(meta.exec_);
    meta.executable_ = std::move(pe.executable);
    meta.exec_args_ = std::move(pe.arguments);
    meta.field_codes_ = std::move(pe.field_codes);

    // Icon
    meta.icon_ = ApplicationIconReference(entry.get_string("Desktop Entry", "Icon"));

    // Booleans
    meta.terminal_ = entry.get_bool("Desktop Entry", "Terminal", false);
    meta.no_display_ = entry.get_bool("Desktop Entry", "NoDisplay", false);
    meta.hidden_ = entry.get_bool("Desktop Entry", "Hidden", false);
    meta.startup_notify_ = entry.get_bool("Desktop Entry", "StartupNotify", false);

    // Lists
    meta.only_show_in_ = entry.get_string_list("Desktop Entry", "OnlyShowIn");
    meta.not_show_in_ = entry.get_string_list("Desktop Entry", "NotShowIn");
    meta.categories_ = entry.get_string_list("Desktop Entry", "Categories");
    meta.mime_types_ = entry.get_string_list("Desktop Entry", "MimeType");
    meta.keywords_ = entry.get_string_list("Desktop Entry", "Keywords");

    // Startup WM Class
    meta.startup_wm_class_ = entry.get_string("Desktop Entry", "StartupWMClass");

    // Actions
    std::vector<std::string> action_names = entry.actions();
    for (const auto& act_name : action_names) {
        std::string act_group = "Desktop Action " + act_name;
        if (entry.has_group(act_group)) {
            ApplicationAction action;
            action.id = act_name;
            action.name = entry.get_localized_string(act_group, "Name", locale);
            action.exec = entry.get_string(act_group, "Exec");
            action.icon = ApplicationIconReference(entry.get_string(act_group, "Icon"));
            meta.actions_.push_back(std::move(action));
        }
    }

    return meta;
}

bool ApplicationMetadata::is_visible_in_desktop(std::string_view desktop_name) const noexcept {
    if (desktop_name.empty()) return true;

    // If OnlyShowIn is specified, desktop_name MUST be in the list
    if (!only_show_in_.empty()) {
        bool found = false;
        for (const auto& d : only_show_in_) {
            if (d == desktop_name) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // If NotShowIn is specified, desktop_name MUST NOT be in the list
    if (!not_show_in_.empty()) {
        for (const auto& d : not_show_in_) {
            if (d == desktop_name) {
                return false;
            }
        }
    }

    return true;
}

bool ApplicationMetadata::is_visible_to_user(std::string_view desktop_name) const noexcept {
    if (hidden_ || no_display_) return false;
    return is_visible_in_desktop(desktop_name);
}

bool ApplicationMetadata::has_category(std::string_view category) const noexcept {
    for (const auto& cat : categories_) {
        if (cat == category) return true;
    }
    return false;
}

bool ApplicationMetadata::matches_search_query(std::string_view query) const noexcept {
    if (query.empty()) return true;
    std::string lq = to_lower(query);

    if (to_lower(name_).find(lq) != std::string::npos) return true;
    if (to_lower(generic_name_).find(lq) != std::string::npos) return true;
    if (to_lower(comment_).find(lq) != std::string::npos) return true;
    if (to_lower(id_.value()).find(lq) != std::string::npos) return true;

    for (const auto& kw : keywords_) {
        if (to_lower(kw).find(lq) != std::string::npos) return true;
    }
    for (const auto& cat : categories_) {
        if (to_lower(cat).find(lq) != std::string::npos) return true;
    }

    return false;
}

} // namespace ldde::application

