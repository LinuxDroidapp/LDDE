#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_metadata.hpp"

namespace ldde::application {

struct CatalogDiff {
    std::vector<ApplicationMetadata> added;
    std::vector<ApplicationMetadata> removed;
    std::vector<ApplicationMetadata> changed;

    [[nodiscard]] bool has_changes() const noexcept {
        return !added.empty() || !removed.empty() || !changed.empty();
    }
};

using ApplicationCallback = std::function<void(const ApplicationMetadata&)>;
using CatalogRefreshCallback = std::function<void(const CatalogDiff&)>;

class ApplicationCatalog {
public:
    ApplicationCatalog() = default;

    // Queries
    [[nodiscard]] std::vector<ApplicationMetadata> all() const;
    [[nodiscard]] std::vector<ApplicationMetadata> visible_applications(std::string_view desktop_name = {}) const;
    [[nodiscard]] const ApplicationMetadata* find(const ApplicationId& id) const noexcept;
    [[nodiscard]] bool contains(const ApplicationId& id) const noexcept;
    [[nodiscard]] size_t count() const noexcept { return applications_.size(); }
    [[nodiscard]] size_t visible_count(std::string_view desktop_name = {}) const;
    [[nodiscard]] std::vector<ApplicationMetadata> search(std::string_view query, std::string_view desktop_name = {}) const;

    // Catalog Mutations & Diffing
    CatalogDiff update_applications(std::vector<ApplicationMetadata> new_applications);
    void clear();

    // Event Subscriptions
    void on_application_added(ApplicationCallback cb) { on_added_ = std::move(cb); }
    void on_application_removed(ApplicationCallback cb) { on_removed_ = std::move(cb); }
    void on_application_changed(ApplicationCallback cb) { on_changed_ = std::move(cb); }
    void on_catalog_refreshed(CatalogRefreshCallback cb) { on_refreshed_ = std::move(cb); }

private:
    std::unordered_map<ApplicationId, ApplicationMetadata> applications_;

    ApplicationCallback on_added_;
    ApplicationCallback on_removed_;
    ApplicationCallback on_changed_;
    CatalogRefreshCallback on_refreshed_;
};

} // namespace ldde::application

