/**
 * @file TestPhysicalLayer.cpp
 * @brief 物理层 UDP 与 RS422-XDMA 适配器端到端测试
 */
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <unistd.h>
#include <string>
#include <math.h>
#include <cstring>

#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Debug/LoggerExtensions.h"
#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/PhysicalLayer/Factory/HardwareFactory.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
#include "MB_DDF/Timer/SystemTimer.h"

// 测试实时定时器
using namespace MB_DDF::Timer;

// 测试回调函数，记录当前时间   
const float K_IN_OUT = 544.6516;
const float K_IN_OUT_1 = 1 / K_IN_OUT;
float ins = 0;
uint16_t fdb[4];
int v_input[4] = {0, 0, 0, 0};
float freq = 0.3f;
float phase = 0.0f;

void helm_callback(void* para) {
    // 舵机 Ad 读取、PWM duty 设置
    auto helm = (MB_DDF::DDS::PubAndSub*)para;
    helm->read((uint8_t*)fdb, sizeof(fdb));
    // kp = 8.0
    phase += freq * 250 / 1e6;
    if (phase >= 1) {
        phase -= 1;
    }
    // ins = 22.0f * sin(2 * std::numbers::pi * phase);
    ins = 22.0f * ((phase > 0.5)? 1:-1);
    // ins = -0.0f;
    v_input[0] = 1e8 / 544.6516 * 8.0 * (ins + K_IN_OUT_1 * static_cast<short>(fdb[2]));
    v_input[1] = 1e8 / 544.6516 * 8.0 * (ins + K_IN_OUT_1 * static_cast<short>(fdb[3]));
    v_input[2] = 1e8 / 544.6516 * 8.0 * (ins + K_IN_OUT_1 * static_cast<short>(fdb[0]));
    v_input[3] = 1e8 / 544.6516 * 8.0 * (ins + K_IN_OUT_1 * static_cast<short>(fdb[1]));
    helm->write((uint8_t*)v_input, sizeof(v_input));
}

// --- 主函数 ---
int main() {
    LOG_SET_LEVEL_INFO();
    LOG_DISABLE_TIMESTAMP();
    LOG_DISABLE_FUNCTION_LINE();

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Starting Helm Test");
    LOG_DOUBLE_SEPARATOR();
    LOG_BLANK_LINE();    

    MB_DDF::PhysicalLayer::ControlPlane::XdmaTransport io;
    MB_DDF::PhysicalLayer::TransportConfig cfg_io;
    cfg_io.device_offset = 0x90000;
    io.open(cfg_io);
    io.writeReg32(0x10 * 4, 0);

    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);
    auto helm = MB_DDF::PhysicalLayer::Factory::HardwareFactory::create("helm");
    auto pwm_writer = dds.create_writer("hardware://pwm_write", helm);
    auto ad_reader = dds.create_reader("hardware://ad_read", helm);
    MB_DDF::DDS::PubAndSub helm_operator = {pwm_writer, ad_reader};

    // 配置舵机定时器选项
    SystemTimerOptions opt_helm;
    opt_helm.sched_policy = SCHED_FIFO;             // 新线程调度策略：FIFO
    opt_helm.priority = sched_get_priority_max(SCHED_FIFO); // 优先级设为最高
    opt_helm.cpu = 6;                               // 绑核
    opt_helm.signal_no = SIGRTMIN;                  // 使用实时信号
    opt_helm.user_data = &helm_operator;            // 回调函数用户数据指针

    auto helm_timer 
    = SystemTimer::start("250us", helm_callback, opt_helm);

    const uint32_t sleep_us = 500000;
    while(1) {     
        LOG_INFO << "Helm ins is: " << ins;
        LOG_INFO << "Helm degree is: " << K_IN_OUT_1 * static_cast<short>(fdb[0]) 
        << " " << K_IN_OUT_1 * static_cast<short>(fdb[1]) 
        << " " << K_IN_OUT_1 * static_cast<short>(fdb[2]) 
        << " " << K_IN_OUT_1 * static_cast<short>(fdb[3]);
        LOG_INFO << "Helm pwm set : " << static_cast<int>(v_input[0]) << " " 
        << static_cast<int>(v_input[1]) << " " 
        << static_cast<int>(v_input[2]) << " " 
        << static_cast<int>(v_input[3]);
        usleep(sleep_us);
    }

    LOG_DOUBLE_SEPARATOR();
    LOG_TITLE("Helm Test Finished");
    LOG_DOUBLE_SEPARATOR();

    return 0;
}