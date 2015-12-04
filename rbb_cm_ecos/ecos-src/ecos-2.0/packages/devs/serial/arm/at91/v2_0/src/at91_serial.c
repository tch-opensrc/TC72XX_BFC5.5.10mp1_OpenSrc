//==========================================================================
//
//      io/serial/arm/at91/at91_serial.c
//
//      Atmel AT91/EB40 Serial I/O Interface Module (interrupt driven)
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
// Author(s):     gthomas
// Contributors:  gthomas
// Date:          2001-07-24
// Purpose:       Atmel AT91/EB40 Serial I/O module (interrupt driven version)
// Description: 
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/system.h>
#include <pkgconf/io_serial.h>
#include <pkgconf/io.h>
#include <pkgconf/kernel.h>

#include <cyg/io/io.h>
#include <cyg/hal/hal_io.h>
#include <cyg/hal/hal_intr.h>
#include <cyg/io/devtab.h>
#include <cyg/io/serial.h>
#include <cyg/infra/diag.h>

#ifdef CYGPKG_IO_SERIAL_ARM_AT91

#include "at91_serial.h"

typedef struct at91_serial_info {
    CYG_ADDRWORD   base;
    CYG_WORD       int_num;
    cyg_interrupt  serial_interrupt;
    cyg_handle_t   serial_interrupt_handle;
} at91_serial_info;

static bool at91_serial_init(struct cyg_devtab_entry *tab);
static bool at91_serial_putc(serial_channel *chan, unsigned char c);
static Cyg_ErrNo at91_serial_lookup(struct cyg_devtab_entry **tab, 
                                    struct cyg_devtab_entry *sub_tab,
                                    const char *name);
static unsigned char at91_serial_getc(serial_channel *chan);
static Cyg_ErrNo at91_serial_set_config(serial_channel *chan, cyg_uint32 key,
                                        const void *xbuf, cyg_uint32 *len);
static void at91_serial_start_xmit(serial_channel *chan);
static void at91_serial_stop_xmit(serial_channel *chan);

static cyg_uint32 at91_serial_ISR(cyg_vector_t vector, cyg_addrword_t data);
static void       at91_serial_DSR(cyg_vector_t vector, cyg_ucount32 count, cyg_addrword_t data);

static SERIAL_FUNS(at91_serial_funs, 
                   at91_serial_putc, 
                   at91_serial_getc,
                   at91_serial_set_config,
                   at91_serial_start_xmit,
                   at91_serial_stop_xmit
    );

#ifdef CYGPKG_IO_SERIAL_ARM_AT91_SERIAL0
static at91_serial_info at91_serial_info0 = {(CYG_ADDRWORD)AT91_USART0,
                                             CYGNUM_HAL_INTERRUPT_USART0};
#if CYGNUM_IO_SERIAL_ARM_AT91_SERIAL0_BUFSIZE > 0
static unsigned char at91_serial_out_buf0[CYGNUM_IO_SERIAL_ARM_AT91_SERIAL0_BUFSIZE];
static unsigned char at91_serial_in_buf0[CYGNUM_IO_SERIAL_ARM_AT91_SERIAL0_BUFSIZE];

static SERIAL_CHANNEL_USING_INTERRUPTS(at91_serial_channel0,
                                       at91_serial_funs, 
                                       at91_serial_info0,
                                       CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_ARM_AT91_SERIAL0_BAUD),
                                       CYG_SERIAL_STOP_DEFAULT,
                                       CYG_SERIAL_PARITY_DEFAULT,
                                       CYG_SERIAL_WORD_LENGTH_DEFAULT,
                                       CYG_SERIAL_FLAGS_DEFAULT,
                                       &at91_serial_out_buf0[0], sizeof(at91_serial_out_buf0),
                                       &at91_serial_in_buf0[0], sizeof(at91_serial_in_buf0)
    );
#else
static SERIAL_CHANNEL(at91_serial_channel0,
                      at91_serial_funs, 
                      at91_serial_info0,
                      CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_ARM_AT91_SERIAL0_BAUD),
                      CYG_SERIAL_STOP_DEFAULT,
                      CYG_SERIAL_PARITY_DEFAULT,
                      CYG_SERIAL_WORD_LENGTH_DEFAULT,
                      CYG_SERIAL_FLAGS_DEFAULT
    );
#endif

DEVTAB_ENTRY(at91_serial_io0, 
             CYGDAT_IO_SERIAL_ARM_AT91_SERIAL0_NAME,
             0,                     // Does not depend on a lower level interface
             &cyg_io_serial_devio, 
             at91_serial_init, 
             at91_serial_lookup,     // Serial driver may need initializing
             &at91_serial_channel0
    );
#endif //  CYGPKG_IO_SERIAL_ARM_AT91_SERIAL1

#ifdef CYGPKG_IO_SERIAL_ARM_AT91_SERIAL1
static at91_serial_info at91_serial_info1 = {(CYG_ADDRWORD)AT91_USART1,
                                              CYGNUM_HAL_INTERRUPT_USART1};
#if CYGNUM_IO_SERIAL_ARM_AT91_SERIAL1_BUFSIZE > 0
static unsigned char at91_serial_out_buf1[CYGNUM_IO_SERIAL_ARM_AT91_SERIAL1_BUFSIZE];
static unsigned char at91_serial_in_buf1[CYGNUM_IO_SERIAL_ARM_AT91_SERIAL1_BUFSIZE];

static SERIAL_CHANNEL_USING_INTERRUPTS(at91_serial_channel1,
                                       at91_serial_funs, 
                                       at91_serial_info1,
                                       CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_ARM_AT91_SERIAL1_BAUD),
                                       CYG_SERIAL_STOP_DEFAULT,
                                       CYG_SERIAL_PARITY_DEFAULT,
                                       CYG_SERIAL_WORD_LENGTH_DEFAULT,
                                       CYG_SERIAL_FLAGS_DEFAULT,
                                       &at91_serial_out_buf1[0], sizeof(at91_serial_out_buf1),
                                       &at91_serial_in_buf1[0], sizeof(at91_serial_in_buf1)
    );
#else
static SERIAL_CHANNEL(at91_serial_channel1,
                      at91_serial_funs, 
                      at91_serial_info1,
                      CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_ARM_AT91_SERIAL1_BAUD),
                      CYG_SERIAL_STOP_DEFAULT,
                      CYG_SERIAL_PARITY_DEFAULT,
                      CYG_SERIAL_WORD_LENGTH_DEFAULT,
                      CYG_SERIAL_FLAGS_DEFAULT
    );
#endif

DEVTAB_ENTRY(at91_serial_io1, 
             CYGDAT_IO_SERIAL_ARM_AT91_SERIAL1_NAME,
             0,                     // Does not depend on a lower level interface
             &cyg_io_serial_devio, 
             at91_serial_init, 
             at91_serial_lookup,     // Serial driver may need initializing
             &at91_serial_channel1
    );
#endif //  CYGPKG_IO_SERIAL_ARM_AT91_SERIAL1

// Internal function to actually configure the hardware to desired baud rate, etc.
static bool
at91_serial_config_port(serial_channel *chan, cyg_serial_info_t *new_config, bool init)
{
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    CYG_ADDRWORD base = at91_chan->base;
    cyg_uint32 parity = select_parity[new_config->parity];
    cyg_uint32 word_length = select_word_length[new_config->word_length-CYGNUM_SERIAL_WORD_LENGTH_5];
    cyg_uint32 stop_bits = select_stop_bits[new_config->stop];

    if ((word_length == 0xFF) ||
        (parity == 0xFF) ||
        (stop_bits == 0xFF)) {
        return false;  // Unsupported configuration
    }

    // Reset device
    HAL_WRITE_UINT32(base+AT91_US_CR, AT91_US_CR_RxRESET | AT91_US_CR_TxRESET);

    // Configuration
    HAL_WRITE_UINT32(base+AT91_US_MR, parity | word_length | stop_bits);

    // Baud rate
    HAL_WRITE_UINT32(base+AT91_US_BRG, AT91_US_BAUD(select_baud[new_config->baud]));

    // Disable all interrupts
    HAL_WRITE_UINT32(base+AT91_US_IDR, 0xFFFFFFFF);

    // Enable Rx interrupts
    HAL_WRITE_UINT32(base+AT91_US_IER, AT91_US_IER_RxRDY);

    // Enable RX and TX
    HAL_WRITE_UINT32(base+AT91_US_CR, AT91_US_CR_RxENAB | AT91_US_CR_TxENAB);

    return true;
}

// Function to initialize the device.  Called at bootstrap time.
static bool 
at91_serial_init(struct cyg_devtab_entry *tab)
{
    serial_channel *chan = (serial_channel *)tab->priv;
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    int res;

#ifdef CYGDBG_IO_INIT
    diag_printf("AT91 SERIAL init - dev: %x.%d\n", at91_chan->base, at91_chan->int_num);
#endif
    (chan->callbacks->serial_init)(chan);  // Really only required for interrupt driven devices
    if (chan->out_cbuf.len != 0) {
        cyg_drv_interrupt_create(at91_chan->int_num,
                                 4,                      // Priority
                                 (cyg_addrword_t)chan,   // Data item passed to interrupt handler
                                 at91_serial_ISR,
                                 at91_serial_DSR,
                                 &at91_chan->serial_interrupt_handle,
                                 &at91_chan->serial_interrupt);
        cyg_drv_interrupt_attach(at91_chan->serial_interrupt_handle);
        cyg_drv_interrupt_unmask(at91_chan->int_num);
    }
    res = at91_serial_config_port(chan, &chan->config, true);
    return res;
}

// This routine is called when the device is "looked" up (i.e. attached)
static Cyg_ErrNo 
at91_serial_lookup(struct cyg_devtab_entry **tab, 
                  struct cyg_devtab_entry *sub_tab,
                  const char *name)
{
    serial_channel *chan = (serial_channel *)(*tab)->priv;

    (chan->callbacks->serial_init)(chan);  // Really only required for interrupt driven devices
    return ENOERR;
}

// Send a character to the device output buffer.
// Return 'true' if character is sent to device
static bool
at91_serial_putc(serial_channel *chan, unsigned char c)
{
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    CYG_ADDRWORD base = at91_chan->base;
    cyg_uint32 stat;

    // Check status
    HAL_READ_UINT32(base+AT91_US_CSR, stat);

    // Send character if possible
    if ((stat & AT91_US_CSR_TxRDY) != 0) {
        HAL_WRITE_UINT32(base+AT91_US_THR, c);
        return true;
    } else {
        return false;  // Couldn't send, tx was busy
    }
}

// Fetch a character from the device input buffer, waiting if necessary
static unsigned char 
at91_serial_getc(serial_channel *chan)
{
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    CYG_ADDRWORD base = at91_chan->base;
    cyg_uint32 c;

    // Read data
    HAL_READ_UINT32(base+AT91_US_RHR, c);
    return c;
}

// Set up the device characteristics; baud rate, etc.
static Cyg_ErrNo
at91_serial_set_config(serial_channel *chan, cyg_uint32 key,
                         const void *xbuf, cyg_uint32 *len)
{
    switch (key) {
    case CYG_IO_SET_CONFIG_SERIAL_INFO:
      {
        cyg_serial_info_t *config = (cyg_serial_info_t *)xbuf;
        if ( *len < sizeof(cyg_serial_info_t) ) {
            return -EINVAL;
        }
        *len = sizeof(cyg_serial_info_t);
        if ( true != at91_serial_config_port(chan, config, false) )
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
at91_serial_start_xmit(serial_channel *chan)
{
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    CYG_ADDRWORD base = at91_chan->base;

    (chan->callbacks->xmt_char)(chan);  // Kick transmitter (if necessary)
    HAL_WRITE_UINT32(base+AT91_US_IER, AT91_US_IER_TxRDY);
}

// Disable the transmitter on the device
static void 
at91_serial_stop_xmit(serial_channel *chan)
{
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    CYG_ADDRWORD base = at91_chan->base;

    HAL_WRITE_UINT32(base+AT91_US_IDR, AT91_US_IER_TxRDY);
}

// Serial I/O - low level interrupt handler (ISR)
static cyg_uint32 
at91_serial_ISR(cyg_vector_t vector, cyg_addrword_t data)
{
    cyg_drv_interrupt_mask(vector);
    cyg_drv_interrupt_acknowledge(vector);
    return (CYG_ISR_CALL_DSR|CYG_ISR_HANDLED);  // Cause DSR to be run
}

// Serial I/O - high level interrupt handler (DSR)
static void       
at91_serial_DSR(cyg_vector_t vector, cyg_ucount32 count, cyg_addrword_t data)
{
    serial_channel *chan = (serial_channel *)data;
    at91_serial_info *at91_chan = (at91_serial_info *)chan->dev_priv;
    CYG_ADDRWORD base = at91_chan->base;
    cyg_uint32 stat, c;

    // Check status
    HAL_READ_UINT32(base+AT91_US_IMR, stat);

    if (stat & (AT91_US_IER_TxRDY)) {
        (chan->callbacks->xmt_char)(chan);
    }
    if (stat & (AT91_US_IER_RxRDY)) {
        while (true) {
            HAL_READ_UINT32(base+AT91_US_CSR, stat);
            if ((stat & AT91_US_CSR_RxRDY) == 0) {
                break;
            }
            HAL_READ_UINT32(base+AT91_US_RHR, c);
            (chan->callbacks->rcv_char)(chan, c);
        }
    }
    cyg_drv_interrupt_unmask(vector);
}
#endif
