#ifndef CYGONCE_DEVS_FLASH_NANO_STRATAFLASH_INL
#define CYGONCE_DEVS_FLASH_NANO_STRATAFLASH_INL
//==========================================================================
//
//      nano_strataflash.inl
//
//      Flash programming - device constants, etc.
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
// Author(s):    gthomas, hmt
// Contributors: gthomas
// Date:         2001-02-15
// Purpose:      
// Description:  
//              
//####DESCRIPTIONEND####
//
//==========================================================================


// This is a single 16-bit device, on the nanoEngine
// a StrataFlash 28F320B3A. The 320 means 32Mbit, so 4Mbyte.
// The B near the end means Boot Block oriented.

#define CYGNUM_FLASH_DEVICES 	(1)
#define CYGNUM_FLASH_BASE 	(0x50000000u)
#define CYGNUM_FLASH_BASE_MASK  (0xFE000000u) // 32Mb devices
#define CYGNUM_FLASH_WIDTH 	(16)
#define CYGNUM_FLASH_BLANK      (1)

#define CYGOPT_FLASH_IS_BOOTBLOCK 1


#endif  // CYGONCE_DEVS_FLASH_NANO_STRATAFLASH_INL
// ------------------------------------------------------------------------
// EOF nano_strataflash.inl
