//==========================================================================
//
//      plf_smp.c
//
//      HAL SMP implementation
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
// Author(s):    Mike Sieweke (based on pcmb version by nickg)
// Contributors: nickg
// Date:         2001-08-03
// Purpose:      HAL SMP implementation
// Description:  This file contains SMP support functions.
//
//####DESCRIPTIONEND####
//
//========================================================================*/

#include <pkgconf/hal.h>

#ifdef CYGPKG_HAL_SMP_SUPPORT

#ifdef CYGPKG_KERNEL
  #include <pkgconf/kernel.h>
#endif

#include <cyg/infra/cyg_type.h>
#include <cyg/infra/diag.h>

#include <cyg/hal/hal_intr.h>
#include <cyg/hal/hal_smp.h>
#include <cyg/hal/hal_if.h>

//#define debug_printf diag_printf
#define debug_printf(...)

//------------------------------------------------------------------------
// Exported SMP variables.

cyg_uint32 cyg_hal_smp_cpu_count = HAL_SMP_CPU_MAX;

cyg_uint8  cyg_hal_smp_cpu_flags[HAL_SMP_CPU_MAX];

cyg_uint32 cyg_hal_smp_default_affinity __attribute__ (( weak )) = 0xffffffff;

cyg_uint32 cyg_hal_smp_master_cpu = 0;

//------------------------------------------------------------------------
// Local (mostly) variables

// These are used to synchronize exception handling and screen output.
volatile cyg_uint32 cyg_hal_smp_cpu_sync_flag[HAL_SMP_CPU_MAX];
volatile cyg_uint32 cyg_hal_smp_cpu_sync[HAL_SMP_CPU_MAX];

// This isn't really very useful.
volatile cyg_uint32 cyg_hal_smp_cpu_running[HAL_SMP_CPU_MAX];

// When running without kernel SMP support, this holds function pointers for
// the slave(s) to execute.
volatile void (*cyg_hal_smp_cpu_entry[HAL_SMP_CPU_MAX])(void);

// Used for synchronizing exceptions in a ROM monitor (not used).
volatile cyg_uint32 cyg_hal_smp_vsr_sync_flag;


//------------------------------------------------------------------------
// Send a software interrupt to the other CPU.  Note that all software
// interrupts are set for "crossover", so we can't send one to ourself.

static inline void software_interrupt( HAL_SMP_CPU_TYPE cpu )
{
    cyg_uint32  swIntBit = 1 << (8 + cpu);
    asm(".set push;"
        ".set noat;"
        "mfc0  $1, $13;" // Get the status register.
        "or    $1, %0;"  // Set the software interrupt bit.
        "mtc0  $1, $13;"
        ".set pop"
        : : "r" (swIntBit) );
}

//------------------------------------------------------------------------
// Set up an init function, and start a slave CPU.

// The trampoline is a function to get the slave(s) started.
__externC cyg_uint32 cyg_hal_slave_trampoline;
__externC cyg_uint32 cyg_hal_slave_trampoline_end;

// Use these to hold the slave(s) until the scheduler is ready for them to start.
static  HAL_SPINLOCK_TYPE slave_lock[HAL_SMP_CPU_MAX];

__externC void cyg_hal_cpu_start( HAL_SMP_CPU_TYPE cpu )
{
    cyg_uint32 *p = &cyg_hal_slave_trampoline;
    cyg_uint32 *q = (cyg_uint32 *) 0xa0000200;

    debug_printf( "cyg_hal_cpu_start - copying trampoline\n" );
    // Copy the trampoline to the slave's exception vector.
    do
    {
        *q++ = *p++;
    } while( (cyg_uint32)p < (cyg_uint32)&cyg_hal_slave_trampoline_end );

    // Init synchronization spinlock as locked to halt slave CPU in
    // cyg_hal_smp_startup().
    slave_lock[cpu] = 1;

    debug_printf( "cyg_hal_cpu_start - starting slave CPU\n" );
    // Send a software interrupt to the slave processor.  It will run the
    // trampoline code, which should cause it to run cyg_hal_smp_start().
    // Due to some obsolete code in the bootloader, it's waiting on
    // software interrupt 0 instead of interrupt 1.
    software_interrupt( 0 );
}

//------------------------------------------------------------------------
// This is called by Cyg_Scheduler::start() to allow each slave
// CPU to continue into the scheduler.

__externC void cyg_hal_cpu_release( HAL_SMP_CPU_TYPE cpu )
{
    debug_printf( "releasing CPU %d...\n", cpu );
    HAL_SPINLOCK_CLEAR( slave_lock[cpu] );
    debug_printf( "released\n" );
}

