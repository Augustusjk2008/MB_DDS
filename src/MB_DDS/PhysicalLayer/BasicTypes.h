/**
 * @file BasicTypes.h
 * @brief 物理层基础类型定义
 * @date 2025-10-10
 * @author Jiangkai
 * 
 * 定义物理层通信所需的基础数据类型和结构体。
 * 包括通用地址类型、链路配置参数、状态枚举和回调函数类型。
 */

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace MB_DDS {
namespace PhysicalLayer {

/**
 * @struct Address
 * @brief 通用地址类型结构体
 * 
 * 封装不同物理链路的地址信息，支持CAN、UDP和串口三种地址类型。
 * 使用联合体(union)节省内存空间，同时提供类型安全的访问方式。
 */
struct Address {
    /**
     * @enum Type
     * @brief 地址类型枚举
     * 
     * 定义支持的物理链路地址类型。
     */
    enum class Type {
        CAN,        ///< CAN/CANFD地址类型，使用CAN ID标识
        UDP,        ///< UDP网络地址类型，使用IP地址和端口号标识
        SERIAL      ///< 串口地址类型，使用COM端口号标识
    };
    
    Type type;      ///< 地址类型标识符
    
    /**
     * @union 地址数据联合体
     * @brief 存储不同类型地址的具体数据
     * 
     * 根据type字段的值，使用对应的联合体成员访问地址数据。
     */
    union {
        uint32_t can_id;        ///< CAN标识符（当type为CAN时使用）
        struct {
            uint32_t ip;        ///< IPv4地址（网络字节序）
            uint16_t port;      ///< 端口号（主机字节序）
        } udp_addr;             ///< UDP地址结构（当type为UDP时使用）
        uint32_t com_port;      ///< 串口编号（当type为SERIAL时使用）
    };
    
    /**
     * @brief 默认构造函数
     * 
     * 初始化为CAN类型，ID为0的地址。
     */
    Address() : type(Type::CAN), can_id(0) {}
    
    /**
     * @brief 通用构造函数
     * @param addr_type 地址类型
     * @param id 地址标识符（CAN ID或串口号）
     * 
     * 用于构造CAN地址或串口地址。
     */
    Address(Type addr_type, uint32_t id) : type(addr_type) {
        switch (addr_type) {
        case Type::CAN:
            can_id = id;
            break;
        case Type::SERIAL:
            com_port = id;
            break;
        default:
            can_id = 0;  ///< 默认值
            break;
        }
    }
    
    /**
     * @brief UDP地址专用构造函数
     * @param addr_type 地址类型（必须为UDP）
     * @param ip IPv4地址
     * @param port 端口号
     * 
     * 专门用于构造UDP网络地址。
     */
    Address(Type addr_type, uint32_t ip, uint16_t port) : type(addr_type) {
        if (addr_type == Type::UDP) {
            udp_addr.ip = ip;
            udp_addr.port = port;
        } else {
            can_id = 0;  ///< 默认值
        }
    }
    
    /**
     * @brief 创建CAN地址的静态工厂方法
     * @param can_id CAN标识符
     * @return CAN类型的Address对象
     */
    static Address createCAN(uint32_t can_id) {
        return Address(Type::CAN, can_id);
    }
    
    /**
     * @brief 创建UDP地址的静态工厂方法
     * @param ip IPv4地址
     * @param port 端口号
     * @return UDP类型的Address对象
     */
    static Address createUDP(uint32_t ip, uint16_t port) {
        return Address(Type::UDP, ip, port);
    }
    
    /**
     * @brief 创建串口地址的静态工厂方法
     * @param com_port 串口号
     * @return SERIAL类型的Address对象
     */
    static Address createSerial(uint32_t com_port) {
        return Address(Type::SERIAL, com_port);
    }
    
    /**
     * @brief 地址相等比较运算符
     * @param other 要比较的另一个地址对象
     * @return 如果两个地址相等返回true，否则返回false
     * 
     * 首先比较地址类型，然后根据类型比较具体的地址数据。
     */
    bool operator==(const Address& other) const {
        if (type != other.type) return false;
        switch (type) {
        case Type::CAN:
            return can_id == other.can_id;
        case Type::UDP:
            return udp_addr.ip == other.udp_addr.ip && udp_addr.port == other.udp_addr.port;
        case Type::SERIAL:
            return com_port == other.com_port;
        default:
            return false;
        }
    }
    
    /**
     * @brief 转换为字符串表示
     * @return 地址的字符串表示形式
     * 
     * 根据地址类型生成可读的字符串格式：
     * - CAN: "CAN:0x[ID]"
     * - UDP: "UDP:[IP]:[Port]"
     * - SERIAL: "SERIAL:COM[Port]"
     */
    std::string toString() const {
        switch (type) {
        case Type::CAN:
            return "CAN:0x" + std::to_string(can_id);
        case Type::UDP: {
            uint32_t ip = udp_addr.ip;
            return "UDP:" + std::to_string((ip >> 24) & 0xFF) + "." +
                           std::to_string((ip >> 16) & 0xFF) + "." +
                           std::to_string((ip >> 8) & 0xFF) + "." +
                           std::to_string(ip & 0xFF) + ":" +
                           std::to_string(udp_addr.port);
        }
        case Type::SERIAL:
            return "SERIAL:COM" + std::to_string(com_port);
        default:
            return "UNKNOWN";
        }
    }
};

/**
 * @struct LinkConfig
 * @brief 链路配置参数结构体
 * 
 * 包含物理链路初始化和运行所需的所有配置参数。
 * 不同类型的物理链路可能只使用其中的部分参数。
 */
struct LinkConfig {
    Address local_addr;          ///< 本地地址（必需）
    Address remote_addr;         ///< 远程地址（可选，用于点对点连接）
    uint32_t baudrate;           ///< 波特率（串口/CAN专用，单位：bps）
    uint16_t mtu;                ///< 最大传输单元（字节）
    uint32_t timeout_ms;         ///< 超时时间（毫秒）
};

/**
 * @enum LinkStatus
 * @brief 链路状态枚举
 * 
 * 定义物理链路的运行状态。
 */
enum class LinkStatus {
    ERROR,          ///< 错误状态，链路不可用
    OPEN,           ///< 已打开状态，链路可正常收发数据
    CLOSED          ///< 已关闭状态，链路未激活
};

}  // namespace PhysicalLayer
}  // namespace MB_DDS
