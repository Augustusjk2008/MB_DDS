#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"
#include "MB_DDF/Monitor/DDSMonitor.h"
#include "MB_DDF/PhysicalLayer/UdpLink.h"

// 测试监控器主程序
int main(int argc, char* argv[]) {
    // 默认参数值
    bool print_info = false;
    bool send_snapshot = true;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--print-info" || arg == "-p") {
            print_info = true;
        } else if (arg == "--no-print-info") {
            print_info = false;
        } else if (arg == "--send-snapshot" || arg == "-s") {
            send_snapshot = true;
        } else if (arg == "--no-send-snapshot") {
            send_snapshot = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  -p, --print-info      启用监控信息打印 (默认: 关闭)" << std::endl;
            std::cout << "  --no-print-info       禁用监控信息打印" << std::endl;
            std::cout << "  -s, --send-snapshot   启用快照数据发送 (默认: 开启)" << std::endl;
            std::cout << "  --no-send-snapshot    禁用快照数据发送" << std::endl;
            std::cout << "  -h, --help            显示此帮助信息" << std::endl;
            return 0;
        } else {
            std::cout << "未知参数: " << arg << std::endl;
            std::cout << "使用 --help 查看可用选项" << std::endl;
            return -1;
        }
    }
    
    // 设置日志输出级别
    LOG_SET_LEVEL_INFO();
    // 禁用时间戳输出
    LOG_DISABLE_TIMESTAMP();
    // 禁用函数名和行号显示
    LOG_DISABLE_FUNCTION_LINE();

    // 初始化DDS核心，分配共享内存
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024);
        
    // 创建监控器实例，0.4秒扫描间隔，3秒活跃超时
    MB_DDF::Monitor::DDSMonitor monitor(400, 3000);  
          
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
    
    // 目标地址
    auto dest_addr = MB_DDF::PhysicalLayer::Address::createUDP("192.168.56.1", 9002);
    
    
    if (!sender.initialize(sender_config) || !sender.open()) {
        LOG_ERROR << "Failed to initialize UdpLink sender";
        return -1;
    }

    // 设置监控回调
    monitor.set_monitor_callback([&monitor, &sender, &dest_addr, print_info, send_snapshot](const MB_DDF::Monitor::DDSSystemSnapshot& snapshot) {

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
            // char data_buffer[5000];
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

    // 永久等待，保持程序运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}