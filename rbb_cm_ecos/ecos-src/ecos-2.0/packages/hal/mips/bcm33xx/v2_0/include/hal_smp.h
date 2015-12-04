#ifndef CYGONCE_HAL_SMP_H
#define CYGONCE_HAL_SMP_H

//=============================================================================
//
//      hal_smp.h
//
//      SMP support
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
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   msieweke (based on x86 version by nickg)
// Contributors:  nickg
// Date:        21-Jun-2005
// Purpose:     Define SMP support abstractions
// Usage:       #include <cyg/hal/hal_smp.h>

//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/hal.h>

#ifdef CYGPKG_HAL_SMP_SUPPORT

#include <cyg/infra/cyg_type.h>

#include <cyg/hal/hal_arch.h>

//=============================================================================

//-----------------------------------------------------------------------------
// SMP configuration determined from platform during initialization

__externC CYG_WORD32 cyg_hal_smp_cpu_count;

__externC CYG_BYTE cyg_hal_smp_cpu_flags[CYGPKG_HAL_SMP_CPU_MAX];

//-----------------------------------------------------------------------------
// CPU numbering macros

#define HAL_SMP_CPU_TYPE        cyg_uint32

#define HAL_SMP_CPU_MAX         CYGPKG_HAL_SMP_CPU_MAX

#define HAL_SMP_CPU_COUNT()     cyg_hal_smp_cpu_count

#define HAL_SMP_CPU_THIS()          \
    ({                              \
    register HAL_SMP_CPU_TYPE __id; \
    asm volatile(                   \
        "mfc0 %0, $22, 3;"          \
        "srl  %0, 31"               \
        : "=r" (__id)               \
    );                              \
    (__id);                         \
    })

#define HAL_SMP_CPU_NONE        (CYGPKG_HAL_SMP_CPU_MAX+1)

//-----------------------------------------------------------------------------
// CPU startup

__externC void cyg_hal_cpu_release(HAL_SMP_CPU_TYPE cpu);

#define HAL_SMP_CPU_START( __cpu ) cyg_hal_cpu_release( __cpu );

#define HAL_SMP_CPU_RESCHEDULE_INTERRUPT( __cpu, __wait ) \
    cyg_hal_cpu_message( __cpu, HAL_SMP_MESSAGE_RESCHEDULE, 0, __wait);

#define HAL_SMP_CPU_TIMESLICE_INTERRUPT( __cpu, __wait ) \
    cyg_hal_cpu_message( __cpu, HAL_SMP_MESSAGE_TIMESLICE, 0, __wait);

//-----------------------------------------------------------------------------
// CPU message exchange

__externC void cyg_hal_cpu_message( HAL_SMP_CPU_TYPE cpu,
                                   CYG_WORD32 msg,
                                   CYG_WORD32 arg,
                                   CYG_WORD32 wait);

#define HAL_SMP_MESSAGE_TYPE            0xF0000000
#define HAL_SMP_MESSAGE_ARG             (~HAL_SMP_MESSAGE_TYPE)

#define HAL_SMP_MESSAGE_RESCHEDULE      0x10000000
#define HAL_SMP_MESSAGE_MASK            0x20000000
#define HAL_SMP_MESSAGE_UNMASK          0x30000000
#define HAL_SMP_MESSAGE_REVECTOR        0x40000000
#define HAL_SMP_MESSAGE_TIMESLICE       0x50000000

//-----------------------------------------------------------------------------
// Test-and-set support
// These macros provide test-and-set support for the least significant bit
// in a word.

#define HAL_TAS_TYPE    volatile CYG_WORD32

// The MIPS doesn't have a test-and-set instruction.  The "ll" and "sc" opcodes
// allow us to read and try to write, and then find out if it succeeded.  If it
// didn't succeed on the write, we loop to try again.
#define HAL_TAS_SET( _tas_, _oldb_ )    \
    CYG_MACRO_START                     \
    register CYG_WORD32 __old;          \
    asm volatile (                      \
        ".set push; .set noreorder;"    \
        "2:"                            \
        " ll    %0, %1;"                \
        " bnez  %0, 1f;"                \
        "  li    %0, 1;"                \
        " sc    %0, %1;"                \
        " beqz  %0, 2b;"                \
        "  li    %0, 0;"                \
        "1:"                            \
        " .set pop"                     \
        : "=r" (__old), "=m" (_tas_)    \
    );                                  \
    _oldb_ = __old != 0;                \
    CYG_MACRO_END

// The clear operation should always succeed on the first try, but it's best
// to do things the right way and loop.
#define HAL_TAS_CLEAR( _tas_, _oldb_ )  \
    CYG_MACRO_START                     \
    register CYG_WORD32 __old;          \
    asm volatile (                      \
        ".set push; .set noreorder;"    \
        "2:"                            \
        " ll    %0, %1;"                \
        " beqz  %0, 1f;"                \
        "  li    %0, 0;"                \
        " sc    %0, %1;"                \
        " beqz  %0, 2b;"                \
        "  li    %0, 1;"                \
        "1:"                            \
        " .set pop"                     \
        : "=r" (__old), "=m" (_tas_)    \
    );                                  \
    _oldb_ = __old != 0;                \
    CYG_MACRO_END

//-----------------------------------------------------------------------------
// Spinlock support.
// Built on top of test-and-set code.

#define HAL_SPINLOCK_TYPE       volatile CYG_WORD32

#define HAL_SPINLOCK_INIT_CLEAR 0

#define HAL_SPINLOCK_INIT_SET   1

#if 0

#define HAL_SPINLOCK_SPIN( _lock_ )         \
    CYG_MACRO_START                         \
    cyg_bool _val_;                         \
    do {                                    \
        HAL_TAS_SET( _lock_, _val_ );       \
    } while( _val_ );                       \
    CYG_MACRO_END

#endif

#define HAL_SPINLOCK_SPIN( _lock_ )     \
    CYG_MACRO_START                     \
    asm volatile (                      \
        ".set push; .set noreorder;"    \
        "1:"                            \
        " ll    $8, %0;"                \
        " bnez  $8, 1b;"                \
        "  li    $8, 1;"                \
        " sc    $8, %0;"                \
        " beqz  $8, 1b;"                \
        "  nop;"                        \
        " .set pop"                     \
        : "=m" (_lock_) : : "$8"        \
    );                                  \
    CYG_MACRO_END

#define HAL_SPINLOCK_CLEAR( _lock_ )        \
    CYG_MACRO_START                         \
    cyg_bool _val_;                         \
    HAL_TAS_CLEAR( _lock_ , _val_ );        \
    CYG_MACRO_END

#define HAL_SPINLOCK_TRY( _lock_, _val_ )   \
    CYG_MACRO_START                         \
    HAL_TAS_SET( _lock_, _val_ );           \
    (_val_) = (((_val_) & 1) == 0);         \
    CYG_MACRO_END

#define HAL_SPINLOCK_TEST( _lock_, _val_ )  \
    CYG_MACRO_START                         \
    (_val_) = (((_lock_) & 1) != 0);        \
    CYG_MACRO_END

//-----------------------------------------------------------------------------
// Diagnostic output serialization

__externC HAL_SPINLOCK_TYPE cyg_hal_smp_diag_lock;

#define CYG_HAL_DIAG_LOCK_DATA_DEFN \
    HAL_SPINLOCK_TYPE cyg_hal_smp_diag_lock = HAL_SPINLOCK_INIT_CLEAR

#define CYG_HAL_DIAG_LOCK() HAL_SPINLOCK_SPIN( cyg_hal_smp_diag_lock )

#define CYG_HAL_DIAG_UNLOCK() HAL_SPINLOCK_CLEAR( cyg_hal_smp_diag_lock )

//-----------------------------------------------------------------------------

#endif // CYGPKG_HAL_SMP_SUPPORT

//-----------------------------------------------------------------------------
#endif // CYGONCE_HAL_SMP_H
// End of hal_smp.h
