#ifndef CYGONCE_VAR_CACHE_H
#define CYGONCE_VAR_CACHE_H
//=============================================================================
//
//      var_cache.h
//
//      Variant HAL cache control API
//
//=============================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
// Copyright (C) 2002, 2003 Gary Thomas
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
// Contributors:nickg, jskov, gthomas
// Date:        2000-04-02
// Purpose:     Variant cache control API
// Description: The macros defined here provide the HAL APIs for handling
//              cache control operations on the PPC60x variant CPUs.
// Usage:       Is included via the architecture cache header:
//              #include <cyg/hal/hal_cache.h>
//              ...
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/hal.h>
#include <cyg/infra/cyg_type.h>

#include <cyg/hal/ppc_regs.h>
#include <cyg/hal/plf_cache.h>


//-----------------------------------------------------------------------------
// Cache dimensions

// Data cache
#ifndef HAL_DCACHE_SIZE
#define HAL_DCACHE_SIZE                 16384   // Size of data cache in bytes
#define HAL_DCACHE_LINE_SIZE            32      // Size of a data cache line
#define HAL_DCACHE_WAYS                 4       // Associativity of the cache
#endif

// Instruction cache
#ifndef HAL_ICACHE_SIZE
#define HAL_ICACHE_SIZE                 16384   // Size of cache in bytes
#define HAL_ICACHE_LINE_SIZE            32      // Size of a cache line
#define HAL_ICACHE_WAYS                 4       // Associativity of the cache
#endif

#define HAL_DCACHE_SETS (HAL_DCACHE_SIZE/(HAL_DCACHE_LINE_SIZE*HAL_DCACHE_WAYS))
#define HAL_ICACHE_SETS (HAL_ICACHE_SIZE/(HAL_ICACHE_LINE_SIZE*HAL_ICACHE_WAYS))

//-----------------------------------------------------------------------------
// Global control of data cache

// Enable the data cache
#define HAL_DCACHE_ENABLE()                     \
    CYG_MACRO_START                             \
    cyg_int32 _scratch;                         \
    asm volatile ("isync;"                      \
                  "mfspr %0,%1;"                \
                  "or  %0,%0,%2;"               \
                  "mtspr %1,%0"                 \
                  : "=&r" (_scratch)            \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_DCE)             \
        );                                      \
    CYG_MACRO_END

// Disable the data cache
#define HAL_DCACHE_DISABLE()                    \
    CYG_MACRO_START                             \
    cyg_int32 _scratch;                         \
    asm volatile ("isync;"                      \
                  "mfspr %0,%1;"                \
                  "andc %0,%0,%2;"              \
                  "mtspr %1,%0"                 \
                  : "=&r" (_scratch)            \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_DCE)             \
        );                                      \
    CYG_MACRO_END

// Invalidate the entire cache
#define HAL_DCACHE_INVALIDATE_ALL()             \
    CYG_MACRO_START                             \
    cyg_int32 _scr1, _scr2;                     \
    asm volatile ("isync;"                      \
                  "mfspr %0,%2;"                \
                  "mr %1,%0;"                   \
                  "or %0,%0,%3;"                \
                  "mtspr %2,%0;"                \
                  "mtspr %2,%1"                 \
                  : "=&r" (_scr1),              \
                    "=&r" (_scr2)               \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_DCFI)            \
        );                                      \
    CYG_MACRO_END

// Synchronize the contents of the cache with memory.
#define HAL_DCACHE_SYNC()                                                     \
    CYG_MACRO_START                                                           \
    cyg_int32 i;                                                              \
    cyg_uint32 *__base = (cyg_uint32 *) (0);                  \
    for (i = 0; i < (HAL_DCACHE_SIZE/HAL_DCACHE_LINE_SIZE); i++, __base += HAL_DCACHE_LINE_SIZE/4){ \
        asm volatile ("lwz %%r0,0(%0)" : : "r" (__base) : "r0");      \
    }                                                                         \
    CYG_MACRO_END

// Query the state of the data cache
#define HAL_DCACHE_IS_ENABLED(_state_)          \
    CYG_MACRO_START                             \
    cyg_int32 _scratch;                         \
    asm volatile ("isync;"                      \
                  "mfspr %0,%1;"                \
                  "and %0,%0,%2;"               \
                  : "=&r" (_scratch)            \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_DCE)             \
        );                                      \
    (_state_) = _scratch != 0;                  \
    CYG_MACRO_END

// Set the data cache refill burst size
//#define HAL_DCACHE_BURST_SIZE(_size_)

// Set the data cache write mode
//#define HAL_DCACHE_WRITE_MODE( _mode_ )

//#define HAL_DCACHE_WRITETHRU_MODE       0
//#define HAL_DCACHE_WRITEBACK_MODE       1

// Load the contents of the given address range into the data cache
// and then lock the cache so that it stays there.
//#define HAL_DCACHE_LOCK(_base_, _size_)

// Undo a previous lock operation
//#define HAL_DCACHE_UNLOCK(_base_, _size_)

// Unlock entire cache
//#define HAL_DCACHE_UNLOCK_ALL()

//-----------------------------------------------------------------------------
// Data cache line control

// Allocate cache lines for the given address range without reading its
// contents from memory.
//#define HAL_DCACHE_ALLOCATE( _base_ , _size_ )

// Write dirty cache lines to memory and invalidate the cache entries
// for the given address range.
#define HAL_DCACHE_FLUSH( _base_ , _size_ )                     \
    CYG_MACRO_START                                             \
    cyg_uint32 __base = (cyg_uint32) (_base_);                  \
    cyg_int32 __size = (cyg_int32) (_size_);                    \
    while (__size > 0) {                                        \
        asm volatile ("dcbf 0,%0;sync;" : : "r" (__base));      \
        __base += HAL_DCACHE_LINE_SIZE;                         \
        __size -= HAL_DCACHE_LINE_SIZE;                         \
    }                                                           \
    CYG_MACRO_END
   
// Invalidate cache lines in the given range without writing to memory.
#define HAL_DCACHE_INVALIDATE( _base_ , _size_ )                \
    CYG_MACRO_START                                             \
    cyg_uint32 __base = (cyg_uint32) (_base_);                  \
    cyg_int32 __size = (cyg_int32) (_size_);                    \
    while (__size > 0) {                                        \
        asm volatile ("dcbi 0,%0;sync;" : : "r" (__base));      \
        __base += HAL_DCACHE_LINE_SIZE;                         \
        __size -= HAL_DCACHE_LINE_SIZE;                         \
    }                                                           \
    CYG_MACRO_END

// Write dirty cache lines to memory for the given address range.
#define HAL_DCACHE_STORE( _base_ , _size_ )                     \
    CYG_MACRO_START                                             \
    cyg_uint32 __base = (cyg_uint32) (_base_);                  \
    cyg_int32 __size = (cyg_int32) (_size_);                    \
    while (__size > 0) {                                        \
        asm volatile ("dcbst 0,%0;sync;" : : "r" (__base));     \
        __base += HAL_DCACHE_LINE_SIZE;                         \
        __size -= HAL_DCACHE_LINE_SIZE;                         \
    }                                                           \
    CYG_MACRO_END

// Preread the given range into the cache with the intention of reading
// from it later.
#define HAL_DCACHE_READ_HINT( _base_ , _size_ )                 \
    CYG_MACRO_START                                             \
    cyg_uint32 __base = (cyg_uint32) (_base_);                  \
    cyg_int32 __size = (cyg_int32) (_size_);                    \
    while (__size > 0) {                                        \
        asm volatile ("dcbt 0,%0;" : : "r" (__base));           \
        __base += HAL_DCACHE_LINE_SIZE;                         \
        __size -= HAL_DCACHE_LINE_SIZE;                         \
    }                                                           \
    CYG_MACRO_END

// Preread the given range into the cache with the intention of writing
// to it later.
#define HAL_DCACHE_WRITE_HINT( _base_ , _size_ )                \
    CYG_MACRO_START                                             \
    cyg_uint32 __base = (cyg_uint32) (_base_);                  \
    cyg_int32 __size = (cyg_int32) (_size_);                    \
    while (__size > 0) {                                        \
        asm volatile ("dcbtst 0,%0;" : : "r" (__base));         \
        __base += HAL_DCACHE_LINE_SIZE;                         \
        __size -= HAL_DCACHE_LINE_SIZE;                         \
    }                                                           \
    CYG_MACRO_END

// Allocate and zero the cache lines associated with the given range.
#define HAL_DCACHE_ZERO( _base_ , _size_ )                      \
    CYG_MACRO_START                                             \
    cyg_uint32 __base = (cyg_uint32) (_base_);                  \
    cyg_int32 __size = (cyg_int32) (_size_);                    \
    while (__size > 0) {                                        \
        asm volatile ("dcbz 0,%0;" : : "r" (__base));           \
        __base += HAL_DCACHE_LINE_SIZE;                         \
        __size -= HAL_DCACHE_LINE_SIZE;                         \
    }                                                           \
    CYG_MACRO_END

//-----------------------------------------------------------------------------
// Global control of Instruction cache

// Enable the instruction cache
#define HAL_ICACHE_ENABLE()                     \
    CYG_MACRO_START                             \
    cyg_int32 _scratch;                         \
    asm volatile ("isync;"                      \
                  "mfspr %0,%1;"                \
                  "or  %0,%0,%2;"               \
                  "mtspr %1,%0"                 \
                  : "=&r" (_scratch)            \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_ICE)             \
        );                                      \
    CYG_MACRO_END

// Disable the instruction cache
#define HAL_ICACHE_DISABLE()                    \
    CYG_MACRO_START                             \
    cyg_int32 _scratch;                         \
    asm volatile ("isync;"                      \
                  "mfspr %0,%1;"                \
                  "andc %0,%0,%2;"              \
                  "mtspr %1,%0"                 \
                  : "=&r" (_scratch)            \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_ICE)             \
        );                                      \
    CYG_MACRO_END

// Invalidate the entire cache
#define HAL_ICACHE_INVALIDATE_ALL()             \
    CYG_MACRO_START                             \
    cyg_int32 _scr1, _scr2;                     \
    asm volatile ("isync;"                      \
                  "mfspr %0,%2;"                \
                  "mr %1,%0;"                   \
                  "or %0,%0,%3;"                \
                  "mtspr %2,%0;"                \
                  "mtspr %2,%1"                 \
                  : "=&r" (_scr1),              \
                    "=&r" (_scr2)               \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_ICFI)            \
        );                                      \
    CYG_MACRO_END

// Synchronize the contents of the cache with memory.
#define HAL_ICACHE_SYNC()                       \
  HAL_ICACHE_INVALIDATE_ALL()

// Query the state of the instruction cache
#define HAL_ICACHE_IS_ENABLED(_state_)          \
    CYG_MACRO_START                             \
    cyg_int32 _scratch;                         \
    asm volatile ("isync;"                      \
                  "mfspr %0,%1;"                \
                  "and %0,%0,%2;"               \
                  : "=&r" (_scratch)            \
                  : "I" (CYGARC_REG_HID0),      \
                    "r" (_HID0_ICE)             \
        );                                      \
    (_state_) = _scratch != 0;                  \
    CYG_MACRO_END

// Set the instruction cache refill burst size
//#define HAL_ICACHE_BURST_SIZE(_size_)

// Load the contents of the given address range into the instruction cache
// and then lock the cache so that it stays there.
//#define HAL_ICACHE_LOCK(_base_, _size_)

// Undo a previous lock operation
//#define HAL_ICACHE_UNLOCK(_base_, _size_)

// Unlock entire cache
//#define HAL_ICACHE_UNLOCK_ALL()

//-----------------------------------------------------------------------------
// Instruction cache line control

// Invalidate cache lines in the given range without writing to memory.
//#define HAL_ICACHE_INVALIDATE( _base_ , _size_ )

//-----------------------------------------------------------------------------
#endif // ifndef CYGONCE_VAR_CACHE_H
// End of var_cache.h
