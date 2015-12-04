#ifndef CYGONCE_VAR_INTR_H
#define CYGONCE_VAR_INTR_H
//=============================================================================
//
//      var_intr.h
//
//      Variant HAL interrupt and clock support
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
// Author(s):   nickg
// Contributors:nickg, jskov, jlarmour, hmt, gthomas
// Date:        2000-04-02
// Purpose:     Variant interrupt support
// Description: The macros defined here provide the HAL APIs for handling
//              interrupts and the clock on the PPC40x variant CPUs.
// Usage:       Is included via the architecture interrupt header:
//              #include <cyg/hal/hal_intr.h>
//              ...
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <cyg/hal/plf_intr.h>

// Special handling for additional exceptions

#define CYGNUM_HAL_VECTOR_TIMERS            16   // Note: must handle all 3!
#define CYGNUM_HAL_VECTOR_DATA_TLB_MISS     17
#define CYGNUM_HAL_VECTOR_INSTR_TLB_MISS    18
#define CYGNUM_HAL_VECTOR_DEBUG             32

// No 'trace'/'single step' trap on this processor
#define CYGNUM_HAL_NO_VECTOR_TRACE

#define CYGNUM_HAL_VSR_MAX                  CYGNUM_HAL_VECTOR_DEBUG

// Special handling for interrupts

#define CYGHWR_HAL_INTERRUPT_CONTROLLER_ACCESS_DEFINED

// Additional interrupt sources which are supported by the 40x
#define CYGNUM_HAL_INTERRUPT_CRITICAL         2
#define CYGNUM_HAL_INTERRUPT_SERIAL_RCV       3
#define CYGNUM_HAL_INTERRUPT_SERIAL_XMT       4
#define CYGNUM_HAL_INTERRUPT_JTAG_RCV         5
#define CYGNUM_HAL_INTERRUPT_JTAG_XMT         6
#define CYGNUM_HAL_INTERRUPT_DMA0             7
#define CYGNUM_HAL_INTERRUPT_DMA1             8
#define CYGNUM_HAL_INTERRUPT_DMA2             9
#define CYGNUM_HAL_INTERRUPT_DMA3            10
#define CYGNUM_HAL_INTERRUPT_EXT0            11
#define CYGNUM_HAL_INTERRUPT_EXT1            12
#define CYGNUM_HAL_INTERRUPT_EXT2            13
#define CYGNUM_HAL_INTERRUPT_EXT3            14
#define CYGNUM_HAL_INTERRUPT_EXT4            15
#define CYGNUM_HAL_INTERRUPT_VAR_TIMER       16
#define CYGNUM_HAL_INTERRUPT_FIXED_TIMER     17
#define CYGNUM_HAL_INTERRUPT_WATCHDOG_TIMER  18

#define CYGNUM_HAL_ISR_MAX                   CYGNUM_HAL_INTERRUPT_WATCHDOG_TIMER

//--------------------------------------------------------------------------
// Interrupt controller access

externC void hal_ppc40x_interrupt_mask(int);
externC void hal_ppc40x_interrupt_unmask(int);
externC void hal_ppc40x_interrupt_acknowledge(int);
externC void hal_ppc40x_interrupt_configure(int, int, int);
externC void hal_ppc40x_interrupt_set_level(int, int);

#define HAL_INTERRUPT_MASK( _vector_ )                     \
    hal_ppc40x_interrupt_mask( _vector_ ) 
#define HAL_INTERRUPT_UNMASK( _vector_ )                   \
    hal_ppc40x_interrupt_unmask( _vector_ )
#define HAL_INTERRUPT_ACKNOWLEDGE( _vector_ )              \
    hal_ppc40x_interrupt_acknowledge( _vector_ )
#define HAL_INTERRUPT_CONFIGURE( _vector_, _level_, _up_ ) \
    hal_ppc40x_interrupt_configure( _vector_, _level_, _up_ )
#define HAL_INTERRUPT_SET_LEVEL( _vector_, _level_ )       \
    hal_ppc40x_interrupt_set_level( _vector_, _level_ )

//--------------------------------------------------------------------------
// Clock control

externC void hal_ppc40x_clock_initialize(cyg_uint32);
externC void hal_ppc40x_clock_read(cyg_uint32 *);
externC void hal_ppc40x_clock_reset(cyg_uint32, cyg_uint32);
externC void hal_ppc40x_delay_us(int);

#define CYGHWR_HAL_CLOCK_DEFINED
#define HAL_CLOCK_INITIALIZE( _period_ )   hal_ppc40x_clock_initialize( _period_ )
#define HAL_CLOCK_RESET( _vec_, _period_ ) hal_ppc40x_clock_reset( _vec_, _period_ )
#define HAL_CLOCK_READ( _pvalue_ )         hal_ppc40x_clock_read( _pvalue_ )
#ifdef CYGVAR_KERNEL_COUNTERS_CLOCK_LATENCY
#define HAL_CLOCK_LATENCY( _pvalue_ )      HAL_CLOCK_READ( (cyg_uint32 *)_pvalue_ )
#endif

#define HAL_DELAY_US(n) hal_ppc40x_delay_us(n)
#define CYGNUM_HAL_INTERRUPT_RTC             CYGNUM_HAL_INTERRUPT_VAR_TIMER

//-----------------------------------------------------------------------------
// Symbols used by assembly code
#define CYGARC_VARIANT_DEFS                                                     \
    DEFINE(CYGNUM_HAL_VECTOR_TIMERS, CYGNUM_HAL_VECTOR_TIMERS);                 \
    DEFINE(CYGNUM_HAL_INTERRUPT_VAR_TIMER,CYGNUM_HAL_INTERRUPT_VAR_TIMER);

//-----------------------------------------------------------------------------
#endif // ifndef CYGONCE_VAR_INTR_H
// End of var_intr.h
