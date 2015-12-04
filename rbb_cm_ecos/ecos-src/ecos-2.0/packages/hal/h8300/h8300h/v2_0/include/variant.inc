#ifndef CYGONCE_HAL_VARIANT_INC
#define CYGONCE_HAL_VARIANT_INC
##=============================================================================
##
##	variant.inc
##
##	H8/300H assembler header file
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
##=============================================================================
#######DESCRIPTIONBEGIN####
##
## Author(s): 	nickg
## Contributors:	nickg
## Date:	1999-04-06
## Purpose:	H8/300H definitions.
## Description:	This file contains various definitions and macros that are
##              useful for writing assembly code for the H8/300H CPU variant.
## Usage:
##		#include <cyg/hal/variant.inc>
##		...
##		
##
######DESCRIPTIONEND####
##
##=============================================================================

#include <pkgconf/hal.h>

#include <cyg/hal/platform.inc>


#------------------------------------------------------------------------------
# Register definitions

#define SYSCR 0xfee012

#------------------------------------------------------------------------------
# CPU state save and restore macros

	.macro	hal_cpu_save_all
	stc	ccr,@-sp
	subs	#2,sp
	subs	#4,sp
	mov.l	er6,@-sp		# push all registers
	mov.l	er5,@-sp
	mov.l	er4,@-sp
	mov.l	er3,@-sp
	mov.l	er2,@-sp
	mov.l	er1,@-sp
	mov.l	er0,@-sp
	.endm

	.macro	hal_cpu_load_all
	mov.l	@sp+,er0		# pop regs
	mov.l	@sp+,er1
	mov.l	@sp+,er2
	mov.l	@sp+,er3
	mov.l	@sp+,er4
	mov.l	@sp+,er5
	mov.l	@sp+,er6
	adds	#4,sp
	adds	#2,sp
	ldc	@sp+,ccr	
	.endm


# Location of PC in saved register context (HAL_SavedRegisters)
#define SAVED_CONTEXT_PC_OFFSET          36

##-----------------------------------------------------------------------------
# Default interrupt decoding macros.

#ifndef CYGPKG_HAL_H8300_INTC_DEFINED
	
#define CYG_ISR_TABLE_SIZE	 64

	.macro  hal_intc_init
	mov.b	@SYSCR,r0l
	bclr	#3,r0l
	mov.b	r0l,@SYSCR
	.endm

#define CYGPKG_HAL_H8300_INTC_DEFINED

#endif


	
#ifndef CYGPKG_HAL_H8300_MON_DEFINED

	.macro	hal_mon_init
	.endm

#if !(defined(CYG_HAL_STARTUP_ROM)	 	|| \
	defined(CYGPKG_HAL_H8300_H8300H_SIM)	|| \
	!defined(CYGSEM_HAL_USE_ROM_MONITOR))

#define CYG_HAL_H8300_VSR_TABLE_DEFINED

#endif



#define CYGPKG_HAL_H8300_MON_DEFINED

#endif	



#------------------------------------------------------------------------------
#endif // ifndef CYGONCE_HAL_VARIANT_INC
# end of variant.inc
