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
#include "ldde/switcher/switcher_item.hpp"
#include "ldde/switcher/switcher_mru.hpp"

namespace ldde::switcher {

enum class SwitcherPresentationMode {
    Application = 0,
    Window
};

class SwitcherModel {
public:
    using ModelChangedCallback = std::function<void()>;

    SwitcherModel(application::ApplicationCatalog& catalog,
                  window::WindowRegistry& registry,
                  window::WindowManager& window_manager);
    ~SwitcherModel();

    void initialize_listeners();

    void set_presentation_mode(SwitcherPresentationMode mode);
    [[nodiscard]] SwitcherPresentationMode presentation_mode() const noexcept { return mode_; }

    void rebuild_items(std::optional<window::WindowId> target_selected_wid = std::nullopt);

    [[nodiscard]] size_t item_count() const noexcept { return items_.size(); }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    [[nodiscard]] const std::vector<SwitcherItem>& items() const noexcept { return items_; }
    [[nodiscard]] const SwitcherItem* item_at(size_t index) const noexcept;
    [[nodiscard]] SwitcherItem* item_at(size_t index) noexcept;
    [[nodiscard]] const SwitcherItem* find_by_window_id(window::WindowId wid) const noexcept;
    [[nodiscard]] const SwitcherItem* find_by_app_id(const application::ApplicationId& id) const noexcept;
    [[nodiscard]] std::optional<size_t> find_index_by_window_id(window::WindowId wid) const noexcept;

    [[nodiscard]] SwitcherMru& mru() noexcept { return mru_; }
    [[nodiscard]] const SwitcherMru& mru() const noexcept { return mru_; }

    void on_model_changed(ModelChangedCallback cb) { model_changed_callbacks_.push_back(std::move(cb)); }

private:
    application::ApplicationCatalog& catalog_;
    window::WindowRegistry& registry_;
    window::WindowManager& window_manager_;

    SwitcherMru mru_;
    SwitcherPresentationMode mode_ = SwitcherPresentationMode::Application;
    std::vector<SwitcherItem> items_;

    window::WindowRegistry::ListenerId window_listener_id_ = 0;
    std::vector<ModelChangedCallback> model_changed_callbacks_;

    void notify_changed();
    [[nodiscard]] bool is_switchable(const window::Window& win) const noexcept;
    const application::ApplicationMetadata* resolve_metadata(const window::Window& win,
                                                          application::ApplicationId& resolved_id) const noexcept;
};

} // namespace ldde::switcher
