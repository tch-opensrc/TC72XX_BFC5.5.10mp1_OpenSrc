#ifndef CYGONCE_HAL_PLATFORM_INC
#define CYGONCE_HAL_PLATFORM_INC
##=============================================================================
##
##	platform.inc
##
##	AKI3068NET "board" assembler header file
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
## Author(s): 	 jlarmour
## Contributors: Yoshinori Sato
## Date:	 1999-09-09
## Purpose:	 AKI3068NET "board" definitions.
## Description:	 This file contains various definitions and macros that are
##               required for writing assembly code for the AKI3068NET board
##               Currently there are none
## Usage:
##		 #include <cyg/hal/platform.inc>
##		 ...
##		
##
######DESCRIPTIONEND####
##
##=============================================================================

#include <pkgconf/hal.h>

#include <cyg/hal/mod_regs_bsc.h>
#include <cyg/hal/mod_regs_pio.h>

##-----------------------------------------------------------------------------

#define CYGPKG_HAL_H8300_MEMC_DEFINED
	.macro	hal_memc_init
#if defined(CYG_HAL_STARTUP_ROM)
	mov.l	#init_regs,er0
	mov.w	#8,e1
1:
	mov.l	@er0+,er2
	mov.w	@er0+,r1
	mov.b	r1l,@er2
	dec.w	#1,e1
	bne	1b
	bra	2f

init_regs:
	.long	CYGARC_RTCOR
	.word	5		; 1.5[ms] reflesh cycle
	.long	CYGARC_RTMCSR
	.word	0x30		; clk x 2048
	.long	CYGARC_DRCRB
	.word	0x90
	.long	CYGARC_DRCRA
	.word	0x22
	.long	CYGARC_P1DDR
	.word	0xff
	.long	CYGARC_P2DDR
	.word	0xff
	.long	CYGARC_P5DDR
	.word	0x01
	.long	CYGARC_P8DDR
	.word	0x0c
init_regs_end:

2:
#endif
	.endm

#define CYGPKG_HAL_H8300_DIAG_DEFINED

	.macro	hal_diag_init
	mov.b	#0x83,r0l
	mov.b	r0l,@CYGARC_PADDR
	mov.b	#0,r0l
	mov.b	r0l,@CYGARC_PADR
	.endm

	.macro	hal_diag_excpt_start
	.endm

	.macro	hal_diag_intr_start
	.endm

	.macro	hal_diag_restore
	.endm

	.macro	hal_diag_data
	.endm

#------------------------------------------------------------------------------
#endif // ifndef CYGONCE_HAL_PLATFORM_INC
# end of platform.inc
