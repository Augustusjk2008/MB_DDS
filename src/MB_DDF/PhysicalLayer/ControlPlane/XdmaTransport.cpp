/**
 * @file XdmaTransport.cpp
 */
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <sys/eventfd.h>

#if MB_DDF_HAS_IOURING
#include <liburing.h>
#elif MB_DDF_HAS_LIBAIO
#include <libaio.h>
#ifndef IOCB_FLAG_RESFD
#define IOCB_FLAG_RESFD (1u << 0)
#endif
// 兼容不同 libaio 头文件中的操作码命名：
// 某些发行版使用 IO_CMD_*，而代码使用 IOCB_CMD_*。
// 如果缺少 IOCB_CMD_*，则映射到 IO_CMD_* 以保证编译通过。
#ifndef IOCB_CMD_PWRITE
#define IOCB_CMD_PWRITE IO_CMD_PWRITE
#endif
#ifndef IOCB_CMD_PREAD
#define IOCB_CMD_PREAD IO_CMD_PREAD
#endif
#endif

namespace MB_DDF {
namespace PhysicalLayer {
namespace ControlPlane {

// 用于在 io_uring 的 user_data 内编码操作类型（最高位标记写）
#if MB_DDF_HAS_IOURING
static constexpr uint64_t kAioFlagWrite = (1ull << 63);
#endif

long XdmaTransport::page_size() {
    static long ps = ::sysconf(_SC_PAGESIZE);
    return ps > 0 ? ps : 4096;
}

int XdmaTransport::set_nonblock(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}

bool XdmaTransport::open(const TransportConfig& cfg) {
    cfg_ = cfg;

    // 打开 user 寄存器映射设备（可选）
    if (!cfg_.device_path.empty()) {
        std::string user = make_user_path(cfg_.device_path);
        user_fd_ = ::open(user.c_str(), O_RDWR | O_CLOEXEC);
        if (user_fd_ >= 0) {
            mapped_len_ = static_cast<size_t>(page_size());
            user_base_ = ::mmap(nullptr, mapped_len_, PROT_READ | PROT_WRITE, MAP_SHARED, user_fd_, cfg_.device_offset);
            LOGI("xdma", "mmap_user", 0, "path=%s, offset=%ld, len=%ld", user.c_str(), cfg_.device_offset, mapped_len_);
            if (user_base_ == MAP_FAILED) {
                LOGE("xdma", "mmap_user", errno, "path=%s, offset=%ld", user.c_str(), cfg_.device_offset);
                user_base_ = nullptr;
                mapped_len_ = 0;
            }
        } else {
            LOGW("xdma", "open_user", errno, "path=%s", user.c_str());
        }
    }

    // 打开 DMA 通道（可选）
    if (cfg_.dma_h2c_channel >= 0) {
        std::string h2c = make_h2c_path(cfg_.device_path, cfg_.dma_h2c_channel);
        h2c_fd_ = ::open(h2c.c_str(), O_WRONLY | O_CLOEXEC);
        if (h2c_fd_ >= 0) set_nonblock(h2c_fd_);
        else LOGW("xdma", "open_h2c", errno, "path=%s", h2c.c_str());
    }
    if (cfg_.dma_c2h_channel >= 0) {
        std::string c2h = make_c2h_path(cfg_.device_path, cfg_.dma_c2h_channel);
        c2h_fd_ = ::open(c2h.c_str(), O_RDONLY | O_CLOEXEC);
        if (c2h_fd_ >= 0) set_nonblock(c2h_fd_);
        else LOGW("xdma", "open_c2h", errno, "path=%s", c2h.c_str());
    }

    // 打开事件设备（可选）
    if (cfg_.event_number >= 0) {
        std::string ev = make_events_path(cfg_.device_path, cfg_.event_number);
        events_fd_ = ::open(ev.c_str(), O_RDONLY | O_CLOEXEC);
        if (events_fd_ >= 0) set_nonblock(events_fd_);
        else LOGW("xdma", "open_events", errno, "path=%s", ev.c_str());
    }

    // 初始化异步资源（如存在 DMA）
    bool any_dma = (h2c_fd_ >= 0 || c2h_fd_ >= 0);
    if (any_dma) {
        // 统一创建 eventfd，便于与上层集成
        if (aio_event_fd_ < 0) {
            aio_event_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (aio_event_fd_ < 0) {
                LOGW("xdma", "eventfd", errno, "create failed");
            }
        }
#if MB_DDF_HAS_IOURING
        if (!iouring_inited_) {
            int rc = ::io_uring_queue_init(256, &ring_, 0);
            if (rc == 0) {
                iouring_inited_ = true;
                // 将 eventfd 注册到 io_uring，用于完成通知
                if (aio_event_fd_ >= 0) {
                    rc = ::io_uring_register_eventfd(&ring_, aio_event_fd_);
                    if (rc < 0) LOGW("xdma", "iouring_register_eventfd", -rc, "");
                }
            } else {
                LOGW("xdma", "iouring_queue_init", rc, "");
            }
        }
#elif MB_DDF_HAS_LIBAIO
        if (!aio_ctx_) {
            const unsigned kQueueDepth = 128;
            int rc = ::io_setup(kQueueDepth, &aio_ctx_);
            if (rc < 0) {
                LOGW("xdma", "io_setup", -rc, "queue=%u", kQueueDepth);
                aio_ctx_ = 0;
            }
        }
#endif
    }

    // 至少要有一个资源成功打开才视为 open 成功（允许只用 mmap/寄存器，无 DMA）
    if (user_fd_ < 0 && h2c_fd_ < 0 && c2h_fd_ < 0 && events_fd_ < 0) {
        LOGE("xdma", "open", -1, "no resources available");
        close();
        return false;
    }
    LOGI("xdma", "open", 0, "user_fd=%d h2c_fd=%d c2h_fd=%d events_fd=%d aio_event_fd=%d",
         user_fd_, h2c_fd_, c2h_fd_, events_fd_, aio_event_fd_);
    return true;
}

bool XdmaTransport::writeReg8(uint64_t offset, uint8_t val) {
    if (!user_base_) {
        LOGW("xdma", "writeReg8", ENODEV, "unmapped");
        return false;
    }
    if (offset + sizeof(uint8_t) > mapped_len_) {
        LOGE("xdma", "writeReg8", EINVAL, "offset=%llu len=%zu", (unsigned long long)offset, mapped_len_);
        return false;
    }
    std::memcpy(static_cast<uint8_t*>(user_base_) + offset, &val, sizeof(val));
    return true;
}

bool XdmaTransport::readReg8(uint64_t offset, uint8_t& val) const {
    if (!user_base_) {
        LOGW("xdma", "readReg8", ENODEV, "unmapped");
        return false;
    }
    if (offset + sizeof(uint8_t) > mapped_len_) {
        LOGE("xdma", "readReg8", EINVAL, "offset=%llu len=%zu", (unsigned long long)offset, mapped_len_);
        return false;
    }
    val = static_cast<uint8_t>(*static_cast<const uint8_t*>(user_base_) + offset);
    return true;
}

bool XdmaTransport::readReg16(uint64_t offset, uint16_t& val) const {
    if (offset % sizeof(uint16_t) != 0) {
        LOGE("xdma", "readReg16", EINVAL, "offset=%llu must be aligned", (unsigned long long)offset);
        return false;
    }
    if (!user_base_) {
        LOGW("xdma", "readReg16", ENODEV, "unmapped");
        return false;
    }
    if (offset + sizeof(uint16_t) > mapped_len_) {
        LOGE("xdma", "readReg16", EINVAL, "offset=%llu len=%zu", (unsigned long long)offset, mapped_len_);
        return false;
    }
    uint16_t tmp = 0;
    std::memcpy(&tmp, static_cast<const uint8_t*>(user_base_) + offset, sizeof(tmp));
    val = ltoh_u16(tmp);
    return true;
}

bool XdmaTransport::writeReg16(uint64_t offset, uint16_t val) {
    if (offset % sizeof(uint16_t) != 0) {
        LOGE("xdma", "writeReg16", EINVAL, "offset=%llu must be aligned", (unsigned long long)offset);
        return false;
    }
    if (!user_base_) {
        LOGW("xdma", "writeReg16", ENODEV, "unmapped");
        return false;
    }
    if (offset + sizeof(uint16_t) > mapped_len_) {
        LOGE("xdma", "writeReg16", EINVAL, "offset=%llu len=%zu", (unsigned long long)offset, mapped_len_);
        return false;
    }
    uint16_t tmp = htol_u16(val);
    std::memcpy(static_cast<uint8_t*>(user_base_) + offset, &tmp, sizeof(tmp));
    return true;
}

bool XdmaTransport::readReg32(uint64_t offset, uint32_t& val) const {
    if (offset % sizeof(uint32_t) != 0) {
        LOGE("xdma", "readReg32", EINVAL, "offset=%llu must be aligned", (unsigned long long)offset);
        return false;
    }
    if (!user_base_) {
        LOGW("xdma", "readReg32", ENODEV, "unmapped");
        return false;
    }
    if (offset + sizeof(uint32_t) > mapped_len_) {
        LOGE("xdma", "readReg32", EINVAL, "offset=%llu len=%zu", (unsigned long long)offset, mapped_len_);
        return false;
    }
    uint32_t tmp = 0;
    std::memcpy(&tmp, static_cast<const uint8_t*>(user_base_) + offset, sizeof(tmp));
    val = ltoh_u32(tmp);
    return true;
}

bool XdmaTransport::writeReg32(uint64_t offset, uint32_t val) {
    if (offset % sizeof(uint32_t) != 0) {
        LOGE("xdma", "writeReg32", EINVAL, "offset=%llu must be aligned", (unsigned long long)offset);
        return false;
    }
    if (!user_base_) {
        LOGW("xdma", "writeReg32", ENODEV, "unmapped");
        return false;
    }
    if (offset + sizeof(uint32_t) > mapped_len_) {
        LOGE("xdma", "writeReg32", EINVAL, "offset=%llu len=%zu", (unsigned long long)offset, mapped_len_);
        return false;
    }
    uint32_t tmp = htol_u32(val);
    std::memcpy(static_cast<uint8_t*>(user_base_) + offset, &tmp, sizeof(tmp));
    return true;
}

void XdmaTransport::close() {
#if MB_DDF_HAS_IOURING
    if (iouring_inited_) {
        // 取消 eventfd 注册
        (void)::io_uring_unregister_eventfd(&ring_);
        ::io_uring_queue_exit(&ring_);
        iouring_inited_ = false;
    }
#elif MB_DDF_HAS_LIBAIO
    if (aio_ctx_) {
        ::io_destroy(aio_ctx_);
        aio_ctx_ = 0;
    }
#endif
    if (aio_event_fd_ >= 0) { ::close(aio_event_fd_); aio_event_fd_ = -1; }

    if (user_base_) {
        ::munmap(user_base_, mapped_len_);
        user_base_ = nullptr;
        mapped_len_ = 0;
    }
    if (user_fd_ >= 0) { ::close(user_fd_); user_fd_ = -1; }
    if (h2c_fd_ >= 0) { ::close(h2c_fd_); h2c_fd_ = -1; }
    if (c2h_fd_ >= 0) { ::close(c2h_fd_); c2h_fd_ = -1; }
    if (events_fd_ >= 0) { ::close(events_fd_); events_fd_ = -1; }
}

bool XdmaTransport::continuousWrite(int channel, const void* buf, size_t len) {
    if (h2c_fd_ < 0 || channel != cfg_.dma_h2c_channel) return false;
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t remain = len;
    off_t off = use_default_device_offset_ ? static_cast<off_t>(default_device_offset_) : -1;
    while (remain > 0) {
        ssize_t n = (off >= 0) ? ::pwrite(h2c_fd_, p, remain, off) : ::write(h2c_fd_, p, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
            LOGE("xdma", "dmaWrite", errno, "fd=%d", h2c_fd_);
            return false;
        }
        remain -= static_cast<size_t>(n);
        p += n;
        if (off >= 0) off += n;
    }
    return true;
}

bool XdmaTransport::continuousWriteAt(int channel, const void* buf, size_t len, uint64_t device_offset) {
    if (h2c_fd_ < 0 || channel != cfg_.dma_h2c_channel) return false;
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t remain = len;
    off_t off = static_cast<off_t>(device_offset);
    while (remain > 0) {
        ssize_t n = ::pwrite(h2c_fd_, p, remain, off);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
            LOGE("xdma", "dmaWriteAt", errno, "fd=%d off=%lld", h2c_fd_, (long long)off);
            return false;
        }
        remain -= static_cast<size_t>(n);
        p += n;
        off += n;
    }
    return true;
}

bool XdmaTransport::continuousRead(int channel, void* buf, size_t len) {
    if (c2h_fd_ < 0 || channel != cfg_.dma_c2h_channel) return false;
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    off_t off = use_default_device_offset_ ? static_cast<off_t>(default_device_offset_) : -1;
    while (got < len) {
        ssize_t n = (off >= 0) ? ::pread(c2h_fd_, p + got, len - got, off) : ::read(c2h_fd_, p + got, len - got);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { break; }
            LOGE("xdma", "dmaRead", errno, "fd=%d", c2h_fd_);
            return false;
        }
        if (n == 0) break;
        got += static_cast<size_t>(n);
        if (off >= 0) off += n;
    }
    return got > 0;
}

void XdmaTransport::setDefaultDeviceOffset(uint64_t off) {
    default_device_offset_ = off;
    use_default_device_offset_ = true;
}

void XdmaTransport::clearDefaultDeviceOffset() {
    use_default_device_offset_ = false;
}

 bool XdmaTransport::continuousReadAt(int channel, void* buf, size_t len, uint64_t device_offset) {
    if (c2h_fd_ < 0 || channel != cfg_.dma_c2h_channel) return false;
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    off_t off = static_cast<off_t>(device_offset);
    while (got < len) {
        ssize_t n = ::pread(c2h_fd_, p + got, len - got, off);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { break; }
            LOGE("xdma", "dmaReadAt", errno, "fd=%d off=%lld", c2h_fd_, (long long)off);
            return false;
        }
        if (n == 0) break;
        got += static_cast<size_t>(n);
        off += n;
    }
    return got > 0;
}

int XdmaTransport::waitEvent(uint32_t* bitmap, uint32_t timeout_ms) {
    if (events_fd_ < 0) return 0;
    struct pollfd pfd{ events_fd_, POLLIN, 0 };
    int ret = ::poll(&pfd, 1, static_cast<int>(timeout_ms));
    if (ret == 0) return 0;
    if (ret < 0) {
        LOGE("xdma", "waitEvent", errno, "poll error fd=%d", events_fd_);
        return -1;
    }
    if (pfd.revents & POLLIN) {
        ::read(events_fd_, bitmap, sizeof(uint32_t));
        return 1;
    }
    return 0;
}

int XdmaTransport::getAioEventFd() const { return aio_event_fd_; }

int XdmaTransport::drainAioCompletions(int max_events) {
    // 清理 aio_event_fd_ 计数（非阻塞）
    if (aio_event_fd_ >= 0) {
        uint64_t cnt = 0; (void)cnt;
        ::read(aio_event_fd_, &cnt, sizeof(cnt));
    }
#if MB_DDF_HAS_IOURING
    if (iouring_inited_) {
        std::vector<io_uring_cqe*> cqes(static_cast<size_t>(max_events));
        int n = ::io_uring_peek_batch_cqe(&ring_, cqes.data(), max_events);
        for (int i = 0; i < n; ++i) {
            auto* cqe = cqes[static_cast<size_t>(i)];
            uint64_t ud = cqe->user_data;
            bool is_write = (ud & kAioFlagWrite) != 0;
            ssize_t res = static_cast<ssize_t>(cqe->res);
            if (is_write) {
                if (on_write_complete_) on_write_complete_(res);
            } else {
                if (on_read_complete_) on_read_complete_(res);
            }
            ::io_uring_cqe_seen(&ring_, cqe);
        }
        return n;
    }
#elif MB_DDF_HAS_LIBAIO
    if (aio_ctx_) {
        std::vector<io_event> events(static_cast<size_t>(max_events));
        timespec ts{0, 0};
        int n = ::io_getevents(aio_ctx_, 1, max_events, events.data(), &ts);
        if (n < 0) {
            LOGE("xdma", "io_getevents", -n, "");
            return -1;
        }
        for (int i = 0; i < n; ++i) {
            auto* obj = static_cast<struct iocb*>(events[i].obj);
            ssize_t res = static_cast<ssize_t>(events[i].res);
            if (obj) {
                // 路由到读/写的全局回调
                if (obj->aio_lio_opcode == IOCB_CMD_PWRITE) {
                    if (on_write_complete_) on_write_complete_(res);
                } else if (obj->aio_lio_opcode == IOCB_CMD_PREAD) {
                    if (on_read_complete_) on_read_complete_(res);
                }
                // 释放对应的 iocb
                delete obj;
            }
        }
        return n;
    }
#endif
    (void)max_events;
    return 0;
}

