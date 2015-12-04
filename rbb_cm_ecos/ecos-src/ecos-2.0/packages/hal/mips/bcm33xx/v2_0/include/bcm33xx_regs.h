//==========================================================================
//
// Copyright (c) 2003-2010 Broadcom Corporation
//
// This file being released as part of eCos, the Embedded Configurable Operating System.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2, available at 
// http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
//
// As a special exception, if you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
//==========================================================================
//
//      hal/mips/bcm33xx/includebcm33xx_serial.h
//
//      Broadcom 33xx register definitions.
//
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    msieweke
// Contributors: 
// Date:         2003-08-08
// Purpose:      Broadcom 33xx register definitions.
// Description:  Broadcom 33xx register definitions.
//####DESCRIPTIONEND####
//==========================================================================

#ifndef BCM33XX_REGS_H
#define BCM33XX_REGS_H

// ----------------------------------------------------------------------------
// Broadcom 33xx UART registers.
// ----------------------------------------------------------------------------

typedef struct Uart
{
    cyg_uint8   unused0;
    cyg_uint8   control;
        #define BRGEN           0x80
        #define TXEN            0x40
        #define RXEN            0x20
        #define LOOPBK          0x10
        #define TXPARITYEN      0x08
        #define TXPARITYEVEN    0x04
        #define RXPARITYEN      0x02
        #define RXPARITYEVEN    0x01

    cyg_uint8   config;
        #define XMITBREAK   0x40
        #define BITS5SYM    0x00
        #define BITS6SYM    0x10
        #define BITS7SYM    0x20
        #define BITS8SYM    0x30
        #define ONESTOP     0x07
        #define ONEPTFIVESTOP     0x0b
        #define TWOSTOP     0x0f

    cyg_uint8   fifoctl;
        #define RSTTXFIFOS  0x80
        #define RSTRXFIFOS  0x40

    cyg_uint32  baudword;
    cyg_uint8   txf_levl;
    cyg_uint8   rxf_levl;
    cyg_uint8   fifocfg;
    cyg_uint8   prog_out;
    cyg_uint32  DeltaIP;
    cyg_uint16  intEnableMask;
    cyg_uint16  intStatus;
        #define DELTAIP     0x0001
        #define TXUNDERR    0x0002
        #define TXOVFERR    0x0004
        #define TXFIFOTHOLD 0x0008
        #define TXREADLATCH 0x0010
        #define TXFIFOEMT   0x0020
        #define RXUNDERR    0x0040
        #define RXOVFERR    0x0080
        #define RXTIMEOUT   0x0100
        #define RXFIFOFULL  0x0200
        #define RXFIFOTHOLD 0x0400
        #define RXFIFONE    0x0800
        #define RXFRAMERR   0x1000
        #define RXPARERR    0x2000
        #define RXBRK       0x4000

    cyg_uint16  unused2;
    cyg_uint16  Data;
} Uart;

#if 1
// ----------------------------------------------------------------------------
// Interrupt Controller registers.
// ----------------------------------------------------------------------------

typedef struct IntControl {
    cyg_uint32  RevID;
    cyg_uint16  unused0;
    cyg_uint16  unused1;
    cyg_uint16  unused2;
    cyg_uint8   unused3;
    cyg_uint8   unused4;
    cyg_uint32  IrqEnableMask;
        #define UART0IRQ        0x00000004
    cyg_uint32  IrqStatus;
    cyg_uint8   unused5;
    cyg_uint8   unused6;
    cyg_uint8   extIrqMskandDetClr;
    cyg_uint8   unused7;

} IntControl;

// ----------------------------------------------------------------------------
// Timer control registers.
// ----------------------------------------------------------------------------
typedef struct Timer {
    cyg_uint16	unused0;
    cyg_uint8	unused1;
    cyg_uint8	unused2;
    cyg_uint32	unused3;
    cyg_uint32	unused4;
    cyg_uint32	unused5;
    cyg_uint32	unused6;
    cyg_uint32	unused7;
    cyg_uint32	unused8;
    cyg_uint32	unused9;
    cyg_uint32	WatchDogCtl;
    cyg_uint32	unused10;
} Timer;
#endif


// ----------------------------------------------------------------------------
// SDRAM Controller registers.
// ----------------------------------------------------------------------------

typedef struct Sdram
{
    cyg_uint32  unused0;
    cyg_uint32  unused1;
    cyg_uint32  unused2;
    cyg_uint32  memoryBase;
        #define SD_MEMSIZE_MASK     0x00000007

} Sdram;

// ----------------------------------------------------------------------------
// Define locations for registers
// ----------------------------------------------------------------------------

extern volatile IntControl *INTC;
extern volatile Uart       *UART;
extern volatile Uart       *UART1;
extern volatile Sdram      *SDRAM;
extern volatile Timer      *TIMER;
extern volatile cyg_uint32 *INTC_IrqStatus;
extern volatile cyg_uint32 *INTC_IrqEnableMask;

#define BCM_INTC_BASE           0xfffe0000  // interrupt controller registers
// Some BCM33xx chips have dual UARTs, but we're only using one.
#define BCM_UART0_BASE          0xfffe0300  // UART0 registers
#define BCM_UART1_BASE          0xfffe0320  // UART1 registers
#define BCM_SDRAM_BASE          0xfffe2300  // SDRAM control registers
#define BCM_TIMER_BASE          0xfffe0200  // Timer control registers


#endif // !defined( BCM33XX_REGS_H )
