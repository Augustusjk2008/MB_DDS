/**
 * @file HelmDevice.cpp
 */
#include "MB_DDF/PhysicalLayer/Device/HelmDevice.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include <cstdint>
#include <cstring>
#include <unistd.h>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

namespace {
// 舵机寄存器偏移，参考 reference/axi/no8driver_803_axi_driver.c
static constexpr uint64_t ADDR_OUTPUT_PWM      = 0xBC * 4;     // PWM 输出起始偏移（32位），每路+4
static constexpr uint64_t ADDR_HELM_ENABLE     = 0xB0 * 4;     // 舵机功能模块使能（16位）
static constexpr uint64_t ADDR_PWM_ENABLE      = 0x10 * 4;     // PWM 使能寄存器（16位）
static constexpr uint64_t ADDR_FILTER_ENABLE   = 0x4;          // AD 滤波寄存器（16位）
static constexpr uint64_t ADDR_PARA_AD         = 0xA0 * 4;     // 参数寄存器起始（16位）
static constexpr uint64_t ADDR_PARA_NUM_AD     = 0xA1 * 4;     // 参数数量（16位），参考值 44
static constexpr uint64_t ADDR_OUT_ENABLE_AD   = 0xAF * 4;     // 输出使能位图（16位）

static constexpr int HELM_NUM = 4;
}

bool HelmDevice::open(const LinkConfig& cfg) {
    if (!TransportLinkAdapter::open(cfg)) {
        LOGE("helm", "open", -1, "adapter base open failed");
        return false;
    }

    // 要求控制面支持寄存器访问（XdmaTransport），并且已映射用户空间寄存器
    auto& tp = transport();
    if (tp.getMappedBase() == nullptr && tp.getMappedLength() == 0) {
        // 允许无 mmap，但需要基本读写接口可用；XdmaTransport 提供 readReg*/writeReg*
        // 若底层不支持，将在具体操作时报错
        LOGW("helm", "open", 0, "register space unmapped; will use direct read/write");
    }

    LOGI("helm", "open", 0, "mtu=%u", getMTU());
    return true;
}

bool HelmDevice::close() {
    initialized_ = false;
    return TransportLinkAdapter::close();
}

bool HelmDevice::send(const uint8_t* data, uint32_t len) {
    // 输入数据应为 4 路 32 位占空比，总长至少 16 字节
    if (!data || len < static_cast<uint32_t>(HELM_NUM * sizeof(uint32_t))) {
        LOGE("helm", "send", -1, "invalid pwm payload len=%u", len);
        return false;
    }

    // 写入 PWM duty
    for (int i = 0; i < HELM_NUM; ++i) {
        uint32_t duty = 0;
        std::memcpy(&duty, data + i * sizeof(uint32_t), sizeof(uint32_t));
        // 与测试保持一致：驱动使用小端；写入 32 位值到 addrOutputAd + i*4
        if (!wr32(ADDR_OUTPUT_PWM + i * 4, duty)) {
            LOGE("helm", "send", -1, "write pwm[%d] failed", i);
            return false;
        }
    }
    return true;
}

int32_t HelmDevice::receive(uint8_t* buf, uint32_t buf_size) {
    // 输出需要容纳 4 路 16 位 AD 值（共 8 字节）
    if (!buf || buf_size < static_cast<uint32_t>(HELM_NUM * sizeof(uint16_t))) {
        return -1;
    }

    for (int i = 0; i < HELM_NUM; ++i) {
        uint16_t ad = 0;
        // reference 驱动按 (i+1)*4 偏移读取 16 位
        if (!rd16((i + 1) * 4, ad)) {
            LOGE("helm", "receive", -1, "read ad[%d] failed", i);
            return -1;
        }
        std::memcpy(buf + i * sizeof(uint16_t), &ad, sizeof(uint16_t));
    }
    return static_cast<int32_t>(HELM_NUM * sizeof(uint16_t));
}

int32_t HelmDevice::receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) {
    // 按需求：带超时的 receive 直接调用非阻塞 receive
    (void)timeout_us;
    return receive(buf, buf_size);
}

int HelmDevice::ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) {
    switch (opcode) {
    case IOCTL_HELM: {
        if (!in || in_len < sizeof(Config)) {
            return -1; // 参数错误
        }
        const Config* cfg = static_cast<const Config*>(in);
        if (!wr16(ADDR_HELM_ENABLE, 0xDAE0)) return -1;
        if (!wr16(ADDR_PARA_NUM_AD, 44)) return -1;
        if (!wr16(ADDR_PARA_AD, cfg->pwm_freq)) return -1;
        if (!wr16(ADDR_FILTER_ENABLE, cfg->ad_filter)) return -1;
        if (!wr16(ADDR_HELM_ENABLE, 0xEAE0)) return -1;
        if (!wr16(ADDR_PWM_ENABLE, 0xEA8C)) return -1;
        if (!wr16(ADDR_OUT_ENABLE_AD, cfg->out_enable)) return -1; // 输出使能
        // if (!wr32(ADDR_HELM_ENABLE, 0xDAE0)) return -1;
        // if (!wr32(ADDR_PARA_NUM_AD, 44)) return -1;
        // if (!wr32(ADDR_PARA_AD, cfg->pwm_freq)) return -1;
        // if (!wr32(ADDR_FILTER_ENABLE, cfg->ad_filter)) return -1;
        // if (!wr32(ADDR_HELM_ENABLE, 0xEAE0)) return -1;
        // if (!wr32(ADDR_PWM_ENABLE, 0xEA8C)) return -1;
        // if (!wr32(ADDR_OUT_ENABLE_AD, cfg->out_enable)) return -1; // 输出使能

        initialized_ = true;
        LOGI("helm", "ioctl", 0, "initialized pwm_freq=%u out_enable=0x%04x ad_filter=%u",
            cfg->pwm_freq, cfg->out_enable, cfg->ad_filter);
        return 0;
    }

    case IOCTL_PWM: {
        // 直接复用 send 语义：设置占空比
        if (!in || in_len < static_cast<size_t>(HELM_NUM * sizeof(uint32_t))) return -1;
        const uint8_t* data = static_cast<const uint8_t*>(in);
        return send(data, static_cast<uint32_t>(in_len)) ? 0 : -1;
    }

    case IOCTL_FDB: {
        if (!out || out_len < static_cast<size_t>(HELM_NUM * sizeof(uint16_t))) return -1;
        uint8_t* buf = static_cast<uint8_t*>(out);
        int32_t n = receive(buf, static_cast<uint32_t>(out_len));
        return (n >= 0) ? n : -1;
    }

    default:
        return -38; // 未实现的操作码
    }
}

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF