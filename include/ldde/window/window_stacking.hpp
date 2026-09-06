#pragma once

#include <vector>
#include <optional>
#include <unordered_map>
#include "ldde/window/types.hpp"

namespace ldde::window {

class WindowRegistry;

class WindowStacking {
public:
    WindowStacking() = default;
    ~WindowStacking() = default;

    void add(WindowId id, std::optional<WindowId> parent_id = std::nullopt);
    void remove(WindowId id);

    void raise(WindowId id);
    void lower(WindowId id);

    [[nodiscard]] bool contains(WindowId id) const noexcept;
    [[nodiscard]] std::optional<WindowId> top() const noexcept;
    [[nodiscard]] std::optional<WindowId> bottom() const noexcept;
    [[nodiscard]] const std::vector<WindowId>& stack() const noexcept { return stack_; }
    [[nodiscard]] const std::vector<WindowId>& visible_stack(const WindowRegistry& registry) const;
    void mark_dirty() noexcept { dirty_ = true; }

    [[nodiscard]] size_t size() const noexcept { return stack_.size(); }
    [[nodiscard]] bool empty() const noexcept { return stack_.empty(); }
    void clear() noexcept;

private:
    std::vector<WindowId> stack_; // Ordered from bottom (index 0) to top (index back)
    std::unordered_map<WindowId, std::optional<WindowId>> parents_;
    mutable std::vector<WindowId> cached_visible_stack_;
    mutable bool dirty_ = true;

    void raise_internal(WindowId id);
};

} // namespace ldde::window
