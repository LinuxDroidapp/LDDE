#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include "ldde/core/error.hpp"
#include "ldde/config/config.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_value.hpp"

namespace ldde::settings {

using SettingChangedCallback = std::function<void(const std::string& key, const SettingsValue& value)>;

class SettingsStore {
public:
    explicit SettingsStore(config::Config& config,
                           SettingsSchema schema = SettingsSchema::create_default_schema(),
                           std::string user_config_path = "");
    ~SettingsStore() = default;

    SettingsStore(const SettingsStore&) = delete;
    SettingsStore& operator=(const SettingsStore&) = delete;

    [[nodiscard]] const SettingsSchema& schema() const noexcept { return schema_; }
    [[nodiscard]] SettingsSchema& schema() noexcept { return schema_; }

    [[nodiscard]] std::optional<SettingsValue> get(std::string_view key) const;
    [[nodiscard]] SettingsValue get_or(std::string_view key, const SettingsValue& default_val) const;

    core::Status set(const std::string& key, const SettingsValue& value, bool auto_persist = true);

    // Reset operations
    core::Status reset_setting(const std::string& key, bool auto_persist = true);
    core::Status reset_category(SettingsCategory category, bool auto_persist = true);
    core::Status reset_all(bool auto_persist = true);

    // Persistence
    core::Status save();
    [[nodiscard]] const std::string& user_config_path() const noexcept { return user_config_path_; }
    void set_user_config_path(std::string path) { user_config_path_ = std::move(path); }

    // Transactional modifications
    void begin_transaction();
    core::Status commit(bool auto_persist = true);
    void rollback();
    [[nodiscard]] bool in_transaction() const noexcept { return in_transaction_; }

    // Change listeners
    void on_setting_changed(SettingChangedCallback cb) {
        global_callbacks_.push_back(std::move(cb));
    }
    void on_setting_changed(const std::string& key, SettingChangedCallback cb) {
        keyed_callbacks_[key].push_back(std::move(cb));
    }

private:
    config::Config& config_;
    SettingsSchema schema_;
    std::string user_config_path_;

    bool in_transaction_ = false;
    std::unordered_map<std::string, SettingsValue> staging_values_;

    std::vector<SettingChangedCallback> global_callbacks_;
    std::unordered_map<std::string, std::vector<SettingChangedCallback>> keyed_callbacks_;

    void notify_changed(const std::string& key, const SettingsValue& value);
    void sync_from_config();
};

} // namespace ldde::settings
