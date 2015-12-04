//==========================================================================
//
//      flash.h
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
// Author(s):    gthomas
// Contributors: gthomas
// Date:         2001-07-17
// Purpose:      
// Description:  
//              
//####DESCRIPTIONEND####
//
//==========================================================================

#ifndef _FLASH_HWR_H_
#define _FLASH_HWR_H_

// Atmel AT29LV1024

#define FLASH_Read_ID      0x9090
#define FLASH_Program      0xA0A0
#define FLASH_Reset        0xF0F0

#define FLASH_Key_Addr0    0x5555
#define FLASH_Key_Addr1    0x2AAA
#define FLASH_Key0         0xAAAA
#define FLASH_Key1         0x5555

#define FLASH_Atmel_code      0x1F
#define FLASH_ATMEL_29LV1024  0x26

#define FLASH_PROGRAM_OK    0x0000
#define FLASH_LENGTH_ERROR  0x0001
#define FLASH_PROGRAM_ERROR 0x0002

#define FLASH_PAGE_SIZE    0x100
#define FLASH_PAGE_MASK   ~(FLASH_PAGE_SIZE-1)
#define FLASH_PAGE_OFFSET  (FLASH_PAGE_SIZE-1)

#endif  // _FLASH_HWR_H_
