#include "MB_DDF/PhysicalLayer/Factory/HardwareFactory.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
#include "MB_DDF/PhysicalLayer/Device/CanDevice.h"
#include "MB_DDF/PhysicalLayer/Device/HelmDevice.h"
#include "MB_DDF/PhysicalLayer/Device/Rs422Device.h"
#include "MB_DDF/PhysicalLayer/Device/DdrDevice.h"
#include <string>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Factory {

namespace {
struct HandleImpl : public DDS::Handle {
    ControlPlane::XdmaTransport tp;
    std::unique_ptr<DataPlane::ILink> dev = nullptr;
    uint32_t mtu{1500};

    bool send(const uint8_t* data, uint32_t len) override { return dev->send(data, len); }
    int32_t receive(uint8_t* buf, uint32_t buf_size) override { return dev->receive(buf, buf_size); }
    int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override { return dev->receive(buf, buf_size, timeout_us); }   
    uint32_t getMTU() const override { return mtu; }
};
}

std::unique_ptr<DDS::Handle> HardwareFactory::create(const std::string& name, void* param) {
    auto h = std::make_unique<HandleImpl>();
    TransportConfig tc;
    tc.device_path = "/dev/xdma0";

    if (name == "can") {
        tc.device_offset = 0x50000;
        tc.event_number = 5;
        h->tp.open(tc);
        h->mtu = 8;
        h->dev = std::make_unique<Device::CanDevice>(h->tp, h->mtu);
        LinkConfig lc; 
        h->dev->open(lc);
        uint32_t off = 0; 
        h->dev->ioctl(Device::CanDevice::IOCTL_SET_LOOPBACK, &off, sizeof(off));
        h->dev->ioctl(Device::CanDevice::IOCTL_SET_BIT_TIMING, param, sizeof(Device::CanDevice::BitTiming));
    } else if (name == "helm") {
        tc.device_offset = 0x60000;
        h->tp.open(tc);
        h->mtu = 16;
        h->dev = std::make_unique<Device::HelmDevice>(h->tp, h->mtu);
        LinkConfig lc; h->dev->open(lc);
        h->dev->ioctl(Device::HelmDevice::IOCTL_HELM, param, sizeof(Device::HelmDevice::Config));
    } else if (name == "rs422_1") {
        tc.device_offset = 0;
        tc.event_number = 0;
        h->tp.open(tc);
        h->mtu = 255;
        h->dev = std::make_unique<Device::Rs422Device>(h->tp, h->mtu);
        LinkConfig lc; 
        h->dev->open(lc);
        Device::Rs422Device::Config cfg_return;
        h->dev->ioctl(Device::Rs422Device::IOCTL_CONFIG, 
            param, sizeof(Device::Rs422Device::Config), 
            &cfg_return, sizeof(Device::Rs422Device::Config)); 
    } else if (name == "ddr") {
        tc.dma_h2c_channel = 0;
        tc.dma_c2h_channel = 0;
        tc.device_offset = 0x0;
        h->tp.open(tc);
        h->mtu = 64 * 1024;
        h->dev = std::make_unique<Device::DdrDevice>(h->tp, h->mtu);
        LinkConfig lc; 
        h->dev->open(lc);
    } else if (name == "udp") {
        h->mtu = 60000;
        h->dev = std::make_unique<DataPlane::UdpLink>();
        LinkConfig lc; 
        lc.name = std::string((const char*)param);
        lc.mtu = h->mtu;
        h->dev->open(lc);
    } else {
        return nullptr;
    }
    return h;
}

} // namespace Factory
} // namespace PhysicalLayer
} // namespace MB_DDF