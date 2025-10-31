/**
 * @file CanFDDevice.cpp
 */
#include "MB_DDF/PhysicalLayer/Device/CanFdDevice.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include "MB_DDF/PhysicalLayer/Hardware/xcanfd_hw.h"
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

    // 清中断
    (void)wr32(XCANFD_ICR_OFFSET, XCANFD_IXR_ALL);
    // 复位
    (void)wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_SRST_MASK);
    usleep(1000);

    // 接受过滤全禁用（全部接收）
    (void)wr32(XCANFD_AFR_OFFSET, 0);
    // 设置 FIFO 水位：FIFO0 设为 1，FIFO1 设为 0；分区为 0
    uint32_t wir = 0;
    wir |= (1 & XCANFD_WIR_MASK);
    (void)wr32(XCANFD_WIR_OFFSET, wir);

    // 使能接收中断
    (void)wr32(XCANFD_IER_OFFSET, XCANFD_IXR_RXOK_MASK | XCANFD_IXR_RXFOFLW_MASK);

    // 进入工作
    (void)wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);

    LOGI("canfd", "open", 0, "mtu=%u", getMTU());
    return true;
}

bool CanFDDevice::close() {
    return TransportLinkAdapter::close();
}

bool CanFDDevice::write_one_frame_to_txfifo(const CanFrame& f) {
    auto& tp = transport();
    uint32_t idr = 0;
    if (f.ide) {
        uint32_t id = f.id & 0x1FFFFFFF;
        uint32_t high11 = (id >> 18) & 0x7FF;
        uint32_t low18  = id & 0x3FFFF;
        idr |= (high11 << XCANFD_IDR_ID1_SHIFT) & XCANFD_IDR_ID1_MASK;
        idr |= XCANFD_IDR_IDE_MASK;
        idr |= (low18 << XCANFD_IDR_ID2_SHIFT) & XCANFD_IDR_ID2_MASK;
    } else {
        idr |= ((f.id & 0x7FF) << XCANFD_IDR_ID1_SHIFT) & XCANFD_IDR_ID1_MASK;
    }
    if (f.rtr) idr |= XCANFD_IDR_RTR_MASK;

    uint32_t dlcr = 0;
    dlcr |= (static_cast<uint32_t>(f.dlc) << XCANFD_DLCR_DLC_SHIFT) & XCANFD_DLCR_DLC_MASK;
    if (f.fdf) dlcr |= XCANFD_DLCR_EDL_MASK;
    if (f.brs) dlcr |= XCANFD_DLCR_BRS_MASK;

    if (!wr32(XCANFD_TXFIFO_0_BASE_ID_OFFSET, idr)) return false;
    if (!wr32(XCANFD_TXFIFO_0_BASE_DLC_OFFSET, dlcr)) return false;

    uint8_t dlen = dlc_to_len(f.dlc);
    const uint8_t* p = f.data.empty() ? nullptr : f.data.data();
    uint32_t ofs = XCANFD_TXFIFO_0_BASE_DW0_OFFSET;
    for (uint8_t off = 0; off < dlen; off += 4) {
        uint32_t word = 0;
        if (p) {
            uint8_t tmp[4] = {0,0,0,0};
            uint8_t n = (dlen - off) >= 4 ? 4 : (dlen - off);
            std::memcpy(tmp, p + off, n);
            word = pack_le32(tmp);
        }
        if (!wr32(ofs, word)) return false;
        ofs += 4;
    }
    return true;
}

bool CanFDDevice::trigger_transmit() {
    // 简化：使用 TX 缓冲 0
    return wr32(XCANFD_TRR_OFFSET, 0x00000001);
}

