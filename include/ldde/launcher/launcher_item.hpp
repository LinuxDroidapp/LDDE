#pragma once

#include <string>
#include <vector>
#include <optional>
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_icon.hpp"
#include "ldde/application/application_metadata.hpp"

namespace ldde::launcher {

class LauncherIconResolver;

class LauncherItem {
public:
    LauncherItem() = default;

    [[nodiscard]] static LauncherItem from_metadata(
        const application::ApplicationMetadata& meta,
        LauncherIconResolver* resolver = nullptr);

    [[nodiscard]] const application::ApplicationId& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& generic_name() const noexcept { return generic_name_; }
    [[nodiscard]] const std::string& comment() const noexcept { return comment_; }
    [[nodiscard]] const application::ApplicationIconReference& icon_ref() const noexcept { return icon_ref_; }
    [[nodiscard]] const std::optional<std::string>& resolved_icon_path() const noexcept { return resolved_icon_path_; }
    [[nodiscard]] const std::vector<std::string>& categories() const noexcept { return categories_; }
    [[nodiscard]] const std::string& executable() const noexcept { return executable_; }
    [[nodiscard]] const std::vector<std::string>& exec_args() const noexcept { return exec_args_; }
    [[nodiscard]] bool terminal() const noexcept { return terminal_; }
    [[nodiscard]] const std::string& desktop_entry_path() const noexcept { return desktop_entry_path_; }
    [[nodiscard]] const std::string& startup_wm_class() const noexcept { return startup_wm_class_; }
    [[nodiscard]] bool startup_notify() const noexcept { return startup_notify_; }

    [[nodiscard]] bool is_selected() const noexcept { return selected_; }
    void set_selected(bool sel) noexcept { selected_ = sel; }

    [[nodiscard]] bool is_pressed() const noexcept { return pressed_; }
    void set_pressed(bool pr) noexcept { pressed_ = pr; }

    void resolve_icon(LauncherIconResolver& resolver, int preferred_size = 48);

private:
    application::ApplicationId id_;
    std::string name_;
    std::string generic_name_;
    std::string comment_;
    application::ApplicationIconReference icon_ref_;
    std::optional<std::string> resolved_icon_path_;
    std::vector<std::string> categories_;
    std::string executable_;
    std::vector<std::string> exec_args_;
    bool terminal_ = false;
    std::string desktop_entry_path_;
    std::string startup_wm_class_;
    bool startup_notify_ = false;
    bool selected_ = false;
    bool pressed_ = false;
};

} // namespace ldde::launcher
