#include "ldde/application/application_discovery.hpp"
#include "ldde/application/desktop_entry_reader.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/core/logging.hpp"
#include <algorithm>
#include <system_error>

namespace ldde::application {

ApplicationDiscovery::ApplicationDiscovery(
    ApplicationCatalog& catalog, ApplicationDiscoveryPolicy policy)
    : catalog_(catalog), policy_(std::move(policy)) {
}

core::Status ApplicationDiscovery::scan_and_refresh() {
    LDDE_LOG_INFO(Application, "Starting desktop application discovery");

    auto dirs = policy_.search_directories();
    // Sort directories so lower priority number (higher precedence) comes first
    std::sort(dirs.begin(), dirs.end(), [](const SearchDirectory& a, const SearchDirectory& b) {
        return a.priority < b.priority;
    });

    struct DiscoveredItem {
        ApplicationId id;
        DesktopEntry entry;
        DesktopEntrySource source;
    };

    std::unordered_map<ApplicationId, DiscoveredItem> discovered;
    std::error_code ec;

    for (const auto& sdir : dirs) {
        if (!std::filesystem::exists(sdir.path, ec) || !std::filesystem::is_directory(sdir.path, ec)) {
            LDDE_LOG_DEBUG(Application, "Application directory does not exist or is not a directory: " << sdir.path.string());
            continue;
        }

        LDDE_LOG_DEBUG(Application, "Scanning application directory: " << sdir.path.string()
                       << " (type=" << source_type_name(sdir.type) << ", priority=" << sdir.priority << ")");

        auto iter = std::filesystem::recursive_directory_iterator(
            sdir.path, std::filesystem::directory_options::skip_permission_denied, ec);

        if (ec) {
            LDDE_LOG_WARN(Application, "Failed to scan directory: " << sdir.path.string() << " (" << ec.message() << ")");
            continue;
        }

        for (auto entry_it = std::filesystem::begin(iter); entry_it != std::filesystem::end(iter); ++entry_it) {
            if (entry_it->is_regular_file(ec) && entry_it->path().extension() == ".desktop") {
                auto rel_path = std::filesystem::relative(entry_it->path(), sdir.path, ec);
                if (ec) {
                    rel_path = entry_it->path().filename();
                }

                ApplicationId app_id = ApplicationId::from_relative_path(rel_path);
                if (!app_id.is_valid()) {
                    LDDE_LOG_DEBUG(Application, "Skipping invalid desktop application ID from: " << entry_it->path().string());
                    continue;
                }

                if (discovered.contains(app_id)) {
                    LDDE_LOG_DEBUG(Application, "Application " << app_id.value() << " in "
                                   << entry_it->path().string() << " shadowed by higher-priority entry");
                    continue;
                }

                auto read_res = DesktopEntryReader::read_file(entry_it->path());
                if (read_res.is_error()) {
                    LDDE_LOG_WARN(Application, "Error reading desktop file " << entry_it->path().string()
                                  << ": " << read_res.error().message());
                    continue;
                }

                auto parse_res = DesktopEntryParser::parse(read_res.value());
                if (parse_res.is_error()) {
                    LDDE_LOG_WARN(Application, "Error parsing desktop file " << entry_it->path().string()
                                  << ": " << parse_res.error().message());
                    continue;
                }

                DesktopEntry entry = std::move(parse_res.value());
                // Only process Type=Application
                std::string type = entry.get_string("Desktop Entry", "Type");
                if (type != "Application") {
                    LDDE_LOG_DEBUG(Application, "Skipping non-application desktop entry (Type=" << type
                                   << "): " << entry_it->path().string());
                    continue;
                }

                if (!entry.is_valid_application()) {
                    LDDE_LOG_WARN(Application, "Desktop entry missing required Name or Exec in "
                                  << entry_it->path().string());
                    continue;
                }

                DesktopEntrySource src(entry_it->path(), sdir.type, sdir.priority);
                discovered.emplace(app_id, DiscoveredItem{app_id, std::move(entry), std::move(src)});
            }
        }
    }

    std::vector<ApplicationMetadata> app_metadata_list;
    app_metadata_list.reserve(discovered.size());

    for (auto& [app_id, item] : discovered) {
        auto meta_res = ApplicationMetadata::from_desktop_entry(
            item.id, item.entry, item.source, policy_.locale());
        if (meta_res.is_ok()) {
            app_metadata_list.push_back(std::move(meta_res.value()));
        } else {
            LDDE_LOG_WARN(Application, "Failed to normalize metadata for " << app_id.value()
                          << ": " << meta_res.error().message());
        }
    }

    CatalogDiff diff = catalog_.update_applications(std::move(app_metadata_list));
    LDDE_LOG_INFO(Application, "Application discovery complete: " << catalog_.count() << " total applications ("
                  << catalog_.visible_count(policy_.desktop_identity()) << " visible). Added: "
                  << diff.added.size() << ", Removed: " << diff.removed.size() << ", Changed: " << diff.changed.size());

    return core::Status::ok();
}

} // namespace ldde::application