bool CanFDDevice::read_one_frame_from_rxfifo(CanFrame& out) {
    uint32_t fsr = 0;
    if (!rd32(XCANFD_FSR_OFFSET, fsr)) return false;
    uint32_t fl = (fsr & XCANFD_FSR_FL_MASK) >> XCANFD_FSR_FL_0_SHIFT;
    if (fl == 0) return false;
    uint32_t ri = fsr & XCANFD_FSR_RI_MASK;

    uint64_t base_id  = XCANFD_RXFIFO_0_BASE_ID_OFFSET + ri * XCANFD_RXFIFO_NEXTID_OFFSET;
    uint64_t base_dlc = XCANFD_RXFIFO_0_BASE_DLC_OFFSET + ri * XCANFD_RXFIFO_NEXTDLC_OFFSET;
    uint64_t base_dw  = XCANFD_RXFIFO_0_BASE_DW0_OFFSET + ri * XCANFD_RXFIFO_NEXTDW_OFFSET;

    uint32_t idr = 0, dlcr = 0;
    if (!rd32(base_id, idr)) return false;
    if (!rd32(base_dlc, dlcr)) return false;

    bool ide = (idr & XCANFD_IDR_IDE_MASK) != 0;
    bool rtr = (idr & XCANFD_IDR_RTR_MASK) != 0;
    uint32_t id = 0;
    if (ide) {
        uint32_t high11 = (idr & XCANFD_IDR_ID1_MASK) >> XCANFD_IDR_ID1_SHIFT;
        uint32_t low18  = (idr & XCANFD_IDR_ID2_MASK) >> XCANFD_IDR_ID2_SHIFT;
        id = ((high11 & 0x7FF) << 18) | (low18 & 0x3FFFF);
    } else {
        id = (idr & XCANFD_IDR_ID1_MASK) >> XCANFD_IDR_ID1_SHIFT;
    }
    uint8_t dlc = static_cast<uint8_t>((dlcr & XCANFD_DLCR_DLC_MASK) >> XCANFD_DLCR_DLC_SHIFT);
    bool fdf = (dlcr & XCANFD_DLCR_EDL_MASK) != 0;
    bool brs = (dlcr & XCANFD_DLCR_BRS_MASK) != 0;
    uint8_t dlen = dlc_to_len(dlc);

    out.id = id;
    out.ide = ide;
    out.rtr = rtr;
    out.fdf = fdf;
    out.brs = brs;
    out.dlc = dlc;
    out.data.resize(dlen);

    uint32_t ofs = static_cast<uint32_t>(base_dw);
    for (uint8_t off = 0; off < dlen; off += 4) {
        uint32_t word = 0;
        if (!rd32(ofs, word)) return false;
        uint8_t tmp[4]; unpack_le32(word, tmp);
        uint8_t n = (dlen - off) >= 4 ? 4 : (dlen - off);
        std::memcpy(out.data.data() + off, tmp, n);
        ofs += 4;
    }

    // 递增读索引与清除接收中断
    (void)wr32(XCANFD_FSR_OFFSET, XCANFD_FSR_IRI_MASK);
    (void)wr32(XCANFD_ICR_OFFSET, XCANFD_IXR_RXOK_MASK);
    return true;
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

    if (!write_one_frame_to_txfifo(f)) {
        LOGE("canfd", "send", -EIO, "write txfifo failed");
        return false;
    }
    if (!trigger_transmit()) {
        LOGE("canfd", "send", -EIO, "trigger transmit failed");
        return false;
    }
    return true;
}

int32_t CanFDDevice::receive(uint8_t* buf, uint32_t buf_size) {
    if (!buf || buf_size < 6) return -EINVAL;
    CanFrame f;
    if (!read_one_frame_from_rxfifo(f)) return 0;

    uint8_t dlen = dlc_to_len(f.dlc);
    uint32_t need = 6u + dlen;
    if (buf_size < need) return -ENOSPC;
    unpack_le32(f.id, buf);
    uint8_t flags = (f.ide ? 0x01 : 0) | (f.rtr ? 0x02 : 0) | (f.fdf ? 0x04 : 0) | (f.brs ? 0x08 : 0);
    buf[4] = flags;
    buf[5] = f.dlc;
    if (dlen) std::memcpy(buf + 6, f.data.data(), dlen);
    return static_cast<int32_t>(need);
}

int32_t CanFDDevice::receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) {
    auto& tp = transport();
    uint32_t bm = 0;
    int ev = tp.waitEvent(&bm, timeout_us / 1000);
    if (ev <= 0) return ev == 0 ? 0 : -1;
    return receive(buf, buf_size);
}

int CanFDDevice::ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) {
    (void)opcode; (void)in; (void)in_len; (void)out; (void)out_len;
    return -ENOSYS;
}

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF