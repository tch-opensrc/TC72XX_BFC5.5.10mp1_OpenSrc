//==========================================================================
//
//      bootblock_flash.c
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
// Contributors: gthomas, jskov
// Date:         2000-07-26
// Purpose:      
// Description:  
//              
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/hal.h>
#include <cyg/hal/hal_arch.h>
#include <cyg/hal/hal_cache.h>

#define  _FLASH_PRIVATE_
#include <cyg/io/flash.h>

#include "flash.h"

#define _si(p) ((p[0]<<8)|p[1])

#define DEBUG 0

int
flash_hwr_init(void)
{
    unsigned short data[4];
    extern char flash_query, flash_query_end;
    typedef int code_fun(unsigned char *);
    code_fun *_flash_query;
    int code_len, stat, num_regions, region_size;

    // Copy 'program' code to RAM for execution
    code_len = (unsigned long)&flash_query_end - (unsigned long)&flash_query;
    _flash_query = (code_fun *)flash_info.work_space;
    memcpy(_flash_query, &flash_query, code_len);
    HAL_DCACHE_SYNC();  // Should guarantee this code will run
    HAL_ICACHE_INVALIDATE_ALL(); // is also required to avoid old contents
    stat = (*_flash_query)(data);
#if DEBUG
    (*flash_info.pf)("stat = %x\n", stat);
    dump_buf(data, sizeof(data));
#endif

    if (data[0] != FLASH_Intel_code) {
        (*flash_info.pf)("Not Intel = %x\n", data[0]);
        return FLASH_ERR_HWR;
    }

    // FIXME: More devices, comments, etc
    if (data[1] == (unsigned short)0x8897) {
        num_regions = 64*2;
        region_size = 0x20000;
    } else if (data[1] == (unsigned short)0x88f4) {
        num_regions = 32; // (only use one for now)*4;             // 4 devices in serial, each having 32 regions
        region_size = 0x10000;
    } else {
        (*flash_info.pf)("Unknown device type: %x\n", data[1]);
        return FLASH_ERR_HWR;
    }

    // Hard wired for now FIXME
    flash_info.block_size = region_size;
    flash_info.blocks = num_regions;
#if 0 // This should be used only for the gross XX firmware build
    flash_info.start = (void *)0;
    flash_info.end = (void *)(0x10000000+(num_regions*region_size));
#else
    flash_info.start = (void *)0x50000000;
    flash_info.end = (void *)(0x50000000+(num_regions*region_size));
#endif

    return FLASH_ERR_OK;
}

// Map a hardware status to a package error
int
flash_hwr_map_error(int err)
{
    if (err & 0x007E007E) {
        (*flash_info.pf)("Err = %x\n", err);
        if (err & 0x00400040) {
            return FLASH_ERR_ERASE_SUSPEND;
        } else 
        if (err & 0x00040004) {
            return FLASH_ERR_PROGRAM_SUSPEND;
        } else 
        if (err & 0x00020002) {
            return FLASH_ERR_PROTECT;
        } else 
        if (err & 0x00100010) {
            return FLASH_ERR_PROGRAM;
        } else 
        if (err & 0x00200020) {
            return FLASH_ERR_ERASE;
        } else 
        return FLASH_ERR_HWR;  // FIXME
    } else {
        return FLASH_ERR_OK;
    }
}

// See if a range of FLASH addresses overlaps currently running code
bool
flash_code_overlaps(void *start, void *end)
{
    extern char _stext[], _etext[];

    return ((((unsigned long)&_stext >= (unsigned long)start) &&
             ((unsigned long)&_stext < (unsigned long)end)) ||
            (((unsigned long)&_etext >= (unsigned long)start) &&
             ((unsigned long)&_etext < (unsigned long)end)));
}
