/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: reg_canfd.h
**
** 创   建   人: Ye.Hao (叶昊)
**
** 文件创建日期: 2023 年 07 月 24 日
**
** 描        述: PL端 寄存器地址
*********************************************************************************************************/
/*********************************************************************************************************
  FIFO和Mailbox均存在的寄存器地址
*********************************************************************************************************/
#define XCANFD_SRR_OFFSET                            0x0000             /* 软件复位寄存器               */
#define XCANFD_MSR_OFFSET                            0x0004             /* 模式选择寄存器               */
#define XCANFD_BRPR_OFFSET                           0x0008             /* 波特率预定标器注册           */
#define XCANFD_BTR_OFFSET                            0x000C             /* 定时寄存器                   */
#define XCANFD_ECR_OFFSET                            0x0010             /* 错误计数器寄存器             */
#define XCANFD_ESR_OFFSET                            0x0014             /* 错误状态寄存器               */
#define XCANFD_SR_OFFSET                             0x0018             /* 总线状态寄存器               */
#define XCANFD_ISR_OFFSET                            0x001C             /* 中断状态寄存器               */
#define XCANFD_IER_OFFSET                            0x0020             /* 中断使寄存器                 */
#define XCANFD_ICR_OFFSET                            0x0024             /* 中断清除寄存器               */
#define XCANFD_TIMESTAMPR_OFFSET                     0x0028             /* 时间戳寄存器                 */
#define XCANFD_F_BRPR_OFFSET                         0x0088             /* 数据相位波特率寄存器         */
#define XCANFD_F_BTR_OFFSET                          0x008C             /* 数据相位时序寄存器           */
#define XCANFD_TRR_OFFSET                            0x0090             /* TX FIFO 就绪请求TRR寄存器    */
#define XCANFD_IETRS_OFFSET                          0x0094             /* TX FIFO 中断使能寄存器       */
#define XCANFD_TCR_OFFSET                            0x0098             /* TX FIFO 清除请求寄存器       */
#define XCANFD_IETCS_OFFSET                          0x009C             /* TX FIFO 中断清除寄存器       */
#define XCANFD_TXE_FSR_OFFSET                        0x00A0             /* TX 事件 FIFO状态寄存器       */
#define XCANFD_TXE_FWM_OFFSET                        0x00A4             /* TX 事件 FIFO 触发偏移寄存器  */
/*********************************************************************************************************
  仅Mailbox下存在的寄存器地址
*********************************************************************************************************/
#define XCANFD_RCS0_OFFSET                           0x00B0             /* RX FIFO 控制状态0寄存器      */
#define XCANFD_RCS1_OFFSET                           0x00B4             /* RX FIFO 控制状态1寄存器      */
#define XCANFD_RCS2_OFFSET                           0x00B8             /* RX FIFO 控制状态2寄存器      */
#define XCANFD_RXBFLL1_OFFSET                        0x00C0             /* RX FIFO 完全中断使能寄存器1  */
#define XCANFD_RXBFLL2_OFFSET                        0x00C4             /* RX FIFO 完全中断使能寄存器2  */
/*********************************************************************************************************
  仅FIFO下存在的寄存器地址
*********************************************************************************************************/
#define XCANFD_AFR_OFFSET                            0x00E0             /* 接收过滤寄存器               */
#define XCANFD_FSR_OFFSET                            0x00E8             /* 接收 FIFO状态寄存器          */
#define XCANFD_WIR_OFFSET                            0x00EC             /* Rx FIFO Watermark Mark Reg   */
/*********************************************************************************************************
  0x0100 -0x09FC CAN FD TX Message Space Register Descriptions st
*********************************************************************************************************/
/*
 * 0x0100-0x0144(TB0 )
 * 0X0100 TB0-ID
 * 0x0104 TB0-DLC
 * 0x0108 TB0-DW1
 * 0x0144 TB0-DW15
 * ...
 * ...
 * 0x09B8-0x09FC(TB31)
 */
#define XCANFD_TXFIFO_0_BASE_ID_OFFSET               0x0100             /* TX FIFO 0 基地址0 ID寄存器   */
#define XCANFD_TXFIFO_0_BASE_DLC_OFFSET              0x0104             /* TX FIFO 0 基地址0 DLC寄存器  */
#define XCANFD_TXFIFO_0_BASE_DW0_OFFSET              0x0108             /* TX FIFO 0 基地址0 DW0寄存器  */
/*********************************************************************************************************
  0x0A00 -0x0AFC 32 ID Filter-Mask
*********************************************************************************************************/
/*
 * 0x0A00 (AFMR0)
 * 0x0A04 (AFIR0)
 * 0x0A08 (AFMR1)
 * 0x0A0C (AFIR1)
 * ...
 * 0x0AFC (AFIR31)
 */
#define XCANFD_AFMR_BASE_OFFSET                      0x0A00             /* 接收过滤器掩码寄存器         */
#define XCANFD_AFIDR_BASE_OFFSET                     0x0A04             /* 接收过滤器ID寄存器           */
/*********************************************************************************************************
  0x2000 -0x20FC CAN FD TX Event FIFO Register Descriptions
*********************************************************************************************************/
/*
 * 0x2000 TB0-ID              这个寄存器是只读的，0x0100 -0x09FC是填充到缓冲区要发送的数据
 * 0x2004 TB0-DLC             而这个寄存器代表发送的状态，没有DW数据内容，
 * ...                        清主意DLC中的ET\MM\Timerstamep可能是有用的。
 * 0x20F8 TB31-ID
 * 0x20FC TB31-DLC
 */
#define XCANFD_TXEFIFO_0_BASE_ID_OFFSET              0x2000             /* Tx Event FIFO 0 ID 寄存器    */
#define XCANFD_TXEFIFO_0_BASE_DLC_OFFSET             0x2004             /* Tx Event FIFO 0 DLC 寄存器   */
/*********************************************************************************************************
  0x2000 -0x0AFC 32 ID Filter-Mask
*********************************************************************************************************/

