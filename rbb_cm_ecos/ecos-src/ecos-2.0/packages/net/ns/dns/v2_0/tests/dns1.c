//==========================================================================
//
//      tests/dns1.c
//
//      Simple test of DNS client support
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
// Author(s):    andrew.lunn
// Contributors: andrew.lunn, jskov
// Date:         2001-09-18
// Purpose:      
// Description:  Test DNS functions. Note that the _XXX defines below
//               control what addresses the test uses. These must be
//               changed to match the particular testing network in which
//               the test is to be run.
//              
//####DESCRIPTIONEND####
//
//==========================================================================

#include <network.h>
#include <netdb.h>

#include <arpa/inet.h>

#include <cyg/infra/testcase.h>

#define STACK_SIZE (CYGNUM_HAL_STACK_SIZE_TYPICAL + 0x1000)
static char stack[STACK_SIZE];
static cyg_thread thread_data;
static cyg_handle_t thread_handle;

#define __string(_x) #_x
#define __xstring(_x) __string(_x)

// Change the following if you aren't using BOOTP! It's almost certainly not right for you.
#define _DNS_IP            __xstring(172.16.1.254) // test farm host addr
#define _LOOKUP_FQDN       __xstring(b.root-servers.net.) // should stay the same?
#define _LOOKUP_DOMAINNAME __xstring(root-servers.net.)
#define _LOOKUP_HOSTNAME   __xstring(b)
#define _LOOKUP_IP         __xstring(128.9.0.107) // must be same as _LOOKUP_FQDN
#define _LOOKUP_IP_BAD     __xstring(10.0.0.0)

void
dns_test(cyg_addrword_t p)
{
    struct in_addr addr;
    struct hostent *hent;
    char dn[256];
    int i;

    CYG_TEST_INIT();

    init_all_network_interfaces();

    CYG_TEST_INFO("Starting dns1 test");

    setdomainname(NULL,0);
    
    for (i=0; i<2; i++) {
        /* Expect _LOOKUP_IP as the answer. This is a CNAME lookup */
        inet_aton(_LOOKUP_IP, &addr);
        hent = gethostbyname(_LOOKUP_FQDN);
        if (hent != NULL) {
            diag_printf("PASS:<%s is %s>\n", hent->h_name, inet_ntoa(*(struct in_addr *)hent->h_addr));
            if (0 != memcmp((void*)&addr, (void*)(hent->h_addr), sizeof(struct in_addr))) {
                diag_printf("FAIL:<expected " _LOOKUP_FQDN " to be " _LOOKUP_IP ">\n");
            }
            break;
        } else {
            diag_printf("FAIL:<Asked for " _LOOKUP_FQDN ". No answer: %s>\n", hstrerror(h_errno));
            CYG_TEST_INFO("Retrying with explicit DNS server");
            CYG_TEST_INFO("Connecting to DNS at " _DNS_IP);
            inet_aton(_DNS_IP, &addr);
            CYG_TEST_CHECK(cyg_dns_res_init(&addr) == 0, "Failed to initialize resolver");
        }
    }

    /* Reverse lookup the _LOOKUP_IP addres, expect _LOOKUP_FQDN
       as the answer. */
    hent = gethostbyaddr((char *)&addr, sizeof(struct in_addr), AF_INET);
    if (hent != NULL) {
        diag_printf("PASS:<%s is %s>\n", hent->h_name, inet_ntoa(*(struct in_addr *)hent->h_addr));
        if (0 != strcmp(_LOOKUP_FQDN, hent->h_name)) {
          diag_printf("FAIL:<expected " _LOOKUP_IP " to be " _LOOKUP_FQDN ">\n");
        }
    } else {
        diag_printf("FAIL:<Asked for " _LOOKUP_IP ". No answer: %s>\n", hstrerror(h_errno));
    }
 
    /* This does not require a DNS lookup. Just turn the value into
       binary */
    hent = gethostbyname(_LOOKUP_IP);
    if (hent != NULL) {
        diag_printf("PASS:<%s is %s>\n", hent->h_name, inet_ntoa(*(struct in_addr *)hent->h_addr));
    } else {
        diag_printf("FAIL:<Asked for " _LOOKUP_IP ". No answer: %s>\n", hstrerror(h_errno));
    }
 
    /* Reverse lookup an address this is not in the server. Expect a
       NULL back */
    inet_aton(_LOOKUP_IP_BAD, &addr);
    hent = gethostbyaddr((char *)&addr, sizeof(struct in_addr), AF_INET);
    if (hent != NULL) {
        diag_printf("FAIL:<%s is %s>\n", hent->h_name, inet_ntoa(*(struct in_addr *)hent->h_addr));
    } else {
        diag_printf("PASS:<Asked for bad IP " _LOOKUP_IP_BAD ". No answer: %s>\n", hstrerror(h_errno));
    }

    /* Setup a domainname. We now don't have to use fully qualified
       domain names */
    setdomainname(_LOOKUP_DOMAINNAME, sizeof(_LOOKUP_DOMAINNAME));
    getdomainname(dn, sizeof(dn));
    diag_printf("INFO:<Domainname is now %s>\n", dn);

    /* Make sure FQDN still work */
    hent = gethostbyname(_LOOKUP_FQDN);
    if (hent != NULL) {
        diag_printf("PASS:<%s is %s>\n", hent->h_name, inet_ntoa(*(struct in_addr *)hent->h_addr));
    } else {
        diag_printf("FAIL:<Asked for " _LOOKUP_FQDN ". No answer: %s>\n", hstrerror(h_errno));
    }

    /* Now just the hostname */
    hent = gethostbyname(_LOOKUP_HOSTNAME);
    if (hent != NULL) {
        diag_printf("PASS:<%s is %s>\n", _LOOKUP_HOSTNAME, inet_ntoa(*(struct in_addr *)hent->h_addr));
    } else {
        diag_printf("FAIL:<Asked for " _LOOKUP_HOSTNAME ". No answer: %s>\n", hstrerror(h_errno));
    }

    CYG_TEST_FINISH("dns1 test completed");
}

void
cyg_start(void)
{
    // Create a main thread, so we can run the scheduler and have time 'pass'
    cyg_thread_create(10,                // Priority - just a number
                      dns_test,          // entry
                      0,                 // entry parameter
                      "DNS test",        // Name
                      &stack[0],         // Stack
                      STACK_SIZE,        // Size
                      &thread_handle,    // Handle
                      &thread_data       // Thread data structure
            );
    cyg_thread_resume(thread_handle);  // Start it
    cyg_scheduler_start();
}
