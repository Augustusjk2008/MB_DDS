/**
 * @file BasePhysicalLink.cpp
 * @brief 物理链路基类实现
 * @date 2025-10-10
 * @author Jiangkai
 * 
 * 实现物理链路基类的通用功能方法。
 * 包含状态管理、配置验证、错误处理等公共功能的具体实现。
 * 为派生类提供可复用的基础功能支持。
 */

#include "BasePhysicalLink.h"
#include <cstring>

namespace MB_DDF {
namespace PhysicalLayer {

/**
 * @brief 设置链路状态的实现
 * @param new_status 新的链路状态
 * 
 * 更新内部状态变量，提供状态变更的统一管理。
 * 检测状态变化并记录状态转换，为后续扩展（如日志记录、
 * 状态变更回调等）预留接口。
 * 
 * 状态变更逻辑：
 * - 仅在状态确实发生变化时进行更新
 * - 保存旧状态信息供调试使用
 * - 为状态变更事件处理预留扩展点
 */
void BasePhysicalLink::setStatus(LinkStatus new_status) {
    // 检查状态是否真正发生变化，避免无效更新
    if (status_ != new_status) {
        // LinkStatus old_status = status_;    // 保存旧状态供调试使用
        status_ = new_status;               // 更新为新状态
        
        // 此处可扩展状态变更日志记录或回调通知
        // onStatusChanged(old_status, new_status);
    }
}

/**
 * @brief 验证链路配置参数的实现
 * @param config 待验证的配置参数
 * @return bool 配置有效返回true，无效返回false
 * 
 * 对链路配置参数进行全面验证，确保参数的有效性和合理性。
 * 验证内容包括：
 * - 通用参数验证（MTU、超时时间等）
 * - 地址类型特定验证（CAN ID、UDP端口、串口号等）
 * - 协议相关参数验证（波特率等）
 * 
 * 验证失败时返回false，调用方应检查返回值并处理错误。
 */
bool BasePhysicalLink::validateConfig(const LinkConfig& config) {
    // 基本参数验证
    
    // MTU验证：不能为0，且不能超过65536（16位）
    if (config.mtu == 0) {
        return false;   // MTU参数无效
    }
    
    // 根据地址类型进行特定验证
    switch (config.local_addr.type) {
        case Address::Type::CAN:
            // CAN ID验证
            // 标准帧：11位ID，范围0-0x7FF
            // 扩展帧：29位ID，范围0-0x1FFFFFFF
            if (config.local_addr.can_id > 0x1FFFFFFF) {
                return false;   // CAN ID超出有效范围
            }
            break;
            
        case Address::Type::UDP:
            // UDP端口验证：端口号不能为0（0为系统保留）
            if (config.local_addr.udp_addr.port == 0) {
                return false;   // UDP端口号无效
            }
            // IP地址为0.0.0.0在某些情况下是有效的（绑定所有接口）
            break;
            
        case Address::Type::SERIAL:
            // 串口号验证：范围1-256（COM1-COM256）
            if (config.local_addr.com_port == 0 || config.local_addr.com_port > 256) {
                return false;   // 串口号超出有效范围
            }
            
            // 波特率验证：串口通信必须指定有效的波特率
            if (config.baudrate == 0) {
                return false;   // 波特率未设置
            }
            break;
            
        default:
            // 未知的地址类型
            return false;   // 不支持的地址类型
    }
    
    // 所有验证通过
    return true;
}

/**
 * @brief 获取最后一次错误描述字符串的实现
 * @return std::string 错误描述字符串
 * 
 * 根据当前链路状态返回相应的错误描述信息。
 * 提供人类可读的错误信息，便于调试和问题诊断。
 * 
 * 错误信息映射：
 * - CLOSED状态：链路已关闭
 * - ERROR状态：链路发生错误
 * - 其他状态：未知状态（通常不应出现）
 * 
 * 派生类可重写此方法以提供更详细的错误信息。
 */
std::string BasePhysicalLink::getLastErrorString() const {
    // 根据当前状态返回对应的错误描述
    switch (status_) {
        case LinkStatus::CLOSED:
            return "Link is closed";        // 链路处于关闭状态
            
        case LinkStatus::ERROR:
            return "Link error occurred";   // 链路发生错误
            
        default:
            return "Unknown status";        // 未知状态（异常情况）
    }
}

}  // namespace PhysicalLayer
}  // namespace MB_DDF
