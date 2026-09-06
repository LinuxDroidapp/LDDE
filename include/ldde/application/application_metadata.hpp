#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "ldde/core/error.hpp"
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_icon.hpp"
#include "ldde/application/desktop_entry_source.hpp"
#include "ldde/application/desktop_entry.hpp"

namespace ldde::application {

struct ApplicationAction {
    std::string id;
    std::string name;
    std::string exec;
    ApplicationIconReference icon;
};

class ApplicationMetadata {
public:
    ApplicationMetadata() = default;

    [[nodiscard]] static core::Result<ApplicationMetadata> from_desktop_entry(
        const ApplicationId& id,
        const DesktopEntry& entry,
        const DesktopEntrySource& source,
        std::string_view locale = {});

    // Accessors
    [[nodiscard]] const ApplicationId& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& generic_name() const noexcept { return generic_name_; }
    [[nodiscard]] const std::string& comment() const noexcept { return comment_; }
    [[nodiscard]] const std::string& exec() const noexcept { return exec_; }
    [[nodiscard]] const std::string& executable() const noexcept { return executable_; }
    [[nodiscard]] const std::vector<std::string>& exec_args() const noexcept { return exec_args_; }
    [[nodiscard]] const std::vector<std::string>& field_codes() const noexcept { return field_codes_; }
    [[nodiscard]] const ApplicationIconReference& icon() const noexcept { return icon_; }
    [[nodiscard]] bool terminal() const noexcept { return terminal_; }
    [[nodiscard]] bool no_display() const noexcept { return no_display_; }
    [[nodiscard]] bool hidden() const noexcept { return hidden_; }
    [[nodiscard]] const std::vector<std::string>& only_show_in() const noexcept { return only_show_in_; }
    [[nodiscard]] const std::vector<std::string>& not_show_in() const noexcept { return not_show_in_; }
    [[nodiscard]] const std::vector<std::string>& categories() const noexcept { return categories_; }
    [[nodiscard]] const std::vector<std::string>& mime_types() const noexcept { return mime_types_; }
    [[nodiscard]] const std::vector<std::string>& keywords() const noexcept { return keywords_; }
    [[nodiscard]] const std::string& startup_wm_class() const noexcept { return startup_wm_class_; }
    [[nodiscard]] bool startup_notify() const noexcept { return startup_notify_; }
    [[nodiscard]] const std::vector<ApplicationAction>& actions() const noexcept { return actions_; }
    [[nodiscard]] const DesktopEntrySource& source() const noexcept { return source_; }

    // Predicates
    [[nodiscard]] bool is_valid() const noexcept { return id_.is_valid() && !name_.empty() && !exec_.empty(); }
    [[nodiscard]] bool is_hidden() const noexcept { return hidden_; }
    [[nodiscard]] bool is_no_display() const noexcept { return no_display_; }
    [[nodiscard]] bool is_visible_in_desktop(std::string_view desktop_name) const noexcept;
    [[nodiscard]] bool is_visible_to_user(std::string_view desktop_name) const noexcept;
    [[nodiscard]] bool has_category(std::string_view category) const noexcept;
    [[nodiscard]] bool matches_search_query(std::string_view query) const noexcept;

private:
    ApplicationId id_;
    std::string name_;
    std::string generic_name_;
    std::string comment_;
    std::string exec_;
    std::string executable_;
    std::vector<std::string> exec_args_;
    std::vector<std::string> field_codes_;
    ApplicationIconReference icon_;
    bool terminal_ = false;
    bool no_display_ = false;
    bool hidden_ = false;
    std::vector<std::string> only_show_in_;
    std::vector<std::string> not_show_in_;
    std::vector<std::string> categories_;
    std::vector<std::string> mime_types_;
    std::vector<std::string> keywords_;
    std::string startup_wm_class_;
    bool startup_notify_ = false;
    std::vector<ApplicationAction> actions_;
    DesktopEntrySource source_;
};

} // namespace ldde::application
