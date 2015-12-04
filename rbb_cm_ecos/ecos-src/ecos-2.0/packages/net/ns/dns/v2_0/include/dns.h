#ifndef CYGONCE_NS_DNS_DNS_H
#define CYGONCE_NS_DNS_DNS_H
//=============================================================================
//
//      dns.h
//
//      DNS client code.
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
// Author(s):   andrew.lunn
// Contributors:andrew.lunn, jskov
// Date:        2001-09-18
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <network.h>
#include <netinet/in.h>

#ifndef _POSIX_SOURCE
/* Initialise the DNS client with the address of the server. The
   address should be a IPv4 or IPv6 numeric address */
externC int cyg_dns_res_start(char * server);

/* Old interface which  is deprecated */
externC int cyg_dns_res_init(struct in_addr *dns_server);
externC int getdomainname(char *name, size_t len);
externC int setdomainname(const char *name, size_t len);
#endif

// Host name / IP mapping
struct hostent {
    char    *h_name;        /* official name of host */
    char    **h_aliases;    /* alias list */
    int     h_addrtype;     /* host address type */
    int     h_length;       /* length of address */
    char    **h_addr_list;  /* list of addresses */
};
#define h_addr  h_addr_list[0]  /* for backward compatibility */

// begin cliffmod
#define H_ADDR_LIST_MAX_ELEMENTS 9
// end cliffmod

externC struct hostent *gethostbyname(const char *host);
externC struct hostent *gethostbyaddr(const char *addr, int len, int type);

// Error reporting
externC int h_errno;
externC const char* hstrerror(int err);

#define DNS_SUCCESS  0
#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4

// Interface between the DNS client and getaddrinfo
externC int 
cyg_dns_getaddrinfo(const char * hostname, 
                    struct sockaddr addrs[], int num,
                    int family, char **canon);
// Interface between the DNS client and getnameinfo
externC int
cyg_dns_getnameinfo(const struct sockaddr * sa, char * host, size_t hostlen);

//-----------------------------------------------------------------------------
#endif // CYGONCE_NS_DNS_DNS_H
// End of dns.h


