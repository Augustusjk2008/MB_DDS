/**
 * @file IPhysicalLink.h
 * @brief 物理链路接口定义
 * @date 2025-10-10
 * @author Jiangkai
 * 
 * 定义物理层通信链路的抽象接口。
 * 提供统一的物理链路操作接口，支持CAN、UDP、串口等多种通信方式。
 * 所有具体的物理链路实现类都必须继承并实现此接口。
 */

#pragma once

#include "BasicTypes.h"

namespace MB_DDF {
namespace PhysicalLayer {

/**
 * @class IPhysicalLink
 * @brief 物理链路抽象接口类
 * 
 * 定义物理层通信链路的标准接口，提供链路初始化、数据收发、
 * 状态管理等核心功能。支持同步和异步数据接收模式。
 * 
 * 接口设计遵循RAII原则，支持链路的完整生命周期管理。
 * 所有方法都是纯虚函数，需要在派生类中具体实现。
 */
class IPhysicalLink {
public:
    /**
     * @brief 虚析构函数
     * 
     * 确保派生类对象能够正确析构，释放相关资源。
     * 使用default关键字提供默认实现。
     */
    virtual ~IPhysicalLink() = default;
    
    /**
     * @brief 初始化物理链路
     * @param config 链路配置参数，包含地址、波特率、MTU等信息
     * @return bool 初始化成功返回true，失败返回false
     * 
     * 根据提供的配置参数初始化物理链路。
     * 此方法必须在调用其他操作方法之前成功执行。
     * 配置参数的有效性由具体实现类负责验证。
     */
    virtual bool initialize(const LinkConfig& config) = 0;
    
    /**
     * @brief 打开物理链路
     * @return bool 打开成功返回true，失败返回false
     * 
     * 激活物理链路，使其进入可收发数据的状态。
     * 必须在成功初始化后调用。打开失败时链路保持关闭状态。
     */
    virtual bool open() = 0;
    
    /**
     * @brief 关闭物理链路
     * @return bool 关闭成功返回true，失败返回false
     * 
     * 停用物理链路，释放相关系统资源（如套接字、串口句柄等）。
     * 关闭后的链路需要重新打开才能继续使用。
     */
    virtual bool close() = 0;
    
    /**
     * @brief 发送数据到指定地址
     * @param data 待发送的数据缓冲区指针
     * @param length 数据长度（字节数）
     * @param dest_addr 目标地址
     * @return bool 发送成功返回true，失败返回false
     * 
     * 通过物理链路发送数据到指定的目标地址。
     * 数据长度不能超过链路的MTU限制。
     * 发送操作通常是非阻塞的，但具体行为由实现类决定。
     */
    virtual bool send(const uint8_t* data, uint32_t length, const Address& dest_addr) = 0;
    
    /**
     * @brief 非阻塞方式接收数据
     * @param buffer 接收数据的缓冲区指针
     * @param buffer_size 缓冲区大小（字节数）
     * @param src_addr 输出参数，存储发送方地址信息
     * @return int32_t 实际接收的字节数，0表示无数据，负数表示错误
     * 
     * 以非阻塞方式从物理链路接收数据。
     * 如果当前无数据可读，立即返回0。
     * 接收到的数据不超过缓冲区大小和链路MTU的较小值。
     */
    virtual int32_t receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr) = 0;
    
    /**
     * @brief 阻塞方式接收数据（带超时）
     * @param buffer 接收数据的缓冲区指针
     * @param buffer_size 缓冲区大小（字节数）
     * @param src_addr 输出参数，存储发送方地址信息
     * @param timeout_us 超时时间（微秒）
     * @return int32_t 实际接收的字节数，0表示超时，负数表示错误
     * 
     * 以阻塞方式从物理链路接收数据，支持超时控制。
     * 在指定超时时间内等待数据到达，超时后返回0。
     * 适用于需要同步等待数据的应用场景。
     */
    virtual int32_t receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr, uint32_t timeout_us) = 0;
    
    /**
     * @brief 获取链路当前状态
     * @return LinkStatus 链路状态枚举值（ERROR/OPEN/CLOSED）
     * 
     * 返回物理链路的当前运行状态。
     * 状态信息用于判断链路是否可用以及诊断问题。
     */
    virtual LinkStatus getStatus() const = 0;
    
    /**
     * @brief 获取链路最大传输单元
     * @return uint16_t MTU值（字节数）
     * 
     * 返回当前链路支持的最大数据包大小。
     * 发送数据时不应超过此限制，否则可能导致发送失败。
     */
    virtual uint16_t getMTU() const = 0;
    
    /**
     * @brief 获取本地地址
     * @return Address 本地地址对象
     * 
     * 返回当前链路绑定的本地地址信息。
     * 地址格式取决于具体的物理链路类型（CAN ID、IP端口等）。
     */
    virtual Address getLocalAddress() const = 0;
    
    /**
     * @brief 设置链路特定的自定义参数
     * @param key 参数名称
     * @param value 参数值
     * @return bool 设置成功返回true，失败返回false
     * 
     * 提供扩展机制，允许设置特定链路类型的专有参数。
     * 例如CAN FD的比特率切换、串口的流控制等。
     * 参数的有效性和支持情况由具体实现类决定。
     */
    virtual bool setCustomParameter(const std::string& key, const std::string& value) = 0;
};

}  // namespace PhysicalLayer
}  // namespace MB_DDF
    
