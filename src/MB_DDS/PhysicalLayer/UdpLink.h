/**
 * @file UdpLink.h
 * @brief UDP物理链路实现类定义
 * @date 2025-10-10
 * @author Jiangkai
 * 
 * 定义基于UDP协议的物理链路实现类。
 * 提供UDP网络通信的完整功能，包括socket管理、数据收发、
 * 地址转换等。支持单播、广播和组播通信模式。
 */

#pragma once

#include "BasePhysicalLink.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <mutex>

namespace MB_DDS {
namespace PhysicalLayer {

/**
 * @class UdpLink
 * @brief UDP物理链路实现类（Linux平台）
 * 
 * 基于UDP协议实现的物理链路类，继承自BasePhysicalLink。
 * 提供完整的UDP网络通信功能，支持：
 * - IPv4单播、广播和组播通信
 * - 非阻塞和阻塞数据接收模式
 * - Socket选项配置（广播、地址重用等）
 * - 自动地址转换和错误处理
 * 
 * 实现特点：
 * - 线程安全的socket操作
 * - 自动资源管理（RAII）
 * - 灵活的配置选项
 * - 完整的错误处理机制
 * 
 * 注意：当前实现针对Linux平台，使用POSIX socket API。
 */
class UdpLink : public BasePhysicalLink {
private:
    int socket_fd_;                     ///< UDP socket文件描述符，-1表示未创建
    struct sockaddr_in local_sockaddr_; ///< 本地socket地址结构体
    struct sockaddr_in remote_sockaddr_;///< 远程socket地址结构体（用于点对点通信）
    
    /**
     * @brief 创建UDP socket
     * @return bool 创建成功返回true，失败返回false
     * 
     * 创建UDP类型的socket文件描述符。
     * 设置socket为非阻塞模式，并进行基本的错误检查。
     * 失败时会设置相应的错误状态。
     */
    bool createSocket();
    
    /**
     * @brief 绑定socket到本地地址
     * @return bool 绑定成功返回true，失败返回false
     * 
     * 将创建的socket绑定到配置的本地IP地址和端口。
     * 支持绑定到特定接口或所有接口（0.0.0.0）。
     * 绑定失败时会关闭socket并设置错误状态。
     */
    bool bindSocket();
    
    /**
     * @brief 关闭socket连接
     * 
     * 安全关闭socket文件描述符，释放系统资源。
     * 重置socket_fd_为-1，防止重复关闭。
     * 此方法是幂等的，可以安全地多次调用。
     */
    void closeSocket();
    
    /**
     * @brief 将Address对象转换为sockaddr_in结构体
     * @param addr 源Address对象
     * @param sockaddr 输出的sockaddr_in结构体
     * @return bool 转换成功返回true，失败返回false
     * 
     * 将通用的Address对象转换为socket API所需的sockaddr_in结构体。
     * 仅处理UDP类型的地址，其他类型返回false。
     * 执行地址格式验证和网络字节序转换。
     */
    bool addressToSockaddr(const Address& addr, struct sockaddr_in& sockaddr);
    
    /**
     * @brief 将sockaddr_in结构体转换为Address对象
     * @param sockaddr 源sockaddr_in结构体
     * @return Address 转换后的Address对象
     * 
     * 将socket API的sockaddr_in结构体转换为通用的Address对象。
     * 执行网络字节序到主机字节序的转换。
     * 返回UDP类型的Address对象。
     */
    Address sockaddrToAddress(const struct sockaddr_in& sockaddr);
    
public:
    /**
     * @brief 默认构造函数
     * 
     * 初始化UDP链路对象，设置socket_fd_为-1（未创建状态）。
     * 清零地址结构体，为后续初始化做准备。
     */
    UdpLink();
    
    /**
     * @brief 虚析构函数
     * 
     * 确保对象销毁时正确释放资源。
     * 自动关闭socket连接，防止资源泄漏。
     */
    virtual ~UdpLink();
    
    // 实现基类的纯虚函数
    
    /**
     * @brief 初始化UDP链路
     * @param config 链路配置参数
     * @return bool 初始化成功返回true，失败返回false
     * 
     * 根据配置参数初始化UDP链路。
     * 验证配置参数的有效性，特别是UDP地址格式。
     * 保存配置信息供后续操作使用。
     */
    bool initialize(const LinkConfig& config) override;
    
    /**
     * @brief 打开UDP链路
     * @return bool 打开成功返回true，失败返回false
     * 
     * 创建UDP socket并绑定到本地地址。
     * 成功时链路状态变为OPEN，失败时保持CLOSED状态。
     * 可重复调用，已打开的链路会先关闭再重新打开。
     */
    bool open() override;
    
    /**
     * @brief 关闭UDP链路
     * @return bool 关闭成功返回true，失败返回false
     * 
     * 关闭UDP socket连接，释放系统资源。
     * 链路状态变为CLOSED。此操作总是成功的。
     */
    bool close() override;
    
    /**
     * @brief 通过UDP发送数据
     * @param data 待发送的数据缓冲区指针
     * @param length 数据长度（字节数）
     * @param dest_addr 目标地址
     * @return bool 发送成功返回true，失败返回false
     * 
     * 通过UDP协议发送数据到指定目标地址。
     * 支持单播、广播和组播发送。
     * 检查数据长度不超过MTU限制。
     */
    bool send(const uint8_t* data, uint32_t length, const Address& dest_addr) override;
    
    /**
     * @brief 非阻塞方式接收UDP数据
     * @param buffer 接收数据的缓冲区指针
     * @param buffer_size 缓冲区大小（字节数）
     * @param src_addr 输出参数，存储发送方地址信息
     * @return int32_t 实际接收的字节数，0表示无数据，负数表示错误
     * 
     * 以非阻塞方式从UDP socket接收数据。
     * 如果当前无数据可读，立即返回0。
     * 自动解析发送方的IP地址和端口信息。
     */
    int32_t receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr) override;
    
    /**
     * @brief 阻塞方式接收UDP数据（带超时）
     * @param buffer 接收数据的缓冲区指针
     * @param buffer_size 缓冲区大小（字节数）
     * @param src_addr 输出参数，存储发送方地址信息
     * @param timeout_us 超时时间（微秒）
     * @return int32_t 实际接收的字节数，0表示超时，负数表示错误
     * 
     * 以阻塞方式从UDP socket接收数据，支持超时控制。
     * 使用select()系统调用实现超时机制。
     * 在指定超时时间内等待数据到达。
     */
    int32_t receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr, uint32_t timeout_us) override;
    
    /**
     * @brief 设置UDP链路特定的自定义参数
     * @param key 参数名称
     * @param value 参数值
     * @return bool 设置成功返回true，失败返回false
     * 
     * 设置UDP特有的socket选项和参数。
     * 支持的参数包括：
     * - "broadcast": 启用/禁用广播模式
     * - "reuseaddr": 启用/禁用地址重用
     * - "rcvbuf": 设置接收缓冲区大小
     * - "sndbuf": 设置发送缓冲区大小
     */
    bool setCustomParameter(const std::string& key, const std::string& value) override;
    
    // UDP特定的公共方法
    
    /**
     * @brief 设置广播模式
     * @param enable true启用广播，false禁用广播
     * @return bool 设置成功返回true，失败返回false
     * 
     * 启用或禁用UDP socket的广播功能。
     * 启用后可以发送广播数据包到255.255.255.255。
     * 必须在socket创建后调用。
     */
    bool setBroadcast(bool enable);
    
    /**
     * @brief 设置地址重用选项
     * @param enable true启用地址重用，false禁用地址重用
     * @return bool 设置成功返回true，失败返回false
     * 
     * 启用或禁用SO_REUSEADDR socket选项。
     * 允许多个socket绑定到同一个地址和端口。
     * 通常用于服务器程序的快速重启。
     */
    bool setReuseAddress(bool enable);
    
    /**
     * @brief 获取socket文件描述符
     * @return int socket文件描述符，-1表示未创建
     * 
     * 返回内部使用的socket文件描述符。
     * 可用于高级socket操作或与其他网络库集成。
     * 返回-1表示socket尚未创建或已关闭。
     */
    int getSocketFd() const { return socket_fd_; }
};

}  // namespace PhysicalLayer
}  // namespace MB_DDS