//==========================================================================
//
//      tests/tftp_server_test.c
//
//      Simple TFTP server test
//
//==========================================================================
//####BSDCOPYRIGHTBEGIN####
//
// -------------------------------------------
//
// Portions of this software may have been derived from OpenBSD or other sources,
// and are covered by the appropriate copyright disclaimers included herein.
//
// -------------------------------------------
//
//####BSDCOPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    gthomas
// Contributors: gthomas
// Date:         2000-04-07
// Purpose:      
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================
// TFTP test code

#include <pkgconf/system.h>
#include <pkgconf/net.h>

#ifdef CYGBLD_DEVS_ETH_DEVICE_H    // Get the device config if it exists
#include CYGBLD_DEVS_ETH_DEVICE_H  // May provide CYGTST_DEVS_ETH_TEST_NET_REALTIME
#endif

#ifdef CYGPKG_NET_TESTS_USE_RT_TEST_HARNESS // do we use the rt test?
# ifdef CYGTST_DEVS_ETH_TEST_NET_REALTIME // Get the test ancilla if it exists
#  include CYGTST_DEVS_ETH_TEST_NET_REALTIME
# endif
#endif


// Fill in the blanks if necessary
#ifndef TNR_OFF
# define TNR_OFF()
#endif
#ifndef TNR_ON
# define TNR_ON()
#endif
#ifndef TNR_INIT
# define TNR_INIT()
#endif
#ifndef TNR_PRINT_ACTIVITY
# define TNR_PRINT_ACTIVITY()
#endif

// ------------------------------------------------------------------------

#include <network.h>
#include <tftp_support.h>

#define STACK_SIZE (CYGNUM_HAL_STACK_SIZE_TYPICAL + 0x1000)
static char stack[STACK_SIZE];
static cyg_thread thread_data;
static cyg_handle_t thread_handle;

extern void
cyg_test_exit(void);

void
pexit(char *s)
{
    perror(s);
    cyg_test_exit();
}

static void
tftp_test(struct bootp *bp)
{
    int server_id, res;
    extern struct tftpd_fileops dummy_fileops;
    server_id = tftpd_start(0, &dummy_fileops);
    if (server_id > 0) {
        diag_printf("TFTP server created - id: %x\n", server_id);
        // Only let the server run for 5 minutes
        cyg_thread_delay(2*100); // let the tftpd start up first
        TNR_ON();
        cyg_thread_delay(5*60*100);
        TNR_OFF();
        res = tftpd_stop(server_id);
        diag_printf("TFTP server stopped - res: %d\n", res);
    } else {
        diag_printf("Couldn't create a server!\n");
    }
}

void
net_test(cyg_addrword_t param)
{
    diag_printf("Start TFTP server test\n");
    init_all_network_interfaces();
    TNR_INIT();
#ifdef CYGHWR_NET_DRIVER_ETH0
    if (eth0_up) {
        tftp_test(&eth0_bootp_data);
    }
#else
    if ( 0 ) ; 
#endif
#ifdef CYGHWR_NET_DRIVER_ETH1
    else if (eth1_up) {
        tftp_test(&eth1_bootp_data);
    }
#endif
    TNR_PRINT_ACTIVITY();
    cyg_test_exit();
}

void
cyg_start(void)
{
    // Create a main thread, so we can run the scheduler and have time 'pass'
    cyg_thread_create(10,                // Priority - just a number
                      net_test,          // entry
                      0,                 // entry parameter
                      "Network test",    // Name
                      &stack[0],         // Stack
                      STACK_SIZE,        // Size
                      &thread_handle,    // Handle
                      &thread_data       // Thread data structure
            );
    cyg_thread_resume(thread_handle);  // Start it
    cyg_scheduler_start();
}

// EOF tftp_server_test.c
