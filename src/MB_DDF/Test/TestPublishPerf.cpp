#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/DDS/Subscriber.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Timer/ChronoHelper.h"
#include "MB_DDF/Timer/SystemTimer.h"
#include "MB_DDF/PhysicalLayer/UdpLink.h"
#include "MB_DDF/PhysicalLayer/BasicTypes.h"

#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <pthread.h>

int main() {
    // 日志配置
    LOG_SET_LEVEL_INFO();
    // LOG_SET_LEVEL_DEBUG();
    LOG_DISABLE_TIMESTAMP();
    LOG_DISABLE_FUNCTION_LINE();

    // 1) 初始化DDS
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);

    const std::string topic_name = "local://perf_test";

    uint64_t start_time;
    auto publisher = dds.create_publisher(topic_name, false);
    auto subscriber = dds.create_subscriber(topic_name, false
        , [&start_time](const void* data, size_t size, uint64_t timestamp) {
            // 计算延迟
            uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

            // 打印数据
            int seq = *(const char*)data;
            LOG_INFO << "Received: " << seq;
            LOG_INFO << "delay: " << (current_time - start_time) / 1000.0 << " us";
        });

    // 绑定订阅者工作线程到4核（例如0,1,2,3）
    // 绑定核心提高实时性
    if (subscriber->get_thread()) {
        MB_DDF::Timer::SystemTimer::configureThread(
            subscriber->get_thread()->native_handle(), SCHED_FIFO, 99, 4);
    }

    // 等待工作线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2) 新建UDP物理端口，自发自收
    MB_DDF::PhysicalLayer::UdpLink sender;
    
    // 配置发送端
    MB_DDF::PhysicalLayer::LinkConfig sender_config;
    sender_config.local_addr = MB_DDF::PhysicalLayer::Address::createUDP("127.0.0.1", 9876); 
    sender_config.mtu = 65535;
    
    // 目标地址
    auto dest_addr = MB_DDF::PhysicalLayer::Address::createUDP("127.0.0.1", 9876);

    // UDP发送配置
    sender_config.remote_addr = dest_addr;

    // 初始化发送端    
    if (!sender.initialize(sender_config) || !sender.open()) {
        LOG_ERROR << "Failed to initialize UdpLink sender";
        return -1;
    }

    // 接收-发布测试
    const size_t payload_size = 65535;
    std::vector<uint8_t> payload(payload_size, 0xAB);
    std::vector<uint8_t> recv_buf(payload_size, 0);

    LOG_INFO << "Classic publish latency test";
    for (int i=0; i<10; i++) {
        payload[0] = i;
        sender.send(payload.data(), payload_size, dest_addr);
        sleep(1);
        start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        sender.receive(recv_buf.data(), payload_size, dest_addr);
        if (publisher) {
            publisher->publish(recv_buf.data(), payload.size());
        }
    }

    // 零拷贝publish测量（使用publish_fill）
    sleep(1);
    LOG_INFO << "Zero-copy publish latency test";  
    for (int i=0; i<10; i++) {
        payload[0] = i;
        sender.send(payload.data(), payload_size, dest_addr);
        sleep(1);
        start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();  
        if (publisher) {
            bool ok = publisher->publish_fill(payload.size(), [&](void* buf, size_t cap) -> size_t {
                size_t w = std::min(cap, payload.size());
                sender.receive((uint8_t*)buf, w, dest_addr);
                return w;
            });
            if (!ok) {
                LOG_ERROR << "Zero-copy publish_fill failed at iteration " << i;
            }
        }
    }

    // 保持一段时间以便查看日志输出
    sleep(1);
    return 0;
}