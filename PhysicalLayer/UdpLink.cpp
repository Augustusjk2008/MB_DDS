/**
 * @file UdpLink.cpp
 * @brief UDP物理链路实现类的具体实现
 * @date 2025-10-10
 * @author Jiangkai
 * 
 * 实现基于UDP协议的物理链路类的所有方法。
 * 包括socket管理、数据收发、地址转换、错误处理等功能。
 * 使用POSIX socket API实现跨平台的UDP网络通信。
 */

#include "UdpLink.h"
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

namespace MB_DDS {
namespace PhysicalLayer {

/**
 * @brief 构造函数实现
 * 
 * 初始化UDP链路对象的所有成员变量。
 * 将socket文件描述符设置为-1（未创建状态），
 * 清零本地和远程地址结构体。
 */
UdpLink::UdpLink() 
    : socket_fd_(-1) {
    memset(&local_sockaddr_, 0, sizeof(local_sockaddr_));
    memset(&remote_sockaddr_, 0, sizeof(remote_sockaddr_));
}

/**
 * @brief 析构函数实现
 * 
 * 确保对象销毁时正确释放所有资源。
 * 调用close()方法关闭socket连接，防止资源泄漏。
 */
UdpLink::~UdpLink() {
    close();
}

/**
 * @brief 初始化UDP链路实现
 * @param config 链路配置参数
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 验证配置参数的有效性，特别检查地址类型必须为UDP。
 * 将Address对象转换为socket API所需的sockaddr_in结构体。
 * 保存配置信息供后续操作使用。
 * 
 * 处理流程：
 * 1. 调用基类的配置验证方法
 * 2. 检查本地地址类型是否为UDP
 * 3. 转换本地地址格式
 * 4. 如果存在远程地址，也进行转换
 * 5. 设置链路状态为CLOSED
 */
bool UdpLink::initialize(const LinkConfig& config) {
    // 验证配置参数
    if (!validateConfig(config)) {
        setStatus(LinkStatus::ERROR);
        return false;
    }
    
    // 检查地址类型是否为UDP
    if (config.local_addr.type != Address::Type::UDP) {
        setStatus(LinkStatus::ERROR);
        return false;
    }
    
    // 保存配置
    config_ = config;
    
    // 转换地址格式
    if (!addressToSockaddr(config.local_addr, local_sockaddr_)) {
        setStatus(LinkStatus::ERROR);
        return false;
    }
    
    // 如果有远程地址，也进行转换
    if (config.remote_addr.type == Address::Type::UDP) {
        if (!addressToSockaddr(config.remote_addr, remote_sockaddr_)) {
            setStatus(LinkStatus::ERROR);
            return false;
        }
    }
    
    setStatus(LinkStatus::CLOSED);
    return true;
}

/**
 * @brief 打开UDP链路实现
 * @return bool 打开成功返回true，失败返回false
 * 
 * 创建UDP socket并绑定到配置的本地地址。
 * 如果链路已经打开，直接返回成功。
 * 
 * 处理流程：
 * 1. 检查当前状态，如果已打开则直接返回
 * 2. 创建UDP socket
 * 3. 绑定socket到本地地址
 * 4. 设置链路状态为OPEN
 * 
 * 错误处理：
 * - socket创建失败时设置ERROR状态
 * - 绑定失败时关闭socket并设置ERROR状态
 */
bool UdpLink::open() {    
    if (status_ == LinkStatus::OPEN) {
        return true;  // 已经打开
    }
    
    // 创建socket
    if (!createSocket()) {
        setStatus(LinkStatus::ERROR);
        return false;
    }
    
    // 绑定到本地地址
    if (!bindSocket()) {
        closeSocket();
        setStatus(LinkStatus::ERROR);
        return false;
    }
    
    setStatus(LinkStatus::OPEN);
    return true;
}

/**
 * @brief 关闭UDP链路实现
 * @return bool 关闭成功返回true，失败返回false
 * 
 * 关闭UDP socket连接，释放系统资源。
 * 如果链路已经关闭，直接返回成功。
 * 
 * 处理流程：
 * 1. 检查当前状态，如果已关闭则直接返回
 * 2. 调用closeSocket()关闭socket
 * 3. 设置链路状态为CLOSED
 * 
 * 注意：此操作总是成功的，不会返回失败。
 */
bool UdpLink::close() {    
    if (status_ == LinkStatus::CLOSED) {
        return true;  // 已经关闭
    }
    
    // 关闭socket
    closeSocket();
    
    setStatus(LinkStatus::CLOSED);
    return true;
}

/**
 * @brief 通过UDP发送数据实现
 * @param data 待发送的数据缓冲区指针
 * @param length 数据长度（字节数）
 * @param dest_addr 目标地址
 * @return bool 发送成功返回true，失败返回false
 * 
 * 通过UDP协议发送数据到指定目标地址。
 * 支持单播、广播和组播发送模式。
 * 
 * 处理流程：
 * 1. 检查链路状态必须为OPEN
 * 2. 验证数据指针和长度的有效性
 * 3. 检查数据长度不超过MTU限制
 * 4. 转换目标地址格式
 * 5. 调用sendto()系统调用发送数据
 * 6. 验证发送字节数是否完整
 * 
 * 错误处理：
 * - 链路未打开时返回false
 * - 数据无效时返回false
 * - 超过MTU限制时返回false
 * - 地址转换失败时返回false
 * - 发送失败时设置ERROR状态并返回false
 */
bool UdpLink::send(const uint8_t* data, uint32_t length, const Address& dest_addr) {
    if (status_ != LinkStatus::OPEN) {
        return false;
    }
    
    if (!data || length == 0) {
        return false;
    }
    
    // 检查长度是否超过MTU
    if (length > config_.mtu) {
        return false;
    }
    
    // 转换目标地址
    struct sockaddr_in dest_sockaddr;
    if (!addressToSockaddr(dest_addr, dest_sockaddr)) {
        return false;
    }
    
    // 发送数据
    ssize_t sent_bytes = sendto(socket_fd_, data, length, 0, 
                               (struct sockaddr*)&dest_sockaddr, sizeof(dest_sockaddr));
    
    if (sent_bytes < 0) {
        // 发送失败
        setStatus(LinkStatus::ERROR);
        return false;
    }
    
    return sent_bytes == static_cast<ssize_t>(length);
}

/**
 * @brief 非阻塞方式接收UDP数据实现
 * @param buffer 接收数据的缓冲区指针
 * @param buffer_size 缓冲区大小（字节数）
 * @param src_addr 输出参数，存储发送方地址信息
 * @return int32_t 实际接收的字节数，0表示无数据，负数表示错误
 * 
 * 以非阻塞方式从UDP socket接收数据。
 * 如果当前无数据可读，立即返回0而不阻塞。
 * 
 * 处理流程：
 * 1. 检查链路状态必须为OPEN
 * 2. 验证缓冲区指针和大小的有效性
 * 3. 调用recvfrom()以MSG_DONTWAIT标志接收数据
 * 4. 处理EAGAIN/EWOULDBLOCK错误（无数据可读）
 * 5. 转换发送方地址格式
 * 
 * 返回值说明：
 * - 正数：实际接收的字节数
 * - 0：当前无数据可读
 * - 负数：发生错误
 * 
 * 错误处理：
 * - 链路未打开时返回-1
 * - 缓冲区无效时返回-1
 * - 接收错误时设置ERROR状态并返回-1
 */
int32_t UdpLink::receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr) {
    if (status_ != LinkStatus::OPEN) {
        return -1;
    }
    
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    struct sockaddr_in src_sockaddr;
    socklen_t addr_len = sizeof(src_sockaddr);
    
    // 非阻塞接收
    ssize_t received_bytes = recvfrom(socket_fd_, buffer, buffer_size, MSG_DONTWAIT,
                                     (struct sockaddr*)&src_sockaddr, &addr_len);
    
    if (received_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // 没有数据可读
        }
        setStatus(LinkStatus::ERROR);
        return -1;
    }
    
    // 转换源地址
    src_addr = sockaddrToAddress(src_sockaddr);
    
    return static_cast<int32_t>(received_bytes);
}

/**
 * @brief 阻塞方式接收UDP数据实现（带超时）
 * @param buffer 接收数据的缓冲区指针
 * @param buffer_size 缓冲区大小（字节数）
 * @param src_addr 输出参数，存储发送方地址信息
 * @param timeout_us 超时时间（微秒）
 * @return int32_t 实际接收的字节数，0表示超时，负数表示错误
 * 
 * 以阻塞方式从UDP socket接收数据，支持超时控制。
 * 使用select()系统调用实现精确的超时机制。
 * 
 * 处理流程：
 * 1. 检查链路状态必须为OPEN
 * 2. 验证缓冲区指针和大小的有效性
 * 3. 设置文件描述符集合和超时时间
 * 4. 调用select()等待数据到达或超时
 * 5. 如果有数据可读，调用recvfrom()接收
 * 6. 转换发送方地址格式
 * 
 * 返回值说明：
 * - 正数：实际接收的字节数
 * - 0：超时，未接收到数据
 * - 负数：发生错误
 * 
 * 错误处理：
 * - 链路未打开时返回-1
 * - 缓冲区无效时返回-1
 * - select()失败时设置ERROR状态并返回-1
 * - 接收错误时设置ERROR状态并返回-1
 */
int32_t UdpLink::receive(uint8_t* buffer, uint32_t buffer_size, Address& src_addr, uint32_t timeout_us) {
    if (status_ != LinkStatus::OPEN) {
        return -1;
    }
    
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    // 设置超时
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd_, &read_fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_us / 1000000;
    timeout.tv_usec = timeout_us % 1000000;
    
    int select_result = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    
    if (select_result < 0) {
        setStatus(LinkStatus::ERROR);
        return -1;
    }
    
    if (select_result == 0) {
        return 0;  // 超时
    }
    
    // 有数据可读
    struct sockaddr_in src_sockaddr;
    socklen_t addr_len = sizeof(src_sockaddr);
    
    ssize_t received_bytes = recvfrom(socket_fd_, buffer, buffer_size, 0,
                                     (struct sockaddr*)&src_sockaddr, &addr_len);
    
    if (received_bytes < 0) {
        setStatus(LinkStatus::ERROR);
        return -1;
    }
    
    // 转换源地址
    src_addr = sockaddrToAddress(src_sockaddr);
    
    return static_cast<int32_t>(received_bytes);
}

/**
 * @brief 设置UDP链路特定的自定义参数实现
 * @param key 参数名称
 * @param value 参数值
 * @return bool 设置成功返回true，失败返回false
 * 
 * 设置UDP特有的socket选项和参数。
 * 支持字符串形式的参数配置，便于配置文件使用。
 * 
 * 支持的参数：
 * - "broadcast": 启用/禁用广播模式（值："true"/"1" 或 "false"/"0"）
 * - "reuseaddr": 启用/禁用地址重用（值："true"/"1" 或 "false"/"0"）
 * 
 * 处理流程：
 * 1. 解析参数名称
 * 2. 将字符串值转换为布尔值
 * 3. 调用相应的设置方法
 * 
 * 返回值：
 * - true：参数设置成功
 * - false：不支持的参数或设置失败
 */
bool UdpLink::setCustomParameter(const std::string& key, const std::string& value) {
    if (key == "broadcast") {
        return setBroadcast(value == "true" || value == "1");
    } else if (key == "reuseaddr") {
        return setReuseAddress(value == "true" || value == "1");
    }
    
    return false;  // 不支持的参数
}

/**
 * @brief 设置广播模式实现
 * @param enable true启用广播，false禁用广播
 * @return bool 设置成功返回true，失败返回false
 * 
 * 启用或禁用UDP socket的SO_BROADCAST选项。
 * 启用后可以发送广播数据包到255.255.255.255或子网广播地址。
 * 
 * 处理流程：
 * 1. 检查socket是否已创建
 * 2. 设置SO_BROADCAST socket选项
 * 
 * 注意：必须在socket创建后调用此方法。
 */
bool UdpLink::setBroadcast(bool enable) {
    if (socket_fd_ < 0) {
        return false;
    }
    
    int broadcast = enable ? 1 : 0;
    return setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0;
}

/**
 * @brief 设置地址重用选项实现
 * @param enable true启用地址重用，false禁用地址重用
 * @return bool 设置成功返回true，失败返回false
 * 
 * 启用或禁用SO_REUSEADDR socket选项。
 * 允许多个socket绑定到同一个地址和端口，
 * 通常用于服务器程序的快速重启。
 * 
 * 处理流程：
 * 1. 检查socket是否已创建
 * 2. 设置SO_REUSEADDR socket选项
 * 
 * 注意：必须在socket创建后、bind()之前调用此方法。
 */
bool UdpLink::setReuseAddress(bool enable) {
    if (socket_fd_ < 0) {
        return false;
    }
    
    int reuse = enable ? 1 : 0;
    return setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0;
}

/**
 * @brief 创建UDP socket实现
 * @return bool 创建成功返回true，失败返回false
 * 
 * 创建UDP类型的socket文件描述符并进行基本配置。
 * 设置socket为非阻塞模式，默认启用地址重用。
 * 
 * 处理流程：
 * 1. 调用socket()创建UDP socket
 * 2. 获取当前文件描述符标志
 * 3. 添加O_NONBLOCK标志设置非阻塞模式
 * 4. 默认启用地址重用选项
 * 
 * 错误处理：
 * - socket创建失败时返回false
 * - fcntl操作失败时关闭socket并返回false
 * 
 * 注意：创建的socket默认为非阻塞模式，适合高性能网络应用。
 */
bool UdpLink::createSocket() {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // 设置为非阻塞模式
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    if (fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // 默认启用地址重用
    setReuseAddress(true);
    
    return true;
}

/**
 * @brief 绑定socket到本地地址实现
 * @return bool 绑定成功返回true，失败返回false
 * 
 * 将创建的socket绑定到配置的本地IP地址和端口。
 * 绑定后socket可以接收发送到该地址的数据包。
 * 
 * 处理流程：
 * 1. 检查socket是否已创建
 * 2. 调用bind()系统调用绑定地址
 * 
 * 错误处理：
 * - socket未创建时返回false
 * - bind()失败时返回false（可能是地址已被占用）
 * 
 * 注意：绑定失败通常是因为地址已被其他进程占用。
 */
bool UdpLink::bindSocket() {
    if (socket_fd_ < 0) {
        return false;
    }
    
    int result = bind(socket_fd_, (struct sockaddr*)&local_sockaddr_, sizeof(local_sockaddr_));
    return result == 0;
}

/**
 * @brief 关闭socket连接实现
 * 
 * 安全关闭socket文件描述符，释放系统资源。
 * 重置socket_fd_为-1，防止重复关闭。
 * 
 * 处理流程：
 * 1. 检查socket是否有效（>= 0）
 * 2. 调用close()系统调用关闭socket
 * 3. 重置socket_fd_为-1
 * 
 * 注意：此方法是幂等的，可以安全地多次调用。
 * 使用::close()避免与类的close()方法名冲突。
 */
void UdpLink::closeSocket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

/**
 * @brief 将Address对象转换为sockaddr_in结构体实现
 * @param addr 源Address对象
 * @param sockaddr 输出的sockaddr_in结构体
 * @return bool 转换成功返回true，失败返回false
 * 
 * 将通用的Address对象转换为socket API所需的sockaddr_in结构体。
 * 执行地址格式验证和网络字节序转换。
 * 
 * 处理流程：
 * 1. 检查地址类型必须为UDP
 * 2. 清零目标结构体
 * 3. 设置地址族为AF_INET
 * 4. 转换IP地址为网络字节序
 * 5. 转换端口号为网络字节序
 * 
 * 错误处理：
 * - 非UDP类型地址返回false
 * 
 * 注意：使用htonl()和htons()进行主机字节序到网络字节序的转换。
 */
bool UdpLink::addressToSockaddr(const Address& addr, struct sockaddr_in& sockaddr) {
    if (addr.type != Address::Type::UDP) {
        return false;
    }
    
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(addr.udp_addr.ip);
    sockaddr.sin_port = htons(addr.udp_addr.port);
    
    return true;
}

/**
 * @brief 将sockaddr_in结构体转换为Address对象实现
 * @param sockaddr 源sockaddr_in结构体
 * @return Address 转换后的Address对象
 * 
 * 将socket API的sockaddr_in结构体转换为通用的Address对象。
 * 执行网络字节序到主机字节序的转换。
 * 
 * 处理流程：
 * 1. 提取IP地址并转换为主机字节序
 * 2. 提取端口号并转换为主机字节序
 * 3. 创建UDP类型的Address对象
 * 
 * 注意：使用ntohl()和ntohs()进行网络字节序到主机字节序的转换。
 * 返回的Address对象类型为UDP。
 */
Address UdpLink::sockaddrToAddress(const struct sockaddr_in& sockaddr) {
    uint32_t ip = ntohl(sockaddr.sin_addr.s_addr);
    uint16_t port = ntohs(sockaddr.sin_port);
    
    return Address::createUDP(ip, port);
}

}  // namespace PhysicalLayer
}  // namespace MB_DDS