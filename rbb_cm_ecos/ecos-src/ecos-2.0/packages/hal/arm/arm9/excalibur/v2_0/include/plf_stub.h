#ifndef CYGONCE_HAL_PLF_STUB_H
#define CYGONCE_HAL_PLF_STUB_H

//=============================================================================
//
//      plf_stub.h
//
//      Platform header for GDB stub support.
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
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   jskov
// Contributors:jskov, gthomas
// Date:        2001-08-06
// Purpose:     Platform HAL stub support for Altera/Excalibur boards.
// Usage:       #include <cyg/hal/plf_stub.h>
//              
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/hal.h>

#ifdef CYGDBG_HAL_DEBUG_GDB_INCLUDE_STUBS

#include <cyg/infra/cyg_type.h>         // CYG_UNUSED_PARAM

#include <cyg/hal/arm_stub.h>           // architecture stub support

//----------------------------------------------------------------------------
// Define some platform specific communication details. This is mostly
// handled by hal_if now, but we need to make sure the comms tables are
// properly initialized.

externC void cyg_hal_plf_comms_init(void);

#define HAL_STUB_PLATFORM_INIT_SERIAL()       cyg_hal_plf_comms_init()

#define HAL_STUB_PLATFORM_SET_BAUD_RATE(baud) CYG_UNUSED_PARAM(int, (baud))
#define HAL_STUB_PLATFORM_INTERRUPTIBLE       0
#define HAL_STUB_PLATFORM_INIT_BREAK_IRQ()    CYG_EMPTY_STATEMENT

//----------------------------------------------------------------------------
// Stub initializer.
#define HAL_STUB_PLATFORM_INIT()              CYG_EMPTY_STATEMENT

#endif // ifdef CYGDBG_HAL_DEBUG_GDB_INCLUDE_STUBS
//-----------------------------------------------------------------------------
#endif // CYGONCE_HAL_PLF_STUB_H
// End of plf_stub.h
