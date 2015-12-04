#ifndef CYGONCE_HAL_PLF_IO_H
#define CYGONCE_HAL_PLF_IO_H
//=============================================================================
//
//      plf_io.h
//
//      Platform specific registers
//
//=============================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with eCos; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
// at http://sources.redhat.com/ecos/ecos-license/
// -------------------------------------------
//####ECOSGPLCOPYRIGHTEND####
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   jskov
// Contributors:jskov, gthomas
// Date:        2001-07-12
// Purpose:     AT91/EB40 platform specific registers
// Description: 
// Usage:       #include <cyg/hal/plf_io.h>
//
//####DESCRIPTIONEND####
//
//=============================================================================

// USART

#define AT91_USART0 0xFFFD0000
#define AT91_USART1 0xFFFCC000

#define AT91_US_CR  0x00  // Control register
#define AT91_US_CR_RxRESET (1<<2)
#define AT91_US_CR_TxRESET (1<<3)
#define AT91_US_CR_RxENAB  (1<<4)
#define AT91_US_CR_RxDISAB (1<<5)
#define AT91_US_CR_TxENAB  (1<<6)
#define AT91_US_CR_TxDISAB (1<<7)
#define AT91_US_CR_RSTATUS (1<<8)
#define AT91_US_MR  0x04  // Mode register
#define AT91_US_MR_CLOCK   4
#define AT91_US_MR_CLOCK_MCK  (0<<AT91_US_MR_CLOCK)
#define AT91_US_MR_CLOCK_MCK8 (1<<AT91_US_MR_CLOCK)
#define AT91_US_MR_CLOCK_SCK  (2<<AT91_US_MR_CLOCK)
#define AT91_US_MR_LENGTH  6
#define AT91_US_MR_LENGTH_5   (0<<AT91_US_MR_LENGTH)
#define AT91_US_MR_LENGTH_6   (1<<AT91_US_MR_LENGTH)
#define AT91_US_MR_LENGTH_7   (2<<AT91_US_MR_LENGTH)
#define AT91_US_MR_LENGTH_8   (3<<AT91_US_MR_LENGTH)
#define AT91_US_MR_SYNC    8
#define AT91_US_MR_SYNC_ASYNC (0<<AT91_US_MR_SYNC)
#define AT91_US_MR_SYNC_SYNC  (1<<AT91_US_MR_SYNC)
#define AT91_US_MR_PARITY  9
#define AT91_US_MR_PARITY_EVEN  (0<<AT91_US_MR_PARITY)
#define AT91_US_MR_PARITY_ODD   (1<<AT91_US_MR_PARITY)
#define AT91_US_MR_PARITY_SPACE (2<<AT91_US_MR_PARITY)
#define AT91_US_MR_PARITY_MARK  (3<<AT91_US_MR_PARITY)
#define AT91_US_MR_PARITY_NONE  (4<<AT91_US_MR_PARITY)
#define AT91_US_MR_PARITY_MULTI (6<<AT91_US_MR_PARITY)
#define AT91_US_MR_STOP   12
#define AT91_US_MR_STOP_1       (0<<AT91_US_MR_STOP)
#define AT91_US_MR_STOP_1_5     (1<<AT91_US_MR_STOP)
#define AT91_US_MR_STOP_2       (2<<AT91_US_MR_STOP)
#define AT91_US_MR_MODE   14
#define AT91_US_MR_MODE_NORMAL  (0<<AT91_US_MR_MODE)
#define AT91_US_MR_MODE_ECHO    (1<<AT91_US_MR_MODE)
#define AT91_US_MR_MODE_LOCAL   (2<<AT91_US_MR_MODE)
#define AT91_US_MR_MODE_REMOTE  (3<<AT91_US_MR_MODE)
#define AT91_US_MR_MODE9  17
#define AT91_US_MR_CLKO   18
#define AT91_US_IER 0x08  // Interrupt enable register
#define AT91_US_IER_RxRDY   (1<<0)  // Receive data ready
#define AT91_US_IER_TxRDY   (1<<1)  // Transmitter ready
#define AT91_US_IER_RxBRK   (1<<2)  // Break received
#define AT91_US_IER_ENDRX   (1<<3)  // Rx end
#define AT91_US_IER_ENDTX   (1<<4)  // Tx end
#define AT91_US_IER_OVRE    (1<<5)  // Rx overflow
#define AT91_US_IER_FRAME   (1<<6)  // Rx framing error
#define AT91_US_IER_PARITY  (1<<7)  // Rx parity
#define AT91_US_IER_TIMEOUT (1<<8)  // Rx timeout
#define AT91_US_IER_TxEMPTY (1<<9)  // Tx empty
#define AT91_US_IDR 0x0C  // Interrupt disable register
#define AT91_US_IMR 0x10  // Interrupt mask register
#define AT91_US_CSR 0x14  // Channel status register
#define AT91_US_CSR_RxRDY 0x01 // Receive data ready
#define AT91_US_CSR_TxRDY 0x02 // Transmit ready
#define AT91_US_RHR 0x18  // Receive holding register
#define AT91_US_THR 0x1C  // Transmit holding register
#define AT91_US_BRG 0x20  // Baud rate generator
#define AT91_US_RTO 0x24  // Receive time out
#define AT91_US_TTG 0x28  // Transmit timer guard

#define AT91_US_BAUD(baud) (CYGNUM_HAL_ARM_AT91_CLOCK_SPEED/(16*(baud)))

// PIO

#define AT91_PIO      0xFFFF0000

#define AT91_PIO_PER  0x00  // PIO enable
#define AT91_PIO_PDR  0x04  // PIO disable
#define AT91_PIO_PSR  0x08  // PIO status
#define AT91_PIO_OER  0x10  // Output enable
#define AT91_PIO_ODR  0x14  // Output disable
#define AT91_PIO_OSR  0x1C  // Output status register
#define AT91_PIO_IFER 0x20  // Input Filter enable
#define AT91_PIO_IFDR 0x24  // Input Filter disable
#define AT91_PIO_IFSR 0x28  // Input Filter status register
#define AT91_PIO_SODR 0x30  // Set out bits
#define AT91_PIO_CODR 0x34  // Clear out bits
#define AT91_PIO_ODSR 0x38  // Output data status register
#define AT91_PIO_IER  0x40  // Interrupt enable
#define AT91_PIO_IDR  0x44  // Interrupt disable
#define AT91_PIO_IMR  0x48  // Interrupt mask
#define AT91_PIO_ISR  0x4C  // Interrupt status register


// Advanced Interrupt Controller (AIC)

#define AT91_AIC      0xFFFFF000

