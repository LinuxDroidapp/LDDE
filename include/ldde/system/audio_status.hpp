#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <vector>

namespace ldde::system {

enum class AudioVolumeLevel {
    Muted,
    Low,
    Medium,
    High,
    Unavailable
};

[[nodiscard]] std::string_view audio_volume_level_name(AudioVolumeLevel level) noexcept;

struct AudioInfo {
    int volume_percent = 70;
    bool is_muted = false;
    bool is_available = true;
    AudioVolumeLevel level = AudioVolumeLevel::Medium;
    std::string device_name = "Default Output";
};

[[nodiscard]] AudioVolumeLevel compute_audio_level(int volume_pct, bool is_muted, bool is_available) noexcept;

class AudioStatusProvider {
public:
    virtual ~AudioStatusProvider() = default;
    [[nodiscard]] virtual AudioInfo query_audio() = 0;
    virtual void set_muted(bool muted) = 0;
    virtual void set_volume(int volume_pct) = 0;
};

class LinuxAudioProvider : public AudioStatusProvider {
public:
    explicit LinuxAudioProvider(std::string proc_asound_path = "/proc/asound");
    [[nodiscard]] AudioInfo query_audio() override;
    void set_muted(bool muted) override;
    void set_volume(int volume_pct) override;

private:
    std::string proc_asound_path_;
    int volume_percent_ = 75;
    bool is_muted_ = false;
};

class MockAudioProvider : public AudioStatusProvider {
public:
    void set_audio_info(AudioInfo info) { info_ = std::move(info); }
    [[nodiscard]] AudioInfo query_audio() override { return info_; }
    void set_muted(bool muted) override {
        info_.is_muted = muted;
        info_.level = compute_audio_level(info_.volume_percent, info_.is_muted, info_.is_available);
    }
    void set_volume(int volume_pct) override {
        info_.volume_percent = std::clamp(volume_pct, 0, 100);
        info_.level = compute_audio_level(info_.volume_percent, info_.is_muted, info_.is_available);
    }

private:
    AudioInfo info_;
};

class AudioStatus {
public:
    using AudioChangedCallback = std::function<void(const AudioInfo&)>;

    explicit AudioStatus(std::shared_ptr<AudioStatusProvider> provider = nullptr);

    void set_provider(std::shared_ptr<AudioStatusProvider> provider);
    bool update();

    [[nodiscard]] const AudioInfo& info() const noexcept { return current_info_; }

    void set_muted(bool muted);
    void toggle_mute();
    void set_volume(int volume_pct);

    void on_changed(AudioChangedCallback callback);

private:
    std::shared_ptr<AudioStatusProvider> provider_;
    AudioInfo current_info_;
    std::vector<AudioChangedCallback> callbacks_;
};

} // namespace ldde::system
