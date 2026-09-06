#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include "ldde/application/application_catalog.hpp"
#include "ldde/launcher/launcher_item.hpp"
#include "ldde/launcher/launcher_category.hpp"
#include "ldde/launcher/launcher_filter.hpp"
#include "ldde/launcher/launcher_search.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"

namespace ldde::launcher {

class LauncherModel {
public:
    using ModelChangedCallback = std::function<void()>;

    explicit LauncherModel(std::string theme_name = "hicolor");
    ~LauncherModel() = default;

    void set_applications(const std::vector<application::ApplicationMetadata>& apps);
    void update_from_catalog(const application::ApplicationCatalog& catalog, std::string_view desktop_name = {});

    void set_search_query(std::string_view query);
    void set_category(std::string_view category_id);
    void clear_search();
    void clear_filter();

    [[nodiscard]] const std::vector<LauncherItem>& items() const noexcept { return filtered_items_; }
    [[nodiscard]] size_t item_count() const noexcept { return filtered_items_.size(); }
    [[nodiscard]] const LauncherItem* item_at(size_t index) const noexcept;
    [[nodiscard]] const LauncherCategoryInfo* category_at(size_t index) const noexcept;
    [[nodiscard]] const std::vector<LauncherCategoryInfo>& categories() const noexcept { return categories_; }
    [[nodiscard]] const LauncherFilter& filter() const noexcept { return filter_; }
    [[nodiscard]] std::optional<size_t> selected_index() const noexcept { return selected_index_; }
    [[nodiscard]] const LauncherItem* selected_item() const noexcept;

    // Selection navigation
    bool select_index(std::optional<size_t> index);
    bool select_next();
    bool select_previous();
    bool select_up(int columns);
    bool select_down(int columns);
    bool select_first();
    bool select_last();

    [[nodiscard]] LauncherIconResolver& icon_resolver() noexcept { return icon_resolver_; }
    [[nodiscard]] const LauncherIconResolver& icon_resolver() const noexcept { return icon_resolver_; }

    void on_model_changed(ModelChangedCallback cb) { on_model_changed_ = std::move(cb); }

private:
    std::vector<application::ApplicationMetadata> raw_applications_;
    std::vector<LauncherItem> filtered_items_;
    std::vector<LauncherCategoryInfo> categories_;
    LauncherFilter filter_;
    std::optional<size_t> selected_index_;
    LauncherIconResolver icon_resolver_;
    ModelChangedCallback on_model_changed_;

    void refilter();
    void notify_changed();
};

} // namespace ldde::launcher

