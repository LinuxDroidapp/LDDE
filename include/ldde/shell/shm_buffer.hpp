#pragma once

#include <wayland-client.h>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include "ldde/core/error.hpp"

namespace ldde::shell {

using core::Status;

class ShmBuffer {
public:
    ShmBuffer(int32_t width, int32_t height, int32_t stride, size_t size, int fd, void* data, wl_buffer* buffer);
    ~ShmBuffer();

    ShmBuffer(const ShmBuffer&) = delete;
    ShmBuffer& operator=(const ShmBuffer&) = delete;

    [[nodiscard]] int32_t width() const noexcept { return width_; }
    [[nodiscard]] int32_t height() const noexcept { return height_; }
    [[nodiscard]] int32_t stride() const noexcept { return stride_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] void* data() const noexcept { return data_; }
    [[nodiscard]] wl_buffer* wl_buf() const noexcept { return buffer_; }
    [[nodiscard]] bool is_busy() const noexcept { return busy_; }

    void set_busy(bool busy) noexcept { busy_ = busy; }

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t stride_ = 0;
    size_t size_ = 0;
    int fd_ = -1;
    void* data_ = nullptr;
    wl_buffer* buffer_ = nullptr;
    bool busy_ = false;

    static const wl_buffer_listener buffer_listener_;
};

class ShmBufferPool {
public:
    explicit ShmBufferPool(wl_shm* shm);
    ~ShmBufferPool();

    ShmBufferPool(const ShmBufferPool&) = delete;
    ShmBufferPool& operator=(const ShmBufferPool&) = delete;

    std::shared_ptr<ShmBuffer> acquire_buffer(int32_t width, int32_t height);
    void prune_stale(const std::vector<std::pair<int32_t, int32_t>>& active_dimensions);
    void prune_idle();
    [[nodiscard]] size_t buffer_count() const noexcept { return buffers_.size(); }
    void release_all();

private:
    static constexpr size_t kMaxBuffersPerGeometry = 3;
    wl_shm* shm_ = nullptr;
    std::vector<std::shared_ptr<ShmBuffer>> buffers_;

    std::shared_ptr<ShmBuffer> create_buffer(int32_t width, int32_t height);
};

} // namespace ldde::shell
