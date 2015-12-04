//==========================================================================
//
//      plf_misc.c
//
//      HAL platform miscellaneous functions
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
// Author(s):    msieweke
// Contributors: nickg, jlarmour, dmoseley, jskov
// Date:         2003-06-16
// Purpose:      HAL miscellaneous functions
// Description:  This file contains miscellaneous functions provided by the
//               HAL.
//
//####DESCRIPTIONEND####
//
// ====================================================================
// Portions of this software Copyright (c) 2003-2010 Broadcom Corporation
// ====================================================================
//========================================================================*/

#define CYGARC_HAL_COMMON_EXPORT_CPU_MACROS
#include <pkgconf/hal.h>

#include <cyg/infra/cyg_type.h>         // Base types
#include <cyg/infra/cyg_trac.h>         // tracing macros
#include <cyg/infra/cyg_ass.h>          // assertion macros
#include <cyg/infra/diag.h>             // diag output

#include <cyg/hal/hal_arch.h>           // architectural definitions
#include <cyg/hal/hal_intr.h>           // Interrupt handling
#include <cyg/hal/hal_cache.h>          // Cache handling
#include <cyg/hal/hal_if.h>
#include <cyg/hal/hal_diag.h>
#include <cyg/hal/bcm33xx_regs.h>

#include <cyg/kernel/kapi.h>


//--------------------------------------------------------------------------

#define MAP_VECTOR_TO_BRCM_EXT_INTERRUPT( vector ) \
                  ((vector) - CYGNUM_HAL_INTERRUPT_PERIPH - 1)

#define MAP_BRCM_EXT_INTERRUPT_TO_VECTOR( int ) \
                  ((int) + CYGNUM_HAL_INTERRUPT_PERIPH + 1)

//--------------------------------------------------------------------------

externC void diag_write_string( const char* );
static void hal_init_mips_interrupts( void );
//static void hal_init_external_interrupts( void );

//--------------------------------------------------------------------------

// We want to support multiple clocks on different platforms, so we can't use
// a hard-coded constant.  This global is defined with weak linkage so it can
// be overridden in the application.
cyg_uint32 hal_mips_rtc_period __attribute__ ((weak)) = 
    (CYGHWR_HAL_MIPS_BCM33XX_CPU_CLOCK / 2) / CYGNUM_HAL_RTC_DENOMINATOR;
cyg_uint32 hal_mips_periph_interrupt __attribute__ ((weak)) =
    CYGNUM_HAL_INTERRUPT_PERIPH;

// The eCos library is built for generic 33xx devices.  The 3368 uses different
// addresses for cores and registers, so we make these variables with weak
// linkage to be substituted in the application.
#ifndef CYGPKG_KERNEL_SMP_SUPPORT
volatile IntControl *INTC  __attribute__ ((weak)) = (IntControl *)  BCM_INTC_BASE;
volatile Uart       *UART  __attribute__ ((weak)) = (Uart *)        BCM_UART0_BASE;
volatile Uart       *UART1 __attribute__ ((weak)) = (Uart *)        BCM_UART1_BASE;
volatile Sdram      *SDRAM __attribute__ ((weak)) = (Sdram*)        BCM_SDRAM_BASE;
volatile Timer      *TIMER __attribute__ ((weak)) = (Timer*)        BCM_TIMER_BASE;
#else // 3368 / 3381 / 3255 SMP.  These are useful for simulation.
volatile IntControl *INTC  __attribute__ ((weak)) = (IntControl *)  0xfff8c000;
volatile Uart       *UART  __attribute__ ((weak)) = (Uart *)        0xfff8c100;
volatile Uart       *UART1 __attribute__ ((weak)) = (Uart *)        0xfff8c120;
volatile Sdram      *SDRAM __attribute__ ((weak)) = (Sdram*)        0xfff84000;
volatile Timer      *TIMER __attribute__ ((weak)) = (Timer*)        0xfff8c040;
#endif
volatile cyg_uint32 *INTC_IrqStatus     __attribute__ ((weak)) = (cyg_uint32*)        0;
volatile cyg_uint32 *INTC_IrqEnableMask __attribute__ ((weak)) = (cyg_uint32*)        0;
volatile cyg_uint32 *INTC_IrqStatusExt     __attribute__ ((weak)) = (cyg_uint32*)        0;
volatile cyg_uint32 *INTC_IrqEnableMaskExt __attribute__ ((weak)) = (cyg_uint32*)        0;

//--------------------------------------------------------------------------

//------------------------------------------------------------------------
// This function is specifically intended to be overridden in the BSP for
// any chip which requires special init code.
void chip_specific_init(void) __attribute__ ((weak));
void chip_specific_init(void)
{
}


void hal_platform_init(void)
{
    // If the INTC_* variables are not defined, hal_if_init() will fail.
    if ( INTC_IrqStatus == 0 )
    {
        INTC_IrqStatus      = &INTC->IrqStatus;
        INTC_IrqEnableMask  = &INTC->IrqEnableMask;
    }

    // Set up eCos/ROM interfaces
    hal_if_init();

    // Do any chip-specific initialization - in the app-level code.
    chip_specific_init();

#if defined(CYGPKG_CYGMON)
    diag_write_string(" CYGMON");
#elif defined(CYGPKG_REDBOOT)
    diag_write_string(" RedBoot");
#else
    diag_write_string(" eCos");
#endif

    hal_init_mips_interrupts();
}


//------------------------------------------------------------------------
// This function is specifically intended to be overridden in the BSP for
// any chip which requires special init code.
void chip_specific_cache_init(void) __attribute__ ((weak));
void chip_specific_cache_init(void)
{
}


//------------------------------------------------------------------------
// There are 32 internal interrupt sources routed through the periphal interrupt,
// so we want to be have an ISR for each one (let's call them sub-ISRs).  Each can
// specify a data value to be passed in as a parameter.  For example a single 
// sub-ISR could handle two or more sources, and the data would indicate which 
// source caused the interrupt.

static void   (*subIsr[64])( void *data )               = { NULL };
static void    *subIsrData[64]                          = { NULL };


static void MapIsrs( volatile cyg_uint32 *mask,
                     volatile cyg_uint32 *status,
                     void   (*subIsr[64])( void *data ),
                     void    *subIsrData[64] )
{
    cyg_uint32  intActive;
    cyg_uint32  intNum;
    cyg_uint32  intMask;
    int         skipCount;

    while (( intActive = *mask & *status ) != 0 )
    {
        while ( intActive != 0 )
        {
            // Look at the leftmost active interrupt bit.  This is
            // optimized by using assembler to skip leading zeroes:
            asm( "clz %0, %1" : "=r" (skipCount) : "r" (intActive) );

            // clz counts from the left, but we number our interrupts from the right.
            intNum  = 31 - skipCount;
            intMask = 1 << intNum;

            // Clear the bit in the active interrupt mask, so it won't be found
            // next time around this loop.
            intActive &= ~intMask;

            // Disable the interrupt.  The ISR must re-enable it.
            // The ISR/IST is also responsible for acknowledging the interrupt, if
            // that is required.
            *mask &= ~intMask;

            // Call interrupt handler.
            if ( subIsr[intNum] != 0 )
            {
                subIsr[intNum]( subIsrData[intNum] );
            }
        }
    }
}


//------------------------------------------------------------------------
// Initial interrupt service routine for peripheral interrupt. This interrupt is
// the sink for all all internal interrupts - most of which should be disabled
// when this ISR is attached.

static void hal_dsr_periph( cyg_vector_t    vector,
                            cyg_ucount32    count,
                            cyg_addrword_t  data )
{

    MapIsrs( INTC_IrqEnableMask,
             INTC_IrqStatus,
             subIsr,
             subIsrData );

    if ( INTC_IrqEnableMaskExt != 0 )
    {
        MapIsrs(  INTC_IrqEnableMaskExt,
                  INTC_IrqStatusExt,
                 &subIsr[32],
                 &subIsrData[32] );
    }

    // The manual says this has no effect.
//    HAL_INTERRUPT_ACKNOWLEDGE( vector );

    // Re-enable our interrupt, which was disabled in the ISR.
    HAL_INTERRUPT_UNMASK( vector );
}


