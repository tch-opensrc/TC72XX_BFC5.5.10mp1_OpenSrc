//===========================================================================
//
//      strcoll.cxx
//
//      Real function definition for ANSI string strcoll() function - not
//      that there is an inline alternative
//
//===========================================================================
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
//===========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     jlarmour
// Contributors: 
// Date:          2000-04-14
// Purpose:     
// Description: 
// Usage:
//
//####DESCRIPTIONEND####
//
//===========================================================================

// CONFIGURATION

#include <pkgconf/libc_string.h>   // Configuration header

// INCLUDES

// We don't want the inline versions of string functions defined here

#ifdef CYGIMP_LIBC_STRING_INLINES
#undef CYGIMP_LIBC_STRING_INLINES
#endif

#include <cyg/infra/cyg_type.h>    // Common type definitions
#include <cyg/infra/cyg_trac.h>    // Tracing support
#include <cyg/infra/cyg_ass.h>     // Assertion support
#include <string.h>                // Header for this file
#include <cyg/libc/string/stringsupp.hxx> // Useful string function support and
                                          // prototypes
// EXPORTED SYMBOLS

externC int
strcoll( const char *s1, const char *s2 ) CYGBLD_ATTRIB_WEAK_ALIAS(__strcoll);


int
__strcoll( const char *s1, const char *s2 )
{
    int retval;

    CYG_REPORT_FUNCNAMETYPE( "__strcoll", "returning %d" );
    CYG_REPORT_FUNCARG2( "s1=%08x, s2=%d", s1, s2 );

    CYG_CHECK_DATA_PTR( s1, "s1 is not a valid pointer!" );
    CYG_CHECK_DATA_PTR( s2, "s2 is not a valid pointer!" );

    retval = strcmp(s1, s2);

    return retval;
} // __strcoll()


// EOF strcoll.cxx
