# MB_DDF - 弹载高性能数据分发框架

## 项目简介

MB_DDF（Missile Borne Data Distribution Framework）是一个用于弹上高实时性场景的、基于共享内存和无锁环形缓冲区实现的高性能数据分发系统。该项目采用发布者-订阅者模式，支持多种物理层通信协议，为实时系统提供低延迟、高吞吐量的数据分发服务。

## 核心特性

- **高性能通信**：基于共享内存和无锁环形缓冲区，实现微秒级延迟
- **发布-订阅模式**：支持一对多、多对一的灵活消息传递
- **多物理层支持**：支持CAN、UDP、串口等多种通信协议
- **消息完整性**：内置校验和机制确保数据完整性
- **时间戳支持**：纳秒级精度的消息时间戳
- **线程安全**：采用无锁设计和互斥锁保护关键资源
- **单例模式**：DDSCore采用单例模式，便于全局管理

## 项目结构

```
MB_DDF/
├── src/MB_DDF/
│   ├── DDS/                    # DDS核心模块
│   │   ├── DDSCore.h/cpp       # DDS主控制类（单例）
│   │   ├── Message.h           # 消息结构定义
│   │   ├── Publisher.h/cpp     # 发布者实现
│   │   ├── Subscriber.h/cpp    # 订阅者实现
│   │   ├── RingBuffer.h/cpp    # 无锁环形缓冲区
│   │   ├── SharedMemory.h/cpp  # 共享内存管理
│   │   └── TopicRegistry.h/cpp # Topic注册表管理
│   ├── Debug/                  # 调试模块
│   │   ├── Logger.h            # 日志系统
│   │   └── LoggerExtensions.h  # 日志扩展
│   ├── Monitor/                # 监控模块
│   │   ├── DDSMonitor.h/cpp    # DDS系统监控
│   │   └── SharedMemoryAccessor.h/cpp # 共享内存访问器
│   ├── PhysicalLayer/          # 物理层模块
│   │   ├── BasicTypes.h        # 基础类型定义
│   │   ├── IPhysicalLink.h     # 物理链路接口
│   │   ├── BasePhysicalLink.h/cpp # 物理链路基类
│   │   └── UdpLink.h/cpp       # UDP链路实现
│   └── Test/                   # 测试程序
│       ├── TestPubSub*.cpp     # 发布-订阅测试
│       ├── TestPub*.cpp        # 发布者测试
│       ├── TestSub*.cpp        # 订阅者测试
│       ├── TestMonitor.cpp     # 监控测试
│       └── TestFunc*.cpp       # 模拟业务测试
├── CMakeLists.txt              # CMake构建配置
├── build.sh                    # 构建脚本
└── README.md                   # 项目文档
```

## 系统要求

- **操作系统**：Linux（支持POSIX共享内存）
- **编译器**：支持C++20标准的编译器（GCC 10+, Clang 12+）
- **CMake**：版本3.10或更高
- **依赖库**：
  - pthread（线程库）
  - rt（实时扩展库，用于共享内存）

## 快速开始

### 1. 编译项目

使用提供的构建脚本：

```bash
# 构建Debug版本（默认）
./build.sh

# 构建Release版本
./build.sh -r

# 清理后重新构建
./build.sh -c

# 构建并自动运行所有测试程序
./build.sh -t

# 查看帮助信息
./build.sh -h
```

### 2. 手动使用CMake

```bash
# 创建构建目录
mkdir build && cd build

# 配置CMake（Debug版本）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 或配置Release版本
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(nproc)

# 查看构建信息
make info
```

### 3. 运行测试程序

编译完成后，可执行文件位于`build/`目录：

```bash
# 运行发布-订阅测试
./build/TestPubSub1

# 运行监控程序
./build/TestMonitor

# 运行其他测试程序
./build/TestPub1
./build/TestSub1
```

## API使用示例

### 基本发布-订阅示例