/*********************************************************************************************************
  0x000 Software Reset mask
*********************************************************************************************************/
#define XCANFD_SRR_CEN_MASK                          0x00000002         /* Can Enable Mask              */
#define XCANFD_SRR_SRST_MASK                         0x00000001         /* 软复位掩码                   */
/*********************************************************************************************************
  0x004 Mode Select mask
*********************************************************************************************************/
#define XCANFD_MSR_SLEEP_MASK                        0x00000001         /* Sleep Mode Select Mask       */
#define XCANFD_MSR_LBACK_MASK                        0x00000002         /* Loop Back Mode Select Mask   */
#define XCANFD_MSR_SNOOP_MASK                        0x00000004         /* Snoop Mode Select Mask       */
#define XCANFD_MSR_BRSD_MASK                         0x00000008         /* Bit Rate Switch Select Mask  */
#define XCANFD_MSR_DAR_MASK                          0x00000010         /* 禁用自动重传掩码             */
#define XCANFD_MSR_DPEE_MASK                         0x00000020         /* Protocol Exception Event Mask*/
#define XCANFD_MSR_SBR_MASK                          0x00000040         /* Start Bus-Off Recovery Mask  */
#define XCANFD_MSR_ABR_MASK                          0x00000080         /* Auto Bus-Off Recovery Mask   */
#define XCANFD_MSR_CONFIG_MASK                       0x000000F8         /* Configuration Mode Mask      */
/*********************************************************************************************************
  0x008 Baud Rate Prescaler mask
*********************************************************************************************************/
#define XCANFD_BRPR_BRP_MASK                         0x000000FF         /* Baud Rate Prescaler Mask     */
#define XCANFD_BTR_TS2_SHIFT                         8                  /* Time Segment 2 Shift         */
#define XCANFD_BTR_SJW_SHIFT                         16                 /* Sync Jump Width Shift        */
/*********************************************************************************************************
  0x00c Arbitration Phase Bit Register
*********************************************************************************************************/
#define XCANFD_BTR_SJW_MASK                          0x007F0000         /* Sync Jump Width Mask         */
#define XCANFD_BTR_TS2_MASK                          0x00007F00         /* Time Segment 2 Mask          */
#define XCANFD_BTR_TS1_MASK                          0x000000FF         /* Time Segment 1 Mask          */
/*********************************************************************************************************
  0x010 Error Counter Register
*********************************************************************************************************/
#define XCANFD_ECR_REC_SHIFT                         8                  /* Receive Error Counter Shift  */
#define XCANFD_ECR_REC_MASK                          0x0000FF00         /* Receive Error Counter Mask   */
#define XCANFD_ECR_TEC_MASK                          0x000000FF         /* Transmit Error Counter Mask  */
/*********************************************************************************************************
  0x014 Error Counter Register mask
*********************************************************************************************************/
#define XCANFD_ESR_CRCER_MASK                        0x00000001         /* CRC Error Mask               */
#define XCANFD_ESR_FMER_MASK                         0x00000002         /* Form Error Mask              */
#define XCANFD_ESR_STER_MASK                         0x00000004         /* Stuff Error Mask             */
#define XCANFD_ESR_BERR_MASK                         0x00000008         /* Bit Error Mask               */
#define XCANFD_ESR_ACKER_MASK                        0x00000010         /* ACK Error Mask               */

#define XCANFD_ESR_F_CRCER_MASK                      0x00000100         /* F_CRC Error Mask             */
#define XCANFD_ESR_F_FMER_MASK                       0x00000200         /* F_Form Error Mask            */
#define XCANFD_ESR_F_STER_MASK                       0x00000400         /* F_Stuff Error Mask           */
#define XCANFD_ESR_F_BERR_MASK                       0x00000800         /* F_Bit Error Mask             */
/*********************************************************************************************************
  0x018 Status Register mask
*********************************************************************************************************/
#define XCANFD_SR_ESTAT_SHIFT                        7                  /* Error Status Shift           */
#define XCANFD_SR_TDCV_MASK                          0x007F0000         /* 收发器延时补偿掩码           */
#define XCANFD_SR_SNOOP_MASK                         0x00001000         /* Snoop Mode Mask              */
#define XCANFD_SR_BSFR_CONFIG_MASK                   0x00000400         /* Bus-Off 恢复指示掩码         */
#define XCANFD_SR_PEE_CONFIG_MASK                    0x00000200         /* 协议异常模式掩码             */
#define XCANFD_SR_ESTAT_MASK                         0x00000180         /* Error Status Mask            */
#define XCANFD_SR_ERRWRN_MASK                        0x00000040         /* Error Warning Mask           */
#define XCANFD_SR_BBSY_MASK                          0x00000020         /* Bus Busy Mask                */
#define XCANFD_SR_BIDLE_MASK                         0x00000010         /* Bus Idle Mask                */
#define XCANFD_SR_NORMAL_MASK                        0x00000008         /* Normal Mode Mask             */
#define XCANFD_SR_SLEEP_MASK                         0x00000004         /* Sleep Mode Mask              */
#define XCANFD_SR_LBACK_MASK                         0x00000002         /* Loop Back Mode Mask          */
#define XCANFD_SR_CONFIG_MASK                        0x00000001         /* Configuration Mode Mask      */
/*********************************************************************************************************
  0x01c Interrupt Status/ 0x020 Enable/ 0x024 Clear Register mask
*********************************************************************************************************/
#define XCANFD_IXR_ARBLST_MASK                       0x00000001         /* 仲裁丢失中断掩码             */
#define XCANFD_IXR_TXOK_MASK                         0x00000002         /* TX 成功中断                  */
#define XCANFD_IXR_PEE_MASK                          0x00000004         /* 协议外异常                   */
#define XCANFD_IXR_BSRD_MASK                         0x00000008         /* BSOFF恢复中断                */
#define XCANFD_IXR_RXOK_MASK                         0x00000010         /* 新收到的接收消息掩码         */
#define XCANFD_IXR_TSCNT_OFLW_MASK                   0x00000020         /* 时间戳溢出中断掩码           */
#define XCANFD_IXR_RXFOFLW_MASK                      0x00000040         /* RX FIFO 溢出掩码 (fifo)      */
#define XCANFD_IXR_ERROR_MASK                        0x00000100         /* 中断错误                     */
#define XCANFD_IXR_BSOFF_MASK                        0x00000200         /* 总线关闭中断掩码             */
#define XCANFD_IXR_SLP_MASK                          0x00000400         /* 睡眠模式中断掩码             */
#define XCANFD_IXR_WKUP_MASK                         0x00000800         /* 睡眠模式唤醒中断掩码         */
#define XCANFD_IXR_RXFWMFLL_MASK                     0x00001000         /* RX 0超过设置的中断水平时产生F*/
#define XCANFD_IXR_TXRRS_MASK                        0x00002000         /* Tx FIFO准备请求服务中断掩码  */
#define XCANFD_IXR_TXCRS_MASK                        0x00004000         /* Tx取消请求完毕中断掩码 M & F */
#define XCANFD_IXR_RXFOFLW_1_MASK                    0x00008000         /* RX 溢出并且新消息丢失 F      */
#define XCANFD_IXR_RXRBF_MASK                        0x00008000         /* Rx 缓冲收一条消息且达到上限M */
#define XCANFD_IXR_RXBOFLW_MASK                      0x00010000         /* RX 溢出中断消息已经丢失 M    */
#define XCANFD_IXR_RXFWMFLL_1_MASK                   0x00010000         /* Rx 1超过设置的中断水平时产生F*/
#define XCANFD_IXR_RXMNF_MASK                        0x00020000         /* 接收未完成匹配中断掩码       */
#define XCANFD_IXR_TXEOFLW_MASK                      0x40000000         /* TX Event FIFO Intr Mask      */
#define XCANFD_IXR_TXEWMFLL_MASK                     0x80000000         /* TX EVENT FIFO满中断触发掩码  */
#define XCANFD_IXR_RXBOFLW_BI_MASK                   0x3F000000         /* Rx 溢出索引值 M              */
#define XCANFD_IXR_RXLRM_BI_MASK                     0x00FC0000         /* Rx 最后消息索引 M            */
#define XCANFD_RXLRM_BI_SHIFT                        18                 /* Rx Buffer Index Shift Value  */
/*********************************************************************************************************
  0x028 Timestamp Register
*********************************************************************************************************/
#define XCANFD_CTS_MASK                              0x00000001         /* 时间戳清除寄存器掩码         */
/*********************************************************************************************************
  0x088 Data Phase Baud Rate Prescaler
*********************************************************************************************************/
#define XCANFD_F_BRPR_TDC_ENABLE_MASK                0x00010000         /* 收发器补偿启用掩码           */
#define XCANFD_F_BRPR_TDCMASK                        0x00003F00         /* 转换器延迟补偿掩码           */
#define XCANFD_F_BTR_TS2_SHIFT                       8                  /* Time Segment 2 Shift         */
#define XCANFD_F_BTR_SJW_SHIFT                       16                 /* Sync Jump Width Shift        */
/*********************************************************************************************************
  0x08c Data Phase Bit Timing Register
*********************************************************************************************************/
#define XCANFD_F_BTR_SJW_MASK                        0x000F0000         /* Sync Jump Width Mask         */
#define XCANFD_F_BTR_TS2_MASK                        0x00000F00         /* Time Segment 2 Mask          */
#define XCANFD_F_BTR_TS1_MASK                        0x0000001F         /* Time Segment 1 Mask          */
/*********************************************************************************************************
  0x090 RR TxBuffer Ready Request Served Interrupt Enable Register Masks
*********************************************************************************************************/
/*
 * 写1表示将对应已经准备好的缓冲区的数据传输出去，
 * 以下3种情况会清0
 * 1.数据在CAN总线传输完毕后此位置会进行清零0
 * 2.ip核处于DAR模式(直接发送模式)一次传输无论是否成功会清0
 * 3.消息传输被取消则清0
 * 此寄存器可以一次写入多个值，进行多条消息的传输
 */
