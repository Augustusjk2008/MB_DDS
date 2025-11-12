/**
 * @file CanFDDeviceHardware.cpp
 * @brief 实现CAN FD设备的硬件操作
 */
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include "MB_DDF/PhysicalLayer/Device/CanFdDevice.h"
#include "MB_DDF/PhysicalLayer/Hardware/pl_canfd.h"
#include <cstdint>
#include <cstring>
#include <unistd.h>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {
// 返回可用发送缓冲区索引
uint32_t CanFDDevice::__axiCanfdGetFreeBuffer(void) {
    uint32_t  uiRegVal = 0;
    uint32_t  uiIndex  = 0;

    rd32(XCANFD_TRR_OFFSET, uiRegVal);
    while (uiRegVal & (1 << uiIndex)) {
        uiIndex++;
        if (uiIndex == MAX_BUFFER_INDEX) {
            break;
        }
    }

    if (uiIndex == MAX_BUFFER_INDEX) {
        return  (33);
    }
    return uiIndex;
}

// 计算TRR bit位
uint32_t CanFDDevice::__axiCanfdTrrValGetSetBitPosition(uint32_t uiValue) const {
    uint32_t uiCount1;
    uint32_t uiCount2;

    uiValue -= 1;
    uiCount1 = uiValue - ((uiValue) & FAST_MATH_MASK1) - ((uiValue >> SHIFT2) & FAST_MATH_MASK2);
    uiCount2 = ((uiCount1 + (uiCount1 >> SHIFT3)) & FAST_MATH_MASK3) % EXTRACTION_MASK;
    return uiCount2;
}

// CanFD字节序转换
uint32_t CanFDDevice::__axiEndianSwap32(uint32_t uiData) const {
    uint16_t usLoWord;
    uint16_t usHiWord;

    usLoWord = (uint16_t) (uiData & 0x0000FFFF);
    usHiWord = (uint16_t) ((uiData & 0xFFFF0000) >> 16);
    usLoWord = (uint16_t)(((usLoWord & 0xFF00) >> 8) | ((usLoWord & 0x00FF) << 8));
    usHiWord = (uint16_t)(((usHiWord & 0xFF00) >> 8) | ((usHiWord & 0x00FF) << 8));
    return  ((((uint32_t)usLoWord) << (uint32_t)16) | (uint32_t)usHiWord);
}

// 中断使能
void CanFDDevice::__axiCanfdInterruptEnable(uint32_t uiMask) {
    uint32_t IntrValue;

    rd32(XCANFD_IER_OFFSET, IntrValue);
    IntrValue |= uiMask & XCANFD_IXR_ALL;
    wr32(XCANFD_IER_OFFSET, IntrValue);
}

// 设备滤波使能
void CanFDDevice::__axiCanfdAcceptFilterEnable(uint32_t uiFilterIndexMask) {
    uint32_t uiEnabledFilters;

    rd32(XCANFD_AFR_OFFSET, uiEnabledFilters);
    uiEnabledFilters |= uiFilterIndexMask;
    wr32(XCANFD_AFR_OFFSET, uiEnabledFilters);
}

// 滤波设置
int CanFDDevice::__axiCanfdAcceptFilterSet(uint32_t uiFilterIndex, uint32_t uiMaskValue, uint32_t uiIdValue) {
    uint32_t uiEnabledFilters;

    rd32(XCANFD_AFR_OFFSET, uiEnabledFilters);  // 查看过滤器是否启用
    if ((uiEnabledFilters & uiFilterIndex) == uiFilterIndex) {  // 如果已经被启用了将无法设置
        LOGW("canfd", "accept filter set", -1, "filter is enabled");
        return  -1;
    }

    uiFilterIndex--;
    wr32(XCANFD_AFMR_OFFSET(uiFilterIndex), uiMaskValue);
    wr32(XCANFD_AFIDR_OFFSET(uiFilterIndex), uiIdValue);
    return 0;
}

