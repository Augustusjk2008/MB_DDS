#include "MB_DDF/PhysicalLayer/Transport/XdmaTransport.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <string>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

// 端序转换：参考 C 代码的 ltoh/htol 宏，保持与设备寄存器小端一致
static inline uint16_t ltohs_u16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap16(x);
#else
    return x;
#endif
}

static inline uint32_t ltohl_u32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(x);
#else
    return x;
#endif
}

static inline uint16_t htols_u16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap16(x);
#else
    return x;
#endif
}

static inline uint32_t htoll_u32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(x);
#else
    return x;
#endif
}

std::string XdmaTransport::make_user_path(const std::string& base) {
    return base + "_user";
}
std::string XdmaTransport::make_h2c_path(const std::string& base, int ch) {
    return base + "_h2c_" + std::to_string(ch);
}
std::string XdmaTransport::make_c2h_path(const std::string& base, int ch) {
    return base + "_c2h_" + std::to_string(ch);
}
std::string XdmaTransport::make_events_path(const std::string& base, int num) {
    return base + "_events_" + std::to_string(num);
}

size_t XdmaTransport::page_size() {
    long p = ::sysconf(_SC_PAGESIZE);
    return p > 0 ? static_cast<size_t>(p) : 4096;
}

bool XdmaTransport::open(const TransportConfig& cfg) {
    // 关闭旧资源，缓存新配置
    close();
    cfg_ = cfg;

    if (cfg.backend != Backend::XDMA) {
        // 仅实现 XDMA 后端
        return false;
    }

    // 若未提供设备基路径，默认使用 /dev/xdma0
    base_path_ = cfg.device_path.empty() ? std::string("/dev/xdma0") : cfg.device_path;

    // 1) 打开 user 设备并进行持久映射（寄存器访问）。性能优先：一次 mmap，重复读写。
    std::string user_path = make_user_path(base_path_);
    user_fd_ = ::open(user_path.c_str(), O_RDWR | O_SYNC);
    if (user_fd_ < 0) {
        close();
        return false;
    }

    // 映射长度：先尝试 fstat 获取文件大小，失败则退回系统页大小。
    size_t map_len = 0;
    struct stat st{};
    if (::fstat(user_fd_, &st) == 0 && st.st_size > 0) {
        map_len = static_cast<size_t>(st.st_size);
    }
    if (map_len == 0) {
        map_len = page_size();
    }

    void* base = ::mmap(nullptr, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, user_fd_, 0);
    if (base == MAP_FAILED) {
        close();
        return false;
    }
    user_base_ = base;
    mapped_len_ = map_len;

    // 2) 可选打开 DMA 通道（_h2c_<ch>, _c2h_<ch>）
    if (cfg.dma_h2c_channel >= 0) {
        std::string h2c_path = make_h2c_path(base_path_, cfg.dma_h2c_channel);
        h2c_fd_ = ::open(h2c_path.c_str(), O_RDWR);
        if (h2c_fd_ < 0) {
            // 打开失败不影响寄存器访问，后续禁用 H2C
            h2c_fd_ = -1;
        }
    }
    if (cfg.dma_c2h_channel >= 0) {
        std::string c2h_path = make_c2h_path(base_path_, cfg.dma_c2h_channel);
        c2h_fd_ = ::open(c2h_path.c_str(), O_RDWR);
        if (c2h_fd_ < 0) {
            c2h_fd_ = -1;
        }
    }

    // 3) 可选打开事件设备（_events_<n>）。不做复杂 epoll 管理，简化为阻塞 read 或 poll。
    if (cfg.event_number >= 0) {
        std::string ev_path = make_events_path(base_path_, cfg.event_number);
        events_fd_ = ::open(ev_path.c_str(), O_RDONLY);
        if (events_fd_ < 0) {
            events_fd_ = -1;
        }
    }

    return true;
}

void XdmaTransport::close() {
    if (user_base_) {
        ::munmap(user_base_, mapped_len_);
        user_base_ = nullptr;
        mapped_len_ = 0;
    }
    if (user_fd_ >= 0) { ::close(user_fd_); user_fd_ = -1; }
    if (h2c_fd_  >= 0) { ::close(h2c_fd_);  h2c_fd_  = -1; }
    if (c2h_fd_  >= 0) { ::close(c2h_fd_);  c2h_fd_  = -1; }
    if (events_fd_ >= 0) { ::close(events_fd_); events_fd_ = -1; }
    base_path_.clear();
}

// 8/16/32 位寄存器读取/写入：与参考 C 程序保持功能一致，但避免每次 mmap。
bool XdmaTransport::readReg8(uint64_t offset, uint8_t& val) const {
    if (!user_base_ || offset + 1 > mapped_len_) return false;
    volatile uint8_t* reg = reinterpret_cast<volatile uint8_t*>(static_cast<uint8_t*>(user_base_) + offset);
    val = *reg;
    return true;
}

bool XdmaTransport::readReg16(uint64_t offset, uint16_t& val) const {
    if (!user_base_ || offset + 2 > mapped_len_) return false;
    volatile uint16_t* reg = reinterpret_cast<volatile uint16_t*>(static_cast<uint8_t*>(user_base_) + offset);
    val = ltohs_u16(*reg);
    return true;
}

bool XdmaTransport::readReg32(uint64_t offset, uint32_t& val) const {
    if (!user_base_ || offset + 4 > mapped_len_) return false;
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t*>(static_cast<uint8_t*>(user_base_) + offset);
    val = ltohl_u32(*reg);
    return true;
}

bool XdmaTransport::writeReg8(uint64_t offset, uint8_t v) {
    if (!user_base_ || offset + 1 > mapped_len_) return false;
    volatile uint8_t* reg = reinterpret_cast<volatile uint8_t*>(static_cast<uint8_t*>(user_base_) + offset);
    *reg = v;
    return true;
}

bool XdmaTransport::writeReg16(uint64_t offset, uint16_t v) {
    if (!user_base_ || offset + 2 > mapped_len_) return false;
    volatile uint16_t* reg = reinterpret_cast<volatile uint16_t*>(static_cast<uint8_t*>(user_base_) + offset);
    *reg = htols_u16(v);
    return true;
}

bool XdmaTransport::writeReg32(uint64_t offset, uint32_t v) {
    if (!user_base_ || offset + 4 > mapped_len_) return false;
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t*>(static_cast<uint8_t*>(user_base_) + offset);
    *reg = htoll_u32(v);
    return true;
}

// 事件等待：当启用 events_fd 时可阻塞等待；支持超时（poll）。
int XdmaTransport::waitEvent(uint32_t timeout_ms) {
    if (events_fd_ < 0) return 0; // 未启用事件

    if (timeout_ms == 0) {
        uint32_t status = 0;
        ssize_t n = ::read(events_fd_, &status, sizeof(status));
        if (n == static_cast<ssize_t>(sizeof(status))) {
            return 1; // 收到事件，返回简化事件号 1
        }
        return -1; // 读取失败
    } else {
        struct pollfd pfd{};
        pfd.fd = events_fd_;
        pfd.events = POLLIN;
        int rc = ::poll(&pfd, 1, static_cast<int>(timeout_ms));
        if (rc > 0 && (pfd.revents & POLLIN)) {
            uint32_t status = 0;
            ssize_t n = ::read(events_fd_, &status, sizeof(status));
            return (n == static_cast<ssize_t>(sizeof(status))) ? 1 : -1;
        }
        return rc == 0 ? 0 : -1;
    }
}

// 高层 DMA：按参考实现分块读写，避免一次性巨大 I/O。
bool XdmaTransport::dmaWrite(int channel, const void* buf, size_t len) {
    if (channel != cfg_.dma_h2c_channel || h2c_fd_ < 0 || !buf) return false;
    const unsigned char* p = static_cast<const unsigned char*>(buf);
    size_t written = 0;
    while (written < len) {
        size_t chunk = len - written;
        if (chunk > 0x7ffff000) chunk = 0x7ffff000; // 参考上限
        ssize_t rc = ::write(h2c_fd_, p + written, chunk);
        if (rc <= 0) return false;
        written += static_cast<size_t>(rc);
    }
    return true;
}

bool XdmaTransport::dmaRead(int channel, void* buf, size_t len) {
    if (channel != cfg_.dma_c2h_channel || c2h_fd_ < 0 || !buf) return false;
    unsigned char* p = static_cast<unsigned char*>(buf);
    size_t readn = 0;
    while (readn < len) {
        size_t chunk = len - readn;
        if (chunk > 0x7ffff000) chunk = 0x7ffff000; // 参考上限
        ssize_t rc = ::read(c2h_fd_, p + readn, chunk);
        if (rc <= 0) return false;
        readn += static_cast<size_t>(rc);
    }
    return true;
}

// 兼容参考程序：支持指定设备偏移与缓冲区偏移（lseek + read/write）
bool XdmaTransport::dmaWriteAt(int channel, const void* buf, size_t len, uint64_t dev_offset, size_t buf_offset) {
    if (channel != cfg_.dma_h2c_channel || h2c_fd_ < 0 || !buf) return false;
    if (::lseek(h2c_fd_, static_cast<off_t>(dev_offset), SEEK_SET) < 0) return false;
    const unsigned char* p = static_cast<const unsigned char*>(buf) + buf_offset;
    size_t written = 0;
    while (written < len) {
        size_t chunk = len - written;
        if (chunk > 0x7ffff000) chunk = 0x7ffff000;
        ssize_t rc = ::write(h2c_fd_, p + written, chunk);
        if (rc <= 0) return false;
        written += static_cast<size_t>(rc);
    }
    return true;
}

bool XdmaTransport::dmaReadAt(int channel, void* buf, size_t len, uint64_t dev_offset, size_t buf_offset) {
    if (channel != cfg_.dma_c2h_channel || c2h_fd_ < 0 || !buf) return false;
    if (::lseek(c2h_fd_, static_cast<off_t>(dev_offset), SEEK_SET) < 0) return false;
    unsigned char* p = static_cast<unsigned char*>(buf) + buf_offset;
    size_t readn = 0;
    while (readn < len) {
        size_t chunk = len - readn;
        if (chunk > 0x7ffff000) chunk = 0x7ffff000;
        ssize_t rc = ::read(c2h_fd_, p + readn, chunk);
        if (rc <= 0) return false;
        readn += static_cast<size_t>(rc);
    }
    return true;
}

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF