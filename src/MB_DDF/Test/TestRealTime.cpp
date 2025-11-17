#include "MB_DDF/Timer/SystemTimer.h"
#include "MB_DDF/Timer/ChronoHelper.h"
#include "MB_DDF/PhysicalLayer/Device/HelmDevice.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
#include <cstdint>

// 测试实时定时器
using namespace MB_DDF::Timer;

// 测试回调函数，记录当前时间
void helm_callback(void* para) {
    // 舵机 Ad 读取、PWM duty 设置
    auto helm = (MB_DDF::PhysicalLayer::Device::HelmDevice*)para;
    static uint16_t fdb[4];
    static uint32_t v_input[4] = {0x11111234, 0x55555678, 0x99999ABC, 0xEEEEEDF0};
    static int32_t hardware_enabled = 1;
    if (hardware_enabled <= 0) {
        hardware_enabled = helm->receive((uint8_t*)fdb, sizeof(fdb));
        helm->send((uint8_t*)v_input, sizeof(v_input));
    }

    ChronoHelper::record(0);
}

void cml_callback(void* para) {
    static const size_t DATA_SIZE = 64 * 1024;
    static std::vector<uint8_t> data(DATA_SIZE);

    static bool hardware_enabled = true;
    if (hardware_enabled) {
        auto cml = (MB_DDF::PhysicalLayer::ControlPlane::XdmaTransport*)para;
        hardware_enabled = cml->continuousReadAsync(0, data.data(), DATA_SIZE, 0);
    }

    // ChronoHelper::record(1);
}

// 测试主函数，设置实时定时器
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // 舵机接口
    MB_DDF::PhysicalLayer::ControlPlane::XdmaTransport tp_helm;
    MB_DDF::PhysicalLayer::TransportConfig cfg_helm;
    cfg_helm.device_offset = 0x60000;

    tp_helm.open(cfg_helm);
    MB_DDF::PhysicalLayer::Device::HelmDevice helm(tp_helm, 0);
    MB_DDF::PhysicalLayer::LinkConfig cfg_link; 
    helm.open(cfg_link);
    void* helm_para = (void*)&helm;

    // 图像接口
    MB_DDF::PhysicalLayer::ControlPlane::XdmaTransport ddr;
    MB_DDF::PhysicalLayer::TransportConfig cfg_ddr;
    cfg_ddr.dma_h2c_channel = 0;
    cfg_ddr.dma_c2h_channel = 0;
    cfg_ddr.device_offset = 0x80000000;
    ddr.open(cfg_ddr);
    void* cml_para = (void*)&ddr;

    // 配置舵机定时器选项
    SystemTimerOptions opt_helm;
    opt_helm.sched_policy = SCHED_FIFO;             // 新线程调度策略：FIFO
    opt_helm.priority = sched_get_priority_max(SCHED_FIFO); // 优先级设为最高
    opt_helm.cpu = 6;                               // 绑核
    opt_helm.signal_no = SIGRTMIN;                  // 使用实时信号
    opt_helm.user_data = helm_para;                  // 回调函数用户数据指针

    auto helm_timer 
    = SystemTimer::start("250us", helm_callback, opt_helm);

    // 配置图像定时器选项
    SystemTimerOptions opt_cml;
    opt_cml.sched_policy = SCHED_FIFO;             // 新线程调度策略：FIFO
    opt_cml.priority = sched_get_priority_max(SCHED_FIFO); // 优先级设为最高
    opt_cml.cpu = 7;                               // 绑核
    opt_cml.signal_no = SIGRTMIN;                  // 使用实时信号
    opt_cml.user_data = cml_para;                  // 回调函数用户数据指针

    // 根据程序传入参数决定是否开启 cml_timer
    std::shared_ptr<SystemTimer> cml_timer;
    if (argc > 1) {
        cml_timer = SystemTimer::start("12ms", cml_callback, opt_cml);
    } else {
        std::cout << "cml_timer is not started." << std::endl;
    }

    std::cout << "press enter to stop" << std::endl;
    std::cin.get();
    helm_timer->stop();
    if (cml_timer) {
        cml_timer->stop();
    }

    return 0;
}