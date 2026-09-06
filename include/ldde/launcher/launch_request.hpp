#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_metadata.hpp"

namespace ldde::launcher {

struct LaunchRequest {
    application::ApplicationId id;
    std::string name;
    std::string desktop_entry_path;
    std::string executable;
    std::vector<std::string> arguments;
    std::string working_directory;
    std::unordered_map<std::string, std::string> environment;
    bool terminal = false;
    std::string startup_wm_class;
    bool startup_notify = false;

    [[nodiscard]] static LaunchRequest from_metadata(
        const application::ApplicationMetadata& meta,
        const std::vector<std::string>& extra_args = {}) {
        LaunchRequest req;
        req.id = meta.id();
        req.name = meta.name();
        req.desktop_entry_path = meta.source().path();
        req.executable = meta.executable();
        req.arguments = meta.exec_args();
        if (!extra_args.empty()) {
            req.arguments.insert(req.arguments.end(), extra_args.begin(), extra_args.end());
        }
        req.terminal = meta.terminal();
        req.startup_wm_class = meta.startup_wm_class();
        req.startup_notify = meta.startup_notify();
        return req;
    }
};

} // namespace ldde::launcher

