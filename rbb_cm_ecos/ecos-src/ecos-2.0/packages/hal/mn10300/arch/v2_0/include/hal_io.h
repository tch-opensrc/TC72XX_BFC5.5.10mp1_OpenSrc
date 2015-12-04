#ifndef CYGONCE_HAL_HAL_IO_H
#define CYGONCE_HAL_HAL_IO_H

//=============================================================================
//
//      hal_io.h
//
//      HAL device IO register support.
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
// Author(s):   nickg
// Contributors:        nickg, dmoseley
// Date:        1998-02-17
// Purpose:     Define IO register support
// Description: The macros defined here provide the HAL APIs for handling
//              device IO control registers.
//              
// Usage:
//              #include <cyg/hal/hal_io.h>
//              ...
//              
//
//####DESCRIPTIONEND####
//
//=============================================================================

#ifndef __ASSEMBLER__
#include <cyg/infra/cyg_type.h>
#include <cyg/hal/plf_io.h>
#endif

//-----------------------------------------------------------------------------
// IO Register address.
// This type is for recording the address of an IO register.

#ifndef __ASSEMBLER__
typedef volatile CYG_ADDRWORD HAL_IO_REGISTER;
#endif

//-----------------------------------------------------------------------------
// BYTE Register access.
// Individual and vectorized access to 8 bit registers.

#define HAL_READ_UINT8( _register_, _value_ ) \
        ((_value_) = *((volatile CYG_BYTE *)(_register_)))

#define HAL_WRITE_UINT8( _register_, _value_ ) \
        (*((volatile CYG_BYTE *)(_register_)) = (_value_))

#define HAL_READ_UINT8_VECTOR( _register_, _buf_, _count_, _step_ )     \
{                                                                       \
    cyg_count32 _i_,_j_;                                                \
    for( _i_ = 0, _j_ = 0; _i_ < (_count_); _i_++, _j_ += (_step_))     \
        (_buf_)[_i_] = ((volatile CYG_BYTE *)(_register_))[_j_];        \
}

#define HAL_WRITE_UINT8_VECTOR( _register_, _buf_, _count_, _step_ )    \
{                                                                       \
    cyg_count32 _i_,_j_;                                                \
    for( _i_ = 0, _j_ = 0; _i_ < (_count_); _i_++, _j_ += (_step_))     \
        ((volatile CYG_BYTE *)(_register_))[_j_] = (_buf_)[_i_];        \
}


//-----------------------------------------------------------------------------
// 16 bit access.
// Individual and vectorized access to 16 bit registers.
    
#define HAL_READ_UINT16( _register_, _value_ ) \
        ((_value_) = *((volatile CYG_WORD16 *)(_register_)))

#define HAL_WRITE_UINT16( _register_, _value_ ) \
        (*((volatile CYG_WORD16 *)(_register_)) = (_value_))

#define HAL_READ_UINT16_VECTOR( _register_, _buf_, _count_, _step_ )    \
{                                                                       \
    cyg_count32 _i_,_j_;                                                \
    for( _i_ = 0, _j_ = 0; _i_ < (_count_); _i_++, _j_ += (_step_))     \
        (_buf_)[_i_] = ((volatile CYG_WORD16 *)(_register_))[_j_];      \
}

#define HAL_WRITE_UINT16_VECTOR( _register_, _buf_, _count_, _step_ )   \
{                                                                       \
    cyg_count32 _i_,_j_;                                                \
    for( _i_ = 0, _j_ = 0; _i_ < (_count_); _i_++, _j_ += (_step_))     \
        ((volatile CYG_WORD16 *)(_register_))[_j_] = (_buf_)[_i_];      \
}

//-----------------------------------------------------------------------------
// 32 bit access.
// Individual and vectorized access to 32 bit registers.
    
#define HAL_READ_UINT32( _register_, _value_ ) \
        ((_value_) = *((volatile CYG_WORD32 *)(_register_)))

#define HAL_WRITE_UINT32( _register_, _value_ ) \
        (*((volatile CYG_WORD32 *)(_register_)) = (_value_))

#define HAL_READ_UINT32_VECTOR( _register_, _buf_, _count_, _step_ )    \
{                                                                       \
    cyg_count32 _i_,_j_;                                                \
    for( _i_ = 0, _j_ = 0; _i_ < (_count_); _i_++, _j_ += (_step_))     \
        (_buf_)[_i_] = ((volatile CYG_WORD32 *)(_register_))[_j_];      \
}

#define HAL_WRITE_UINT32_VECTOR( _register_, _buf_, _count_, _step_ )   \
{                                                                       \
    cyg_count32 _i_,_j_;                                                \
    for( _i_ = 0, _j_ = 0; _i_ < (_count_); _i_++, _j_ += (_step_))     \
        ((volatile CYG_WORD32 *)(_register_))[_j_] = (_buf_)[_i_];      \
}

//-----------------------------------------------------------------------------
#endif // ifndef CYGONCE_HAL_HAL_IO_H
// End of hal_io.h
