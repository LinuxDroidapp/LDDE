#include "ldde/window/window_stacking.hpp"
#include "ldde/window/window_registry.hpp"
#include <algorithm>

namespace ldde::window {

void WindowStacking::add(WindowId id, std::optional<WindowId> parent_id) {
    if (contains(id)) {
        remove(id);
    }

    parents_[id] = parent_id;

    if (parent_id.has_value() && contains(parent_id.value())) {
        // Insert right after the parent (or after the parent's latest transient child)
        auto parent_it = std::find(stack_.begin(), stack_.end(), parent_id.value());
        if (parent_it != stack_.end()) {
            auto insert_it = parent_it + 1;
            while (insert_it != stack_.end()) {
                auto it_parent = parents_.find(*insert_it);
                if (it_parent != parents_.end() && it_parent->second == parent_id) {
                    ++insert_it;
                } else {
                    break;
                }
            }
            stack_.insert(insert_it, id);
            return;
        }
    }

    stack_.push_back(id);
    dirty_ = true;
}

void WindowStacking::remove(WindowId id) {
    auto it = std::find(stack_.begin(), stack_.end(), id);
    if (it != stack_.end()) {
        stack_.erase(it);
    }
    parents_.erase(id);

    // Unparent any children of this window
    for (auto& [child_id, parent] : parents_) {
        if (parent.has_value() && parent.value() == id) {
            parent = std::nullopt;
        }
    }
    dirty_ = true;
}

void WindowStacking::raise_internal(WindowId id) {
    auto it = std::find(stack_.begin(), stack_.end(), id);
    if (it != stack_.end()) {
        stack_.erase(it);
        stack_.push_back(id);
        dirty_ = true;
    }
}

void WindowStacking::raise(WindowId id) {
    if (!contains(id)) return;

    // Collect all transient children of id
    std::vector<WindowId> children;
    for (WindowId other : stack_) {
        auto p_it = parents_.find(other);
        if (p_it != parents_.end() && p_it->second.has_value() && p_it->second.value() == id) {
            children.push_back(other);
        }
    }

    raise_internal(id);

    // Raise children above parent in their relative order
    for (WindowId child : children) {
        raise(child);
    }
    dirty_ = true;
}

void WindowStacking::lower(WindowId id) {
    if (!contains(id)) return;

    // Collect all transient children of id
    std::vector<WindowId> children;
    for (WindowId other : stack_) {
        auto p_it = parents_.find(other);
        if (p_it != parents_.end() && p_it->second.has_value() && p_it->second.value() == id) {
            children.push_back(other);
        }
    }

    auto it = std::find(stack_.begin(), stack_.end(), id);
    if (it != stack_.end()) {
        stack_.erase(it);
        stack_.insert(stack_.begin(), id);
    }

    // Insert children directly above id at bottom
    size_t insert_idx = 1;
    for (WindowId child : children) {
        auto child_it = std::find(stack_.begin(), stack_.end(), child);
        if (child_it != stack_.end()) {
            stack_.erase(child_it);
            stack_.insert(stack_.begin() + static_cast<std::ptrdiff_t>(insert_idx++), child);
        }
    }
    dirty_ = true;
}

bool WindowStacking::contains(WindowId id) const noexcept {
    return std::find(stack_.begin(), stack_.end(), id) != stack_.end();
}

std::optional<WindowId> WindowStacking::top() const noexcept {
    if (stack_.empty()) return std::nullopt;
    return stack_.back();
}

std::optional<WindowId> WindowStacking::bottom() const noexcept {
    if (stack_.empty()) return std::nullopt;
    return stack_.front();
}

const std::vector<WindowId>& WindowStacking::visible_stack(const WindowRegistry& registry) const {
    cached_visible_stack_.clear();
    cached_visible_stack_.reserve(stack_.size());

    for (WindowId id : stack_) {
        auto win = registry.lookup(id);
        if (win && win->state() != WindowState::Minimized && win->is_visible()) {
            cached_visible_stack_.push_back(id);
        }
    }
    return cached_visible_stack_;
}

void WindowStacking::clear() noexcept {
    stack_.clear();
    parents_.clear();
    cached_visible_stack_.clear();
    dirty_ = true;
}

} // namespace ldde::window
