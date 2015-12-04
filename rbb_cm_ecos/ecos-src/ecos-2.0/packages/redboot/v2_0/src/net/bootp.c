//==========================================================================
//
//      net/bootp.c
//
//      Stand-alone minimal BOOTP support for RedBoot
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
// Copyright (C) 2002 Gary Thomas
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
// Author(s):    gthomas
// Contributors: gthomas
// Date:         2000-07-14
// Purpose:      
// Description:  
//              
// This code is part of RedBoot (tm).
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <redboot.h>
#include <net/net.h>
#include <net/bootp.h>

extern int net_debug;

#define SHOULD_BE_RANDOM  0x12345555

/* How many milliseconds to wait before retrying the request */
#define RETRY_TIME  1000
#define MAX_RETRIES   30

static bootp_header_t *bp_info;
  
#ifdef CYGSEM_REDBOOT_NETWORKING_USE_GATEWAY
static const unsigned char dhcpCookie[] = {99,130,83,99};
static const unsigned char dhcpEndOption[] = {255};
static const unsigned char dhcpRequestOption[] = {52,1,3};
#endif

static void
bootp_handler(udp_socket_t *skt, char *buf, int len,
	      ip_route_t *src_route, word src_port)
{
    bootp_header_t *b;
#ifdef CYGSEM_REDBOOT_NETWORKING_USE_GATEWAY
    unsigned char *p,*end;
    int optlen;
#endif

    b = (bootp_header_t *)buf;
    if (bp_info) {
        memset(bp_info,0,sizeof *bp_info);
        if (len > sizeof *bp_info)
            len = sizeof *bp_info;
        memcpy(bp_info, b, len);
    }

    // Only accept pure REPLY responses
    if (b->bp_op != BOOTREPLY)
      return;
    
    // Must be sent to me, as well!
    if (memcmp(b->bp_chaddr, __local_enet_addr, 6))
      return;
        
    memcpy(__local_ip_addr, &b->bp_yiaddr, 4);
#ifdef CYGSEM_REDBOOT_NETWORKING_USE_GATEWAY
    memcpy(__local_ip_gate, &b->bp_giaddr, 4);
    
    if (memcmp(b->bp_vend, dhcpCookie, sizeof(dhcpCookie)))
      return;
                
    optlen = len - (b->bp_vend - ((unsigned char*)b));
    
    p = b->bp_vend+4;
    end = ((unsigned char*)b) + len;
    
    while (p < end) {
        unsigned char tag = *p;
        if (tag == TAG_END)
          break;
        if (tag == TAG_PAD)
          optlen = 1;
        else {
            optlen = p[1];
            p += 2;
            switch (tag) {
             case TAG_SUBNET_MASK:  // subnet mask
                memcpy(__local_ip_mask,p,4); 
                break;
             case TAG_GATEWAY:  // router
                memcpy(__local_ip_gate,p,4); 
                break;
             default:
                break;
            }
        }
        p += optlen;
    }
#endif
}

#define AddOption(p,d) do {memcpy(p,d,sizeof d); p += sizeof d;} while (0)

/*
 * Find our IP address and copy to __local_ip_addr.
 * Return zero if successful, -1 if not.
 */
int
__bootp_find_local_ip(bootp_header_t *info)
{
    udp_socket_t udp_skt;
    bootp_header_t b;
    ip_route_t     r;
    int            retry;
    unsigned long  start;
    ip_addr_t saved_ip_addr;
#ifdef CYGSEM_REDBOOT_NETWORKING_USE_GATEWAY
    unsigned char *p;
#endif
    int txSize;

    bp_info = info;

    memset(&b, 0, sizeof(b));

    b.bp_op = BOOTREQUEST;
    b.bp_htype = HTYPE_ETHERNET;
    b.bp_hlen = 6;
    b.bp_xid = SHOULD_BE_RANDOM;
         
#ifdef CYGSEM_REDBOOT_NETWORKING_USE_GATEWAY
    p = b.bp_vend;
     
    AddOption(p,dhcpCookie);
    AddOption(p,dhcpRequestOption);
    AddOption(p,dhcpEndOption);

    // Some servers insist on a minimum amount of "vendor" data
    if (p < &b.bp_vend[BP_MIN_VEND_SIZE]) p = &b.bp_vend[BP_MIN_VEND_SIZE];
    txSize = p - (unsigned char*)&b;
#else
    txSize = sizeof(b);
#endif

    memcpy( saved_ip_addr, __local_ip_addr, sizeof(__local_ip_addr) );
    memset( __local_ip_addr, 0, sizeof(__local_ip_addr) );

    memcpy(b.bp_chaddr, __local_enet_addr, 6);

    /* fill out route for a broadcast */
    r.ip_addr[0] = 255;
    r.ip_addr[1] = 255;
    r.ip_addr[2] = 255;
    r.ip_addr[3] = 255;
    r.enet_addr[0] = 255;
    r.enet_addr[1] = 255;
    r.enet_addr[2] = 255;
    r.enet_addr[3] = 255;
    r.enet_addr[4] = 255;
    r.enet_addr[5] = 255;

    /* setup a socket listener for bootp replies */
    __udp_install_listener(&udp_skt, IPPORT_BOOTPC, bootp_handler);

    retry = MAX_RETRIES;
    while (retry-- > 0) {
	start = MS_TICKS();

	__udp_send((char *)&b, txSize, &r, IPPORT_BOOTPS, IPPORT_BOOTPC);

	do {
	    __enet_poll();
	    if (__local_ip_addr[0] || __local_ip_addr[1] ||
		__local_ip_addr[2] || __local_ip_addr[3]) {
		/* success */
		__udp_remove_listener(IPPORT_BOOTPC);
		return 0;
	    }
	} while ((MS_TICKS_DELAY() - start) < RETRY_TIME);
    }

    /* timed out */
    __udp_remove_listener(IPPORT_BOOTPC);
    net_debug = 0;
    memcpy( __local_ip_addr, saved_ip_addr, sizeof(__local_ip_addr));
    return -1;
}


