#ifndef CYGONCE_HAL_PLF_INTR_H
#define CYGONCE_HAL_PLF_INTR_H

//==========================================================================
//
//      plf_intr.h
//
//      Bcm33xx Interrupt and clock support
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
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    msieweke
// Contributors: nickg, jskov,
//               gthomas, jlarmour, dmoseley
// Date:         2003-06-17
// Purpose:      Define Interrupt support
// Description:  The macros defined here provide the HAL APIs for handling
//               interrupts and the clock for the Bcm33xx board.
//
// Usage:
//              #include <cyg/hal/plf_intr.h>
//              ...
//
//
//####DESCRIPTIONEND####
//
//==========================================================================

#ifndef __ASSEMBLER__
#include <pkgconf/hal.h>
#include <cyg/infra/cyg_type.h>
#include <cyg/hal/plf_io.h>
#include <cyg/hal/hal_smp.h>
#endif

#define HAL_PLATFORM_RESET()            CYG_EMPTY_STATEMENT

#define HAL_PLATFORM_RESET_ENTRY        0xbfc00000


//--------------------------------------------------------------------------
// Interrupt vectors.

// The default for all MIPS variants is to use 6 bits in the cause register.
// We've added support for two more bits for software interrupts.

#define CYGNUM_HAL_INTERRUPT_0                0
#define CYGNUM_HAL_INTERRUPT_1                1
#define CYGNUM_HAL_INTERRUPT_2                2
#define CYGNUM_HAL_INTERRUPT_3                3
#define CYGNUM_HAL_INTERRUPT_4                4
#define CYGNUM_HAL_INTERRUPT_5                5
#define CYGNUM_HAL_INTERRUPT_6                6
#define CYGNUM_HAL_INTERRUPT_7                7

// Min/Max ISR numbers and how many there are
#define CYGNUM_HAL_ISR_MIN                     0
#define CYGNUM_HAL_ISR_MAX                     7
#define CYGNUM_HAL_ISR_COUNT                   8



// Software interrupts.
#define CYGNUM_HAL_INTERRUPT_SOFTWARE_0   CYGNUM_HAL_INTERRUPT_0
#define CYGNUM_HAL_INTERRUPT_SOFTWARE_1   CYGNUM_HAL_INTERRUPT_1


// The vector used by the internal peripheral interrupt. The peripheral
// interrupt muxes interrupts from all the peripheral blocks internal to the
// bcm33xx chip. It maps to the MIPS core "external" interrupt 0 on most bcm33xx
// chips.

#define CYGNUM_HAL_INTERRUPT_PERIPH         CYGNUM_HAL_INTERRUPT_2


// The vector used by the Real time clock. The default here is to use
// interrupt 7, which is connected to the counter/comparator registers
// in many MIPS variants.

#define CYGNUM_HAL_INTERRUPT_RTC            CYGNUM_HAL_INTERRUPT_7

#define CYGHWR_HAL_INTERRUPT_VECTORS_DEFINED


//--------------------------------------------------------------------------
// Interrupt controller access
// The default code here simply uses the fields present in the CP0 status
// and cause registers to implement this functionality.
// Beware of nops in this code. They fill delay slots and avoid CP0 hazards
// that might otherwise cause following code to run in the wrong state or
// cause a resource conflict.
//
// These macros are the same as those in hal_intr.h, except that they support
// software interrupts. The default macros found in hal_intr.h only support
// the MIPS core external and timer interrupts.

// To preserve compatibility with old versions of the BFC source, we only
// override the HAL_INTERRUPT_* macros when USE_SW_INTERRUPTS is defined.
// I'd prefer not to have any BFC references in the eCos code, but the
// alternative is to support two versions of the eCos library - one for legacy
// builds and one for current code.

#if defined( bcmos_h ) && !defined( BCM_USE_SW_INTERRUPTS )

    #define hal_map_sub_interrupt_0 hal_map_periph_interrupt

#else

#if !defined( CYGHWR_HAL_INTERRUPT_CONTROLLER_ACCESS_DEFINED )

#define HAL_INTERRUPT_MASK( _vector_ )  \
CYG_MACRO_START                         \
    asm volatile (                      \
        " mfc0  $3, $12;"               \
        " la    $2, 0x00000100;"        \
        " sllv  $2, $2, %0;"            \
        " nor   $2, $2, $0;"            \
        " and   $3, $3, $2;"            \
        " mtc0  $3, $12;"               \
        " nop; nop; nop;"               \
        :                               \
        : "r"(_vector_)                 \
        : "$2", "$3"                    \
        );                              \
CYG_MACRO_END

#define HAL_INTERRUPT_UNMASK( _vector_ )\
CYG_MACRO_START                         \
    asm volatile (                      \
        "mfc0   $3, $12;"               \
        "la     $2, 0x00000100;"        \
        "sllv   $2, $2, %0;"            \
        "or     $3, $3, $2;"            \
        "mtc0   $3, $12;"               \
        "nop; nop; nop;"                \
        :                               \
        : "r"(_vector_)                 \
        : "$2", "$3"                    \
        );                              \
CYG_MACRO_END

#define HAL_INTERRUPT_ACKNOWLEDGE( _vector_ )\
CYG_MACRO_START                         \
    asm volatile (                      \
        ".set push;"                    \
        ".set noreorder;"               \
        "sltiu $2, %0, 2;"              \
        "beqz  $2, 1f;"                 \
        " li    $2, 0x100;"             \
        "sllv  $2, $2, %0;"             \
        "mfc0  $3, $13;"                \
        "not   $2;"                     \
        "and   $3, $2;"                 \
        "mtc0  $3, $13;"                \
        "nop ;"                         \
        "1:"                            \
        ".set pop"                      \
        :                               \
        : "r"(_vector_)                 \
        : "$2", "$3"                    \
        );                              \
CYG_MACRO_END

#define HAL_INTERRUPT_CONFIGURE( _vector_, _level_, _up_ )

#define HAL_INTERRUPT_SET_LEVEL( _vector_, _level_ )

#ifdef CYGPKG_HAL_SMP_SUPPORT    

#define HAL_INTERRUPT_SET_CPU( _vector_, _cpu_ ) \
    CYG_MACRO_START                 \
    asm volatile (                  \
        " .set push; .set noreorder;" \
        " mfc0  $8, $22, 1;"        \
        " li    $9, 3;"             \
        " bltu  %0, 2, 2f;"         \
        "  sll   $9, 15;"           \
        " li    $9, (1<<25);"       \
        " bnez  %1, 2f;"            \
        "  sll   $9, %0;"           \
        " xor   $8, $9;"            \
        "2:"                        \
        " or    $8, $9;"            \
        "3:"                        \
        " mtc0  $8, $22, 1;"        \
        " .set pop;"                \
        :                           \
        : "r"(_vector_), "r"(_cpu_) \
        : "$8", "$9"                \
    );                              \
    CYG_MACRO_END
#define HAL_INTERRUPT_GET_CPU( _vector_, _cpu_ ) \
    CYG_MACRO_START                 \
    asm volatile (                  \
        " .set push; .set noreorder;" \
        " bltu  %1, 2, 1f;"         \
        "  move  %0, %1;"           \
        " mfc0  %0, $22, 1;"        \
        " addiu $8, %0, 25;"        \
        " srl   %0, $8;"            \
        " andi  %0, 1;"             \
        "1:"                        \
        " .set pop;"                \
        : "=r"(_cpu_)               \
        : "r"(_vector_)             \
        : "$8"                      \
    );                              \
    CYG_MACRO_END

#define CYGNUM_HAL_SMP_CPU_INTERRUPT_VECTOR(_n_) \
	((_n_ == 0) ? CYGNUM_HAL_INTERRUPT_SOFTWARE_0 : CYGNUM_HAL_INTERRUPT_SOFTWARE_1)

#endif


#define CYGHWR_HAL_INTERRUPT_CONTROLLER_ACCESS_DEFINED

#endif

#endif

// BRCM mod - start.
//--------------------------------------------------------------------------
// Watchdog check.
//
// Check if the watch-dog timer is about to expire. If it is, invoke the
// registered callback to allow the application to perform any
// shutdown procedures necessary prior to the chip being reset
// asynchronously when the watch-dog timer expires.
//

#ifndef __ASSEMBLER__

externC void hal_check_watchdog_timer( void );

#define HAL_CHECK_WATCHDOG_TIMER()  hal_check_watchdog_timer()

#endif

// BRCM mod - end.


//--------------------------------------------------------------------------
// Interrupt controller access
// The default code here simply uses the fields present in the CP0 status
// and cause registers to implement this functionality.
// Beware of nops in this code. They fill delay slots and avoid CP0 hazards
// that might otherwise cause following code to run in the wrong state or
// cause a resource conflict.

#ifndef __ASSEMBLER__

// We implement our own interrupt mechanism for internal and external
// interrupts instead of directly using eCos interrupts.  These functions
// map ISR functions which are called indirectly through our mechanism.

// Sub-interrupts are internal interrupts which are all mapped through
// the peripheral interrupt.
externC bool hal_map_periph_interrupt( cyg_uint32 subIsrNum,
                                       void     (*newSubIsr)(void *),
                                       void      *newData );
externC void hal_enable_periph_interrupt( cyg_uint32 subIsrNum );
externC void hal_disable_periph_interrupt( cyg_uint32 subIsrNum );


// External interrupts are normal interrupts, but we keep track of the
// eCos interrupt data structures and use simple functions for the ISRs.
externC bool hal_map_ext_interrupt( cyg_uint32  extIsrNum,
                                    void      (*newExtIsr)(void *),
                                    void       *newData );
externC void hal_enable_ext_interrupt( cyg_uint32 extIsrNum );
externC void hal_disable_ext_interrupt( cyg_uint32 extIsrNum );

// When the MIPS external interrupts aren't used for BCM external interrupts,
// we may want to use the MIPS interrupts for other purposes.
externC bool hal_map_mips_interrupt( cyg_uint32  mipsIsrNum,
                                     void      (*newMipsIsr)(void *),
                                     void       *newData );


// Software interrupts are normal interrupts, but we keep track of the
// eCos interrupt data structures and use simple functions for the ISRs.
externC bool hal_map_software_interrupt( cyg_uint32  extIsrNum,
                                         void      (*newExtIsr)(void *),
                                         void       *newData );
//externC void hal_enable_software_interrupt( cyg_uint32 extIsrNum );
//externC void hal_disable_software_interrupt( cyg_uint32 extIsrNum );


// We want to support multiple clocks on different platforms, so we can't use
// a hard-coded constant.  This global is defined with weak linkage so it can
// be overridden in the application.  The default value is (cpu_clock / 2 / 100).
externC cyg_uint32 hal_mips_rtc_period;

#endif // __ASSEMBLER__

#endif /* ifndef CYGONCE_HAL_PLF_INTR_H */
//--------------------------------------------------------------------------
// End of plf_intr.h
