#include "ldde/system/audio_status.hpp"
#include <filesystem>
#include <algorithm>

namespace ldde::system {

std::string_view audio_volume_level_name(AudioVolumeLevel level) noexcept {
    switch (level) {
        case AudioVolumeLevel::Muted:       return "Muted";
        case AudioVolumeLevel::Low:         return "Low";
        case AudioVolumeLevel::Medium:      return "Medium";
        case AudioVolumeLevel::High:        return "High";
        case AudioVolumeLevel::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

AudioVolumeLevel compute_audio_level(int volume_pct, bool is_muted, bool is_available) noexcept {
    if (!is_available) return AudioVolumeLevel::Unavailable;
    if (is_muted || volume_pct <= 0) return AudioVolumeLevel::Muted;
    if (volume_pct <= 33) return AudioVolumeLevel::Low;
    if (volume_pct <= 66) return AudioVolumeLevel::Medium;
    return AudioVolumeLevel::High;
}

LinuxAudioProvider::LinuxAudioProvider(std::string proc_asound_path)
    : proc_asound_path_(std::move(proc_asound_path)) {}

AudioInfo LinuxAudioProvider::query_audio() {
    AudioInfo info;
    std::error_code ec;
    bool has_sound = std::filesystem::exists(proc_asound_path_ + "/cards", ec) ||
                     std::filesystem::exists("/dev/snd", ec);

    info.is_available = has_sound;
    info.is_muted = is_muted_;
    info.volume_percent = volume_percent_;
    info.level = compute_audio_level(info.volume_percent, info.is_muted, info.is_available);
    if (!has_sound) {
        info.device_name = "None";
    } else {
        info.device_name = "Default Output";
    }
    return info;
}

void LinuxAudioProvider::set_muted(bool muted) {
    is_muted_ = muted;
}

void LinuxAudioProvider::set_volume(int volume_pct) {
    volume_percent_ = std::clamp(volume_pct, 0, 100);
}

AudioStatus::AudioStatus(std::shared_ptr<AudioStatusProvider> provider)
    : provider_(std::move(provider)) {
    if (!provider_) {
        provider_ = std::make_shared<LinuxAudioProvider>();
    }
    update();
}

void AudioStatus::set_provider(std::shared_ptr<AudioStatusProvider> provider) {
    provider_ = std::move(provider);
    if (!provider_) {
        provider_ = std::make_shared<LinuxAudioProvider>();
    }
    update();
}

bool AudioStatus::update() {
    if (!provider_) return false;
    AudioInfo new_info = provider_->query_audio();

    bool changed = (new_info.volume_percent != current_info_.volume_percent ||
                    new_info.is_muted != current_info_.is_muted ||
                    new_info.is_available != current_info_.is_available ||
                    new_info.level != current_info_.level);

    current_info_ = std::move(new_info);

    if (changed) {
        for (const auto& cb : callbacks_) {
            if (cb) cb(current_info_);
        }
    }
    return changed;
}

void AudioStatus::set_muted(bool muted) {
    if (provider_) {
        provider_->set_muted(muted);
        update();
    }
}

void AudioStatus::toggle_mute() {
    set_muted(!current_info_.is_muted);
}

void AudioStatus::set_volume(int volume_pct) {
    if (provider_) {
        provider_->set_volume(volume_pct);
        update();
    }
}

void AudioStatus::on_changed(AudioChangedCallback callback) {
    callbacks_.push_back(std::move(callback));
}

} // namespace ldde::system
