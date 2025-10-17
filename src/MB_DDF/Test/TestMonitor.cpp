#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Monitor/DDSMonitor.h"

// int main(int argc, char* argv[]) {
// 无参数 main
int main() {
    // 初始化DDS核心，分配共享内存
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);
        
    // 创建监控器实例，1秒扫描间隔，3秒活跃超时
    MB_DDF::Monitor::DDSMonitor monitor(1000, 3000);  
          
    // 初始化监控器
    if (!monitor.initialize(dds)) {
        LOG_ERROR << "Failed to initialize DDS monitor";
        return -1;
    }
    LOG_INFO << "DDS monitor initialized successfully";

    // 设置监控回调
    monitor.set_monitor_callback([](const MB_DDF::Monitor::DDSSystemSnapshot& snapshot) {
        std::cout << "\n=== 监控快照 (时间戳: " << snapshot.timestamp << ") ===" << std::endl;
        std::cout << "DDS版本号: " << MB_DDF::Monitor::DDSMonitor::version_to_string(snapshot.dds_version) 
                  << " (0x" << std::hex << snapshot.dds_version << std::dec << ")" << std::endl;
        std::cout << "Topics数量: " << snapshot.topics.size() << std::endl;
        std::cout << "发布者数量: " << snapshot.publishers.size() << std::endl;
        std::cout << "订阅者数量: " << snapshot.subscribers.size() << std::endl;
        std::cout << "共享内存总大小: " << snapshot.total_shared_memory_size << " bytes" << std::endl;
        std::cout << "已使用内存: " << snapshot.used_shared_memory_size << " bytes" << std::endl;
            
        // 显示Topic信息
        for (const auto& topic : snapshot.topics) {
                std::cout << "  Topic[" << topic.topic_id << "]: " << topic.topic_name 
                         << " (订阅者: " << topic.subscriber_count 
                         << ", 有发布者: " << (topic.has_publisher ? "是" : "否")
                         << ", 总消息: " << topic.total_messages << ")" << std::endl;
        }
            
        // 显示发布者信息
        for (const auto& pub : snapshot.publishers) {
                std::cout << "  发布者[" << pub.publisher_id << "]: " << pub.publisher_name
                         << " -> " << pub.topic_name 
                         << " (序列号: " << pub.last_sequence
                         << ", 活跃: " << (pub.is_active ? "是" : "否") << ")" << std::endl;
        }
            
            // 显示订阅者信息
        for (const auto& sub : snapshot.subscribers) {
                std::cout << "  订阅者[" << sub.subscriber_id << "]: " << sub.subscriber_name
                         << " <- " << sub.topic_name
                         << " (读取位置: " << sub.read_pos
                         << ", 活跃: " << (sub.is_active ? "是" : "否") << ")" << std::endl;
        }
    });    
        
    // 启动监控
    if (!monitor.start_monitoring()) {
        std::cerr << "Failed to start monitoring" << std::endl;
        return -1;
    }
    
    // 等待用户输入
    std::cout << "监控已启动，按任意键停止..." << std::endl;
    std::cin.get();

    return 0;
}