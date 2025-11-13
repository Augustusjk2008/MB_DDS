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
    cfg_tp.event_number = 1;
    cfg_tp.device_offset = 0x20000;
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

// 测试 CAN 设备
void test_can_transport() {   
    LOG_TITLE("CAN Transport Test");

    ControlPlane::XdmaTransport can;
    TransportConfig cfg_can;
    cfg_can.device_path  = "/dev/xdma0"; // 必须设置设备路径以映射寄存器与事件设备
    cfg_can.device_offset = 0x80000;      // CAN 核心在 user 空间的偏移
    cfg_can.event_number = 8;             // 事件设备编号（当前未使用中断，但保持一致）
    if (!can.open(cfg_can)) {
        LOG_ERROR << "XdmaTransport open failed for CAN (check device nodes).";
        return;
    }

    Device::CanDevice can_dev(can, 8);
    LinkConfig cfg_link; 
    cfg_link.mtu = 64;
    if (!can_dev.open(cfg_link)) {
        LOG_ERROR << "CanDevice open failed.";
        cleanup_device(can_dev, can);
        return;
    }

    #define CAN_REG_MODE 0
    // Simple CAN mode
    #if CAN_REG_MODE == 1
    
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
    can_dev.wr32(0x000, 0x1);
    usleep(100);  // AXI时钟100MHz时，1us > 16个周期（160ns），确保复位完成

    // 验证复位：读取SRR，确认bit31（SRST）已自动清0
    can_dev.rd32(0x000, reg_val);
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
    can_dev.wr32(0x000, 0x00000000);
    usleep(100);

    // 验证配置模式：读取Status Register (SR)，确认bit31（CONFIG）=1
    can_dev.rd32(0x018, reg_val);
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
    can_dev.wr32(0x004, 0x2);
    // can_dev.wr32(0x004, 0x1);
    usleep(100);

    // 验证回环模式配置：读取MSR，确认bit30=1、bit31=0
    can_dev.rd32(0x004, reg_val);
    LOG_INFO << "3. MSR回环模式配置: 0x" << std::hex << reg_val 
             << "（预期：0x00000002，LBACK=1、SLEEP=0）";
    if ((reg_val & 0x3) != 0x2) {  // 检查bit30-31=01
        LOG_WARN << "回环模式配置失败！LBACK/SLEEP位错误";
    }

    /****************************************************************************
    * 步骤4：配置1M波特率（CAN_CLK=24M，手册Table11-14）
    * 计算依据：
    * 1. (BRP+1)*(1+TS1+TS2) = 24M/1M =24 → 选BRP=1（BRP+1=2），1+TS1+TS2=12
    * 2. TS1=8tq（TSEG1=7）、TS2=3tq（TSEG2=2）、SJW=1tq（SJW=0）
    ***************************************************************************/
    // 4.1 配置BRPR（0x008）：BRP=1→写入0x01（大端序bit7-0=0x01）
    can_dev.wr32(0x008, 0x00000001);
    usleep(100);
    can_dev.rd32(0x008, reg_val);   
    LOG_INFO << "4.1 BRPR配置: 0x" << std::hex << reg_val << "（预期：0x00000001）";
    if (reg_val != 0x00000001) {
        LOG_ERROR << "BRPR配置失败！";
        return;
    }

    // 4.2 配置BTR（0x00C）：TS1=0x07、TS2=0x02、SJW=0x00→写入0x000001C7
    can_dev.wr32(0x00C, 0x000001C7);
    usleep(100);
    can_dev.rd32(0x00C, reg_val);   
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
    can_dev.wr32(0x060, 0x00000000);
    usleep(100);

    // 5.2 等待滤波器空闲：读取SR的bit20（手册编号）→实际bit11→掩码0x00000800
    do {
        can_dev.rd32(0x018, reg_val);
    } while ((reg_val & 0x00000800) != 0);  // 正确掩码：0x00000800
    LOG_INFO << "5.1 滤波器已空闲（ACFBSY=0）";

    // 5.3 配置滤波器1掩码（AFMR1=0x00000000，全0掩码→不限制ID）
    can_dev.wr32(0x064, 0x00000000);
    usleep(100);
    can_dev.rd32(0x064, reg_val);
    LOG_INFO << "5.2 AFMR1（掩码）: 0x" << std::hex << reg_val << "（预期：0x00000000）";

    // 5.4 配置滤波器1ID（AFIR1=0x00000000，掩码全0时ID无意义）
    can_dev.wr32(0x068, 0x00000000);
    usleep(100);
    can_dev.rd32(0x068, reg_val);
    LOG_INFO << "5.3 AFIR1（ID）: 0x" << std::hex << reg_val << "（预期：0x00000000）";

    // 5.5 启用滤波器1：写AFR的bit31（UAF1）=1
    can_dev.wr32(0x060, 0x1);
    usleep(100);
    can_dev.rd32(0x060, reg_val);
    LOG_INFO << "5.4 AFR（滤波器使能）: 0x" << std::hex << reg_val << "（预期：0x1）";
    if ((reg_val & 0x1) != 0x1) {
        LOG_WARN << "滤波器1启用失败！UAF1位未置1";
    }

    /****************************************************************************
     * 步骤6：启用核心，退出配置模式（手册Table8/20：SRR的CEN位+SR的模式位，地址0x000/0x018）
     * 要求：1. 写SRR的bit30（CEN）=1启用核心；2. 核心检测11个隐性位后进入回环模式；3. 验证模式位
     ***************************************************************************/
    // 写SRR=0x40000000（CEN=1，启用核心）
    can_dev.wr32(0x000, 0x2);
    usleep(10);  // 等待核心检测11个隐性位（回环模式下自动生成）

    // 验证模式：读取SR，确认CONFIG=0（bit31=0）、LBACK=1（bit30=1
    can_dev.rd32(0x018, reg_val);
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
        LOG_WARN << "核心未进入回环模式！状态位错误";
    }

    /****************************************************************************
     * 步骤7：填充TX FIFO（手册Table6：TX FIFO地址0x030~0x03C）
     * 要求：1. 按ID→DLC→DataWord1→DataWord2顺序写；2. 标准ID需配置IDE=0
     ***************************************************************************/
    // 7.1 配置TX ID（0x030）：标准ID=0x123（bit0-10）、IDE=0（bit12=0）、RTR=0（bit11=0）
    const uint32_t tx_id_reg = (TX_ID & 0x7FF) << 21;  // 11位标准ID占bit0-10，其他位=0
    can_dev.wr32(0x030, tx_id_reg);
    LOG_INFO << "7.1 TX FIFO ID: 0x" << std::hex << tx_id_reg << "（实际ID=0x" << TX_ID << "）";

    // 7.2 配置TX DLC（0x034）：DLC=4（bit0-3），其他保留位=0
    const uint32_t tx_dlc_reg = ((uint32_t)TX_DLC & 0x0F) << 28;
    can_dev.wr32(0x034, tx_dlc_reg);
    LOG_INFO << "7.2 TX FIFO DLC: 0x" << std::hex << tx_dlc_reg << "（实际DLC=" << (int)TX_DLC << "）";

    // 7.3 配置TX数据（0x038：DataWord1=DB0~DB3，0x03C：DataWord2=DB4~DB7，此处DLC=4故写0）
    can_dev.wr32(0x038, TX_DATA);
    can_dev.wr32(0x03C, 0x00000000);
    LOG_INFO << "7.3 TX FIFO DataWord1: 0x" << std::hex << TX_DATA;

    /****************************************************************************
     * 步骤8：等待发送&接收完成（手册Table21/22：ISR中断状态，地址0x01C）
     * 回环模式特征：1. 发送成功→ISR的bit30（TXOK）=1；2. 接收成功→ISR的bit27（RXOK）=1
     ***************************************************************************/
    uint32_t timeout = 1000;  // 超时1000ms
    bool tx_rx_done = false;
    while (timeout--) {
        can_dev.rd32(0x01C, reg_val);
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
    can_dev.rd32(0x050, reg_val);
    rx_id = (reg_val >> 21) & 0x7FF;  // 提取bit21-31为标准ID
    LOG_INFO << "9.1 RX FIFO ID: 0x" << std::hex << reg_val << "（提取ID=0x" << rx_id << "）";
    if (rx_id != TX_ID) {
        LOG_WARN << "ID不匹配！发送=0x" << std::hex << TX_ID << "，接收=0x" << rx_id;
    }

    // 9.2 读取RX DLC（0x054）：提取bit28-31为DLC
    can_dev.rd32(0x054, reg_val);
    rx_dlc = (reg_val >> 28) & 0x0F;
    LOG_INFO << "9.2 RX FIFO DLC: 0x" << std::hex << reg_val << "（提取DLC=" << (int)rx_dlc << "）";
    if (rx_dlc != TX_DLC) {
        LOG_WARN << "DLC不匹配！发送=" << (int)TX_DLC << "，接收=" << (int)rx_dlc;
    }

    // 9.3 读取RX数据（0x058：DataWord1）
    can_dev.rd32(0x058, rx_data);
    LOG_INFO << "9.3 RX FIFO DataWord1: 0x" << std::hex << rx_data;
    if (rx_data != TX_DATA) {
        LOG_WARN << "数据不匹配！发送=0x" << std::hex << TX_DATA << "，接收=0x" << rx_data;
    }

    /****************************************************************************
     * 步骤10：清除中断状态（手册Table25/26：ICR中断清除寄存器，地址0x024）
     * 要求：写1到对应位清除ISR状态（TXOK=bit30，RXOK=bit27）
     ***************************************************************************/
    can_dev.wr32(0x024, 0x00000012);
    usleep(100);
    can_dev.rd32(0x01C, reg_val);
    LOG_INFO << "10. 清除中断后ISR: 0x" << std::hex << reg_val << "（预期：TXOK=0、RXOK=0）";

    /****************************************************************************
     * 步骤11：循环监控状态（可选，验证回环模式稳定性）
     ***************************************************************************/
    LOG_INFO << "\n=== 1M波特率自发自收测试结束，进入循环模式===";// 循环监控：用大端序实际bit掩码
    while (1) {
        sleep(1);
        // 1. 监控核心模式（LBACK=0x2，NORMAL=0x8）
        can_dev.rd32(0x018, reg_val);
        LOG_INFO << "Loop: 模式状态（LBACK=" << ((reg_val & 0x2) ? 1 : 0) 
                << "，NORMAL=" << ((reg_val & 0x8) ? 1 : 0) << "）";
        // 2. 监控错误计数器（TEC=手册bit24-31→实际bit7-0；REC=手册bit16-23→实际bit15-8）
        can_dev.rd32(0x010, ecr_val);
        const uint8_t tec = ecr_val & 0xFF;  // 实际bit7-0→手册bit24-31（TEC）
        const uint8_t rec = (ecr_val >> 8) & 0xFF;  // 实际bit15-8→手册bit16-23（REC）
        LOG_INFO << "Loop: 错误计数器（TEC=" << (int)tec << "，REC=" << (int)rec << "）";
    }
    return;
    #endif

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

    const int rounds = 100; // 测试若干次
    int pass = 0, fail = 0;
    for (int i = 0; i < rounds; ++i) {
        Device::CanFrame tx;
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

        // 写入前清 ISR 以消除残留状态
        can_dev.wr32(0x024, 0x12);
        bool sent = can_dev.send(tx);
        if (!sent) {
            LOG_ERROR << "send() failed at round " << i;
            ++fail;
            continue;
        }

        // 轮询接收：无 event 中断，轮询 receive(CanFrame&)
        Device::CanFrame rx;
        int32_t got = -1;
        int tries = 0;
        for (; tries < 100; ++tries) { // 最长约100ms
            got = can_dev.receive(rx);
            if (got > 0) break;
            // 也轮询 ISR 位来辅助判断是否该继续等
            uint32_t isr = 0;
            can_dev.rd32(0x01C, isr);
            if ((isr & 0x10) != 0) {
                // 有 RXOK 但读取失败，稍作等待再读
                usleep(500);
            }
            usleep(1000); // 1ms 间隔
        }
        if (got <= 0) {
            LOG_ERROR << "receive() timeout/failed at round " << i << " ret=" << got;
            ++fail;
            continue;
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
    }
    LOG_INFO << "CAN loopback self-test done. pass=" << pass << " fail=" << fail;

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
    test_rs422_device();
    // test_helm_transport();
    // test_ddr_transport();
    // test_can_transport();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Physical Layer Test Suite Finished");
    LOG_DOUBLE_SEPARATOR();

    return 0;
}