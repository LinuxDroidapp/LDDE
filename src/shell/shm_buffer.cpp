#include "ldde/shell/shm_buffer.hpp"
#include "ldde/core/logging.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#if !defined(MFD_CLOEXEC)
#define MFD_CLOEXEC 0x0001U
#endif
#if !defined(MFD_ALLOW_SEALING)
#define MFD_ALLOW_SEALING 0x0002U
#endif

namespace ldde::shell {

namespace {

int create_shm_fd(size_t size) {
    int fd = memfd_create("ldde-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        // Fallback to shm_open with unique name
        char name[32];
        std::snprintf(name, sizeof(name), "/ldde-shm-%d-%ld", getpid(), static_cast<long>(size));
        fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
        }
    }

    if (fd < 0) {
        LDDE_LOG_ERROR(Shell, "Failed to create shared memory file: " << std::strerror(errno));
        return -1;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
        LDDE_LOG_ERROR(Shell, "ftruncate failed on shm fd: " << std::strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

} // namespace

const wl_buffer_listener ShmBuffer::buffer_listener_ = {
    .release = [](void* data, wl_buffer*) {
        auto* buf = static_cast<ShmBuffer*>(data);
        buf->set_busy(false);
    }
};

ShmBuffer::ShmBuffer(int32_t width, int32_t height, int32_t stride, size_t size,
                     int fd, void* data, wl_buffer* buffer)
    : width_(width), height_(height), stride_(stride), size_(size),
      fd_(fd), data_(data), buffer_(buffer) {
    if (buffer_) {
        wl_buffer_add_listener(buffer_, &buffer_listener_, this);
    }
}

ShmBuffer::~ShmBuffer() {
    if (buffer_) {
        wl_buffer_destroy(buffer_);
        buffer_ = nullptr;
    }
    if (data_ && data_ != MAP_FAILED) {
        munmap(data_, size_);
        data_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

ShmBufferPool::ShmBufferPool(wl_shm* shm)
    : shm_(shm) {}

ShmBufferPool::~ShmBufferPool() {
    release_all();
}

void ShmBufferPool::release_all() {
    buffers_.clear();
}

std::shared_ptr<ShmBuffer> ShmBufferPool::acquire_buffer(int32_t width, int32_t height) {
    // Look for an available, non-busy buffer matching dimensions
    for (auto& buf : buffers_) {
        if (!buf->is_busy() && buf->width() == width && buf->height() == height) {
            return buf;
        }
    }

    // Allocate new buffer
    auto new_buf = create_buffer(width, height);
    if (new_buf) {
        buffers_.push_back(new_buf);
    }
    return new_buf;
}

std::shared_ptr<ShmBuffer> ShmBufferPool::create_buffer(int32_t width, int32_t height) {
    if (!shm_ || width <= 0 || height <= 0) {
        return nullptr;
    }

    int32_t stride = width * 4; // 32bpp ARGB8888
    size_t size = static_cast<size_t>(stride * height);

    int fd = create_shm_fd(size);
    if (fd < 0) {
        return nullptr;
    }

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        LDDE_LOG_ERROR(Shell, "mmap failed for shm buffer: " << std::strerror(errno));
        close(fd);
        return nullptr;
    }

    wl_shm_pool* pool = wl_shm_create_pool(shm_, fd, static_cast<int32_t>(size));
    if (!pool) {
        LDDE_LOG_ERROR(Shell, "wl_shm_create_pool failed");
        munmap(data, size);
        close(fd);
        return nullptr;
    }

    wl_buffer* buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    if (!buffer) {
        LDDE_LOG_ERROR(Shell, "wl_shm_pool_create_buffer failed");
        munmap(data, size);
        close(fd);
        return nullptr;
    }

    return std::make_shared<ShmBuffer>(width, height, stride, size, fd, data, buffer);
}

} // namespace ldde::shell
