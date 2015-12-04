//==========================================================================
//
//      bcm33xx_serial.c
//
//      Serial device driver for Broadcom 33xx reference platform on-chip serial devices
//
//==========================================================================
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
// Author(s):    msieweke, based on mips idt driver by tmichals, based on driver by dmoseley, based on POWERPC driver by jskov
// Contributors: gthomas, jskov, dmoseley, tmichals
// Date:         2003-07-02
// Purpose:      Broadcom 33xx reference platform serial device driver
// Description:  Broadcom 33xx serial device driver
//
//
//####DESCRIPTIONEND####
//
//==========================================================================
// =========================================================================
// Portions of this software Copyright (c) 2003-2010 Broadcom Corporation
// =========================================================================

#include <pkgconf/io_serial.h>
#include <pkgconf/io.h>

#include <cyg/io/io.h>
#include <cyg/hal/hal_intr.h>
#include <cyg/io/devtab.h>
#include <cyg/infra/diag.h>
#include <cyg/io/serial.h>

#ifdef CYGPKG_IO_SERIAL_MIPS_BCM33XX

#include "bcm33xx_serial.h"

cyg_uint8 gSerialPortEnabled0 __attribute__ ((weak)) = 1;
cyg_uint8 gSerialPortEnabled1 __attribute__ ((weak)) = 1;

#define UART0_IRQ_BIT       2
#define UART1_IRQ_BIT       3
CYG_WORD irq_bits[]  __attribute__ ((weak)) = { UART0_IRQ_BIT, UART1_IRQ_BIT };

typedef struct bcm33xx_serial_info {
    CYG_ADDRWORD   base;
    CYG_WORD       channelNumber;
    void           (*break_callout)(void);
} bcm33xx_serial_info;

static bool          bcm33xx_serial_init(struct cyg_devtab_entry *tab);
static bool          bcm33xx_serial_putc(serial_channel *chan, unsigned char c);
static Cyg_ErrNo     bcm33xx_serial_lookup(struct cyg_devtab_entry **tab, 
                                   struct cyg_devtab_entry *sub_tab,
                                   const char *name);
static unsigned char bcm33xx_serial_getc(serial_channel *chan);
static Cyg_ErrNo     bcm33xx_serial_set_config(serial_channel *chan, cyg_uint32 key,
                                         const void *xbuf, cyg_uint32 *len);
static void          bcm33xx_serial_start_xmit(serial_channel *chan);
static void          bcm33xx_serial_stop_xmit(serial_channel *chan);

static void          bcm33xx_serial_ISR(void * data);

       void          bcm33xx_default_break_callout( void ) __attribute__(( weak ));

static SERIAL_FUNS(bcm33xx_serial_funs, 
                   bcm33xx_serial_putc, 
                   bcm33xx_serial_getc,
                   bcm33xx_serial_set_config,
                   bcm33xx_serial_start_xmit,
                   bcm33xx_serial_stop_xmit
    );

static bcm33xx_serial_info bcm33xx_serial_info0 = { 0, 0, bcm33xx_default_break_callout };

#if CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_A_BUFSIZE > 0
static unsigned char bcm33xx_serial_out_buf0[CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_A_BUFSIZE];
static unsigned char bcm33xx_serial_in_buf0[CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_A_BUFSIZE];

static SERIAL_CHANNEL_USING_INTERRUPTS(bcm33xx_serial_channel0,
                                       bcm33xx_serial_funs, 
                                       bcm33xx_serial_info0,
                                       CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_A_BAUD),
                                       CYG_SERIAL_STOP_DEFAULT,
                                       CYG_SERIAL_PARITY_DEFAULT,
                                       CYG_SERIAL_WORD_LENGTH_DEFAULT,
                                       CYG_SERIAL_FLAGS_DEFAULT,
                                       &bcm33xx_serial_out_buf0[0], 
                                       sizeof(bcm33xx_serial_out_buf0),
                                       &bcm33xx_serial_in_buf0[0], 
                                       sizeof(bcm33xx_serial_in_buf0)
    );
#else
static SERIAL_CHANNEL(bcm33xx_serial_channel0,
                      bcm33xx_serial_funs, 
                      bcm33xx_serial_info0,
                      CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_A_BAUD),
                      CYG_SERIAL_STOP_DEFAULT,
                      CYG_SERIAL_PARITY_DEFAULT,
                      CYG_SERIAL_WORD_LENGTH_DEFAULT,
                      CYG_SERIAL_FLAGS_DEFAULT
    );
#endif

DEVTAB_ENTRY(bcm33xx_serial_io0, 
             CYGDAT_IO_SERIAL_MIPS_BCM33XX_SERIAL_A_NAME,
             0,                 // Does not depend on a lower level interface
             &cyg_io_serial_devio, 
             bcm33xx_serial_init, 
             bcm33xx_serial_lookup,     // Serial driver may need initializing
             &bcm33xx_serial_channel0
    );

#ifdef CYGPKG_IO_SERIAL_MIPS_BCM33XX_SERIAL_B

static bcm33xx_serial_info bcm33xx_serial_info1 = { 0, 1, NULL };

#if CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_B_BUFSIZE > 0
static unsigned char bcm33xx_serial_out_buf1[CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_B_BUFSIZE];
static unsigned char bcm33xx_serial_in_buf1[CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_B_BUFSIZE];

static SERIAL_CHANNEL_USING_INTERRUPTS(bcm33xx_serial_channel1,
                                       bcm33xx_serial_funs, 
                                       bcm33xx_serial_info1,
                                       CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_B_BAUD),
                                       CYG_SERIAL_STOP_DEFAULT,
                                       CYG_SERIAL_PARITY_DEFAULT,
                                       CYG_SERIAL_WORD_LENGTH_DEFAULT,
                                       CYG_SERIAL_FLAGS_DEFAULT,
                                       &bcm33xx_serial_out_buf1[0], 
                                       sizeof(bcm33xx_serial_out_buf1),
                                       &bcm33xx_serial_in_buf1[0], 
                                       sizeof(bcm33xx_serial_in_buf1)
    );
#else
static SERIAL_CHANNEL(bcm33xx_serial_channel1,
                      bcm33xx_serial_funs, 
                      bcm33xx_serial_info1,
                      CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_MIPS_BCM33XX_SERIAL_B_BAUD),
                      CYG_SERIAL_STOP_DEFAULT,
                      CYG_SERIAL_PARITY_DEFAULT,
                      CYG_SERIAL_WORD_LENGTH_DEFAULT,
                      CYG_SERIAL_FLAGS_DEFAULT
    );
#endif

DEVTAB_ENTRY(bcm33xx_serial_io1, 
             CYGDAT_IO_SERIAL_MIPS_BCM33XX_SERIAL_B_NAME,
             0,                 // Does not depend on a lower level interface
             &cyg_io_serial_devio, 
             bcm33xx_serial_init, 
             bcm33xx_serial_lookup,     // Serial driver may need initializing
             &bcm33xx_serial_channel1
    );
#endif // CYGPKG_IO_SERIAL_MIPS_BCM33XX_SERIAL_B


extern cyg_uint32 PeripheralClockFrequency;

void WatchDogKernelShow(cyg_uint32 taskId) __attribute__ ((weak));

void bcm33xx_default_break_callout( void )
{
    WatchDogKernelShow((cyg_uint32)cyg_thread_self());
}

// Internal function to actually configure the hardware to desired baud rate, etc.
static bool
bcm33xx_serial_config_port(serial_channel    *chan, 
                           cyg_serial_info_t *new_config, 
                           bool               initialize)
{
    bcm33xx_serial_info *bcm33xx_chan   = (bcm33xx_serial_info *) chan->dev_priv;
    volatile Uart       *UART           = (Uart *)                bcm33xx_chan->base;
    int                  baud_rate;
    cyg_uint32           baud_divisor;
    cyg_uint32           interrupt_mask;

#ifdef CYGDBG_IO_INIT
    diag_printf("BCM 33XX SERIAL config\n\n");
#endif

    // Translate baud rate from an enum to an integer.
    baud_rate = select_baud[new_config->baud];
    if (baud_rate == 0)
        return false;    // Invalid baud rate selected

    // Calculate baudword by dividing peripheral frequency
    // by (baud_rate * 32), rounding up, and subtracting 1.
//    baud_divisor = (( PeripheralClockFrequency + ( baud_rate * 16 )) / ( baud_rate * 32 )) - 1;
    // This is a simplified version of the above formula.  It's probably not
    // necessary to help the compiler like this, but it's a fun exercise.
    baud_divisor = ( PeripheralClockFrequency / baud_rate / 16 - 1 ) / 2;

    // Disable UART interrupts while changing hardware
    interrupt_mask = UART->intEnableMask;
    UART->intEnableMask = 0;

    // If the UART has already been initialized, let the TX FIFO empty.
    if (( UART->control & ( BRGEN | TXEN ) ) == ( BRGEN | TXEN ) )
    {
        // Wait for transmit FIFO to empty.  If we reset the FIFO before it empties, 
        // we'll lose characters and we can transmit garbage.
        while ( UART->txf_levl > 0 )
        {
        }
    }

    // Set receive character timeout.  This is the number of character times
    // the receive FIFO will remain non-empty before generating an interrupt
    // if the receive FIFO threshold is not met.
    UART->fifoctl = 16;

    // Enable FIFO.  This sets fifo thresholds - TX to 2 and RX to 8.
    UART->fifocfg = 0x28;

    // Disable TX and RX while changing baudword.
    UART->control &= ~(BRGEN | TXEN | RXEN);

    // Set databits, stopbits and parity.
    UART->config  = select_word_length[new_config->word_length] |
                    select_stop_bits  [new_config->stop       ];
    UART->control = select_parity[new_config->parity];

    // Set baud rate.
    UART->baudword = baud_divisor;
    // For simulation, set to the fastest possible bit rate.
//    UART->baudword = 1;

    // Enable TX and RX clocks.
    UART->control |= (BRGEN | TXEN | RXEN);

    if (initialize) {
        // Reset FIFOs and set receive character timeout.  This is the number 
        // of character times the receive FIFO will remain non-empty before 
        // generating an interrupt if the receive FIFO threshold is not met.
        UART->fifoctl = RSTTXFIFOS | RSTRXFIFOS | 16;

        // Enable interrupts only if we're operating in buffered mode.
        // Only receive interrupts are enabled here.  Transmit interrupts
        // are enabled when the "start_xmit" function is called.
        if (chan->out_cbuf.len != 0) {
            UART->intEnableMask = 
                RXBRK       |       // RX break state (ususally CTRL-break)
                RXOVFERR    |       // RX fifo overflow
                RXUNDERR    |       // RX fifo underflow (read too many)
                RXTIMEOUT   |       // RX fifo not empty, but threshold not met
                RXFIFOTHOLD;        // RX fifo above threshold
            // Enable general UART interrupt.
            hal_enable_periph_interrupt( irq_bits[bcm33xx_chan->channelNumber] );
        } else {
            UART->intEnableMask = 0;
        }
    } else {
        UART->intEnableMask = interrupt_mask;
    }

    if (new_config != &chan->config) {
        chan->config = *new_config;
    }
    return true;
}

// Function to initialize the device.  Called at bootstrap time.
static bool 
bcm33xx_serial_init(struct cyg_devtab_entry *tab)
{
    serial_channel      *chan         = (serial_channel *)      tab->priv;
    bcm33xx_serial_info *bcm33xx_chan = (bcm33xx_serial_info *) chan->dev_priv;

    // This really isn't the right way to do this, but it works for 33xx and 3368.
    if ( bcm33xx_chan->channelNumber == 0 )
    {
        bcm33xx_chan->base            = (CYG_ADDRWORD) UART;
    }
    else
    {
        bcm33xx_chan->base            = (CYG_ADDRWORD) UART1;
    }

#ifdef CYGDBG_IO_INIT
    diag_printf("BCM 33XX SERIAL init - dev: %x.%d\n", bcm33xx_chan->base, irq_bits[bcm33xx_chan->channelNumber]);
#endif

    // Initialize higher level serial driver.  Really only required for interrupt driven devices.
    (chan->callbacks->serial_init)(chan);
    if (chan->out_cbuf.len != 0) {
        // We're not using the normal interrupt model.  All internal interrupts
        // pass through ISR 0, which then calls our sub-ISR.
        hal_map_periph_interrupt(    irq_bits[bcm33xx_chan->channelNumber], bcm33xx_serial_ISR, (void *) chan );
        hal_enable_periph_interrupt( irq_bits[bcm33xx_chan->channelNumber] );
    }
    // Configure port and enable UART interrupts.
    bcm33xx_serial_config_port(chan, &chan->config, true);
    return true;
}

// This routine is called when the device is "looked" up (i.e. attached)
static Cyg_ErrNo 
bcm33xx_serial_lookup(struct cyg_devtab_entry **tab, 
                      struct cyg_devtab_entry  *sub_tab,
                      const char               *name)
{
    serial_channel  *chan = (serial_channel *)(*tab)->priv;

    // Initialize higher level serial driver.  Really only required for interrupt driven devices.
    (chan->callbacks->serial_init)(chan);
    return ENOERR;
}

// Send a character to the device output buffer.
// Return 'true' if character is sent to device
static bool
bcm33xx_serial_putc(serial_channel *chan, 
                    unsigned char   c)
{
    bcm33xx_serial_info *bcm33xx_chan = (bcm33xx_serial_info *) chan->dev_priv;
    volatile Uart       *UART         = (Uart *)                bcm33xx_chan->base;

    if ( UART->txf_levl < 15 )
    {
        if ((( bcm33xx_chan->channelNumber == 0 ) && ( gSerialPortEnabled0 )) ||
            (( bcm33xx_chan->channelNumber == 1 ) && ( gSerialPortEnabled1 )))
        {
            // The transmit buffer is not full, so send a character
            UART->Data = c;
        }
        return true;
    }
    else
    {
        // Buffer is full
        return false;
    }
}

// Fetch a character from the device input buffer, waiting if necessary
static unsigned char 
bcm33xx_serial_getc(serial_channel *chan)
{
    bcm33xx_serial_info *bcm33xx_chan = (bcm33xx_serial_info *) chan->dev_priv;
    volatile Uart       *UART         = (Uart *)                bcm33xx_chan->base;
    cyg_uint16           intStatus;
    unsigned char        ch           = 0;

    do
    {
        intStatus = UART->intStatus;
    } while (( intStatus & RXFIFONE ) == 0 );

    ch = UART->Data;
    if ((( bcm33xx_chan->channelNumber == 0 ) && ( !gSerialPortEnabled0 )) ||
        (( bcm33xx_chan->channelNumber == 1 ) && ( !gSerialPortEnabled1 )))
    {
        ch = 0;
    }
    return ch;
}

// Set up the device characteristics; baud rate, etc.
static Cyg_ErrNo
bcm33xx_serial_set_config(serial_channel *chan, 
                          cyg_uint32      key,
                          const void     *xbuf, 
                          cyg_uint32     *len)
{
    switch (key) {
        case CYG_IO_SET_CONFIG_SERIAL_INFO:
        {
            cyg_serial_info_t *config = (cyg_serial_info_t *)xbuf;
            if ( *len < sizeof(cyg_serial_info_t) ) {
                return -EINVAL;
            }
            *len = sizeof(cyg_serial_info_t);
            if ( bcm33xx_serial_config_port(chan, config, false) != true )
                return -EINVAL;
        }
        break;
        default:
            return -EINVAL;
    }
    return ENOERR;
}

// Enable the transmitter on the device
static void
bcm33xx_serial_start_xmit(serial_channel *chan)
{
    bcm33xx_serial_info *bcm33xx_chan = (bcm33xx_serial_info *) chan->dev_priv;
    volatile Uart       *UART         = (Uart *)                bcm33xx_chan->base;

    // Enable general UART transmit interrupt.
    UART->intEnableMask |= \
        TXFIFOTHOLD |       // TX fifo below threshold
        TXOVFERR    |       // TX fifo overflow
        TXUNDERR;           // TX fifo underflow - Is this possible?

    // We should not need to call this here.  TXFIFOTHOLD Interrupts are enabled, and the ISR
    // below calls this function.  However, sometimes we get called with Master Interrupts
    // disabled, and thus the ISR never runs.  This is unfortunate because it means we
    // will be doing multiple processing steps for the same thing.
    (chan->callbacks->xmt_char)(chan);
}

// Disable the transmitter on the device
static void 
bcm33xx_serial_stop_xmit(serial_channel *chan)
{
    bcm33xx_serial_info *bcm33xx_chan = (bcm33xx_serial_info *) chan->dev_priv;
    volatile Uart       *UART         = (Uart *)                bcm33xx_chan->base;

    // Disable general UART transmit interrupt.
    UART->intEnableMask &= ~TXFIFOTHOLD;
}

// Serial I/O - low level interrupt handler (ISR)
static void 
bcm33xx_serial_ISR( void *data )
{
    serial_channel      *chan         = (serial_channel *)      data;
    bcm33xx_serial_info *bcm33xx_chan = (bcm33xx_serial_info *) chan->dev_priv;
    volatile Uart       *UART         = (Uart *)                bcm33xx_chan->base;
    cyg_uint8            rxChar       = 0;

    if (( UART->intStatus & TXFIFOTHOLD ) != 0 )
    {
        (chan->callbacks->xmt_char)(chan);
    }
    if (( UART->intStatus & TXFIFOEMT ) != 0 )
    {
        UART->intEnableMask &= ~(TXFIFOTHOLD|TXFIFOEMT);
    }
    if (( UART->intStatus & RXBRK ) != 0 )
    {
        // Read the character to clear the break status.
        rxChar = UART->Data;
        // If there's a break callout function for this channel, call it.
        if ( bcm33xx_chan->break_callout != NULL )
        {
            (bcm33xx_chan->break_callout)();
        }
    }
    while (( UART->intStatus & RXFIFONE ) != 0 )
    {
        rxChar = UART->Data;
        if ((( bcm33xx_chan->channelNumber == 0 ) && ( !gSerialPortEnabled0 )) ||
            (( bcm33xx_chan->channelNumber == 1 ) && ( !gSerialPortEnabled1 )))
        {
            rxChar = 0;
        }
        (chan->callbacks->rcv_char)(chan, rxChar);
    }

    // Check for possible errors.  Any other errors are cleared by reading
    // from or writing to the fifos.
    if (( UART->intStatus & ( RXOVFERR | RXUNDERR )) != 0 )
    {
        UART->fifoctl |= RSTRXFIFOS;
    }

    if (( UART->intStatus & ( TXOVFERR | TXUNDERR )) != 0 )
    {
        UART->fifoctl |= RSTTXFIFOS;
    }

    // The interrupt was disabled by the int0 ISR.  Need to re-enable.
    hal_enable_periph_interrupt(irq_bits[bcm33xx_chan->channelNumber]);
}

#endif

//-------------------------------------------------------------------------
// EOF bcm33xx_serial.c
