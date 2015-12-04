#ifndef CYGONCE_HAL_IDT32334SIO_H
#define CYGONCE_HAL_IDT32334SIO_H

/*=============================================================================
//
//      idt32334sio.h
//
//      IDT 32334 serial I/O definitions
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
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    Tim Michals
// Contributors: nickg
// Date:         2003-02-13
// Purpose:      IDT 32334 serial I/O definitions
// Description:  
// Usage:        
//####DESCRIPTIONEND####
//========================================================================*/

// Interrupt Enable Register
#define IER_RCV 0x01
#define IER_XMT 0x02
#define IER_LS  0x04
#define IER_MS  0x08

// Line Control Register
#define LCR_WL5 0x00    // Word length
#define LCR_WL6 0x01
#define LCR_WL7 0x02
#define LCR_WL8 0x03
#define LCR_SB1 0x00    // Number of stop bits
#define LCR_SB1_5 0x04  // 1.5 -> only valid with 5 bit words
#define LCR_SB2 0x04
#define LCR_PN  0x00    // Parity mode - none
#define LCR_PE  0x0C    // Parity mode - even
#define LCR_PO  0x08    // Parity mode - odd
#define LCR_PM  0x28    // Forced "mark" parity
#define LCR_PS  0x38    // Forced "space" parity
#define LCR_DL  0x80    // Enable baud rate latch

// Line Status Register
#define LSR_RSR 0x01
#define LSR_THE 0x20

// Modem Control Register
#define MCR_DTR 0x01
#define MCR_RTS 0x02
#define MCR_INT 0x08   // Enable interrupts

// Interrupt status register
#define ISR_Tx  0x02
#define ISR_Rx  0x04

// FIFO control register
#define FCR_ENABLE     0x01
#define FCR_CLEAR_RCVR 0x02
#define FCR_CLEAR_XMIT 0x04



////////////////////////////////////////////////////////////
// Clean this up.

//-----------------------------------------------------------------------------
// There are two serial ports.
#define CMA_SER_16550_BASE_A    0xb8000803	// port A
#define CMA_SER_16550_BASE_B    0xb8000823 	// port B
#define SER_16550_BASE CMA_SER_16550_BASE_A

//-----------------------------------------------------------------------------
// Define the serial registers. The Cogent board is equipped with a 16552
// serial chip.
#define SER_16550_RBR 0x00   // receiver buffer register, read, dlab = 0
#define SER_16550_THR 0x00   // transmitter holding register, write, dlab = 0
#define SER_16550_DLL 0x00   // divisor latch (LS), read/write, dlab = 1
#define SER_16550_IER 0x04   // interrupt enable register, read/write, dlab = 0
#define SER_16550_DLM 0x04   // divisor latch (MS), read/write, dlab = 1
#define SER_16550_IIR 0x08   // interrupt identification reg, read, dlab = 0
#define SER_16550_FCR 0x08   // fifo control register, write, dlab = 0
#define SER_16550_AFR 0x10   // alternate function reg, read/write, dlab = 1
#define SER_16550_LCR 0x0c   // line control register, read/write
#define SER_16550_MCR 0x10   // modem control register, read/write
#define SER_16550_LSR 0x14   // line status register, read
#define SER_16550_MSR 0x18   // modem status register, read
#define SER_16550_SCR 0x1C   // scratch pad register

// The interrupt enable register bits.
#define SIO_IER_ERDAI   0x01            // enable received data available irq
#define SIO_IER_ETHREI  0x02            // enable THR empty interrupt
#define SIO_IER_ELSI    0x04            // enable receiver line status irq
#define SIO_IER_EMSI    0x08            // enable modem status interrupt

// The interrupt identification register bits.
#define SIO_IIR_IP      0x01            // 0 if interrupt pending
#define SIO_IIR_ID_MASK 0x0e            // mask for interrupt ID bits

// The line status register bits.
#define SIO_LSR_DR      0x01            // data ready
#define SIO_LSR_OE      0x02            // overrun error
#define SIO_LSR_PE      0x04            // parity error
#define SIO_LSR_FE      0x08            // framing error
#define SIO_LSR_BI      0x10            // break interrupt
#define SIO_LSR_THRE    0x20            // transmitter holding register empty
#define SIO_LSR_TEMT    0x40            // transmitter register empty
#define SIO_LSR_ERR     0x80            // any error condition

// The modem status register bits.
#define SIO_MSR_DCTS  0x01              // delta clear to send
#define SIO_MSR_DDSR  0x02              // delta data set ready
#define SIO_MSR_TERI  0x04              // trailing edge ring indicator
#define SIO_MSR_DDCD  0x08              // delta data carrier detect
#define SIO_MSR_CTS   0x10              // clear to send
#define SIO_MSR_DSR   0x20              // data set ready
#define SIO_MSR_RI    0x40              // ring indicator
#define SIO_MSR_DCD   0x80              // data carrier detect

// The line control register bits.
#define SIO_LCR_WLS0   0x01             // word length select bit 0
#define SIO_LCR_WLS1   0x02             // word length select bit 1
#define SIO_LCR_STB    0x04             // number of stop bits
#define SIO_LCR_PEN    0x08             // parity enable
#define SIO_LCR_EPS    0x10             // even parity select
#define SIO_LCR_SP     0x20             // stick parity
#define SIO_LCR_SB     0x40             // set break
#define SIO_LCR_DLAB   0x80             // divisor latch access bit

// The FIFO control register
#define SIO_FCR_FCR0   0x01             // enable xmit and rcvr fifos
#define SIO_FCR_FCR1   0x02             // clear RCVR FIFO
#define SIO_FCR_FCR2   0x04             // clear XMIT FIFO
/////////////////////////////////////////


static unsigned char select_word_length[] = {
    LCR_WL5,    // 5 bits / word (char)
    LCR_WL6,
    LCR_WL7,
    LCR_WL8
};

static unsigned char select_stop_bits[] = {
    0,
    LCR_SB1,    // 1 stop bit
    LCR_SB1_5,  // 1.5 stop bit
    LCR_SB2     // 2 stop bits
};

static unsigned char select_parity[] = {
    LCR_PN,     // No parity
    LCR_PE,     // Even parity
    LCR_PO,     // Odd parity
    LCR_PM,     // Mark parity
    LCR_PS,     // Space parity
};

// FIXME: calc all properly
// The Cogent board has a 3.6864 MHz crystal
static unsigned short select_baud[] = {
    0,    // Unused
    4608, // 50
    0,    // 75
    2094, // 110
    0,    // 134.5
    1536, // 150
    0,    // 200
    768,  // 300
    384,  // 600
    182,  // 1200
    0,    // 1800
    96,   // 2400
    0,    // 3600
    48,   // 4800
    32,   // 7200
    24,   // 9600
    16,   // 14400
    12,   // 19200
    6,    // 38400
    4,    // 57600
    2,    // 115200
    0,    // 230400
};


#define NS16550_XTAL_FREQ       (75 * 1000000)
#define IDT_BAUD_RATE			115200

#endif /* CYGONCE_HAL_IDT32334SIO_H */
/*---------------------------------------------------------------------------*/
/* end of idt32334sio.h                                                      */