#define XCANFD_TXBUFFER0_RDY_RQT_MASK                0x00000001         /* TxBuffer0 Ready Request Mask */
#define XCANFD_TXBUFFER1_RDY_RQT_MASK                0x00000002         /* TxBuffer1 Ready Request Mask */
#define XCANFD_TXBUFFER2_RDY_RQT_MASK                0x00000004         /* TxBuffer2 Ready Request Mask */
#define XCANFD_TXBUFFER3_RDY_RQT_MASK                0x00000008         /* TxBuffer3 Ready Request Mask */
#define XCANFD_TXBUFFER4_RDY_RQT_MASK                0x00000010         /* TxBuffer4 Ready Request Mask */
#define XCANFD_TXBUFFER5_RDY_RQT_MASK                0x00000020         /* TxBuffer5 Ready Request Mask */
#define XCANFD_TXBUFFER6_RDY_RQT_MASK                0x00000040         /* TxBuffer6 Ready Request Mask */
#define XCANFD_TXBUFFER7_RDY_RQT_MASK                0x00000080         /* TxBuffer7 Ready Request Mask */
#define XCANFD_TXBUFFER8_RDY_RQT_MASK                0x00000100         /* TxBuffer8 Ready Request Mask */
#define XCANFD_TXBUFFER9_RDY_RQT_MASK                0x00000200         /* TxBuffer9 Ready Request Mask */
#define XCANFD_TXBUFFER10_RDY_RQT_MASK               0x00000400
#define XCANFD_TXBUFFER11_RDY_RQT_MASK               0x00000800
#define XCANFD_TXBUFFER12_RDY_RQT_MASK               0x00001000
#define XCANFD_TXBUFFER13_RDY_RQT_MASK               0x00002000
#define XCANFD_TXBUFFER14_RDY_RQT_MASK               0x00004000
#define XCANFD_TXBUFFER15_RDY_RQT_MASK               0x00008000
#define XCANFD_TXBUFFER16_RDY_RQT_MASK               0x00010000
#define XCANFD_TXBUFFER17_RDY_RQT_MASK               0x00020000
#define XCANFD_TXBUFFER18_RDY_RQT_MASK               0x00040000
#define XCANFD_TXBUFFER19_RDY_RQT_MASK               0x00080000
#define XCANFD_TXBUFFER20_RDY_RQT_MASK               0x00100000
#define XCANFD_TXBUFFER21_RDY_RQT_MASK               0x00200000
#define XCANFD_TXBUFFER22_RDY_RQT_MASK               0x00400000
#define XCANFD_TXBUFFER23_RDY_RQT_MASK               0x00800000
#define XCANFD_TXBUFFER24_RDY_RQT_MASK               0x01000000
#define XCANFD_TXBUFFER25_RDY_RQT_MASK               0x02000000
#define XCANFD_TXBUFFER26_RDY_RQT_MASK               0x04000000
#define XCANFD_TXBUFFER27_RDY_RQT_MASK               0x08000000
#define XCANFD_TXBUFFER28_RDY_RQT_MASK               0x10000000
#define XCANFD_TXBUFFER29_RDY_RQT_MASK               0x20000000
#define XCANFD_TXBUFFER30_RDY_RQT_MASK               0x40000000
#define XCANFD_TXBUFFER31_RDY_RQT_MASK               0x80000000        /* TxBuffer31 Ready Request Mask */
#define XCANFD_TXBUFFER_ALL_RDY_RQT_MASK             0xFFFFFFFF        /* Tx buf Ready Request Mask ALL */
/*********************************************************************************************************
  0x094 ERRS Interrupt Enable TX Buffer Ready Request Served/Cleared
*********************************************************************************************************/
/*
 * 发送缓冲区就绪中断使能
 * TRR(TX Ready Request)寄存器从1变为0(即出现上述的清0过程)，中断状态寄存器(ISR)中的TXRRS会变为1
 * 如果31个位都不使能,则ISR中的TXRRS不会置位
 */
/*********************************************************************************************************
  0x098 CR TX Buffer Cancel Request
*********************************************************************************************************/
/*
 * 写1表示取消对应缓冲区的发送请求
 * 如果TRR为0则会忽略当前请求
 * 如果已经进入了发送状态，则发送状态结束后会取消发送请求，无论发送是否成功，
 * 也就是说无论是错误引起的还是成功发送都会取消发送，这种情况下错误引起的数据会丢失。此时TRR也会清0
 */
#define XCANFD_TXBUFFER0_CANCEL_RQT_MASK             0x00000001         /* TxBuffer0 Cancel Request Mask*/
#define XCANFD_TXBUFFER1_CANCEL_RQT_MASK             0x00000002         /* TxBuffer1 Cancel Request Mask*/
#define XCANFD_TXBUFFER2_CANCEL_RQT_MASK             0x00000004         /* TxBuffer2 Cancel Request Mask*/
#define XCANFD_TXBUFFER3_CANCEL_RQT_MASK             0x00000008         /* TxBuffer3 Cancel Request Mask*/
#define XCANFD_TXBUFFER4_CANCEL_RQT_MASK             0x00000010         /* TxBuffer4 Cancel Request Mask*/
#define XCANFD_TXBUFFER5_CANCEL_RQT_MASK             0x00000020         /* TxBuffer5 Cancel Request Mask*/
#define XCANFD_TXBUFFER6_CANCEL_RQT_MASK             0x00000040         /* TxBuffer6 Cancel Request Mask*/
#define XCANFD_TXBUFFER7_CANCEL_RQT_MASK             0x00000080         /* TxBuffer7 Cancel Request Mask*/
#define XCANFD_TXBUFFER8_CANCEL_RQT_MASK             0x00000100
#define XCANFD_TXBUFFER9_CANCEL_RQT_MASK             0x00000200
#define XCANFD_TXBUFFER10_CANCEL_RQT_MASK            0x00000400
#define XCANFD_TXBUFFER11_CANCEL_RQT_MASK            0x00000800
#define XCANFD_TXBUFFER12_CANCEL_RQT_MASK            0x00001000
#define XCANFD_TXBUFFER13_CANCEL_RQT_MASK            0x00002000
#define XCANFD_TXBUFFER14_CANCEL_RQT_MASK            0x00004000
#define XCANFD_TXBUFFER15_CANCEL_RQT_MASK            0x00008000
#define XCANFD_TXBUFFER16_CANCEL_RQT_MASK            0x00010000
#define XCANFD_TXBUFFER17_CANCEL_RQT_MASK            0x00020000
#define XCANFD_TXBUFFER18_CANCEL_RQT_MASK            0x00040000
#define XCANFD_TXBUFFER19_CANCEL_RQT_MASK            0x00080000
#define XCANFD_TXBUFFER20_CANCEL_RQT_MASK            0x00100000
#define XCANFD_TXBUFFER21_CANCEL_RQT_MASK            0x00200000
#define XCANFD_TXBUFFER22_CANCEL_RQT_MASK            0x00400000
#define XCANFD_TXBUFFER23_CANCEL_RQT_MASK            0x00800000
#define XCANFD_TXBUFFER24_CANCEL_RQT_MASK            0x01000000
#define XCANFD_TXBUFFER25_CANCEL_RQT_MASK            0x02000000
#define XCANFD_TXBUFFER26_CANCEL_RQT_MASK            0x04000000
#define XCANFD_TXBUFFER27_CANCEL_RQT_MASK            0x08000000
#define XCANFD_TXBUFFER28_CANCEL_RQT_MASK            0x10000000
#define XCANFD_TXBUFFER29_CANCEL_RQT_MASK            0x20000000
#define XCANFD_TXBUFFER30_CANCEL_RQT_MASK            0x40000000
#define XCANFD_TXBUFFER31_CANCEL_RQT_MASK            0x80000000
#define XCANFD_TXBUFFER_CANCEL_RQT_ALL_MASK          0xFFFFFFFF         /* TxBuf Cancel Request Mask ALL*/
/*********************************************************************************************************
  0x09c ECRS TX Buffer Cancel Request
*********************************************************************************************************/
/*
 * 发送缓冲区取消请求使能
 * CR(tx buffer cancel request)寄存器从1变为0(即出现上述的清0过程)，中断状态寄存器(ISR)中的TXCRS会变为1
 * 如果31个位都不使能,则ISR中的TXCRS不会置位
 * 这里手册上的寄存器描述好像有错误
 */

/*********************************************************************************************************
  0x0A0  TX Event FIFO Status Register
*********************************************************************************************************/
#define XCANFD_TXE_FL_MASK                           0x00001F00         /* TX E F 数据深度，从RI开始计算*/
#define XCANFD_TXE_FL_SHIFT                          8                  /* 如RI=3，FI=5，2018开始5条消息*/
#define XCANFD_TXE_RI_MASK                           0x0000001F         /* TX E F 索引IRI设置此位+1     */
#define XCANFD_TXE_IRI_SHIFT                         7                  /* TX E F 每次写递增RI索引偏移  */
#define XCANFD_TXE_IRI_MASK                          0x00000080         /* TX E F 每次写递增RI索引      */
/*********************************************************************************************************
  0x0A4  TX Event FIFO Watermark Register
*********************************************************************************************************/
/*
 * 通过设置此寄存器可以控制发送TX FIFO中断深度 1-31
 * 只要TXE_FI中的值高于TXE_FWM中的值，ISR中的TXEWMFLL将一直产生中断
 */
#define XCANFD_TXE_FWM_MASK                          0x0000001F         /* TX Event FIFO watermark Mask */
/*********************************************************************************************************
  0x0B0 RX Buffer Control Status Register 0 (0-15)(M?)
*********************************************************************************************************/
#define XCANFD_RCS_HCB_MASK                          0xFFFF             /* RX FIFO 主机控制位掩码       */
#define XCANFD_CSB_SHIFT                             16                 /* Core Status Bit Shift Value  */
#define XCANFD_MAILBOX_RB_MASK_BASE_OFFSET           0x2F00             /* 邮箱RX FIFO 掩码寄存器       */
#define XCANFD_MAILBOX_NXT_RB                        4                  /* 邮箱下一个缓冲区             */
#define XCANFD_MBRXBUF_MASK                          0x0000FFFF         /* 邮箱最大Rx缓冲区掩码         */
/*********************************************************************************************************
  0x0B4 RX Buffer Control Status Register 1 (16-31)(M)
*********************************************************************************************************/

/*********************************************************************************************************
  0x0B8 RX Buffer Control Status Register 2 (32-48)(M)
*********************************************************************************************************/

