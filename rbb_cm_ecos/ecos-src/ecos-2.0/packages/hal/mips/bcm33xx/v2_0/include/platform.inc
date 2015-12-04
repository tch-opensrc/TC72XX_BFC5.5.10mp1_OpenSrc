#ifndef CYGONCE_HAL_PLATFORM_INC
#define CYGONCE_HAL_PLATFORM_INC
##=============================================================================
##
##      platform.inc
##
##      Bcm933xx board assembler header file
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
## Date:        2001-03-20
## Purpose:     Bcm933xx board definitions.
## Description: This file contains various definitions and macros that are
##              useful for writing assembly code for the Bcm933xx board.
## Usage:
##              #include <cyg/hal/platform.inc>
##              ...
##              
##
######DESCRIPTIONEND####
##
##=============================================================================

#include <cyg/hal/mips.inc>

#include <cyg/hal/hal_arch.h>
#include <cyg/hal/plf_io.h>

#define CYGARC_HAL_COMMON_EXPORT_CPU_MACROS
#include <cyg/hal/mips-regs.h>
#include <cyg/hal/var_arch.h>

#------------------------------------------------------------------------------
# Interrupt controller init

# MIPS interrupt decoding macros.
#
# These macros are the same as those in arch.inc, except that they support
# software interrupts. The default macros found in arch.inc only support
# the MIPS core external and timer interrupts.


#ifndef CYGPKG_HAL_MIPS_INTC_DEFINED

#ifndef CYGPKG_HAL_MIPS_INTC_DECODE_DEFINED
#define CYGPKG_HAL_MIPS_INTC_DECODE_DEFINED
   .macro   hal_intc_decode vnum
   mfc0  v1,status      # get status register (interrupt mask)
   nop                  # delay slot
   mfc0  v0,cause       # get cause register
   nop                  # delay slot
   and   v0,v0,v1       # apply interrupt mask
   srl   v0,v0,8        # shift interrupt bits down
   andi  v0,v0,0xff     # isolate 6 interrupt bits
   la    v1,hal_intc_translation_table
   add   v0,v0,v1       # index into table
   lb    \vnum,0(v0)    # pick up vector number
   .endm

   # Software interrupt 0 has the highest priority. Followed by sw1, external
   # interrupt0, 1, 2, 3, 4, and the timer interrupt, which has the lowest priority.
   .macro   hal_intc_decode_data
hal_intc_translation_table:   
   .byte   0, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   3, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   4, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   3, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   5, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   3, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   4, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   3, 0, 1, 0    
   .byte   2, 0, 1, 0    
   .byte   6, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   4, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   5, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   4, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   7, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   4, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   5, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   4, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   6, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   4, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   5, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   4, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .byte   3, 0, 1, 0              
   .byte   2, 0, 1, 0              
   .endm
#endif
#endif


#------------------------------------------------------------------------------
# Diagnostic macros

#ifndef CYGPKG_HAL_MIPS_DIAG_DEFINED

        .macro  hal_diag_init
        .endm

        .macro  hal_diag_excpt_start
        .endm

        .macro  hal_diag_intr_start
        .endm

        .macro  hal_diag_restore
        .endm

#define CYGPKG_HAL_MIPS_DIAG_DEFINED

#endif // ifndef CYGPKG_HAL_MIPS_DIAG_DEFINED
        
#------------------------------------------------------------------------------
# MEMC macros.
# 
        
#------------------------------------------------------------------------------
# Cache macros.
# 
        
#ifndef CYGPKG_HAL_MIPS_CACHE_DEFINED

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

.macro  hal_cache_init

        # Setup a temporary stack pointer for running C code.
        la      a0,__interrupt_stack
        move    sp,a0
        CYGARC_ADDRESS_REG_UNCACHED(sp)
        
        // Skip cache initialization if it is already done
        mfc0    t1, BCM33xx_CONFIG
        li      a0, CP0_CFG_ICSHEN|CP0_CFG_DCSHEN
        and     t1, a0
        beq     t1, a0, 1f
        nop

        # Read the CONFIG1 register into a0
        mfc0    a0, C0_CONFIG, 1
        // NOTE: The MIPS32 has register interlocks for "mfc0", so these NOPs
        // should be unnecessary.
        nop
        nop
        nop

        # Jump to C-code to get cache sizes (uncached)
        lar     k0, hal_c_cache_init
        CYGARC_ADDRESS_REG_UNCACHED(k0)
        jalr    k0
        nop

        # Jump to assembly code to initialize caches (uncached)
        lar     k0, initCache
        CYGARC_ADDRESS_REG_UNCACHED(k0)
        jalr    k0
         nop

        # Jump to code to do chip-specific initialization (uncached, just in case)
        lar     k0, chip_specific_cache_init
        CYGARC_ADDRESS_REG_UNCACHED(k0)
        jalr    k0
         nop
        b       2f
         nop
1:
        # Set cache to correct mode if it was already enabled.        
        # NOTE: only K0 coherency is implemented, so this is technically illegal.
        mfc0    t1, config0
        and     t1, ~(CP0_K0Coherency | CP0_K23Coherency | CP0_KUCoherency)

#ifdef CYGSEM_HAL_DCACHE_STARTUP_MODE_COPYBACK
        or      t1,  (CP0_K0Writeback | CP0_K23Writeback | CP0_KUWriteback)
#else
        or      t1,  (CP0_K0WriteThrough | CP0_K23WriteThrough | CP0_KUWriteThrough)
#endif

        mtc0    t1, config0      # Set kseg0 cache algorithm
        nop
2:

.endm

#define CYGPKG_HAL_MIPS_CACHE_DEFINED

#endif  


#------------------------------------------------------------------------------
#endif // ifndef CYGONCE_HAL_PLATFORM_INC
# end of platform.inc