// 设备滤波禁用
void CanFDDevice::__axiCanfdAcceptFilterDisable(uint32_t uiFilterIndexMask) {
    uint32_t uiEnabledFilters;

    rd32(XCANFD_AFR_OFFSET, uiEnabledFilters);
    uiEnabledFilters &= (~uiFilterIndexMask);
    wr32(XCANFD_AFR_OFFSET, uiEnabledFilters);  // 禁用掉对应过滤器
}

// 设备滤波设置
uint32_t CanFDDevice::__axiCanfdSetFilter(uint32_t uiFilterIndex, uint32_t uiMask, uint32_t uiId) {
    uint32_t uiFMask = XCanFd_CreateIdValue(uiMask, 0, 0, 0, 0);    // 将AMR进行转换
    uint32_t uiFId   = XCanFd_CreateIdValue(uiId, 0, 0, 0, 0);      // 将ACR进行转换
    if((uiFilterIndex > 32) || (uiFilterIndex == 0)) {
        LOGW("canfd", "set Filter error", -1, "filter index error");
        return -1;
    }

    // 设置滤波：先禁用，再设置，最后启用
    __axiCanfdAcceptFilterDisable((1 << (uiFilterIndex - 1)));
    __axiCanfdAcceptFilterSet(uiFilterIndex, uiFMask, uiFId);
    __axiCanfdAcceptFilterEnable((1 << (uiFilterIndex - 1)));
    return 0;
}

// 获取CANFD模式
uint8_t CanFDDevice::__axiCanfdGetMode(void) {
    uint8_t   ucModeStatus;
    uint32_t  uiRegVal = 0;

    rd32(XCANFD_SR_OFFSET, uiRegVal);

    if (uiRegVal & XCANFD_SR_CONFIG_MASK) {
        ucModeStatus = XCANFD_MODE_CONFIG;
    } else if (uiRegVal & XCANFD_SR_SLEEP_MASK) {
        ucModeStatus = XCANFD_MODE_SLEEP;
    } else if (uiRegVal & XCANFD_SR_NORMAL_MASK) {
        if ((uiRegVal & XCANFD_SR_SNOOP_MASK) != 0) {
            ucModeStatus = XCANFD_MODE_SNOOP;
        } else {
            ucModeStatus = XCANFD_MODE_NORMAL;
        }
    } else {
        ucModeStatus = XCANFD_MODE_LOOPBACK;
    }

    if ((uiRegVal & XCANFD_SR_PEE_CONFIG_MASK) != 0) {
        ucModeStatus = ucModeStatus | XCANFD_MODE_PEE;
    }

    return ucModeStatus;
}

// CANFD位时间设置
int CanFDDevice::__axiCanfdSetBitTiming(uint8_t ucSyncJumpWidth, uint8_t ucTimeSegment2, uint16_t ucTimeSegment1) {
    uint32_t uiValue;

    if (ucSyncJumpWidth > XCANFD_MAX_SJW_VALUE ||
        ucTimeSegment2  > XCANFD_MAX_TS2_VALUE ||
        ucTimeSegment1  > XCANFD_MAX_TS1_VALUE) {
        return 0;
    }

    if (__axiCanfdGetMode() != XCANFD_MODE_CONFIG) {
        return -1;
    }

    uiValue = ((uint32_t)ucTimeSegment1) & XCANFD_BTR_TS1_MASK;
    uiValue |= (((uint32_t)ucTimeSegment2) << XCANFD_BTR_TS2_SHIFT) & XCANFD_BTR_TS2_MASK;
    uiValue |= (((uint32_t)ucSyncJumpWidth) << XCANFD_BTR_SJW_SHIFT) & XCANFD_BTR_SJW_MASK;

    wr32(XCANFD_BTR_OFFSET, uiValue);
    return 0;
}

// 设置仲裁域波特率
int CanFDDevice::__axiCanfdSetBaudRatePrescaler(uint8_t ucPrescaler) {
    if (__axiCanfdGetMode() != XCANFD_MODE_CONFIG) {    // 判断是否处于配置模式
        return  -1;
    }

    wr32(XCANFD_BRPR_OFFSET, (uint32_t)ucPrescaler);
    return  0;
}

