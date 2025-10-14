/**
 * @file BasePhysicalLink.h
 * @brief 物理链路基类定义
 * @date 2025-10-10
 * @author Jiangkai
 * 
 * 定义物理链路的抽象基类，提供通用功能实现。
 * 包含状态管理、配置验证、错误处理等公共方法。
 * 具体的物理链路实现类应继承此基类以复用通用功能。
 */

#pragma once

#include "IPhysicalLink.h"
#include <string>

namespace MB_DDS {
namespace PhysicalLayer {
    
/**
 * @class BasePhysicalLink
 * @brief 物理链路抽象基类
 * 
 * 提供物理链路的通用功能实现，包括状态管理、配置验证等。
 * 实现了IPhysicalLink接口中的部分通用方法，简化具体链路类的开发。
 * 
 * 基类负责：
 * - 链路状态的统一管理
 * - 配置参数的验证和存储
 * - 错误信息的处理和获取
 * - 通用接口方法的默认实现
 * 
 * 派生类需要实现具体的通信协议相关方法。
 */
class BasePhysicalLink : public IPhysicalLink {
protected:
    LinkConfig config_;     ///< 链路配置参数，存储初始化时的配置信息
    LinkStatus status_;     ///< 当前链路状态，用于状态管理和查询
    
    /**
     * @brief 设置链路状态
     * @param new_status 新的链路状态
     * 
     * 更新内部状态变量，提供状态变更的统一入口。
     * 派生类应通过此方法更新状态，而不是直接修改status_成员。
     * 可在此方法中添加状态变更的日志记录或回调通知。
     */
    void setStatus(LinkStatus new_status);
    
    /**
     * @brief 验证链路配置参数
     * @param config 待验证的配置参数
     * @return bool 配置有效返回true，无效返回false
     * 
     * 检查配置参数的有效性，包括地址格式、MTU大小、超时值等。
     * 提供通用的配置验证逻辑，派生类可重写以添加特定验证规则。
     * 在initialize方法中调用，确保配置参数的正确性。
     */
    bool validateConfig(const LinkConfig& config);
    
    /**
     * @brief 获取最后一次错误的字符串描述
     * @return std::string 错误描述字符串
     * 
     * 返回最近一次操作失败的详细错误信息。
     * 用于调试和错误诊断，提供比返回码更详细的错误描述。
     * 派生类应在发生错误时更新错误信息。
     */
    std::string getLastErrorString() const;
    
public:
    /**
     * @brief 默认构造函数
     * 
     * 初始化基类成员变量，将链路状态设置为CLOSED。
     * 配置参数将在initialize方法中设置。
     */
    BasePhysicalLink() : status_(LinkStatus::CLOSED) {}
    
    /**
     * @brief 获取链路当前状态
     * @return LinkStatus 当前链路状态
     * 
     * 实现IPhysicalLink接口的getStatus方法。
     * 返回内部维护的状态变量，无需派生类重新实现。
     */
    LinkStatus getStatus() const override { return status_; }
    
    /**
     * @brief 获取链路最大传输单元
     * @return uint16_t MTU值（字节数）
     * 
     * 实现IPhysicalLink接口的getMTU方法。
     * 返回配置中设置的MTU值，无需派生类重新实现。
     */
    uint16_t getMTU() const override { return config_.mtu; }
    
    /**
     * @brief 获取本地地址
     * @return Address 本地地址对象
     * 
     * 实现IPhysicalLink接口的getLocalAddress方法。
     * 返回配置中设置的本地地址，无需派生类重新实现。
     */
    Address getLocalAddress() const override { return config_.local_addr; }
    
    // 纯虚函数 - 由具体物理链路子类实现
    
    /**
     * @brief 初始化物理链路（纯虚函数）
     * @param config 链路配置参数
     * @return bool 初始化成功返回true，失败返回false
     * 
     * 派生类必须实现此方法，完成具体链路类型的初始化工作。
     * 应在实现中调用validateConfig验证配置参数。
     */
    virtual bool initialize(const LinkConfig& config) = 0;
    
    /**
     * @brief 打开物理链路（纯虚函数）
     * @return bool 打开成功返回true，失败返回false
     * 
     * 派生类必须实现此方法，完成具体链路的打开操作。
     * 成功时应调用setStatus(LinkStatus::OPEN)更新状态。
     */
    virtual bool open() = 0;
    
    /**
     * @brief 关闭物理链路（纯虚函数）
     * @return bool 关闭成功返回true，失败返回false
     * 
     * 派生类必须实现此方法，完成具体链路的关闭操作。
     * 成功时应调用setStatus(LinkStatus::CLOSED)更新状态。
     */
    virtual bool close() = 0;
    
    /**
     * @brief 发送数据到指定地址（纯虚函数）
     * @param data 待发送的数据缓冲区指针
     * @param length 数据长度（字节数）
     * @param dest_addr 目标地址
     * @return bool 发送成功返回true，失败返回false
     * 
     * 派生类必须实现此方法，完成具体协议的数据发送。
     * 应检查数据长度不超过MTU限制。
     */
    virtual bool send(const uint8_t* data, uint32_t length, const Address& dest_addr) = 0;
    
    /**
     * @brief 非阻塞方式接收数据（纯虚函数）
     * @param buffer 接收数据的缓冲区指针
     * @param buffer_size 缓冲区大小（字节数）
     * @param src_addr 输出参数，存储发送方地址信息
     * @return int32_t 实际接收的字节数，0表示无数据，负数表示错误
     * 
     * 派生类必须实现此方法，完成具体协议的非阻塞数据接收。
     */
    virtual int32_t receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr) = 0;
    
    /**
     * @brief 阻塞方式接收数据（纯虚函数）
     * @param buffer 接收数据的缓冲区指针
     * @param buffer_size 缓冲区大小（字节数）
     * @param src_addr 输出参数，存储发送方地址信息
     * @param timeout_us 超时时间（微秒）
     * @return int32_t 实际接收的字节数，0表示超时，负数表示错误
     * 
     * 派生类必须实现此方法，完成具体协议的阻塞数据接收。
     */
    virtual int32_t receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr, uint32_t timeout_us) = 0;
    
    /**
     * @brief 设置链路特定的自定义参数（纯虚函数）
     * @param key 参数名称
     * @param value 参数值
     * @return bool 设置成功返回true，失败返回false
     * 
     * 派生类必须实现此方法，处理特定链路类型的自定义参数。
     */
    virtual bool setCustomParameter(const std::string& key, const std::string& value) = 0;
};

}  // namespace PhysicalLayer
}  // namespace MB_DDS
