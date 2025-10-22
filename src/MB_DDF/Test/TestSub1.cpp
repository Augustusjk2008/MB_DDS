#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"

// int main(int argc, char* argv[]) {
// 无参数 main
int main() {
    // 设置日志输出级别
    LOG_SET_LEVEL_INFO();
    // 禁用时间戳输出
    LOG_DISABLE_TIMESTAMP();
    // 禁用函数名和行号显示
    LOG_DISABLE_FUNCTION_LINE();

    // 初始化DDS核心，分配共享内存
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);

    // 创建发布者和订阅者
    auto subscriber_a = dds.create_subscriber("local://test_topic_a"
        , [](const void* data, size_t size, uint64_t timestamp) {
            // 计算延迟
            uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            uint64_t delay = current_time - timestamp;

            // 打印数据
            const char* str = static_cast<const char*>(data);
            LOG_DEBUG << "Received " << size << " bytes of data: " << str;
            LOG_DEBUG << "Delay: " << delay / 1000.0f << " us";
        });
    
    auto subscriber_b = dds.create_subscriber("local://test_topic_b"
        , [](const void* data, size_t size, uint64_t timestamp) {
            // 计算延迟
            uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            uint64_t delay = current_time - timestamp;

            // 打印数据
            const char* str = static_cast<const char*>(data);
            LOG_DEBUG << "Received " << size << " bytes of data: " << str;
            LOG_DEBUG << "Delay: " << delay / 1000.0f << " us";
        });

    // 绑定订阅者到CPU核心
    subscriber_a->bind_to_cpu(0);
    subscriber_b->bind_to_cpu(1);

    // 永久等待，保持程序运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}