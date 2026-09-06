#include "ldde/application/application_catalog.hpp"
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

bool compare_applications_deterministic(const ApplicationMetadata& a, const ApplicationMetadata& b) {
    std::string name_a = to_lower(a.name());
    std::string name_b = to_lower(b.name());
    if (name_a != name_b) {
        return name_a < name_b;
    }
    return a.id().value() < b.id().value();
}

bool has_metadata_changed(const ApplicationMetadata& a, const ApplicationMetadata& b) {
    if (a.name() != b.name()) return true;
    if (a.generic_name() != b.generic_name()) return true;
    if (a.comment() != b.comment()) return true;
    if (a.exec() != b.exec()) return true;
    if (a.icon() != b.icon()) return true;
    if (a.hidden() != b.hidden()) return true;
    if (a.no_display() != b.no_display()) return true;
    if (a.terminal() != b.terminal()) return true;
    if (a.source().path() != b.source().path()) return true;
    if (a.source().modified_time() != b.source().modified_time()) return true;
    return false;
}

} // namespace

std::vector<ApplicationMetadata> ApplicationCatalog::all() const {
    std::vector<ApplicationMetadata> result;
    result.reserve(applications_.size());
    for (const auto& [_, meta] : applications_) {
        result.push_back(meta);
    }
    std::sort(result.begin(), result.end(), compare_applications_deterministic);
    return result;
}

std::vector<ApplicationMetadata> ApplicationCatalog::visible_applications(std::string_view desktop_name) const {
    std::vector<ApplicationMetadata> result;
    for (const auto& [_, meta] : applications_) {
        if (meta.is_visible_to_user(desktop_name)) {
            result.push_back(meta);
        }
    }
    std::sort(result.begin(), result.end(), compare_applications_deterministic);
    return result;
}

const ApplicationMetadata* ApplicationCatalog::find(const ApplicationId& id) const noexcept {
    auto it = applications_.find(id);
    if (it != applications_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ApplicationCatalog::contains(const ApplicationId& id) const noexcept {
    return applications_.contains(id);
}

size_t ApplicationCatalog::visible_count(std::string_view desktop_name) const {
    size_t count = 0;
    for (const auto& [_, meta] : applications_) {
        if (meta.is_visible_to_user(desktop_name)) {
            count++;
        }
    }
    return count;
}

std::vector<ApplicationMetadata> ApplicationCatalog::search(
    std::string_view query, std::string_view desktop_name) const {
    std::vector<ApplicationMetadata> result;
    for (const auto& [_, meta] : applications_) {
        if (meta.is_visible_to_user(desktop_name) && meta.matches_search_query(query)) {
            result.push_back(meta);
        }
    }
    std::sort(result.begin(), result.end(), compare_applications_deterministic);
    return result;
}

CatalogDiff ApplicationCatalog::update_applications(std::vector<ApplicationMetadata> new_applications) {
    CatalogDiff diff;
    std::unordered_map<ApplicationId, ApplicationMetadata> new_map;
    new_map.reserve(new_applications.size());

    for (auto& meta : new_applications) {
        new_map.emplace(meta.id(), std::move(meta));
    }

    // Identify added and changed
    for (const auto& [id, new_meta] : new_map) {
        auto old_it = applications_.find(id);
        if (old_it == applications_.end()) {
            diff.added.push_back(new_meta);
        } else if (has_metadata_changed(old_it->second, new_meta)) {
            diff.changed.push_back(new_meta);
        }
    }

    // Identify removed
    for (const auto& [id, old_meta] : applications_) {
        if (!new_map.contains(id)) {
            diff.removed.push_back(old_meta);
        }
    }

    applications_ = std::move(new_map);

    // Notify subscribers
    if (on_added_) {
        for (const auto& added_meta : diff.added) {
            on_added_(added_meta);
        }
    }
    if (on_removed_) {
        for (const auto& removed_meta : diff.removed) {
            on_removed_(removed_meta);
        }
    }
    if (on_changed_) {
        for (const auto& changed_meta : diff.changed) {
            on_changed_(changed_meta);
        }
    }
    if (on_refreshed_) {
        on_refreshed_(diff);
    }

    return diff;
}

void ApplicationCatalog::clear() {
    applications_.clear();
}

} // namespace ldde::application

