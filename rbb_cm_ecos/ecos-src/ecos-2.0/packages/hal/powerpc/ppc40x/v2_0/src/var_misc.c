//==========================================================================
//
//      var_misc.c
//
//      HAL implementation miscellaneous functions
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
// Author(s):    jskov
// Contributors: jskov, gthomas
// Date:         2000-02-04
// Purpose:      HAL miscellaneous functions
// Description:  This file contains miscellaneous functions provided by the
//               HAL.
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/hal.h>

#define CYGARC_HAL_COMMON_EXPORT_CPU_MACROS
#include <cyg/hal/ppc_regs.h>
#include <cyg/infra/cyg_type.h>

#include <cyg/hal/hal_mem.h>

void hal_ppc40x_clock_initialize(cyg_uint32 period);

//--------------------------------------------------------------------------
void hal_variant_init(void)
{
    // Initialize real-time clock (for delays, etc, even if kernel doesn't use it)
    hal_ppc40x_clock_initialize(CYGNUM_HAL_RTC_PERIOD);
}


//--------------------------------------------------------------------------
// Variant specific idle thread action.
bool
hal_variant_idle_thread_action( cyg_uint32 count )
{
    // Let architecture idle thread action run
    return true;
}

//---------------------------------------------------------------------------
// Use MMU resources to map memory regions.  
// Takes and returns an int used to ID the MMU resource to use. This ID
// is increased as resources are used and should be used for subsequent
// invocations.
//
// The PPC4xx CPUs do not have BATs. Fortunately we don't currently
// use the MMU, so we can simulate BATs by using the TLBs.
int
cyg_hal_map_memory (int id, CYG_ADDRESS virt, CYG_ADDRESS phys, 
                    cyg_int32 size, cyg_uint8 flags)
{
    cyg_uint32 epn, rpn;
    int sv, lv, max_tlbs;

    // There are 64 TLBs.
    max_tlbs = 64;

    // Use the smallest "size" value which is big enough (round up)
    for (sv = 0, lv = 0x400;  sv < 8;  sv++, lv <<= 2) {
        if (lv >= size) break;        
    }

    // Note: the process ID comes from the PID register (always 0)
    epn = (virt & M_EPN_EPNMASK) | M_EPN_EV | M_EPN_SIZE(sv);
    rpn = (phys & M_RPN_RPNMASK) | M_RPN_EX | M_RPN_WR;

    if (flags & CYGARC_MEMDESC_CI) {
        rpn |= M_RPN_I;
    }

    if (flags & CYGARC_MEMDESC_GUARDED) 
        rpn |= M_RPN_G;

    CYGARC_TLBWE(id, epn, rpn);
    id++;

    // Make caches default disabled when MMU is disabled.

    return id;
}


// Initialize MMU to a sane (NOP) state.
//
// Initialize TLBs with 0, Valid bits unset.
void
cyg_hal_clear_MMU (void)
{
    cyg_uint32 tlbhi = 0;
    cyg_uint32 tlblo = 0;
    int id, max_tlbs;

    // There are 64 TLBs.
    max_tlbs = 64;

    CYGARC_MTSPR (SPR_PID, 0);

    for (id = 0; id < max_tlbs; id++) {
        CYGARC_TLBWE(id, tlbhi, tlblo);
    }
}

//--------------------------------------------------------------------------
// Clock control - use the programmable (variable period) timer

static cyg_uint32 _period;
extern cyg_uint32 _hold_tcr;  // Shadow of TCR register which can't be read

void 
hal_ppc40x_clock_initialize(cyg_uint32 period)
{
    cyg_uint32 tcr;    

    // Enable auto-reload
    CYGARC_MFSPR(SPR_TCR, tcr);
    tcr = _hold_tcr;
    tcr |= TCR_ARE;
    CYGARC_MTSPR(SPR_TCR, tcr);
    _hold_tcr = tcr;

    // Set up the counter register
    _period = period;
    CYGARC_MTSPR(SPR_PIT, period);
}

// Returns the number of clocks since the last interrupt
externC void 
hal_ppc40x_clock_read(cyg_uint32 *val)
{
    cyg_uint32 cur_val;

    CYGARC_MFSPR(SPR_PIT, cur_val);
    *val = _period - cur_val;
}

externC void 
hal_ppc40x_clock_reset(cyg_uint32 vector, cyg_uint32 period)
{
    hal_ppc40x_clock_initialize(period);
}

//
// Delay for the specified number of microseconds.
// Assumption: _period has been set already and corresponds to the
// system clock frequency, normally 10ms.
//

externC void 
hal_ppc40x_delay_us(int us)
{
    cyg_uint32 delay_period, delay, diff;
    cyg_uint32 pit_val1, pit_val2;

    delay_period = (_period * us) / 10000;
    delay = 0;
    CYGARC_MFSPR(SPR_PIT, pit_val1);
    while (delay < delay_period) {
        // Wait for clock to "tick"
        while (true) {
            CYGARC_MFSPR(SPR_PIT, pit_val2);
            if (pit_val2 != pit_val1) break;
        }
        if (pit_val2 > pit_val1) {
            diff = pit_val2 - pit_val1;
        } else {
            diff = (pit_val2 + _period) - pit_val1;
        }
        delay += diff;        
        pit_val1 = pit_val2;
    }
}

//--------------------------------------------------------------------------
// End of var_misc.c
