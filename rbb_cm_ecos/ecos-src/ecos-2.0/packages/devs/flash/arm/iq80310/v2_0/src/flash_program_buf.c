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
// Contributors: gthomas, msalter
// Date:         2000-07-14
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
flash_program_buf(volatile unsigned char *addr, unsigned char *data, int len)
{
    volatile unsigned char *ROM;
    volatile unsigned char *BA;
    unsigned short stat;
    int timeout = 5000000;
    int i, wc, cache_on;

    HAL_DCACHE_IS_ENABLED(cache_on);
    if (cache_on) {
        HAL_DCACHE_SYNC();
        HAL_DCACHE_DISABLE();
    }

    ROM = FLASH_P2V((unsigned long)addr & 0xFF800000);
    BA  = FLASH_P2V((unsigned long)addr & 0xFFFE0000);

    // Clear any error conditions
    ROM[0] = FLASH_Clear_Status;

    wc = 32;
    while (len >= wc) {
        len -= wc;

	// The IQ803010 has a hole in flash which must be avoided.
	if (((unsigned char *)0x1000) <= addr && addr < ((unsigned char *)0x2000)) {
	    addr += wc;
	    data += wc;
	    continue;
	}

        *BA = FLASH_Write_Buffer;
        timeout = 5000000;
        while(((stat = ROM[0]) & FLASH_Status_Ready) != FLASH_Status_Ready) {
            if (--timeout == 0) {
                stat |= 0x0100;
                goto bad;
            }
            *BA = FLASH_Write_Buffer;
        }
        *BA = wc-1;  // Count is 0..N-1
	if (FLASH_P2V(addr) != addr) {
	    volatile unsigned char *tmp;

	    tmp = FLASH_P2V(addr);
	    for (i = 0;  i < wc;  i++)
		*tmp++ = *data++;
	    addr += wc;
	} else {
	    for (i = 0;  i < wc;  i++)
		*addr++ = *data++;
	}
        *BA = FLASH_Confirm;
	stat = *BA;
    }
    
    ROM[0] = FLASH_Read_Status;
    timeout = 5000000;
    while(((stat = ROM[0]) & FLASH_Status_Ready) != FLASH_Status_Ready) {
        if (--timeout == 0) {
            stat |= 0x0200;
            goto bad;
        }
    }

    while (len > 0) {
        ROM[0] = FLASH_Program;
	
        *FLASH_P2V(addr) = *data++;
	addr++;
        timeout = 5000000;
        while(((stat = ROM[0]) & FLASH_Status_Ready) != FLASH_Status_Ready) {
            if (--timeout == 0) {
                stat |= 0x0300;
                goto bad;
            }
        }
        --len;
    }

    // Restore ROM to "normal" mode
 bad:
    ROM[0] = FLASH_Reset;            

    if (cache_on) {
        HAL_DCACHE_ENABLE();
    }

    return stat;
}


