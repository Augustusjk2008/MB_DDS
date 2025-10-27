#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/DDS/Subscriber.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Timer/SystemTimer.h"
#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
// removed old BasicTypes include; UdpLink now uses LinkConfig.name

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

    // 初始化DDS
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);

    const std::string topic_name = "local://perf_test";

    uint64_t start_time;
    auto publisher = dds.create_publisher(topic_name, false);
    auto subscriber = dds.create_subscriber(topic_name, false
        , [&start_time](const void* data, size_t size, uint64_t timestamp) {
            (void)timestamp;
            (void)size;
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

    // 新建UDP物理端口，自发自收（同端口回环）
    MB_DDF::PhysicalLayer::DataPlane::UdpLink sender;
    MB_DDF::PhysicalLayer::LinkConfig sender_config;
    sender_config.mtu = 60000;
    sender_config.name = "127.0.0.1:9876|127.0.0.1:9876"; // 绑定并连接到自身端口

    if (!sender.open(sender_config)) {
        LOG_ERROR << "Failed to open UdpLink sender";
        return -1;
    }

    // 接收-发布测试
    const size_t payload_size = 60000;
    std::vector<uint8_t> payload(payload_size, 0xAB);
    std::vector<uint8_t> recv_buf(payload_size, 0);

    LOG_INFO << "Classic publish latency test";
    for (int i=0; i<10; i++) {
        payload[0] = i;
        bool sent = sender.send(payload.data(), payload_size);
        if (!sent) {
            LOG_ERROR << "send() failed at iteration " << i;
            continue;
        }
        sleep(1);
        start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        MB_DDF::PhysicalLayer::DataPlane::Endpoint src_ep;
        int32_t recv_len = sender.receive(recv_buf.data(), payload_size, src_ep, 1000 * 1000);
        if (recv_len > 0) {
            if (publisher) {
                publisher->publish(recv_buf.data(), static_cast<size_t>(recv_len));
            }
        } else {
            LOG_ERROR << "receive() failed or timed out at iteration " << i << ", ret=" << recv_len;
        }
    }

    // 零拷贝publish测量（使用publish_fill）
    sleep(1);
    LOG_INFO << "Zero-copy publish latency test";  
    for (int i=0; i<10; i++) {
        payload[0] = i;
        bool sent = sender.send(payload.data(), payload_size);
        if (!sent) {
            LOG_ERROR << "send() failed at iteration " << i;
            continue;
        }
        sleep(1);
        start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();  
        if (publisher) {
            bool ok = publisher->publish_fill(payload.size(), [&](void* buf, size_t cap) -> size_t {
                MB_DDF::PhysicalLayer::DataPlane::Endpoint src_ep;
                size_t w = std::min(cap, payload.size());
                int32_t r = sender.receive(reinterpret_cast<uint8_t*>(buf), static_cast<uint32_t>(w), src_ep, 1000 * 1000);
                return r > 0 ? static_cast<size_t>(r) : 0;
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