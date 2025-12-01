#pragma once
#include <cstdint>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Hardware {

// 寄存器地址映射
enum class IORegAddr : uint32_t {
    // 数字输出
    QBXH = 0x04,
    AZJB = 0x08,
    YXKJ = 0x0C,
    SJL_DO1 = 0x10,
    SJL_DO2 = 0x14,
    SJL_DO3 = 0x18,
    SJL_DO4 = 0x1C,
    SJL_DO5 = 0x20,

    // 数字输入
    YXBJ = 0x24,
    SJL_DI1 = 0x28,
    SJL_DI2 = 0x2C,
    FSD = 0x30,

    // 导引头PWM切换
    DYT_SWITCH = 0x40,
    DYT_RSVD = 0x44
};

}   // namespace Hardware
}   // namespace PhysicalLayer
}   // namespace MB_DDF
