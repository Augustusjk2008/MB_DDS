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
#include "MB_DDF/PhysicalLayer/Device/CanFdDevice.h"
#include "MB_DDF/PhysicalLayer/Hardware/pl_canfd.h"
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
    assert(ioctl_ret == -ENOTSUP);
    LOG_INFO << "ioctl() correctly returns -ENOTSUP.";

    // 5. 关闭
    cleanup_udp_links(link1, link2);
}

/**
 * @brief 测试 Rs422Device
 * @note 这个测试依赖于存在的 RS422 驱动和设备节点。
 *       如果环境不满足，open 会失败，但仍能验证接口行为。
 */
void test_rs422_device() {
    LOG_TITLE("RS422 Device Test");

    // 1. 控制面配置与打开
    ControlPlane::XdmaTransport transport;
    TransportConfig cfg_tp;
    cfg_tp.device_path = "/dev/xdma0";
    cfg_tp.event_number = 4;
    cfg_tp.device_offset = 0x40000;
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
    assert(adapter_422.getStatus() == LinkStatus::OPEN);
    LOG_INFO << "getStatus() returns OPEN.";

    assert(adapter_422.getMTU() == test_mtu);
    LOG_INFO << "getMTU() returns correct value (" << test_mtu << ").";
    
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
        .tx_head_hi = 0x55,
        .rx_head_lo = 0xAA,
        .rx_head_hi = 0x55,
    };
    Device::Rs422Device::Config cfg_return;
    int ioctl_ret = adapter_422.ioctl(Device::Rs422Device::IOCTL_CONFIG, 
        &cfg, sizeof(cfg), 
        &cfg_return, sizeof(cfg_return)); 
    assert(ioctl_ret == 0);
    LOG_INFO << "ioctl() correctly returns 0.";

    // 6. 数据收发验证
    for (int i=0; i<test_mtu; i++) {
        buf.data()[i] = i;
    }
    adapter_422.receive(buf_r.data(), buf.size(), 10000);
    LOG_INFO << "Clear old data.";
    MB_DDF::Timer::ChronoHelper::timingAverage(1000, [&]() {
        static bool err = false;
        if (err) return;
        buf.data()[1] += 1;
        adapter_422.send(buf.data(), test_mtu);
        int received = adapter_422.receive(buf_r.data(), buf.size(), 100000);
        if (received != test_mtu) {
            LOG_ERROR << "receive() failed or timed out. ret=" << received;
            // err = true;
        } else if (memcmp(buf.data(), buf_r.data(), received) != 0) {
            LOG_ERROR << "Received data does not match sent data.";
            // 打印数据
            for (int i=0; i<20; i++) {
                LOG_ERROR << "Data " << i << " is: " << (uint32_t)(buf.data()[i]) << " and " << (uint32_t)(buf_r.data()[i]);
            }
            err = true;
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
    // 舵机 Ad 读取、PWM duty 设置
    uint16_t fdb[4];
    uint32_t v_input[4] = {0x11111234, 0x55555678, 0x99999ABC, 0xEEEEEDF0};
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
    LOG_INFO << "Helm ad is: " << fdb[0] << " " << fdb[1] << " " << fdb[2] << " " << fdb[3];
    LOG_INFO << "Helm degree is: " << K_IN_OUT_1 * static_cast<short>(fdb[0]) 
    << " " << K_IN_OUT_1 * static_cast<short>(fdb[1]) 
    << " " << K_IN_OUT_1 * static_cast<short>(fdb[2]) 
    << " " << K_IN_OUT_1 * static_cast<short>(fdb[3]);
    LOG_INFO << "Helm pwm set : " << v_input[0] << " " << v_input[1] << " " << v_input[2] << " " << v_input[3];
    LOG_INFO << "Helm pwm get : " << v_output[0] << " " << v_output[1] << " " << v_output[2] << " " << v_output[3];

    // 释放
    cleanup_device(helm, tp_helm);
}

// 测试 CANFD 设备
void test_canfd_transport() {   
    LOG_TITLE("CANFD Transport Test");

    ControlPlane::XdmaTransport canfd;
    TransportConfig cfg_canfd;
    cfg_canfd.device_offset = 0x80000;
    cfg_canfd.event_number = 5;
    canfd.open(cfg_canfd);

    Device::CanFDDevice canfd_dev(canfd, 64);
    LinkConfig cfg_link; 
    cfg_link.mtu = 64;
    // canfd_dev.open(cfg_link);

    #define CAN_MODE 1
    #define CAN_FD_MODE 0 
    // Simple CAN mode
    #if CAN_MODE == 1 && CAN_FD_MODE == 0
    
    uint32_t reg_val = 0;
    uint32_t ecr_val = 0;
    const uint32_t TX_DATA = 0x11223344;  // 待发送数据（4字节，对应DB0~DB3）
    const uint32_t TX_ID = 0x123;         // 标准CAN ID（11位，范围0~0x7FF）
    const uint8_t TX_DLC = 4;             // 发送数据长度（1~8字节）
    uint32_t rx_id = 0;                   // 接收ID缓存
    uint8_t rx_dlc = 0;                   // 接收DLC缓存
    uint32_t rx_data = 0;                 // 接收数据缓存
    LOG_INFO << "CAN Device Opened.";

    /****************************************************************************
     * 步骤1：软件复位（手册Table7/8：Software Reset Register (SRR)，地址0x000）
     * 要求：1. 写SRR的bit31（SRST）=1触发复位；2. 复位后SRST自动清0；3. 等待≥16个AXI时钟
     ***************************************************************************/
    // 触发软件复位（SRR=0x1：bit31=1，其他保留位=0）
    canfd_dev.wr32(0x000, 0x1);
    usleep(100);  // AXI时钟100MHz时，1us > 16个周期（160ns），确保复位完成

    // 验证复位：读取SRR，确认bit31（SRST）已自动清0
    canfd_dev.rd32(0x000, reg_val);
    LOG_INFO << "1. SRR复位后值: 0x" << std::hex << reg_val 
             << "（预期：0x00000000，SRST位自动清零）";
    if ((reg_val & 0x1) != 0) {  // 检查bit31是否为0
        LOG_ERROR << "软件复位失败！SRST位未自动清零";
        return;
    }

    /****************************************************************************
     * 步骤2：进入配置模式（手册Table8/20：SRR的CEN位+SR的CONFIG位，地址0x000/0x018）
     * 要求：1. 写SRR的bit30（CEN）=0进入配置模式；2. 验证SR的bit31（CONFIG）=1
     ***************************************************************************/
    // 写SRR=0x00000000（CEN=0，进入配置模式）
    canfd_dev.wr32(0x000, 0x00000000);
    usleep(100);

    // 验证配置模式：读取Status Register (SR)，确认bit31（CONFIG）=1
    canfd_dev.rd32(0x018, reg_val);
    LOG_INFO << "2. SR配置模式标志: 0x" << std::hex << reg_val 
             << "（预期：bit31=1，CONFIG模式生效）";
    if ((reg_val & 0x1) != 0x1) {  // 检查bit31是否为1
        LOG_ERROR << "未进入配置模式！SR的CONFIG位未置1";
        return;
    }

    /****************************************************************************
     * 步骤3：配置回环模式（手册Table9/10：Mode Select Register (MSR)，地址0x004）
     * 要求：1. 回环模式需LBACK=1（bit30=1）；2. SLEEP=0（bit31=0，避免冲突）；3. 两者不可同时为1
     ***************************************************************************/
    // 写MSR=0x2（bit30=1=LBACK，bit31=0=SLEEP，其他保留位=0）
    canfd_dev.wr32(0x004, 0x2);
    usleep(100);

    // 验证回环模式配置：读取MSR，确认bit30=1、bit31=0
    canfd_dev.rd32(0x004, reg_val);
    LOG_INFO << "3. MSR回环模式配置: 0x" << std::hex << reg_val 
             << "（预期：0x00000002，LBACK=1、SLEEP=0）";
    if ((reg_val & 0x3) != 0x2) {  // 检查bit30-31=01
        LOG_ERROR << "回环模式配置失败！LBACK/SLEEP位错误";
        return;
    }

    /****************************************************************************
    * 步骤4：配置1M波特率（CAN_CLK=24M，手册Table11-14）
    * 计算依据：
    * 1. (BRP+1)*(1+TS1+TS2) = 24M/1M =24 → 选BRP=1（BRP+1=2），1+TS1+TS2=12
    * 2. TS1=8tq（TSEG1=7）、TS2=3tq（TSEG2=2）、SJW=1tq（SJW=0）
    ***************************************************************************/
    // 4.1 配置BRPR（0x008）：BRP=1→写入0x01（大端序bit7-0=0x01）
    canfd_dev.wr32(0x008, 0x00000001);
    usleep(100);
    canfd_dev.rd32(0x008, reg_val);
    LOG_INFO << "4.1 BRPR配置: 0x" << std::hex << reg_val << "（预期：0x00000001）";
    if (reg_val != 0x00000001) {
        LOG_ERROR << "BRPR配置失败！";
        return;
    }

    // 4.2 配置BTR（0x00C）：TS1=0x07、TS2=0x02、SJW=0x00→写入0x000001C7
    canfd_dev.wr32(0x00C, 0x000001C7);
    usleep(100);
    canfd_dev.rd32(0x00C, reg_val);
    LOG_INFO << "4.2 BTR配置: 0x" << std::hex << reg_val << "（预期：0x000001C7）";
    if (reg_val != 0x000001C7) {
        LOG_ERROR << "BTR配置失败！";
        return;
    }

    /****************************************************************************
     * 步骤5：配置验收滤波器（接收所有帧，手册Table34-37：AFR+AFMR1+AFIR1，地址0x060/0x064/0x068）
     * 要求：1. 启用1个滤波器（UAF1=1）；2. 掩码全0（接收任意ID）；3. 先禁用滤波器再配置
     ***************************************************************************/
    // 5.1 禁用滤波器1：写AFR（0x060）的bit31（UAF1）=0
    canfd_dev.wr32(0x060, 0x00000000);
    usleep(100);

    // 5.2 等待滤波器空闲：读取SR的bit20（手册编号）→实际bit11→掩码0x00000800
    do {
        canfd_dev.rd32(0x018, reg_val);
    } while ((reg_val & 0x00000800) != 0);  // 正确掩码：0x00000800
    LOG_INFO << "5.1 滤波器已空闲（ACFBSY=0）";

    // 5.3 配置滤波器1掩码（AFMR1=0x00000000，全0掩码→不限制ID）
    canfd_dev.wr32(0x064, 0x00000000);
    usleep(100);
    canfd_dev.rd32(0x064, reg_val);
    LOG_INFO << "5.2 AFMR1（掩码）: 0x" << std::hex << reg_val << "（预期：0x00000000）";

    // 5.4 配置滤波器1ID（AFIR1=0x00000000，掩码全0时ID无意义）
    canfd_dev.wr32(0x068, 0x00000000);
    usleep(100);
    canfd_dev.rd32(0x068, reg_val);
    LOG_INFO << "5.3 AFIR1（ID）: 0x" << std::hex << reg_val << "（预期：0x00000000）";

    // 5.5 启用滤波器1：写AFR的bit31（UAF1）=1
    canfd_dev.wr32(0x060, 0x1);
    usleep(100);
    canfd_dev.rd32(0x060, reg_val);
    LOG_INFO << "5.4 AFR（滤波器使能）: 0x" << std::hex << reg_val << "（预期：0x1）";
    if ((reg_val & 0x1) != 0x1) {
        LOG_WARN << "滤波器1启用失败！UAF1位未置1";
    }

    /****************************************************************************
     * 步骤6：启用核心，退出配置模式（手册Table8/20：SRR的CEN位+SR的模式位，地址0x000/0x018）
     * 要求：1. 写SRR的bit30（CEN）=1启用核心；2. 核心检测11个隐性位后进入回环模式；3. 验证模式位
     ***************************************************************************/
    // 写SRR=0x40000000（CEN=1，启用核心）
    canfd_dev.wr32(0x000, 0x2);
    usleep(10);  // 等待核心检测11个隐性位（回环模式下自动生成）

    // 验证模式：读取SR，确认CONFIG=0（bit31=0）、LBACK=1（bit30=1
    canfd_dev.rd32(0x018, reg_val);
    LOG_INFO << "6. SR核心状态: 0x" << std::hex << reg_val 
            //  << "（预期：bit31=0、bit30=1、bit28=1）";
             << "（预期：bit31=0、bit30=1）";
    // if (((reg_val & 0x1) != 0) ||  // CONFIG=1错误
    //     ((reg_val & 0x2) == 0) ||  // LBACK=0错误
    //     ((reg_val & 0x8) == 0)) {  // NORMAL=0错误
    //     LOG_ERROR << "核心未进入回环模式！状态位错误";
    //     return;
    // }
    if (((reg_val & 0x1) != 0) ||  // CONFIG=1错误
        ((reg_val & 0x2) == 0)) {  // LBACK=0错误
        LOG_ERROR << "核心未进入回环模式！状态位错误";
        return;
    }

    /****************************************************************************
     * 步骤7：填充TX FIFO（手册Table6：TX FIFO地址0x030~0x03C）
     * 要求：1. 按ID→DLC→DataWord1→DataWord2顺序写；2. 标准ID需配置IDE=0
     ***************************************************************************/
    // 7.1 配置TX ID（0x030）：标准ID=0x123（bit0-10）、IDE=0（bit12=0）、RTR=0（bit11=0）
    const uint32_t tx_id_reg = (TX_ID & 0x7FF) << 21;  // 11位标准ID占bit0-10，其他位=0
    canfd_dev.wr32(0x030, tx_id_reg);
    LOG_INFO << "7.1 TX FIFO ID: 0x" << std::hex << tx_id_reg << "（实际ID=0x" << TX_ID << "）";

    // 7.2 配置TX DLC（0x034）：DLC=4（bit0-3），其他保留位=0
    const uint32_t tx_dlc_reg = ((uint32_t)TX_DLC & 0x0F) << 28;
    canfd_dev.wr32(0x034, tx_dlc_reg);
    LOG_INFO << "7.2 TX FIFO DLC: 0x" << std::hex << tx_dlc_reg << "（实际DLC=" << (int)TX_DLC << "）";

    // 7.3 配置TX数据（0x038：DataWord1=DB0~DB3，0x03C：DataWord2=DB4~DB7，此处DLC=4故写0）
    canfd_dev.wr32(0x038, TX_DATA);
    canfd_dev.wr32(0x03C, 0x00000000);
    LOG_INFO << "7.3 TX FIFO DataWord1: 0x" << std::hex << TX_DATA;

    /****************************************************************************
     * 步骤8：等待发送&接收完成（手册Table21/22：ISR中断状态，地址0x01C）
     * 回环模式特征：1. 发送成功→ISR的bit30（TXOK）=1；2. 接收成功→ISR的bit27（RXOK）=1
     ***************************************************************************/
    uint32_t timeout = 1000;  // 超时1000ms
    bool tx_rx_done = false;
    while (timeout--) {
        canfd_dev.rd32(0x01C, reg_val);
        if (((reg_val & 0x2) != 0) &&  // TXOK=1（bit30）
            ((reg_val & 0x10) != 0)) {  // RXOK=1（bit27）
            LOG_INFO << "8. ISR状态: 0x" << std::hex << reg_val 
                     << "（TXOK=1、RXOK=1，发送接收完成）";
            tx_rx_done = true;
            break;
        }
        usleep(1000);  // 每1ms轮询一次
    }
    if (!tx_rx_done) {
        LOG_ERROR << "发送接收超时！ISR=0x" << std::hex << reg_val;
        return;
    }

    /****************************************************************************
     * 步骤9：读取RX FIFO并验证（手册Table6：RX FIFO地址0x050~0x05C）
     * 要求：1. 按ID→DLC→DataWord1→DataWord2顺序读；2. 验证ID、DLC、数据一致性
     ***************************************************************************/
    // 9.1 读取RX ID（0x050）：提取bit0-10为标准ID
    canfd_dev.rd32(0x050, reg_val);
    rx_id = (reg_val >> 21) & 0x7FF;  // 提取bit21-31为标准ID
    LOG_INFO << "9.1 RX FIFO ID: 0x" << std::hex << reg_val << "（提取ID=0x" << rx_id << "）";
    if (rx_id != TX_ID) {
        LOG_WARN << "ID不匹配！发送=0x" << std::hex << TX_ID << "，接收=0x" << rx_id;
    }

    // 9.2 读取RX DLC（0x054）：提取bit28-31为DLC
    canfd_dev.rd32(0x054, reg_val);
    rx_dlc = (reg_val >> 28) & 0x0F;
    LOG_INFO << "9.2 RX FIFO DLC: 0x" << std::hex << reg_val << "（提取DLC=" << (int)rx_dlc << "）";
    if (rx_dlc != TX_DLC) {
        LOG_WARN << "DLC不匹配！发送=" << (int)TX_DLC << "，接收=" << (int)rx_dlc;
    }

    // 9.3 读取RX数据（0x058：DataWord1）
    canfd_dev.rd32(0x058, rx_data);
    LOG_INFO << "9.3 RX FIFO DataWord1: 0x" << std::hex << rx_data;
    if (rx_data != TX_DATA) {
        LOG_WARN << "数据不匹配！发送=0x" << std::hex << TX_DATA << "，接收=0x" << rx_data;
    }

    /****************************************************************************
     * 步骤10：清除中断状态（手册Table25/26：ICR中断清除寄存器，地址0x024）
     * 要求：写1到对应位清除ISR状态（TXOK=bit30，RXOK=bit27）
     ***************************************************************************/
    canfd_dev.wr32(0x024, 0x00000012);
    usleep(100);
    canfd_dev.rd32(0x01C, reg_val);
    LOG_INFO << "10. 清除中断后ISR: 0x" << std::hex << reg_val << "（预期：TXOK=0、RXOK=0）";

    /****************************************************************************
     * 步骤11：循环监控状态（可选，验证回环模式稳定性）
     ***************************************************************************/
    LOG_INFO << "\n=== 1M波特率自发自收测试结束，进入循环模式===";// 循环监控：用大端序实际bit掩码
    while (1) {
        sleep(1);
        // 1. 监控核心模式（LBACK=0x2，NORMAL=0x8）
        canfd_dev.rd32(0x018, reg_val);
        LOG_INFO << "Loop: 模式状态（LBACK=" << ((reg_val & 0x2) ? 1 : 0) 
                << "，NORMAL=" << ((reg_val & 0x8) ? 1 : 0) << "）";
        // 2. 监控错误计数器（TEC=手册bit24-31→实际bit7-0；REC=手册bit16-23→实际bit15-8）
        canfd_dev.rd32(0x010, ecr_val);
        const uint8_t tec = ecr_val & 0xFF;  // 实际bit7-0→手册bit24-31（TEC）
        const uint8_t rec = (ecr_val >> 8) & 0xFF;  // 实际bit15-8→手册bit16-23（REC）
        LOG_INFO << "Loop: 错误计数器（TEC=" << (int)tec << "，REC=" << (int)rec << "）";
    }
    return;
    #endif
    // CAN mode
    #if CAN_FD_MODE == 0 && CAN_MODE == 0
    uint32_t v = 0;
    uint32_t ecr_val = 0;
    uint32_t tx_data = 0x11223344;  // 待发送数据（4字节）
    uint32_t rx_data = 0;           // 接收数据缓存
    uint32_t tx_id = 0x123;         // 发送标准CAN ID（11位）
    uint8_t tx_dlc = 4;            // 发送数据长度（4字节）

    /****************************************************************************
     * 步骤1：软件复位（文档《Chapter 2: Software Reset Register》）
     * 要求：复位后等待16个AXI4-Lite时钟周期（此处用usleep简化，实际需按时钟频率计算）
     ***************************************************************************/
    canfd_dev.wr32(0x0000, 0x00000001);  // SRR: SRST=1（软件复位）
    usleep(100);  // AXI时钟100MHz时，1us > 16个周期（160ns）
    // 验证复位：SRST自动清0
    canfd_dev.rd32(0x0000, v);
    LOG_INFO << "1. SRR after reset: 0x" << std::hex << v 
             << "（预期：0x00000000，SRST位自动清零）";
    if ((v & 0x1) != 0) {
        LOG_ERROR << "软件复位失败！";
        return;
    }

    /****************************************************************************
     * 步骤2：进入配置模式（修改寄存器前提，文档《Chapter 2: SRR》）
     ***************************************************************************/
    canfd_dev.wr32(0x0000, 0x00000000);  // SRR: CEN=0（配置模式）
    usleep(100);
    // 验证配置模式：SR寄存器CONFIG位（bit0）=1
    canfd_dev.rd32(0x0018, v);
    LOG_INFO << "2. SR in config mode: 0x" << std::hex << v 
             << "（预期：bit0=1，CONFIG模式标志）";
    if ((v & 0x1) != 1) {
        LOG_ERROR << "未进入配置模式！";
        return;
    }

    /****************************************************************************
     * 步骤3：配置MSR（回环模式+CAN FD禁用，文档《Chapter 2: MSR》）
     * 配置值0x0000002A：
     * - bit1=1（LBACK=1，回环模式）
     * - bit3=1（BRSD=1，禁用CAN FD位率切换）
     * - bit5=1（DPEE=1，禁用FD协议异常检测）
     * - 其他位=0（SLEEP/SNOOP禁用，无冲突）
     ***************************************************************************/
    canfd_dev.wr32(0x0004, 0x0000002A);  // MSR: 回环模式+CAN FD禁用
    usleep(100);
    // 验证MSR配置
    canfd_dev.rd32(0x0004, v);
    LOG_INFO << "3. MSR (loopback mode): 0x" << std::hex << v 
             << "（预期：0x0000002A，LBACK=1、BRSD=1、DPEE=1）";
    if ((v & 0x2A) != 0x2A) {
        LOG_ERROR << "回环模式配置失败！";
        return;
    }

    /****************************************************************************
     * 步骤4：配置1Mbps波特率（仲裁域，CAN 2.0无需数据域配置，文档《Chapter 2: BRPR/BTR》）
     * 假设CAN时钟=40MHz：
     * - BRPR（0x0008）=0 → 预分频=0+1=1 → 量子时钟=40MHz/1=40MHz
     * - BTR（0x000C）=0x00001E07 → TS1=31tq、TS2=8tq、SJW=1tq → 总周期=40tq → 波特率=1Mbps
     ***************************************************************************/
    // 4.1 配置仲裁域预分频
    canfd_dev.wr32(0x0008, 0x00000000);  // BRPR: BRP=0 → 预分频=1
    usleep(100);
    canfd_dev.rd32(0x0008, v);
    LOG_INFO << "4.1 BRPR: 0x" << std::hex << v << "（预期：0x00000000）";

    // 4.2 配置仲裁域位时序
    canfd_dev.wr32(0x000C, 0x00001E07);  // BTR: 1Mbps时序配置
    usleep(100);
    canfd_dev.rd32(0x000C, v);
    LOG_INFO << "4.2 BTR: 0x" << std::hex << v << "（预期：0x00001E07）";

    /****************************************************************************
     * 步骤5：配置RX FIFO滤波（接收所有CAN帧，文档《Chapter 2: Acceptance Filter》）
     * - AFR（0x00E0）: UAF0=1（启用第0组滤波）
     * - AFMR0（0x0A00）: 掩码全0（忽略所有ID位，接收任意ID）
     * - AFIR0（0x0A04）: ID全0（因掩码全0，无需匹配）
     ***************************************************************************/
    canfd_dev.wr32(0x00E0, 0x00000001);  // AFR: UAF0=1（启用滤波0）
    canfd_dev.wr32(0x0A00, 0x00000000);  // AFMR0: 掩码全0（接收所有ID）
    canfd_dev.wr32(0x0A04, 0x00000000);  // AFIR0: ID全0（无意义，因掩码全0）
    usleep(100);
    canfd_dev.rd32(0x00E0, v);
    LOG_INFO << "5. AFR (filter enable): 0x" << std::hex << v << "（预期：0x00000001）";

    /****************************************************************************
     * 步骤6：启用核心，退出配置模式（文档《Chapter 2: SRR》）
     * 要求：CEN=1，核心检测11个隐性位后进入回环模式（NORMAL=1、LBACK=1）
     ***************************************************************************/
    canfd_dev.wr32(0x0000, 0x00000002);  // SRR: CEN=1（启用核心）
    usleep(100);  // 等待核心完成初始化（检测11个隐性位）
    // 验证核心状态：SR寄存器LBACK=1、NORMAL=1、CONFIG=0
    canfd_dev.rd32(0x0018, v);
    LOG_INFO << "6. SR after enable: 0x" << std::hex << v 
             << "（预期：bit1=1（LBACK）、bit3=1（NORMAL）、bit0=0（CONFIG））";
    if (((v & 0x0A) != 0x0A) || ((v & 0x01) != 0)) {
        LOG_ERROR << "核心未进入回环模式！";
        return;
    }

    /****************************************************************************
     * 步骤7：填充TX缓冲区（TB0），准备发送数据（文档《Chapter 2: TX Message Space》）
     * TX缓冲区地址：
     * - TB0-ID（0x0100）: 标准ID=0x123，IDE=0（标准ID），RTR=0（数据帧）
     * - TB0-DLC（0x0104）: DLC=4（4字节数据），FDF=0（CAN模式）
     * - TB0-DW0（0x0108）: 数据=0x11223344（4字节）
     ***************************************************************************/
    // 7.1 配置TX ID（标准ID=0x123，IDE=0，RTR=0）
    uint32_t tx_id_reg = (tx_id & 0x7FF) << 18;  // 标准ID占bit28:18（11位）
    tx_id_reg &= ~(1 << 19);  // IDE=0（标准ID）
    tx_id_reg &= ~(1 << 20);  // SRR/RTR=0（数据帧）
    canfd_dev.wr32(0x0100, tx_id_reg);  // TB0-ID
    LOG_INFO << "7.1 TX TB0-ID: 0x" << std::hex << tx_id_reg << "（ID=0x" << tx_id << "）";

    // 7.2 配置TX DLC（DLC=4，FDF=0）
    uint32_t tx_dlc_reg = (tx_dlc & 0x0F) << 28;  // DLC占bit31:28
    tx_dlc_reg &= ~(1 << 27);  // FDF=0（CAN模式，非FD）
    canfd_dev.wr32(0x0104, tx_dlc_reg);  // TB0-DLC
    LOG_INFO << "7.2 TX TB0-DLC: 0x" << std::hex << tx_dlc_reg << "（DLC=" << (int)tx_dlc << "）";

    // 7.3 配置TX数据（4字节：0x11223344）
    canfd_dev.wr32(0x0108, tx_data);  // TB0-DW0（数据字节0~3）
    LOG_INFO << "7.3 TX TB0-DW0: 0x" << std::hex << tx_data;

    /****************************************************************************
     * 步骤8：触发TX发送（文档《Chapter 2: TX Buffer Ready Request Register》）
     * TRR（0x0090）: RR0=1（触发TB0发送），核心发送完成后自动清RR0
     ***************************************************************************/
    canfd_dev.wr32(0x0090, 0x00000001);  // TRR: RR0=1（触发发送）
    LOG_INFO << "8. Trigger TX: TRR=0x00000001（TB0发送请求）";

    /****************************************************************************
     * 步骤9：等待发送&接收完成（回环模式下TXOK和RXOK均会置位，文档《Chapter 2: ISR》）
     * ISR（0x001C）: 
     * - bit1（TXOK）=1：发送成功
     * - bit4（RXOK）=1：接收成功（内部回环）
     ***************************************************************************/
    uint32_t isr_timeout = 1000;  // 超时1000ms
    while (isr_timeout--) {
        canfd_dev.rd32(0x001C, v);
        if ((v & (1 << 1)) && (v & (1 << 4))) {  // TXOK=1且RXOK=1
            LOG_INFO << "9. ISR: 0x" << std::hex << v 
                     << "（TXOK=1、RXOK=1，发送接收完成）";
            break;
        }
        usleep(2000);  // 每1ms轮询一次
    }
    if (isr_timeout == 0) {
        LOG_ERROR << "9. 发送接收超时！ISR=0x" << std::hex << v;
        return;
    }

    /****************************************************************************
     * 步骤10：读取RX FIFO数据，验证与发送数据一致（文档《Chapter 2: RX Message Space》）
     * RX FIFO-0地址：
     * - RB0-ID（0x2100）: 读取接收的ID
     * - RB0-DLC（0x2104）: 读取接收的DLC
     * - RB0-DW0（0x2108）: 读取接收的数据
     * 读取后需设置FSR的IRI位（bit7），递增读指针（清理FIFO）
     ***************************************************************************/
    // 10.1 读取RX FIFO填充水平（FSR: FL[14:8] > 0表示有数据）
    canfd_dev.rd32(0x00E8, v);
    uint8_t rx_fl = (v >> 8) & 0x7F;  // FL字段（RX FIFO-0填充水平）
    if (rx_fl == 0) {
        LOG_ERROR << "10. RX FIFO无数据！FSR=0x" << std::hex << v;
        return;
    }
    LOG_INFO << "10.1 RX FIFO-0填充水平: " << (int)rx_fl << "（预期≥1）";

    // 10.2 读取RX ID（验证与发送ID一致）
    uint32_t rx_id_reg = 0;
    canfd_dev.rd32(0x2100, rx_id_reg);
    uint32_t rx_id = (rx_id_reg >> 18) & 0x7FF;  // 提取标准ID（bit28:18）
    LOG_INFO << "10.2 RX RB0-ID: 0x" << std::hex << rx_id_reg << "（ID=0x" << rx_id << "）";
    if (rx_id != tx_id) {
        LOG_ERROR << "10. ID不匹配！发送=0x" << std::hex << tx_id << "，接收=0x" << rx_id;
        return;
    }

    // 10.3 读取RX DLC（验证与发送DLC一致）
    uint32_t rx_dlc_reg = 0;
    canfd_dev.rd32(0x2104, rx_dlc_reg);
    uint8_t rx_dlc = (rx_dlc_reg >> 28) & 0x0F;  // 提取DLC（bit31:28）
    LOG_INFO << "10.3 RX RB0-DLC: 0x" << std::hex << rx_dlc_reg << "（DLC=" << (int)rx_dlc << "）";
    if (rx_dlc != tx_dlc) {
        LOG_ERROR << "10. DLC不匹配！发送=" << (int)tx_dlc << "，接收=" << (int)rx_dlc;
        return;
    }

    // 10.4 读取RX数据（验证与发送数据一致）
    canfd_dev.rd32(0x2108, rx_data);  // RB0-DW0（接收数据）
    LOG_INFO << "10.4 RX RB0-DW0: 0x" << std::hex << rx_data;
    if (rx_data != tx_data) {
        LOG_ERROR << "10. 数据不匹配！发送=0x" << std::hex << tx_data << "，接收=0x" << rx_data;
        return;
    }

    // 10.5 清理RX FIFO：设置FSR的IRI位（bit7=1），递增读指针
    canfd_dev.wr32(0x00E8, 0x00000080);  // FSR: IRI=1（清理RX FIFO-0）
    LOG_INFO << "10.5 清理RX FIFO-0：FSR=0x00000080";

    /****************************************************************************
     * 步骤11：循环验证状态（可选，持续监控回环模式稳定性）
     ***************************************************************************/
    LOG_INFO << "\n=== 回环模式测试成功！发送数据=0x" << std::hex << tx_data << " ===";
    while (1) {
        sleep(1);
        // 验证核心模式（LBACK=1、NORMAL=1）
        canfd_dev.rd32(0x0018, v);
        LOG_INFO << "Loop: SR=0x" << std::hex << v 
                 << "（LBACK=" << ((v & 0x02) ? 1 : 0) << "，NORMAL=" << ((v & 0x08) ? 1 : 0) << "）";
        // 验证错误计数器（TEC/REC=0，无错误）
        canfd_dev.rd32(0x0010, ecr_val);
        uint8_t tec = ecr_val & 0xFF;
        uint8_t rec = (ecr_val >> 8) & 0xFF;
        LOG_INFO << "Loop: TEC=" << (int)tec << "，REC=" << (int)rec << "（预期均为0）";
    }

    return ;
    #endif

    // CANFD mode
    #if CAN_FD_MODE == 1
    uint32_t v = 0;
    // 直接寄存器操作，测试CANFD IP核（自发自收模式：Loopback模式+TX→RX内部回环）
    // 步骤 1：软件复位，等待：32 个 AXI4-Lite/APB 时钟周期（usleep(1000)足够）
    canfd_dev.wr32(0x0000, 0x00000001); // SRR: SRST=1触发软复位
    usleep(1000);
    // 验证步骤1：读取SRR寄存器，确认复位触发（SRST位自动清0）
    canfd_dev.rd32(0x0000, v);
    LOG_INFO << "1. Software Reset Register (SRR) after reset: " << std::hex << v 
            << "（预期值：0x0000，SRST位已自动清0）";

    // 步骤 2：进入配置模式（CEN=0）
    canfd_dev.wr32(0x0000, 0x00000000); // SRR: CEN=0，核心进入配置模式
    usleep(1000);
    // 验证步骤2：读取SRR确认CEN=0，读取SR确认CONFIG=1（配置模式）
    canfd_dev.rd32(0x0000, v);
    LOG_INFO << "2. SRR after enter config mode: " << std::hex << v 
            << "（预期值：0x0000，CEN=0）";
    canfd_dev.rd32(0x0018, v);
    LOG_INFO << "2. Status Register (SR) in config mode: " << std::hex << v 
            << "（预期值：CONFIG=1（bit0=1），其他无关位忽略）";

    // 2.1 验证中断状态寄存器（初始状态，无中断）
    canfd_dev.rd32(0x001C, v); // ISR: 中断状态寄存器（原代码误写为ESR，修正）
    usleep(1000);
    LOG_INFO << "2.1 Interrupt Status Register (ISR) initial: " << std::hex << v 
            << "（预期值：0x0000，无初始中断）";

    // 步骤 3：配置模式寄存器（进入Loopback自发自收模式）
    // 文档表2-6：Loopback模式需LBACK=1（bit1）、SLEEP=0（bit0）、SNOOP=0（bit2），其他默认0
    canfd_dev.wr32(0x0004, 0x00000002); // MSR: LBACK=1，启用内部回环
    usleep(1000);
    // 验证MSR配置
    canfd_dev.rd32(0x0004, v);
    LOG_INFO << "3. Mode Select Register (MSR) configured: " << std::hex << v 
            << "（预期值：0x00000002，LBACK=1/SLEEP=0/SNOOP=0）";

            /*
    // 步骤 4：配置仲裁域波特率为 1Mbps（与原配置一致，确保时序匹配）
    // 4.1 仲裁域预分频寄存器（BRPR）：预分频值=0 → 实际分频=0+1=1
    canfd_dev.wr32(0x0008, 0x00000000);
    usleep(1000);
    // 验证4.1：读取BRPR确认值正确
    canfd_dev.rd32(0x0008, v);
    LOG_INFO << "4.1 Arbitration BRPR: " << std::hex << v 
            << "（预期值：0x00000000，预分频=1）";

    // 4.2 仲裁域位时序寄存器（BTR）：TS1=30tq/TS2=8tq/SJW=2tq（原配置不变）
    canfd_dev.wr32(0x000C, 0x00001D07);
    usleep(1000);
    // 验证4.2：读取BTR确认值正确
    canfd_dev.rd32(0x000C, v);
    LOG_INFO << "4.2 Arbitration BTR: " << std::hex << v 
            << "（预期值：0x00001D07，TS1=30tq/TS2=8tq/SJW=2tq）";

    // 步骤 5：配置数据域波特率为 4Mbps（与原配置一致，确保速率切换正常）
    // 5.1 数据域预分频寄存器（DP_BRPR）：预分频值=0 → 实际分频=0+1=1
    canfd_dev.wr32(0x0088, 0x00000000);
    usleep(1000);
    // 验证5.1：读取DP_BRPR确认值正确
    canfd_dev.rd32(0x0088, v);
    LOG_INFO << "5.1 Data Phase DP_BRPR: " << std::hex << v 
            << "（预期值：0x00000000，预分频=1）";

    // 5.2 数据域位时序寄存器（DP_BTR）：DP_TS1=7tq/DP_TS2=2tq/DP_SJW=1tq（原配置不变）
    canfd_dev.wr32(0x008C, 0x00000160);
    usleep(1000);
    // 验证5.2：读取DP_BTR确认值正确
    canfd_dev.rd32(0x008C, v);
    LOG_INFO << "5.2 Data Phase DP_BTR: " << std::hex << v
            << "（预期值：0x00000160，DP_TS1=7tq/DP_TS2=2tq/DP_SJW=1tq）";

    // 步骤 6：配置接收滤波（自发自收需匹配TX ID，启用第0组滤波对）
    // 6.1 配置滤波控制寄存器（AFR）：UAF0=1（启用第0组滤波），其他UAF位=0
    canfd_dev.wr32(0x00E0, 0x00000001); // AFR: UAF0=1，启用滤波对0
    usleep(1000);
    // 6.2 配置滤波掩码寄存器（AFMR0，地址0x0A00）：标准ID全匹配（掩码高11位为1）
    canfd_dev.wr32(0x0A00, 0xFFFFF800); // AFMR0: AMID[28:18]=0xFFF（11位全1），其他位0
    usleep(1000);
    // 6.3 配置滤波ID寄存器（AFIR0，地址0x0A04）：匹配TX的标准ID=0x123（IDE=0）
    // 文档表2-43：AIID[28:18]=0x123（标准ID），AIIDE=0（bit19=0）
    canfd_dev.wr32(0x0A04, 0x00000123 << 18); // AFIR0: 标准ID=0x123，IDE=0
    usleep(1000);
    // 验证步骤6：读取AFR确认滤波启用
    canfd_dev.rd32(0x00E0, v);
    LOG_INFO << "6. Acceptance Filter Register (AFR): " << std::hex << v 
            << "（预期值：0x00000001，UAF0=1，其他UAF位=0）";
            */

    // 步骤 7：启用 CAN FD 核心（CEN=1）
    canfd_dev.wr32(0x0000, 0x00000002); // SRR: CEN=1，核心退出配置模式
    usleep(1000);
    // 验证步骤7：读取SRR确认CEN=1，读取SR确认进入Loopback模式
    canfd_dev.rd32(0x0000, v);
    LOG_INFO << "7. SRR after enable core: " << std::hex << v 
            << "（预期值：0x00000002，CEN=1）";
    canfd_dev.rd32(0x0018, v);
    LOG_INFO << "7. SR after enable core: " << std::hex << v 
            << "（预期值：CONFIG=0（bit0=0）、LBACK=1（bit1=1），无错误）";

    // 步骤 8：等待核心稳定进入Loopback模式
    while (1) {
        usleep(500000);
        canfd_dev.rd32(0x0018, v);
        if ((v & 0x00000002) && !(v & 0x00000001)) { // LBACK=1且CONFIG=0
            LOG_INFO << "8. Core entered Loopback mode successfully: SR=" << std::hex << v;
            break;
        } else {
            // 配置模式寄存器（进入Loopback自发自收模式）
            // 文档表2-6：Loopback模式需LBACK=1（bit1）、SLEEP=0（bit0）、SNOOP=0（bit2），其他默认0
            canfd_dev.wr32(0x0004, 0x00000002); // MSR: LBACK=1，启用内部回环
            usleep(1000);
            // 验证MSR配置
            canfd_dev.rd32(0x0004, v);
            LOG_INFO << "3. Mode Select Register (MSR) configured: " << std::hex << v 
                    << "（预期值：0x00000002，LBACK=1/SLEEP=0/SNOOP=0）";
        }
        LOG_INFO << "8. Waiting for Loopback mode: SR=" << std::hex << v;
    }

    // 步骤 9：发送CAN FD消息（TB0缓冲区，ID=0x123，数据=0x11223344）
    // 9.1 写TX缓冲区TB0的ID（地址0x0100）：标准ID=0x123，IDE=0，SRR/RTR=0
    // 文档表2-31：31:21=ID[28:18]=0x123，19=IDE=0，其他位0
    canfd_dev.wr32(0x0100, 0x00000123 << 18); 
    usleep(1000);
    // 9.2 写TX缓冲区TB0的DLC（地址0x0104）：DLC=4，FDF=1（CAN FD帧），BRS=1（速率切换）
    // 文档表2-32：31:28=DLC=4（0x4），27=FDF=1，26=BRS=1，24=EFC=1（事件记录）
    canfd_dev.wr32(0x0104, 0x40000000 | 0x08000000 | 0x04000000 | 0x01000000); // 0x4D000000
    usleep(1000);
    // 9.3 写TX缓冲区TB0的数据（地址0x0108，DW0）：4字节数据0x11223344
    canfd_dev.wr32(0x0108, 0x11223344);
    usleep(1000);
    // 9.4 触发发送：写TRR寄存器（0x0090）的RR0位（bit0=1）
    canfd_dev.wr32(0x0090, 0x00000001);
    LOG_INFO << "9. Trigger TX: TRR=0x00000001（TB0发送请求）";

    // 步骤10：等待发送完成（轮询TRR的RR0位清0，或TXOK中断）
    uint32_t tx_timeout = 0;
    MB_DDF::Timer::ChronoHelper::clockStart(10);
    while (1) {
        canfd_dev.rd32(0x0090, v);
        if (!(v & 0x00000001)) { // RR0=0表示发送完成
            MB_DDF::Timer::ChronoHelper::clockEnd(10);
            LOG_INFO << "10. TX completed: TRR=" << std::hex << v;
            break;
        }
        tx_timeout++;
        if (tx_timeout > 2000) { // 
            LOG_ERROR << "10. TX timeout!";
            return;
        }
        usleep(500);
    }

    // 步骤11：接收消息（读取RX FIFO-0，验证自发自收数据）
    // 11.1 读取RX FIFO状态寄存器（FSR，0x00E8），确认有数据（FL>0）
    tx_timeout = 0;
    uint32_t rx_fl = 0; // FL[6:0]：RX FIFO-0填充量
    MB_DDF::Timer::ChronoHelper::clockStart(10);
    while (1) {
        usleep(500);
        canfd_dev.rd32(0x00E8, v);
        rx_fl = (v >> 8) & 0x7F; // FL[6:0]：RX FIFO-0填充量
        if (rx_fl > 0) {
            MB_DDF::Timer::ChronoHelper::clockEnd(10);
            break;
        }
        tx_timeout++;
        if (tx_timeout > 2000) { // 超时1s
            LOG_ERROR << "11. RX FIFO-0 empty! FSR=" << std::hex << v;
            break;
        }
    }
    if (rx_fl == 0) {
        LOG_ERROR << "11. RX FIFO-0 empty! FSR=" << std::hex << v;
        return;
    }
    LOG_INFO << "11. RX FIFO-0 has data: FL=" << std::dec << rx_fl << ", FSR=" << std::hex << v;

    // 11.2 读取RX FIFO-0的RB0数据（地址0x2100~0x2108）
    uint32_t rx_id, rx_dlc, rx_data;
    canfd_dev.rd32(0x2100, rx_id); // RB0-ID
    canfd_dev.rd32(0x2104, rx_dlc); // RB0-DLC
    canfd_dev.rd32(0x2108, rx_data); // RB0-DW0
    LOG_INFO << "11. Received data: ID=" << std::hex << rx_id 
            << ", DLC=" << std::hex << rx_dlc 
            << ", Data=" << std::hex << rx_data;

    // 11.3 验证接收数据与发送数据一致
    uint32_t expected_id = 0x00000123 << 18;
    uint32_t expected_dlc = 0x4D000000;
    uint32_t expected_data = 0x11223344;
    if (rx_id == expected_id && rx_dlc == expected_dlc && rx_data == expected_data) {
        LOG_INFO << "11. Self-transmit-receive SUCCESS! Data matched.";
    } else {
        LOG_ERROR << "11. Self-transmit-receive FAILED! Data mismatched.";
        LOG_ERROR << "11. Expected: ID=" << std::hex << expected_id 
                << ", DLC=" << expected_dlc << ", Data=" << expected_data;
    }

    // 11.4 递增RX FIFO-0读索引（更新填充量，准备下次接收）
    canfd_dev.wr32(0x00E8, 0x00000080); // FSR: IRI=1（bit7=1）
    usleep(500);
    canfd_dev.rd32(0x00E8, v);
    LOG_INFO << "11. RX FIFO-0 after IRI update: FSR=" << std::hex << v 
            << "（预期值：FL=" << std::dec << (rx_fl-1) << "）";

    return;
    #endif

    // CANFD mode
    uint32_t baud_Prescaler = 1000000;
    uint32_t baud_Data = 4000000;
    // 设置不滤波 - 接收所有CAN帧
    Device::AXI_CANFD_FILTER no_filter = {
        .uiFilterIndex = 1,  // 使用滤波器索引1
        .uiMask = 0xFFFFFFFF,  // 表示所有位都不关心
        .uiId = 0xFFFFFFFF    // ID 为 0xFFFFFFFF 表示接收所有 ID
    };
    canfd_dev.ioctl(CAN_DEV_OPEN);
    canfd_dev.ioctl(CAN_DEV_SET_BAUD, &baud_Prescaler);
    canfd_dev.ioctl(CAN_DEV_SET_DATA_BAUD, &baud_Data);
    // canfd_dev.ioctl(CAN_DEV_SET_FILTER, &no_filter);

    canfd_dev.__axiCanfdEnterMode(4);
    sleep(1);
    uint32_t mode = canfd_dev.__axiCanfdGetMode();
    LOG_INFO << "CANFD mode is: " << mode;

    // 简单测试：收一帧，然后发一帧
    Device::CanFrame tx_frame = {
        .id = 0x123,
        .ide = false,
        .rtr = false,
        .fdf = true,
        .brs = true,
        .len = 8,
        .data = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
    };
    Device::CanFrame rx_frame;
    for (uint32_t i=0; i<10; i++) {
        LOG_INFO << "Test " << i << "th frame, waiting for 1s.";
        int rx_len = canfd_dev.receive(rx_frame, 1000000);
        if (rx_len <= 0) {
            rx_len = canfd_dev.receive(rx_frame);
            LOG_ERROR << "receive() failed or timed out. ret=" << rx_len;
        } else {
            LOG_INFO << "Received frame id: " << rx_frame.id << " len: " << rx_frame.len;
        }
        canfd_dev.send(tx_frame);
    }

    // 释放
    cleanup_device(canfd_dev, canfd);
}

// 测试 DMA-DDR 接口
void test_ddr_transport() {   
    LOG_TITLE("DDR Transport Test");

    ControlPlane::XdmaTransport ddr;
    TransportConfig cfg_ddr;
    cfg_ddr.dma_h2c_channel = 0;
    cfg_ddr.dma_c2h_channel = 0;
    cfg_ddr.device_offset = 0x80000000;

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

// --- 主函数 ---
int main() {
    LOG_SET_LEVEL_INFO();
    LOG_DISABLE_TIMESTAMP();
    LOG_DISABLE_FUNCTION_LINE();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Starting Physical Layer Test Suite");
    LOG_DOUBLE_SEPARATOR();
    LOG_BLANK_LINE();

    // test_udp_link();
    // test_rs422_device();
    // test_helm_transport();
    // test_ddr_transport();
    test_canfd_transport();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Physical Layer Test Suite Finished");
    LOG_DOUBLE_SEPARATOR();

    return 0;
}