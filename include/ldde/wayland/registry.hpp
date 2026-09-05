#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <functional>
#include "ldde/core/error.hpp"
#include "ldde/wayland/wrappers.hpp"

namespace ldde::wayland {

using core::Status;

struct GlobalInfo {
    uint32_t name = 0;
    std::string interface;
    uint32_t version = 0;
};

enum class ProtocolRequirement {
    Required,
    Optional,
    Future
};

class WaylandRegistry {
public:
    using GlobalAddCallback = std::function<void(uint32_t name, std::string_view interface, uint32_t version)>;
    using GlobalRemoveCallback = std::function<void(uint32_t name)>;

    WaylandRegistry();
    ~WaylandRegistry();

    WaylandRegistry(const WaylandRegistry&) = delete;
    WaylandRegistry& operator=(const WaylandRegistry&) = delete;

    Status initialize(wl_display* display);
    void reset() noexcept;

    [[nodiscard]] wl_registry* registry() const noexcept { return registry_.get(); }

    [[nodiscard]] bool has_global(std::string_view interface) const;
    [[nodiscard]] std::optional<GlobalInfo> get_global(std::string_view interface) const;
    [[nodiscard]] const std::unordered_map<uint32_t, GlobalInfo>& globals() const noexcept {
        return globals_;
    }

    [[nodiscard]] Status verify_required_globals() const;

    void add_global_listener(std::string_view interface, GlobalAddCallback on_add,
                             GlobalRemoveCallback on_remove = nullptr);

    template <typename T>
    T* bind(uint32_t name, const wl_interface* interface, uint32_t version) {
        if (!registry_) return nullptr;
        return static_cast<T*>(wl_registry_bind(registry_.get(), name, interface, version));
    }

    // Wayland C callbacks
    void on_global_added(uint32_t name, const char* interface, uint32_t version);
    void on_global_removed(uint32_t name);

private:
    UniqueRegistry registry_;
    std::unordered_map<uint32_t, GlobalInfo> globals_;

    struct ListenerEntry {
        std::string interface;
        GlobalAddCallback on_add;
        GlobalRemoveCallback on_remove;
    };
    std::vector<ListenerEntry> listeners_;

    static const wl_registry_listener registry_listener_;
};

} // namespace ldde::wayland
