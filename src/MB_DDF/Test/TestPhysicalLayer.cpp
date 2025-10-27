/**
 * @file TestPhysicalLayer.cpp
 * @brief 物理层 UDP 与 RS422-XDMA 适配器端到端测试
 */
#include <cstdint>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <poll.h>

#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
#include "MB_DDF/PhysicalLayer/Device/Rs422Device.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
#include "MB_DDF/Debug/LoggerExtensions.h"

// --- 测试用例 ---
using namespace MB_DDF::PhysicalLayer;

static void cleanup_udp_links(DataPlane::UdpLink& link1, DataPlane::UdpLink& link2) {
    link1.close();
    link2.close();
    assert(link1.getStatus() == LinkStatus::CLOSED);
    assert(link2.getStatus() == LinkStatus::CLOSED);
    LOG_INFO << "Links closed and getStatus() returns CLOSED.";
}

static void cleanup_rs422(Device::Rs422Device& device, ControlPlane::XdmaTransport& transport) {
    device.close();
    transport.close();
    assert(device.getStatus() == LinkStatus::CLOSED);
    LOG_INFO << "Device closed and getStatus() returns CLOSED.";
}

/**
 * @brief 测试 UdpLink 的核心功能
 */
void test_udp_link() {
    LOG_TITLE("UDP Link Test");

    DataPlane::UdpLink link1, link2;
    const uint16_t default_mtu = 60000;

    // 1. 配置与打开
    LinkConfig cfg1, cfg2;
    cfg1.name = "12345"; // 作为服务端，格式："<local_port>"
    cfg1.mtu = default_mtu;
    cfg2.name = "127.0.0.1:12346|127.0.0.1:12345"; // 作为客户端，连接到 link1，格式："<local_ip>:<local_port>|<remote_ip>:<remote_port>"
    cfg2.mtu = default_mtu;

    bool ok1 = link1.open(cfg1);
    bool ok2 = link2.open(cfg2);

    if (ok1 && ok2) {
        LOG_INFO << "UDP links opened successfully.";
    } else {
        LOG_ERROR << "Failed to open UDP links.";
        if (!ok1) LOG_ERROR << "  - Link1 failed.";
        if (!ok2) LOG_ERROR << "  - Link2 failed.";
        return;
    }

    // 2. 状态与能力检查
    assert(link1.getStatus() == LinkStatus::OPEN);
    assert(link2.getStatus() == LinkStatus::OPEN);
    LOG_INFO << "getStatus() returns OPEN.";

    assert(link1.getMTU() == default_mtu);
    assert(link2.getMTU() == default_mtu);
    LOG_INFO << "getMTU() returns correct value.";

    int fd1 = link1.getIOFd();
    int fd2 = link2.getIOFd();
    assert(fd1 > 0 && fd2 > 0);
    LOG_INFO << "getIOFd() returns valid file descriptors.";
    assert(link1.getEventFd() == fd1);
    LOG_INFO << "getEventFd() returns same fd as getIOFd().";

    // 3. 数据收发测试
    std::vector<uint8_t> send_buf(default_mtu, 0xA5);
    std::vector<uint8_t> recv_buf(default_mtu, 0x5A);

    bool sent = link2.send(send_buf.data(), send_buf.size());
    if (sent) {
        LOG_INFO << "send() executed successfully.";
    } else {
        LOG_ERROR << "send() failed.";
        cleanup_udp_links(link1, link2);
        return;
    }

    // 使用带超时的 receive
    DataPlane::Endpoint src;
    int32_t recv_len = link1.receive(recv_buf.data(), recv_buf.size(), src, 1000 * 1000 /*1s*/);

    if (recv_len > 0) {
        LOG_INFO << "receive() with timeout got " << recv_len << " bytes.";
        assert(recv_len == (int32_t)send_buf.size());
        assert(memcmp(send_buf.data(), recv_buf.data(), recv_len) == 0);
        LOG_INFO << "Received data matches sent data.";
        // 对于 UDP，channel_id 应该是源端口
        assert(src.channel_id == 12346);
        LOG_INFO << "Source endpoint (port) is correct.";

        bool echoed = link1.send(recv_buf.data(), recv_len);
        if (echoed) {
            LOG_INFO << "Echo sent back to Link2.";
        } else {
            LOG_ERROR << "Echo send failed.";
            cleanup_udp_links(link1, link2);
            return;
        }

        std::vector<uint8_t> recv_back_buf(default_mtu, 0);
        DataPlane::Endpoint echo_src;
        int32_t echoed_len = link2.receive(recv_back_buf.data(), recv_back_buf.size(), echo_src, 1000 * 1000 /*1s*/);
        if (echoed_len > 0) {
            LOG_INFO << "Link2 received echo " << echoed_len << " bytes.";
            assert(echoed_len == recv_len);
            assert(memcmp(recv_back_buf.data(), recv_buf.data(), echoed_len) == 0);
            LOG_INFO << "Echo data matches received data.";
            assert(echo_src.channel_id == 12345);
            LOG_INFO << "Echo source endpoint (port) is correct.";
        } else {
            LOG_ERROR << "Link2 receive echo failed or timed out. ret=" << echoed_len;
            cleanup_udp_links(link1, link2);
            return;
        }
    } else {
        LOG_ERROR << "receive() with timeout failed or timed out. ret=" << recv_len;
    }
    
    // 4. ioctl 测试
    int ioctl_ret = link1.ioctl(0, nullptr, 0, nullptr, 0);
    assert(ioctl_ret == -ENOTSUP);
    LOG_INFO << "ioctl() correctly returns -ENOTSUP.";

    // 5. 关闭
    cleanup_udp_links(link1, link2);
}

/**
 * @brief 测试 Rs422Device
 * @note 这个测试依赖于存在的 RS422 驱动和设备节点（/dev/rs422/rs422_*）。
 *       如果环境不满足，open 会失败，但仍能验证接口行为。
 */
void test_rs422_device() {
    LOG_TITLE("RS422 Device Test");

    // 1. 控制面配置与打开
    ControlPlane::XdmaTransport transport;
    TransportConfig cfg_tp;
    cfg_tp.device_path = "rs422_0";
    cfg_tp.dma_h2c_channel = 0;
    cfg_tp.dma_c2h_channel = 0;
    cfg_tp.event_number = 0;
    
    LOG_INFO << "Attempting to open XdmaTransport with path: " << cfg_tp.device_path;
    LOG_INFO << "Note: This requires /dev/xdma/" << cfg_tp.device_path << "_* device nodes.";

    if (!transport.open(cfg_tp)) {
        LOG_ERROR << "XdmaTransport open failed. This is expected if drivers are not loaded.";
        LOG_INFO << "Skipping further RS422 adapter tests.";
        return;
    }
    LOG_INFO << "XdmaTransport open succeeded (or partially succeeded).";
    
    // 2. 适配器配置与打开
    const uint16_t test_mtu = 2048;
    Device::Rs422Device adapter(transport, test_mtu);

    LinkConfig cfg_link; // 对于设备型链路，此配置通常可为空
    if (!adapter.open(cfg_link)) {
        LOG_ERROR << "Rs422Device open failed.";
        cleanup_rs422(adapter, transport);
        return;
    }
    LOG_INFO << "Rs422Device open succeeded.";

    // 3. 状态与能力检查
    assert(adapter.getStatus() == LinkStatus::OPEN);
    LOG_INFO << "getStatus() returns OPEN.";

    assert(adapter.getMTU() == test_mtu);
    LOG_INFO << "getMTU() returns correct value (" << test_mtu << ").";
    
    // 4. 数据收发（仅调用，不校验数据，因为没有硬件环回）
    LOG_INFO << "Calling send/receive (hardware loopback required for data validation).";
    std::vector<uint8_t> buf(128, 'A');
    DataPlane::Endpoint ep{0};

    // send/receive 依赖于 DMA 文件描述符是否成功打开
    if (transport.getEventFd() >= 0) { // 简单假设 event_fd 存在则 dma fd 也存在
        bool sent = adapter.send(buf.data(), buf.size(), ep);
        LOG_INFO << "send() returned: " << (sent ? "true" : "false");
        
        DataPlane::Endpoint src_ep;
        int32_t received = adapter.receive(buf.data(), buf.size(), src_ep, 100);
        LOG_INFO << "receive() returned: " << received;
    } else {
        LOG_INFO << "Skipping send/receive calls as transport resources are unavailable.";
    }

    // 5. ioctl 测试
    int ioctl_ret = adapter.ioctl(123, nullptr, 0, nullptr, 0); // 123 是一个虚拟操作码
    assert(ioctl_ret == -ENOSYS);
    LOG_INFO << "ioctl() correctly returns -ENOSYS as it's not implemented yet.";

    // 6. 关闭
    cleanup_rs422(adapter, transport);
}

// --- 主函数 ---
int main() {
    LOG_SET_LEVEL_INFO();
    LOG_DISABLE_TIMESTAMP();
    LOG_DISABLE_FUNCTION_LINE();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Starting Physical Layer Test Suite");
    LOG_DOUBLE_SEPARATOR();
    LOG_BLANK_LINE();

    test_udp_link();
    test_rs422_device();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Physical Layer Test Suite Finished");
    LOG_DOUBLE_SEPARATOR();

    return 0;
}