//------------------------------------------------------------------------
// Register a sub-interrupt handler for the peripheral ISR. There are 32 possible
// handlers, although some are not used on some CPUs.

bool hal_map_periph_interrupt( cyg_uint32  subIsrNum,
                               void      (*newSubIsr)(void *), 
                               void       *newData )
{
    if ( subIsrNum < 64 )
    {
        subIsr[subIsrNum]     = newSubIsr;
        subIsrData[subIsrNum] = newData;
        return true;
    }
    else
    {
        return false;
    }
}


//------------------------------------------------------------------------
// Enable a sub-interrupt under the peripheral ISR. This is a simple 32-bit mask,
// and the interrupt numbers are defined as the bit offset.

void hal_enable_periph_interrupt( cyg_uint32 subIsrNum )
{
    if ( subIsrNum < 32 )
    {
        *INTC_IrqEnableMask |= (1 << subIsrNum);
    }
    else if ( ( subIsrNum < 64 ) && ( INTC_IrqEnableMaskExt != 0 ) )
    {
        *INTC_IrqEnableMaskExt |= (1 << ( subIsrNum - 32 ));
    }
}


//------------------------------------------------------------------------
// Disable a sub-interrupt under the peripheral ISR. This is a simple 32-bit mask,
// and the interrupt numbers are defined as the bit offset.

void hal_disable_periph_interrupt( cyg_uint32 subIsrNum )
{
    if ( subIsrNum < 32 )
    {
        *INTC_IrqEnableMask &= ~(1 << subIsrNum);
    }
    else if ( ( subIsrNum < 64 ) && ( INTC_IrqEnableMaskExt != 0 ) )
    {
        *INTC_IrqEnableMaskExt &= ~(1 << ( subIsrNum - 32 ));
    }
}


//------------------------------------------------------------------------
// There are 7 MIPS interrupts (SW0-1, HW0-4).  They all use the same ISR, which
// only disables the MIPS interrupt and triggers the DSR to run.
// We allow the app code to map a simple function call to each vector, and we
// maintain the eCos interrupt data structures.

static void      (*mipsIsr[7])( void *data ) = { NULL };
static void       *mipsIsrData[7]            = { NULL };


//------------------------------------------------------------------------
static cyg_uint32 hal_isr_mips( cyg_vector_t    vector, 
                                cyg_addrword_t  data )
{
//    hal_diag_write_char( 'x' );
    // Disable the interrupt until after the DSR runs.
    HAL_INTERRUPT_MASK( vector );

    // This ISR's only function is to kick off the DSR.
    return( CYG_ISR_HANDLED | CYG_ISR_CALL_DSR );
}


// This function is intended to be called from the eCos kernel ISR.  The
// external interrupts are mapped differently on some chips, so we need to
// be able to override the default action.
void BcmHalClearExtInterrupt( cyg_vector_t vector ) __attribute__ ((weak));
void BcmHalClearExtInterrupt( cyg_vector_t vector )
{
    // Note that our external interrupts 0-3 are equivalent to eCos vectors
    // 3-6.  So we have to map "vector" to get a Broadcom interrupt number.
    INTC->extIrqMskandDetClr |= 1 << (MAP_VECTOR_TO_BRCM_EXT_INTERRUPT( vector ));
}


//------------------------------------------------------------------------
static void hal_dsr_mips( cyg_vector_t    vector, 
                          cyg_ucount32    count,
                          cyg_addrword_t  data )
{
    // Acknowledge the interrupt, because the external "ISR" may unmask it.
    HAL_INTERRUPT_ACKNOWLEDGE( vector );

    // Clear the external interrupt, so we won't get another interrupt before
    // we're ready.  Don't call this for software or peripheral interrupts.
    if ( vector > hal_mips_periph_interrupt )
    {
        BcmHalClearExtInterrupt( vector );
    }

    // Call the associated external interrupt routine, with parameter.
    if ( mipsIsr[vector] != NULL )
    {
        mipsIsr[vector]( mipsIsrData[vector] );
    }

    // The external ISR should re-enable the interrupt vector.
    // HAL_INTERRUPT_UNMASK( vector );
}


// There are 7 usable MIPS interrupts, 2 SW and 5 HW.
static cyg_handle_t   hal_isr_mips_handle[7];
static cyg_interrupt  hal_isr_mips_interrupt[7];


//------------------------------------------------------------------------
// Create the data structures for a single MIPS interrupt.

static void hal_create_mips_int( cyg_vector_t vector )
{
    // Most interrupts can use a generic DSR.  The peripheral interrupt has to
    // handle sub-interrupts in the periph block.
    cyg_DSR_t *dsr = hal_dsr_mips;
    if ( vector == hal_mips_periph_interrupt )
    {
        dsr = hal_dsr_periph;
    }

    cyg_interrupt_create( vector,
                          0,                    // priority (unused in MIPS)
                          0,                    // Data item passed to ISR
                          &hal_isr_mips,
                          dsr,
                          &hal_isr_mips_handle[vector],
                          &hal_isr_mips_interrupt[vector] );
    cyg_interrupt_attach( hal_isr_mips_handle[vector] );
    HAL_INTERRUPT_UNMASK( vector );
}


//------------------------------------------------------------------------
// Configure our ISRs for external interrupts.  The application may replace this with
// its own ISR at a later time.

static void hal_init_mips_interrupts( void )
{
  #ifndef CYGPKG_KERNEL_SMP_SUPPORT
    // Interrupts 0 and 1 are software interrupts, which are handled elsewhere
    // when using SMP.
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_0 );
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_1 );
  #endif
    // Interrupts 2-6 are "external" interrupts for peripherals.
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_2 );
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_3 );
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_4 );
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_5 );
    hal_create_mips_int( CYGNUM_HAL_INTERRUPT_6 );
    // Interrupt 7 is the MIPS timer interrupt, handled in the kernel.
}

//------------------------------------------------------------------------
// Register an external interrupt handler for one of the MIPS interrupts.

bool hal_map_mips_interrupt( cyg_uint32  vector, 
                             void      (*newMipsIsr)(void *), 
                             void       *newData )
{
    if ( vector < CYGNUM_HAL_INTERRUPT_7 )
    {
        mipsIsr[vector]     = newMipsIsr;
        mipsIsrData[vector] = newData;
        return true;
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------
// Register an external interrupt handler.  There are 4 possible external
// sources, numbered 0-3, corresponding to MIPS interrupts 1-4, or vectors 3-6.

bool hal_map_ext_interrupt( cyg_uint32  extIsrNum, 
                            void      (*newExtIsr)(void *), 
                            void       *newData )
{
    if ( extIsrNum < 4 )
    {
        return hal_map_mips_interrupt( CYGNUM_HAL_INTERRUPT_3 + extIsrNum, newExtIsr, newData );
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------
// Enable an external interrupt, numbered 0-3.

void hal_enable_ext_interrupt( cyg_uint32 extIsrNum )
{
    cyg_uint32 vector = MAP_BRCM_EXT_INTERRUPT_TO_VECTOR( extIsrNum );
    HAL_INTERRUPT_UNMASK( vector );
}


//------------------------------------------------------------------------
// Disable an external interrupt, numbered 0-3.

void hal_disable_ext_interrupt( cyg_uint32 extIsrNum )
{
    cyg_uint32 vector = MAP_BRCM_EXT_INTERRUPT_TO_VECTOR( extIsrNum );
    HAL_INTERRUPT_MASK( vector );
}



#ifndef CYGPKG_KERNEL_SMP_SUPPORT

//------------------------------------------------------------------------
// Register a software interrupt handler.  There are 2 possible software
// sources, numbered 0-1.

bool hal_map_software_interrupt( cyg_uint32  swIsrNum,
                                 void      (*newSwIsr)(void *),
                                 void       *newData )
{
    if ( swIsrNum < 2 )
    {
        return hal_map_mips_interrupt( CYGNUM_HAL_INTERRUPT_0 + swIsrNum, newSwIsr, newData );
    }
    else
    {
        return false;
    }
}

#endif // !defined CYGPKG_KERNEL_SMP_SUPPORT


/*------------------------------------------------------------------------*/
/* Reset support                                                          */

void hal_bcm33xx_reset(void)
{
    diag_write_string( "(hal_bcm33xx_reset) Resetting...\n" );
//***** unfinished
    for(;;);                            // wait for it
}



//*****************************************************************************
// Calculate RAM size by writing test values to RAM.  This counts on the address
// being partially decoded, so RAM will be duplicated (RAM space)/(true size) times.
//
// DANGER: Since this function modifies memory owned by other tasks, it is
// not interrupt safe.  To guard against memory corruption, interrupts are
// disabled while it runs.
//
// We shouldn't have to worry about overwriting the stack and local variables,
// because the initial stack grows downward from location 0x10000.

cyg_uint32 CalculateRamSize(void) __attribute__ ((weak));
cyg_uint32 CalculateRamSize(void)
{
    unsigned long __state;
    cyg_uint16    RevID;

    RevID = INTC->RevID >> 16;
    // Assume the 3368 has exactly 16M of RAM.
    if ( RevID == 0x3368 || RevID == 0x0000 )
    {
        return 16 * 1024 * 1024;
    }
    // Assume the 3255 has 32M of RAM.
    if ( RevID == 0x3255)
    {
        return 32 * 1024 * 1024;
    }

    HAL_DISABLE_INTERRUPTS(__state);

    // These are the base-2 logs of address space size (in megabytes) for values
    // in the memory base register/size field.
    static cyg_uint8  MemSizeValues[8] = { 1, 3, 4, 5, 6, 7, 8, 9 };

    cyg_uint32   MemSizeReg;          // Size of the memory address space - an enum
                                  // where 0=2M, 1=8M, 2=16M, 3=32M, 4=64M...
    cyg_uint32   DetectedRamSize = 0; // Set to 0 to eliminate compile warning.
    cyg_uint32   SavedValues[8];      // Saved values for 1M - 128M

    int                   i;
    int                   ProbeLimit;
    volatile cyg_uint32  *ProbeAddress;
    cyg_uint32            RamBase        = 0xa0000000; // RAM address 0 in non-cached space
    cyg_uint32            RamBaseValue;

    // The memory-base-register/memory-space says how large RAM _could_ be.  We 
    // can't probe outside this address space without getting a bus error.
    MemSizeReg  = SDRAM->memoryBase & SD_MEMSIZE_MASK;

    // Convert from the register value to the base-2 logarithm of the memory 
    // address space size in megabytes.
    ProbeLimit   = MemSizeValues[MemSizeReg];

    // Save the value at RAM location 0.
    ProbeAddress  = (cyg_uint32*) RamBase;
    RamBaseValue  = *ProbeAddress;

    // Save values at address 1M, 2M, 4M, 8M, etc.  Then store 0 in each location.
    // ( Remember that 1<<20 is 1M. )
    for (i = 0; i < ProbeLimit; i++)
    {
        ProbeAddress   = (cyg_uint32*) (RamBase + (1 << (i + 20)));
        SavedValues[i] = *ProbeAddress;
        *ProbeAddress  = 0;
    }

    #define PROBE_VALUE 0xf0f0f0f0

    // Write a non-zero value to RAM location 0.  Where RAM wraps around, this
    // value will show up above the top of memory.
    ProbeAddress  = (cyg_uint32*) RamBase;
    *ProbeAddress = PROBE_VALUE;
    
    // Start probing at memory address 1Meg.  Technically it's not necessary to
    // probe addresses at 1M or 4M, but this way is simpler.
    for (i = 0; i < ProbeLimit; i++)
    {
        ProbeAddress  = (cyg_uint32*) (RamBase + (1 << (i + 20)));
        if (*ProbeAddress == PROBE_VALUE)
        {
            break;
        }
        // Restore the saved value for this address.
        *ProbeAddress = SavedValues[i];
    }
    // RAM size is 2^i megabytes...
    DetectedRamSize = 1 << (i + 20);

    // Restore the value we found at RAM address 0.
    ProbeAddress  = (cyg_uint32*) RamBase;
    *ProbeAddress =  RamBaseValue;

    HAL_RESTORE_INTERRUPTS(__state);

    return DetectedRamSize;
}


//------------------------------------------------------------------------
// Run-time memory size info.

// Allow the application to set an arbitrary limit on memory usage.
// If the application defines this variable, that's used as the size limit.
cyg_uint32 hal_mips_mem_limit   __attribute__ ((weak)) = 64*1024*1024;

// Allow the application to reserve space at the top of memory.
// We reserve 4k at the top of RAM, for no good reason.  VxWorks reserved
// 64k, apparently for the vxWorks bootloader, which we aren't using.
cyg_uint32 hal_mips_mem_reserve __attribute__ ((weak)) = 4*1024;

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

cyg_uint8 *hal_mips_mem_real_region_top( cyg_uint8 *_regionend_ )
{
  
    static cyg_uint8  *memTop   = NULL;
    // Don't assume which segment to put the "memory top" value.  Use the
    // same segment as the one passed in.
    cyg_uint32         segMask  = (cyg_uint32)_regionend_ & 0xf0000000;

    if (memTop == NULL)
    {
        memTop = (cyg_uint8 *)( MIN( CalculateRamSize(), hal_mips_mem_limit ) - hal_mips_mem_reserve + segMask);
    }
    return memTop;
}


// The watch-dog is initialized by setting the counter value register
// However, the watch-dog control register needs to be read to get the
// current counter value. Reading the counter value register simply
// returns the original value that was written to it.

extern cyg_uint32 gWatchDogMinimumValue;
extern void     (*gWatchDogResetCallout)(void);

void hal_check_watchdog_timer( void ) __attribute__ ((weak));
void hal_check_watchdog_timer( void )
{
    static bool CalloutCalled = false;

    if (TIMER->WatchDogCtl < gWatchDogMinimumValue)
    {
        if (gWatchDogResetCallout != 0 && !CalloutCalled)
        {
            (*gWatchDogResetCallout)();
            CalloutCalled = true;
        }
    }
}


void BcmTestAndComputeLocalNetIf( void ) __attribute__ ((weak));
void BcmTestAndComputeLocalNetIf( void )
{
}
void BcmShowMbuf( void ) __attribute__ ((weak));
void BcmShowMbuf( void )
{
}
void BcmDefIpOutputRoutingMods( void ) __attribute__ ((weak));
void BcmDefIpOutputRoutingMods( void )
{
}
void BcmTestAndApplyUdpOutputRouting( void ) __attribute__ ((weak));
void BcmTestAndApplyUdpOutputRouting( void )
{
}
void BcmTestAndApplyRawIpOutputRouting( void ) __attribute__ ((weak));
void BcmTestAndApplyRawIpOutputRouting( void )
{
}
void BcmIsIpAddrOnAnyLocalNetIf( void ) __attribute__ ((weak));
void BcmIsIpAddrOnAnyLocalNetIf( void )
{
}
void BcmComputeDefaultGatewayIpAndMacAddr( void ) __attribute__ ((weak));
void BcmComputeDefaultGatewayIpAndMacAddr( void )
{
}
void BcmIsIpAddrOnThisLocalNetIf( void ) __attribute__ ((weak));
void BcmIsIpAddrOnThisLocalNetIf( void )
{
}
void ethmac_ntoa( void ) __attribute__ ((weak));
void ethmac_ntoa( void )
{
}


/*------------------------------------------------------------------------*/
/* End of plf_misc.c                                                      */
