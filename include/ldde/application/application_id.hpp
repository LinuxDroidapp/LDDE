#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <ostream>
#include <functional>

namespace ldde::application {

class ApplicationId {
public:
    ApplicationId() = default;
    explicit ApplicationId(std::string id);

    [[nodiscard]] static ApplicationId from_desktop_filename(std::string_view filename);
    [[nodiscard]] static ApplicationId from_relative_path(const std::filesystem::path& relative_path);

    [[nodiscard]] const std::string& value() const noexcept { return id_; }
    [[nodiscard]] std::string_view view() const noexcept { return id_; }
    [[nodiscard]] const char* c_str() const noexcept { return id_.c_str(); }

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return id_.empty(); }

    [[nodiscard]] std::string basename_without_extension() const;

    [[nodiscard]] bool operator==(const ApplicationId& other) const noexcept { return id_ == other.id_; }
    [[nodiscard]] bool operator!=(const ApplicationId& other) const noexcept { return id_ != other.id_; }
    [[nodiscard]] bool operator<(const ApplicationId& other) const noexcept { return id_ < other.id_; }

    friend std::ostream& operator<<(std::ostream& os, const ApplicationId& id) {
        return os << id.id_;
    }

private:
    std::string id_;
};

} // namespace ldde::application

namespace std {
template <>
struct hash<ldde::application::ApplicationId> {
    size_t operator()(const ldde::application::ApplicationId& id) const noexcept {
        return hash<string>()(id.value());
    }
};
} // namespace std