// 设置数据域位时间
int CanFDDevice::__axiCanfdSetFBitTiming(uint8_t ucSyncJumpWidth, uint8_t ucTimeSegment2, uint8_t ucTimeSegment1) {
    uint32_t uiValue;
    if (ucSyncJumpWidth > XCANFD_MAX_F_SJW_VALUE ||
        ucTimeSegment2 > XCANFD_MAX_F_TS2_VALUE ||
        ucTimeSegment1 > XCANFD_MAX_F_TS1_VALUE) {
        return  -1;
    }

    if (__axiCanfdGetMode() != XCANFD_MODE_CONFIG) {    // 判断是否处于配置模式
        return  -1;
    }

    uiValue  = ((uint32_t)ucTimeSegment1) & XCANFD_F_BTR_TS1_MASK;
    uiValue |= (((uint32_t)ucTimeSegment2) << XCANFD_F_BTR_TS2_SHIFT) & XCANFD_F_BTR_TS2_MASK;
    uiValue |= (((uint32_t)ucSyncJumpWidth) << XCANFD_F_BTR_SJW_SHIFT) & XCANFD_F_BTR_SJW_MASK;

    wr32(XCANFD_F_BTR_OFFSET, uiValue);
    return 0;
}

// 设置数据域波特率
int CanFDDevice::__axiCanfdSetFBaudRatePrescaler(uint8_t ucPrescaler) {
    uint32_t uiRegValue;
    if (__axiCanfdGetMode() != XCANFD_MODE_CONFIG) {    // 判断是否处于配置模式
        return  -1;
    }

    rd32(XCANFD_F_BRPR_OFFSET, uiRegValue);
    uiRegValue |= ((uint32_t)ucPrescaler & XCANFD_BRPR_BRP_MASK);
    wr32(XCANFD_F_BRPR_OFFSET, uiRegValue);
    return  0;
}

// CANFD模式设置
int CanFDDevice::__axiCanfdEnterMode(uint8_t ucMode) {
    uint8_t   ucCurMode;
    uint32_t  uiMsrReg;

    if ((ucMode != XCANFD_MODE_CONFIG) && (ucMode != XCANFD_MODE_SLEEP)    &&
        (ucMode != XCANFD_MODE_NORMAL) && (ucMode != XCANFD_MODE_LOOPBACK) &&
        (ucMode != XCANFD_MODE_SNOOP)  && (ucMode != XCANFD_MODE_PEE)      &&
        (ucMode != XCANFD_MODE_ABR)    && (ucMode != XCANFD_MODE_DAR)      &&
        (ucMode != XCANFD_MODE_SBR)) {
       LOGW("canfd", "enter Mode error", -1, "mode error");
       return -1;
    }

    ucCurMode = __axiCanfdGetMode();    // 获取当前的 CAN 模式 
    rd32(XCANFD_MSR_OFFSET, uiMsrReg);
    uiMsrReg = uiMsrReg & XCANFD_MSR_CONFIG_MASK;

    if ((ucCurMode == (uint8_t)XCANFD_MODE_NORMAL) &&
        (ucMode == (uint8_t)XCANFD_MODE_SLEEP)) {
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_SLEEP_MASK | uiMsrReg));
        return 0;
    } else if((ucCurMode == (uint8_t)XCANFD_MODE_SLEEP) && (ucMode == (uint8_t)XCANFD_MODE_NORMAL)) {
        wr32(XCANFD_MSR_OFFSET, uiMsrReg);
        return 0;
    }
    // 如果不是上述两种模式，则在设置模式之前必须切换到配置模式，再进行模式切换
    wr32(XCANFD_SRR_OFFSET, 0);
    if (__axiCanfdGetMode() != (uint8_t)XCANFD_MODE_CONFIG) {   // 判断是否进入配置模式
        LOGE("canfd", "enter Mode error", XCANFD_MODE_CONFIG, "not in config mode");
        return -1;
    }

    switch (ucMode) {
    case XCANFD_MODE_CONFIG:                                            // 已经处于配置模式
        break;
    case XCANFD_MODE_SLEEP:                                             // 切换到睡眠模式
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_SLEEP_MASK | uiMsrReg));
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
    case XCANFD_MODE_NORMAL:                                            // 切换正常模式
        wr32(XCANFD_MSR_OFFSET, uiMsrReg);
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
    case XCANFD_MODE_LOOPBACK:                                          // 切换回环模式
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_LBACK_MASK | uiMsrReg));
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
        // 下面的模式配置方式来自于Linux源码
    case XCANFD_MODE_SNOOP:
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_SNOOP_MASK | uiMsrReg));
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
    case XCANFD_MODE_ABR:
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_ABR_MASK | uiMsrReg));
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
    case XCANFD_MODE_SBR:
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_SBR_MASK | uiMsrReg));
        break;
    case XCANFD_MODE_PEE:
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_DPEE_MASK | uiMsrReg));
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
    case XCANFD_MODE_DAR:
        wr32(XCANFD_MSR_OFFSET, (XCANFD_MSR_DAR_MASK | uiMsrReg));
        wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_CEN_MASK);
        break;
    }
    return 0;
}

// 禁用正常模式下的波特率切换功能
void CanFDDevice::__axiCanfdSetBitRateSwitchDisableNominal(void) {
    uint32_t uiResult;

    rd32(XCANFD_MSR_OFFSET, uiResult);
    if (!(uiResult & XCANFD_SRR_CEN_MASK)) {
        uiResult = uiResult & (~XCANFD_MSR_BRSD_MASK);
        wr32(XCANFD_MSR_OFFSET, uiResult);
    }
}

// CANFD硬件初始化
int CanFDDevice::__axiCanfdHwInit(void) {
    // 重置设备进入初始状态
    wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_SRST_MASK);

    __axiCanfdEnterMode(XCANFD_MODE_CONFIG);                /* 进入配置模式                 */
    while (__axiCanfdGetMode() != XCANFD_MODE_CONFIG);
    // 设置波特率默认配置位仲裁域1M 数据域 4M
    wr32(XCANFD_BRPR_OFFSET, 1);
    __axiCanfdSetBitTiming(3, 3, 14);
    __axiCanfdSetFBaudRatePrescaler(0);
    __axiCanfdSetFBitTiming(1, 1, 6);

    wr32(XCANFD_IETRS_OFFSET, 0xFFFFFFFF);  // 使能发送FIFO中断
    __axiCanfdInterruptEnable((XCANFD_IXR_TXOK_MASK  |       // 使能中断
                                           XCANFD_IXR_BSOFF_MASK |
                                           XCANFD_IXR_RXMNF_MASK |
                                           XCANFD_IXR_TXEWMFLL_MASK|
                                           XCANFD_IXR_TXEOFLW_MASK|
                                           XCANFD_IXR_RXFWMFLL_1_MASK |
                                           XCANFD_IXR_RXFOFLW_1_MASK |
                                           XCANFD_IXR_TXRRS_MASK |
                                           XCANFD_IXR_RXFWMFLL_MASK |
                                           XCANFD_IXR_BSRD_MASK |
                                           XCANFD_IXR_RXOK_MASK |
                                           XCANFD_IXR_RXFOFLW_MASK |
                                           XCANFD_IXR_ERROR_MASK));
    __axiCanfdSetBitRateSwitchDisableNominal();               // 配置波特率切换功能

    __axiCanfdEnterMode(XCANFD_MODE_NORMAL);                // 进入工作模式
    while (__axiCanfdGetMode() != XCANFD_MODE_NORMAL);

    return 0;
}

// 处理CANFD总线离线事件
void CanFDDevice::__axiCanfdPeeBusOffHandler() {
    uint32_t uiRegValue;
    uint32_t uiValue;

    rd32(XCANFD_TRR_OFFSET, uiRegValue);
    wr32(XCANFD_TCR_OFFSET, uiRegValue);
    rd32(XCANFD_TRR_OFFSET, uiValue);
    while (uiValue != 0) {
        rd32(XCANFD_TRR_OFFSET, uiValue);
    }

    wr32(XCANFD_TRR_OFFSET, uiRegValue);
}

// CANFD中断处理
int CanFDDevice::__axiCanfdIrqHandel() {
    uint32_t uiIrqStatus;
    CanFrame canFrame;

    rd32(XCANFD_ISR_OFFSET, uiIrqStatus);
    if (uiIrqStatus == 0) {
        return 0;
    }
    wr32(XCANFD_IER_OFFSET, 0);

    // 查看中断状态并进行处理
    if (uiIrqStatus & XCANFD_IXR_SLP_MASK) {
        wr32(XCANFD_ICR_OFFSET, XCANFD_IXR_SLP_MASK);
        __axiCanfdEnterMode(XCANFD_MODE_SLEEP);
    }

    if (uiIrqStatus & XCANFD_IXR_BSOFF_MASK) {
        __axiCanfdPeeBusOffHandler();
    }

    if (uiIrqStatus & XCANFD_IXR_PEE_MASK) {
        __axiCanfdPeeBusOffHandler();
    }

    if (uiIrqStatus & (XCANFD_IXR_RXFWMFLL_MASK | XCANFD_IXR_RXOK_MASK |
                       XCANFD_IXR_RXFWMFLL_1_MASK | XCANFD_IXR_RXRBF_MASK)) {
        __axiCanfdRecvFifo(&canFrame);

    }

    if (uiIrqStatus & (XCANFD_IXR_TXOK_MASK | XCANFD_IXR_TXEWMFLL_MASK)) {
        // 发送成功中断，此处留空暂不处理
    }

    if (uiIrqStatus & XCANFD_IXR_RXFOFLW_MASK) {
        LOGE("canfd", "irq", -1, "Receive Data Overflow");
    }

    if (uiIrqStatus & XCANFD_IXR_TXEOFLW_MASK) {
        LOGE("canfd", "irq", -1, "TX Event FIFO Overflow");
    }

    // 清中断
    rd32(XCANFD_ISR_OFFSET, uiIrqStatus);
    wr32(XCANFD_ICR_OFFSET, uiIrqStatus);

    __axiCanfdInterruptEnable((XCANFD_IXR_TXOK_MASK  |      /* 重新使能设备中断             */
                                           XCANFD_IXR_BSOFF_MASK |
                                           XCANFD_IXR_RXMNF_MASK |
                                           XCANFD_IXR_TXEWMFLL_MASK|
                                           XCANFD_IXR_TXEOFLW_MASK|
                                           XCANFD_IXR_RXFWMFLL_1_MASK |
                                           XCANFD_IXR_RXFOFLW_1_MASK |
                                           XCANFD_IXR_TXRRS_MASK |
                                           XCANFD_IXR_RXFWMFLL_MASK |
                                           XCANFD_IXR_BSRD_MASK |
                                           XCANFD_IXR_RXOK_MASK |
                                           XCANFD_IXR_RXFOFLW_MASK |
                                           XCANFD_IXR_ERROR_MASK));
    return 0;
}

// CanFD发送
int CanFDDevice::__axiCanfdSend(CanFrame *pCanFrame) {
    if (pCanFrame == nullptr) {
        LOGW("canfd", "send", -1, "pCanFrame is nullptr");
        return -1;
    }
    
    uint32_t uiId           = 0;
    uint32_t uiTrrVal       = 0;
    uint32_t uiValue        = 0;
    uint32_t uiDlc          = 0;
    uint32_t uiDwIndex      = 0;
    uint32_t uiFreeTxBuffer = 0;
    uint32_t uiTxFrame[16];
    uint32_t uiDw           = 0;

    // 计算实际数据长度
    // 应由使用者写入pCanFrame->len
    // pCanFrame->len = dlc_to_len(pCanFrame->dlc);
    
    uiDlc = ((pCanFrame->dlc << XCANFD_DLCR_DLC_SHIFT) & XCANFD_DLCR_DLC_MASK);
    uiDlc |= XCANFD_DLCR_EDL_MASK;  // 发送CANFD帧，EDL位必须设置为1
    if (pCanFrame->brs) {   // CANFD是否进行加速
        uiDlc |= XCANFD_DLCR_BRS_MASK;
    }

    // 计算ID位中的值
    if (pCanFrame->ide) {   // 扩展帧
        uiId  = (pCanFrame->id & 0x3FFFF) << XCANFD_IDR_ID2_SHIFT;
        uiId |= ((pCanFrame->id & 0x1FFC0000) >> 18) << XCANFD_IDR_ID1_SHIFT;
        uiId |= (XCANFD_IDR_IDE_MASK);
        uiId |= (XCANFD_IDR_SRR_MASK);
    } else {    // 标准帧
        uiId  = (pCanFrame->id << XCANFD_IDR_ID1_SHIFT) & XCANFD_IDR_ID1_MASK;
    }

    // CANFD 不支持远程帧
    if (pCanFrame->rtr) {
        LOGW("canfd", "send", -1, "rtr frame not support");
        return -1;
    }
    
    // 计算空闲FIFO索引
    uiTrrVal = __axiCanfdGetFreeBuffer();
    uiValue  = (~uiTrrVal) & TRR_MASK_INIT_VAL;
    uiValue  = XCanFD_Check_TrrVal_Set_Bit(uiValue);
    uiFreeTxBuffer = __axiCanfdTrrValGetSetBitPosition(uiValue);
    if (uiFreeTxBuffer == 33) {
        LOGW("canfd", "send", -1, "tx fifo fill");
        return -1;
    } else {
        LOGI("canfd", "send", uiTrrVal, "uiFreeTxBuffer: %d", uiFreeTxBuffer);
    }

    // 将ID值和DLC值写入相应的寄存器
    wr32(XCANFD_TXID_OFFSET(uiFreeTxBuffer), uiId);
    wr32(XCANFD_TXDLC_OFFSET(uiFreeTxBuffer), uiDlc);

    // 拼接CANFD数据并写入数据寄存器 - 修复数据拷贝问题
    for (uiDw = 0; uiDw < pCanFrame->len; uiDw += 4) {
        uint32_t txData = 0;
        // 安全地拷贝数据，避免数组越界
        for (int i = 0; i < 4 && (uiDw + i) < pCanFrame->len && (uiDw + i) < pCanFrame->data.size(); i++) {
            txData |= (static_cast<uint32_t>(pCanFrame->data[uiDw + i]) << (24 - i * 8));
        }
        uiTxFrame[uiDw / 4] = txData;
        wr32((XCANFD_TXDW_OFFSET(uiFreeTxBuffer) + (uiDwIndex * XCANFD_DW_BYTES)), uiTxFrame[uiDw/4]);
        uiDwIndex ++;
    }

    rd32(XCANFD_TRR_OFFSET, uiValue);
    uiValue |= (1 << uiFreeTxBuffer);
    wr32(XCANFD_TRR_OFFSET, uiValue);   // 置位TRR代表进行发送
    return 0;
}

// CanFD接收（FIFO模式）
int CanFDDevice::__axiCanfdRecvFifo(CanFrame *pCanFrame) {
    if (pCanFrame == nullptr) {
        LOGE("canfd", "recv", -1, "pCanFrame is nullptr");
        return -1;
    }
    uint32_t uiResult    = 0;
    uint32_t uiReadIndex = 0;
    uint32_t uiValue     = 0;
    uint32_t uiDlc       = 0;
    uint32_t Len         = 0;
    uint32_t data        = 0;
    uint32_t uiDwIndex   = 0;

    rd32(XCANFD_FSR_OFFSET, uiResult);// FIFO 0 状态，只用了FIFO 0
    if (!(uiResult & XCANFD_FSR_FL_MASK)) {    // FIFO 0 没有消息
        return 0;
    }
    
    uiReadIndex = uiResult & XCANFD_FSR_RI_MASK;    // FIFO 0 消息索引
    rd32(XCANFD_RXID_OFFSET(uiReadIndex), uiValue);
    pCanFrame->ide = (uiValue & XCANFD_IDR_IDE_MASK) >> XCANFD_IDR_IDE_SHIFT;
    pCanFrame->rtr = (uiValue & XCANFD_IDR_RTR_MASK);

    if (uiValue & (XCANFD_IDR_IDE_MASK)) {
        pCanFrame->id  = (uiValue & (0x3FFFF << 1)) >> 1;
        pCanFrame->id |= ((uiValue & (0x7ff<<21)) >> 3);
    } else {
        pCanFrame->id = (uiValue >> 21);
    }

    rd32(XCANFD_RXFIFO_0_BASE_DLC_OFFSET+(uiReadIndex * XCANFD_MAX_FRAME_SIZE), uiDlc);

    pCanFrame->dlc = (uiDlc & XCANFD_DLCR_DLC_MASK) >> XCANFD_DLCR_DLC_SHIFT;
    pCanFrame->len = dlc_to_len(pCanFrame->dlc);
    if (uiDlc & XCANFD_DLCR_EDL_MASK) {
        pCanFrame->fdf = true;
    }

    if (uiDlc & XCANFD_DLCR_BRS_MASK) {
        pCanFrame->brs = true;
    }

    if (uiDlc & XCANFD_DLCR_ESI_MASK) {
        pCanFrame->esi = true;
    }

    // 确保data vector有足够的空间
    pCanFrame->data.resize(pCanFrame->len);
    
    if (uiDlc & XCANFD_DLCR_EDL_MASK){
        for (Len = 0; Len < pCanFrame->len; Len += 4) { 
            rd32(XCANFD_RXFIFO_0_BASE_DW0_OFFSET+(uiReadIndex * XCANFD_MAX_FRAME_SIZE) + (uiDwIndex * XCANFD_DW_BYTES), data);
            uiDwIndex++;
            // 安全地拷贝数据，避免数组越界
            if (Len < pCanFrame->len) pCanFrame->data[Len]     = (data >> 24) & 0xFF;
            if (Len + 1 < pCanFrame->len) pCanFrame->data[Len + 1] = (data >> 16) & 0xFF;
            if (Len + 2 < pCanFrame->len) pCanFrame->data[Len + 2] = (data >> 8)  & 0xFF;
            if (Len + 3 < pCanFrame->len) pCanFrame->data[Len + 3] = data & 0xFF;
        }
    }

    // Set the IRI bit causes core to increment RI in FSR Register
    rd32(XCANFD_FSR_OFFSET, uiResult);
    uiResult |= XCANFD_FSR_IRI_MASK;
    wr32(XCANFD_FSR_OFFSET, uiResult);
    
    return 1; // 返回接收到的帧数
}

int CanFDDevice::__axiCanfdIoctl(int iCmd, void* lArg) {
    uint8_t ucNewBrp = 0;
    uint8_t ucNewFBrp = 0;
    uint8_t ucNewTsg1 = 0, ucNewTsg2 = 0, ucSJW = 0;
    uint8_t ucNewFTsg1 = 0, ucNewFTsg2= 0, ucFSJW = 0;

    switch (iCmd) {
        case CAN_DEV_OPEN: {  // 打开 CAN 设备
            __axiCanfdHwInit();
            break;
        }

        case CAN_DEV_CLOSE: {
            wr32(XCANFD_SRR_OFFSET, XCANFD_SRR_SRST_MASK);
            break;
        }

        case CAN_DEV_REST_CONTROLLER: {
            __axiCanfdHwInit();
            break;
        }

        case CAN_DEV_SET_BAUD: {                                       // 设置仲裁段波特率 
            __axiCanfdEnterMode(XCANFD_MODE_CONFIG);            // 进入配置模式 
            while (__axiCanfdGetMode() != XCANFD_MODE_CONFIG);

            switch (*(uint32_t*)lArg) {
                case 125000: {
                    ucNewBrp = 7; ucNewTsg1 = 13; ucNewTsg2 = 4; ucSJW = 2;
                    break;
                }
                case 250000: {
                    ucNewBrp = 3; ucNewTsg1 = 13; ucNewTsg2 = 4; ucSJW = 2;
                    break;
                }
                case 500000: {
                    ucNewBrp = 1; ucNewTsg1 = 13; ucNewTsg2 = 4; ucSJW = 2;
                    break;
                }
                case 800000: {
                    ucNewBrp = 0; ucNewTsg1 = 15; ucNewTsg2 = 7; ucSJW = 3;
                    break;
                }   
                case 1000000: {
                    ucNewBrp = 1; ucNewTsg1 = 14; ucNewTsg2 = 3; ucSJW = 3;
                    break;
                }
                default: {
                    __axiCanfdEnterMode(XCANFD_MODE_NORMAL);        // 进入工作模式
                    while (__axiCanfdGetMode() != XCANFD_MODE_NORMAL);
                    return -ENOSYS;
                }
            }

            __axiCanfdSetBaudRatePrescaler(ucNewBrp);
            __axiCanfdSetBitTiming(ucSJW, ucNewTsg2, ucNewTsg1);
            __axiCanfdEnterMode(XCANFD_MODE_NORMAL);            // 进入工作模式
            while (__axiCanfdGetMode() != XCANFD_MODE_NORMAL);
            break;
        }

        case CAN_DEV_SET_DATA_BAUD: {                                   // 设置数据段波特率
            __axiCanfdEnterMode(XCANFD_MODE_CONFIG);            // 进入配置模式
            while (__axiCanfdGetMode() != XCANFD_MODE_CONFIG);

            switch (*(uint32_t*)lArg) {
                case 125000: {
                    ucNewFBrp = 7; ucNewFTsg1 = 13; ucNewFTsg2 = 4; ucFSJW = 2;
                    break;
                }
                case 250000: {
                    ucNewFBrp = 3; ucNewFTsg1 = 13; ucNewFTsg2 = 4; ucFSJW = 2;
                    break;
                }
                case 500000: {
                    ucNewFBrp = 1; ucNewFTsg1 = 13; ucNewFTsg2 = 4; ucFSJW = 2;
                    break;
                }
                case 800000: {
                    ucNewFBrp = 0; ucNewFTsg1 = 15; ucNewFTsg2 = 7; ucFSJW = 3;
                    break;
                }
                case 1000000: {
                    ucNewFBrp = 0; ucNewFTsg1 = 13; ucNewFTsg2 = 4; ucFSJW = 2;
                    break;
                }
                case 2000000: {
                    ucNewFBrp = 0; ucNewFTsg1 = 14; ucNewFTsg2 = 3; ucFSJW = 3;
                    break;
                }
                case 4000000: {
                    ucNewFBrp = 0; ucNewFTsg1 = 6; ucNewFTsg2 = 1; ucFSJW = 1;
                    break;
                }
                default: {
                    __axiCanfdEnterMode(XCANFD_MODE_NORMAL);        // 进入工作模式
                    while (__axiCanfdGetMode() != XCANFD_MODE_NORMAL);
                    return -ENOSYS;
                }
            }

            __axiCanfdSetFBaudRatePrescaler(ucNewFBrp);
            __axiCanfdSetFBitTiming(ucFSJW, ucNewFTsg2, ucNewFTsg1);
            __axiCanfdEnterMode(XCANFD_MODE_NORMAL);            // 进入工作模式
            while (__axiCanfdGetMode() != XCANFD_MODE_NORMAL);
            break;
        }

        case CAN_DEV_SET_FILTER: {                                         // 设置过滤模式
            if (lArg) {
                AXI_CANFD_FILTER* pAxiCanFilter = (AXI_CANFD_FILTER *)lArg;
                __axiCanfdSetFilter(pAxiCanFilter->uiFilterIndex,
                                    pAxiCanFilter->uiMask,
                                    pAxiCanFilter->uiId);
            } else {
                __axiCanfdAcceptFilterDisable(0);
            }
            break;
        }

        case CAN_DEV_INTE_DISABLED: {                                   // 禁用中断
            __axiCanfdInterruptEnable(0);
            break;
        }

        default: {                                                         // 默认返回错误
            return -ENOSYS;
        }
    }
    return 0;
}

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF