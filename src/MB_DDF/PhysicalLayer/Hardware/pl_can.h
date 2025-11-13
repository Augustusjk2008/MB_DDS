#pragma once

// 说明：
// - 基于 Xilinx AXI CAN (v1.03.a) 文档（docs/can/can.md）提取的寄存器偏移与位定义。
// - 本工程中的硬件实现采用“低位表示高位标识”的映射（参考 pl_canfd.h 与 TestPhysicalLayer.cpp）。
// - 因此 SRR/MSR/SR/ISR 等寄存器的掩码按实际可用位编码：SRST=0x1、CEN=0x2、LBACK/SLEEP/NORMAL/CONFIG 等同样如此。

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

/*********************************************************************************************************
  控制与状态寄存器偏移
*********************************************************************************************************/
#define XCAN_SRR_OFFSET            0x000   /* Software Reset Register        */
#define XCAN_MSR_OFFSET            0x004   /* Mode Select Register           */
#define XCAN_BRPR_OFFSET           0x008   /* Baud Rate Prescaler Register   */
#define XCAN_BTR_OFFSET            0x00C   /* Bit Timing Register            */
#define XCAN_ECR_OFFSET            0x010   /* Error Counter Register         */
#define XCAN_ESR_OFFSET            0x014   /* Error Status Register          */
#define XCAN_SR_OFFSET             0x018   /* Status Register                */
#define XCAN_ISR_OFFSET            0x01C   /* Interrupt Status Register      */
#define XCAN_IER_OFFSET            0x020   /* Interrupt Enable Register      */
#define XCAN_ICR_OFFSET            0x024   /* Interrupt Clear Register       */

/*********************************************************************************************************
  TX/RX FIFO 与验收滤波器寄存器偏移（Table 6）
*********************************************************************************************************/
// TX FIFO 写序：ID -> DLC -> DW1 -> DW2
#define XCAN_TX_ID_OFFSET          0x030
#define XCAN_TX_DLC_OFFSET         0x034
#define XCAN_TX_DW1_OFFSET         0x038
#define XCAN_TX_DW2_OFFSET         0x03C

// RX FIFO 读序：ID -> DLC -> DW1 -> DW2
#define XCAN_RX_ID_OFFSET          0x050
#define XCAN_RX_DLC_OFFSET         0x054
#define XCAN_RX_DW1_OFFSET         0x058
#define XCAN_RX_DW2_OFFSET         0x05C

// 验收滤波器控制与四对 Mask/ID 寄存器
#define XCAN_AFR_OFFSET            0x060   /* Acceptance Filter Register     */
#define XCAN_AFMR1_OFFSET          0x064   /* Acceptance Filter Mask Reg 1   */
#define XCAN_AFIR1_OFFSET          0x068   /* Acceptance Filter ID Reg 1     */
#define XCAN_AFMR2_OFFSET          0x06C   /* Acceptance Filter Mask Reg 2   */
#define XCAN_AFIR2_OFFSET          0x070   /* Acceptance Filter ID Reg 2     */
#define XCAN_AFMR3_OFFSET          0x074   /* Acceptance Filter Mask Reg 3   */
#define XCAN_AFIR3_OFFSET          0x078   /* Acceptance Filter ID Reg 3     */
#define XCAN_AFMR4_OFFSET          0x07C   /* Acceptance Filter Mask Reg 4   */
#define XCAN_AFIR4_OFFSET          0x080   /* Acceptance Filter ID Reg 4     */

/*********************************************************************************************************
  SRR 软件复位/使能位（按工程实际低位编码）
*********************************************************************************************************/
#define XCAN_SRR_SRST_MASK         0x00000001  /* 写1触发复位；自动清0 */
#define XCAN_SRR_CEN_MASK          0x00000002  /* CAN 核心使能 */