#define AT91_AIC_SMR0   ((0*4)+0x000)
#define AT91_AIC_SMR1   ((1*4)+0x000)
#define AT91_AIC_SMR2   ((2*4)+0x000)
#define AT91_AIC_SMR3   ((3*4)+0x000)
#define AT91_AIC_SMR4   ((4*4)+0x000)
#define AT91_AIC_SMR5   ((5*4)+0x000)
#define AT91_AIC_SMR6   ((6*4)+0x000)
#define AT91_AIC_SMR7   ((7*4)+0x000)
#define AT91_AIC_SMR8   ((8*4)+0x000)
#define AT91_AIC_SMR9   ((9*4)+0x000)
#define AT91_AIC_SMR10  ((10*4)+0x000)
#define AT91_AIC_SMR11  ((11*4)+0x000)
#define AT91_AIC_SMR12  ((12*4)+0x000)
#define AT91_AIC_SMR13  ((13*4)+0x000)
#define AT91_AIC_SMR14  ((14*4)+0x000)
#define AT91_AIC_SMR15  ((15*4)+0x000)
#define AT91_AIC_SMR16  ((16*4)+0x000)
#define AT91_AIC_SMR17  ((17*4)+0x000)
#define AT91_AIC_SMR18  ((18*4)+0x000)
#define AT91_AIC_SMR19  ((19*4)+0x000)
#define AT91_AIC_SMR20  ((20*4)+0x000)
#define AT91_AIC_SMR21  ((21*4)+0x000)
#define AT91_AIC_SMR22  ((22*4)+0x000)
#define AT91_AIC_SMR23  ((23*4)+0x000)
#define AT91_AIC_SMR24  ((24*4)+0x000)
#define AT91_AIC_SMR25  ((25*4)+0x000)
#define AT91_AIC_SMR26  ((26*4)+0x000)
#define AT91_AIC_SMR27  ((27*4)+0x000)
#define AT91_AIC_SMR28  ((28*4)+0x000)
#define AT91_AIC_SMR29  ((29*4)+0x000)
#define AT91_AIC_SMR30  ((30*4)+0x000)
#define AT91_AIC_SMR31  ((31*4)+0x000)
#define AT91_AIC_SMR_LEVEL_LOW  (0<<5)
#define AT91_AIC_SMR_LEVEL_HI   (2<<5)
#define AT91_AIC_SMR_EDGE_NEG   (1<<5)
#define AT91_AIC_SMR_EDGE_POS   (3<<5)
#define AT91_AIC_SMR_PRIORITY   0x07
#define AT91_AIC_SVR0   ((0*4)+0x080)
#define AT91_AIC_SVR1   ((1*4)+0x080)
#define AT91_AIC_SVR2   ((2*4)+0x080)
#define AT91_AIC_SVR3   ((3*4)+0x080)
#define AT91_AIC_SVR4   ((4*4)+0x080)
#define AT91_AIC_SVR5   ((5*4)+0x080)
#define AT91_AIC_SVR6   ((6*4)+0x080)
#define AT91_AIC_SVR7   ((7*4)+0x080)
#define AT91_AIC_SVR8   ((8*4)+0x080)
#define AT91_AIC_SVR9   ((9*4)+0x080)
#define AT91_AIC_SVR10  ((10*4)+0x080)
#define AT91_AIC_SVR11  ((11*4)+0x080)
#define AT91_AIC_SVR12  ((12*4)+0x080)
#define AT91_AIC_SVR13  ((13*4)+0x080)
#define AT91_AIC_SVR14  ((14*4)+0x080)
#define AT91_AIC_SVR15  ((15*4)+0x080)
#define AT91_AIC_SVR16  ((16*4)+0x080)
#define AT91_AIC_SVR17  ((17*4)+0x080)
#define AT91_AIC_SVR18  ((18*4)+0x080)
#define AT91_AIC_SVR19  ((19*4)+0x080)
#define AT91_AIC_SVR20  ((20*4)+0x080)
#define AT91_AIC_SVR21  ((21*4)+0x080)
#define AT91_AIC_SVR22  ((22*4)+0x080)
#define AT91_AIC_SVR23  ((23*4)+0x080)
#define AT91_AIC_SVR24  ((24*4)+0x080)
#define AT91_AIC_SVR25  ((25*4)+0x080)
#define AT91_AIC_SVR26  ((26*4)+0x080)
#define AT91_AIC_SVR27  ((27*4)+0x080)
#define AT91_AIC_SVR28  ((28*4)+0x080)
#define AT91_AIC_SVR29  ((29*4)+0x080)
#define AT91_AIC_SVR30  ((30*4)+0x080)
#define AT91_AIC_SVR31  ((31*4)+0x080)
#define AT91_AIC_IVR    0x100
#define AT91_AIC_FVR    0x104
#define AT91_AIC_ISR    0x108
#define AT91_AIC_IPR    0x10C
#define AT91_AIC_IMR    0x110
#define AT91_AIC_CISR   0x114
#define AT91_AIC_IECR   0x120
#define AT91_AIC_IDCR   0x124
#define AT91_AIC_ICCR   0x128
#define AT91_AIC_ISCR   0x12C
#define AT91_AIC_EOI    0x130
#define AT91_AIC_SVR    0x134

// Timer / counter

#define AT91_TC         0xFFFE0000
#define AT91_TC_TC0     0x00
#define AT91_TC_CCR     0x00
#define AT91_TC_CCR_CLKEN  0x01
#define AT91_TC_CCR_CLKDIS 0x02
#define AT91_TC_CCR_TRIG   0x04
#define AT91_TC_CMR     0x04
// Capture mode definitions
#define AT91_TC_CMR_CLKS   0
#define AT91_TC_CMR_CLKS_MCK2      (0<<0)
#define AT91_TC_CMR_CLKS_MCK8      (1<<0)
#define AT91_TC_CMR_CLKS_MCK32     (2<<0)
#define AT91_TC_CMR_CLKS_MCK128    (3<<0)
#define AT91_TC_CMR_CLKS_MCK1024   (4<<0)
#define AT91_TC_CMR_CLKS_XC0       (5<<0)
#define AT91_TC_CMR_CLKS_XC1       (6<<0)
#define AT91_TC_CMR_CLKS_XC2       (7<<0)
#define AT91_TC_CMR_CLKI           (1<<3)
#define AT91_TC_CMR_BURST_NONE     (0<<4)
#define AT91_TC_CMR_BURST_XC0      (1<<4)
#define AT91_TC_CMR_BURST_XC1      (2<<4)
#define AT91_TC_CMR_BURST_XC2      (3<<4)
#define AT91_TC_CMR_LDBSTOP        (1<<6)
#define AT91_TC_CMR_LDBDIS         (1<<7)
#define AT91_TC_CMR_TRIG_NONE      (0<<8)
#define AT91_TC_CMR_TRIG_NEG       (1<<8)
#define AT91_TC_CMR_TRIG_POS       (2<<8)
#define AT91_TC_CMR_TRIG_BOTH      (3<<8)
#define AT91_TC_CMR_EXT_TRIG_TIOB  (0<<10)
#define AT91_TC_CMR_EXT_TRIG_TIOA  (1<<10)
#define AT91_TC_CMR_CPCTRG         (1<<14)
#define AT91_TC_CMR_LDRA_NONE      (0<<16)
#define AT91_TC_CMR_LDRA_TIOA_NEG  (1<<16)
#define AT91_TC_CMR_LDRA_TIOA_POS  (2<<16)
#define AT91_TC_CMR_LDRA_TIOA_BOTH (3<<16)
#define AT91_TC_CMR_LDRB_NONE      (0<<16)
#define AT91_TC_CMR_LDRB_TIOA_NEG  (1<<16)
#define AT91_TC_CMR_LDRB_TIOA_POS  (2<<16)
#define AT91_TC_CMR_LDRB_TIOA_BOTH (3<<16)
// Waveform mode definitions [missing]
#define AT91_TC_CV      0x10
#define AT91_TC_RA      0x14
#define AT91_TC_RB      0x18
#define AT91_TC_RC      0x1C
#define AT91_TC_SR      0x20
#define AT91_TC_SR_COVF    (1<<0)  // Counter overrun
#define AT91_TC_SR_LOVR    (1<<1)  // Load overrun
#define AT91_TC_SR_CPA     (1<<2)  // RA compare
#define AT91_TC_SR_CPB     (1<<3)  // RB compare
#define AT91_TC_SR_CPC     (1<<4)  // RC compare
#define AT91_TC_SR_LDRA    (1<<5)  // Load A status
#define AT91_TC_SR_LDRB    (1<<6)  // Load B status
#define AT91_TC_SR_EXT     (1<<7)  // External trigger
#define AT91_TC_SR_CLKSTA  (1<<16) // Clock enable/disable status
#define AT91_TC_SR_MTIOA   (1<<17) // TIOA mirror
#define AT91_TC_SR_MTIOB   (1<<18) // TIOB mirror
#define AT91_TC_IER     0x24
#define AT91_TC_IER_COVF   (1<<0)  // Counter overrun
#define AT91_TC_IER_LOVR   (1<<1)  // Load overrun
#define AT91_TC_IER_CPA    (1<<2)  // RA compare
#define AT91_TC_IER_CPB    (1<<3)  // RB compare
#define AT91_TC_IER_CPC    (1<<4)  // RC compare
#define AT91_TC_IER_LDRA   (1<<5)  // Load A status
#define AT91_TC_IER_LDRB   (1<<6)  // Load B status
#define AT91_TC_IER_EXT    (1<<7)  // External trigger
#define AT91_TC_IDR     0x28
#define AT91_TC_IMR     0x2C
#define AT91_TC_TC1     0x40
#define AT91_TC_TC2     0x80
#define AT91_TC_BCR     0xC0
#define AT91_TC_BCR_SYNC   0x01
#define AT91_TC_BMR     0xC4

// External Bus Interface

#define AT91_EBI        0xFFE00000  // Base

#define AT91_EBI_CSR0   0x00    // Chip selects 0 - 7
#define AT91_EBI_CSR1   0x04
#define AT91_EBI_CSR2   0x08
#define AT91_EBI_CSR3   0x0C
#define AT91_EBI_CSR4   0x10
#define AT91_EBI_CSR5   0x14
#define AT91_EBI_CSR6   0x18
#define AT91_EBI_CSR7   0x1C

#define AT91_EBI_RCR    0x20       // Reset control
#define AT91_EBI_MCR    0x24       // Memory control

#define AT91_EBI_CSEN              (1<<13)  // Chip Select enable
#define AT91_EBI_BAT_BYTE_WRITE    (0<<12)  // Byte write access
#define AT91_EBI_BAT_BYTE_SELECT   (1<<12)  // Byte select access type
#define AT91_EBI_TDF0              (0<<9)   // 0 cycles of data float time
#define AT91_EBI_TDF1              (1<<9)   // 1 
#define AT91_EBI_TDF2              (2<<9)   // 2, etc
#define AT91_EBI_TDF3              (3<<9)   //
#define AT91_EBI_TDF4              (4<<9)   //
#define AT91_EBI_TDF5              (5<<9)   //
#define AT91_EBI_TDF6              (6<<9)   //
#define AT91_EBI_TDF7              (7<<9)   //

#define AT91_EBI_PAGES_1M          (0<<7)   // 1MByte page size
#define AT91_EBI_PAGES_4M          (1<<7)   // 4MByte page size
#define AT91_EBI_PAGES_16M         (2<<7)   // 16MByte page size
#define AT91_EBI_PAGES_64M         (3<<7)   // 64MByte page size

#define AT91_EBI_WSE               (1<<5)   // Wait State enable

#define AT91_EBI_NWS_1             (0<<2)   // 1 wait state
#define AT91_EBI_NWS_2             (1<<2)   // 1 wait state
#define AT91_EBI_NWS_3             (2<<2)   // 1 wait state
#define AT91_EBI_NWS_4             (3<<2)   // 1 wait state
#define AT91_EBI_NWS_5             (4<<2)   // 1 wait state
#define AT91_EBI_NWS_6             (5<<2)   // 1 wait state
#define AT91_EBI_NWS_7             (6<<2)   // 1 wait state
#define AT91_EBI_NWS_8             (7<<2)   // 1 wait state

#define AT91_EBI_DBW_8             (2<<0)   // 8-bit data bus width
#define AT91_EBI_DBW_16            (1<<0)   // 16-bit data bus width

#define AT91_EBI_RCB               (1<<0)   // Remap command bit

#define AT91_EBI_ALE_16M           (0<<0)   // Address line enable: A20,A21,A22,A23
#define AT91_EBI_ALE_8M            (4<<0)   //   "   "  A20,A21,A22 CS4
#define AT91_EBI_ALE_4M            (5<<0)   //   "   "  A20,A21     CS4,CS5
#define AT91_EBI_ALE_2M            (6<<0)   //   "   "  A20         CS4,CS5,CS6
#define AT91_EBI_ALE_1M            (7<<0)   //   "   "              CS4,CS5,CS6,CS7

#define AT91_EBI_DRP_STANDARD      (0<<4)   // Standard data read protocol
#define AT91_EBI_DRP_EARLY         (1<<4)   // Early data read protocol



// Power Savings control

#define AT91_PS         0xFFFF4000
#define AT91_PS_CR        0x000    // Control
#define AT91_PS_PCER      0x004    // Peripheral clock enable
#define AT91_PS_PCDR      0x004    // Peripheral clock disable
#define AT91_PS_PCSR      0x004    // Peripheral clock status

// Watchdog

#define AT91_WD             0xFFFF8000
#define AT91_WD_OMR         0x00
#define AT91_WD_OMR_WDEN    0x00000001
#define AT91_WD_OMR_RSTEN   0x00000002
#define AT91_WD_OMR_IRQEN   0x00000004
#define AT91_WD_OMR_EXTEN   0x00000008
#define AT91_WD_OMR_OKEY    (0x00000234 << 4)
#define AT91_WD_CMR         0x04
#define AT91_WD_CMR_WDCLKS  0x00000003
#define AT91_WD_CMR_HPCV    0x0000003C
#define AT91_WD_CMR_CKEY    (0x0000006E << 7)
#define AT91_WD_CR          0x08
#define AT91_WD_CR_RSTKEY   0x0000C071
#define AT91_WD_SR          0x0C
#define AT91_WD_SR_WDOVF    0x00000001


//-----------------------------------------------------------------------------
// end of plf_io.h
#endif // CYGONCE_HAL_PLF_IO_H