bool XdmaTransport::continuousWriteAsync(int channel,
                                  const void* buf,
                                  size_t len,
                                  uint64_t device_offset) {
    if (h2c_fd_ < 0 || channel != cfg_.dma_h2c_channel) return false;
#if MB_DDF_HAS_IOURING
    if (iouring_inited_) {
        auto* sqe = ::io_uring_get_sqe(&ring_);
        if (!sqe) { LOGW("xdma", "iouring_get_sqe", -ENOMEM, ""); return false; }
        uint64_t key = kAioFlagWrite | (next_aio_id_++ & (~kAioFlagWrite));
        ::io_uring_prep_write(sqe, h2c_fd_, const_cast<void*>(buf), len, static_cast<off_t>(device_offset));
        sqe->user_data = key;
        int rc = ::io_uring_submit(&ring_);
        if (rc < 0) {
            LOGE("xdma", "iouring_submit", -rc, "write_async");
            return false;
        }
        return true;
    }
#elif MB_DDF_HAS_LIBAIO
    if (aio_ctx_) {
        auto* iocb_ptr = new struct iocb();
        memset(iocb_ptr, 0, sizeof(*iocb_ptr));
        ::io_prep_pwrite(iocb_ptr, h2c_fd_, const_cast<void*>(buf), len, static_cast<long long>(device_offset));
        iocb_ptr->u.c.flags |= IOCB_FLAG_RESFD;
        iocb_ptr->u.c.resfd = aio_event_fd_;
        struct iocb* list[1] = { iocb_ptr };
        int rc = ::io_submit(aio_ctx_, 1, list);
        if (rc != 1) {
            LOGE("xdma", "io_submit", rc < 0 ? -rc : rc, "write_async");
            delete iocb_ptr;
            return false;
        }
        // iocb_ptr 由完成后释放
        return true;
    }
#endif
    return continuousWriteAt(channel, buf, len, device_offset);
}

bool XdmaTransport::continuousReadAsync(int channel,
                                 void* buf,
                                 size_t len,
                                 uint64_t device_offset) {
    if (c2h_fd_ < 0 || channel != cfg_.dma_c2h_channel) return false;
#if MB_DDF_HAS_IOURING
    if (iouring_inited_) {
        auto* sqe = ::io_uring_get_sqe(&ring_);
        if (!sqe) { LOGW("xdma", "iouring_get_sqe", -ENOMEM, ""); return false; }
        uint64_t key = (next_aio_id_++ & (~kAioFlagWrite)); // 最高位为0表示读
        ::io_uring_prep_read(sqe, c2h_fd_, buf, len, static_cast<off_t>(device_offset));
        sqe->user_data = key;
        int rc = ::io_uring_submit(&ring_);
        if (rc < 0) {
            LOGE("xdma", "iouring_submit", -rc, "read_async");
            return false;
        }
        return true;
    }
#elif MB_DDF_HAS_LIBAIO
    if (aio_ctx_) {
        auto* iocb_ptr = new struct iocb();
        memset(iocb_ptr, 0, sizeof(*iocb_ptr));
        ::io_prep_pread(iocb_ptr, c2h_fd_, buf, len, static_cast<long long>(device_offset));
        iocb_ptr->u.c.flags |= IOCB_FLAG_RESFD;
        iocb_ptr->u.c.resfd = aio_event_fd_;
        struct iocb* list[1] = { iocb_ptr };
        int rc = ::io_submit(aio_ctx_, 1, list);
        if (rc != 1) {
            LOGE("xdma", "io_submit", rc < 0 ? -rc : rc, "read_async");
            delete iocb_ptr;
            return false;
        }
        return true;
    }
#endif
    return continuousReadAt(channel, buf, len, device_offset);
}

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF