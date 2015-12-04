//==========================================================================
//
//      flash_program_buf.c
//
//      Flash programming
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
// Author(s):    gthomas
// Contributors: gthomas
// Date:         2001-07-17
// Purpose:      
// Description:  
//              
//####DESCRIPTIONEND####
//
//==========================================================================

#include "flash.h"

#include <pkgconf/hal.h>
#include <cyg/hal/hal_arch.h>
#include <cyg/hal/hal_cache.h>

//
// CAUTION!  This code must be copied to RAM before execution.  Therefore,
// it must not contain any code which might be position dependent!
//

int
flash_program_buf(volatile unsigned short *addr, unsigned short *data, int len)
{
    volatile unsigned short *PAGE, *ROM;
    int timeout = 50000;
    int cache_on;
    int i, offset;
    unsigned short hold[FLASH_PAGE_SIZE/2];

    HAL_DCACHE_IS_ENABLED(cache_on);
    if (cache_on) {
        HAL_DCACHE_SYNC();
        HAL_DCACHE_DISABLE();
    }

    if (len != FLASH_PAGE_SIZE) {
        return FLASH_LENGTH_ERROR;
    }

    ROM = (volatile unsigned short *)((unsigned long)addr & 0xFFFF0000);
    PAGE = (volatile unsigned short *)((unsigned long)addr & FLASH_PAGE_MASK);

    // Copy current contents (allows for partial updates)
    for (i = 0;  i < FLASH_PAGE_SIZE/2;  i++) {
        hold[i] = PAGE[i];
    }

    // Merge data into holding buffer
    offset = (unsigned long)addr & FLASH_PAGE_OFFSET;
    for (i = 0;  i < (len/2);  i++) {
        hold[offset+i] = data[i];
    }

    // Now write the data

    // Send lead-in sequence
    ROM[FLASH_Key_Addr0] = FLASH_Key0;
    ROM[FLASH_Key_Addr1] = FLASH_Key1;

    // Send 'write' command
    ROM[FLASH_Key_Addr0] = FLASH_Program;

    // Send one page/sector of data
    for (i = 0;  i < FLASH_PAGE_SIZE/2;  i++) {
        PAGE[i] = hold[i];
    }

    // Wait for data to be programmed
    while (timeout-- > 0) {
        if (PAGE[(FLASH_PAGE_SIZE/2)-1] == hold[(FLASH_PAGE_SIZE/2)-1]) {
            break;
        }
    }

    if (timeout <= 0) {
        return FLASH_PROGRAM_ERROR;
    }

    if (cache_on) {
        HAL_DCACHE_ENABLE();
    }

    return FLASH_PROGRAM_OK;
}
