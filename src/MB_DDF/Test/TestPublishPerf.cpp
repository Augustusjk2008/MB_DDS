#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/DDS/Subscriber.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Debug/LoggerExtensions.h"
#include "MB_DDF/Timer/SystemTimer.h"
#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
// removed old BasicTypes include; UdpLink now uses LinkConfig.name

#include <chrono>
#include <cstdint>
#include <thread>
#include <unistd.h>
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

    // 测试参数和数据
    uint64_t start_time = 0;
    uint64_t tr_delay = 0, total_delay = 0;
    const size_t num_messages = 300000;

    // 测试控制信号量
    sem_t tr_sem;
    sem_init(&tr_sem, 0, 0);
    auto publisher = dds.create_publisher(topic_name, false);
    auto subscriber = dds.create_subscriber(topic_name, false
        , [&start_time, &tr_delay, &total_delay, &tr_sem](const void* data, size_t size, uint64_t timestamp) {
            // (void)timestamp;
            (void)size;
            (void)data;
            // 计算延迟
            uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            tr_delay += (current_time - timestamp);
            total_delay += (current_time - start_time);
            sem_post(&tr_sem);
        });

    // 绑定订阅者工作线程到4核（例如0,1,2,3）
    // 绑定核心提高实时性
    if (subscriber->get_thread()) {
        MB_DDF::Timer::SystemTimer::configureThread(
            subscriber->get_thread()->native_handle(), SCHED_FIFO, 99, 4);
    }
    MB_DDF::Timer::SystemTimer::configureThread(
        pthread_self(), SCHED_FIFO, 99, 5);

    // 等待工作线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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
    const size_t payload_size = 61357;
    uint32_t wait_usec = 10;
    std::vector<uint8_t> payload(payload_size, 0xAB);
    std::vector<uint8_t> recv_buf(payload_size, 0);

    sleep(1);
    std::string test1 = "Classic publish latency test";
    tr_delay = total_delay = 0;
    LOG_TITLE(test1);
    for (int i=0; i<num_messages; i++) {
        if ((i+1) % (num_messages / 50) == 0) {
            LOG_PROGRESS(test1, (i+1) * 100 / num_messages);
        }
        bool sent = sender.send(payload.data(), payload_size);
        if (!sent) {
            LOG_ERROR << "send() failed at iteration " << i;
            break;
        }
        start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        int32_t recv_len = sender.receive(recv_buf.data(), payload_size, 1000 * 1000);
        if (recv_len > 0) {
            if (publisher) {
                publisher->publish(recv_buf.data(), static_cast<size_t>(recv_len));
            }
        } else {
            LOG_ERROR << "receive() failed or timed out at iteration " << i << ", ret=" << recv_len;
            break;
        }
        sem_wait(&tr_sem);
        usleep(wait_usec);
    }
    LOG_INFO << test1 << " completed";
    LOG_INFO << "average t & r delay: " << tr_delay / 1000.0 / num_messages << " us";
    LOG_INFO << "average total delay: " << total_delay / 1000.0 / num_messages << " us";

    // 零拷贝publish测量（使用publish_fill）
    sleep(1);
    std::string test2 = "Zero-copy publish latency test";
    tr_delay = total_delay = 0;
    LOG_TITLE(test2);
    for (int i=0; i<num_messages; i++) {
        if ((i+1) % (num_messages / 50) == 0) {
            LOG_PROGRESS(test2, (i+1) * 100 / num_messages);
        }
        bool sent = sender.send(payload.data(), payload_size);
        if (!sent) {
            LOG_ERROR << "send() failed at iteration " << i;
            break;
        }
        start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();  
        if (publisher) {
            bool ok = publisher->publish_fill(payload.size(), [&](void* buf, size_t cap) -> size_t {
                size_t w = std::min(cap, payload.size());
                int32_t r = sender.receive(reinterpret_cast<uint8_t*>(buf), static_cast<uint32_t>(w), 1000 * 1000);
                return r > 0 ? static_cast<size_t>(r) : 0;
            });
            if (!ok) {
                LOG_ERROR << "Zero-copy publish_fill failed at iteration " << i;
                break;
            }
        }
        sem_wait(&tr_sem);
        usleep(wait_usec);
    }
    LOG_INFO << test2 << " completed";
    LOG_INFO << "average t & r delay: " << tr_delay / 1000.0 / num_messages << " us";
    LOG_INFO << "average total delay: " << total_delay / 1000.0 / num_messages << " us";

    // 保持一段时间以便查看日志输出
    sleep(1);
    return 0;
}