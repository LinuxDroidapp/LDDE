#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <wayland-client.h>
#include "ldde/display/display_info.hpp"
#include "ldde/wayland/registry.hpp"
#include "ldde/wayland/wrappers.hpp"

namespace ldde::display {

using core::Status;

class DisplayManager {
public:
    using DisplayCallback = std::function<void(const DisplayInfo& info)>;

    DisplayManager();
    ~DisplayManager();

    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    Status initialize(wayland::WaylandRegistry& registry);
    void reset() noexcept;

    [[nodiscard]] const std::vector<DisplayInfo>& displays() const noexcept {
        return display_list_;
    }

    [[nodiscard]] std::optional<DisplayInfo> primary_display() const noexcept;
    [[nodiscard]] std::optional<DisplayInfo> find_display_by_id(uint32_t id) const noexcept;
    [[nodiscard]] std::optional<DisplayInfo> find_display_by_name(std::string_view name) const noexcept;

    void on_display_added(DisplayCallback cb) { on_added_ = std::move(cb); }
    void on_display_removed(DisplayCallback cb) { on_removed_ = std::move(cb); }
    void on_display_changed(DisplayCallback cb) { on_changed_ = std::move(cb); }

    // Internal Output Wrapper
    struct OutputHandle {
        uint32_t id = 0;
        wayland::UniqueOutput output;
        DisplayInfo info;
        DisplayInfo pending_info;
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

private:
    std::unordered_map<uint32_t, std::unique_ptr<OutputHandle>> outputs_;
    std::vector<DisplayInfo> display_list_;

    DisplayCallback on_added_;
    DisplayCallback on_removed_;
    DisplayCallback on_changed_;

    void rebuild_display_list();

    static const wl_output_listener output_listener_;
};

} // namespace ldde::display
