##
#=============================================================================
##      platform.S
##
##      Bcm33xx MIPS platform code
##
##=============================================================================
#####ECOSGPLCOPYRIGHTBEGIN####
## -------------------------------------------
## This file is part of eCos, the Embedded Configurable Operating System.
## Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
##
## eCos is free software; you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free
## Software Foundation; either version 2 or (at your option) any later version.
##
## eCos is distributed in the hope that it will be useful, but WITHOUT ANY
## WARRANTY; without even the implied warranty of MERCHANTABILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License along
## with eCos; if not, write to the Free Software Foundation, Inc.,
## 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
##
## As a special exception, if other files instantiate templates or use macros
## or inline functions from this file, or you compile this file and link it
## with other works to produce a work based on this file, this file does not
## by itself cause the resulting work to be covered by the GNU General Public
## License. However the source code for this file must still be made available
## in accordance with section (3) of the GNU General Public License.
##
## This exception does not invalidate any other reasons why a work based on
## this file might be covered by the GNU General Public License.
##
## Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
## at http://sources.redhat.com/ecos/ecos-license/
## -------------------------------------------
#####ECOSGPLCOPYRIGHTEND####
## ----------------------------------------------------------------------
## Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
## ----------------------------------------------------------------------
##=============================================================================
#######DESCRIPTIONBEGIN####
##
## Author(s):   msieweke
## Contributors:dmoseley, jskov
## Date:        2003-06-13
## Purpose:     Bcm33xx MIPS platform code
## Description: Platform specific code for Bcm33xx reference design
##
##
##
##
######DESCRIPTIONEND####
##
##=============================================================================

#include <pkgconf/system.h>
#include <pkgconf/hal.h>

#ifdef CYGPKG_KERNEL
# include <pkgconf/kernel.h>
#endif

#include <cyg/hal/arch.inc>
#include <cyg/hal/plf_io.h>
#include <cyg/hal/hal_arch.h>
#include <cyg/hal/plf_defs.inc>
#include CYGBLD_HAL_PLATFORM_H

##-----------------------------------------------------------------------------

##-----------------------------------------------------------------------------
# Platform Initialization.
# This code performs platform specific initialization.

## This section probably will never be used because our boot ROM is a separate
## project - not based on eCos.
##-----------------------------------------------------------------------------
## MEMC initialization.
##

##-----------------------------------------------------------------------------
# Utility functions.  Some were provided in VxWorks, so it's easier to provide
# them here.

FUNC_START(intCRGet)
        mfc0    v0, cause
        nop
        nop
        jr      ra
        nop
FUNC_END(intCRGet)
       
FUNC_START(intSRGet)
        mfc0    v0, status
        nop
        nop
        jr      ra
        nop
FUNC_END(intSRGet)

# 
# unsigned int cp0Get( unsigned int reg, unsigned int sel );
FUNC_START(cp0Get)
        andi    a0, 31          # Limit reg to 0-31
        andi    a1, 7           # Limit sel to 0-7
        sll     a0, a0, 11      # Shift CP0 register number to correct place
        or      a0, a1          # Or in select number
        lw      v0, 1f          # Load "mfc0" instruction
        or      v0, a0          # Or in register number and select number
        la      a1, 2f          # Load address of temporary location
        lui     a0, 0xa000
        or      a1, a0          # Make it an un-cached address
        sw      v0, 0(a1)       # Store "mfc0" instruction
        sync
        jr      a1              # Jump to "mfc0" instruction
        nop
1:
        mfc0    v0, $0
2:
        nop
        nop
        nop
        jr      ra
        nop
FUNC_END(cp0Get)

# void cp0Set( unsigned int reg, unsigned int sel, unsigned int value );
FUNC_START(cp0Set)
        andi    a0, 31          # Limit reg to 0-31
        andi    a1, 7           # Limit sel to 0-7
        sll     a0, a0, 11      # Shift CP0 register number to correct place
        or      a0, a1          # Or in select number
        lw      v0, 1f          # Load "mtc0" instruction
        or      v0, a0          # Or in register number and select number
        la      a1, 2f          # Load address of temporary location
        lui     a0, 0xa000
        or      a1, a0          # Make it an un-cached address
        sw      v0, 0(a1)       # Store "mfc0" instruction
        sync
        jr      a1              # Jump to "mfc0" instruction
        nop
1:
        mtc0    a2, $0
2:
        nop
        nop
        nop
        jr      ra
        nop
FUNC_END(cp0Set)

# The error level bit in the status register
#define CP0_CFG_ICSHEN     (1<<31)
#define CP0_CFG_DCSHEN     (1<<30)
#define BCM33xx_CONFIG     $22
#define CP0_K0Coherency		 	(0x7 << 0)	 // 0 no Coherency
#define CP0_K0Uncached		 	(0x2 << 0)	 // 2 = Uncached
#define CP0_K0WriteThrough	 	(0x0 << 0)	 // 0 = Cached, Dcache write thru
#define CP0_K0Writeback		 	(0x3)		 // 0 = Cached, Dcache write back
#define CP0_KUCoherency		 	(0x7 << 25)	 // 0 no Coherency
#define CP0_KUUncached		 	(0x2 << 25)	 // 2 = Uncached
#define CP0_KUWriteThrough	 	(0x0 << 25)	 // 0 = Cached, Dcache write thru
#define CP0_KUWriteback		 	(0x3 << 25)	 // 0 = Cached, Dcache write back
#define CP0_K23Coherency	 	(0x7 << 28)	 // 0 no Coherency
#define CP0_K23Uncached		 	(0x2 << 28)	 // 2 = Uncached
#define CP0_K23WriteThrough	 	(0x0 << 28)	 // 0 = Cached, Dcache write thru
#define CP0_K23Writeback	 	(0x3 << 28)	 // 0 = Cached, Dcache write back
#define K0BASE             0x80000000
#define K1BASE             0xa0000000
        .extern hal_icache_size
        .extern hal_dcache_size

FUNC_START(initCache)
        # Start executing in non-cached space (K1).
        la      t0, 1f
        or      t0, K1BASE
        j       t0
        nop
1:
        # Load cache sizes before enabling cache.  These values were calculated
        # in hal_size_icache() and hal_size_dcache().
        la      t3, hal_icache_size
        or      t3, K1BASE
        lw      t3, 0(t3)
        la      t4, hal_dcache_size
        or      t4, K1BASE
        lw      t4, 0(t4)

        # enable caches so they can be cleared
        li      t1, CP0_CFG_ICSHEN|CP0_CFG_DCSHEN
        mtc0    t1, BCM33xx_CONFIG

        # NOTE: only K0 coherency is implemented, so this is technically illegal.
        mfc0    t1, config0
        and     t1, ~CP0_K0Coherency

#ifdef CYGSEM_HAL_DCACHE_STARTUP_MODE_COPYBACK
        #warning "Writeback mode selected"
        or      t1,  CP0_K0Writeback
#else
        #warning "Writethrough mode selected"
        or      t1,  CP0_K0WriteThrough
#endif

        mtc0    t1, config0

        mfc0    t1, status
        and     t1, ~SR_ERL
        mtc0    t1, status

        # clear I and DCache...
        mtc0    $0, taglo, 0             //Set tagLO
        mtc0    $0, taglo, 1             //Set dataLO
        .set noreorder
        la      a1, clearicache
        li      t1, K1BASE      # force to uncached space
        or      a1, t1
        j       a1
        nop
clearicache:
        li      t2, K1BASE
        add     t1, t2, t3
        sub     t1, 0x10            # use this one for speed
1:      cache   0x8, 0(t1)          # clear icache
        bne     t1, t2, 1b
        sub     t1, 0x10            # use this one for speed

        la      a1, cleardcache
        li      t1, ~K1BASE         # force to cached space
        and     a1, t1
        li      t1, K0BASE          # force to cached space
        or      a1, t1
        j       a1
        nop
cleardcache:
        li      t2, K1BASE
        add     t1, t2, t4
        sub     t1, 0x10            # use this one for speed
1:      cache   0x9, 0(t1)          # clear dcache
        bne     t1, t2, 1b
        sub     t1, 0x10            # use this one for speed

        la      a1, doneclearcache  # back to linked mode
        j       a1
        nop

doneclearcache:                             # now set correct cache mode
        mfc0    t1, BCM33xx_CONFIG
        nop
        and     t1, t1, ~(CP0_CFG_ICSHEN|CP0_CFG_DCSHEN)

#if (CYGSEM_HAL_ENABLE_ICACHE_ON_STARTUP)
        #warning "I-Cache Enabled"
        or      t1, t1, (CP0_CFG_ICSHEN)
#else
        #warning "I-cache disabled"
#endif                  
                                        
#if (CYGSEM_HAL_ENABLE_DCACHE_ON_STARTUP)
        #warning "D-Cache Enabled"
        or      t1, t1, (CP0_CFG_DCSHEN)
#else
        #warning "D-cache disabled"
#endif                                                          

        mtc0    t1, BCM33xx_CONFIG      # enable configured caches
        nop
#if 0
        #warning Enabling branch prediction
        # Enable branch prediction
        mfc0    t0, $22, 5      # get cp0 reg22, select 5
        li      t1, 0xfffeffff
        and     t0, t1, t0
        mtc0    t0, $22, 5      # set cp0 reg22, select 5
        nop
        nop
#else
        #warning NOT enabling branch prediction
#endif
        jr      ra
        nop
FUNC_END(initCache)


##-----------------------------------------------------------------------------
## end of platform.S
