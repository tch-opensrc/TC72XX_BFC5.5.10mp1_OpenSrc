#ifndef CYGONCE_PLF_IO_H
#define CYGONCE_PLF_IO_H

//=============================================================================
//
//      plf_io.h
//
//      Platform specific IO support
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
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    msieweke
// Date:         2003-06-13
// Purpose:      Bcm33xx platform IO support
// Description: 
// Usage:        #include <cyg/hal/plf_io.h>
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/hal.h>
#include <cyg/hal/hal_misc.h>
#include <cyg/hal/hal_arch.h>
#include <cyg/hal/plf_intr.h>

#ifdef __ASSEMBLER__
#define HAL_REG(x)              x
#define HAL_REG8(x)             x
#define HAL_REG16(x)            x
#else
#define HAL_REG(x)              (volatile CYG_WORD *)(x)
#define HAL_REG8(x)             (volatile CYG_BYTE *)(x)
#define HAL_REG16(x)            (volatile CYG_WORD16 *)(x)
#endif

//-----------------------------------------------------------------------------


#ifdef __ASSEMBLER__

#  define DEBUG_ASCII_DISPLAY(register, character)             \
	li	k0, CYGARC_UNCACHED_ADDRESS(register);                 \
	li	k1, character;                                         \
	sw	k1, 0(k0);                                             \
    nop;                                                       \
    nop;                                                       \
    nop

#  define DEBUG_LED_IMM(val)                                   \
    li k0, HAL_DISPLAY_LEDBAR;                                 \
    li k1, val;                                                \
    sw k1, 0(k0)

#  define DEBUG_LED_REG(reg)                                   \
    li k0, HAL_DISPLAY_LEDBAR;                                 \
    sw reg, 0(k0)

#  define DEBUG_HEX_DISPLAY_IMM(val)                           \
    li k0, HAL_DISPLAY_ASCIIWORD;                              \
    li k1, val;                                                \
    sw k1, 0(k0)

#  define DEBUG_HEX_DISPLAY_REG(reg)                           \
    li k0, HAL_DISPLAY_ASCIIWORD;                              \
    sw reg, 0(k0)

#  define DEBUG_DELAY()                                        \
     li	k0, 0x20000;                                           \
0:	 sub k0, k0, 1;                                            \
     bnez k0, 0b;                                              \
     nop

#else

#  define DEBUG_ASCII_DISPLAY(register, character)             \
    *(register) = character

#  define DEBUG_LED_IMM(val)                                   \
    *HAL_DISPLAY_LEDBAR = val

#  define DEBUG_HEX_DISPLAY_IMM(val)                           \
    *HAL_DISPLAY_ASCIIWORD = val

#  define DEBUG_DELAY()                                        \
   {                                                           \
     volatile int i = 0x20000;                                 \
     while (--i) ;                                             \
   }

#  define DEBUG_DISPLAY(str)                                   \
   {                                                           \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS0, str[0]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS1, str[1]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS2, str[2]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS3, str[3]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS4, str[4]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS5, str[5]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS6, str[6]);       \
     DEBUG_ASCII_DISPLAY(HAL_DISPLAY_ASCIIPOS7, str[7]);       \
   }




#endif

//-----------------------------------------------------------------------------
// end of plf_io.h
#endif // CYGONCE_PLF_IO_H