//------------------------------------------------------------------------

void  cyg_kernel_smp_startup( void );

__externC void cyg_hal_smp_startup(void)
{
    HAL_SMP_CPU_TYPE cpu;

    cpu  = HAL_SMP_CPU_THIS();

    debug_printf( "cyg_hal_smp_startup - slave starting\n" );

    // Halt each slave until the scheduler is ready for it.  This may not be
    // necessary, but it won't hurt.
    debug_printf( "cyg_hal_smp_startup - slave waiting on spinlock\n" );
    HAL_SPINLOCK_SPIN( slave_lock[cpu] );
    debug_printf( "cyg_hal_smp_startup - slave continuing\n" );

    HAL_ENABLE_INTERRUPTS();

#ifdef CYGPKG_KERNEL_SMP_SUPPORT

    cyg_hal_smp_cpu_running[cpu] = 1;
    cyg_kernel_smp_startup();

#else

    for(;;)
    {
        void (*entry)(void);

        while( (entry = cyg_hal_smp_cpu_entry[cpu]) == 0 )
        {
            hal_delay_us( 100 );
        }

        cyg_hal_smp_cpu_entry[cpu] = 0;

        if( entry != NULL )
        {
            cyg_hal_smp_cpu_running[cpu] = 1;
            entry();
        }
    }

#endif
}


//------------------------------------------------------------------------

__externC void cyg_hal_smp_cpu_start_all(void)
{
    HAL_SMP_CPU_TYPE cpu;

    // This function runs from the master, so we can save the cpu number.
    cyg_hal_smp_master_cpu = HAL_SMP_CPU_THIS();

    for( cpu = 0; cpu < HAL_SMP_CPU_COUNT(); cpu++ )
    {
        debug_printf( "plf_smp - Starting CPU %d\n", cpu );
        cyg_hal_smp_cpu_sync[cpu]      = 0;
        cyg_hal_smp_cpu_sync_flag[cpu] = 0;
        cyg_hal_smp_cpu_running[cpu]   = 0;
        cyg_hal_smp_cpu_entry[cpu]     = 0;

        if( cpu != HAL_SMP_CPU_THIS() )
            cyg_hal_cpu_start( cpu );
        else
            cyg_hal_smp_cpu_running[cpu] = 1;
    }
}


//------------------------------------------------------------------------
// SMP message buffers.
// SMP CPUs pass messages to eachother via a small circular buffer
// protected by a spinlock. Each message is a single 32 bit word with
// a type code in the top 4 bits and any argument in the remaining
// 28 bits.

#define SMP_MSGBUF_SIZE 4   // Note this must be a power of 2.

static struct smp_msg_t
{
    HAL_SPINLOCK_TYPE           lock;           // protecting spinlock
    volatile cyg_uint32         msgs[SMP_MSGBUF_SIZE]; // message buffer
    volatile cyg_uint32         head;           // head of list
    volatile cyg_uint32         tail;           // tail of list
    volatile cyg_uint32         reschedule;     // reschedule request
    volatile cyg_uint32         timeslice;      // timeslice request
} smp_msg[HAL_SMP_CPU_MAX];


//------------------------------------------------------------------------
// Pass a message to another CPU.

__externC void cyg_hal_cpu_message( HAL_SMP_CPU_TYPE cpu,
                                    cyg_uint32       msg,
                                    cyg_uint32       arg,
                                    cyg_uint32       wait)
{
    int                  i;
    CYG_INTERRUPT_STATE  istate;
    struct smp_msg_t    *m      = &smp_msg[cpu];

    HAL_DISABLE_INTERRUPTS( istate );

    // Get access to the message buffer for the selected CPU
    HAL_SPINLOCK_SPIN( m->lock );

    if ( msg == HAL_SMP_MESSAGE_RESCHEDULE )
    {
        m->reschedule = true;
    }
    else if ( msg == HAL_SMP_MESSAGE_TIMESLICE )
    {
        m->timeslice = true;
    }
    else
    {
        cyg_uint32 next = (m->tail + 1) & (SMP_MSGBUF_SIZE-1);

        // If the buffer is full, wait for space to appear in it.
        // This should only need to be done very rarely.

        while ( next == m->head )
        {
            HAL_SPINLOCK_CLEAR( m->lock );
            for ( i = 0; i < 1000; i++ )
            {
                // Wait
            }
            HAL_SPINLOCK_SPIN( m->lock );
        }

        m->msgs[m->tail] = msg | arg;

        m->tail = next;
    }

    // Now send an interrupt to the CPU.

    if ( cyg_hal_smp_cpu_running[cpu] )
    {
        software_interrupt( cpu );
    }

    HAL_SPINLOCK_CLEAR( m->lock );

    // If we are expected to wait for the command to complete, then
    // spin here until it does. We actually wait for the destination
    // CPU to empty its input buffer. So we might wait for messages
    // from other CPUs as well. But this is benign.

    while ( wait )
    {
        for( i = 0; i < 1000; i++ )
        {
            // Wait
        }

        HAL_SPINLOCK_SPIN( m->lock );

        if( m->head == m->tail )
        {
            wait = false;
        }

        HAL_SPINLOCK_CLEAR( m->lock );

    }

    HAL_RESTORE_INTERRUPTS( istate );
}

//------------------------------------------------------------------------

__externC cyg_uint32 cyg_hal_cpu_message_isr( cyg_uint32 vector, CYG_ADDRWORD data )
{
    HAL_SMP_CPU_TYPE    me      = HAL_SMP_CPU_THIS();
    struct smp_msg_t   *m       = &smp_msg[me];
    cyg_uint32          ret     = CYG_ISR_HANDLED;
    CYG_INTERRUPT_STATE istate;

    HAL_DISABLE_INTERRUPTS( istate );

    HAL_SPINLOCK_SPIN( m->lock );

    // First, acknowledge the interrupt.

    HAL_INTERRUPT_ACKNOWLEDGE( vector );

    if ( m->reschedule || m->timeslice )
        ret |= CYG_ISR_CALL_DSR;               // Ask for the DSR to be called.

    // Now pick messages out of the buffer and handle them

    while ( m->head != m->tail )
    {
        cyg_uint32 msg = m->msgs[m->head];

        switch ( msg & HAL_SMP_MESSAGE_TYPE )
        {
            case HAL_SMP_MESSAGE_RESCHEDULE:
                ret |= CYG_ISR_CALL_DSR;           // Ask for the DSR to be called.
                break;
            case HAL_SMP_MESSAGE_MASK:
                // Mask the supplied vector
    //            cyg_hal_interrupt_set_mask( msg&HAL_SMP_MESSAGE_ARG, false );
                break;
            case HAL_SMP_MESSAGE_UNMASK:
                // Unmask the supplied vector
    //            cyg_hal_interrupt_set_mask( msg&HAL_SMP_MESSAGE_ARG, true );
                break;
            case HAL_SMP_MESSAGE_REVECTOR:
                // Deal with a change of CPU assignment for a vector. We
                // only actually worry about what happens when the vector
                // is changed to some other CPU. We just mask the
                // interrupt locally.
    //            if( hal_interrupt_cpu[msg&HAL_SMP_MESSAGE_ARG] != me )
    //                cyg_hal_interrupt_set_mask( msg&HAL_SMP_MESSAGE_ARG, false );
                break;
        }

        // Update the head pointer after handling the message, so that
        // the wait in cyg_hal_cpu_message() completes after the action
        // requested.

        m->head = (m->head + 1) & (SMP_MSGBUF_SIZE-1);
    }

    HAL_SPINLOCK_CLEAR( m->lock );

    HAL_RESTORE_INTERRUPTS( istate );

    return ret;
}

//------------------------------------------------------------------------
// CPU message DSR.
// This is only executed if the message was HAL_SMP_MESSAGE_RESCHEDULE.
// It calls up into the kernel to effect a reschedule.

__externC void cyg_scheduler_set_need_reschedule(void);
__externC void cyg_scheduler_timeslice_cpu(void);

__externC cyg_uint32 cyg_hal_cpu_message_dsr( cyg_uint32 vector, CYG_ADDRWORD data )
{
    HAL_SMP_CPU_TYPE     me     = HAL_SMP_CPU_THIS();
    struct smp_msg_t    *m      = &smp_msg[me];
    CYG_INTERRUPT_STATE  istate;
    cyg_uint32           reschedule,
                         timeslice;

    HAL_DISABLE_INTERRUPTS( istate );
    HAL_SPINLOCK_SPIN( m->lock );

    reschedule    = m->reschedule;
    timeslice     = m->timeslice;
    m->reschedule = m->timeslice = false;

    HAL_SPINLOCK_CLEAR( m->lock );
    HAL_RESTORE_INTERRUPTS( istate );

    if ( reschedule )
    {
        cyg_scheduler_set_need_reschedule();
    }
    if ( timeslice )
    {
        cyg_scheduler_timeslice_cpu();
    }

    return 0;

}

#endif // CYGPKG_HAL_SMP_SUPPORT

//------------------------------------------------------------------------
// End of pcmb_smp.c
