LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) DS791 June
22, 2011 ProductSpecification Introduction LogiCORE IP Facts Table The
Xilinx® LogiCORE™ IP Controller Area Network Core Specifics (CAN)
product specification defines the architecture Virtex-7 Kintex™-7 and
features of the Xilinx CAN controller core. This Supported Artix™-7
document also defines the addressing and functionality Device Family(1)
Zynq™-7000 Virtex-6 of the various registers in the design, in addition
to Spartan-6/XA describing the user interface. The scope of this
Supported User document does not extend to describing the CAN AXI4-Lite
Interfaces protocol and assumes knowledge of the specifications
Resources described in the Reference Documents section. See Table40 and
Table41. Provided with Core Features Documentation Product Specification
(cid:129) Conforms to the ISO 11898 -1, CAN 2.0A, and CAN Design Files
VHDL 2.0B standards Example Design Not Provided (cid:129) Supports
Industrial (I) and Extended Temperature Test Bench Not Provided Range
(Q) grade device support Constraints File None (cid:129) Supports both
standard (11-bit identifier) and extended (29-bit identifier) frames
Simulation None Model (cid:129) Supports bit rates up to 1 Mb/s Tested
Design Tools (cid:129) Transmit message FIFO with a user-configurable
depth of up to 64 messages Design Entry XPS 13.2 Tools (cid:129)
Transmit prioritization through one High-Priority Simulation Mentor
Graphics ModelSim (2) Transmit buffer Synthesis Tools XST 13.2 (cid:129)
Automatic re-transmission on errors or arbitration loss Support
(cid:129) Receive message FIFO with a user- configurable Provided by
Xilinx, Inc. depth of up to 64 messages 1. For a complete listing of
supported devices, see IDS Embedded Edition Derivative Device Support
for this core. (cid:129) Acceptance filtering (through a
user-configurable 2. For the supported versions of the tools, see the
ISE Design number) of up to four acceptance filters Suite 13: Release
Notes Guide. (cid:129) Sleep Mode with automatic walk-up (cid:129) Loop
Back Mode for diagnostic applications (cid:129) Maskable Error and
Status Interrupts (cid:129) Readable Error Counters © Copyright
2010-2011. Xilinx, Inc. Xilinx, Inc. Xilinx, the Xilinx logo, Artix,
ISE, Kintex, Spartan, Virtex, Zynq, and other designated brands included
herein are trademarks of Xilinx in the United States and other
countries. ARM is a registered trademark of ARM in the EU and other
countries. The AMBA trademark is a registered trademark of ARM Limited.
All other trademarks are the property of their respective owners. DS791
June 22, 2011 www.xilinx.com 1 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Functional
Description Figure1 illustrates the high-level architecture of the CAN
core. The CAN core requires an external 3.3 V compatible PHY device.
Descriptions of the submodules are given in the following sections.
Configuration Registers Table6 defines the configuration registers. This
module allows for read and write access to the registers through the
external micro-controller interface. Transmit and Receive Messages
Separate storage buffers exist for transmit (TX FIFO) and receive (RX
FIFO) messages through a FIFO structure. The depth of each buffer is
individually configurable up to a maximum of 64 messages. Transmit High
Priority Buffer The Transfer High Priority Buffer (TX HPB) provides
storage for one transmit message. Messages written on this buffer have
maximum transmit priority. They are queued for transmission immediately
after the current transmission is complete, preempting any message in
the TX FIFO. Acceptance Filters Acceptance Filters sort incoming
messages with the user-defined acceptance mask and ID registers to
determine whether to store messages in the RX FIFO, or to acknowledge
and discard them. The number of acceptance filters can be configured
from 0 to 4. Messages passed through acceptance filters are stored in
the RX FIFO. X-Ref Target - Figure 1 AXI4-Lite Xilinx CAN Controller
Object Layer Transfer Layer CAN Protocol Engine TX FIFO CAN BUS IPIF TX
P L r o io g r ic it y B P i r t o S c t e r s e s a o m r TX TX HPB CAN
PHY MicroBlaze IPIC Processor RX Bit Timing Configuration Module
Registers CAN CLK RX Acceptance FIFO Filtering DS791_01_100701 Figure 1:
AXI CAN Block Diagram DS791 June 22, 2011 www.xilinx.com 2
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Protocol
Engine The CAN protocol engine consists primarily of the Bit Timing
Logic (BTL) and the Bit Stream Processor (BSP) modules. Figure2
illustrates a block diagram of the CAN protocol engine. X-Ref Target -
Figure 2 To / From CAN Protocol Object Layer Engine TX TX Bit Message TX
Control Bit Timing CAN Control / PHY Status Bit Stream RX Bit Logic RX
Processor RX Message Sampling Clock Clock CAN CLK BRPR Prescaler
DS649_02_091007 Figure 2: CAN Protocol Engine Bit Timing Logic The
primary functions of the Bit Timing Logic (BTL) module include:
(cid:129) Synchronizing the CAN controller to CAN traffic on the bus
(cid:129) Sampling the bus and extracting the data stream from the bus
during reception (cid:129) Inserting the transmit bitstream onto the bus
during transmission (cid:129) Generating a sampling clock for the BSP
module state machine X-Ref Target - Figure 3 TS1 TS2 Sync Propagation
Phase Phase Segment Segment Segment 1 Segment 2 DS791_03_100701 Figure
3: CAN Bit Timing As illustrated in Figure3, the CAN bit time is divided
into four parts: (cid:129) Sync segment (cid:129) Propagation segment
(cid:129) Phase segment 1 (cid:129) Phase segment 2 These bit time parts
are comprised of a number of smaller segments of equal length called
time quanta (tq). The length of each time quantum is equal to the
quantum clock time period (period = tq). The quantum clock is generated
internally by dividing the incoming oscillator clock by the baud rate
pre-scaler. The pre-scaler value is passed to the BTL module through the
Baud Rate Prescaler (BRPR) register. DS791 June 22, 2011 www.xilinx.com
3 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) The
propagation segment and phase segment 1 are joined and called 'time
segment1' (TS1), while phase segment 2 is called 'time segment2' (TS2).
The number of time quanta in TS1 and TS2 vary with different networks
and are specified in the Bit Timing Register (BTR), which is passed to
the BTL module. The Sync segment is always one time quantum long. The
BTL state machine runs on the quantum clock. During the SOF bit of every
CAN frame, the state machine is instructed by the Bit Stream Processor
module to perform a hard sync, forcing the recessive (r) to dominant
edge (d) to lie in the sync segment. During the rest of the
recessive-to-dominant edges in the CAN frame, the BTL is prompted to
perform resynchronization. During resynchronization, the BTL waits for a
recessive-to-dominant edge and after that occurs, it calculates the time
difference (number of tqs) between the edge and the nearest sync
segment. To compensate for this time difference and to force the
sampling point to occur at the correct instant in the CAN bit time, the
BTL modifies the length of phase segment 1 or phase segment 2. The
maximum amount by which the phase segments can be modified is dictated
by the Synchronization Jump Width (SJW) parameter, which is also passed
to the BTL through the BTR. The length of the bit time of subsequent CAN
bits are unaffected by this process. This synchronization process
corrects for propagation delays and oscillator mismatches between the
transmitting and receiving nodes. After the controller is synchronized
to the bus, the state machine waits for a time period of TS1 and then
samples the bus, generating a digital '0' or '1'. This is passed on to
the BSP module for higher level tasks. Bit Stream Processor The Bit
Stream Processor (BSP) module performs several MAC/LLC functions during
reception (RX) and transmission (TX) of CAN messages. The BSP receives a
message for transmission from either the TX FIFO or the TX HPB and
performs the following functions before passing the bitstream to BTL.
(cid:129) Serializing the message (cid:129) Inserting stuff bits, CRC
bits, and other protocol defined fields during transmission During
transmission, the BSP simultaneously monitors RX data and performs bus
arbitration tasks. It then transmits the complete frame when arbitration
is won, and retrying when arbitration is lost. During reception, the BSP
removes Stuff bits, CRC bits, and other protocol fields from the
received bitstream. The BSP state machine also analyses bus traffic
during transmission and reception for Form, CRC, ACK, Stuff, and Bit
violations. The state machine then performs error signaling and error
confinement tasks. The CAN controller does not voluntarily generate
overload frames but does respond to overload flags detected on the bus.
This module determines the error state of the CAN controller: Error
Active, Error Passive, or Bus-off. When TX or RX errors are observed on
the bus, the BSP updates the transmit and receive error counters
according to the rules defined in the CAN 2.0 A, CAN 2.0 B and ISO
11898-1 standards. Based on the values of these counters, the error
state of the CAN controller is updated by the BSP. DS791 June 22, 2011
www.xilinx.com 4 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) I/O Signals
The AXI CAN I/O signals are listed and described inTable1 Table 1: I/O
Signal Description Signal Initial Port Signal Name Interface Description
Type State AXI Global System Signals P1 S_AXI_ACLK AXI I N/A AXI Clock
P2 S_AXI_ARESET_N AXI I N/A AXI Reset (active Low) AXI Write Address
Channel Signals AXI Write address. The write address S_AXI_AWADDR\[C_S\_
P3 AXI I N/A bus gives the address of the write AXI_ADDR_WIDTH- 1:0\]
transaction. Write address valid. This signal indicates P4 S_AXI_AWVALID
AXI I N/A that valid write address and control information are
available. Write address ready. This signal indicates that the slave is
ready to accept P5 S_AXI_AWREADY AXI O N/A an address and associated
control signals. AXI Write Data Channel Signals S_AXI_WDATA\[C_S\_AXI\_
P6 AXI I N/A Write Data DATA_WIDTH - 1: 0\] S_AXI_WSTB\[C_S\_AXI\_ Write
strobes. This signal indicates P7 AXI I N/A DATA_WIDTH/8- 1:0\] which
byte lanes to update in memory. Write valid. This signal indicates that
P8 S_AXI_WVALID AXI I N/A valid write data and strobes are available.
Write ready. This signal indicates that the P9 S_AXI_WREADY AXI O 0x0
slave can accept the write data. AXI Write Response Channel Signal Write
response. This signal indicates the status of the write transaction. P10
S_AXI_BRESP\[1:0\] AXI O 0x0 "00"- OKAY "10"- SLVERR "11"- DECERR Write
response valid. This signal P11 S_AXI_BVALID AXI O 0x0 indicates that a
valid write response is available Response ready. This signal indicates
P12 S_AXI_BREADY AXI I 0x1 that the master can accept the response
information DS791 June 22, 2011 www.xilinx.com 5 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 1: I/O
Signal Description (Cont'd) AXI Read Address Channel Signal
S_AXI_ARADDR\[C_S\_ Read address. The read address bus P13 AXI I N/A
AXI_ADDR_WIDTH - 1:0\] gives the address of a read transaction. Read
address valid. This signal indicates, when HIGH, that the read address
and control information is valid P14 S_AXI_ARVALID AXI I N/A and remains
stable until the address acknowledgement signal, S_AXI_ARREADY, is high.
Read address ready. This signal indicates that the slave is ready to
accept P15 S_AXI_ARREADY AXI O 0x1 an address and associated control
signals. AXI Read Data Channel Signal S_AXI_RDATA\[C_S\_AXI\_ P16 AXI O
0x0 Read data DATA_WIDTH -1:0\] Read response. This signal indicates the
status of the read transfer. P17 S_AXI_RRESP\[1:0\] AXI O 0x0 "00"- OKAY
"10"- SLVERR "11"- DECERR Read valid. This signal indicates that the P18
S_AXI_RVALID AXI O 0x0 required read data is available and the read
transfer can complete. Read ready. This signal indicates that P19
S_AXI_RREADY AXI I 0x1 the master can accept the read data and response
information. CAN Signals Oscillator Clock input (max value of 24 P20
CAN_CLK CAN I MHz) P21 CAN_PHY_RX CAN I CAN bus receive signal from PHY
P22 CAN_PHY_TX CAN O 1 CAN bus transmit signal to PHY P23
IP2Bus_IntrEvent CAN O 1 Interrupt line from CAN Note: S_AXI_Clk
frequency must be greater than or equal to CAN_CLK frequency. Register
Bit Ordering All registers use big-endian bit ordering where bit-0 is
MSB and bit-31 is LSB.Table2 shows the bit ordering. Table 2: Register
Bit Ordering 0 1 2 ............................................ 29 30 31
MSB LSB DS791 June 22, 2011 www.xilinx.com 6 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Controller
Design Parameters To obtain a CAN controller uniquely tailored to meet
the minimum system requirements, certain features are parameterized.
This results in a design using only the required resources, providing
the best possible performance. Table3 shows the CAN controller features
that can be parameterized. In addition to the parameters listed in this
table, there are also parameters that are inferred for each AXI
interface in the EDK tools. Through the design, these EDK-inferred
parameters control the behavior of the AXI Interconnect. For a complete
list of the interconnect settings related to the AXI interface, see AXI
Interconnect IP Data Sheet (DS768). Table 3: Design Parameters Default
VHDL Generic Feature/Description Parameter Name Allowable Values Values
Type System Parameters G1 Target FPGA Family C_FAMILY Spartan-6 &
Virtex-6 string See Base Address of the Xilinx std_logic G2
C_S\_AXI_BASEADDR Valid address footnotes CAN Controller \_vector (1)and
(2) See High Address of the Xilinx std_logic G3 C_S\_AXI_HIGHADDR Valid
address footnotes CAN Controller \_vector (1)and (2) CAN Parameters
Number of Acceptance G4 C_CAN_NUM_ACF 0 - 4 0 integer Filters used G5
Depth of the RX FIFO C_CAN_RX_DPTH 2,4,8,16,32,64 2 integer G6 Depth of
the TX FIFO C_CAN_TX_DPTH 2,4,8,16,32,64 2 integer AXI Parameters G7 AXI
Address bus width C_S\_AXI_ADDR_WIDTH 32 32 integer G8 AXI Data bus
width C_S\_AXI_DATA_WIDTH 32 32 integer 1. Address range specified by
C_S\_AXI_BASEADDR and C_S\_AXI_HIGHADDR must be at least 0x100 and must
be power of 2. C_S\_AXI_BASEADDR must be multiple of the range, where
the range is C_S\_AXI_HIGHADDR - C_S\_AXI_BASEADDR + 1. Also make sure
that LSB 8 bits of the C_S\_AXI_BASEADDR to be zero. 2. No default value
is specified to ensure that the actual value is set, that is, if the
value is not set, a compiler error is generated. The address range must
be at least 0x00FF. For example, C_S\_AXI_BASEADDR = 0x80000000,
C_S\_AXI_HIGHADDR = 0x800000FF. DS791 June 22, 2011 www.xilinx.com 7
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) The width of
some of the AXI CAN device signals depends on parameters selected in the
design. The dependencies between the AXI CAN device design parameters
and I/O signals are shown in Table4. Table 4: Parameter Port
Dependencies Generic Name Affects Depends Relationship Description or
Port Design Parameters G3 C_S\_AXI_HIGHADDR G2 Address range pair
dependency G7 C_S\_AXI_ADDR_WIDTH P3, P13 Defines the width of the ports
G8 C_S\_AXI_DATA_WIDTH P6, P7, P16 Defines the width of the ports I/O
Signals S_AXI_AWADDR\[C_S Port width depends on the generic P3 - G7
\_AXI_ADDR_WIDTH- 1:0\] C_S\_AXI_ADDR_WIDTH. S_AXI_WDATA\[C_S\_ Port
width depends on the generic P6 - G8 AXI_DATA_WIDTH - 1: 0\]
C_S\_AXI_DATA_WIDTH. S_AXI_WSTB\[C_S\_ Port width depends on the generic
P7 - G8 AXI_DATA_WIDTH/8- 1:0\] C_S\_AXI_DATA_WIDTH. S_AXI_ARADDR\[C_S
Port width depends on the generic P13 - G7 \_AXI_ADDR_WIDTH - 1:0\]
C_S\_AXI_ADDR_WIDTH. S_AXI_RDATA\[C_S\_AXI_DATA Port width depends on
the generic P16 - G8 \_WIDTH -1:0\] C_S\_AXI_DATA_WIDTH. Operational
Modes The CAN controller supports these modes of operation: (cid:129)
Configuration (cid:129) Normal (cid:129) Sleep (cid:129) Loop Back
Table5 contains the CAN Controller modes of operation and the
corresponding control and status bits. Inputs that affect the mode
transitions are discussed in Configuration Register Descriptions,
page13. Table 5: CAN Controller Modes of Operation Status Register Bits
(SR) S_AXI\_ SRST CEN LBACK SLEEP (Read Only) ARESET Bit Bit Bit Bit
Operation Mode \_N (SRR) (SRR) (MSR) (MSR) CONFIG LBACK SLEEP NORMAL '0'
X X X X '1' '0' '0' '0' Core is Reset '1' '1' X X X '1' '0' '0' '0' Core
is Reset Configuration '1' '0' '0' X X '1' '0' '0' '0' Mode '1' '0' '1'
'1' X '0' '1' '0' '0' Loop Back Mode '1' '0' '1' '0' '1' '0' '0' '1' '0'
Sleep Mode '1' '0' '1' '0' '0' '0' '0' '0' '1' Normal Mode DS791 June
22, 2011 www.xilinx.com 8 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a)
Configuration Mode The CAN controller enters the Configuration mode when
any of the following actions are performed, regardless of the operation
mode: (cid:129) Writing a '0' to the CEN bit in the SRR register.
(cid:129) Writing a '1' to the SRST bit in the SRR register. The core
enters the Configuration mode immediately following the software reset.
(cid:129) Driving a '0' on the S_AXI_ARESET_N input. The core continues
to be in reset as long as S_AXI_ARESET_N is '0'. The core enters
Configuration mode after S_AXI_ARESET_N is negated to '1'. The following
describes the Configuration mode features. (cid:129) CAN controller
loses synchronization with the CAN bus and drives a constant recessive
bit on the bus line. (cid:129) ECR register is reset. (cid:129) ESR
register is reset. (cid:129) BTR and BRPR registers can be modified.
(cid:129) CONFIG bit in the Status Register is '1.' (cid:129) CAN
controller does not receive any new messages. (cid:129) CAN controller
does not transmit any messages. Messages in the TX FIFO and the TX high
priority buffer are kept pending. These packets are sent when normal
operation is resumed. (cid:129) Reads from the RX FIFO can be performed.
(cid:129) Writes to the TX FIFO and TX HPB can be performed. (cid:129)
Interrupt Status Register bits ARBLST, TXOK, RXOK, RXOFLW, ERROR, BSOFF,
SLP and WKUP are cleared. (cid:129) Interrupt Status Register bits
RXNEMP, RXUFLW can be set due to read operations to the RX FIFO.
(cid:129) Interrupt Status Register bits TXBFLL and TXFLL, and the
Status Register bits TXBFLL and TXFLL, can be set due to write
operations to the TX HPB and TX FIFO, respectively. (cid:129) Interrupts
are generated if the corresponding bits in the IER are '1.' (cid:129)
All Configuration Registers are accessible. When in Configuration mode,
the CAN controller continues to stay in this mode until the CEN bit in
the SRR register is set to '1'. After the CEN bit is set to '1', the CAN
controller waits for a sequence of 11 recessive bits before exiting
Configuration mode. The CAN controller enters Normal, Loop Back, or
Sleep modes from Configuration mode, depending on the LBACK and SLEEP
bits in the MSR Register. Normal Mode In Normal mode, the CAN controller
participates in bus communication by transmitting and receiving
messages. From Normal mode, the CAN controller can enter either
Configuration or Sleep modes. For Normal mode, the CAN controller state
transitions are as follows: (cid:129) Enters Configuration mode when any
configuration condition is satisfied (cid:129) Enters Sleep mode when
the SLEEP bit in the MSR is '1' (cid:129) Enters Normal mode from
Configuration mode only when the LBACK and SLEEP bits in the MSR are '0'
and the CEN bit is '1' (cid:129) Enters Normal mode from Sleep mode when
a wake-up condition occurs DS791 June 22, 2011 www.xilinx.com 9
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Sleep Mode
The CAN controller enters Sleep mode from Configuration mode when the
LBACK bit in MSR is '0', the SLEEP bit in MSR is '1', and the CEN bit in
SRR is '1'. The CAN controller enters Sleep mode only when there are no
pending transmission requests from either the TX FIFO or the TX High
Priority Buffer. The CAN controller enters Sleep mode from Normal mode
only when the SLEEP bit is '1', the CAN bus is idle, and there are no
pending transmission requests from either the TX FIFO or TX High
Priority Buffer. When another node transmits a message, the CAN
controller receives the transmitted message and exits Sleep mode. When
the controller is in Sleep mode, if there are new transmission requests
from either the TX FIFO or the TX High Priority Buffer, these requests
are serviced, and the CAN controller exits Sleep mode. Interrupts are
generated when the CAN controller enters Sleep mode or wakes up from
Sleep mode. The CAN controller can enter either Configuration or Normal
modes from Sleep mode. The CAN controller can enter Configuration mode
when any configuration condition is satisfied. The CAN controller enters
Normal mode upon the following conditions (wake-up conditions):
(cid:129) Whenever the SLEEP bit is set to '0' (cid:129) Whenever the
SLEEP bit is '1', and bus activity is detected (cid:129) Whenever there
is a new message in the TX FIFO or the TX High Priority Buffer Loop Back
Mode In Loop Back mode, the CAN controller transmits a recessive
bitstream on to the CAN Bus. Any message that is transmitted is looped
back to the RX line and is acknowledged. The CAN controller receives any
message that it transmits. It does not participate in normal bus
communication and does not receive any messages that are transmitted by
other CAN nodes. This mode is used for diagnostic purposes. When in Loop
Back mode, the CAN controller can enter Configuration mode only. The CAN
controller enters Configuration mode when any of the configuration
conditions are satisfied. The CAN controller enters Loop Back mode from
the Configuration mode if the LBACK bit in MSR is '1' and the CEN bit in
SRR is '1.' Clocking and Reset Clocking The CAN core has two clocks:
CAN_CLK and S_AXI_ACLK. The following conditions apply for clock
frequencies: (cid:129) CAN_CLK can be 8 to 24 MHz in frequency.
(cid:129) CAN_CLK and S_AXI_ACLK can be asynchronous or can be clocked
from the same source Either of these clocks can be sourced from external
oscillator sources or generated within the FPGA. The oscillator used for
CAN_CLK must be compliant with the oscillator tolerance range given in
the ISO 11898 -1, CAN 2.0A, and CAN 2.0B standards. S_AXI_ACLK The user
can specify the operating frequency for S_AXI_ACLK. Using a DCM to
generate this clock is optional. S_AXI_ACLK frequency must be greater
than or equal to CAN_CLK frequency. DS791 June 22, 2011 www.xilinx.com
10 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) CAN_CLK The
range of CAN_CLK clock is 8-24 MHz. The user determines whether a DCM or
an external oscillator is used to generate the CAN_CLK. If an external
oscillator is used, it should meet the tolerance requirements specified
in the ISO 11898-1, CAN 2.0A and CAN 2.0B standards. Reset Mechanism Two
different reset mechanisms are provided for the CAN controller. The
S_AXI_ARESET_N input mentioned in Table1 acts as the system reset. Apart
from the system reset, a software reset is provided through the SRST bit
in the SRR register. Both the software reset and the system reset, reset
the complete CAN core (the Object Layer and the Transfer Layer as shown
in Figure1). Software Reset The software reset can be enabled by writing
a '1' to the SRST bit in the SRR Register. When a software reset is
asserted, all the configuration registers including the SRST bit in the
SRR Register are reset to their default values. Read/Write transactions
can be performed starting at the next valid transaction window. System
Reset The system reset can be enabled by driving a '0' on the
S_AXI_ARESET_N input. All the configuration registers are reset to their
default values. Read/Write transactions cannot be performed when the
S_AXI_ARESET_N input is '0'. Exceptions The contents of the acceptance
filter mask registers and acceptance filter ID registers are not cleared
when the software reset or system reset is asserted. Reset
Synchronization A reset synchronizer resets each clock domain in the
core. Because of this, some latency exists between the assertion of
reset and the actual reset of the core. DS791 June 22, 2011
www.xilinx.com 11 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Interrupts
The CAN IP core uses a hard-vector interrupt mechanism. It has a single
interrupt line (IP2Bus_IntrEvent) to indicate an interrupt. Interrupts
are indicated by asserting the IP2Bus_IntrEvent line (transition of the
IP2Bus_IntrEvent line from a logic '0' to a logic '1'). Events such as
errors on the bus line, message transmission and reception, FIFO
overflows and underflow conditions can generate interrupts. During power
on, the Interrupt line is driven low. The Interrupt Status Register
(ISR) indicates the interrupt status bits. These bits are set and
cleared regardless of the status of the corresponding bit in the
Interrupt Enable Register (IER). The IER handles the interrupt-enable
functionality. The clearing of a status bit in the ISR is handled by
writing a '1' to the corresponding bit in the Interrupt Clear Register
(ICR). The following two conditions cause the IP2Bus_IntrEvent line to
be asserted: (cid:129) If a bit in the ISR is '1' and the corresponding
bit in the IER is '1'. (cid:129) Changing an IER bit from a '0' to '1';
when the corresponding bit in the ISR is already '1'. (cid:129) Two
conditions cause the IP2Bus_IntrEvent line to be deasserted: (cid:129)
Clearing a bit in the ISR that is '1' (by writing a '1' to the
corresponding bit in the ICR); provided the corresponding bit in the IER
is '1'. (cid:129) Changing an IER bit from '1' to '0'; when the
corresponding bit in the ISR is '1'. When both deassertion and assertion
conditions occur simultaneously, the IP2Bus_IntrEvent line is deasserted
first, and is reasserted if the assertion condition remains true. DS791
June 22, 2011 www.xilinx.com 12 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a)
Configuration Register Descriptions Table6 lists the CAN controller
configuration registers. Each register is 32-bits wide and is
represented in big endian format. Any read operations to reserved bits
or bits that are not used return '0'. '0's are written to reserved bits
and bit fields that are not used. Writes to reserved locations are
ignored Table 6: Configuration Registers AXI Address Register Name
Access (offset from C_S\_AXI_BASEADDR) Control Registers Software Reset
Register (SRR) 0x000 Read/Write Mode Select Register (MSR) 0x004
Read/Write Transfer Layer Configuration Registers Baud Rate Prescaler
Register (BRPR) 0x008 Read/Write Bit Timing Register (BTR) 0x00C
Read/Write Error Indication Registers Error Counter Register (ECR) 0x010
Read Error Status Register (ESR) 0x014 Read/Write to Clear CAN Status
Registers Status Register (SR) 0x018 Read Interrupt Registers Interrupt
Status Register (ISR) 0x01C Read Interrupt Enable Register (IER) 0x020
Read/Write Interrupt Clear Register (ICR) 0x024 Write Reserved Reads
Return 0/ Reserved Locations 0x028 to 0x02C Write has no affect Messages
Transmit Message FIFO (TX FIFO) ID 0x030 Write DLC 0x034 Write Data Word
1 0x038 Write Data Word 2 0x03C Write Transmit High Priority Buffer (TX
HPB) ID 0x040 Write DLC 0x044 Write Data Word 1 0x048 Write Data Word 2
0x04C Write DS791 June 22, 2011 www.xilinx.com 13 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 6:
Configuration Registers Receive Message FIFO (RX FIFO) ID 0x050 Read DLC
0x054 Read Data Word 1 0x058 Read Data Word 2 0x05C Read Acceptance
Filtering Acceptance Filter Register (AFR) 0x060 Read/Write Acceptance
Filter Mask Register 1 (AFMR1) 0x064 Read/Write Acceptance Filter ID
Register 1 (AFIR1) 0x068 Read/Write Acceptance Filter Mask Register
2(AFMR2) 0x06C Read/Write Acceptance Filter ID Register 2 (AFIR2) 0x070
Read/Write Acceptance Filter Mask Register 3(AFMR3) 0x074 Read/Write
Acceptance Filter ID Register 3 (AFIR3) 0x078 Read/Write Acceptance
Filter Mask Register 4(AFMR4) 0x07C Read/Write Acceptance Filter ID
Register 4 (AFIR4) 0x080 Read/Write Reserved Reads Return 0/ Reserved
Locations 0x084 to 0x0FC Write has no affect Control Registers Software
Reset Register Writing '1' to the Software Reset Register (SRR) places
the CAN controller in Configuration mode. When in Configuration mode,
the CAN controller drives recessive on the bus line and does not
transmit or receive messages. During power-up, the CEN and SRST bits are
'0' and the CONFIG bit in the Status Register (SR) is '1'. The Transfer
Layer Configuration Registers can be changed only when the CEN bit in
the SRR Register is '0.' Use these steps to configure the CAN controller
at power up: 1. Configure the Transfer Layer Configuration Registers
(BRPR and BTR) with the values calculated for the specific bit rate. See
Baud Rate Prescaler Register and Bit Timing Register for more
information. 2. Do one of the following: a. For Loop Back mode, write
'1' to the LBACK bit in the MSR. a. For Sleep mode, write '1' to the
SLEEP bit in the MSR. See Operational Modes, page8 for information about
operational modes. 3. Set the CEN bit in the SRR to 1. After the
occurrence of 11 consecutive recessive bits, the CAN controller clears
the CONFIG bit in the Status Register to '0', and sets the appropriate
Status bit in the Status Register. Table7 shows the bit positions in the
SR register and Table8 provides the Software Reset Register bit
descriptions. DS791 June 22, 2011 www.xilinx.com 14 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 7:
Software Reset Register Bit Position 0 --- 29 30 31 Reserved CEN SRST
Table 8: Software Reset Register Bit Description Core Default Bit(s)
Name Description Access Value 0--29 Reserved Read/Write 0 Reserved:
These bit positions are reserved for future expansion. CAN Enable: The
Enable bit for the CAN controller. '1' = The CAN controller is in Loop
Back, Sleep, or Normal mode depending 30 CEN Read/Write 0 on the LBACK
and SLEEP bits in the MSR. '0' = The CAN controller is in the
Configuration mode. Reset: The Software reset bit for the CAN
controller. '1' = CAN controller is reset. 31 SRST Read/Write 0 If a '1'
is written to this bit, all the CAN controller configuration registers
(including the SRR) are reset. Reads to this bit always return a '0.'
Mode Select Register Writing to the Mode Select Register (MSR) enables
the CAN controller to enter Sleep, Loop Back, or Normal modes. In Normal
mode, the CAN controller participates in normal bus communication. If
the SLEEP bit is set to '1', the CAN controller enters Sleep mode. If
the LBACK bit is set to '1', the CAN controller enters Loop Back mode.
The LBACK and SLEEP bits should never both be '1' at the same time. At
any given point the CAN controller can be either in Loop Back mode or
Sleep mode, but not at the same time. If both are set, the LBACK Mode
takes priority. Table9 shows the bit positions in the MSR and Table10
provides MSR bit descriptions Table 9: Model Select Register Bit
Positions 0 --- 29 30 31 Reserved LBACK SLEEP DS791 June 22, 2011
www.xilinx.com 15 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 10:
Model Select Register Bit Descriptions Default Bit(s) Name Core Access
Description Value Reserved: These bit positions are reserved for 0--29
Reserved Read/Write 0 future expansion. Loop Back Mode Select: The Loop
Back Mode Select bit. '1' = CAN controller is in Loop Back mode. 30
LBACK Read/Write 0 '0' = CAN controller is in Normal, Configuration, or
Sleep mode. This bit can be written to only when the CEN bit in SRR is
'0'. Sleep Mode Select: The Sleep Mode select bit. '1' = CAN controller
is in Sleep mode. '0' = CAN controller is in Normal, Configuration or 31
SLEEP Read/Write 0 Loop Back mode. This bit is cleared when the CAN
controller wakes up from the Sleep mode. Transfer Layer Configuration
Registers There are two Transfer Layer Configuration Registers: Baud
Rate Prescaler Register (BRPR) and Bit Timing Register (BTR). These
registers can be written to only when the CEN bit in the SRR is '0.'
Baud Rate Prescaler Register The CAN clock for the CAN controller is
divided by (prescaler + 1) to generate the quantum clock needed for
sampling and synchronization. Table11 shows the bit positions in the
BRPR and Table12 provides BRPR descriptions. Table 11: Baud Rate
Prescaler Register Positions 0 --- 23 24 --- 31 Reserved BRP \[7.0\]
Table 12: Baud Rate Prescaler Register Bit Descriptions Default Bit(s)
Name Core Access Description Value 0--23 Reserved Read/Write 0 Reserved:
These bit positions are reserved for future expansion. Baud Rate
Prescaler: These bits indicate the prescaler value. 24 --31 BRP\[7.0\]
Read/Write 0 The actual value ranges from 1---256. The BRPR can be
programmed to any value in the range 0---255. The actual value is one
more than the value written into the register. The CAN quantum clock can
be calculated using this equation: tq = tosc\*(BRP+1) where tq and tosc
are the time periods of the quantum and oscillator/system clocks
respectively. Note: A given CAN bit rate can be achieved with several
bit-time configurations, but values should be chosen after careful
consideration of oscillator tolerances and CAN propagation delays. For
more information on CAN bit-time register settings, see the
specifications CAN 2.0A, CAN 2.0B, and ISO 11898-1. DS791 June 22, 2011
www.xilinx.com 16 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Bit Timing
Register The Bit Timing Register (BTR) specifies the bits needed to
configure bit time. Specifically, the Propagation Segment, Phase segment
1, Phase segment 2, and Synchronization Jump Width (as defined in CAN
2.0A, CAN 2.0B, and ISO 11898-1) are written to the BTR. The actual
value of each of these fields is one more than the value written to this
register. Table13 shows the bit positions in the BTR and Table14
provides BTR bit descriptions. Table 13: Bit Timing Register Bit
Positions 0---22 23---24 25---27 28---31 Reserved SJW\[1..0\]
TS2\[2..0\] TS1\[3..0\] Table 14: Bit Timing Register Bit Descriptions
Core Default Bit(s) Name Description Access Value 0--22 Reserved
Read/Write 0 Reserved: These bit positions are reserved for future
expansion Synchronization Jump Width: Indicates the Synchronization Jump
Width as specified in the CAN 2.0A and CAN 2.0B 23--24 SJW\[1..0\]
Read/Write 0 standard. The actual value is one more than the value
written to the register. Time Segment 2: Indicates Phase Segment 2 as
specified in the CAN 2.0A and CAN 2.0B standard. 25--27 TS2\[2..0\]
Read/Write 0 The actual value is one more than the value written to the
register. Time Segment 1: Indicates the Sum of Propagation Segment and
Phase Segment 1 as specified in the CAN 2.0A and CAN 28--31 TS1\[3..0\]
Read/Write 0 2.0B standard. The actual value is one more than the value
written to the register. The following equations can be used to
calculate the number of time quanta in bit-time segments: tTSEG1 =
tq*(8*TSEG1\[3\]+4*TSEG1\[2\]+2*TSEG1\[1\]+TSEG1\[0\]+1) tTSEG2 =
tq*(4*TSEG2\[2\]+2*TSEG2\[1\]+TSEG2\[0\]+1) tSJW =
tq*(2\*SJW\[1\]+SJW\[0\]+1) where tTSEG1, tTSEG2, and tSJW are the
lengths of TS1, TS2, and SJW. Note: A given bit-rate can be achieved
with several bit-time configurations, but values should be chosen after
careful consideration of oscillator tolerances and CAN propagation
delays. For more information on CAN bit-time register settings, see the
CAN 2.0A, CAN 2.0B, and ISO 11898-1 specifications. DS791 June 22, 2011
www.xilinx.com 17 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Error
Indication Registers The Error Counter Register (ECR) and the Error
Status Register (ESR) comprise the Error Indication Registers. Error
Counter Register The ECR is a read-only register. Writes to the ECR have
no effect. The value of the error counters in the register reflect the
values of the transmit and receive error counters in the CAN Protocol
Engine Module (see Figure2). The following conditions reset the Transmit
and Receive Error counters: (cid:129) When a '1' is written to the SRST
bit in the SRR. (cid:129) When a '0' is written to the CEN bit in the
SRR. (cid:129) When the CAN controller enters Bus Off state. (cid:129)
During Bus Off recovery when the CAN controller enters Error Active
state after 128 occurrences of 11 consecutive recessive bits. When in
Bus Off recovery, the Receive Error counter is advanced by 1 whenever a
sequence of 11 consecutive recessive bits is seen. Table15 shows the bit
positions in the ECR and Table16 provides ECR bit descriptions. Table
15: Error Count Register BIT Positions 0 ---15 16 --- 23 24 --- 31
Reserved REC\[7..0\] TEC\[7..0\] Table 16: Error Count Register Bit
Descriptions Default Bit(s) Name Core Access Description Value 0--15
Reserved Read Only 0 Reserved: These bit positions are reserved for
future expansion. Receive Error Counter: Indicates the value of the
Receive Error 16--23 REC\[7..0\] Read Only 0 Counter Transmit Error
Counter: Indicates the value of the Transmit 24--31 TEC\[7..0\] Read
Only 0 Error Counter DS791 June 22, 2011 www.xilinx.com 18
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Error Status
Register The Error Status Register (ESR) indicates the type of error
that has occurred on the bus. If more than one error occurs, all
relevant error flag bits are set in this register. The ESR is a
write-to-clear register. Writes to this register does not set any bits,
but does clear the bits that are set. Table17 shows the bit positions in
the ESR and Table18 provides ESR bit descriptions. All the bits in the
ESR are cleared when a '0' is written to the CEN bit in the SRR. Table
17: Error Status Register BIT Positions 0 --- 26 27 28 29 30 31 Reserved
ACKER BERR STER FMER CRCER Table 18: Error Status Register Bit
Descriptions Core Default Bit(s) Name Description Access Value Reserved:
These bit positions are reserved for future 0---26 Reserved Read/Write 0
expansion. ACK Error: Indicates an acknowledgement error. '1' =
Indicates that an acknowledgement error has occurred. 27 ACKER Write to
Clear 0 '0' = Indicates that an acknowledgement error has not occurred
on the bus since the last write to this register. If this bit is set,
writing a '1' clears it. Bit Error: Indicates that the received bit is
not the same as the transmitted bit during bus communication. '1' =
Indicates that a bit error has occurred. 28 BERR Write to Clear 0 '0' =
Indicates that a bit error has not occurred on the bus since the last
write to this register. If this bit is set, writing a '1' clears it.
Stuff Error: Indicates an error if there is a stuffing violation. '1' =
Indicates that a stuff error has occurred. 29(1) STER Write to Clear 0
'0' = Indicates that a stuff error has not occurred on the bus since the
last write to this register. If this bit is set, writing a '1' clears
it. 1. In case of a CRC Error and a CRC delimiter corruption, only the
FMER bit is set. Status Register The CAN Status Register provides a
status of all conditions of the core. Specifically, FIFO status, Error
State, Bus State and Configuration mode are reported. Status Register
Table19 shows the SR bit positions in the SR and Table20 provides SR bit
descriptions. Table 19: Status Register BIT Positions 0 --- 19 20 21 22
23 --- 24 25 Reserved ACFBSY TXFLL TXBFLL ESTAT\[1..0\] ERRWRN 26 27 28
29 30 31 BBSY BIDLE NORMAL SLEEP LBACK CONFIG DS791 June 22, 2011
www.xilinx.com 19 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 20:
Status Register Bit Descriptions Core Default Bit(s) Name Description
Access Value 0---19 Reserved Read/Write 0 Reserved: These bit positions
are reserved for future expansion. Acceptance Filter Busy: This bit
indicates that the Acceptance Filter Mask Registers and the Acceptance
Filter ID Registers cannot be written to. '1' = Acceptance Filter Mask
Registers and Acceptance Filter ID Registers cannot be written to. 20
ACFBSY Read Only 0 '0' = Acceptance Filter Mask Registers and the
Acceptance Filter ID Registers can be written to. This bit exists only
when the number of acceptance filters is not '0' This bit is set when a
'0' is written to any of the valid UAF bits in the Acceptance Filter
Register. Transmit FIFO Full: Indicates that the TX FIFO is full. 21
TXFLL Read Only 0 '1' = Indicates that the TX FIFO is full. '0' =
Indicates that the TX FIFO is not full. High Priority Transmit Buffer
Full: Indicates that the High Priority Transmit Buffer is full. 22
TXBFLL Read Only 0 '1' = Indicates that the High Priority Transmit
Buffer is full. '0' = Indicates that the High Priority Transmit Buffer
is not full. Normal Mode: Indicates that the CAN controller is in Normal
Read Mode. 28 NORMAL 0 Only '1' = Indicates that the CAN controller is
in Normal Mode. '0' = Indicates that the CAN controller is not in Normal
mode. Sleep Mode: Indicates that the CAN controller is in Sleep mode.
Read 29 SLEEP 0 '1' = Indicates that the CAN controller is in Sleep
mode. Only '0' = Indicates that the CAN controller is not in Sleep mode.
Loop Back Mode: Indicates that the CAN controller is in Loop Read Back
mode. 30 LBACK 0 Only '1' = Indicates that the CAN controller is in Loop
Back mode. '0' = Indicates that the CAN controller is not in Loop Back
mode. Configuration Mode Indicator: Indicates that the CAN controller is
in Configuration mode. 31 CONFIG Read Only 1 '1' = Indicates that the
CAN controller is in Configuration mode. '0' = Indicates that the CAN
controller is not in Configuration mode. Interrupt Registers The CAN
controller contains a single interrupt line only, but contains several
interrupt conditions. Interrupts are controlled by the interrupt status,
enable, and clear registers. Interrupt Status Register The Interrupt
Status Register (ISR) contains bits that are set when a specific
interrupt condition occurs. If the corresponding mask bit in the
Interrupt Enable Register is set, an interrupt is generated. Interrupt
bits in the ISR can be cleared by writing to the Interrupt Clear
Register. For all bits in the ISR, a set condition takes priority over
the clear condition and the bit continues to remain '1'. DS791 June 22,
2011 www.xilinx.com 20 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table21
shows the bit positions in the ISR and Table22 provides ISR
descriptions. Table 21: Interrupt Status Register Bit Positions 0 --- 19
20 21 22 23 24 25 Reserved WKUP SLP BSOFF ERROR RXNEMP RXOFLW 26 27 28
29 30 31 RXUFLW RXOK TXBFLL TXFLL TXOK ARBLST Table 22: Interrupt Status
Register Bit Descriptions Core Default Bit(s) Name Description Access
Value 0--19 Reserved Read/Write 0 Reserved: These bit positions are
reserved for future expansion. Wake up Interrupt: A '1' indicates that
the CAN controller entered Read Normal mode from Sleep Mode. 20 WKUP 0
Only This bit can be cleared by writing to the ICR or when '0' is
written to the CEN bit in the SRR. Sleep Interrupt: A '1' indicates that
the CAN controller entered Read Sleep mode. 21 SLP 0 Only This bit can
be cleared by writing to the ICR or when '0' is written to the CEN bit
in the SRR. Bus Off Interrupt: A '1' indicates that the CAN controller
entered Read the Bus Off state. 22 BSOFF 0 Only This bit can be cleared
by writing to the ICR or when '0' is written to the CEN bit in the SRR.
Error Interrupt: A '1' indicates that an error occurred during Read
message transmission or reception. 23 ERROR 0 Only This bit can be
cleared by writing to the ICR or when '0' is written to the CEN bit in
the SRR. Receive FIFO Not Empty Interrupt: A '1' indicates that the Read
24 RXNEMP 0 Receive FIFO is not empty. Only This bit can be cleared only
by writing to the ICR. RX FIFO Overflow Interrupt: A '1' indicates that
a message has been lost. This condition occurs when a new message is
being Read 25 RXOFLW 0 received and the Receive FIFO is Full. Only This
bit can be cleared by writing to the ICR or when '0' is written to the
CEN bit in the SRR. RX FIFO Underflow Interrupt: A '1' indicates that a
read 26 RXUFLW Read Only 0 operation was attempted on an empty RX FIFO.
This bit can be cleared only by writing to the ICR. New Message Received
Interrupt: A '1' indicates that a Read message was received successfully
and stored into the RX FIFO. 27 RXOK 0 Only This bit can be cleared by
writing to the ICR or when '0' is written to the CEN bit in the SRR.
High Priority Transmit Buffer Full Interrupt: A '1' indicates that the
High Priority Transmit Buffer is full. TXBFLL Read 28 0 The status of
the bit is unaffected if write transactions occur on the Only High
Priority Transmit Buffer when it is already full. This bit can be
cleared only by writing to the ICR. DS791 June 22, 2011 www.xilinx.com
21 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 22:
Interrupt Status Register Bit Descriptions (Cont'd) Transmit FIFO Full
Interrupt: A '1' indicates that the TX FIFO is full. Read The status of
the bit is unaffected if write transactions occur on the 29 TXFLL 0 Only
Transmit FIFO when it is already full. This bit can be cleared only by
writing to the Interrupt Clear Register. Transmission Successful
Interrupt: A '1' indicates that a Read message was transmitted
successfully. 30 TXOK(1) 0 Only This bit can be cleared by writing to
the ICR or when '0' is written to the CEN bit in the SRR. Arbitration
Lost Interrupt: A '1' indicates that arbitration was lost during message
transmission. 31 ARBLST Read Only 0 This bit can be cleared by writing
to the ICR or when '0' is written to the CEN bit in the SRR. 1. In Loop
Back mode, both TXOK and RXOK bits are set. The RXOK bit is set before
the TXOK bit. Interrupt Enable Register The Interrupt Enable Register
(IER) is used to enable interrupt generation. Table23 shows the bit
positions in the IER and Table24 provides IER bit descriptions. Table
23: Interrupt Enable Register Bit Positions 0 --- 19 20 21 22 23 24 25
Reserved EWKUP ESLP EBSOFF EERROR ERXNEMP ERXOFLW 26 27 28 29 30 31
ERXUFLW ERXOK ETXBFLL ETXFLL ETXOK EARBLST Table 24: Interrupt Enable
Register Bit Descriptions Default Bit(s) Name Core Access Description
Value 0--19 Reserved Read/Write 0 Reserved: These bit positions are
reserved for future expansion. Enable Wake up Interrupt: Writes to this
bit enable or disable interrupts when the WKUP bit in the ISR is set. 20
EWKUP Read/Write 0 '1' = Enable interrupt generation if WKUP bit in ISR
is set. '0' = Disable interrupt generation if WKUP bit in ISR is set.
Enable Sleep Interrupt: Writes to this bit enable or disable interrupts
when the SLP bit in the ISR is set. 21 ESLP Read/Write 0 '1' = Enable
interrupt generation if SLP bit in ISR is set. '0' = Disable interrupt
generation if SLP bit in ISR is set. Enable Bus OFF Interrupt: Writes to
this bit enable or disable interrupts when the BSOFF bit in the ISR is
set. 22 EBSOFF Read/Write 0 '1' = Enable interrupt generation if BSOFF
bit in ISR is set. '0' = Disable interrupt generation if BSOFF bit in
ISR is set. Enable Error Interrupt: Writes to this bit enable or disable
interrupts when the ERROR bit in the ISR is set. 23 EERROR Read/Write 0
'1' = Enable interrupt generation if ERROR bit in ISR is set. '0' =
Disable interrupt generation if ERROR bit in ISR is set. DS791 June 22,
2011 www.xilinx.com 22 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 24:
Interrupt Enable Register Bit Descriptions (Cont'd) Enable Receive FIFO
Not Empty Interrupt: Writes to this bit enable or disable interrupts
when the RXNEMP bit in the ISR is 24 ERXNEMP Read/Write 0 set. '1' =
Enable interrupt generation if RXNEMP bit in ISR is set. '0' = Disable
interrupt generation if RXNEMP bit in ISR is set. Enable RX FIFO
Overflow Interrupt: Writes to this bit enable or disable interrupts when
the RXOFLW bit in the ISR is set. 25 ERXOFLW Read/Write 0 '1' = Enable
interrupt generation if RXOFLW bit in ISR is set. '0' = Disable
interrupt generation if RXOFLW bit in ISR is set. Enable RX FIFO
Underflow Interrupt: Writes to this bit enable or disable interrupts
when the RXUFLW bit in the ISR is set. 26 ERXUFLW Read/Write 0 '1' =
Enable interrupt generation if RXUFLW bit in ISR is set. '0' = Disable
interrupt generation if RXUFLW bit in ISR is set. Enable New Message
Received Interrupt: Writes to this bit enable or disable interrupts when
the RXOK bit in the ISR is set. 27 ERXOK Read/Write 0 '1' = Enable
interrupt generation if RXOK bit in ISR is set. '0' = Disable interrupt
generation if RXOK bit in ISR is set. Enable High Priority Transmit
Buffer Full Interrupt: Writes to this bit enable or disable interrupts
when the TXBFLL bit in the 28 ETXBFLL Read/Write 0 ISR is set. '1' =
Enable interrupt generation if TXBFLL bit in ISR is set. '0' = Disable
interrupt generation if TXBFLL bit in ISR is set. Enable Transmit FIFO
Full Interrupt: Writes to this bit enable or disable interrupts when
TXFLL bit in the ISR is set. 29 ETXFLL Read/Write 0 '1' = Enable
interrupt generation if TXFLL bit in ISR is set. '0' = Disable interrupt
generation if TXFLL bit in ISR is set. Enable Transmission Successful
Interrupt: Writes to this bit enable or disable interrupts when the TXOK
bit in the ISR is set. 30 ETXOK Read/Write 0 '1' = Enable interrupt
generation if TXOK bit in ISR is set. '0' = Disable interrupt generation
if TXOK bit in ISR is set. Enable Arbitration Lost Interrupt: Writes to
this bit enable or disable interrupts when the ARBLST bit in the ISR is
set. 31 EARBLST Read/Write 0 '1' = Enable interrupt generation if ARBLST
bit in ISR is set. '0' = Disable interrupt generation if ARBLST bit in
ISR is set. Interrupt Clear Register The Interrupt Clear Register (ICR)
is used to clear interrupt status bits. Table25 shows the bit positions
in the ICR and Table26 gives the ICR bit descriptions. Table 25:
Interrupt Clear Register Bit Positions 0 --- 19 20 21 22 23 24 25
Reserved CWKUP CSLP CBSOFF CERROR CRXNEMP CRXOFLW 26 27 28 29 30 31
CRXUFLW CRXOK CTXBFLL CTXFLL CTXOK CARBLST DS791 June 22, 2011
www.xilinx.com 23 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 26:
Interrupt Clear Register Bit Descriptions Core Default Bit(s) Name
Description Access Value 0--19 Reserved Read/Write 0 Reserved: These bit
positions are reserved for future expansion. Clear Wake up Interrupt:
Writing a '1' to this bit clears the WKUP bit 20 CWKUP Write Only 0 in
the ISR Clear Sleep Interrupt: Writing a '1' to this bit clears the SLP
bit in the 21 CSLP Write Only 0 ISR. Clear Bus Off Interrupt: Writing a
'1' to this bit clears the BSOFF bit 22 CBSOFF Write Only 0 in the ISR.
Clear Error Interrupt: Writing a '1' to this bit clears the ERROR bit in
23 CERROR Write Only 0 the ISR. Clear Receive FIFO Not Empty Interrupt:
Writing a '1' to this bit 24 CRXNEMP Write Only 0 clears the RXNEMP bit
in the ISR. Clear RX FIFO Overflow Interrupt: Writing a '1' to this bit
clears the 25 CRXOFLW Write Only 0 RXOFLW bit in the ISR. Clear RX FIFO
Underflow Interrupt: Writing a '1' to this bit clears the 26 CRXUFLW
Write Only 0 RXUFLW bit in the ISR. Clear New Message Received
Interrupt: Writing a '1' to this bit clears 27 CRXOK Write Only 0 the
RXOK bit in the ISR. Clear High Priority Transmit Buffer Full Interrupt:
Writing a '1' to 28 CTXBFLL Write Only 0 this bit clears the TXBFLL bit
in the ISR. Clear Transmit FIFO Full Interrupt: Writing a '1' to this
bit clears the 29 CTXFLL Write Only 0 TXFLL bit in the ISR. Clear
Transmission Successful Interrupt: Writing a '1' to this bit 30 CTXOK
Write Only 0 clears the TXOK bit in the ISR. Clear Arbitration Lost
Interrupt: Writing a '1' to this bit clears the 31 CARBLST Write Only 0
ARBLST bit in the ISR. Message Storage The CAN controller has a Receive
FIFO (RX FIFO) for storing received messages. The RX FIFO depth is
configurable and can store up to 64 messages. Messages that pass any of
the acceptance filters are stored in the RX FIFO. When no acceptance
filter has been selected, all received messages are stored in the RX
FIFO. The CAN controller has a configurable Transmit FIFO (TX FIFO) that
can store up to 64 messages. The CAN controller also has a High Priority
Transmit Buffer (TX HPB), with storage for one message. When a higher
priority message needs to be sent, write the message to the High
Priority Transmit Buffer. The message in the Transmit Buffer has
priority over messages in the TX FIFO. DS791 June 22, 2011
www.xilinx.com 24 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Message
Transmission and Reception The following rules apply regarding message
transmission and reception: (cid:129) A message in the TX High Priority
Buffer (TX HPB) has priority over messages in the TX FIFO. (cid:129) In
case of arbitration loss or errors during the transmission of a message,
the CAN controller tries to retransmit the message. No subsequent
message, even a newer, higher priority message is transmitted until the
original message is transmitted without errors or arbitration loss.
(cid:129) The messages in the TX FIFO, TX HPB and RX FIFO are retained
even if the CAN controller enters Bus off state or Configuration mode.
Message Structure Each message is 16 bytes. Byte ordering for the CAN
message structure is shown in Table27, Table28,Table29, and Table30.
Table 27: Message Identifier \[IDR\] 0 --- 10 11 12 13 --- 30 31 ID
\[28..18\] SRR/RTR IDE ID\[17..0\] RTR Table 28: Data Length Code
\[DLCR\] 0 --- 3 4 --- 31 DLC \[3..0\] Reserved Table 29: Data Word 1
\[DW1R\] 0 --- 7 8 --- 15 16 --- 23 24 --- 31 DB0\[7..0\] DB1\[7..0\]
DB2\[7..0\] DB3\[7..0\] Table 30: Data Word 2 \[DW2R\] 0 --- 7 8 --- 15
16 --- 23 24 --- 31 DB4\[7..0\] DB5\[7..0\] DB6\[7..0\] DB7\[7..0\] All
16 bytes must be read from the RX FIFO to receive the complete message.
The first word read (4 bytes) returns the identifier of the received
message (IDR). The second read returns the Data Length Code (DLC) field
of the received message (DLCR). The third read returns Data Word 1
(DW1R), and the fourth read returns Data Word 2 (DW2R). All four words
have to be read for each message, even if the message contains less than
8 data bytes. Write transactions to the RX FIFO are ignored. Reads from
an empty RX FIFO return invalid data. DS791 June 22, 2011 www.xilinx.com
25 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Writes to TX
FIFO and High Priority TX Buffer When writing to the TX FIFO or the TX
HPB, all 16 bytes must be written. The first word written (4 bytes) is
the Identifier (IDR). The second word written is the DLC field (DLCR).
The third word written is Data Word 1 (DW1R) and the fourth word written
is Data Word 2 (DW2R). When transmitting on the CAN bus, the CAN
controller transmits the data bytes in the following order (DB0, DB1,
DB2, DB3, DB4, DB5, DB6, DB7). The MSB of a data byte is transmitted
first. All four words must be written for each message, including
messages containing fewer than 8 data bytes. Reads transactions from the
TX FIFO or the TX High Priority Buffer return '0's. (cid:129) '0's must
be written to Data Fields in DW1R and DW2R registers that are not used
(cid:129) '0's must be written to bits 4 to 31 in the DLCR (cid:129)
'0's must be written to IDR \[13 to 31\] for standard frames Identifier
The Identifier (IDR) word contains the identifier field of the CAN
message. Two different formats exist for the Identifier field of the CAN
message frame: Standard and Extended frames. (cid:129) Standard Frames:
Standard frames have an 11-bit identifier field called the Standard
Identifier.Only the ID\[28..18\], SRR/RTR, and IDE bits are valid.
ID\[28..18\] is the 11 bit identifier. The SRR/RTR bit differentiates
between data and remote frames. IDE is '0' for standard frames. The
other bit fields are not used. (cid:129) Extended Frames: Extended
frames have an 18-bit identifier extension in addition to the Standard
Identifier. All bit fields are valid. The RTR bit is used to
differentiate between data and remote frames (The SRR/RTR bit and IDE
bit are both '1' for all Extended Frames). Table31 provides bit
descriptions for the Identifier Word. Table32 provides bit descriptions
for the DLC Word. Table33 provides bit descriptions for Data Word 1 and
Data Word 2 DS791 June 22, 2011 www.xilinx.com 26 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 31:
Identifier Word Bit Descriptions Default Bit(s) Name Core Access
Description Value Reads from RX FIFO Standard Message ID: The Identifier
portion for a Standard Frame is 11 bits. 0--10 ID\[28..18\] 0 Writes to
These bits indicate the Standard Frame ID. TX FIFO and This field is
valid for both Standard and Extended Frames. TX HPB Reads from RX
Substitute Remote Transmission Request: This bit FIFO differentiates
between data frames and remote frames. Valid only for Standard Frames.
For Extended frames this bit 11 SRR/RTR 0 Writes to is 1. TX FIFO and
'1' = Indicates that the message frame is a Remote Frame. TX HPB '0' =
Indicates that the message frame is a Data Frame. Reads from RX FIFO
Identifier Extension: This bit differentiates between frames using the
Standard Identifier and those using the Extended 12 IDE 0 Identifier.
Valid for both Standard and Extended Frames. Writes to '1' = Indicates
the use of an Extended Message Identifier. TX FIFO and '0'= Indicates
the use of a Standard Message Identifier. TX HPB Reads from RX Extended
Message ID: This field indicates the Extended FIFO Identifier. 13--30
ID\[17..0\] 0 Valid only for Extended Frames. Writes to For Standard
Frames, reads from this field return '0's TX FIFO and For Standard
Frames, writes to this field should be '0's TX HPB Remote Transmission
Request: This bit differentiates Reads from RX between data frames and
remote frames. FIFO Valid only for Extended Frames. 31 RTR 0 '1' =
Indicates that the message object is a Remote Frame Writes to '0' =
Indicates that the message object is a Data Frame TX FIFO and For
Standard Frames, reads from this bit returns '0' TX HPB For Standard
Frames, writes to this bit should be '0' Table 32: DLC Word Bit
Descriptions Default Bit(s) Name Core Access Description Value Data
Length Code: This is the data length portion of the control field of the
CAN 0--3 DLC Read/Write 0 frame. This indicates the number valid data
bytes in Data Word 1 and Data Word 2 registers. Reads from this field
return '0's. 4--31 Reserved Read/Write Writes to this field should be
'0's. DS791 June 22, 2011 www.xilinx.com 27 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 33:
Data Word 1 and Data Word 2 Bit Descriptions Default Register Field Core
Access Description Value DW1R Data Byte 0: Reads from this field return
invalid data if the DB0\[7..0\] Read/Write 0 \[0..7\] message has no
data. DW1R Data Byte 1: Reads from this field return invalid data if the
DB1\[7..0\] Read/Write 0 \[8..15\] message has only 1 byte of data or
fewer. DW1R Data Byte 2: Reads from this field return invalid data if
the DB2\[7..0\] Read/Write 0 \[16..23\] message has 2 bytes of data or
fewer. DW1R Data Byte 3: Reads from this field return invalid data if
the DB3\[7..0\] Read/Write 0 \[24..31\] message has 3 bytes of data or
fewer. DW2R Data Byte 4: Reads from this field return invalid data if
the DB4\[7..0\] Read/Write 0 \[0..7\] message has 4 bytes of data or
fewer. DW2R Data Byte 5: Reads from this field return invalid data if
the DB5\[7..0\] Read/Write 0 \[8..15\] message has 5 bytes of data or
fewer. DW2R Data Byte 6: Reads from this field return invalid data if
the DB6\[7..0\] Read/Write 0 \[16..23\] message has 6 bytes of data or
fewer. DW2R Data Byte 7: Reads from this field return invalid data if
the DB7\[7..0\] Read/Write 0 \[24..31\] message has 7 bytes of data or
fewer. Acceptance Filters The number of acceptance filters is
configurable from 0 to 4. The parameter Number of Acceptance Filters
specifies the number of acceptance filters that are chosen. Each
acceptance filter has an Acceptance Filter Mask Register and an
Acceptance Filter ID Register. Acceptance filtering is performed in the
following sequence: 1. The incoming Identifier is masked with the bits
in the Acceptance Filter Mask Register. 2. The Acceptance Filter ID
Register is also masked with the bits in the Acceptance Filter Mask
Register. 3. The resultant values are compared. 4. If the values are
equal, the message is stored in the RX FIFO. 5. Acceptance filtering is
processed by each of the defined filters. If the incoming identifier
passes through any acceptance filter, the message is stored in the RX
FIFO. The following rules apply to the Acceptance filtering process:
(cid:129) If no acceptance filters are selected (for example, if all the
valid UAF bits in the AFR register are '0's or if the parameter Number
of Acceptance Filters = '0'), all received messages are stored in the RX
FIFO. (cid:129) If the number of acceptance filters is greater than or
equal to 1, all the Acceptance Filter Mask Register and the Acceptance
Filter ID Register locations can be written to and read from. However,
the use of these filter pairs for acceptance filtering is governed by
the existence of the UAF bits in the AFR register. DS791 June 22, 2011
www.xilinx.com 28 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Acceptance
Filter Register The Acceptance Filter Register (AFR) defines which
acceptance filters to use. Each Acceptance Filter ID Register (AFIR) and
Acceptance Filter Mask Register (AFMR) pair is associated with a UAF
bit. When the UAF bit is '1', the corresponding acceptance filter pair
is used for acceptance filtering. When the UAF bit is '0', the
corresponding acceptance filter pair is not used for acceptance
filtering. The AFR exists only if the Number of Acceptance Filters
parameter is not set to '0.' To modify an acceptance filter pair in
Normal mode, the corresponding UAF bit in this register must be set to
'0.' After the acceptance filter is modified, the corresponding UAF bit
must be set to '1.' These conditions govern the number of UAF bits that
can exist in the AFR: (cid:129) If the number of acceptance filters is
1:UAF1 bit exists (cid:129) If the number of acceptance filters is
2:UAF1 and UAF2 bits exist (cid:129) If the number of acceptance filters
is 3:UAF1, UAF2 and UAF3 bits exist (cid:129) If the number of
acceptance filters is 4:UAF1, UAF2, UAF3 and UAF4 bits exist (cid:129)
UAF bits for filters that do not exist are not written to (cid:129)
Reads from UAF bits that do not exist return '0's (cid:129) If all
existing UAF bits are set to '0', all received messages are stored in
the RX FIFO (cid:129) If the UAF bits are changed from a '1' to '0'
during reception of a CAN message, the message may not be stored in the
RX FIFO. Table34 shows the bit positions in the AFR and Table35 gives
the AFR bit descriptions. Table 34: Acceptance Filter Register Bit
Positions 0 --- 27 28 29 30 31 Reserved UAF4 UAF3 UAF2 UAF1 DS791 June
22, 2011 www.xilinx.com 29 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 35:
Acceptance Filter Register Bit Descriptions Core Default Bit(s) Name
Description Access Value 0--27 Reserved Read/Write 0 Reserved: These bit
positions are reserved for future expansion. Use Acceptance Filter
Number 4: Enables the use of acceptance filter pair 4. '1' = Indicates
that Acceptance Filter Mask Register 4 and Acceptance 28 UAF4 Read/Write
0 Filter ID Register 4 are used for acceptance filtering. '0' =
Indicates that Acceptance Filter Mask Register 4 and Acceptance Filter
ID Register 4 are not used for acceptance filtering. Use Acceptance
Filter Number 3: Enables the use of acceptance filter pair 3. '1' =
Indicates that Acceptance Filter Mask Register 3 and Acceptance 29 UAF3
Read/Write 0 Filter ID Register 3 are used for acceptance filtering. '0'
= Indicates that Acceptance Filter Mask Register 3 and Acceptance Filter
ID Register 3 are not used for acceptance filtering. Use Acceptance
Filter Number 2: Enables the use of acceptance filter pair 2. '1' =
Indicates that Acceptance Filter Mask Register 2 and Acceptance 30 UAF2
Read/Write 0 Filter ID Register 2 are used for acceptance filtering. '0'
= Indicates that Acceptance Filter Mask Register 2 and Acceptance Filter
ID Register 2 are not used for acceptance filtering. Use Acceptance
Filter Number 1: Enables the use of acceptance filter pair 1. '1' =
Indicates that Acceptance Filter Mask Register 1 and Acceptance 31 UAF1
Read/Write 0 Filter ID Register 1 are used for acceptance filtering. '0'
= Indicates that Acceptance Filter Mask Register 1 and Acceptance Filter
ID Register 1 are not used for acceptance filtering. DS791 June 22, 2011
www.xilinx.com 30 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Acceptance
Filter Mask Registers The Acceptance Filter Mask Registers (AFMR)
contain mask bits that are used for acceptance filtering. The incoming
message identifier portion of a message frame is compared with the
message identifier stored in the acceptance filter ID register. The mask
bits define which identifier bits stored in the acceptance filter ID
register are compared to the incoming message identifier. There are at
most four AFMRs. These registers are stored in a block RAM. Asserting a
software reset or system reset does not clear register contents. If the
number of acceptance filters is greater than or equal to 1, all four
AFMRs are defined. These registers can be read from and written to.
However, filtering operations are only performed on the number of
filters defined by the Number of Acceptance Filters parameter. These
registers are written to only when the corresponding UAF bits in the AFR
are '0' and ACFBSY bit in the SR is '0.' The following conditions govern
AFMRs: (cid:129) If the number of acceptance filters is 1, AFMR 1 is
used for acceptance filtering. (cid:129) If the number of acceptance
filters is 2, AFMR 1 and AFMR 2 are used for acceptance filtering.
(cid:129) If the number of acceptance filters is 3, AFMR 1, AFMR 2 and
AFMR 3 are used for acceptance filtering. (cid:129) If the number of
acceptance filters is 4, AFMR 1, AFMR 2, AFMR 3 and AFMR 4 are used for
acceptance filtering. (cid:129) Extended Frames: All bit fields (AMID
\[28..18\], AMSRR, AMIDE, AMID \[17..0\] and AMRTR) need to be defined.
(cid:129) Standard Frames: Only AMID \[28..18\], AMSRR and AMIDE need to
be defined. AMID \[17..0\] and AMRTR should be written as '0'. Table36
shows the bit positions in the AFMR and Table37 provides bit
descriptions. Table 36: Acceptance Filter Mask Registers Bit Positions 0
--- 10 11 12 13 --- 30 31 AMID\[28..18\] AMSRR AMIDE AMID\[17..0\] AMRTR
DS791 June 22, 2011 www.xilinx.com 31 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 37:
Acceptance Filter Mask Bit Descriptions Core Default Bit(s) Name
Description Access Value Standard Message ID Mask: These bits are used
for masking the Identifier in a Standard Frame. '1'= Indicates that the
corresponding bit in the Acceptance Mask ID 0--10 AMID \[28..18\]
Read/Write 0 Register is used when comparing the incoming message
identifier. '0' = Indicates that the corresponding bit in the Acceptance
Mask ID Register is not used when comparing the incoming message
identifier. Substitute Remote Transmission Request Mask: This bit is
used for masking the RTR bit in a Standard Frame. '1'= Indicates that
the corresponding bit in the Acceptance Mask ID 11 AMSRR Read/Write 0
Register is used when comparing the incoming message identifier. '0' =
Indicates that the corresponding bit in the Acceptance Mask ID Register
is not used when comparing the incoming message identifier. Identifier
Extension Mask: Used for masking the IDE bit in CAN frames. '1'=
Indicates that the corresponding bit in the Acceptance Mask ID Register
is used when comparing the incoming message identifier. '0' = Indicates
that the corresponding bit in the Acceptance Mask ID Register is not
used when comparing the incoming message identifier. 12 AMIDE Read/Write
0 If AMIDE = '1' and the AIIDE bit in the corresponding Acceptance ID
register is '0', this mask is applicable to only Standard frames. If
AMIDE = '1' and the AIIDE bit in the corresponding Acceptance ID
register is '1', this mask is applicable to only extended frames. If
AMIDE = '0' this mask is applicable to both Standard and Extended
frames. Extended Message ID Mask: These bits are used for masking the
Identifier in an Extended Frame. '1'= Indicates that the corresponding
bit in the Acceptance Mask ID 13--30 AMID\[17..0\] Read/Write 0 Register
is used when comparing the incoming message identifier. '0' = Indicates
that the corresponding bit in the Acceptance Mask ID Register is not
used when comparing the incoming message identifier. Remote Transmission
Request Mask: This bit is used for masking the RTR bit in an Extended
Frame. '1'= Indicates that the corresponding bit in the Acceptance Mask
ID 31 AMRTR Read/Write 0 Register is used when comparing the incoming
message identifier. '0' = Indicates that the corresponding bit in the
Acceptance Mask ID Register is not used when comparing the incoming
message identifier. DS791 June 22, 2011 www.xilinx.com 32
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Acceptance
Filter ID Registers The Acceptance Filter ID registers (AFIR) contain
Identifier bits, which are used for acceptance filtering. There are at
most four Acceptance Filter ID Registers. These registers are stored in
a block RAM. Asserting a software reset or system reset does not clear
the contents of these registers. If the number of acceptance filters is
greater than or equal to 1, all four AFIRs are defined. These registers
can be read from and written to. These registers should be written to
only when the corresponding UAF bits in the AFR are '0' and ACFBSY bit
in the SR is '0'. The following conditions govern the use of the AFIRs:
(cid:129) If the number of acceptance filters is 1, AFIR 1 is used for
acceptance filtering. (cid:129) If the number of acceptance filters is
2, AFIR 1 and AFIR 2 are used for acceptance filtering. (cid:129) If the
number of acceptance filters is 3, AFIR 1, AFIR 2 and AFIR 3 are used
for acceptance filtering. (cid:129) If the number of acceptance filters
is 4, AFIR 1, AFIR 2, AFIR 3 and AFIR 4 are used for acceptance
filtering. (cid:129) Extended Frames: All the bit fields (AIID
\[28..18\], AISRR, AIIDE, AIID \[17..0\] and AIRTR) must be defined.
(cid:129) Standard Frames: Only AIID \[28..18\], AISRR and AIIDE need to
be defined. AIID \[17..0\] and AIRTR should be written with '0' Table38
shows AFIR bit positions. Table39 provides AFIR bit descriptions. Table
38: Acceptance Filter ID Registers Bit Positions 0 --- 10 11 12 13 ---
30 31 AIID\[28..18\] AISRR AIIDE AIID\[17..0\] AIRTR Table 39:
Acceptance Filter ID Registers Bit Descriptions Core Default Bit(s) Name
Description Access Value 0--10 AIID \[28..18\] Read/Write 0 Standard
Message ID: This is the Standard Identifier. Substitute Remote
Transmission Request: Indicates the Remote 11 AISRR Read/Write 0
Transmission Request bit for Standard frames Identifier Extension:
Differentiates between Standard and Extended 12 AIIDE Read/Write 0
frames 13--30 AIID\[17..0\] Read/Write 0 Extended Message ID: Extended
Identifier 31 AIRTR Read/Write 0 Remote Transmission Request Mask: RTR
bit for Extended frames. Configuring the CAN Controller This section
covers the various configuration steps that must be performed to program
the CAN core for operation. The following key configuration steps are
detailed in this section. 1. Choose the mode of operation of the CAN
core. 2. Program the configuration registers to initialize the core. 3.
Write messages to the TX FIFO/ TX HPB. 4. Read messages from the RX
FIFO. DS791 June 22, 2011 www.xilinx.com 33 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Programming
the Configuration Registers The following are steps to configure the
core when the core is powered-on, or after system or software reset. 1.
Choose the operation mode. a. For Loop Back mode, write a '1' to the
LBACK bit in the MSR and '0' to the SLEEP bit in the MSR. b. For Sleep
mode, write a '1' to the SLEEP bit in the MSR and '0' to the LBACK bit
in the MSR. c. For Normal Mode, write '0's to the LBACK and SLEEP bits
in the MSR. 2. Configure the Transfer Layer Configuration Registers. a.
Program the Baud Rate Prescalar Register and the Bit Timing Register to
correspond to the network timing parameters and the network
characteristics of the system. 3. Configure the Acceptance Filter
Registers. The number of Acceptance Filter Mask and Acceptance Filter ID
Register pairs is chosen at build time. To configure these registers do
the following: a. Write a '0' to the UAF bit in the AFR register
corresponding to the Acceptance Filter Mask and ID Register pair to be
configured. b. Wait until the ACFBSY bit in the SR is '0'. c. Write the
appropriate mask information to the Acceptance Filter Mask Register.
d. Write the appropriate ID information to the to the Acceptance Filter
ID Register. e. Write a '1' to the UAF bit corresponding to the
Acceptance Filter Mask and ID Register pair. f. Repeat the preceding
steps for each Acceptance Filter Mask and ID Register pair. 4. Write to
the Interrupt Enable Register to choose the bits in the Interrupt Status
Register than can generate an interrupt. 5. Enable the CAN controller by
writing a '1' to the CEN bit in the SRR register. Transmitting a Message
A message to be transmitted can be written to either the TX FIFO or the
TX HPB. A message in the TX HPB gets priority over the messages in the
TX FIFO. The TXOK bit in the ISR is set after the CAN core successfully
transmits a message. Writing a Message to the TX FIFO All messages
written to the TX FIFO should follow the format described in Message
Storage, page24. To perform a write: 1. Poll the TXFLL bit in the SR.
The message can be written into the TX FIFO when the TXFLL bit is '0' 2.
Write the ID of the message to the TX FIFO ID memory location (0x030).
3. Write the DLC of the message to the TX FIFO DLC memory location
(0x034). 4. Write Data Word 1 of the message to the TX FIFO DW1 memory
location (0x038). 5. Write Data Word 2 of the message to the TX FIFO DW2
memory location (0x03C). Messages can be continuously written to the TX
FIFO until the TX FIFO is full. When the TX FIFO is full, the TXFLL bit
in the ISR and the TXFLL bit in the SR are set. If polling, the TXFLL
bit in the Status Register should be polled after each write. If using
interrupt mode, writes can continue until the TXFLL bit in the ISR
generates an interrupt. DS791 June 22, 2011 www.xilinx.com 34
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Writing a
Message to the TX HPB All messages written to the TX FIFO should follow
the format described in Message Storage, page24. To write a message to
the TX HPB: 1. Poll the TXBFLL bit in the SR. The message can be written
into the TX HPB when the TXBFLL bit is '0'. 2. Write the ID of the
message to the TX HPB ID memory location (0x040). 3. Write the DLC of
the message to the TX HPB DLC memory location (0x044). 4. Write Data
Word 1 of the message to the TX HPB DW1 memory location (0x048). 5.
Write Data Word 2 of the message to the TX HPB DW2 memory location
(0x04C). After each write to the TX HPB, the TXBFLL bit in the Status
Register and the TXBFLL bit in the Interrupt Status Register are set.
Receiving a Message Whenever a new message is received and written into
the RX FIFO, the RXNEMP bit and the RXOK bits in the ISR are set. In
case of a read operation on an empty RX FIFO, the RXUFLW bit in the ISR
is set. Reading a Message from the RX FIFO Perform the following steps
to read a message from the RX FIFO. 1. Poll the RXOK or RXNEMP bits in
the ISR. In interrupt mode, the reads can occur after the RXOK or RXNEMP
bits in the ISR generate an interrupt. a. Read from the RX FIFO memory
locations. All the locations must be read regardless of the number of
data bytes in the message. b. Read from the RX FIFO ID location (0x050)
c. Read from the RX FIFO DLC location (0x054) d. Read from the RX FIFO
DW1 location (0x058) e. Read from the RX FIFO DW2 location (0x05C) 2.
After performing the read, if there are one or more messages in the RX
FIFO, the RXNEMP bit in the ISR is set. This bit can either be polled or
can generate an interrupt. 3. Repeat until the FIFO is empty. Extra
Design Consideration The AXI CAN cores requires an input register on the
RX line to avoid a potential error condition where multiple registers
receive different values resulting in error frames. This error condition
is rare; however, the work-around should be implemented in all cases. To
work around this issue, insert a register on the RX line clocked by
CAN_CLK with an initial value of '1'. This applies to all versions of
the AXI CAN cores. DS791 June 22, 2011 www.xilinx.com 35
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Design
Implementation Device and Package Selection (cid:129) The AXI CAN can be
implemented in an FPGA listed in the Supported Device Family field in
the LogiCORE IP Facts Table. Ensure that the device used has the
following attributes: (cid:129) The device is large enough to
accommodate the core, and (cid:129) It contains a sufficient number of
IOBs. Location Constraints No specific I/O location constraints.
Placement Constraints No specific placement constraints. Timing
Constraints The core has two different clock domains: S_AXI_ACLK and
CAN_CLK. The constraints given in the following sections can be used
with the CAN Controller. PERIOD Constraints for Clock Nets CAN_CLK The
clock provided to CAN_CLK must be constrained for a clock frequency of
less than or equal to 24 MHz, based on the input oscillator frequency.
\# Set the CAN_CLK constraints NET "CAN_CLK" TNM_NET = "CAN_CLK";
TIMESPEC "TS_CAN_CLK" = PERIOD "CAN_CLK" 40 ns HIGH 50%; S_AXI_ACLK The
clock provided to S_AXI_ACLK must be constrained for a clock frequency
of 100 MHz or less. \# Set the S_AXI_ACLK constraints \# This can be
relaxed based on the actual frequency NET "S_AXI_ACLK" TNM_NET =
"S_AXI_ACLK"; TIMESPEC "TS_S\_AXI_ACLK" = PERIOD "S_AXI_ACLK" 10 ns HIGH
50%; Timing Ignore Constraints For all the signals that cross clock
domains, the following timing ignore (TIG) constraints are specified in
the default UCF of the axi_can core. This default UCF is created in the
implementation directory, under the core's wrapper files' directory. \#
Timing Ignore constraint on all signals that cross from CAN_CLK domain
to S_AXI_ACLK domain TIMESPEC "TS_CAN_SYS_TIG" = FROM "CAN_CLK" TO
"S_AXI_ACLK" TIG; \# Timing Ignore constraint on all signals that cross
from S_AXI_ACLK domain to CAN_CLK domain TIMESPEC "TS_SYS_CAN_TIG" =
FROM "S_AXI_ACLK" TO "CAN_CLK" TIG; The user must ensure that these
default constraints are removed when both the clocks are driven from the
same net. DS791 June 22, 2011 www.xilinx.com 36 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) I/O
Constraints I/O Standards The pins that interface to the CAN PHY device
have a 3.3 volt signal level interface. The following constraints can be
used, provided the device I/O Banking rules are followed. \# Select the
I/O standards for the interface to the CAN PHY INST "CAN_PHY_TX"
IOSTANDARD = "LVTTL" INST "CAN_PHY_RX" IOSTANDARD = "LVTTL" Device
Utilization and Performance Benchmarks Core Performance Table40 shows
example performance and resource utilization benchmarks for the
Spartan®-6 FPGA (xc6slx45t-2-fgg484 device). Table 40: Performance and
Resource Utilization Benchmarks for the Spartan-6 FPGA
(xc6slx45t-2-fgg484) Parameter Values Device Resources F (MHz) MAX DS791
June 22, 2011 www.xilinx.com 37 ProductSpecification HTPD_XR_NAC_C
HTPD_XT_NAC_C FCA_MUN_NAC_C secilS spolF -pilF ecilS sTUL AXI F MAX 2 2
0 374 553 794 74.582 2 2 1 405 648 866 68.474 2 2 2 311 651 896 79.930 2
2 3 394 654 878 78.616 2 2 4 386 648 864 72.427 4 4 0 378 561 805 61.755
4 4 1 398 664 893 76.383 4 4 2 385 667 897 73.089 4 4 3 389 670 891
74.974 4 4 4 393 664 886 74.438 8 8 0 368 585 826 71.058 8 8 1 403 680
895 74.789 8 8 2 384 683 909 71.235 8 8 3 392 686 912 82.597 8 8 4 365
680 902 77.042 16 16 0 391 601 848 69.959 16 16 1 402 696 919 83.605 16
16 2 411 699 925 75.120 16 16 3 401 678 905 73.228 16 16 4 430 696 905
69.832

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Parameter
Values Device Resources F (MHz) MAX Table41 shows example performance
and resource utilization benchmarks for the Virtex®-6 FPGA
(xc6vlx130t-2-ff484 device). DS791 June 22, 2011 www.xilinx.com 38
ProductSpecification HTPD_XR_NAC_C HTPD_XT_NAC_C FCA_MUN_NAC_C secilS
spolF -pilF ecilS sTUL Table 40: Performance and Resource Utilization
Benchmarks for the Spartan-6 FPGA (xc6slx45t-2-fgg484) AXI F MAX 32 32 0
371 617 845 66.291 32 32 1 419 712 926 75.245 32 32 2 430 715 924 74.666
32 32 3 401 718 935 80.959 32 32 4 372 712 911 83.752 64 64 0 364 633
889 75.740 64 64 1 428 728 962 74.482 64 64 2 404 731 970 60.190 64 64 3
428 734 955 73.185 64 64 4 399 728 948 75.375

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 41:
Performance and Resource Utilization Benchmarks for Virtex-6 FPGA
(xc6vlx130t-2-ff484) Parameter Values Device Resources F (MHz) MAX DS791
June 22, 2011 www.xilinx.com 39 ProductSpecification HTPD_XR_NAC_C
HTPD_XT_NAC_C FCA_MUN_NAC_C secilS spolF -pilF ecilS sTUL AXI F MAX 2 2
0 263 555 799 200.321 2 2 1 287 642 874 181.422 2 2 2 314 651 879
206.313 2 2 3 305 654 875 194.590 2 2 4 311 652 864 172.176 4 4 0 284
563 805 174.490 4 4 1 284 658 896 183.688 4 4 2 312 667 895 187.336 4 4
3 280 670 893 193.911 4 4 4 310 668 877 168.549 8 8 0 290 587 822
187.301 8 8 1 269 674 901 196.117 8 8 2 328 683 916 184.298 8 8 3 299
686 905 197.161 8 8 4 271 684 897 170.999 16 16 0 284 602 843 200.965 16
16 1 294 690 922 183.655 16 16 2 327 699 925 179.404 16 16 3 295 678 899
196.541 16 16 4 280 700 918 176.149 32 32 0 271 618 860 193.911 32 32 1
257 706 906 165.317 32 32 2 296 715 920 190.006 32 32 3 311 718 924
194.515 32 32 4 325 717 914 167.420 64 64 0 269 634 884 200.682 64 64 1
305 722 949 174.155 64 64 2 293 731 942 191.571 64 64 3 309 734 944
192.234 64 64 4 299 732 934 182.882

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 42:
Performance and Resource Utilization Benchmarks for Virtex-7 FPGA
(xc7v285t-1-ffg1157) Parameter Values Device Resources F (MHz) MAX DS791
June 22, 2011 www.xilinx.com 40 ProductSpecification HTPD_XR_NAC_C
HTPD_XT_NAC_C FCA_MUN_NAC_C secilS spolF -pilF ecilS sTUL AXI F MAX 2 2
0 326 557 757 141.784 2 2 1 304 650 844 144.739 2 2 2 280 653 852
136.761 2 2 3 309 656 840 140.766 2 2 4 317 659 847 141.703 4 4 0 301
565 773 140.944 4 4 1 291 666 856 133.672 4 4 2 342 669 864 127.502 4 4
3 353 672 866 139.024 4 4 4 313 675 869 133.833 8 8 0 319 589 796
135.759 8 8 1 337 682 870 129.249 8 8 2 338 685 869 128.667 8 8 3 340
688 870 130.073 8 8 4 341 691 867 136.761 16 16 0 316 605 813 131.631 16
16 1 334 698 884 134.300 16 16 2 383 701 886 133.869 16 16 3 347 680 872
131.372 16 16 4 350 707 901 124.131 32 32 0 333 621 819 136.911 32 32 1
364 714 888 136.166 32 32 2 324 717 895 149.098 32 32 3 309 720 911
148.743 32 32 4 355 723 906 137.836 64 64 0 342 637 831 132.433 64 64 1
363 730 934 131.857 64 64 2 347 733 927 136.073 64 64 3 348 736 927
133.440 64 64 4 314 739 936 134.246

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 43:
Performance and Resource Utilization Benchmarks for Kintex-7 FPGA
(xc7k410t-1-ffg900) Parameter Values Device Resources F (MHz) MAX DS791
June 22, 2011 www.xilinx.com 41 ProductSpecification HTPD_XR_NAC_C
HTPD_XT_NAC_C FCA_MUN_NAC_C secilS spolF -pilF ecilS sTUL AXI F MAX 2 2
0 339 557 758 148.214 2 2 1 372 650 844 145.264 2 2 2 362 653 851
144.279 2 2 3 356 656 849 134.048 2 2 4 369 659 854 146.156 4 4 0 341
565 767 151.561 4 4 1 338 666 861 122.459 4 4 2 366 669 858 144.051 4 4
3 359 672 867 141.323 4 4 4 355 675 862 124.517 8 8 0 330 589 788
105.742 8 8 1 345 682 861 140.213 8 8 2 355 685 874 149.298 8 8 3 352
688 870 142.025 8 8 4 342 691 879 153.822 16 16 0 339 605 803 133.511 16
16 1 350 698 886 141.523 16 16 2 349 701 899 141.824 16 16 3 341 680 868
138.523 16 16 4 370 707 894 142.878 32 32 0 337 621 822 148.148 32 32 1
389 714 890 146.284 32 32 2 375 717 905 133.458 32 32 3 378 720 898
136.073 32 32 4 373 723 903 137.099 64 64 0 345 637 825 139.723 64 64 1
388 730 920 148.456 64 64 2 373 733 929 139.567 64 64 3 384 736 925
136.874 64 64 4 365 739 927 124.922

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Table 44:
Performance and Resource Utilization Benchmarks for Artix-7 FPGA
(xc7a355tdie-3) Parameter Values Device Resources F (MHz) MAX DS791 June
22, 2011 www.xilinx.com 42 ProductSpecification HTPD_XR_NAC_C
HTPD_XT_NAC_C FCA_MUN_NAC_C secilS spolF -pilF ecilS sTUL AXI F MAX 2 2
0 314 552 754 151.423 2 2 1 353 644 842 157.257 2 2 2 344 648 846
141.343 2 2 3 350 651 861 159.923 2 2 4 326 653 850 114.521 4 4 0 277
560 744 131.216 4 4 1 355 660 844 135.685 4 4 2 337 664 866 135.062 4 4
3 333 667 852 123.016 4 4 4 329 669 859 144.970 8 8 0 305 584 760
131.475 8 8 1 339 677 868 152.369 8 8 2 338 680 868 129.786 8 8 3 349
683 879 138.504 8 8 4 337 685 878 132.714 16 16 0 308 600 800 101.698 16
16 1 343 693 874 130.361 16 16 2 335 696 879 140.135 16 16 3 330 675 876
134.354 16 16 4 347 701 895 139.451 32 32 0 299 616 799 114.038 32 32 1
348 709 893 115.221 32 32 2 364 712 901 117.302 32 32 3 323 715 902
132.679 32 32 4 364 717 889 122.820 64 64 0 341 632 829 133.404 64 64 1
342 727 931 133.958 64 64 2 355 730 922 141.283 64 64 3 359 733 914
131.596 64 64 4 359 735 923 142.816

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) System
Performance To measure the system performance (F ) of this core, this
core was added as the Device Under Test (DUT) to MAX Spartan-6 and
Virtex-6 FPGA system as shown in Figure4. Because the AXI CAN core is
used with other design modules in the FPGA, the utilization and timing
numbers reported in this section are estimates only. When this core is
combined with other designs in the system, the utilization of FPGA
resources and timing of the design varies from the results reported
here. X-Ref Target - Figure 4 MicroBlaze AXI4 Domain Processor Domain
(M_AXI_IC) AXI DDR AXI4 (M_AXI_DC) Interconnect Memory Memory Controller
MicroBlaze AXI CDMA (M_AXI_DP) Device Under D_LMB Test (DUT) I_LMB (Low
Speed Slave) AXI4-Lite AXI INTC Interconnect AXI GPIO LEDs BRAM
Controller AXI UARTLite RS232 MDM AXI4-Lite Domain Figure 4:
Spartan-6/Virtex-6 FPGA System with the AXI CAN Core as the DUT DS6224
The target FPGA was then filled with logic to drive the LUT and block
RAM utilization to approximately 70% and the I/O utilization to
approximately 80%. Using the default tool options and the slowest speed
grade for the target FPGA, the resulting target F numbers are shown in
Table45. MAX Table 45: System Performance Target FPGA Target F (MHz) MAX
S6LXT45-2 90 V6LXT130-1 180 The target F is influenced by the exact
system and is provided for guidance. It is not a guaranteed value across
MAX all systems. DS791 June 22, 2011 www.xilinx.com 43
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Support
Xilinx provides technical support for this LogiCORE IP product when used
as described in the product documentation. Xilinx cannot guarantee
timing, functionality, or support of product if implemented in devices
that are not defined in the documentation, if customized beyond that
allowed in the product documentation, or if changes are made to any
section of the design labeled DO NOT MODIFY. Reference Documents 1. ISO
11898-1: Road Vehicles - Interchange of Digital Information - Controller
Area Network (CAN) for High-Speed Communication. 2. Controller Area
Network (CAN) version 2.0A and B Specification, Robert Bosch GmbH 3. AXI
Interconnect IP Data Sheet (DS768) See
www.xilinx.com/support/documentation/index.htm to find more Xilinx
documentation. Ordering Information This Xilinx LogiCORE IP module is
provided under the terms of the Xilinx Core Site License. The core is
generated using the Xilinx ISE® Embedded Edition software (EDK). For
full access to all core functionality in simulation and in hardware, you
must purchase a license for the core. Contact your local Xilinx sales
representative for information on pricing and availability of Xilinx
LogiCORE IP. For more information, visit the AXI CAN product web page.
Information about this and other Xilinx LogiCORE IP modules is available
at the Xilinx Intellectual Property page. For information on pricing and
availability of other Xilinx LogiCORE IP modules and software, contact
your local Xilinx sales representative. List of Acronyms Acronym Spelled
Out AFIR Acceptance Filter ID Register AFMR Acceptance Filter Mask
Register AFR Acceptance Filter Register BRPR Baud Rate Prescaler BSP Bit
Stream Processor BTL Bit Timing Logic BTR Bit Timing Register CAN
Controller Area Network CRC Cyclic Redundancy Check DCM Digital Clock
Manager DLC Data Length Code DUT Device Under Test DW Data Word ECR
Error Counter Register DS791 June 22, 2011 www.xilinx.com 44
ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Acronym
Spelled Out ESR Error Status Register FIFO First In First Out FPGA Field
Programmable Gate Array I Industrial I/O Input/Output ICR Interrupt
Clear Register ID Identifier IDR Identifier of the Received Message IER
Interrupt Enable Register IP Intellectual Property ISE Integrated
Software Environment ISO International Organization for Standardization
ISR Interrupt Status Register LLC Logical Link Control LSB Least
Significant Bit MAC Media Access Controller Mb/s Megabits per second MHz
Mega Hertz MSB Most Significant Bit MSR Mode Select Register RTR Remote
Transmission Request RX Reception SJW Synchronization Jump Width SOF
Start Of Frame SR Status Register SRR Software Reset Register TX
Transmission UAF Use Acceptance Filter VHDL VHSIC Hardware Description
Language (VHSIC an acronym for Very High-Speed Integrated Circuits) XPS
Xilinx Platform Studio (part of the EDK software) XST Xilinx Synthesis
Technology DS791 June 22, 2011 www.xilinx.com 45 ProductSpecification