/*********************************************************************************************************
  0x0C0 Interrupt Enable RX Buffer Full Register 0 (0-31)(M)
*********************************************************************************************************/

/*********************************************************************************************************
  0x0C4 Interrupt Enable RX Buffer Full Register 1 (32-48)(M)
*********************************************************************************************************/

/*********************************************************************************************************
  0x0E0 Acceptance Filter (Control) Register (F)
*********************************************************************************************************/
#define XCANFD_AFR_UAF_ALL_MASK                      0xFFFFFFFF         /* Acceptance Filter Register   */
/*********************************************************************************************************
  0x0E8 RX FIFO Status Register (F)
*********************************************************************************************************/
#define XCANFD_FSR_FL_1_MASK                         0x7F000000         /* Fill Level Mask FIFO 1       */
#define XCANFD_FSR_IRI_1_MASK                        0x00800000         /* Increment Read Index Mask FIFO1 */
#define XCANFD_FSR_RI_1_MASK                         0x003F0000         /* Read Index Mask FIFO 1       */
#define XCANFD_FSR_FL_MASK                           0x00007F00         /* Fill Level Mask FIFO 0       */
#define XCANFD_FSR_IRI_MASK                          0x00000080         /* Increment Read Index Mask    */
#define XCANFD_FSR_RI_MASK                           0x0000003F         /* Read Index Mask FIFO 0       */
#define XCANFD_FSR_FL_0_SHIFT                        8                  /* Fill Level Mask FIFO 0       */
#define XCANFD_FSR_FL_1_SHIFT                        24                 /* Fill Level Mask FIFO 1       */
#define XCANFD_FSR_RI_1_SHIFT                        16                 /* Read Index Mask FIFO 1       */
/*********************************************************************************************************
  0x0EC RX FIFO Watermark Register (F)
*********************************************************************************************************/
#define XCANFD_WMR_RXFWM_MASK                        0x0000003F         /* RX FIFO0 Full Watermark Mask */
#define XCANFD_WMR_RXFWM_1_MASK                      0x00003F00         /* RX FIFO1 Full Watermark Mask */
#define XCANFD_WMR_RXFWM_1_SHIFT                     8                  /* RX FIFO1 满标志位偏移        */
#define XCANFD_WMR_RXFP_MASK                         0x001F0000         /* RX 过滤掩码位                */
#define XCANFD_WMR_RXFP_SHIFT                        16                 /* RX 过滤偏移                  */
#define XCANFD_TXFIFO_ID_OFFSET                      0x030              /* TX FIFO ID                   */
#define XCANFD_TXFIFO_DLC_OFFSET                     0x034              /* TX FIFO DLC                  */
#define XCANFD_TXFIFO_DW1_OFFSET                     0x038              /* TX FIFO 数据字 1             */
/*********************************************************************************************************
  TxBuffer Element ID Registers，Tx Message Buffer Element Address 0x0100 - 0x09FF(2304 Bytes)
*********************************************************************************************************/
#define XCANFD_DLCR_TIMESTAMP_MASK                   0x0000FFFF         /* Dlc寄存器时间戳掩码          */
#define XCANFD_RXFIFO_0_BASE_ID_OFFSET               0x2100             /* RX FIFO 0 基地址0 ID寄存器   */
#define XCANFD_RXFIFO_0_BASE_DLC_OFFSET              0x2104             /* RX FIFO 0 基地址0 DLC寄存器  */
#define XCANFD_RXFIFO_0_BASE_DW0_OFFSET              0x2108             /* RX FIFO 0 基地址0 DW0寄存器  */
#define XCANFD_RXFIFO_1_BUFFER_0_BASE_ID_OFFSET      0x4100             /* RX FIFO 1 基地址0 ID寄存器   */
#define XCANFD_RXFIFO_1_BUFFER_0_BASE_DLC_OFFSET     0x4104             /* RX FIFO 1 基地址0 DLC寄存器  */
#define XCANFD_RXFIFO_1_BUFFER_0_BASE_DW0_OFFSET     0x4108             /* RX FIFO 1 基地址0 DW0寄存器  */
/*********************************************************************************************************
  Rx Message Buffer Element ID,DLC,DW Sizes
*********************************************************************************************************/
#define XCANFD_RXFIFO_NEXTID_OFFSET                  72                 /* Rx FIFO ID 地址偏移          */
#define XCANFD_RXFIFO_NEXTDLC_OFFSET                 72                 /* Rx FIFO DLC 地址偏移         */
#define XCANFD_RXFIFO_NEXTDW_OFFSET                  72                 /* Rx FIFO DW0 地址偏移         */
/*********************************************************************************************************
  EDL | BRS | ESI Masks.
*********************************************************************************************************/
#define XCANFD_DLCR_EDL_MASK                         0x08000000         /* EDL Mask in DLC              */
#define XCANFD_DLCR_BRS_MASK                         0x04000000         /* BRS Mask in DLC              */
#define XCANFD_DLCR_ESI_MASK                         0x02000000         /* ESI Mask in DLC              */
/*********************************************************************************************************
  Acceptance Filter Mask Registers
*********************************************************************************************************/
#define XCANFD_AFMR_NXT_OFFSET                       8                  /* AFMR 接收滤波偏移            */
#define XCANFD_AFIDR_NXT_OFFSET                      8                  /* AFIDR 接收滤波ID偏移         */
#define XCANFD_NOOF_AFR                              32                 /* 接收滤波寄存器数量           */
#define XCANFD_WIR_MASK                              0x0000003F         /* Rx FIFO Full watermark Mask  */
#define XCANFD_WM_FIFO0_THRESHOLD                    63                 /* Watermark Threshold Value    */
#define XCANFD_TXEVENT_WIR_OFFSET                    0x000000A4         /* TX FIFO Watermark Offset     */
#define XCANFD_TXEVENT_WIR_MASK                      0x0F               /* TX Event Watermark Mask      */
#define XCANFD_DAR_MASK                              0x00000010         /* 禁能自动重传掩码             */
/*********************************************************************************************************
  CAN Frame Identifier (TX High Priority Buffer/TX/RX/Acceptance Filter Mask/Acceptance Filter ID)
*********************************************************************************************************/
#define XCANFD_IDR_ID1_MASK                          0xFFE00000         /* Standard Messg Ident Mask    */
#define XCANFD_IDR_ID1_SHIFT                         21                 /* Standard Messg Ident Shift   */
#define XCANFD_IDR_SRR_MASK                          0x00100000         /* Substitute Remote TX Req     */
#define XCANFD_IDR_SRR_SHIFT                         20                 /* Substitue Remote TX Shift    */
#define XCANFD_IDR_IDE_MASK                          0x00080000         /* Identifier Extension Mask    */
#define XCANFD_IDR_IDE_SHIFT                         19                 /* Identifier Extension Shift   */
#define XCANFD_IDR_ID2_MASK                          0x0007FFFE         /* Extended Message Ident Mask  */
#define XCANFD_IDR_ID2_SHIFT                         1                  /* Extended Message Ident Shift */
#define XCANFD_IDR_RTR_MASK                          0x00000001         /* Remote TX Request Mask       */
/*********************************************************************************************************
  CAN Frame Data Length Code (TX High Priority Buffer/TX/RX)
*********************************************************************************************************/
#define XCANFD_DLCR_DLC_MASK                         0xF0000000         /* Data Length Code Mask        */
#define XCANFD_DLCR_DLC_SHIFT                        28                 /* Data Length Code Shift       */
#define XCANFD_DLCR_MM_MASK                          0x00FF0000         /* Message Marker Mask          */
#define XCANFD_DLCR_MM_SHIFT                         16                 /* Message Marker Shift         */
#define XCANFD_DLCR_EFC_MASK                         0x01000000         /* Event FIFO Control Mask      */
#define XCANFD_DLCR_EFC_SHIFT                        24                 /* Event FIFO Control Shift     */
#define XCANFD_DLC1                                  0x10000000         /* Data Length Code 1           */
#define XCANFD_DLC2                                  0x20000000         /* Data Length Code 2           */
#define XCANFD_DLC3                                  0x30000000         /* Data Length Code 3           */
#define XCANFD_DLC4                                  0x40000000         /* Data Length Code 4           */
#define XCANFD_DLC5                                  0x50000000         /* Data Length Code 5           */
#define XCANFD_DLC6                                  0x60000000         /* Data Length Code 6           */
#define XCANFD_DLC7                                  0x70000000         /* Data Length Code 7           */
#define XCANFD_DLC8                                  0x80000000         /* Data Length Code 8           */
#define XCANFD_DLC9                                  0x90000000         /* Data Length Code 9           */
#define XCANFD_DLC10                                 0xA0000000         /* Data Length Code 10          */
#define XCANFD_DLC11                                 0xB0000000         /* Data Length Code 11          */
#define XCANFD_DLC12                                 0xC0000000         /* Data Length Code 12          */
#define XCANFD_DLC13                                 0xD0000000         /* Data Length Code 13          */
#define XCANFD_DLC14                                 0xE0000000         /* Data Length Code 14          */
#define XCANFD_DLC15                                 0xF0000000         /* Data Length Code 15          */
/*********************************************************************************************************
  CAN RxBuffer Full Register
*********************************************************************************************************/
#define XCANFD_RXBUFFER0_FULL_MASK                   0x00000001         /* RxBuffer0 Full Mask          */
#define XCANFD_RXBUFFER1_FULL_MASK                   0x00000002         /* RxBuffer1 Full Mask          */
#define XCANFD_RXBUFFER2_FULL_MASK                   0x00000004         /* RxBuffer2 Full Mask          */
#define XCANFD_RXBUFFER3_FULL_MASK                   0x00000008
#define XCANFD_RXBUFFER4_FULL_MASK                   0x00000010
#define XCANFD_RXBUFFER5_FULL_MASK                   0x00000020
#define XCANFD_RXBUFFER6_FULL_MASK                   0x00000040
#define XCANFD_RXBUFFER7_FULL_MASK                   0x00000080
#define XCANFD_RXBUFFER8_FULL_MASK                   0x00000100
#define XCANFD_RXBUFFER9_FULL_MASK                   0x00000200
#define XCANFD_RXBUFFER10_FULL_MASK                  0x00000400
#define XCANFD_RXBUFFER11_FULL_MASK                  0x00000800
#define XCANFD_RXBUFFER12_FULL_MASK                  0x00001000
#define XCANFD_RXBUFFER13_FULL_MASK                  0x00002000
#define XCANFD_RXBUFFER14_FULL_MASK                  0x00004000
#define XCANFD_RXBUFFER15_FULL_MASK                  0x00008000
#define XCANFD_RXBUFFER16_FULL_MASK                  0x00010000
#define XCANFD_RXBUFFER17_FULL_MASK                  0x00020000
#define XCANFD_RXBUFFER18_FULL_MASK                  0x00040000
#define XCANFD_RXBUFFER19_FULL_MASK                  0x00080000
#define XCANFD_RXBUFFER20_FULL_MASK                  0x00100000
#define XCANFD_RXBUFFER21_FULL_MASK                  0x00200000
#define XCANFD_RXBUFFER22_FULL_MASK                  0x00400000
#define XCANFD_RXBUFFER23_FULL_MASK                  0x00800000
#define XCANFD_RXBUFFER24_FULL_MASK                  0x01000000
#define XCANFD_RXBUFFER25_FULL_MASK                  0x02000000
#define XCANFD_RXBUFFER26_FULL_MASK                  0x04000000
#define XCANFD_RXBUFFER27_FULL_MASK                  0x08000000
#define XCANFD_RXBUFFER28_FULL_MASK                  0x10000000
#define XCANFD_RXBUFFER29_FULL_MASK                  0x20000000
#define XCANFD_RXBUFFER30_FULL_MASK                  0x40000000
#define XCANFD_RXBUFFER31_FULL_MASK                  0x80000000
#define XCANFD_RXBUFFER32_FULL_MASK                  0x00000001
#define XCANFD_RXBUFFER33_FULL_MASK                  0x00000002
#define XCANFD_RXBUFFER34_FULL_MASK                  0x00000004
#define XCANFD_RXBUFFER35_FULL_MASK                  0x00000008
#define XCANFD_RXBUFFER36_FULL_MASK                  0x00000010
#define XCANFD_RXBUFFER37_FULL_MASK                  0x00000020
#define XCANFD_RXBUFFER38_FULL_MASK                  0x00000040
#define XCANFD_RXBUFFER39_FULL_MASK                  0x00000080
#define XCANFD_RXBUFFER40_FULL_MASK                  0x00000100
#define XCANFD_RXBUFFER41_FULL_MASK                  0x00000200
#define XCANFD_RXBUFFER42_FULL_MASK                  0x00000400
#define XCANFD_RXBUFFER43_FULL_MASK                  0x00000800
#define XCANFD_RXBUFFER44_FULL_MASK                  0x00001000
#define XCANFD_RXBUFFER45_FULL_MASK                  0x00002000
#define XCANFD_RXBUFFER46_FULL_MASK                  0x00004000
#define XCANFD_RXBUFFER47_FULL_MASK                  0x00008000         /* RxBuffer47 Full Mask         */
/*********************************************************************************************************
  name CAN frame length constants
*********************************************************************************************************/
#define XCANFD_MAX_FRAME_SIZE                        72                 /* Maximum CAN frame length in  */
#define XCANFD_TXE_MESSAGE_SIZE                      8                  /* TX Message Size              */
#define XCANFD_DW_BYTES                              4                  /* Data Word Bytes              */
#define XST_NOBUFFER                                 33                 /* All Buffers (32) are filled  */
#define XST_BUFFER_ALREADY_FILLED                    34                 /* Given Buffer is Already filled*/
#define TRR_POS_MASK                                 0x1                /* TRR Position Mask            */
#define MAX_BUFFER_VAL                               32                 /* Max Buffer Value             */
#define FAST_MATH_MASK1                              0xDB6DB6DB         /* Fast Math Mask 1             */
#define FAST_MATH_MASK2                              0x49249249         /* Fast Math Mask 2             */
#define FAST_MATH_MASK3                              0xC71C71C7         /* Fast Math Mask 3             */
#define TRR_INIT_VAL                                 0x00000000         /* TRR Initial value            */
#define TRR_MASK_INIT_VAL                            0xFFFFFFFF         /* TRR Mask Initial value       */
#define DESIGN_RANGE_1                               15                 /* Design Range 1               */
#define DESIGN_RANGE_2                               31                 /* Design Range 2               */
#define CONTROL_STATUS_1                             0                  /* Control Status 1             */
#define CONTROL_STATUS_2                             1                  /* Control Status 2             */
#define CONTROL_STATUS_3                             2                  /* Congrol Status 3             */
#define EXTRACTION_MASK                              63                 /* Extraction Mask              */
#define SHIFT1                                       1                  /* Flag for Shift 1             */
#define SHIFT2                                       2                  /* Flag for Shift 2             */
#define SHIFT3                                       3                  /* Flag for Shift 3             */
#define TDC_MAX_OFFSET                               32                 /* TDC Max Offset               */
#define TDC_SHIFT                                    8                  /* Shift Value for TDC          */
#define MAX_BUFFER_INDEX                             32                 /* Max Buffer Index             */
#define MIN_FILTER_INDEX                             0                  /* Minimum Filter Index         */
#define MAX_FILTER_INDEX                             32                 /* Maximum Filter Index         */
#define EDL_CANFD                                    1                  /* CANFD 扩展帧                 */
#define EDL_CAN                                      0                  /* Extended Data Length for CAN */
/*********************************************************************************************************
  CAN normal Bit rate fields
*********************************************************************************************************/
#define XCANFD_MAX_SJW_VALUE                   0x80                     /* Synchronization Jump Width   */
#define XCANFD_MAX_TS1_VALUE                   0x100                    /* Time Segment 1               */
#define XCANFD_MAX_TS2_VALUE                   0x80                     /* Time Segment 2               */
/*********************************************************************************************************
  CAN Fast Bit rate fields
*********************************************************************************************************/
#define XCANFD_MAX_F_SJW_VALUE                 0x10                     /* Sync Jump  for Data Phase    */
#define XCANFD_MAX_F_TS1_VALUE                 0x20                     /* Time Segment1 for Data Phase */
#define XCANFD_MAX_F_TS2_VALUE                 0x10                     /* Time Segment2 for Data Phase */
/*********************************************************************************************************
  CAN operation modes
*********************************************************************************************************/
#define XCANFD_MODE_CONFIG                     0x00000001               /* Configuration mode           */
#define XCANFD_MODE_NORMAL                     0x00000002               /* Normal mode                  */
#define XCANFD_MODE_LOOPBACK                   0x00000004               /* Loop Back mode               */
#define XCANFD_MODE_SLEEP                      0x00000008               /* Sleep mode                   */
#define XCANFD_MODE_SNOOP                      0x00000010               /* Snoop mode                   */
#define XCANFD_MODE_ABR                        0x00000020               /* Auto Bus-Off Recovery        */
#define XCANFD_MODE_SBR                        0x00000040               /* Starut Bus-Off Recovery      */
#define XCANFD_MODE_PEE                        0x00000080               /* Protocol Exception mode      */
#define XCANFD_MODE_DAR                        0x0000000A               /* Disable Auto Retransmission mode */
#define XCANFD_MODE_BR                         0x0000000B               /* Bus-Off Recovery Mode        */
#define XCANFD_RX_FIFO_0                       0                        /* 选择 for RX Fifo 0           */
#define XCANFD_RX_FIFO_1                       1                        /* 选择 for RX Fifo 1           */
/*********************************************************************************************************
  寄存器配置掩码
*********************************************************************************************************/
#define XCANFD_SRR_SRST_MASK                   0x00000001               /* 重置掩码                     */
#define XCANFD_SR_CONFIG_MASK                  0x00000001               /* 状态寄存器config 模式掩码    */
#define XCANFD_SR_SLEEP_MASK                   0x00000004               /* 状态寄存器Sleep 模式掩码     */
#define XCANFD_SR_NORMAL_MASK                  0x00000008               /* 状态寄存器Normal 模式掩码    */
#define XCANFD_SR_SNOOP_MASK                   0x00001000               /* 状态寄存器Snoop 模式掩码     */
#define XCANFD_SR_PEE_CONFIG_MASK              0x00000200               /* 状态寄存器协议外模式掩码     */
#define XCANFD_MSR_CONFIG_MASK                 0x000000F8               /* 模式设置掩码                 */
/*********************************************************************************************************
  AFMR偏移宏定义
*********************************************************************************************************/
#define XCANFD_AFMR_OFFSET(FilterIndex) \
    (XCANFD_AFMR_BASE_OFFSET + ((uint32_t)FilterIndex * 8))

