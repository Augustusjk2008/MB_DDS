# MB_DDF — 弹载高性能数据分发框架（按当前 src 内容重写）

## 项目简介

MB_DDF（Missile-Borne Data Distribution Framework）面向高实时性与高可靠性场景，采用共享内存与无锁环形缓冲的发布-订阅模型，提供微秒级延迟的数据分发能力。物理层遵循数据面/控制面分层，当前实现包含 UDP、RS422 设备，以及 CAN/CAN-FD、Helm 等设备适配与 XDMA/SPI 等控制面入口，可据硬件能力扩展。

## 核心特性

- 发布/订阅高性能：共享内存 + 无锁 `RingBuffer`
- 清晰分层：数据面 `ILink` 与控制面 `IDeviceTransport`
- 设备适配丰富：`Rs422Device`、`CanDevice`、`CanFdDevice`、`HelmDevice`
- 控制面实现：`XdmaTransport`、`SpiTransport`、`NullTransport`
- 事件聚合：`EventMultiplexer`；统一事件 fd/等待机制
- 调试与监控：`Logger`、`DDSMonitor`、`SharedMemoryAccessor`
- 定时能力：`SystemTimer`（支持 `s/ms/us/ns` 周期）

## 目录结构（基于 src/MB_DDF）

```
src/MB_DDF/
├── DDS/                      # DDS 核心
│   ├── DDSCore.{h,cpp}
│   ├── Message.h
│   ├── Publisher.{h,cpp}
│   ├── Subscriber.{h,cpp}
│   ├── RingBuffer.{h,cpp}
│   ├── SharedMemory.{h,cpp}
│   ├── SemaphoreGuard.h
│   └── TopicRegistry.{h,cpp}
├── Debug/                    # 日志与调试
│   ├── Logger.h
│   └── LoggerExtensions.h
├── Monitor/                  # 运行监控
│   ├── DDSMonitor.{h,cpp}
│   └── SharedMemoryAccessor.{h,cpp}
├── PhysicalLayer/            # 物理层（数据面/控制面/设备）
│   ├── DataPlane/
│   │   ├── ILink.h
│   │   └── UdpLink.{h,cpp}
│   ├── ControlPlane/
│   │   ├── IDeviceTransport.h
│   │   ├── XdmaTransport.{h,cpp}
│   │   ├── SpiTransport.{h,cpp}
│   │   └── NullTransport.h
│   ├── Device/
│   │   ├── CanDevice.{h,cpp}
│   │   ├── CanFdDevice.{h,cpp}
│   │   ├── CanFdDeviceHardware.cpp
│   │   ├── Rs422Device.{h,cpp}
│   │   ├── HelmDevice.{h,cpp}
│   │   └── TransportLinkAdapter.h
│   ├── EventMultiplexer.{h,cpp}
│   ├── Hardware/
│   │   ├── pl_can.h
│   │   └── pl_canfd.h
│   ├── Support/Log.h
│   └── Types.h               # TransportConfig/LinkConfig/Endpoint 等
├── Timer/
│   ├── SystemTimer.{h,cpp}
│   └── ChronoHelper.{h,cpp}
└── Test/                     # 测试程序（可执行）
    ├── TestPub* / TestSub* / TestPubSub*
    ├── TestMonitor.cpp
    ├── TestPhysicalLayer.cpp
    ├── TestRealTime.cpp
    ├── TestPublishPerf.cpp
    ├── TestFuncAutoPilot.cpp
    ├── TestFuncFlyControl.cpp
    ├── TestFuncHelmControl.cpp
    └── TestWithUI.cpp        # 需要 FTXUI
```

根目录还包含：`CMakeLists.txt`（构建配置）、`build.sh`（一键构建/交叉编译/测试）、`docs/can/` 与 `docs/canfd/`（硬件资料）。

## 构建与运行

### 使用脚本（推荐）

- `./build.sh`：本机 Debug 构建，输出到 `build/host/`
- `./build.sh -r`：本机 Release 构建
- `./build.sh -c`：清理后构建（清理 `build/`）
- `./build.sh -t`：构建后后台启动所有本机测试程序
- `./build.sh --cross`：ARM aarch64 交叉编译（Yocto/UCAS），输出到 `build/aarch64/`
- `./build.sh --both`：同时构建本机与交叉产物

提示：脚本会生成 `build/host/compile_commands.json` 并在根目录创建软链 `compile_commands.json`，便于 IDE/clangd 使用。

### 直接使用 CMake

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel $(nproc)
# 查看构建信息
cmake --build . --target info
```

交叉编译（手动）：

```bash
source /opt/wanghuo/v2.0.0-rc4/environment-setup-armv8a-ucas-linux
cmake .. -DCMAKE_BUILD_TYPE=Release -DCROSS_COMPILE=ON \
  -DCMAKE_TOOLCHAIN_FILE=$OECORE_CMAKE_TOOLCHAIN_FILE \
  -DCMAKE_SYSROOT=$SDKTARGETSYSROOT \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY
cmake --build . --parallel $(nproc)
```

### 运行测试程序

- 本机构建产物位于 `build/host/`：例如运行 `./build/host/TestMonitor`
- 交叉构建产物位于 `build/aarch64/`：需拷贝至目标板运行（例如 ARM64）

`TestWithUI` 依赖 FTXUI（本机 `/usr/local` 或交叉环境 `/usr/local`），若未找到将自动跳过该目标。

## 系统要求与依赖

- 操作系统：Linux
- 编译器：GCC/Clang，支持 C++20
- 构建工具：CMake ≥ 3.10
- 基础库：`pthread`、`rt`
- 可选库：
  - `liburing`（启用 `MB_DDF_HAS_IOURING`，io_uring 异步支持）
  - `libaio`（启用 `MB_DDF_HAS_LIBAIO`，异步 DMA）
  - `libgpiod`（GPIO 事件支持，供 `SpiTransport` 使用）
  - `ftxui`（仅 `TestWithUI` 需要）

## 快速示例

### 发布-订阅（共享内存）

```cpp
#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"
int main(){
  LOG_SET_LEVEL_INFO();
  auto& dds = MB_DDF::DDS::DDSCore::instance();
  dds.initialize(64 * 1024 * 1024);
  auto pub = dds.create_publisher("local://topic");
  auto sub = dds.create_subscriber("local://topic", true,
    [](const void* d,size_t n,uint64_t ts){ LOG_INFO << "n=" << n; });
  const char msg[] = "hello"; pub->write(msg, sizeof(msg));
}
```

### UDP 链路

```cpp
#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
using namespace MB_DDF::PhysicalLayer::DataPlane;
int main(){ UdpLink link; LinkConfig c; c.name="127.0.0.1:6000|127.0.0.1:7000"; link.open(c); }
```

### RS422 + XDMA 控制面

```cpp
#include "MB_DDF/PhysicalLayer/Device/Rs422Device.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/XdmaTransport.h"
using namespace MB_DDF::PhysicalLayer;
int main(){
  ControlPlane::XdmaTransport tp; TransportConfig tpc; tpc.device_path="/dev/xdma/rs422_0"; /* 可选: tpc.device_offset=0x0 */
  tp.open(tpc); Device::Rs422Device dev(tp, 2048); LinkConfig lc; dev.open(lc);
}
```

## 物理层设计要点

- 数据面 `ILink`：统一 `open/close/send/receive/ioctl`；提供超时阻塞接收与事件 fd
- 控制面 `IDeviceTransport`：统一寄存器/DMA/事件访问；支持 `XdmaTransport`、`SpiTransport`
- 设备适配：`TransportLinkAdapter` 桥接控制面至数据面；`Rs422Device` 等按设备寄存器实现
- 事件聚合：`EventMultiplexer` 将多事件源集合为统一等待接口
- 典型配置：`TransportConfig.device_path`（基路径，派生 `_user/_h2c/_c2h/_events`），`TransportConfig.device_offset`（设备偏移，示例：`0x00000`），事件编号/通道号等
- UDP 配置：`LinkConfig.name` 支持 `"<local_port>"` 或 `"<local_ip>:<local_port>|<remote_ip>:<remote_port>"`
- RS422 限制：单次 `send` 最多 255 字节；内部按 4 字节对齐写寄存器并触发发送命令

## 日志/监控与定时器

- 日志：`Logger.h` 提供等级与格式控制（`TRACE/DEBUG/INFO/WARN/ERROR`）
- 监控：`DDSMonitor` 与 `SharedMemoryAccessor` 提供共享内存/Topic 观测
- 定时器：`SystemTimer` 支持在信号处理上下文或独立线程执行；可配置 `SCHED_FIFO/RR`、优先级与绑核

## IDE/Clangd（交叉场景）

- 生成 `build/host/compile_commands.json` 并软链到根目录
- 建议设置 clangd `--query-driver=/opt/wanghuo/v2.0.0-rc4/sysroots/x86_64-ucassdk-linux/usr/bin/aarch64-ucas-linux/*` 以解析交叉包含路径

## 测试程序速览

- 发布订阅：`TestPubSub1/2`，发布者/订阅者：`TestPub1/2`、`TestSub1/2/3`
- 监控：`TestMonitor`
- 物理层：`TestPhysicalLayer`（含控制面/设备示例与注释）
- 性能与实时：`TestPublishPerf`、`TestRealTime`
- 业务模拟：`TestFuncAutoPilot`、`TestFuncFlyControl`、`TestFuncHelmControl`
- UI 演示：`TestWithUI`（需要 FTXUI）

## 文档与资料

- `docs/can/can.md`、`docs/can/can.pdf`
- `docs/canfd/pg223-canfd.pdf`

## 注意事项

- 共享内存大小需按数据量调整（`DDSCore::initialize(bytes)`）
- Topic 前缀建议包含域，如 `local://` 或 `192.168.x.x://`
- 对于 `SpiTransport`，若无 `libgpiod` 将禁用 GPIO 事件 fd（自动检测）
- 若未找到 `liburing/libaio`，将禁用对应异步特性（自动检测）
- `TestWithUI` 需 FTXUI；未安装将跳过该可执行目标

## 贡献与反馈

- 作者：Jiangkai
- 问题反馈：请在代码库内提 Issue 或通过既有渠道沟通

——

最后更新：2025-11-13