```cpp
#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"

int main() {
    // 设置日志级别
    LOG_SET_LEVEL_INFO();
    
    // 获取DDS实例并初始化
    auto& dds = MB_DDF::DDS::DDSCore::instance();
    dds.initialize(128 * 1024 * 1024); // 分配128MB共享内存
    
    // 创建发布者
    auto publisher = dds.create_publisher("local://test_topic");
    
    // 创建订阅者（带回调函数）
    auto subscriber = dds.create_subscriber("local://test_topic", 
        [](const void* data, size_t size, uint64_t timestamp) {
            const char* msg = static_cast<const char*>(data);
            LOG_INFO << "Received: " << msg;
        });
    
    // 发布数据
    std::string message = "Hello, DDS!";
    publisher->write(message.c_str(), message.size());
    
    // 读取数据
    char buffer[1024];
    size_t size = sizeof(buffer);
    subscriber->read_latest(buffer, size);
    
    return 0;
}
```

### 物理层地址配置

```cpp
#include "MB_DDF/PhysicalLayer/BasicTypes.h"

// 创建CAN地址
auto can_addr = MB_DDF::PhysicalLayer::Address::createCAN(0x123);

// 创建UDP地址
auto udp_addr = MB_DDF::PhysicalLayer::Address::createUDP("192.168.1.100", 8080);

// 创建串口地址
auto serial_addr = MB_DDF::PhysicalLayer::Address::createSerial(1); // COM1
```

## 核心组件说明

### DDSCore
- **功能**：DDS系统的主控制类，采用单例模式
- **职责**：管理共享内存、Topic注册、发布者/订阅者创建
- **版本**：当前版本 0x00004001

### 消息系统
- **MessageHeader**：包含魔数、Topic ID、序列号、时间戳、数据大小和校验和
- **Message**：完整消息结构，包含消息头和数据部分
- **校验机制**：支持CRC32校验确保数据完整性

### 共享内存
- **SharedMemoryManager**：管理进程间共享内存区域
- **RingBuffer**：无锁环形缓冲区，支持高并发读写
- **TopicRegistry**：Topic注册表，管理Topic元数据

### 物理层
- **支持协议**：CAN/CANFD、UDP、串口
- **地址类型**：统一的地址抽象，支持多种物理介质
- **链路配置**：支持波特率、MTU、超时等参数配置

## 调试和监控

### 日志系统
```cpp
// 设置日志级别
LOG_SET_LEVEL_TRACE();  // 最详细
LOG_SET_LEVEL_DEBUG();
LOG_SET_LEVEL_INFO();   // 推荐
LOG_SET_LEVEL_WARN();
LOG_SET_LEVEL_ERROR();

// 控制日志格式
LOG_DISABLE_TIMESTAMP();     // 禁用时间戳
LOG_DISABLE_FUNCTION_LINE(); // 禁用函数名和行号
```

### 系统监控
- **DDSMonitor**：提供DDS系统运行状态监控
- **SharedMemoryAccessor**：共享内存访问统计和监控

### GDB调试
```bash
# Debug版本支持GDB调试
make debug  # 自动启动GDB调试第一个测试程序
```

## 性能特性

- **延迟**：微秒级消息传递延迟
- **吞吐量**：基于共享内存，支持高频数据传输
- **内存效率**：无锁环形缓冲区，避免内存拷贝
- **并发性**：支持多发布者、多订阅者并发访问

## 应用场景

- **实时控制系统**：自动驾驶、飞行控制、船舶控制
- **高频数据采集**：传感器数据分发、监控系统
- **进程间通信**：需要低延迟、高可靠性的IPC场景
- **分布式系统**：微服务间的高性能消息传递

## 注意事项

1. **共享内存大小**：根据实际数据量调整初始化时的内存大小
2. **Topic命名**：使用有意义的Topic名称，支持`local://`前缀
3. **线程安全**：发布者和订阅者都是线程安全的
4. **资源清理**：程序退出时会自动清理共享内存资源
5. **错误处理**：注意检查初始化和创建操作的返回值

## 许可证

本项目采用开源许可证，具体许可证信息请查看项目根目录下的LICENSE文件。

## 贡献指南

欢迎提交Issue和Pull Request来改进项目。在提交代码前，请确保：

1. 代码符合项目的编码规范
2. 通过所有测试用例
3. 添加必要的文档和注释
4. 遵循C++20标准

## 联系方式

- **作者**：Jiangkai
- **项目地址**：[项目仓库链接]
- **问题反馈**：通过GitHub Issues提交

---

*最后更新：2025年1月*
