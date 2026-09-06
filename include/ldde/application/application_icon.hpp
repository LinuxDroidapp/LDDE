#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <ostream>

namespace ldde::application {

enum class IconType {
    None = 0,
    ThemeName,
    FilePath
};

class ApplicationIconReference {
public:
    ApplicationIconReference() = default;
    explicit ApplicationIconReference(std::string raw_icon);

    [[nodiscard]] const std::string& raw() const noexcept { return raw_icon_; }
    [[nodiscard]] IconType type() const noexcept { return type_; }
    [[nodiscard]] bool has_icon() const noexcept { return type_ != IconType::None; }
    [[nodiscard]] bool is_file_path() const noexcept { return type_ == IconType::FilePath; }
    [[nodiscard]] bool is_theme_name() const noexcept { return type_ == IconType::ThemeName; }

    [[nodiscard]] bool operator==(const ApplicationIconReference& other) const noexcept {
        return raw_icon_ == other.raw_icon_;
    }
    [[nodiscard]] bool operator!=(const ApplicationIconReference& other) const noexcept {
        return raw_icon_ != other.raw_icon_;
    }

    friend std::ostream& operator<<(std::ostream& os, const ApplicationIconReference& icon) {
        return os << icon.raw_icon_;
    }

private:
    std::string raw_icon_;
    IconType type_ = IconType::None;
};

} // namespace ldde::application

