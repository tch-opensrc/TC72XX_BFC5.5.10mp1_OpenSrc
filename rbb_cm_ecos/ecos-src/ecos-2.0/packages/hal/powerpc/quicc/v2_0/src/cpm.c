//==========================================================================
//
//      cpm.c
//
//      PowerPC QUICC support functions
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 2003 Gary Thomas
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
// Author(s):    Gary Thomas 
// Contributors: 
// Date:         2003-03-04
// Purpose:      Common support for the QUICC/CPM
// Description:  
//               
// Usage:
// Notes:        
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/hal.h>
#include <pkgconf/hal_powerpc_quicc.h>
#include <cyg/infra/cyg_type.h>
#include <cyg/hal/hal_arch.h>
#include <string.h>           // memset

#ifdef CYGPKG_HAL_POWERPC_MPC860
// eCos headers decribing PowerQUICC:
#include <cyg/hal/quicc/ppc8xx.h>

// Information about DPRAM usage
// This lets the CPM/DPRAM information be shared by all environments
//
static short *nextBd = (short *)(CYGHWR_HAL_VSR_TABLE + 0x1F0);

/*
 * Reset the communications processor
 */

void
_mpc8xx_reset_cpm(void)
{
    EPPC *eppc = eppc_base();
    static int init_done = 0;

    if (init_done) return;
    init_done++;

    eppc->cp_cr = QUICC_CPM_CR_RESET | QUICC_CPM_CR_BUSY;
    memset(eppc->pram, 0, sizeof(eppc->pram));
    while (eppc->cp_cr & QUICC_CPM_CR_BUSY)
        CYG_EMPTY_STATEMENT;

    *nextBd = QUICC_BD_BASE;
}

//
// Allocate a chunk of memory in the shared CPM memory, typically
// used for buffer descriptors, etc.  The length will be aligned
// to a multiple of 8 bytes.
//
unsigned short
_mpc8xx_allocBd(int len)
{
    unsigned short bd;

    bd = *nextBd;
    if ((bd < QUICC_BD_BASE) || (bd > QUICC_BD_END)) {
        // Most likely not set up - make a guess :-(
        bd = *nextBd = QUICC_BD_BASE+0x400;
    }
    len = (len + 7) & ~7;  // Multiple of 8 bytes
    *nextBd += len;
    if (*nextBd >= QUICC_BD_END) {
        *nextBd = QUICC_BD_BASE;
    }
    return bd;
}

#endif // CYGPKG_HAL_POWERPC_MPC860
// EOF cpm.c
