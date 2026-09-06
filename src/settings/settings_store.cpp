#include "ldde/settings/settings_store.hpp"
#include "ldde/core/logging.hpp"

namespace ldde::settings {

SettingsStore::SettingsStore(config::Config& config,
                             SettingsSchema schema,
                             std::string user_config_path)
    : config_(config),
      schema_(std::move(schema)),
      user_config_path_(std::move(user_config_path)) {
    if (user_config_path_.empty()) {
        user_config_path_ = config::Config::default_user_config_path();
    }
}

std::optional<SettingsValue> SettingsStore::get(std::string_view key) const {
    if (in_transaction_) {
        auto it = staging_values_.find(std::string(key));
        if (it != staging_values_.end()) {
            return it->second;
        }
    }

    const auto* def = schema_.find(key);
    if (!def) {
        return std::nullopt;
    }

    auto sec = def->section();
    auto prop = def->property();
    auto opt_str = config_.get_string(sec, prop);
    if (opt_str.has_value()) {
        auto parsed = SettingsValue::from_string(def->type, *opt_str);
        if (parsed.has_value()) {
            return parsed;
        }
    }

    return def->default_value;
}

SettingsValue SettingsStore::get_or(std::string_view key, const SettingsValue& default_val) const {
    auto opt = get(key);
    return opt.value_or(default_val);
}

core::Status SettingsStore::set(const std::string& key, const SettingsValue& value, bool auto_persist) {
    const auto* def = schema_.find(key);
    if (!def) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                 core::ErrorCode::SettingNotFound,
                                 "Setting definition not found for key: " + key);
    }

    core::Status valid = def->validate(value);
    if (valid.is_error()) {
        LDDE_LOG_WARN(Settings, "Validation failed for setting '" << key << "': " << valid.to_string());
        return valid;
    }

    if (in_transaction_) {
        staging_values_[key] = value;
        return core::Status::ok();
    }

    std::string str_val = value.to_string();
    config_.set(def->section(), def->property(), str_val);

    LDDE_LOG_INFO(Settings, "Setting updated: " << key << " = " << str_val);
    notify_changed(key, value);

    if (auto_persist && !user_config_path_.empty()) {
        core::Status save_status = save();
        if (save_status.is_error()) {
            LDDE_LOG_WARN(Settings, "Failed to persist setting to " << user_config_path_ << ": " << save_status.to_string());
            return save_status;
        }
    }

    return core::Status::ok();
}

core::Status SettingsStore::reset_setting(const std::string& key, bool auto_persist) {
    const auto* def = schema_.find(key);
    if (!def) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                 core::ErrorCode::SettingNotFound,
                                 "Cannot reset unknown setting: " + key);
    }

    return set(key, def->default_value, auto_persist);
}

core::Status SettingsStore::reset_category(SettingsCategory category, bool auto_persist) {
    auto settings = schema_.settings_in_category(category);
    for (const auto* def : settings) {
        if (!def) continue;
        core::Status s = set(def->key, def->default_value, false);
        if (s.is_error()) return s;
    }

    if (auto_persist && !user_config_path_.empty()) {
        return save();
    }
    return core::Status::ok();
}

core::Status SettingsStore::reset_all(bool auto_persist) {
    for (const auto& def : schema_.all_settings()) {
        core::Status s = set(def.key, def.default_value, false);
        if (s.is_error()) return s;
    }

    if (auto_persist && !user_config_path_.empty()) {
        return save();
    }
    return core::Status::ok();
}

core::Status SettingsStore::save() {
    if (user_config_path_.empty()) {
        return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                 core::ErrorCode::SettingPersistenceFailed,
                                 "User config path not set");
    }

    core::Status s = config_.save_to_file(user_config_path_);
    if (s.is_ok()) {
        LDDE_LOG_DEBUG(Settings, "Settings saved successfully to " << user_config_path_);
    } else {
        LDDE_LOG_ERROR(Settings, "Failed saving settings: " << s.to_string());
    }
    return s;
}

void SettingsStore::begin_transaction() {
    in_transaction_ = true;
    staging_values_.clear();
}

core::Status SettingsStore::commit(bool auto_persist) {
    if (!in_transaction_) {
        return core::Status::ok();
    }

    // First validate all staged values
    for (const auto& [key, val] : staging_values_) {
        const auto* def = schema_.find(key);
        if (!def) {
            rollback();
            return LDDE_STATUS_ERROR(core::ErrorCategory::Settings,
                                     core::ErrorCode::SettingNotFound,
                                     "Unknown setting in transaction: " + key);
        }
        core::Status s = def->validate(val);
        if (s.is_error()) {
            rollback();
            return s;
        }
    }

    // Apply all staged values
    auto staged = std::move(staging_values_);
    in_transaction_ = false;

    for (const auto& [key, val] : staged) {
        const auto* def = schema_.find(key);
        if (def) {
            config_.set(def->section(), def->property(), val.to_string());
            notify_changed(key, val);
        }
    }

    if (auto_persist && !user_config_path_.empty()) {
        return save();
    }
    return core::Status::ok();
}

void SettingsStore::rollback() {
    staging_values_.clear();
    in_transaction_ = false;
}

void SettingsStore::notify_changed(const std::string& key, const SettingsValue& value) {
    for (const auto& cb : global_callbacks_) {
        if (cb) cb(key, value);
    }

    auto it = keyed_callbacks_.find(key);
    if (it != keyed_callbacks_.end()) {
        for (const auto& cb : it->second) {
            if (cb) cb(key, value);
        }
    }
}

} // namespace ldde::settings