/*********************************************************************************************************
  MSR 模式选择位（按工程实际低位编码）
*********************************************************************************************************/
#define XCAN_MSR_SLEEP_MASK        0x00000001  /* Sleep 模式 */
#define XCAN_MSR_LBACK_MASK        0x00000002  /* Loop Back 模式 */
/* 其他模式位可按需扩展 */

/*********************************************************************************************************
  BRPR/BTR 位字段（编码直接写入寄存器值）
*********************************************************************************************************/
#define XCAN_BRPR_BRP_MASK         0x000000FF  /* 预分频 BRP */
#define XCAN_BTR_TS1_MASK          0x000000FF  /* TSEG1（TS1） */
#define XCAN_BTR_TS2_MASK          0x000003FF  /* TSEG2（TS2）在 Test 中用值 0x02（按实际编码写入）*/
#define XCAN_BTR_SJW_MASK          0x000000FF  /* SJW（同步跳宽） */

// 约定：本工程直接按 docs/can/can.md 的示例写入组合值，例如 BTR=0x000001C7（TS1=7、TS2=2、SJW=0）

/*********************************************************************************************************
  ECR 错误计数器位字段（按工程实际编码）
*********************************************************************************************************/
#define XCAN_ECR_TEC_MASK          0x000000FF  /* Transmit Error Counter 低8位 */
#define XCAN_ECR_REC_MASK          0x0000FF00  /* Receive  Error Counter 高8位 */
#define XCAN_ECR_REC_SHIFT         8

/*********************************************************************************************************
  SR 状态位（按工程实际低位编码）
*********************************************************************************************************/
#define XCAN_SR_CONFIG_MASK        0x00000001  /* 配置模式 */
#define XCAN_SR_LBACK_MASK         0x00000002  /* 回环模式 */
#define XCAN_SR_SLEEP_MASK         0x00000004  /* 睡眠模式 */
#define XCAN_SR_NORMAL_MASK        0x00000008  /* 正常模式 */
#define XCAN_SR_BIDLE_MASK         0x00000010  /* 总线空闲 */
#define XCAN_SR_BBSY_MASK          0x00000020  /* 总线忙 */
#define XCAN_SR_ERRWRN_MASK        0x00000040  /* 错误警告 */
#define XCAN_SR_ESTAT_MASK         0x00000180  /* 错误状态（两位） */
#define XCAN_SR_ACFBSY_MASK        0x00000800  /* 验收滤波器忙（Test 使用 0x00000800） */

/*********************************************************************************************************
  ISR/IER/ICR 位（按工程实际低位编码）
*********************************************************************************************************/
#define XCAN_ISR_TXOK_MASK         0x00000002  /* 发送完成 */
#define XCAN_ISR_RXOK_MASK         0x00000010  /* 接收完成 */
/* IER/ICR 同位定义，用于使能与清除 */
#define XCAN_IER_TXOK_MASK         XCAN_ISR_TXOK_MASK
#define XCAN_IER_RXOK_MASK         XCAN_ISR_RXOK_MASK
#define XCAN_ICR_TXOK_MASK         XCAN_ISR_TXOK_MASK
#define XCAN_ICR_RXOK_MASK         XCAN_ISR_RXOK_MASK

/*********************************************************************************************************
  AFR 验收滤波器使能位（UAF1..UAF4）
*********************************************************************************************************/
#define XCAN_AFR_UAF1_MASK         0x00000001
#define XCAN_AFR_UAF2_MASK         0x00000002
#define XCAN_AFR_UAF3_MASK         0x00000004
#define XCAN_AFR_UAF4_MASK         0x00000008

/*********************************************************************************************************
  ID/DLC 编码辅助（按工程实际编码）
*********************************************************************************************************/
#define XCAN_ID_STD_SHIFT          21           /* 标准11位ID位移（TX/RX 一致） */
#define XCAN_ID_STD_MASK           0x7FF        /* 11-bit */
#define XCAN_DLC_SHIFT             28           /* DLC 位移（4位） */
#define XCAN_DLC_MASK              0x0F

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF