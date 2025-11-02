/**
 * @file CanFDDevice.cpp
 */
#include "MB_DDF/PhysicalLayer/Device/CanFdDevice.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include "MB_DDF/PhysicalLayer/Hardware/pl_canfd.h"
#include <cstring>
#include <unistd.h>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

static inline uint32_t pack_le32(const uint8_t* p) {
    uint32_t v = 0; std::memcpy(&v, p, sizeof(v)); return v;
}
static inline void unpack_le32(uint32_t v, uint8_t* p) {
    std::memcpy(p, &v, sizeof(v));
}

uint8_t CanFDDevice::dlc_to_len(uint8_t dlc) {
    if (dlc <= 8) return dlc;
    switch (dlc) {
        case 9:  return 12;
        case 10: return 16;
        case 11: return 20;
        case 12: return 24;
        case 13: return 32;
        case 14: return 48;
        case 15: return 64;
        default: return 0;
    }
}

uint8_t CanFDDevice::len_to_dlc(uint8_t len) {
    if (len <= 8) return len;
    if (len <= 12) return 9;
    if (len <= 16) return 10;
    if (len <= 20) return 11;
    if (len <= 24) return 12;
    if (len <= 32) return 13;
    if (len <= 48) return 14;
    return 15;
}

bool CanFDDevice::open(const LinkConfig& cfg) {
    if (!TransportLinkAdapter::open(cfg)) {
        LOGE("canfd", "open", -1, "adapter base open failed");
        return false;
    }

    auto& tp = transport();
    if (tp.getMappedBase() == nullptr || tp.getMappedLength() == 0) {
        LOGW("canfd", "open", 0, "register space unmapped; will use direct read/write");
    }

    // 使用硬件初始化方法
    if (__axiCanfdHwInit() != 0) {
        LOGE("canfd", "open", -1, "hardware initialization failed");
        return false;
    }

    LOGI("canfd", "open", 0, "mtu=%u", getMTU());
    return true;
}

bool CanFDDevice::close() {
    return TransportLinkAdapter::close();
}

bool CanFDDevice::send(CanFrame& frame) {
    // 使用硬件发送方法
    if (__axiCanfdSend(&frame) != 0) {
        LOGE("canfd", "send", -EIO, "hardware send failed");
        return false;
    }
    return true;
}

int32_t CanFDDevice::receive(CanFrame& frame) {
    // 使用硬件接收方法
    return __axiCanfdRecvFifo(&frame);
}

bool CanFDDevice::send(const uint8_t* data, uint32_t len) {
    if (!data || len < 6) {
        LOGE("canfd", "send", -EINVAL, "payload too short len=%u", len);
        return false;
    }
    
    CanFrame f;
    f.id = pack_le32(data);
    uint8_t flags = data[4];
    f.ide = (flags & 0x01) != 0;
    f.rtr = (flags & 0x02) != 0;
    f.fdf = (flags & 0x04) != 0;
    f.brs = (flags & 0x08) != 0;
    f.dlc = data[5];
    uint8_t dlen = dlc_to_len(f.dlc);
    if (len < 6u + dlen) {
        LOGE("canfd", "send", -EINVAL, "len=%u < header+data=%u", len, 6u + dlen);
        return false;
    }
    f.data.assign(data + 6, data + 6 + dlen);

    // 使用硬件发送方法
    return send(f);
}

int32_t CanFDDevice::receive(uint8_t* buf, uint32_t buf_size) {
    if (!buf || buf_size < 6) return -EINVAL;
    
    CanFrame f;
    // 使用硬件接收方法
    int frames_received = receive(f);
    if (frames_received <= 0) return frames_received;

    uint8_t dlen = dlc_to_len(f.dlc);
    uint32_t need = 6u + dlen;
    if (buf_size < need) return -ENOSPC;
    
    unpack_le32(f.id, buf);
    uint8_t flags = (f.ide ? 0x01 : 0) | (f.rtr ? 0x02 : 0) | (f.fdf ? 0x04 : 0) | (f.brs ? 0x08 : 0);
    buf[4] = flags;
    buf[5] = f.dlc;
    if (dlen && !f.data.empty()) {
        std::memcpy(buf + 6, f.data.data(), dlen);
    }
    return static_cast<int32_t>(need);
}

int32_t CanFDDevice::receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) {
    uint32_t bm = 0;
    int ev = transport().waitEvent(&bm, timeout_us / 1000);
    if (ev <= 0) return ev == 0 ? 0 : -1;
    return receive(buf, buf_size);
}

int CanFDDevice::ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) {
    // 使用硬件ioctl方法
    (void)in_len;
    (void)out;
    (void)out_len;
    return __axiCanfdIoctl(static_cast<int>(opcode), const_cast<void*>(in));
}

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF