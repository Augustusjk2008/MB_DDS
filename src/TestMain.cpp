#include "MB_DDF/DDS/DDSCore.h"

#include <iostream>

// int main(int argc, char* argv[]) {
// 无参数 main
int main() {
    MB_DDF::DDS::DDSCore& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);

    auto publisher = dds.create_publisher("local://test_topic_a");
    auto subscriber = dds.create_reader("local://test_topic_a"
        , [](const void* data, size_t size, uint64_t timestamp) {
            // 计算延迟
            uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            uint64_t delay = current_time - timestamp;

            // 打印数据
            const char* str = static_cast<const char*>(data);
            std::cout << "Received " << size << " bytes of data: " << str << std::endl;
            std::cout << "Delay: " << delay / 1000.0f << " us" << std::endl;
        });

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (publisher) {
            // 发布的数据带一个递增的数，方便查看
            static int counter = 0;
            std::string msg = "Hello, World! " + std::to_string(counter++);
            publisher->write(msg.c_str(), msg.size());
        }
    }

    return 0;
}