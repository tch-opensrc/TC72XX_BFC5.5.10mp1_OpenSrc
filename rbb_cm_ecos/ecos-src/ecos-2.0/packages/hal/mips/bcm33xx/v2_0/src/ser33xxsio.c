//=============================================================================
//
//      ser33xxsio.c
//
//      Simple driver for the serial controllers on the Broadcom 33xx
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
// Author(s):   Mike Sieweke
// Contributors:Mike Sieweke
// Date:        2003-06-09
// Description: Simple driver for the Broadcom 33xx serial controller
//
//####DESCRIPTIONEND####
// ====================================================================
// Portions of this software Copyright (c) 2003-2010 Broadcom Corporation
// ====================================================================
//=============================================================================

#include <pkgconf/hal.h>
#include <pkgconf/system.h>
#include CYGBLD_HAL_PLATFORM_H

#include <cyg/hal/hal_arch.h>           // SAVE/RESTORE GP macros
#include <cyg/hal/hal_io.h>             // IO macros
#include <cyg/hal/hal_if.h>             // interface API
#include <cyg/hal/hal_intr.h>           // HAL_ENABLE/MASK/UNMASK_INTERRUPTS
#include <cyg/hal/hal_misc.h>           // Helper functions
#include <cyg/hal/drv_api.h>            // CYG_ISR_HANDLED
#include <cyg/hal/bcm33xx_regs.h>

//-----------------------------------------------------------------------------
//  Local types and constants

cyg_uint8 gSerialPortEnabled __attribute__ ((weak)) = 1;

// For Broadcom reference designs, the peripheral clock should be 50 MHz,
// which makes the baudword for 115.2kps = 13.  And we only use 115.2kbps
//#define   BAUD_115200   13

// Unfortunately, someone designed a chip where the peripheral clock isn't 50 MHz.
extern cyg_uint32 PeripheralClockFrequency;

//-----------------------------------------------------------------------------
typedef struct {
    cyg_uint8 *base;
    cyg_int32  msec_timeout;
    int        isr_vector;
} channel_data_t;

static channel_data_t channels[1] = {
    { (cyg_uint8*)BCM_UART0_BASE, 1000, 0}
};


//-----------------------------------------------------------------------------
// The minimal init, get and put functions. All by polling.

void
cyg_hal_plf_serial_init_channel(void* __ch_data)
{
    static bool            doneOnce = 0;

    // Only initialize the serial device once.  If we do it again, it sends
    // garbage characters to the port.
    if (doneOnce == 1)
    {
        return;
    }
    
    doneOnce = 1;

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*)&channels[0];

    // Disable UART interrupts while changing hardware
    UART->intEnableMask = 0;

    // Disable TX and RX while changing baudword.
    UART->control &= ~(BRGEN | TXEN | RXEN);

    // Enable UART Clock...
    // *** This won't be correct for some chips.  Let the bootloader enable the
    // *** UART clock, if needed.
//    INTC->blkEnables |= UART_CLK_EN;

    // Set bit rate to 115.2k bps
    UART->baudword = ( PeripheralClockFrequency / 115200 / 16 - 1 ) / 2;
    // For simulation, set to the fastest possible bit rate.
//    UART->baudword = 1;

    // Set 8 databits, 1 stopbit and no parity.
    UART->config = BITS8SYM|ONESTOP;

    // Enable and clear FIFO.  This sets RX and TX fifo levels at 4.
    UART->fifocfg = 0x44;

    // Enable transmit, receive, and buad rate clock.
    UART->control = TXEN|RXEN|BRGEN;

    // Set receive character timeout to 5 character idle times, and
    // reset TX and RX FIFOs.
    UART->fifoctl = (RSTTXFIFOS | RSTRXFIFOS | 5);

}

void
cyg_hal_plf_serial_putc(void* __ch_data, cyg_uint8 __ch)
{

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*)&channels[0];

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

    // Wait until the transmit FIFO is not full.  Full is 16 characters.
#if !SIMULATION
    // When in simulation, don't worry about overflowing the FIFO.
    while ( UART->txf_levl >= 14 )
#endif
    {
    }
#if 0
    // Wait until the transmit fifo is below its threshold.
    do {
        intStatus = UART->intStatus;
    } while ((intStatus & TXFIFOTHOLD) == 0);
#endif

    // The transmit buffer is available, so send a character
    if (gSerialPortEnabled)
    {
        UART->Data = __ch;
    }

// *****  This can be enabled for debugging.  The worst case is that 16
// *****  characters of output will be lost at reset.
#if 0
    // Hang around until the character has been safely sent.
    do {
        intStatus = UART->intStatus;
    } while ((intStatus & TXFIFOEMT) == 0);
#endif

}

static cyg_bool
cyg_hal_plf_serial_getc_nonblock(void* __ch_data, cyg_uint8* ch)
{
    cyg_uint16      intStatus;

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*)&channels[0];


    intStatus = UART->intStatus;
    if ((intStatus & RXFIFONE) == 0)
        return false;

    *ch = UART->Data;
    if (!gSerialPortEnabled)
    {
        *ch = 0;
    }

    return true;
}

cyg_uint8
cyg_hal_plf_serial_getc(void* __ch_data)
{
    cyg_uint8 ch;

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*) &channels[0];

    while(!cyg_hal_plf_serial_getc_nonblock(__ch_data, &ch));

    return ch;
}

static void
cyg_hal_plf_serial_write(void* __ch_data, const cyg_uint8* __buf, 
                         cyg_uint32 __len)
{
    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*) &channels[0];

    while(__len-- > 0)
        cyg_hal_plf_serial_putc(__ch_data, *__buf++);

}

static void
cyg_hal_plf_serial_read(void* __ch_data, cyg_uint8* __buf, cyg_uint32 __len)
{
    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*)&channels[0];

    while(__len-- > 0)
        *__buf++ = cyg_hal_plf_serial_getc(__ch_data);

}


cyg_bool
cyg_hal_plf_serial_getc_timeout(void* __ch_data, cyg_uint8* ch)
{
    int             delay_count;
    channel_data_t *chan;
    cyg_bool        charFound;

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*)&channels[0];

    chan = (channel_data_t*)__ch_data;

    delay_count = chan->msec_timeout * 10; // delay in .1 ms steps

    for(;;) {
        charFound = cyg_hal_plf_serial_getc_nonblock(__ch_data, ch);
        if (charFound || ( delay_count-- == 0 ))
            break;
        CYGACC_CALL_IF_DELAY_US(100);
    }

    return charFound;
}

extern cyg_uint32 PeripheralClockFrequency;

static int
cyg_hal_plf_serial_control(void *__ch_data, __comm_control_cmd_t __func, ...)
{
    channel_data_t      *chan;
    cyg_uint16           intEnableMask;
    int                  returnValue        = 0;

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*)&channels[0];

    chan = (channel_data_t*)__ch_data;

    switch (__func) {
        case __COMMCTL_IRQ_ENABLE:
            // Enable interrupts in the UART and interrupt controller.
            UART->intEnableMask |= (RXFIFONE | RXTIMEOUT);
            *INTC_IrqEnableMask  |= UART0IRQ;
            break;
        case __COMMCTL_IRQ_DISABLE:
            // Disable interrupts in the UART and interrupt controller.

            // Return the current interrupt enable state.
            returnValue = *INTC_IrqEnableMask & UART0IRQ;
    
            // Is this really necessary?  Just disabling the overall interrupt
            // should suffice.
            // UART->intEnableMask &= ~(RXFIFONE | RXTIMEOUT);

            *INTC_IrqEnableMask &= ~UART0IRQ;
            break;
        case __COMMCTL_DBG_ISR_VECTOR:
            returnValue = chan->isr_vector;
            break;
        case __COMMCTL_SET_TIMEOUT:
        {
            va_list ap;
    
            returnValue = chan->msec_timeout;

            va_start(ap, __func);
            chan->msec_timeout = va_arg(ap, cyg_uint32);
            va_end(ap);
            break;
        }        
        case __COMMCTL_SETBAUD:
        {
            cyg_uint32 baud_rate;
            cyg_uint16 baud_divisor;
            va_list    ap;
    
            va_start(ap, __func);
            baud_rate = va_arg(ap, cyg_uint32);
            va_end(ap);

            // Calculate baudword by dividing peripheral frequency
            // by (baud_rate * 32), rounding up, and subtracting 1.
//            baud_divisor = (( PeripheralClockFrequency + 16 ) / ( baud_rate * 32 )) - 1;
            baud_divisor = ( PeripheralClockFrequency / baud_rate / 16 - 1 ) / 2;
    
            // Disable UART interrupts while changing baudword
            intEnableMask = UART->intEnableMask;
            UART->intEnableMask = 0;
    
            // Disable TX and RX while changing baudword.
            UART->control &= ~(BRGEN | TXEN | RXEN);
    
            // Set baud rate.
            UART->baudword = baud_divisor;
            // For simulation, set to the fastest possible bit rate.
//            UART->baudword = 6;
    
            // Re-enable TX and RX.
            UART->control &= ~(BRGEN | TXEN | RXEN);
    
            // Re-enable interrupts if necessary
            UART->intEnableMask = intEnableMask;
            break;
        }
    
        case __COMMCTL_GETBAUD:
            break;
        default:
            break;
    }
    return returnValue;
}

static int
cyg_hal_plf_serial_isr(void *__ch_data, int* __ctrlc, 
                       CYG_ADDRWORD __vector, CYG_ADDRWORD __data)
{
    int             returnVal   = 0;
    cyg_uint8       intStatus;
    cyg_uint8       c;
    channel_data_t *chan;

    // Some of the diagnostic print code calls through here with no idea what the ch_data is.
    // Go ahead and assume it is channels[0].
    if (__ch_data == 0)
        __ch_data = (void*) &channels[0];

    chan = (channel_data_t*)__ch_data;

// No need to acknowledge UART interrupts.  They are cleared by handling the
// condition.
//    HAL_INTERRUPT_ACKNOWLEDGE(chan->isr_vector);

    intStatus = UART->intStatus;

    *__ctrlc = 0;
    if ((intStatus & (RXFIFONE)) != 0) {

        c = UART->Data;
        if (!gSerialPortEnabled)
        {
            c = 0;
        }
    
        if( cyg_hal_is_break( &c , 1 ) )
            *__ctrlc = 1;

        returnVal = CYG_ISR_HANDLED;
    }

    return returnVal;
}

static void
cyg_hal_plf_serial_init(void)
{
    hal_virtual_comm_table_t *comm;
    int                       currentID;

    currentID = CYGACC_CALL_IF_SET_CONSOLE_COMM(CYGNUM_CALL_IF_SET_COMM_ID_QUERY_CURRENT);

    // Disable interrupts.
    *INTC_IrqEnableMask &= ~UART0IRQ;

    // Init channels
    cyg_hal_plf_serial_init_channel((void*)&channels[0]);
    
    // Setup procs in the vector table

    // Set channel 0
    CYGACC_CALL_IF_SET_CONSOLE_COMM(0);
    comm = CYGACC_CALL_IF_CONSOLE_PROCS();
    CYGACC_COMM_IF_CH_DATA_SET(*comm, &channels[0]);
    CYGACC_COMM_IF_WRITE_SET(*comm, cyg_hal_plf_serial_write);
    CYGACC_COMM_IF_READ_SET(*comm, cyg_hal_plf_serial_read);
    CYGACC_COMM_IF_PUTC_SET(*comm, cyg_hal_plf_serial_putc);
    CYGACC_COMM_IF_GETC_SET(*comm, cyg_hal_plf_serial_getc);
    CYGACC_COMM_IF_CONTROL_SET(*comm, cyg_hal_plf_serial_control);
    CYGACC_COMM_IF_DBG_ISR_SET(*comm, cyg_hal_plf_serial_isr);
    CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, cyg_hal_plf_serial_getc_timeout);

    // Restore original console
    CYGACC_CALL_IF_SET_CONSOLE_COMM(currentID);
}

void
cyg_hal_plf_comms_init(void)
{
    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    cyg_hal_plf_serial_init();
}

//-----------------------------------------------------------------------------
// end of ser16c550c.c

