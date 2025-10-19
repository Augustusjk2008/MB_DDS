#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"

// int main(int argc, char* argv[]) {
// 无参数 main
int main() {
    // 设置日志输出级别
    // LOG_SET_LEVEL_TRACE();
    LOG_SET_LEVEL_INFO();
    // 禁用时间戳输出
    LOG_DISABLE_TIMESTAMP();
    // 禁用函数名和行号显示
    LOG_DISABLE_FUNCTION_LINE();

    // 初始化DDS核心，分配共享内存
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);

    // 创建发布者和订阅者
    auto publisher = dds.create_publisher("local://test_topic_c");
    auto subscriber = dds.create_subscriber("local://test_topic_a"
        , [](const void* data, size_t size, uint64_t timestamp) {
            // 计算延迟
            uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            uint64_t delay = current_time - timestamp;

            // 打印数据
            const char* str = static_cast<const char*>(data);
            LOG_DEBUG << "Received " << size << " bytes of data: " << str;
            LOG_DEBUG << "Delay: " << delay / 1000.0f << " us";
        });

    // 主循环，持续发布数据
    while (true) {
        // 间隔 0.5 秒
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        if (publisher) {
            // 发布的数据带一个递增的数，方便查看
            static int counter = 0;
            std::string msg = "Hello, World! " + std::to_string(counter++);
            publisher->write(msg.c_str(), msg.size());
        }
        // 读取订阅者数据，此段不能和订阅者回调函数共存，否则会导致数据丢失
        // if (subscriber) {
        //     char data[1024];
        //     size_t size = sizeof(data);
        //     subscriber->read_latest(data, size);
        // }
    }

    return 0;
}