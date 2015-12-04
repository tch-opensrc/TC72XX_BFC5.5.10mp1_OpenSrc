//==========================================================================
//
//      powerpc_vads_flash.c
//
//      Flash programming for SHARP device on POWERPC MPC8260 Voyager
//      ADS board
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
// Copyright (C) 2002 Gary Thomas
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
// Author(s):    pfine
// Original:     jskov
// Contributors: 
// Date:         2002-01-11
// Purpose:      
// Description:  
//              
//####DESCRIPTIONEND####
//
//==========================================================================

//--------------------------------------------------------------------------
// Device properties

#define CYGNUM_FLASH_INTERLEAVE	(4)
#define CYGNUM_FLASH_SERIES	(1)
#define CYGNUM_FLASH_WIDTH      (8)
#define nPF_HACK
#ifdef PF_HACK
#define CYGNUM_FLASH_BASE 	(0xfe000000u)
#else
#define CYGNUM_FLASH_BASE 	(0xff800000u)
#endif

//--------------------------------------------------------------------------
// Platform specific extras

//--------------------------------------------------------------------------
// Now include the driver code.
#include "cyg/io/flash_28fxxx.inl"

// ------------------------------------------------------------------------
// EOF powerpc_vads_flash.c