#define XCANFD_AFIDR_OFFSET(FilterIndex)    \
    (XCANFD_AFIDR_BASE_OFFSET + ((uint32_t)FilterIndex * 8))

#define XCANFD_RCS_OFFSET(NoCtrlStatus) \
    (XCANFD_RCS0_OFFSET + ((uint32_t)NoCtrlStatus * 4))                     /* 返回RCS寄存器偏移量          */
/*********************************************************************************************************
  数据偏移计算宏定义
*********************************************************************************************************/
#define XCANFD_RXID_OFFSET(ReadIndex) \
        (XCANFD_RXFIFO_0_BASE_ID_OFFSET + \
        ((uint32_t)ReadIndex * XCANFD_MAX_FRAME_SIZE))                      /* 返回RX BUFF ID偏移量         */

#define XCANFD_RXDLC_OFFSET(ReadIndex) \
        (XCANFD_RXFIFO_0_BASE_DLC_OFFSET + \
        ((uint32_t)ReadIndex * XCANFD_MAX_FRAME_SIZE))                      /* 返回RX BUFF DLC偏移量        */

#define XCANFD_RXDW_OFFSET(ReadIndex) \
        (XCANFD_RXFIFO_0_BASE_DW0_OFFSET + \
        ((uint32_t)ReadIndex * XCANFD_MAX_FRAME_SIZE))                      /* 返回RX BUFF DW偏移量         */

#define XCANFD_FIFO_1_RXDW_OFFSET(ReadIndex) \
        (XCANFD_RXFIFO_1_BUFFER_0_BASE_DW0_OFFSET + \
        ((uint32_t)ReadIndex * XCANFD_MAX_FRAME_SIZE))                      /* 返回FIFO 1的RX BUFF DW偏移量 */

#define XCANFD_FIFO_1_RXID_OFFSET(ReadIndex) \
        (XCANFD_RXFIFO_1_BUFFER_0_BASE_ID_OFFSET + \
        ((uint32_t)ReadIndex * XCANFD_MAX_FRAME_SIZE))                      /* 返回FIFO 1的RX BUFF ID偏移量 */

#define XCANFD_FIFO_1_RXDLC_OFFSET(ReadIndex) \
        (XCANFD_RXFIFO_1_BUFFER_0_BASE_DLC_OFFSET + \
        ((uint32_t)ReadIndex * XCANFD_MAX_FRAME_SIZE))                      /* 返回FIFO 1的RX BUFF DLC偏移量 */ 

#define XCANFD_TXID_OFFSET(FreeTxBuffer) \
        (XCANFD_TXFIFO_0_BASE_ID_OFFSET + \
        ((uint32_t)FreeTxBuffer * XCANFD_MAX_FRAME_SIZE))

#define XCANFD_TXDLC_OFFSET(FreeTxBuffer) \
        (XCANFD_TXFIFO_0_BASE_DLC_OFFSET + \
        ((uint32_t)FreeTxBuffer * XCANFD_MAX_FRAME_SIZE))

#define XCANFD_TXDW_OFFSET(FreeTxBuffer) \
        (XCANFD_TXFIFO_0_BASE_DW0_OFFSET + \
        ((uint32_t)FreeTxBuffer * XCANFD_MAX_FRAME_SIZE))

#define XCanFD_Check_TrrVal_Set_Bit(Var) \
        ((Var) & (~(Var) + (uint32_t)1))

#define XCanFd_IsBufferTransmitted(pCanfdChan,TxBuffer)    \
        ((AXI_CANFD_READ(pCanfdChan, XCANFD_TRR_OFFSET) & (1 << TxBuffer)) ? FALSE : TRUE)

#define XCANFD_IXR_ALL      (XCANFD_IXR_PEE_MASK         | \
                             XCANFD_IXR_BSRD_MASK        | \
                             XCANFD_IXR_RXMNF_MASK       | \
                             XCANFD_IXR_RXBOFLW_MASK     | \
                             XCANFD_IXR_RXRBF_MASK       | \
                             XCANFD_IXR_TXCRS_MASK       | \
                             XCANFD_IXR_TXRRS_MASK       | \
                             XCANFD_IXR_RXFWMFLL_MASK    | \
                             XCANFD_IXR_WKUP_MASK        | \
                             XCANFD_IXR_SLP_MASK         | \
                             XCANFD_IXR_BSOFF_MASK       | \
                             XCANFD_IXR_ERROR_MASK       | \
                             XCANFD_IXR_RXFOFLW_MASK     | \
                             XCANFD_IXR_RXOK_MASK        | \
                             XCANFD_IXR_TXOK_MASK        | \
                             XCANFD_IXR_ARBLST_MASK      | \
                             XCANFD_IXR_TSCNT_OFLW_MASK  | \
                             XCANFD_IXR_RXFOFLW_1_MASK   | \
                             XCANFD_IXR_RXFWMFLL_1_MASK  | \
                             XCANFD_IXR_TXEOFLW_MASK     | \
                             XCANFD_IXR_TXEWMFLL_MASK)

#define XCanFd_CreateIdValue(StandardId, SubRemoteTransReq, IdExtension, \
        ExtendedId, RemoteTransReq) \
        (((((uint64_t)StandardId) << XCANFD_IDR_ID1_SHIFT) & XCANFD_IDR_ID1_MASK) | \
        (((SubRemoteTransReq) << XCANFD_IDR_SRR_SHIFT) & XCANFD_IDR_SRR_MASK) | \
        (((IdExtension) << XCANFD_IDR_IDE_SHIFT) & XCANFD_IDR_IDE_MASK) | \
        (((ExtendedId) << XCANFD_IDR_ID2_SHIFT) & XCANFD_IDR_ID2_MASK) | \
        ((RemoteTransReq) & XCANFD_IDR_RTR_MASK))
#define XCAN_FRAME_ID_OFFSET(frame_base)    ((frame_base) + 0x00)
#define XCAN_FRAME_DLC_OFFSET(frame_base)   ((frame_base) + 0x04)
#define XCANFD_FRAME_DW_OFFSET(frame_base)  ((frame_base) + 0x08)

#define CAN_DEV_OPEN                    201              /*  CAN 设备打开命令            */
#define CAN_DEV_CLOSE                   202              /*  CAN 设备关闭命令            */
#define CAN_DEV_GET_BUS_STATE           203              /*  获取 CAN 控制器状态         */
#define CAN_DEV_REST_CONTROLLER         205              /*  复位 CAN 控制器             */
#define CAN_DEV_SET_BAUD                206              /*  设置 CAN 波特率             */
#define CAN_DEV_SET_FLITER              207              /*  设置 CAN 滤波器 (暂不支持)  */
#define CAN_DEV_STARTUP                 208              /*  启动 CAN 控制器             */
#define CAN_DEV_SET_MODE                209              /*  0: BASIC CAN 1: PELI CAN    */
#define CAN_DEV_LISTEN_ONLY             210              /*  设置只听模式                */
#define CAN_DEV_SET_DATA_BAUD           214              /* 设置 CAN FD 数据段波特率     */
#define CAN_DEV_GET_STATS               215              /* 获取 CAN FD 统计信息         */
#define CAN_DEV_INTE_DISABLED           216              /* 禁用中断这个实现并不合理     */
#define CAN_DEV_SET_SIG                 217              /* 设置 CAN FD 给应用层传输信号 */

typedef struct  axi_canfd_filter {
    unsigned int uiFilterIndex;
    unsigned int uiMask;
    unsigned int uiId;
} AXI_CANFD_FILTER;