/**
 * @file TestPhysicalLayer.cpp
 * @brief 物理层 UDP 与 RS422-XDMA 适配器端到端测试
 */
#include <cstddef>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <poll.h>
#include <random>
#include <algorithm>

#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Debug/LoggerExtensions.h"
#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
#include "MB_DDF/PhysicalLayer/Device/CanDevice.h"
#include "MB_DDF/PhysicalLayer/Device/Rs422Device.h"
#include "MB_DDF/PhysicalLayer/Device/HelmDevice.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
#include "MB_DDF/Timer/ChronoHelper.h"

// --- 测试用例 ---
using namespace MB_DDF::PhysicalLayer;

static void cleanup_udp_links(DataPlane::UdpLink& link1, DataPlane::UdpLink& link2) {
    link1.close();
    link2.close();
    assert(link1.getStatus() == LinkStatus::CLOSED);
    assert(link2.getStatus() == LinkStatus::CLOSED);
    LOG_INFO << "Links closed and getStatus() returns CLOSED.";
}

static void cleanup_device(Device::TransportLinkAdapter& device, ControlPlane::IDeviceTransport& transport) {
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
    if (fd1 <= 0 || fd2 <= 0) {
        LOG_ERROR << "getIOFd() failed. fd1=" << fd1 << ", fd2=" << fd2;
        cleanup_udp_links(link1, link2);
        return;
    }
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
    int32_t recv_len = link1.receive(recv_buf.data(), recv_buf.size(), 1000 * 1000 /*1s*/);

    if (recv_len > 0) {
        LOG_INFO << "receive() with timeout got " << recv_len << " bytes.";
        assert(recv_len == (int32_t)send_buf.size());
        assert(memcmp(send_buf.data(), recv_buf.data(), recv_len) == 0);
        LOG_INFO << "Received data matches sent data.";

        bool echoed = link1.send(recv_buf.data(), recv_len);
        if (echoed) {
            LOG_INFO << "Echo sent back to Link2.";
        } else {
            LOG_ERROR << "Echo send failed.";
            cleanup_udp_links(link1, link2);
            return;
        }

        std::vector<uint8_t> recv_back_buf(default_mtu, 0);
        int32_t echoed_len = link2.receive(recv_back_buf.data(), recv_back_buf.size(), 1000 * 1000 /*1s*/);
        if (echoed_len > 0) {
            LOG_INFO << "Link2 received echo " << echoed_len << " bytes.";
            assert(echoed_len == recv_len);
            assert(memcmp(recv_back_buf.data(), recv_buf.data(), echoed_len) == 0);
            LOG_INFO << "Echo data matches received data.";
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
    if (ioctl_ret != -ENOTSUP) {
        LOG_ERROR << "ioctl() failed. ret=" << ioctl_ret;
        cleanup_udp_links(link1, link2);
        return;
    }
    LOG_INFO << "ioctl() correctly returns -ENOTSUP.";

    // 5. 关闭
    cleanup_udp_links(link1, link2);
}

/**
 * @brief 测试 Rs422Device
 * @note 这个测试依赖于存在的 RS422 驱动和设备节点。
 *       如果环境不满足，open 会失败，但仍能验证接口行为。
 */
void test_rs422_device(int num) {
    LOG_TITLE("RS422 Device Test");
    LOG_INFO << "Testing RS422 device with event number: " << num;

    // 1. 控制面配置与打开
    ControlPlane::XdmaTransport transport;
    TransportConfig cfg_tp;
    cfg_tp.device_path = "/dev/xdma0";
    cfg_tp.event_number = num;
    cfg_tp.device_offset = 0x10000 * num;
    // cfg_tp.device_offset = 0x00000; // 3号引信
    // cfg_tp.device_offset = 0x10000; // 
    // cfg_tp.device_offset = 0x20000; // 
    // cfg_tp.device_offset = 0x30000; // 
    
    LOG_INFO << "Attempting to open XdmaTransport with path: " << cfg_tp.device_path;
    LOG_INFO << "Note: This requires " << cfg_tp.device_path << "_* device nodes.";

    if (!transport.open(cfg_tp)) {
        LOG_ERROR << "XdmaTransport open failed. This is expected if drivers are not loaded.";
        LOG_INFO << "Skipping further RS422 adapter tests.";
        return;
    }
    LOG_INFO << "XdmaTransport open succeeded (or partially succeeded).";
    
    // 2. 适配器配置与打开
    const uint16_t test_mtu = 128;
    Device::Rs422Device adapter_422(transport, test_mtu);

    LinkConfig cfg_link; // 对于设备型链路，此配置通常可为空
    if (!adapter_422.open(cfg_link)) {
        LOG_ERROR << "Rs422Device open failed.";
        cleanup_device(adapter_422, transport);
        return;
    }
    LOG_INFO << "Rs422Device open succeeded.";

    // 3. 状态与能力检查
    // assert(adapter_422.getStatus() == LinkStatus::OPEN);
    // LOG_INFO << "getStatus() returns OPEN.";

    // assert(adapter_422.getMTU() == test_mtu);
    // LOG_INFO << "getMTU() returns correct value (" << test_mtu << ").";
    
    // 4. 收发数据准备
    LOG_INFO << "Calling send/receive (hardware loopback required for data validation).";
    std::vector<uint8_t> buf(test_mtu, 'A');
    std::vector<uint8_t> buf_r(test_mtu, 'B');


    // 5. ioctl 测试
    // - ucr: UART 控制寄存器值（参考: parity/check 等编码）
    // - mcr: 模式控制寄存器值（参考驱动默认 0x20）
    // - brsr: 波特率选择寄存器值
    // - icr: 中断/状态控制寄存器值（通常写 1 以启用状态）
    // - tx_head_lo/hi: 发送头两个字节（低/高）
    // - rx_head_lo/hi: 接收头两个字节（低/高）
    Device::Rs422Device::Config cfg = {
        .ucr = 0x30,
        .mcr = 0x20,
        .brsr = 0x0A,
        .icr = 0x01,
        .tx_head_lo = 0xAA,
        .tx_head_hi = 0x1A,
        .rx_head_lo = 0xAA,
        .rx_head_hi = 0x1A,
        .lpb = 0x00,    // AF = 回环
        .intr = 0xAE,   // AE = 自控中断
        .evt = 1250,    // 125 = 脉冲宽度 1us
    };
    Device::Rs422Device::Config cfg_return;
    int ioctl_ret = adapter_422.ioctl(Device::Rs422Device::IOCTL_CONFIG, 
        &cfg, sizeof(cfg), 
        &cfg_return, sizeof(cfg_return)); 
    if (ioctl_ret != 0) {
        LOG_ERROR << "ioctl() failed. ret=" << ioctl_ret;
        return;
    }
    LOG_INFO << "ioctl() correctly returns 0.";

    // 6. 数据收发验证
    for (int i=0; i<test_mtu; i++) {
        buf.data()[i] = i;
    }
    // adapter_422.send(buf.data(), test_mtu);
    adapter_422.receive(buf_r.data(), buf.size());
    MB_DDF::Timer::ChronoHelper::timingAverage(10000, [&]() {
        static bool err = false;
        if (err) return;
        // buf.data()[1] += 1;
        int received = adapter_422.receive(buf_r.data(), buf.size(), 1000000);
        if (received <= 0) {
            received = adapter_422.receive(buf_r.data(), buf.size());
            LOG_ERROR << "receive() failed or timed out. ret=" << received;
            // err = true;
        } else if (memcmp(buf.data(), buf_r.data(), received) != 0) {
            LOG_ERROR << "Received data does not match sent data.";
            // 打印数据
            for (int i=0; i<20; i++) {
                LOG_ERROR << "Data " << i << " is: " << (uint32_t)(buf.data()[i]) << " and " << (uint32_t)(buf_r.data()[i]);
            }
            // err = true;
            adapter_422.send(buf.data(), test_mtu);
        } else {
            // static size_t count = 0;
            // count++;
            // if (count % 100 == 0) {
            //     LOG_INFO << "recv count: " << count;
            //     LOG_INFO << "recv size: " << received;
            // }
            adapter_422.send(buf.data(), test_mtu);
        }
    });

    // 7. 关闭
    cleanup_device(adapter_422, transport);
}

// 测试舵机控制接口
void test_helm_transport() {   
    LOG_TITLE("Helm Transport Test");

    ControlPlane::XdmaTransport tp_helm;
    TransportConfig cfg_helm;
    cfg_helm.device_offset = 0x60000;

    tp_helm.open(cfg_helm);
    Device::HelmDevice helm(tp_helm, 0);
    LinkConfig cfg_link; 
    helm.open(cfg_link);
    // helm 配置
    Device::HelmDevice::Config ctl_helm = {
        .pwm_freq = 4000,
        .out_enable = 0xFFFF,
        .ad_filter = 0,
    };
    helm.ioctl(Device::HelmDevice::IOCTL_HELM, &ctl_helm, sizeof(ctl_helm));

    while(1) {
    // 舵机 Ad 读取、PWM duty 设置
    uint16_t fdb[4];
    uint32_t v_input[4] = {uint32_t(0.1e8), uint32_t(0.3e8), uint32_t(0.5e8), uint32_t(0.7e8)};
    uint32_t v_output[4] = {0, 0, 0, 0};
    helm.receive((uint8_t*)fdb, sizeof(fdb));
    helm.send((uint8_t*)v_input, sizeof(v_input));
    for (uint32_t i=0; i<4; i++) {
        tp_helm.readReg32((i+0xBC)*4, v_output[i]);
        if (v_input[i] >= 0x80000000) {
            v_input[i] = 0xFFFFFFFF - v_input[i];
        }
    }

    // 打印 AD 和 PWM duty
    const float K_IN_OUT = 311.22;
    const float K_IN_OUT_1 = 1 / K_IN_OUT;
    const float AD_SWITCH = -10.0 / 65536.0;
    const float OFFSET = -12.048;
    LOG_INFO << "Helm ad is: " << fdb[0] << " " << fdb[1] << " " << fdb[2] << " " << fdb[3];
    LOG_INFO << "Helm ad real is: " << AD_SWITCH * fdb[0] - OFFSET
    << " " << AD_SWITCH * fdb[1] - OFFSET
    << " " << AD_SWITCH * fdb[2] - OFFSET
    << " " << AD_SWITCH * fdb[3] - OFFSET;
    LOG_INFO << "Helm degree is: " << K_IN_OUT_1 * static_cast<short>(fdb[0]) 
    << " " << K_IN_OUT_1 * static_cast<short>(fdb[1]) 
    << " " << K_IN_OUT_1 * static_cast<short>(fdb[2]) 
    << " " << K_IN_OUT_1 * static_cast<short>(fdb[3]);
    LOG_INFO << "Helm pwm set : " << v_input[0] << " " << v_input[1] << " " << v_input[2] << " " << v_input[3];
    LOG_INFO << "Helm pwm get : " << v_output[0] << " " << v_output[1] << " " << v_output[2] << " " << v_output[3];

    uint32_t status;
    tp_helm.readReg32(0xFF*4, status);
    LOG_INFO << "Helm status is: " << status;
    break;
    // sleep(1);
    }
    // 释放
    cleanup_device(helm, tp_helm);
}

// 测试 CAN 设备
void test_can_transport() {   
    LOG_TITLE("CAN Transport Test");

    ControlPlane::XdmaTransport can;
    TransportConfig cfg_can;
    cfg_can.device_path  = "/dev/xdma0"; // 必须设置设备路径以映射寄存器与事件设备
    cfg_can.device_offset = 0x50000;      // CAN 核心在 user 空间的偏移
    cfg_can.event_number = 5;             // 事件设备编号
    if (!can.open(cfg_can)) {
        LOG_ERROR << "XdmaTransport open failed for CAN (check device nodes).";
        return;
    }

    Device::CanDevice can_dev(can, 8);
    LinkConfig cfg_link; 
    if (!can_dev.open(cfg_link)) {
        LOG_ERROR << "CanDevice open failed.";
        cleanup_device(can_dev, can);
        return;
    }

    // 使用 CanDevice 接口进行 1M 回环自发自收测试（轮询接收）
    LOG_INFO << "Start 1Mbps loopback self-test using CanDevice (polling)";

    // 显式开启回环（open 已默认开启，重复设置以确保状态）
    uint32_t loop_on = 1;
    int lb_ret = can_dev.ioctl(Device::CanDevice::IOCTL_SET_LOOPBACK, &loop_on, sizeof(loop_on));
    if (lb_ret != 0) {
        LOG_WARN << "IOCTL_SET_LOOPBACK returned " << lb_ret << ", continuing.";
    }

    // 设置 1Mbps 位时间（open 默认 1M，如设置失败则继续）
    uint32_t baud_1m = 1000000;
    int bt_ret = can_dev.ioctl(Device::CanDevice::IOCTL_SET_BIT_TIMING, &baud_1m, sizeof(baud_1m));
    if (bt_ret != 0) {
        LOG_WARN << "IOCTL_SET_BIT_TIMING(1M) returned " << bt_ret << ", open defaults to 1M.";
    }

    int pass = 0, fail = 0;
    Device::CanFrame tx;    
    MB_DDF::Timer::ChronoHelper::timingAverage(1000, [&]() {
        static int i = 0;
        tx.id = 0x123;    // 标准帧 ID
        tx.ide = false;
        tx.rtr = false;
        tx.dlc = 8;
        tx.data = {
            static_cast<uint8_t>(i & 0xFF), static_cast<uint8_t>((i+1) & 0xFF),
            static_cast<uint8_t>((i+2) & 0xFF), static_cast<uint8_t>((i+3) & 0xFF),
            static_cast<uint8_t>((i+4) & 0xFF), static_cast<uint8_t>((i+5) & 0xFF),
            static_cast<uint8_t>((i+6) & 0xFF), static_cast<uint8_t>((i+7) & 0xFF)
        };

        // 写入
        bool sent = can_dev.send(tx);
        if (!sent) {
            LOG_ERROR << "send() failed at round " << i;
            ++fail;
            return;
        }

        // 轮询接收：无 event 中断，轮询 receive(CanFrame&)
        Device::CanFrame rx;
        int32_t got = -1;
        got = can_dev.receive(rx, 2000);
        if (got <= 0) {
            LOG_ERROR << "receive() timeout/failed at round " << i << " ret=" << got;
            ++fail;
            return;
        }

        bool ok = (rx.id == tx.id) && (rx.dlc == tx.dlc) && (rx.data == tx.data);
        if (!ok) {
            LOG_ERROR << "mismatch at round " << i
                      << " id tx=0x" << std::hex << tx.id << " rx=0x" << rx.id << std::dec
                      << " dlc tx=" << (int)tx.dlc << " rx=" << (int)rx.dlc;
            for (int k = 0; k < rx.dlc; ++k) {
                LOG_ERROR << "rx[" << k << "]=" << (int)rx.data[k]
                          << ", tx[" << k << "]=" << (int)tx.data[k];
            }
            ++fail;
        } else {
            ++pass;
        }
        ++i;
    });
    LOG_INFO << "CAN loopback self-test done. pass=" << pass << " fail=" << fail;

    // 结束对外发一次
    loop_on = 0;
    lb_ret = can_dev.ioctl(Device::CanDevice::IOCTL_SET_LOOPBACK, &loop_on, sizeof(loop_on));
    if (lb_ret != 0) {
        LOG_WARN << "IOCTL_SET_LOOPBACK(0) returned " << lb_ret << ", continuing.";
    }
    can_dev.send(tx);
    
    // 释放
    cleanup_device(can_dev, can);
}

// 测试 DMA-DDR 接口
void test_ddr_transport() {   
    LOG_TITLE("DDR Transport Test");

    ControlPlane::XdmaTransport ddr;
    TransportConfig cfg_ddr;
    cfg_ddr.dma_h2c_channel = 0;
    cfg_ddr.dma_c2h_channel = 0;
    cfg_ddr.device_offset = 0x0;

    ddr.open(cfg_ddr);

    // 初始化数据
    const size_t DATA_SIZE = 64 * 1024;
    std::vector<uint8_t> data(DATA_SIZE);
    std::vector<uint8_t> back(DATA_SIZE);
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    // 用随机数填充vector：对每个元素调用lambda生成随机数
    std::generate(data.begin(), data.end(), [&]() {
        return dist(rng); // 从分布中生成一个随机数
    });

    // DMA 收发测试
    ddr.continuousWrite(0, data.data(), DATA_SIZE);
    ddr.continuousRead(0, back.data(), DATA_SIZE);

    // 验证数据是否一致
    if (data != back) {
        LOG_ERROR << "DMA read data does not match written data.";
    } else {
        LOG_INFO << "DMA read data matches written data.";
    }

    // 测试发送时长
    LOG_INFO << "Testing DMA write duration (64KB):";
    MB_DDF::Timer::ChronoHelper::timingAverage(3000, [&]() {
        ddr.continuousWrite(0, data.data(), DATA_SIZE);
    });

    // 测试接收时长
    LOG_INFO << "Testing DMA read duration (64KB):";
    MB_DDF::Timer::ChronoHelper::timingAverage(3000, [&]() {
        ddr.continuousRead(0, back.data(), DATA_SIZE);
    });

    // back 数据清零
    std::fill(back.begin(), back.end(), 0);
    // 异步DMA 收发测试
    LOG_INFO << "Testing DMA async write and read duration (64KB):";
    ddr.continuousWriteAsync(0, data.data(), DATA_SIZE, 0);
    usleep(200);
    ddr.continuousReadAsync(0, back.data(), DATA_SIZE, 0);
    usleep(200);

    // 验证数据是否一致
    if (data != back) {
        LOG_ERROR << "DMA read data does not match written data.";
    } else {
        LOG_INFO << "DMA read data matches written data.";
    }

    // back 数据清零
    std::fill(back.begin(), back.end(), 0);
    ddr.drainAioCompletions(1024);
    // 异步DMA 收发时间测试    
    LOG_INFO << "Testing DMA async write and read with callback (64KB):";
    int efd = ddr.getAioEventFd();
    struct pollfd pfd{ efd, POLLIN, 0 };
    ddr.setOnContinuousWriteComplete([&](ssize_t ret) {
        (void)ret;
        MB_DDF::Timer::ChronoHelper::clockEnd(1);
        LOG_INFO << "DMA write completed.";
    }); 
    ddr.setOnContinuousReadComplete([&](ssize_t ret) {
        (void)ret;
        MB_DDF::Timer::ChronoHelper::clockEnd(3);
        LOG_INFO << "DMA read completed.";
    });
    MB_DDF::Timer::ChronoHelper::clockStart(0);
    MB_DDF::Timer::ChronoHelper::clockStart(1);
    ddr.continuousWriteAsync(0, data.data(), DATA_SIZE, 0);
    MB_DDF::Timer::ChronoHelper::clockEnd(0);
    int rc = ::poll(&pfd, 1, -1);
    if (rc < 0) {
        LOG_ERROR << "poll error: " << strerror(errno);
    }
    ddr.drainAioCompletions(10);
    MB_DDF::Timer::ChronoHelper::clockStart(2);
    MB_DDF::Timer::ChronoHelper::clockStart(3);
    ddr.continuousReadAsync(0, back.data(), DATA_SIZE, 0);
    MB_DDF::Timer::ChronoHelper::clockEnd(2);
    rc = ::poll(&pfd, 1, -1);
    if (rc < 0) {
        LOG_ERROR << "poll error: " << strerror(errno);
    }
    ddr.drainAioCompletions(10);

    // 验证数据是否一致
    if (data != back) {
        LOG_ERROR << "DMA read data does not match written data.";
    } else {
        LOG_INFO << "DMA read data matches written data.";
    }

    ddr.close();
    LOG_INFO << "DDR transport closed.";
}

void test_IO_transport() {
    LOG_TITLE("DI/DO Test");

    ControlPlane::XdmaTransport io;
    TransportConfig cfg_io;
    cfg_io.device_offset = 0x90000;

    io.open(cfg_io);
    const uint64_t do_addr_offset = 0;
    const uint64_t di_addr_offset = 0;
    uint32_t switch_do = 1;
    uint32_t get_di = 0;
    while(1) {
        LOG_INFO << "switch_do: " << switch_do;
        for (int i = 1; i < 0x9; i++) {
            io.writeReg32(do_addr_offset + i * 4, switch_do);
        }
        for (int i = 9; i < 0xD; i++) {
            io.readReg32(di_addr_offset + i * 4, get_di);
            LOG_INFO << "di_addr: " << di_addr_offset + i * 4 << ", get_di: " << get_di;
        }
        switch_do = 1 - switch_do;
        sleep(1);
    }
}

void test_any_transport() {
    LOG_TITLE("PCIE-AXI-lite Test");

    ControlPlane::XdmaTransport pcie;
    TransportConfig cfg_pcie;
    cfg_pcie.device_offset = 0x80000;

    pcie.open(cfg_pcie);
    const uint64_t do_addr_offset = 0;
    const uint64_t di_addr_offset = 0;
    const size_t data_size = 64;
    uint32_t test_set[data_size] = {0}, test_get[data_size] = {0};
    for (size_t i = 0; i < data_size; i++) {
        test_set[i] = i * 5;
    }
    while(1) {
        for (size_t i = 0; i < data_size; i++) {
            pcie.writeReg32(do_addr_offset + i * 4, test_set[i]);
        }
        for (size_t i = 0; i < data_size; i++) {
            pcie.readReg32(di_addr_offset + i * 4, test_get[i]);
            LOG_INFO << "rd_wr_addr: " << di_addr_offset + i * 4 << ", get: " << test_get[i];
        }
        sleep(1);
    }
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

    // test_ddr_transport();
    // test_udp_link();
    // test_rs422_device(0);
    // test_rs422_device(1);
    test_rs422_device(2);
    // test_rs422_device(3);
    // test_rs422_device(4);
    // test_can_transport();
    // test_helm_transport();
    // test_IO_transport();
    // test_any_transport();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Physical Layer Test Suite Finished");
    LOG_DOUBLE_SEPARATOR();

    return 0;
}