#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Monitor/DDSMonitor.h"
#include "MB_DDF/PhysicalLayer/UdpLink.h"

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
        
    // 创建监控器实例，1秒扫描间隔，3秒活跃超时
    MB_DDF::Monitor::DDSMonitor monitor(200, 3000);  
          
    // 初始化监控器
    if (!monitor.initialize(dds)) {
        LOG_ERROR << "Failed to initialize DDS monitor";
        return -1;
    }
    LOG_INFO << "DDS monitor initialized successfully";

    // 创建UDP发送端
    MB_DDF::PhysicalLayer::UdpLink sender;
    
    // 配置发送端
    MB_DDF::PhysicalLayer::LinkConfig sender_config;
    sender_config.local_addr = MB_DDF::PhysicalLayer::Address::createUDP("192.168.56.132", 9001); 
    sender_config.mtu = 32768;
    // sender_config.remote_addr = dest_addr;
    
    // 目标地址
    auto dest_addr = MB_DDF::PhysicalLayer::Address::createUDP("192.168.56.1", 9002);
    // auto dest_addr = MB_DDF::PhysicalLayer::Address::createUDP("192.168.56.132", 9001);
    
    
    if (!sender.initialize(sender_config) || !sender.open()) {
        LOG_ERROR << "Failed to initialize UdpLink sender";
        return -1;
    }

    // 设置监控回调
    monitor.set_monitor_callback([&monitor, &sender, &dest_addr](const MB_DDF::Monitor::DDSSystemSnapshot& snapshot) {
        static const bool print_info = false;
        static const bool send_snapshot = true;

        if (print_info) {
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
                    << ", 最后读取序列号: " << sub.last_read_sequence
                    << ", 活跃: " << (sub.is_active ? "是" : "否") << ")" << std::endl;
            }   
        }
        
        if (send_snapshot) {
            // 准备数据
            char data_buffer[5000];
            // uint32_t message_len = monitor.serialize_to_binary(snapshot, data_buffer, 5000);        
            auto json_str = monitor.serialize_to_json(snapshot);
            
            // 发送数据
            if (!json_str.empty()) {
                bool send_result = sender.send(reinterpret_cast<const uint8_t*>(json_str.c_str()), 
                                    json_str.size(), dest_addr);
                if (!send_result) {
                    LOG_ERROR << "Failed to send snapshot data";
                }
            }
        }
    });    
        
    // 启动监控
    if (!monitor.start_monitoring()) {
        LOG_ERROR << "Failed to start monitoring";
        return -1;
    }
    
    // 等待用户输入
    LOG_INFO << "Monitoring started, press any key to stop...";
    std::cin.get();
    
    return 0;
}