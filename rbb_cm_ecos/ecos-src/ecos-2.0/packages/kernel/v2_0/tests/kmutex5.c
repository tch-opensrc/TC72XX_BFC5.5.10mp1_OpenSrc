//==========================================================================
//
//        kmutex5.c
//
//        Mutex test 5 - dynamic priority inheritance protocol
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
// Author(s):     msieweke
// Contributors:  
// Date:          14-Jun-2006
// Description:   Tests mutex priority inheritance.
//####DESCRIPTIONEND####

#include <pkgconf/hal.h>
#include <pkgconf/kernel.h>

#include <cyg/infra/testcase.h>

#include <cyg/hal/hal_arch.h>           // CYGNUM_HAL_STACK_SIZE_TYPICAL

#include <cyg/infra/diag.h>             // diag_printf

#ifdef CYGSEM_HAL_STOP_CONSTRUCTORS_ON_FLAG
externC void
cyg_hal_invoke_constructors(void);
#endif

#include <cyg/kernel/kapi.h>

#include <cyg/infra/cyg_ass.h>
#include <cyg/infra/cyg_trac.h>
#include <cyg/infra/diag.h>             // diag_printf

// ------------------------------------------------------------------------


#define STACKSIZE CYGNUM_HAL_STACK_SIZE_TYPICAL

static cyg_handle_t thread[4];

static cyg_thread thread_obj[4];

static cyg_uint32 stack[4][STACKSIZE];


// ------------------------------------------------------------------------

static cyg_mutex_t mutex1_obj;
static cyg_mutex_t mutex2_obj;
static cyg_mutex_t *mutex1;
static cyg_mutex_t *mutex2;


// ------------------------------------------------------------------------

static void t1( cyg_addrword_t data )
{
    cyg_handle_t self = cyg_thread_self();

    CYG_TEST_INFO( "Thread 1 running" );

    CYG_TEST_INFO( "Thread 1 locking mutex1 and mutex2" );
    cyg_mutex_lock( mutex1 );
    cyg_mutex_lock( mutex2 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 1 unlocking mutex2" );
    cyg_mutex_unlock( mutex2 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 1 unlocking mutex1" );
    cyg_mutex_unlock( mutex1 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 1 locking mutex1 and mutex2" );
    cyg_mutex_lock( mutex1 );
    cyg_mutex_lock( mutex2 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 1 unlocking mutex1 and mutex2" );
    cyg_mutex_unlock( mutex1 );
    cyg_mutex_unlock( mutex2 );

    // That's all.
    CYG_TEST_INFO( "Thread 1 exit" );
}

// ------------------------------------------------------------------------

static void t2( cyg_addrword_t data )
{
    cyg_handle_t self = cyg_thread_self();
    CYG_TEST_INFO( "Thread 2 running" );

    CYG_TEST_INFO( "Thread 2 locking mutex1" );
    cyg_mutex_lock( mutex1 );

    CYG_TEST_INFO( "Thread 2 unlocking mutex1" );
    cyg_mutex_unlock( mutex1 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 2 locking mutex1" );
    cyg_mutex_lock( mutex1 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 2 unlocking mutex1" );
    cyg_mutex_unlock( mutex1 );

    CYG_TEST_PASS( "Thread 2 exit" );
}

// ------------------------------------------------------------------------

static void t3( cyg_addrword_t data )
{
    cyg_handle_t self = cyg_thread_self();
    CYG_TEST_INFO( "Thread 3 running" );

    CYG_TEST_INFO( "Thread 3 locking mutex2" );
    cyg_mutex_lock( mutex2 );
    CYG_TEST_INFO( "Thread 3 unlocking mutex2" );
    cyg_mutex_unlock( mutex2 );

    cyg_thread_suspend( self );

    CYG_TEST_INFO( "Thread 3 locking mutex1" );
    cyg_mutex_lock( mutex1 );

    CYG_TEST_INFO( "Thread 3 unlocking mutex1" );
    cyg_mutex_unlock( mutex1 );

    CYG_TEST_INFO( "Thread 3 exit" );
}


static void print_priority( int threadnum )
{
    cyg_thread_info info;

    if (!cyg_thread_get_info( thread[threadnum], 0, &info ))
        diag_printf( "Error getting thread info!\n" );
    diag_printf( "Thread %d priority %d\n", threadnum, info.cur_pri );
}

static void create_thread( int threadnum, cyg_addrword_t priority, cyg_thread_entry_t *entry, char* name )
{
    diag_printf( "Creating thread %d\n", threadnum );
    cyg_thread_create( priority,
                   entry,
                   0, 
                   name,
                   (void *)(stack[threadnum]),
                   STACKSIZE,
                   &thread[threadnum],
                   &thread_obj[threadnum] );

}

static void resume_thread( int threadnum )
{
    cyg_thread_resume( thread[threadnum] );
}

// ------------------------------------------------------------------------

static void control_thread( cyg_addrword_t data )
{

    CYG_TEST_INIT();
    CYG_TEST_INFO( "Control Thread running" );

    mutex1 = &mutex1_obj;
    mutex2 = &mutex2_obj;
            
    cyg_mutex_init( mutex1 );
    cyg_mutex_init( mutex2 );

    cyg_mutex_set_protocol( mutex1, CYG_MUTEX_INHERIT );
    cyg_mutex_set_protocol( mutex2, CYG_MUTEX_INHERIT );

    create_thread( 1, 5, t1, "test 1" );
    create_thread( 2, 4, t2, "test 2" );
    create_thread( 3, 3, t3, "test 3" );

    // Start the low priority thread and give it time to lock both mutexes
    resume_thread( 1 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    // Start the mid priority thread and give it time to block on mutex1.
    resume_thread( 2 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    // Start the high priority thread and give it time to block on mutex2.
    resume_thread( 3 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    // Resume the low priority thread and give it time to release mutex2.
    resume_thread( 1 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    // Resume the low priority thread and give it time to release mutex1.
    resume_thread( 1 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    //-------------------- New test -------------------------------------
    diag_printf( "----------- New test -------------\n" );

    // Start the low priority thread and give it time to lock both mutexes
    resume_thread( 1 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    // Start the mid priority thread and give it time to block on mutex1.
    resume_thread( 2 );
    cyg_thread_delay( 20 );

    print_priority( 1 );

    // Start the high priority thread and give it time to block on mutex1.
    resume_thread( 3 );
    cyg_thread_delay( 20 );

    print_priority( 1 );
    print_priority( 2 );
    print_priority( 3 );

    // Resume the low priority thread and give it time to release mutex1.
    resume_thread( 1 );
    cyg_thread_delay( 20 );

    print_priority( 1 );
    print_priority( 2 );
    print_priority( 3 );

    // Start the mid priority thread and give it time to release mutex1.
    resume_thread( 2 );
    cyg_thread_delay( 20 );

    print_priority( 1 );
    print_priority( 2 );
    print_priority( 3 );

    CYG_TEST_EXIT( "Control Thread exit" );
}

// ------------------------------------------------------------------------

externC void
cyg_user_start( void )
{ 
#ifdef CYGSEM_HAL_STOP_CONSTRUCTORS_ON_FLAG
    cyg_hal_invoke_constructors();
#endif
    create_thread( 0, 1, control_thread, "control thread" );
    cyg_thread_resume( thread[0] );
}

// ------------------------------------------------------------------------
// EOF mutex5.c
