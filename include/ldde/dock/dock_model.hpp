#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/window/window_registry.hpp"
#include "ldde/window/window_manager.hpp"
#include "ldde/dock/dock_item.hpp"

namespace ldde::dock {

class DockModel {
public:
    using ModelChangedCallback = std::function<void()>;

    DockModel(application::ApplicationCatalog& catalog,
              window::WindowRegistry& registry,
              window::WindowManager& window_manager);
    ~DockModel();

    void initialize_listeners();

    void load_pinned_from_string(std::string_view pinned_str);
    std::string serialize_pinned() const;

    bool pin(const application::ApplicationId& id);
    bool unpin(const application::ApplicationId& id);
    [[nodiscard]] bool is_pinned(const application::ApplicationId& id) const noexcept;

    void rebuild_items();

    [[nodiscard]] size_t item_count() const noexcept { return items_.size(); }
    [[nodiscard]] const std::vector<DockItem>& items() const noexcept { return items_; }
    [[nodiscard]] const DockItem* item_at(size_t index) const noexcept;
    [[nodiscard]] DockItem* item_at(size_t index) noexcept;
    [[nodiscard]] const DockItem* find_by_id(const application::ApplicationId& id) const noexcept;
    [[nodiscard]] const DockItem* find_by_window_id(window::WindowId wid) const noexcept;

    void on_model_changed(ModelChangedCallback cb) { on_model_changed_ = std::move(cb); }

private:
    application::ApplicationCatalog& catalog_;
    window::WindowRegistry& registry_;
    window::WindowManager& window_manager_;

    std::vector<application::ApplicationId> pinned_ids_;
    std::vector<DockItem> items_;

    window::WindowRegistry::ListenerId window_listener_id_ = 0;
    ModelChangedCallback on_model_changed_;

    void notify_changed();
    [[nodiscard]] bool window_matches_app(const window::Window& win,
                                          const application::ApplicationId& id,
                                          const application::ApplicationMetadata* meta) const noexcept;
};

} // namespace ldde::dock
