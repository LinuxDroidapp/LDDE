#include "ldde/launcher/launcher_model.hpp"
#include <algorithm>

namespace ldde::launcher {

LauncherModel::LauncherModel(std::string theme_name)
    : icon_resolver_(std::move(theme_name)) {}

void LauncherModel::set_applications(const std::vector<application::ApplicationMetadata>& apps) {
    raw_applications_ = apps;
    refilter();
}

void LauncherModel::update_from_catalog(
    const application::ApplicationCatalog& catalog,
    std::string_view desktop_name) {
    raw_applications_ = catalog.visible_applications(desktop_name);
    refilter();
}

void LauncherModel::set_search_query(std::string_view query) {
    if (filter_.search_query == query) return;
    filter_.search_query = std::string(query);
    refilter();
}

void LauncherModel::set_category(std::string_view category_id) {
    if (filter_.category == category_id) return;
    filter_.category = std::string(category_id);
    refilter();
}

void LauncherModel::clear_search() {
    if (filter_.search_query.empty()) return;
    filter_.search_query.clear();
    refilter();
}

void LauncherModel::clear_filter() {
    if (filter_.is_empty()) return;
    filter_.reset();
    refilter();
}

const LauncherItem* LauncherModel::item_at(size_t index) const noexcept {
    if (index >= filtered_items_.size()) {
        return nullptr;
    }
    return &filtered_items_[index];
}

const LauncherCategoryInfo* LauncherModel::category_at(size_t index) const noexcept {
    if (index >= categories_.size()) {
        return nullptr;
    }
    return &categories_[index];
}

const LauncherItem* LauncherModel::selected_item() const noexcept {
    if (!selected_index_ || *selected_index_ >= filtered_items_.size()) {
        return nullptr;
    }
    return &filtered_items_[*selected_index_];
}

bool LauncherModel::select_index(std::optional<size_t> index) {
    if (selected_index_ && *selected_index_ < filtered_items_.size()) {
        filtered_items_[*selected_index_].set_selected(false);
    }

    if (!index || *index >= filtered_items_.size()) {
        selected_index_ = std::nullopt;
        notify_changed();
        return false;
    }

    selected_index_ = index;
    filtered_items_[*selected_index_].set_selected(true);
    notify_changed();
    return true;
}

bool LauncherModel::select_next() {
    if (filtered_items_.empty()) return false;
    if (!selected_index_) {
        return select_index(0);
    }
    size_t next = (*selected_index_ + 1) % filtered_items_.size();
    return select_index(next);
}

bool LauncherModel::select_previous() {
    if (filtered_items_.empty()) return false;
    if (!selected_index_) {
        return select_index(filtered_items_.size() - 1);
    }
    size_t prev = (*selected_index_ == 0) ? (filtered_items_.size() - 1) : (*selected_index_ - 1);
    return select_index(prev);
}

bool LauncherModel::select_up(int columns) {
    if (filtered_items_.empty() || columns <= 0) return false;
    if (!selected_index_) {
        return select_index(0);
    }
    if (*selected_index_ >= static_cast<size_t>(columns)) {
        return select_index(*selected_index_ - columns);
    }
    return false;
}

bool LauncherModel::select_down(int columns) {
    if (filtered_items_.empty() || columns <= 0) return false;
    if (!selected_index_) {
        return select_index(0);
    }
    size_t next = *selected_index_ + columns;
    if (next < filtered_items_.size()) {
        return select_index(next);
    }
    return false;
}

bool LauncherModel::select_first() {
    if (filtered_items_.empty()) return false;
    return select_index(0);
}

bool LauncherModel::select_last() {
    if (filtered_items_.empty()) return false;
    return select_index(filtered_items_.size() - 1);
}

void LauncherModel::refilter() {
    // 1. Filter by category
    std::vector<application::ApplicationMetadata> cat_matched;
    cat_matched.reserve(raw_applications_.size());

    for (const auto& app : raw_applications_) {
        if (LauncherCategory::matches_category(app, filter_.category)) {
            cat_matched.push_back(app);
        }
    }

    // 2. Search & rank
    auto search_results = LauncherSearch::search(cat_matched, filter_.search_query);

    // 3. Build LauncherItems
    filtered_items_.clear();
    filtered_items_.reserve(search_results.size());

    for (const auto& res : search_results) {
        filtered_items_.push_back(LauncherItem::from_metadata(*res.application, &icon_resolver_));
    }

    // 4. Update categories
    categories_ = LauncherCategory::compute_categories(raw_applications_, false);

    // 5. Restore or clamp selection
    if (selected_index_) {
        if (filtered_items_.empty()) {
            selected_index_ = std::nullopt;
        } else if (*selected_index_ >= filtered_items_.size()) {
            selected_index_ = filtered_items_.size() - 1;
            filtered_items_[*selected_index_].set_selected(true);
        } else {
            filtered_items_[*selected_index_].set_selected(true);
        }
    }

    notify_changed();
}

void LauncherModel::notify_changed() {
    if (on_model_changed_) {
        on_model_changed_();
    }
}

} // namespace ldde::launcher

