/**
 * @file SpiTransport.cpp
 */
#include "MB_DDF/PhysicalLayer/ControlPlane/SpiTransport.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
#include <ctime>
#include <cstdlib>

namespace MB_DDF {
namespace PhysicalLayer {
namespace ControlPlane {

namespace {
static int parse_int_env(const char* name, int defval) {
    const char* s = ::getenv(name);
    if (!s || !*s) return defval;
    long v = std::strtol(s, nullptr, 0);
    if (v <= 0) return defval;
    return static_cast<int>(v);
}
static const char* parse_str_env(const char* name, const char* defval) {
    const char* s = ::getenv(name);
    return (s && *s) ? s : defval;
}
} // namespace

int SpiTransport::set_nonblock(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}

bool SpiTransport::open(const TransportConfig& cfg) {
    cfg_ = cfg;

    // 解析 SPI 参数（可由环境变量覆盖）
    spi_speed_ = static_cast<uint32_t>(parse_int_env("MB_SPI_SPEED_HZ", static_cast<int>(spi_speed_)));
    spi_mode_  = static_cast<uint8_t>(parse_int_env("MB_SPI_MODE", spi_mode_));
    spi_bits_  = static_cast<uint8_t>(parse_int_env("MB_SPI_BITS", spi_bits_));

    // 打开 spidev 设备
    if (!cfg_.device_path.empty()) {
        spi_fd_ = ::open(cfg_.device_path.c_str(), O_RDWR | O_CLOEXEC);
        if (spi_fd_ < 0) {
            LOGE("spi", "open_spidev", errno, "path=%s", cfg_.device_path.c_str());
        } else {
            (void)set_nonblock(spi_fd_);
            // 配置模式、位宽、速率
            if (::ioctl(spi_fd_, SPI_IOC_WR_MODE, &spi_mode_) < 0) {
                LOGW("spi", "SPI_IOC_WR_MODE", errno, "mode=%u", spi_mode_);
            }
            if (::ioctl(spi_fd_, SPI_IOC_WR_BITS_PER_WORD, &spi_bits_) < 0) {
                LOGW("spi", "SPI_IOC_WR_BITS_PER_WORD", errno, "bits=%u", spi_bits_);
            }
            if (::ioctl(spi_fd_, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed_) < 0) {
                LOGW("spi", "SPI_IOC_WR_MAX_SPEED_HZ", errno, "speed=%u", spi_speed_);
            }
            uint8_t rd_mode = 0; uint8_t rd_bits = 0; uint32_t rd_speed = 0;
            (void)::ioctl(spi_fd_, SPI_IOC_RD_MODE, &rd_mode);
            (void)::ioctl(spi_fd_, SPI_IOC_RD_BITS_PER_WORD, &rd_bits);
            (void)::ioctl(spi_fd_, SPI_IOC_RD_MAX_SPEED_HZ, &rd_speed);
            LOGI("spi", "open_spidev", 0, "fd=%d mode=%u/%u bits=%u/%u speed=%u/%u", spi_fd_, spi_mode_, rd_mode, spi_bits_, rd_bits, spi_speed_, rd_speed);
        }
    }

    // 打开 GPIO 中断（事件）
    if (cfg_.event_number >= 0) {
#if MB_DDF_HAS_GPIOD
        const char* chip_name = parse_str_env("MB_GPIO_CHIP", "gpiochip0");
        chip_ = ::gpiod_chip_open_by_name(chip_name);
        if (!chip_) {
            LOGW("spi", "gpiod_chip_open_by_name", errno, "chip=%s", chip_name);
        } else {
            line_ = ::gpiod_chip_get_line(chip_, cfg_.event_number);
            if (!line_) {
                LOGW("spi", "gpiod_chip_get_line", errno, "line=%d", cfg_.event_number);
            } else {
                int rc = ::gpiod_line_request_both_edges_events(line_, "mb_ddf_spi");
                if (rc < 0) {
                    LOGW("spi", "gpiod_line_request_both_edges_events", errno, "line=%d", cfg_.event_number);
                } else {
                    gpio_event_fd_ = ::gpiod_line_event_get_fd(line_);
                    if (gpio_event_fd_ >= 0) (void)set_nonblock(gpio_event_fd_);
                    LOGI("spi", "gpio_event", 0, "chip=%s line=%d fd=%d", chip_name, cfg_.event_number, gpio_event_fd_);
                }
            }
        }
#else
        LOGW("spi", "gpio_events", ENOSYS, "libgpiod unavailable; event disabled");
#endif
    }

    if (spi_fd_ < 0 && !line_) {
        LOGE("spi", "open", -1, "no resources available");
        close();
        return false;
    }
    return true;
}

void SpiTransport::close() {
#if MB_DDF_HAS_GPIOD
    if (line_) { ::gpiod_line_release(line_); line_ = nullptr; }
    if (chip_) { ::gpiod_chip_close(chip_); chip_ = nullptr; }
#endif
    gpio_event_fd_ = -1;
    if (spi_fd_ >= 0) { ::close(spi_fd_); spi_fd_ = -1; }
}

bool SpiTransport::spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) const {
    if (spi_fd_ < 0 || (!tx && !rx) || len == 0) return false;
    struct spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<__u64>(tx);
    tr.rx_buf = reinterpret_cast<__u64>(rx);
    tr.len = static_cast<__u32>(len);
    tr.speed_hz = spi_speed_;
    tr.bits_per_word = spi_bits_;
    tr.delay_usecs = 0;
    tr.cs_change = 0;
    int rc = ::ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &tr);
    if (rc < 0) {
        LOGE("spi", "SPI_IOC_MESSAGE", errno, "len=%zu", len);
        return false;
    }
    return true;
}

bool SpiTransport::reg_read(uint64_t offset, void* out, size_t len) const {
    LOGW("spi", "reg_read", ENOSYS, "not supported; use xfer");
    (void)offset; (void)out; (void)len;
    return false;
}

bool SpiTransport::reg_write(uint64_t offset, const void* in, size_t len) {
    LOGW("spi", "reg_write", ENOSYS, "not supported; use xfer");
    (void)offset; (void)in; (void)len;
    return false;
}

bool SpiTransport::readReg8(uint64_t offset, uint8_t& val) const {
    LOGW("spi", "readReg8", ENOSYS, "not supported; use xfer");
    (void)offset; (void)val; return false;
}
bool SpiTransport::writeReg8(uint64_t offset, uint8_t val) {
    LOGW("spi", "writeReg8", ENOSYS, "not supported; use xfer");
    (void)offset; (void)val; return false;
}

bool SpiTransport::readReg16(uint64_t offset, uint16_t& val) const {
    LOGW("spi", "readReg16", ENOSYS, "not supported; use xfer");
    (void)offset; (void)val; return false;
}
bool SpiTransport::writeReg16(uint64_t offset, uint16_t val) {
    LOGW("spi", "writeReg16", ENOSYS, "not supported; use xfer");
    (void)offset; (void)val; return false;
}

bool SpiTransport::readReg32(uint64_t offset, uint32_t& val) const {
    LOGW("spi", "readReg32", ENOSYS, "not supported; use xfer");
    (void)offset; (void)val; return false;
}
bool SpiTransport::writeReg32(uint64_t offset, uint32_t val) {
    LOGW("spi", "writeReg32", ENOSYS, "not supported; use xfer");
    (void)offset; (void)val; return false;
}

bool SpiTransport::continuousWrite(int /*channel*/, const void* /*buf*/, size_t /*len*/) {
    LOGW("spi", "continuousWrite", ENOSYS, "not supported; use xfer");
    return false;
}
bool SpiTransport::continuousRead(int /*channel*/, void* /*buf*/, size_t /*len*/) {
    LOGW("spi", "continuousRead", ENOSYS, "not supported; use xfer");
    return false;
}

bool SpiTransport::continuousWriteAt(int /*channel*/, const void* /*buf*/, size_t /*len*/, uint64_t /*device_offset*/) {
    LOGW("spi", "continuousWriteAt", ENOSYS, "not supported; use xfer");
    return false;
}
bool SpiTransport::continuousReadAt(int /*channel*/, void* /*buf*/, size_t /*len*/, uint64_t /*device_offset*/) {
    LOGW("spi", "continuousReadAt", ENOSYS, "not supported; use xfer");
    return false;
}

bool SpiTransport::continuousWriteAsync(int /*channel*/, const void* /*buf*/, size_t /*len*/, uint64_t /*device_offset*/) {
    LOGW("spi", "continuousWriteAsync", ENOSYS, "not supported; use xfer");
    if (on_write_complete_) on_write_complete_(-1);
    return false;
}
bool SpiTransport::continuousReadAsync(int /*channel*/, void* /*buf*/, size_t /*len*/, uint64_t /*device_offset*/) {
    LOGW("spi", "continuousReadAsync", ENOSYS, "not supported; use xfer");
    if (on_read_complete_) on_read_complete_(-1);
    return false;
}

int SpiTransport::waitEvent(uint32_t* bitmap, uint32_t timeout_ms) {
#if MB_DDF_HAS_GPIOD
    if (!line_) return -1;
    struct timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000ull;
    int rc = ::gpiod_line_event_wait(line_, timeout_ms == (uint32_t)-1 ? nullptr : &ts);
    if (rc < 0) {
        LOGE("spi", "gpiod_line_event_wait", errno, "");
        return -1;
    }
    if (rc == 0) return 0; // 超时
    struct gpiod_line_event ev{};
    if (::gpiod_line_event_read(line_, &ev) < 0) {
        LOGE("spi", "gpiod_line_event_read", errno, "");
        return -1;
    }
    if (bitmap) *bitmap = 0x1; // 单一事件线，bit0 表示触发
    return 1; // 返回事件号 1
#else
    (void)bitmap; (void)timeout_ms;
    return -1;
#endif
}

bool SpiTransport::xfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    return spi_transfer(tx, rx, len);
}

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF