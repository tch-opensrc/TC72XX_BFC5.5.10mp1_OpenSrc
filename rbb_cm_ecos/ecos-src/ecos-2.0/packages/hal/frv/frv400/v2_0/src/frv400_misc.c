//==========================================================================
//
//      frv400_misc.c
//
//      HAL misc board support code for Fujitsu MB93091 ( FR-V 400)
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
// Author(s):    gthomas
// Contributors: gthomas
// Date:         2001-09-07
// Purpose:      HAL board support
// Description:  Implementations of HAL board interfaces
//
//####DESCRIPTIONEND####
//
//========================================================================*/

#include <pkgconf/hal.h>
#include <pkgconf/system.h>
#include CYGBLD_HAL_PLATFORM_H

#include <cyg/infra/cyg_type.h>         // base types
#include <cyg/infra/cyg_trac.h>         // tracing macros
#include <cyg/infra/cyg_ass.h>          // assertion macros
#include <cyg/infra/diag.h>             // diag_printf() and friends

#include <cyg/hal/hal_io.h>             // IO macros
#include <cyg/hal/hal_arch.h>           // Register state info
#include <cyg/hal/hal_diag.h>
#include <cyg/hal/hal_intr.h>           // Interrupt names
#include <cyg/hal/hal_cache.h>
#include <cyg/hal/frv400.h>             // Hardware definitions
#include <cyg/hal/hal_if.h>             // calling interface API

#include <pkgconf/io_pci.h>
#include <cyg/io/pci_hw.h>
#include <cyg/io/pci.h>

static cyg_uint32 _period;

void hal_clock_initialize(cyg_uint32 period)
{
    _period = period;
    // Set timer #1 to run in terminal count mode for period
    HAL_WRITE_UINT8(_FRV400_TCTR, _FRV400_TCTR_SEL1|_FRV400_TCTR_RLOHI|_FRV400_TCTR_MODE0);
    HAL_WRITE_UINT8(_FRV400_TCSR1, period & 0xFF);
    HAL_WRITE_UINT8(_FRV400_TCSR1, period >> 8);
    // Configure interrupt
    HAL_INTERRUPT_CONFIGURE(CYGNUM_HAL_INTERRUPT_TIMER1, 1, 1);  // Interrupt when TOUT1 is high
}

void hal_clock_reset(cyg_uint32 vector, cyg_uint32 period)
{
    cyg_int16 offset;
    cyg_uint8 _val;

    // Latch & read counter from timer #1
    HAL_WRITE_UINT8(_FRV400_TCTR, _FRV400_TCTR_LATCH|_FRV400_TCTR_RLOHI|_FRV400_TCTR_SEL1);
    HAL_READ_UINT8(_FRV400_TCSR1, _val);
    offset = _val;
    HAL_READ_UINT8(_FRV400_TCSR1, _val);
    offset |= _val << 8;    // This will be the number of clocks beyond 0
    period += offset;
    // Reinitialize with adjusted count
    // Set timer #1 to run in terminal count mode for period
    HAL_WRITE_UINT8(_FRV400_TCTR, _FRV400_TCTR_SEL1|_FRV400_TCTR_RLOHI|_FRV400_TCTR_MODE0);
    HAL_WRITE_UINT8(_FRV400_TCSR1, period & 0xFF);
    HAL_WRITE_UINT8(_FRV400_TCSR1, period >> 8);
}

// Read the current value of the clock, returning the number of hardware "ticks"
// that have occurred (i.e. how far away the current value is from the start)

void hal_clock_read(cyg_uint32 *pvalue)
{
    cyg_int16 offset;
    cyg_uint8 _val;

    // Latch & read counter from timer #1
    HAL_WRITE_UINT8(_FRV400_TCTR, _FRV400_TCTR_LATCH|_FRV400_TCTR_RLOHI|_FRV400_TCTR_SEL1);
    HAL_READ_UINT8(_FRV400_TCSR1, _val);
    offset = _val;
    HAL_READ_UINT8(_FRV400_TCSR1, _val);
    offset |= _val << 8;

    // 'offset' is the current timer value
    *pvalue = _period - offset;
}

// Delay for some number of useconds.
// Assumptions:
//   Use timer #2
//   Min granularity is 10us
#define _MIN_DELAY 10

void hal_delay_us(int us)
{
    cyg_uint8 stat;
    int timeout;

    while (us >= _MIN_DELAY) {
	us -= _MIN_DELAY;
        // Set timer #2 to run in terminal count mode for _MIN_DELAY us
        HAL_WRITE_UINT8(_FRV400_TCTR, _FRV400_TCTR_SEL2|_FRV400_TCTR_RLOHI|_FRV400_TCTR_MODE0);
        HAL_WRITE_UINT8(_FRV400_TCSR2, _MIN_DELAY & 0xFF);
        HAL_WRITE_UINT8(_FRV400_TCSR2, _MIN_DELAY >> 8);
        timeout = 100000;
        // Wait for TOUT to indicate terminal count reached
        do {
            HAL_WRITE_UINT8(_FRV400_TCTR, _FRV400_TCTR_RB|_FRV400_TCTR_RB_NCOUNT|_FRV400_TCTR_RB_CTR2);
            HAL_READ_UINT8(_FRV400_TCSR2, stat);
            if (--timeout == 0) break;
        } while ((stat & _FRV400_TCxSR_TOUT) == 0);
    }
}

//
// Early stage hardware initialization
//   Some initialization has already been done before we get here.  For now
// just set up the interrupt environment.

long _system_clock;  // Calculated clock frequency

void hal_hardware_init(void)
{
    cyg_uint32 clk;

    // Set up interrupt controller
    HAL_WRITE_UINT16(_FRV400_IRC_MASK, 0xFFFE);  // All masked
    HAL_WRITE_UINT16(_FRV400_IRC_RC, 0xFFFE);    // All cleared
    HAL_WRITE_UINT16(_FRV400_IRC_IRL, 0x10);     // Clear IRL (interrupt request latch)    

    // Onboard FPGA interrupts
    HAL_WRITE_UINT16(_FRV400_FPGA_CONTROL, _FRV400_FPGA_CONTROL_IRQ);  // Enable IRQ registers
    HAL_WRITE_UINT16(_FRV400_FPGA_IRQ_MASK,      // Set up for LAN, PCI INTx
                     0x7FFE & 
                     ~(_FRV400_FPGA_IRQ_LAN |
                       _FRV400_FPGA_IRQ_INTA |
                       _FRV400_FPGA_IRQ_INTB |
                       _FRV400_FPGA_IRQ_INTC |
                       _FRV400_FPGA_IRQ_INTD)
        );
    HAL_WRITE_UINT16(_FRV400_FPGA_IRQ_LEVELS,    // Set up for LAN, PCI INTx
                     0x7FFE & 
                     ~(_FRV400_FPGA_IRQ_LAN |
                       _FRV400_FPGA_IRQ_INTA |
                       _FRV400_FPGA_IRQ_INTB |
                       _FRV400_FPGA_IRQ_INTC |
                       _FRV400_FPGA_IRQ_INTD)
        );
    HAL_INTERRUPT_CONFIGURE(CYGNUM_HAL_INTERRUPT_LAN, 1, 0);  // Level, low

    // Set up system clock
    HAL_READ_UINT32(_FRV400_MB_CLKSW, clk);
    _system_clock = (((clk&0xFF) * 125 * 2) / 240) * 1000000;

    // Set scalers to achieve 1us resolution in timer
    HAL_WRITE_UINT8(_FRV400_TPRV, _system_clock / (1000*1000));
    HAL_WRITE_UINT8(_FRV400_TCKSL0, 0x80);
    HAL_WRITE_UINT8(_FRV400_TCKSL1, 0x80);
    HAL_WRITE_UINT8(_FRV400_TCKSL2, 0x80);

    hal_if_init();

    // Initialize real-time clock (for delays, etc, even if kernel doesn't use it)
    hal_clock_initialize(CYGNUM_HAL_RTC_PERIOD);

    _frv400_pci_init();
}

//
// Interrupt control
//

void hal_interrupt_mask(int vector)
{
    cyg_uint16 _mask;

    switch (vector) {
    case CYGNUM_HAL_INTERRUPT_LAN:
        HAL_READ_UINT16(_FRV400_FPGA_IRQ_MASK, _mask);
        _mask |= _FRV400_FPGA_IRQ_LAN;
        HAL_WRITE_UINT16(_FRV400_FPGA_IRQ_MASK, _mask);
        break;
    }
    HAL_READ_UINT16(_FRV400_IRC_MASK, _mask);
    _mask |= (1<<(vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1));
    HAL_WRITE_UINT16(_FRV400_IRC_MASK, _mask);
}

void hal_interrupt_unmask(int vector)
{
    cyg_uint16 _mask;

    switch (vector) {
    case CYGNUM_HAL_INTERRUPT_LAN:
        HAL_READ_UINT16(_FRV400_FPGA_IRQ_MASK, _mask);
        _mask &= ~_FRV400_FPGA_IRQ_LAN;
        HAL_WRITE_UINT16(_FRV400_FPGA_IRQ_MASK, _mask);
        break;
    }
    HAL_READ_UINT16(_FRV400_IRC_MASK, _mask);
    _mask &= ~(1<<(vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1));
    HAL_WRITE_UINT16(_FRV400_IRC_MASK, _mask);
}

void hal_interrupt_acknowledge(int vector)
{
    cyg_uint16 _mask;

    switch (vector) {
    case CYGNUM_HAL_INTERRUPT_LAN:
        HAL_WRITE_UINT16(_FRV400_FPGA_IRQ_REQUEST,      // Clear LAN interrupt
                         0x7FFE & ~_FRV400_FPGA_IRQ_LAN);
        break;
    }
    _mask = (1<<(vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1));
    HAL_WRITE_UINT16(_FRV400_IRC_RC, _mask);
    HAL_WRITE_UINT16(_FRV400_IRC_IRL, 0x10);  // Clears IRL latch
}

//
// Configure an interrupt
//  level - boolean (0=> edge, 1=>level)
//  up - edge: (0=>falling edge, 1=>rising edge)
//       level: (0=>low, 1=>high)
//
void hal_interrupt_configure(int vector, int level, int up)
{
    cyg_uint16 _irr, _tmr, _trig;

    if (level) {
        if (up) {
            _trig = 0;     // level, high
        } else {
            _trig = 1;     // level, low
        }
    } else {
        if (up) {
            _trig = 2;     // edge, rising
        } else {
            _trig = 3;     // edge, falling
        }
    }
    switch (vector) {
    case  CYGNUM_HAL_INTERRUPT_TIMER0:
        HAL_READ_UINT16(_FRV400_IRC_IRR5, _irr);
        _irr = (_irr & 0xFFF0) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<0);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR5, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0xFFFC) | (_trig<<0);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_TIMER1:
        HAL_READ_UINT16(_FRV400_IRC_IRR5, _irr);
        _irr = (_irr & 0xFF0F) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<4);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR5, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0xFFF3) | (_trig<<2);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_TIMER2:
        HAL_READ_UINT16(_FRV400_IRC_IRR5, _irr);
        _irr = (_irr & 0xF0FF) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<8);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR5, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0xFFCF) | (_trig<<4);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_DMA0:
        HAL_READ_UINT16(_FRV400_IRC_IRR4, _irr);
        _irr = (_irr & 0xFFF0) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<0);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR4, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0xFCFF) | (_trig<<8);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_DMA1:
        HAL_READ_UINT16(_FRV400_IRC_IRR4, _irr);
        _irr = (_irr & 0xFF0F) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<4);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR4, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0xF3FF) | (_trig<<10);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_DMA2:
        HAL_READ_UINT16(_FRV400_IRC_IRR4, _irr);
        _irr = (_irr & 0xF0FF) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<8);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR4, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0xCFFF) | (_trig<<12);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_DMA3:
        HAL_READ_UINT16(_FRV400_IRC_IRR4, _irr);
        _irr = (_irr & 0x0FFF) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<12);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR4, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM0, _tmr);
        _tmr = (_tmr & 0x3FFF) | (_trig<<14);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM0, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_UART0:
        HAL_READ_UINT16(_FRV400_IRC_IRR6, _irr);
        _irr = (_irr & 0xFFF0) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<0);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR6, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM1, _tmr);
        _tmr = (_tmr & 0xFCFF) | (_trig<<8);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM1, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_UART1:
        HAL_READ_UINT16(_FRV400_IRC_IRR6, _irr);
        _irr = (_irr & 0xFF0F) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<4);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR6, _irr);
        HAL_READ_UINT16(_FRV400_IRC_ITM1, _tmr);
        _tmr = (_tmr & 0xF3FF) | (_trig<<10);
        HAL_WRITE_UINT16(_FRV400_IRC_ITM1, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_EXT0:
        HAL_READ_UINT16(_FRV400_IRC_IRR3, _irr);
        _irr = (_irr & 0xFFF0) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<0);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR3, _irr);
        HAL_READ_UINT16(_FRV400_IRC_TM1, _tmr);
        _tmr = (_tmr & 0xFFFC) | (_trig<<0);
        HAL_WRITE_UINT16(_FRV400_IRC_TM1, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_EXT1:
        HAL_READ_UINT16(_FRV400_IRC_IRR3, _irr);
        _irr = (_irr & 0xFF0F) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<4);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR3, _irr);
        HAL_READ_UINT16(_FRV400_IRC_TM1, _tmr);
        _tmr = (_tmr & 0xFFF3) | (_trig<<2);
        HAL_WRITE_UINT16(_FRV400_IRC_TM1, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_EXT2:
        HAL_READ_UINT16(_FRV400_IRC_IRR3, _irr);
        _irr = (_irr & 0xF0FF) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<8);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR3, _irr);
        HAL_READ_UINT16(_FRV400_IRC_TM1, _tmr);
        _tmr = (_tmr & 0xFFCF) | (_trig<<4);
        HAL_WRITE_UINT16(_FRV400_IRC_TM1, _tmr);
        break;
    case  CYGNUM_HAL_INTERRUPT_EXT3:
        HAL_READ_UINT16(_FRV400_IRC_IRR3, _irr);
        _irr = (_irr & 0x0FFF) | ((vector-CYGNUM_HAL_VECTOR_EXTERNAL_INTERRUPT_LEVEL_1+1)<<12);
        HAL_WRITE_UINT16(_FRV400_IRC_IRR3, _irr);
        HAL_READ_UINT16(_FRV400_IRC_TM1, _tmr);
        _tmr = (_tmr & 0xFF3F) | (_trig<<6);
        HAL_WRITE_UINT16(_FRV400_IRC_TM1, _tmr);
        break;
    default:
        ; // Nothing to do
    };
}

void hal_interrupt_set_level(int vector, int level)
{
//    UNIMPLEMENTED(__FUNCTION__);
}

// PCI support

externC void
_frv400_pci_init(void)
{
    static int _init = 0;
    cyg_uint8 next_bus;
    cyg_uint32 cmd_state;

    if (_init) return;
    _init = 1;

    // Enable controller - most of the basic configuration
    // was set up at boot time in "platform.inc"

    // Setup for bus mastering
    HAL_PCI_CFG_READ_UINT32(0, CYG_PCI_DEV_MAKE_DEVFN(0,0),
                            CYG_PCI_CFG_COMMAND, cmd_state);
    if ((cmd_state & CYG_PCI_CFG_COMMAND_MEMORY) == 0) {
        HAL_PCI_CFG_WRITE_UINT32(0, CYG_PCI_DEV_MAKE_DEVFN(0,0),
                                 CYG_PCI_CFG_COMMAND,
                                 CYG_PCI_CFG_COMMAND_MEMORY |
                                 CYG_PCI_CFG_COMMAND_MASTER |
                                 CYG_PCI_CFG_COMMAND_PARITY |
                                 CYG_PCI_CFG_COMMAND_SERR);

        // Setup latency timer field
        HAL_PCI_CFG_WRITE_UINT8(0, CYG_PCI_DEV_MAKE_DEVFN(0,0),
                                CYG_PCI_CFG_LATENCY_TIMER, 32);

        // Configure PCI bus.
        next_bus = 1;
        cyg_pci_configure_bus(0, &next_bus);
    }

}

externC void 
_frv400_pci_translate_interrupt(int bus, int devfn, int *vec, int *valid)
{
    cyg_uint8 req;                                                            
    cyg_uint8 dev = CYG_PCI_DEV_GET_DEV(devfn);

    if (dev == CYG_PCI_MIN_DEV) {
        // On board LAN
        *vec = CYGNUM_HAL_INTERRUPT_LAN;
        *valid = true;
    } else {
        HAL_PCI_CFG_READ_UINT8(bus, devfn, CYG_PCI_CFG_INT_PIN, req);         
        if (0 != req) {                                                           
            CYG_ADDRWORD __translation[4] = {                                       
                CYGNUM_HAL_INTERRUPT_PCIINTC,   /* INTC# */                         
                CYGNUM_HAL_INTERRUPT_PCIINTB,   /* INTB# */                         
                CYGNUM_HAL_INTERRUPT_PCIINTA,   /* INTA# */                         
                CYGNUM_HAL_INTERRUPT_PCIINTD};  /* INTD# */                         
                                                                                
            /* The PCI lines from the different slots are wired like this  */       
            /* on the PCI backplane:                                       */       
            /*                pin6A     pin7B    pin7A   pin8B             */       
            /* I/O Slot 1     INTA#     INTB#    INTC#   INTD#             */       
            /* I/O Slot 2     INTD#     INTA#    INTB#   INTC#             */       
            /* I/O Slot 3     INTC#     INTD#    INTA#   INTB#             */       
            /*                                                             */       
            /* (From PCI Development Backplane, 3.2.2 Interrupts)          */       
            /*                                                             */       
            /* Devsel signals are wired to, resulting in device IDs:       */       
            /* I/O Slot 1     AD30 / dev 19      [(8+1)&3 = 1]             */       
            /* I/O Slot 2     AD29 / dev 18      [(7+1)&3 = 0]             */       
            /* I/O Slot 3     AD28 / dev 17      [(6+1)&3 = 3]             */       
                                                                                
            *vec = __translation[((req+dev)&3)];        
            *valid = true;                                                         
        } else {                                                                    
            /* Device will not generate interrupt requests. */                      
            *valid = false;                                                        
        }                                                                           
        diag_printf("Int - dev: %d, req: %d, vector: %d\n", dev, req, *vec);
    }
}

// PCI configuration space access
#define _EXT_ENABLE 0x80000000  // Could be 0x80000000

static __inline__ cyg_uint32
_cfg_addr(int bus, int devfn, int offset)
{
    return _EXT_ENABLE | (bus << 22) | (devfn << 8) | (offset << 0);
}

externC cyg_uint8 
_frv400_pci_cfg_read_uint8(int bus, int devfn, int offset)
{
    cyg_uint32 cfg_addr, addr, status;
    cyg_uint8 cfg_val = (cyg_uint8)0xFF;

#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%s(bus=%x, devfn=%x, offset=%x) = ", __FUNCTION__, bus, devfn, offset);
#endif // CYGPKG_IO_PCI_DEBUG
    if ((bus == 0) && (CYG_PCI_DEV_GET_DEV(devfn) == 0)) {
        // PCI bridge
        addr = _FRV400_PCI_CONFIG + ((offset << 1) ^ 0x03);
    } else {
        cfg_addr = _cfg_addr(bus, devfn, offset ^ 0x03);
        HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, cfg_addr);
        addr = _FRV400_PCI_CONFIG_DATA + ((offset & 0x03) ^ 0x03);
    }
    HAL_READ_UINT8(addr, cfg_val);
    HAL_READ_UINT16(_FRV400_PCI_STAT_CMD, status);
    if (status & _FRV400_PCI_STAT_ERROR_MASK) {
        // Cycle failed - clean up and get out
        cfg_val = (cyg_uint8)0xFF;
        HAL_WRITE_UINT16(_FRV400_PCI_STAT_CMD, status & _FRV400_PCI_STAT_ERROR_MASK);
    }
#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%x\n", cfg_val);
#endif // CYGPKG_IO_PCI_DEBUG
    HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, 0);
    return cfg_val;
}

externC cyg_uint16 
_frv400_pci_cfg_read_uint16(int bus, int devfn, int offset)
{
    cyg_uint32 cfg_addr, addr, status;
    cyg_uint16 cfg_val = (cyg_uint16)0xFFFF;

#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%s(bus=%x, devfn=%x, offset=%x) = ", __FUNCTION__, bus, devfn, offset);
#endif // CYGPKG_IO_PCI_DEBUG
    if ((bus == 0) && (CYG_PCI_DEV_GET_DEV(devfn) == 0)) {
        // PCI bridge
        addr = _FRV400_PCI_CONFIG + ((offset << 1) ^ 0x02);
    } else {
        cfg_addr = _cfg_addr(bus, devfn, offset ^ 0x02);
        HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, cfg_addr);
        addr = _FRV400_PCI_CONFIG_DATA + ((offset & 0x03) ^ 0x02);
    }
    HAL_READ_UINT16(addr, cfg_val);
    HAL_READ_UINT16(_FRV400_PCI_STAT_CMD, status);
    if (status & _FRV400_PCI_STAT_ERROR_MASK) {
        // Cycle failed - clean up and get out
        cfg_val = (cyg_uint16)0xFFFF;
        HAL_WRITE_UINT16(_FRV400_PCI_STAT_CMD, status & _FRV400_PCI_STAT_ERROR_MASK);
    }
#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%x\n", cfg_val);
#endif // CYGPKG_IO_PCI_DEBUG
    HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, 0);
    return cfg_val;
}

externC cyg_uint32 
_frv400_pci_cfg_read_uint32(int bus, int devfn, int offset)
{
    cyg_uint32 cfg_addr, addr, status;
    cyg_uint32 cfg_val = (cyg_uint32)0xFFFFFFFF;

#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%s(bus=%x, devfn=%x, offset=%x) = ", __FUNCTION__, bus, devfn, offset);
#endif // CYGPKG_IO_PCI_DEBUG
    if ((bus == 0) && (CYG_PCI_DEV_GET_DEV(devfn) == 0)) {
        // PCI bridge
        addr = _FRV400_PCI_CONFIG + (offset << 1);
    } else {
        cfg_addr = _cfg_addr(bus, devfn, offset);
        HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, cfg_addr);
        addr = _FRV400_PCI_CONFIG_DATA;
    }
    HAL_READ_UINT32(addr, cfg_val);
    HAL_READ_UINT16(_FRV400_PCI_STAT_CMD, status);
    if (status & _FRV400_PCI_STAT_ERROR_MASK) {
        // Cycle failed - clean up and get out
        cfg_val = (cyg_uint32)0xFFFFFFFF;
        HAL_WRITE_UINT16(_FRV400_PCI_STAT_CMD, status & _FRV400_PCI_STAT_ERROR_MASK);
    }
#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%x\n", cfg_val);
#endif // CYGPKG_IO_PCI_DEBUG
    HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, 0);
    return cfg_val;
}

externC void
_frv400_pci_cfg_write_uint8(int bus, int devfn, int offset, cyg_uint8 cfg_val)
{
    cyg_uint32 cfg_addr, addr, status;

#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%s(bus=%x, devfn=%x, offset=%x, val=%x)\n", __FUNCTION__, bus, devfn, offset, cfg_val);
#endif // CYGPKG_IO_PCI_DEBUG
    if ((bus == 0) && (CYG_PCI_DEV_GET_DEV(devfn) == 0)) {
        // PCI bridge
        addr = _FRV400_PCI_CONFIG + ((offset << 1) ^ 0x03);
    } else {
        cfg_addr = _cfg_addr(bus, devfn, offset ^ 0x03);
        HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, cfg_addr);
        addr = _FRV400_PCI_CONFIG_DATA + ((offset & 0x03) ^ 0x03);
    }
    HAL_WRITE_UINT8(addr, cfg_val);
    HAL_READ_UINT16(_FRV400_PCI_STAT_CMD, status);
    if (status & _FRV400_PCI_STAT_ERROR_MASK) {
        // Cycle failed - clean up and get out
        HAL_WRITE_UINT16(_FRV400_PCI_STAT_CMD, status & _FRV400_PCI_STAT_ERROR_MASK);
    }
    HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, 0);
}

externC void
_frv400_pci_cfg_write_uint16(int bus, int devfn, int offset, cyg_uint16 cfg_val)
{
    cyg_uint32 cfg_addr, addr, status;

#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%s(bus=%x, devfn=%x, offset=%x, val=%x)\n", __FUNCTION__, bus, devfn, offset, cfg_val);
#endif // CYGPKG_IO_PCI_DEBUG
    if ((bus == 0) && (CYG_PCI_DEV_GET_DEV(devfn) == 0)) {
        // PCI bridge
        addr = _FRV400_PCI_CONFIG + ((offset << 1) ^ 0x02);
    } else {
        cfg_addr = _cfg_addr(bus, devfn, offset ^ 0x02);
        HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, cfg_addr);
        addr = _FRV400_PCI_CONFIG_DATA + ((offset & 0x03) ^ 0x02);
    }
    HAL_WRITE_UINT16(addr, cfg_val);
    HAL_READ_UINT16(_FRV400_PCI_STAT_CMD, status);
    if (status & _FRV400_PCI_STAT_ERROR_MASK) {
        // Cycle failed - clean up and get out
        HAL_WRITE_UINT16(_FRV400_PCI_STAT_CMD, status & _FRV400_PCI_STAT_ERROR_MASK);
    }
    HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, 0);
}

externC void
_frv400_pci_cfg_write_uint32(int bus, int devfn, int offset, cyg_uint32 cfg_val)
{
    cyg_uint32 cfg_addr, addr, status;

#ifdef CYGPKG_IO_PCI_DEBUG
    diag_printf("%s(bus=%x, devfn=%x, offset=%x, val=%x)\n", __FUNCTION__, bus, devfn, offset, cfg_val);
#endif // CYGPKG_IO_PCI_DEBUG
    if ((bus == 0) && (CYG_PCI_DEV_GET_DEV(devfn) == 0)) {
        // PCI bridge
        addr = _FRV400_PCI_CONFIG + (offset << 1);
    } else {
        cfg_addr = _cfg_addr(bus, devfn, offset);
        HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, cfg_addr);
        addr = _FRV400_PCI_CONFIG_DATA;
    }
    HAL_WRITE_UINT32(addr, cfg_val);
    HAL_READ_UINT16(_FRV400_PCI_STAT_CMD, status);
    if (status & _FRV400_PCI_STAT_ERROR_MASK) {
        // Cycle failed - clean up and get out
        HAL_WRITE_UINT16(_FRV400_PCI_STAT_CMD, status & _FRV400_PCI_STAT_ERROR_MASK);
    }
    HAL_WRITE_UINT32(_FRV400_PCI_CONFIG_ADDR, 0);
}

// ------------------------------------------------------------------------
//
// Hardware breakpoint/watchpoint support
// ======================================
//
// Now follows a load of extreme unpleasantness to deal with the totally
// broken debug model of this device.
//
// To modify the special hardware debug registers, it is necessary to put
// the CPU into "debug mode".  This can only be done by executing a break
// instruction, or taking a special hardware break event as described by
// the special hardware debug registers.
//
// But once in debug mode, no break is taken, and break instructions are
// ignored, because we are in debug mode.
//
// So we must exit debug mode for normal running, which you can only do via
// a rett #1 instruction.  Because rett is for returning from traps, it
// halts the CPU if you do it with traps enabled.  So you have to mess
// about disabling traps before the rett.  Also, because rett #1 is for
// returning from a *debug* trap, you can only issue it from debug mode -
// or it halts the CPU.
//
// To be able to set and unset hardware debug breakpoints and watchpoints,
// we must enter debug mode (via a "break" instruction).  Fortunately, it
// is possible to return from a "break" remaining in debug mode, using a
// rett #0, so we can arrange that a break instruction just means "go to
// debug mode".
//
// So we can manipulate the special hardware debug registers by executing a
// "break", doing the work, then doing the magic sequence to rett #1.
// These are encapsulated in HAL_FRV_ENTER_DEBUG_MODE() and
// HAL_FRV_EXIT_DEBUG_MODE() from plf_stub.h
//
// So, we get into break_hander() for two reasons:
//   1) a break instruction.  Detect this and do nothing; return skipping
//      over the break instruction.  CPU remains in debug mode.
//   2) a hardware debug trap.  Continue just as for a normal exception;
//      GDB and the stubs will handle it.  But first, exit debug mode, or
//      stuff happening in the stubs will go wrong.
//
// In order to be certain that we are in debug mode, for performing (2)
// safely, vectors.S installs a special debug trap handler on vector #255.
// That's the reason for break_handler() existing as a separate routine.
// 
// Note that there is no need to define CYGSEM_HAL_FRV_HW_DEBUG for the
// FRV_FRV400 target; while we do use Hardware Debug, we don't use *that*
// sort of hardware debug, specifically we do not use hardware single-step,
// because it breaks as soon as we exit debug mode, ie. whilst we are still
// within the stub.  So in fact defining CYGSEM_HAL_FRV_HW_DEBUG is bad; I
// guess it is mis-named.
//

// ------------------------------------------------------------------------
// First a load of ugly boilerplate for register access.

#ifdef CYGDBG_HAL_DEBUG_GDB_INCLUDE_STUBS

#include <cyg/hal/hal_stub.h>           // HAL_STUB_HW_STOP_NONE et al
#include <cyg/hal/frv_stub.h>           // register names PC, PSR et al
#include <cyg/hal/plf_stub.h>           // HAL_FRV_EXIT_DEBUG_MODE()

// First a load of glue
static inline unsigned get_bpsr(void) {
    unsigned retval;
    asm volatile ( "movsg   bpsr,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_bpsr(unsigned val) {
    asm volatile ( "movgs   %0,bpsr\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dcr(void) {
    unsigned retval;
    asm volatile ( "movsg   dcr,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dcr(unsigned val) {
    asm volatile ( "movgs   %0,dcr\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_brr(void) {
    unsigned retval;
    asm volatile ( "movsg   brr,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_brr(unsigned val) {
    asm volatile ( "movgs   %0,brr\n" : /* no outputs */  : "r" (val) );}

// Four Instruction Break Address Registers
static inline unsigned get_ibar0(void) {
    unsigned retval;
    asm volatile ( "movsg   ibar0,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_ibar0(unsigned val) {
    asm volatile ( "movgs   %0,ibar0\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_ibar1(void) {
    unsigned retval;
    asm volatile ( "movsg   ibar1,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_ibar1(unsigned val){
    asm volatile ( "movgs   %0,ibar1\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_ibar2(void) {
    unsigned retval;
    asm volatile ( "movsg   ibar2,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_ibar2(unsigned val) {
    asm volatile ( "movgs   %0,ibar2\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_ibar3(void) {
    unsigned retval;
    asm volatile ( "movsg   ibar3,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_ibar3(unsigned val){
    asm volatile ( "movgs   %0,ibar3\n" : /* no outputs */  : "r" (val) );}

// Two Data Break Address Registers
static inline unsigned get_dbar0(void) {
    unsigned retval;
    asm volatile ( "movsg   dbar0,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbar0(unsigned val){
    asm volatile ( "movgs   %0,dbar0\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbar1(void){
    unsigned retval;
    asm volatile ( "movsg   dbar1,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbar1(unsigned val){
    asm volatile ( "movgs   %0,dbar1\n" : /* no outputs */  : "r" (val) );}

// Two times two Data Break Data Registers
static inline unsigned get_dbdr00(void){
    unsigned retval;
    asm volatile ( "movsg   dbdr00,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbdr00(unsigned val){
    asm volatile ( "movgs   %0,dbdr00\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbdr01(void){
    unsigned retval;
    asm volatile ( "movsg   dbdr01,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbdr01(unsigned val){
    asm volatile ( "movgs   %0,dbdr01\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbdr10(void){
    unsigned retval;
    asm volatile ( "movsg   dbdr10,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbdr10(unsigned val){
    asm volatile ( "movgs   %0,dbdr10\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbdr11(void){
    unsigned retval;
    asm volatile ( "movsg   dbdr11,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbdr11(unsigned val){
    asm volatile ( "movgs   %0,dbdr11\n" : /* no outputs */  : "r" (val) );}

// Two times two Data Break Mask Registers
static inline unsigned get_dbmr00(void){
    unsigned retval;
    asm volatile ( "movsg   dbmr00,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbmr00(unsigned val){
    asm volatile ( "movgs   %0,dbmr00\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbmr01(void){
    unsigned retval;
    asm volatile ( "movsg   dbmr01,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbmr01(unsigned val){
    asm volatile ( "movgs   %0,dbmr01\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbmr10(void){
    unsigned retval;
    asm volatile ( "movsg   dbmr10,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbmr10(unsigned val){
    asm volatile ( "movgs   %0,dbmr10\n" : /* no outputs */  : "r" (val) );}

static inline unsigned get_dbmr11(void){
    unsigned retval;
    asm volatile ( "movsg   dbmr11,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_dbmr11(unsigned val){
    asm volatile ( "movgs   %0,dbmr11\n" : /* no outputs */  : "r" (val) );}

// and here's the prototype.  Which compiles, believe it or not.
static inline unsigned get_XXXX(void){
    unsigned retval;
    asm volatile ( "movsg   XXXX,%0\n" : "=r" (retval) : /* no inputs */  );
    return retval;}
static inline void set_XXXX(unsigned val){
    asm volatile ( "movgs   %0,XXXX\n" : /* no outputs */  : "r" (val) );}

// ------------------------------------------------------------------------
// This is called in the same manner as exception_handler() in hal_misc.c
// Comments compare and contrast what we do here.

static unsigned int saved_brr = 0;

void
break_handler(HAL_SavedRegisters *regs)
{
    unsigned int i, old_bpsr;

    // See if it an actual "break" instruction.
    i = get_brr();
    saved_brr |= i; // do not lose previous state
    // Acknowledge the trap, clear the "factor" (== cause)
    set_brr( 0 );

    // Now leave debug mode so that it's safe to run the stub code.

    // Unfortunately, leaving debug mode isn't a self-contained
    // operation.  The only means of doing it is with a "rett #1"
    // instruction, which will also restore the previous values of
    // the ET and S status flags.  We can massage the BPSR
    // register so that the flags keep their current values, but
    // we need to save the old one first.
    i = old_bpsr = get_bpsr ();
    i |= _BPSR_BS; // Stay in supervisor mode
    i &= ~_BPSR_BET; // Keep traps disabled
    set_bpsr (i);
    HAL_FRV_EXIT_DEBUG_MODE();

    // Only perturb this variable if stopping, not
    // just for a break instruction.
    _hal_registers = regs; 

    // Continue with the standard mechanism:
    __handle_exception();

    // Go back into debug mode.
    HAL_FRV_ENTER_DEBUG_MODE();
    // Restore the original BPSR register.
    set_bpsr (old_bpsr);
    return;
}

// ------------------------------------------------------------------------

// Now the routines to manipulate said hardware break and watchpoints.

int cyg_hal_plf_hw_breakpoint(int setflag, void *vaddr, int len)
{
    unsigned int addr = (unsigned)vaddr;
    unsigned int dcr;
    unsigned int retcode = 0;

    HAL_FRV_ENTER_DEBUG_MODE();
    dcr = get_dcr();

    // GDB manual suggests that idempotency is required, so first remove
    // any identical BP in residence.  Implements remove arm anyway.
    if ( 0 != (dcr & (_DCR_IBE0 | _DCR_IBCE0)) &&
         get_ibar0() == addr                       )
        dcr &=~(_DCR_IBE0 | _DCR_IBCE0);
    else if ( 0 != (dcr & (_DCR_IBE1 | _DCR_IBCE1)) &&
              get_ibar1() == addr                       )
        dcr &=~(_DCR_IBE1 | _DCR_IBCE1);
    else if ( 0 != (dcr & (_DCR_IBE2 | _DCR_IBCE2)) &&
              get_ibar2() == addr                       )
        dcr &=~(_DCR_IBE2 | _DCR_IBCE2);
    else if ( 0 != (dcr & (_DCR_IBE3 | _DCR_IBCE3)) &&
              get_ibar3() == addr                       )
        dcr &=~(_DCR_IBE3 | _DCR_IBCE3);
    else
        retcode = -1;
    
    if (setflag) {
        retcode = 0; // it is OK really
        if ( 0 == (dcr & (_DCR_IBE0 | _DCR_IBCE0)) ) {
	    set_ibar0(addr);
            dcr |= _DCR_IBE0;
        }
	else if ( 0 == (dcr & (_DCR_IBE1 | _DCR_IBCE1)) ) {
	    set_ibar1(addr);
            dcr |= _DCR_IBE1;
        }
	else if ( 0 == (dcr & (_DCR_IBE2 | _DCR_IBCE2)) ) {
	    set_ibar2(addr);
            dcr |= _DCR_IBE2;
        }
	else if ( 0 == (dcr & (_DCR_IBE3 | _DCR_IBCE3)) ) {
	    set_ibar3(addr);
            dcr |= _DCR_IBE3;
        }
	else
	    retcode = -1;
    }

    if ( 0 == retcode )
        set_dcr(dcr);
    HAL_FRV_EXIT_DEBUG_MODE();
    return retcode;
}

int cyg_hal_plf_hw_watchpoint(int setflag, void *vaddr, int len, int type)
{
    unsigned int addr = (unsigned)vaddr;
    unsigned int mode;
    unsigned int dcr;
    unsigned int retcode = 0;
    unsigned long long mask;
    unsigned int mask0, mask1;
    int i;

    // Check the length fits within one block.
    if ( ((~7) & (addr + len - 1)) != ((~7) & addr) )
        return -1;

    // Assuming big-endian like the platform seems to be...

    // Get masks for the 8-byte span.  00 means enabled, ff means ignore a
    // byte, which is why this looks funny at first glance.
    mask = 0x00ffffffffffffffULL >> ((len - 1) << 3);
    for (i = 0; i < (addr & 7); i++) {
        mask >>= 8;
        mask |= 0xff00000000000000ULL;
    }

    mask0 = mask >> 32;
    mask1 = mask & 0xffffffffULL;

    addr &=~7; // round to 8-byte block

    HAL_FRV_ENTER_DEBUG_MODE();
    dcr = get_dcr();

    // GDB manual suggests that idempotency is required, so first remove
    // any identical WP in residence.  Implements remove arm anyway.
    if (      0 != (dcr & (7 * _DCR_DBASE0)) &&
              get_dbar0() == addr            &&
              get_dbmr00() == mask0 && get_dbmr01() == mask1 )
        dcr &=~(7 * _DCR_DBASE0);
    else if ( 0 != (dcr & (7 * _DCR_DBASE1)) &&
              get_dbar1() == addr&&
              get_dbmr10() == mask0 && get_dbmr11() == mask1 )
        dcr &=~(7 * _DCR_DBASE1);
    else
        retcode = -1;

    if (setflag) {
        retcode = 0; // it is OK really
        if      (type == 2)       mode = 2; // break on write
        else if (type == 3)       mode = 4; // break on read
        else if (type == 4)       mode = 6; // break on any access
        else {
            mode = 0; // actually add no enable at all.
            retcode = -1;
        }
        if ( 0 == (dcr & (7 * _DCR_DBASE0)) ) {
            set_dbar0(addr);
            // Data and Mask 0,1 to zero (mask no bits/bytes)
            set_dbdr00(0); set_dbdr01(0); set_dbmr00(mask0); set_dbmr01(mask1);
            mode *= _DCR_DBASE0;
            dcr |= mode;
        }
        else if ( 0 == (dcr & (7 * _DCR_DBASE1)) ) {
            set_dbar1(addr);
            set_dbdr10(0); set_dbdr11(0); set_dbmr10(mask0); set_dbmr11(mask1);
            mode *= _DCR_DBASE1;
            dcr |= mode;
        }
        else
            retcode = -1;
    }

    if ( 0 == retcode )
        set_dcr(dcr);
    HAL_FRV_EXIT_DEBUG_MODE();
    return retcode;
}

// Return indication of whether or not we stopped because of a
// watchpoint or hardware breakpoint. If stopped by a watchpoint,
// also set '*data_addr_p' to the data address which triggered the
// watchpoint.
int cyg_hal_plf_is_stopped_by_hardware(void **data_addr_p)
{
    unsigned int brr;
    int retcode = HAL_STUB_HW_STOP_NONE;
    unsigned long long mask;

    // There was a debug event. Check the BRR for details
    brr = saved_brr;
    saved_brr = 0;

    if ( brr & (_BRR_IB0 | _BRR_IB1 | _BRR_IB2 | _BRR_IB3) ) {
        // then it was an instruction break
        retcode = HAL_STUB_HW_STOP_BREAK;
    }
    else if ( brr & (_BRR_DB0 | _BRR_DB1) ) {
        unsigned int addr, kind;
        kind = get_dcr();
        if ( brr & (_BRR_DB0) ) {
            addr = get_dbar0();
            kind &= 7 * _DCR_DBASE0;
            kind /= _DCR_DBASE0;
            mask = (((unsigned long long)get_dbmr00())<<32) | (unsigned long long)get_dbmr01();
        } else {
            addr = get_dbar1();
            kind &= 7 * _DCR_DBASE1;
            kind /= _DCR_DBASE1;
            mask = (((unsigned long long)get_dbmr10())<<32) | (unsigned long long)get_dbmr11();
        }

        if ( data_addr_p ) {
            // Scan for a zero byte in the mask - this gives the true address.
            //              0123456789abcdef
            while ( 0 != (0xff00000000000000LLU & mask) ) {
                mask <<= 8;
                addr++;
            }
            *data_addr_p = (void *)addr;
        }

        // Inverse of the mapping above in the "set" code.
        if      (kind == 2)       retcode = HAL_STUB_HW_STOP_WATCH;
        else if (kind == 6)       retcode = HAL_STUB_HW_STOP_AWATCH;
        else if (kind == 4)       retcode = HAL_STUB_HW_STOP_RWATCH;
    }
    return retcode;
}

// ------------------------------------------------------------------------

#endif // CYGDBG_HAL_DEBUG_GDB_INCLUDE_STUBS

/*------------------------------------------------------------------------*/
// EOF frv400_misc.c
