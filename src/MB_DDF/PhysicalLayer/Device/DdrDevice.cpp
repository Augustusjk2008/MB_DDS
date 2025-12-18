#include "MB_DDF/PhysicalLayer/Device/DdrDevice.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"
#include <unistd.h>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

bool DdrDevice::open(const LinkConfig& cfg) {
    return TransportLinkAdapter::open(cfg);
}

bool DdrDevice::close() {
    return TransportLinkAdapter::close();
}

bool DdrDevice::send(const uint8_t* data, uint32_t len) {
    if (!data || len == 0) return true;
    if (transport().continuousWriteAsync(0, data, len, 0)) return true;
    return transport().continuousWrite(0, data, len);
}

int32_t DdrDevice::receive(uint8_t* buf, uint32_t buf_size) {
    if (!buf || buf_size == 0) return 0;
    bool ok = transport().continuousRead(0, buf, buf_size);
    if (!ok) return 0;
    return static_cast<int32_t>(buf_size);
}

int32_t DdrDevice::receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) {
    auto& tp = transport();
    if (!buf || buf_size == 0) return 0;

    if (tp.getEventFd() >= 0) {
        uint32_t bitmap = 0;
        int ev = tp.waitEvent(&bitmap, timeout_us / 1000);
        if (ev <= 0) return ev == 0 ? 0 : -1;
        return receive(buf, buf_size);
    }

    const uint32_t step_us = 100;
    uint32_t waited = 0;
    while (waited <= timeout_us) {
        int32_t n = receive(buf, buf_size);
        if (n != 0) return n;
        usleep(step_us);
        waited += step_us;
    }
    return 0;
}

int DdrDevice::ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) {
    (void)opcode; (void)in; (void)in_len; (void)out; (void)out_len;
    return -38;
}

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF
