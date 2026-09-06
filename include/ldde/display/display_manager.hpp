#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <wayland-client.h>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/display/display_event.hpp"
#include "ldde/wayland/registry.hpp"
#include "ldde/wayland/wrappers.hpp"

namespace ldde::display {

using core::Status;

class DisplayManager {
public:
    using DisplayCallback = std::function<void(const DisplayInfo& info)>;
    using DisplayEventCallback = std::function<void(const DisplayEvent& event, const DisplayInfo& info)>;

    DisplayManager();
    ~DisplayManager();

    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    Status initialize(wayland::WaylandRegistry& registry, const config::Config* config = nullptr);
    void reset() noexcept;

    [[nodiscard]] const std::vector<DisplayInfo>& displays() const noexcept {
        return display_list_;
    }

    [[nodiscard]] std::optional<DisplayInfo> primary_display() const noexcept;
    [[nodiscard]] std::optional<DisplayId> primary_display_id() const noexcept;
    [[nodiscard]] const DisplayPolicy* primary_policy() const noexcept;
    [[nodiscard]] DisplayPolicy* primary_policy() noexcept;

    [[nodiscard]] std::optional<DisplayInfo> find_display_by_id(DisplayId id) const noexcept;
    [[nodiscard]] std::optional<DisplayInfo> find_display_by_name(std::string_view name) const noexcept;
    [[nodiscard]] const DisplayPolicy* find_policy_by_id(DisplayId id) const noexcept;
    [[nodiscard]] DisplayPolicy* find_policy_by_id(DisplayId id) noexcept;

    void on_display_added(DisplayCallback cb) { on_added_ = std::move(cb); }
    void on_display_removed(DisplayCallback cb) { on_removed_ = std::move(cb); }
    void on_display_changed(DisplayCallback cb) { on_changed_ = std::move(cb); }
    void on_event(DisplayEventCallback cb) { on_event_ = std::move(cb); }

    // Internal Output Wrapper
    struct OutputHandle {
        DisplayId id = 0;
        wayland::UniqueOutput output;
        DisplayInfo info;
        DisplayInfo pending_info;
        std::unique_ptr<DisplayPolicy> policy;
        DisplayManager* manager = nullptr;
    };

    void handle_geometry(OutputHandle* handle, int32_t x, int32_t y,
                         int32_t physical_width, int32_t physical_height,
                         int32_t subpixel, const char* make, const char* model,
                         int32_t transform);
    void handle_mode(OutputHandle* handle, uint32_t flags, int32_t width,
                     int32_t height, int32_t refresh);
    void handle_done(OutputHandle* handle);
    void handle_scale(OutputHandle* handle, int32_t factor);
    void handle_name(OutputHandle* handle, const char* name);
    void handle_description(OutputHandle* handle, const char* description);

    // Direct registration for tests and synthetic outputs
    void register_synthetic_display(const DisplayInfo& info);
    void remove_synthetic_display(DisplayId id);

private:
    std::unordered_map<DisplayId, std::unique_ptr<OutputHandle>> outputs_;
    std::vector<DisplayInfo> display_list_;
    std::optional<DisplayId> primary_id_;
    std::string preferred_primary_name_;

    std::optional<LayoutClass> config_layout_class_;
    std::optional<SafeInsets> config_safe_insets_;
    std::optional<int32_t> config_scale_;

    DisplayCallback on_added_;
    DisplayCallback on_removed_;
    DisplayCallback on_changed_;
    DisplayEventCallback on_event_;

    void load_config(const config::Config& config);
    void rebuild_display_list();
    void update_primary_display();
    void emit_event(DisplayEventType type, DisplayId id, const DisplayInfo& info);

    static const wl_output_listener output_listener_;
};

} // namespace ldde::display
