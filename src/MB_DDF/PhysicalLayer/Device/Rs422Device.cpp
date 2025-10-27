/**
 * @file Rs422Device.cpp
 */
#include "MB_DDF/PhysicalLayer/Device/Rs422Device.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include <cstring>
#include <unistd.h>
#include <errno.h>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

namespace {
// RS422 寄存器/缓冲区偏移（参考 reference/rs422/no8driver_803_rs422_driver.h）
static constexpr uint64_t RECV_BUF = 0x000; // BRAM 接收缓冲区起始
static constexpr uint64_t SEND_BUF = 0x100; // BRAM 发送缓冲区起始
static constexpr uint64_t UCR_reg  = 0x200; // UART 控制寄存器（8位）
static constexpr uint64_t MCR_reg  = 0x201; // 模式控制寄存器（8位）
static constexpr uint64_t BRSR_reg = 0x202; // 波特率选择寄存器（8位）
static constexpr uint64_t ICR_reg  = 0x203; // 中断/状态控制寄存器（8位）
static constexpr uint64_t THL_reg  = 0x204; // 发送头低字节（8位）
static constexpr uint64_t THH_reg  = 0x205; // 发送头高字节（8位）
static constexpr uint64_t RHL_reg  = 0x206; // 接收头低字节（8位）
static constexpr uint64_t RHH_reg  = 0x207; // 接收头高字节（8位）

// static constexpr uint64_t TTL_reg  = 0x208; // 发送尾低字节（8位，可选）
// static constexpr uint64_t TTH_reg  = 0x209; // 发送尾高字节（8位，可选）
// static constexpr uint64_t RTL_reg  = 0x20A; // 接收尾低字节（8位，可选）
// static constexpr uint64_t RTH_reg  = 0x20B; // 接收尾高字节（8位，可选）

static constexpr uint64_t CMD_reg  = 0x300; // 命令/状态寄存器（写入命令用 32 位，读取状态用 8 位）
static constexpr uint64_t STU_reg  = 0x300; // 状态寄存器（与 CMD 同址，读 8 位）
static constexpr uint64_t ERR_reg  = 0x301; // 错误寄存器（8位）

// 状态位
static constexpr uint8_t STU_RX_READY_MASK = 0x01; // bit0：接收有数据
static constexpr uint8_t STU_TX_READY_MASK = 0x02; // bit1：发送可用

// 命令值
static constexpr uint32_t CMD_TX = 0x81; // 触发发送
static constexpr uint32_t CMD_RX = 0x82; // 触发接收

// 8位寄存器读写（通过 32 位接口实现）
inline bool readReg8(ControlPlane::IDeviceTransport& tp, uint64_t offset, uint8_t& val) {
    uint64_t aligned = offset & ~0x3ULL;
    uint32_t word = 0;
    if (!tp.readReg32(aligned, word)) return false;
    unsigned byte_index = static_cast<unsigned>(offset & 0x3ULL);
    val = static_cast<uint8_t>((word >> (byte_index * 8)) & 0xFF);
    return true;
}

inline bool writeReg8(ControlPlane::IDeviceTransport& tp, uint64_t offset, uint8_t val) {
    uint64_t aligned = offset & ~0x3ULL;
    uint32_t word = 0;
    if (!tp.readReg32(aligned, word)) return false;
    unsigned byte_index = static_cast<unsigned>(offset & 0x3ULL);
    uint32_t mask = ~(0xFFu << (byte_index * 8));
    uint32_t new_word = (word & mask) | (static_cast<uint32_t>(val) << (byte_index * 8));
    return tp.writeReg32(aligned, new_word);
}

inline bool readReg32(ControlPlane::IDeviceTransport& tp, uint64_t offset, uint32_t& val) {
    return tp.readReg32(offset, val);
}
inline bool writeReg32(ControlPlane::IDeviceTransport& tp, uint64_t offset, uint32_t val) {
    return tp.writeReg32(offset, val);
}
}

bool Rs422Device::open(const LinkConfig& cfg) {
    // 调用基类打开，建立数据面状态
    if (!TransportLinkAdapter::open(cfg)) {
        LOGE("rs422", "open", -1, "adapter base open failed");
        return false;
    }

    // 仅支持寄存器访问：要求已映射 user 寄存器空间
    auto& tp = transport();
    if (tp.getMappedBase() == nullptr || tp.getMappedLength() == 0) {
        LOGE("rs422", "open", -1, "register space unmapped");
        return false;
    }

    // 将 ICR 置 1，确保状态可读；其余寄存器配置由上层通过 ioctl 完成
    (void)writeReg8(tp, ICR_reg, 1);

    LOGI("rs422", "open", 0, "mtu=%u, regs=mmapped", getMTU());
    return true;
}

bool Rs422Device::close() {
    // 目前仅更新状态，资源由控制面自身负责关闭
    return TransportLinkAdapter::close();
}

bool Rs422Device::send(const uint8_t* data, uint32_t len, const DataPlane::Endpoint& dst) {
    (void)dst;
    auto& tp = transport();
    if (!data || len == 0) return true; // 空帧视为成功
    if (tp.getMappedBase() == nullptr) return false;

    // 检查发送就绪
    uint8_t stu = 0;
    if (!readReg8(tp, STU_reg, stu)) return false;
    if ((stu & STU_TX_READY_MASK) != STU_TX_READY_MASK) {
        // 设备忙
        return false;
    }

    // 限制长度至 255（与参考实现一致）
    uint8_t sendlen = static_cast<uint8_t>(len > 255 ? 255 : len);

    // 写入首 4 字节：长度 + 前 3 字节数据
    uint8_t first4[4] = { sendlen, 0, 0, 0 };
    if (sendlen >= 1) first4[1] = data[0];
    if (sendlen >= 2) first4[2] = data[1];
    if (sendlen >= 3) first4[3] = data[2];
    uint32_t word0 = 0;
    std::memcpy(&word0, first4, sizeof(word0));
    if (!writeReg32(tp, SEND_BUF, word0)) return false;

    // 写入余下数据：从 data[3] 开始，以 4 字节对齐写入到 SEND_BUF + 4, 8, ...
    uint32_t bram_offset = 4;
    while (bram_offset <= sendlen) {
        uint32_t w = 0;
        // 组装 4 字节（不足补零）
        for (int i = 0; i < 4; ++i) {
            uint32_t idx = bram_offset + i - 1; // 注意参考实现对齐方式（-1）
            uint8_t b = (idx < sendlen) ? data[idx] : 0;
            w |= (static_cast<uint32_t>(b) << (8 * i));
        }
        if (!writeReg32(tp, SEND_BUF + bram_offset, w)) return false;
        bram_offset += 4;
    }

    // 写入发送命令
    if (!writeReg32(tp, CMD_reg, CMD_TX)) return false;
    return true;
}

int32_t Rs422Device::receive(uint8_t* buf, uint32_t buf_size, DataPlane::Endpoint& src) {
    (void)src;
    auto& tp = transport();
    if (!buf || buf_size == 0) return 0;
    if (tp.getMappedBase() == nullptr) return -1;

    // 错误检查
    uint8_t err = 0;
    if (!readReg8(tp, ERR_reg, err)) return -1;
    if (err != 0) {
        // 设备返回错误，按 ILink 约定以负数表示错误
        return -1;
    }

    // 检查是否有数据
    uint8_t stu = 0;
    if (!readReg8(tp, STU_reg, stu)) return -1;
    if ((stu & STU_RX_READY_MASK) == 0) {
        // 无数据
        return 0;
    }

    // 触发接收命令，设备将接收缓冲区准备就绪
    if (!writeReg32(tp, CMD_reg, CMD_RX)) return -1;

    // 读取首 4 字节：长度 + 前 3 字节数据
    uint32_t word0 = 0;
    if (!readReg32(tp, RECV_BUF, word0)) return -1;
    uint8_t first4[4];
    std::memcpy(first4, &word0, sizeof(word0));
    uint32_t reallen = first4[0];
    if (reallen == 0) return 0;
    uint32_t out_len = reallen < buf_size ? reallen : buf_size;

    // 将前 3 字节数据写入到输出缓冲区（去除第一个长度字节）
    uint32_t produced = 0;
    if (out_len >= 1) { buf[0] = first4[1]; produced = 1; }
    if (out_len >= 2) { buf[1] = first4[2]; produced = 2; }
    if (out_len >= 3) { buf[2] = first4[3]; produced = 3; }

    // 读取余下数据：以 4 字节为单位从 RECV_BUF + 4 开始，照参考实现对齐（-1）
    uint32_t bram_offset = 4;
    while (produced < out_len) {
        uint32_t w = 0;
        if (!readReg32(tp, RECV_BUF + bram_offset, w)) break;
        uint8_t bytes[4];
        std::memcpy(bytes, &w, sizeof(w));
        // 按参考实现，将 4 字节写入到 buf[produced-1 ... produced+2]
        for (int i = 0; i < 4 && produced < out_len; ++i) {
            // 对齐偏移：第一个字节应写到 produced（已写 3 个字节后相当于 data[3] 开始）
            buf[produced] = bytes[i];
            ++produced;
        }
        bram_offset += 4;
    }

    return static_cast<int32_t>(produced);
}

int32_t Rs422Device::receive(uint8_t* buf, uint32_t buf_size, DataPlane::Endpoint& src, uint32_t timeout_us) {
    // 若支持事件设备，则优先等待事件；否则轮询状态位直到超时
    auto& tp = transport();
    if (tp.getEventFd() >= 0) {
        uint32_t bitmap = 0;
        int ev = tp.waitEvent(&bitmap, timeout_us / 1000);
        if (ev <= 0) return ev == 0 ? 0 : -1; // 0 超时；-1 错误
        return receive(buf, buf_size, src);
    }

    // 如果没有绑定 event 则轮询：每 100us 检查一次状态，直到超时或有数据
    const uint32_t step_us = 100;
    uint32_t waited = 0;
    while (waited <= timeout_us) {
        uint8_t stu = 0;
        if (!readReg8(tp, STU_reg, stu)) return -1;
        if ((stu & STU_RX_READY_MASK) != 0) {
            return receive(buf, buf_size, src);
        }
        usleep(step_us);
        waited += step_us;
    }
    return 0; // 超时
}

int Rs422Device::ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) {
    (void)out; (void)out_len;

    auto& tp = transport();
    if (tp.getMappedBase() == nullptr || tp.getMappedLength() == 0) {
        LOGE("rs422", "ioctl", -1, "register space unmapped");
        return -ENODEV;
    }

    switch (opcode) {
    case IOCTL_CONFIG: {
        // 校验输入参数
        if (in == nullptr || in_len != sizeof(Config)) {
            LOGE("rs422", "ioctl", -1, "invalid config payload");
            return -EINVAL;
        }

        const Config* cfg = reinterpret_cast<const Config*>(in);

        // 参考 rs422_config 的寄存器写序，并在每次写后短暂延时
        // UCR: UART 控制；MCR: 模式控制；BRSR: 波特率；ICR: 状态控制
        if (!writeReg8(tp, UCR_reg, cfg->ucr))  { return -EIO; } 
        if (!writeReg8(tp, MCR_reg, cfg->mcr))  { return -EIO; } 
        if (!writeReg8(tp, BRSR_reg, cfg->brsr)){ return -EIO; } 
        if (!writeReg8(tp, ICR_reg, cfg->icr))  { return -EIO; } // 超时返回空闲状态

        // 发送/接收头两个字节（低/高字节）
        if (!writeReg8(tp, THL_reg, cfg->tx_head_lo)) { return -EIO; } 
        if (!writeReg8(tp, THH_reg, cfg->tx_head_hi)) { return -EIO; } 
        if (!writeReg8(tp, RHL_reg, cfg->rx_head_lo)) { return -EIO; } 
        if (!writeReg8(tp, RHH_reg, cfg->rx_head_hi)) { return -EIO; } 

        LOGI("rs422", "ioctl", 0,
             "configured ucr=0x%02x mcr=0x%02x brsr=0x%02x icr=0x%02x tx_head=[0x%02x,0x%02x] rx_head=[0x%02x,0x%02x]",
             cfg->ucr, cfg->mcr, cfg->brsr, cfg->icr,
             cfg->tx_head_lo, cfg->tx_head_hi, cfg->rx_head_lo, cfg->rx_head_hi);

        return 0; // 成功
    }

    default:
        return -ENOSYS; // 未实现的操作码
    }
}

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF