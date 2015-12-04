#ifndef CYGONCE_HAL_PLF_INTR_H
#define CYGONCE_HAL_PLF_INTR_H
//==========================================================================
//
//      plf_intr.h
//
//      i386/PC Interrupt and clock support
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
// Author(s):    proven
// Contributors: proven, jskov, pjo
// Date:         1999-10-15
// Purpose:      Define Interrupt support
// Description:  The macros defined here provide the HAL APIs for handling
//               interrupts and the clock.
//               This file contains info about interrupts and
//               peripherals that are common on all PCs; for example,
//               the clock always activates irq 0 and would therefore
//               be listed here; an ethernet card is configured for
//               the individual system and would be in plf_intr.h
//               instead.
//              
// Usage:
//               #include <cyg/hal/plf_intr.h>
//               ...
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/hal.h>
#include <pkgconf/hal_i386.h>

#include <cyg/infra/cyg_type.h>

#include <cyg/hal/pcmb_intr.h>

//----------------------------------------------------------------------------
// Reset.

#define HAL_PLATFORM_RESET()             hal_pc_reset()

#define HAL_PLATFORM_RESET_ENTRY	 &hal_pc_reset

//---------------------------------------------------------------------------
// Microsecond delay

__externC void hal_delay_us(int us);

#define HAL_DELAY_US(_us) hal_delay_us(_us)

//---------------------------------------------------------------------------
#endif // ifndef CYGONCE_HAL_PLF_INTR_H
// End of plf_intr.h