LogiCORE IP AXI Controller Area Network (axi_can) (v1.03.a) Revision
History Date Version Revision First release of the core with AXI
interface support. The previous release of this document was 09/21/10
1.0 ds265. 09/21/10 1.0.1 Documentation only. Added inferred parameters
text on page 7. 06/22/11 2.0 Updated for core version 1.03.a and 13.2
tools. Added new supported architectures. Notice of Disclaimer The
information disclosed to you hereunder (the "Materials") is provided
solely for the selection and use of Xilinx products. To the maximum
extent permitted by applicable law: (1) Materials are made available "AS
IS" and with all faults, Xilinx hereby DISCLAIMS ALL WARRANTIES AND
CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO
WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY
PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether in
contract or tort, including negligence, or under any other theory of
liability) for any loss or damage of any kind or nature related to,
arising under, or in connection with, the Materials (including your use
of the Materials), including for any direct, indirect, special,
incidental, or consequential loss or damage (including loss of data,
profits, goodwill, or any type of loss or damage suffered as a result of
any action brought by a third party) even if such damage or loss was
reasonably foreseeable or Xilinx had been advised of the possibility of
the same. Xilinx assumes no obligation to correct any errors contained
in the Materials or to notify you of updates to the Materials or to
product specifications. You may not reproduce, modify, distribute, or
publicly display the Materials without prior written consent. Certain
products are subject to the terms and conditions of the Limited
Warranties which can be viewed at http://www.xilinx.com/warranty.htm; IP
cores may be subject to warranty and support terms contained in a
license issued to you by Xilinx. Xilinx products are not designed or
intended to be fail-safe or for use in any application requiring
fail-safe performance; you assume sole risk and liability for use of
Xilinx products in Critical Applications:
http://www.xilinx.com/warranty.htm\#critapps. DS791 June 22, 2011
www.xilinx.com 46 ProductSpecification
