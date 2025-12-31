#pragma once
#include <cstddef>
#include <cstdint>
#include "MB_DDF/PhysicalLayer/Device/TransportLinkAdapter.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

class DdrDevice : public TransportLinkAdapter {
public:
    explicit DdrDevice(MB_DDF::PhysicalLayer::ControlPlane::IDeviceTransport& tp, uint16_t mtu)
        : TransportLinkAdapter(tp, mtu) {}
    bool open(const LinkConfig& cfg) override;
    bool close() override;
    bool send(const uint8_t* data, uint32_t len) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override;
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override;
};

}
}
}
