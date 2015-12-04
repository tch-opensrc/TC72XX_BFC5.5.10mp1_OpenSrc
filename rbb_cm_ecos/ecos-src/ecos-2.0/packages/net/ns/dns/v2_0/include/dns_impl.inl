#ifndef CYGONCE_NS_DNS_DNS_IMPL_H
#define CYGONCE_NS_DNS_DNS_IMPL_H
//=============================================================================
//
//      dns_impl.inl
//
//      DNS client code implementation.
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
// Description: The code is kept in this separate file to allow it to be
//              used from RedBoot.
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <cyg/ns/dns/dns_priv.h>
#include <cyg/infra/diag.h>		// For diagnostic printing.
#include <netinet/if_bcm.h>     // cliffmod

/* Validate a hostname is legal as defined in RFC 1035 */
static int
valid_hostname(const char *hostname)
{
    const char * label;
    const char * label_end;
  
    if (!hostname) {
        return false;
    }
    label = hostname;
    while (*label) {
        if (!isalpha(*label))
            return false;
        label_end = strchr(label, (int)'.') - 1;
        if (label_end == (char *)-1) {
            label_end = strchr(label, (int)'\0') - 1;
        }
        while (label != label_end) {
            if (!isalnum(*label) && (*label != '-')) {
                return false;
            }
            label++;
        }
        label = label_end+1;            /* Move onto the . or the null. */
        if (*label == '.') {            /* Move over the . if there is one */
            label++;
        }
    }
    return true;
}

/* Build a qname structure. The structure consists of pairs of
   <len><label> where <label> is eg ma in tux.ma.tech.ascom.ch. len is
   the length of the label. */
static int 
build_qname(char *ptr, const char *hostname)
{
    const char *label = hostname;
    char *end_label;
    int total_len = 0;
    int len;
  
    while (*label) {
        end_label = strchr(label, (int)'.') - 1;
        if (end_label == (char *)-1) {
            end_label = strchr(label, (int)'\0') - 1;
        }
        len = end_label - label + 1;
        if (len > 63) {
            return -1;
        }
        *ptr++ = (char) len;            /* Put the length of the label */
        memcpy(ptr, label, len);        /* and now the label */
        ptr += len;
    
        total_len += len +1;
        label = end_label+1;            /* Move onto the . or the null. */
        if (*label == '.') {            /* Move over the . if there is one */
            label++;
        }
    }
    *ptr = 0;                           /* Add the last length of zero
                                           to mark the end */
    return (total_len+1);
}

/* Given a pointer to a qname, find the length of the qname. This is
   the number of bytes needed to represent the qname, not the length
   of the name. A qname is terminated by either a 00, or a pointer
   into another qname. This pointer has the top two bits set. */
static int 
qname_len(unsigned char * qname)
{
    unsigned char * ptr = qname;
    
    while ((*ptr != 0) && ((*ptr & 0xc0) != 0xc0)) {
        ptr += *ptr + 1;
    }
    /* Pointers are two bytes */
    if ((*ptr & 0xc0) == 0xc0) {
        ptr++;
    }
    ptr++;                              /* Skip over the trailing byte */

    return (ptr - qname);
}

/* Build a real name from a qname. Alloc the memory needed and return
   it. Return NULL on error */
char *
real_name(char *msg, unsigned char *qname)
{
    unsigned char * ptr = qname;
    char * name;
    char * label;
    int len = 0;
    int offset;

    /* First pass works out the length of the name */
    while (*ptr != 0) {
        if ((*ptr & 0xc0) == 0xc0) {
            /* pointer to somewhere else. Follow the pointer */
            offset = ((*ptr & 0x3f) << 8) | *(ptr+1);
            ptr = msg + offset;
        } else {
            len += *ptr + 1;
            ptr += *ptr + 1;
        }
    }

    /* Now allocate the memory needed and copy the host name into it */
    name = alloc_string(len+1);
    if (name) {
        label = name;
        ptr = qname;
        while (*ptr != 0) {
            if ((*ptr & 0xc0) == 0xc0) {
                /* pointer to somewhere else. Follow the pointer */
                offset = ((*ptr & 0x3f) << 8) | *(ptr+1);
                ptr = msg + offset;
            } else {
                len = *ptr;
                memcpy(label, (ptr+1), len);
                label += len;
                *label++ = '.';
                ptr += *ptr + 1;
            }
        }
        *label = '\0';
    }
    return name;
}

/* Build a query message which can be sent to the server. If something
   goes wrong return -1, otherwise the length of the query message */
static int 
build_query(char * msg, const char * hostname, short rr_type)
{
    struct dns_header *dns_hdr;
    char *ptr;
    int len;

    /* Fill out the header */
    dns_hdr = (struct dns_header *) msg;
    dns_hdr->id = htons(id++);
    dns_hdr->rd = true;
    dns_hdr->opcode = DNS_QUERY;
    dns_hdr->qdcount = htons(1);
  
    /* Now the question we want to ask */
    ptr = (char *)&dns_hdr[1];

    len = build_qname(ptr, hostname);

    if (len < 0) {
        h_errno = NO_RECOVERY;
        return -1;
    }
    ptr += len;

    /* Set the type and class fields */
    *ptr++ = (rr_type >> 8) & 0xff;
    *ptr++ = rr_type & 0xff;
    *ptr++ = (DNS_CLASS_IN >> 8) & 0xff;
    *ptr++ = DNS_CLASS_IN & 0xff;

    len = ptr - msg;

    return len;
}

/* Check if the hostname is actually of dot format. If so convert it 
   and return host entity structure. If not, return NULL. */
static struct hostent *
dot_hostname(const char *hostname)
{
    struct hostent *hent = NULL;
    struct in_addr addr;

    if (inet_aton(hostname, &addr)) {
        hent = alloc_hent();
        if (hent) {
            memcpy(hent->h_addr_list[0], &addr, sizeof(struct in_addr));
            hent->h_addrtype = AF_INET;
            hent->h_length = sizeof(struct in_addr);
            hent->h_name = alloc_string(strlen(hostname)+1);
            if (!hent->h_name) {
                free_hent(hent);
                return NULL;
            }
            strcpy(hent->h_name, hostname);
        }
    }
    return hent;
}

/* Decode the answer from the server. Returns NULL if failed, or 
   a hostent structure containing different data depending on the
   query type:
    DNS_TYPE_A: 
      h_name:         a pointer to the hostname
      h_addr_list[0]: a pointer to in_addr structure
    DNS_TYPE_PTR:
      h_name:         a pointer to the hostname
      h_addr_list[0]: a pointer to an empty in_addr structure. Caller
                      must fill out!
*/

static struct hostent *
parse_answer(char * msg, short rr_type)
{
    static struct hostent *hent;
    struct dns_header *dns_hdr;
    struct resource_record rr, *rr_p = NULL, *first_interesting_rr_p = NULL;  // cliffmod
    char *qname = NULL;
    char *ptr;

    // begin cliffmod
    struct in_addr temp_h_addr_list[H_ADDR_LIST_MAX_ELEMENTS]; 
    struct in_addr tmp_in_addr;
    struct in_addr* ptmp_in_addr;
    int addr_idx = 0;
    int num_interesting_list_elements = 0;
    memset( temp_h_addr_list, 0, (H_ADDR_LIST_MAX_ELEMENTS * sizeof(struct in_addr)) );
    // end cliffmod

    dns_hdr = (struct dns_header *)msg;

    if (DNS_REPLY_NAME_ERROR == dns_hdr->rcode) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    if ((dns_hdr->qr != 1) || 
        (dns_hdr->opcode != DNS_QUERY) ||
        (dns_hdr->rcode != DNS_REPLY_NOERR)) {
        h_errno = NO_RECOVERY;
        return NULL;
    }
  
    dns_hdr->ancount = ntohs(dns_hdr->ancount);
    dns_hdr->qdcount = ntohs(dns_hdr->qdcount);
    ptr = (char *)&dns_hdr[1];

    /* Skip over the query section */
    if (dns_hdr->qdcount > 0) {
        while (dns_hdr->qdcount) {
            ptr += qname_len(ptr);
            ptr += 4;                   /* skip type & class */
            dns_hdr->qdcount--;
        }
    }  
    /* Skip over the answers resource records to find an answer of the
       correct type. */
    while (dns_hdr->ancount) {
        qname = ptr;
        ptr += qname_len(ptr);
        rr_p = (struct resource_record *)ptr;
        memcpy(&rr, ptr, sizeof(rr));
        if( (rr.rr_type == htons(rr_type)) && (rr.class == htons(DNS_CLASS_IN)) ) 
        {
// begin cliffmod

            //break;
            //
            if( first_interesting_rr_p == NULL )
            {
                // only assign when first interesting resource record is found.
                // this is for the old school DNS_TYPE_PTR case...
                first_interesting_rr_p = rr_p;
            }

            if( addr_idx < (H_ADDR_LIST_MAX_ELEMENTS-2) )
            {
                // not at last writable array entry. 
                // --> copy in ip addr data.
                #if DMP_STACK_DEBUG
                diag_printf("parse_answer() --> copy rr_p->rdata to temp array item\n");
                #endif

                memcpy((void*)(&(tmp_in_addr.s_addr)), rr_p->rdata, sizeof(struct in_addr));
                memcpy( (void*)(&temp_h_addr_list[ addr_idx ].s_addr), rr_p->rdata, sizeof(struct in_addr) );

                #if DMP_STACK_DEBUG
                diag_printf("parse_answer() --> rr_p->rdata=%s    temp_h_addr_list[ %x ]=%s\n\n", 
                    inet_ntoa( tmp_in_addr ), 
                    addr_idx, inet_ntoa( temp_h_addr_list[ addr_idx ] ) );
                #endif
                addr_idx++;

            }        
            else
            {
                // else at (or beyond???) end of array.  break out of loop.
                #if DMP_STACK_DEBUG
                diag_printf("parse_answer() --> at end of array. break out of loop\n");
                #endif
                break;
            }
            // end cliffmod
        }
        ptr += sizeof(struct resource_record) - sizeof(rr.rdata) + ntohs(rr.rdlength);
        dns_hdr->ancount--;
    }

    /* If we found one. decode it */
//    if (dns_hdr->ancount > 0) {
    num_interesting_list_elements = addr_idx;
    if( num_interesting_list_elements > H_ADDR_LIST_MAX_ELEMENTS )
    {
        num_interesting_list_elements = H_ADDR_LIST_MAX_ELEMENTS - 1;
    }

    if( num_interesting_list_elements )
    {
        // found at least one interesting answer
        switch (rr_type) 
        {
        case DNS_TYPE_A:
            {
                hent = alloc_hent_n( num_interesting_list_elements + 1 );
                if( !hent ) 
                {
                    #if DMP_STACK_DEBUG
                    diag_printf("parse_answer() --> alloc_hent_n() returned NULL!\n");
                    #endif
                    return NULL;
                }

                hent->h_name = real_name(msg, qname);
                if (!hent->h_name) 
                {
                    free_hent(hent);
                    return NULL;
                }
//                memcpy(hent->h_addr_list[0], rr_p->rdata, sizeof(struct in_addr));
                // copy our temp_h_addr_list to the host entry addr list.
                for( addr_idx = 0; addr_idx < num_interesting_list_elements; addr_idx++ )
                {
                    #if DMP_STACK_DEBUG
                    diag_printf("parse_answer() --> prior to copying from temp_h_addr_list to h_addr_list\n");
                
                    diag_printf("parse_answer() --> temp_h_addr_list[ %x ].s_addr=0x%x == %s\n", 
                        addr_idx, temp_h_addr_list[ addr_idx ].s_addr, 
                        inet_ntoa( temp_h_addr_list[ addr_idx ] ) );
                    diag_printf("parse_answer() --> hent->h_addr_list[ %x ]=%p\n", 
                        addr_idx, hent->h_addr_list[ addr_idx ] );
                    #endif
//                    diag_printf("parse_answer() --> temp_h_addr_list[ addr_row_idx ]=%s\n",
//                        inet_ntoa( (struct in_addr)(temp_h_addr_list[ addr_row_idx ]) ) );
                
                    tmp_in_addr.s_addr = temp_h_addr_list[ addr_idx ].s_addr;
                    ptmp_in_addr = (struct in_addr*)(hent->h_addr_list[ addr_idx ]);
                    ptmp_in_addr->s_addr = tmp_in_addr.s_addr;
//                    memcpy( hent->h_addr_list[ addr_idx ], 
//                        (void*)(&temp_h_addr_list[ addr_idx ].s_addr), sizeof(struct in_addr) );
                
                    #if DMP_STACK_DEBUG
                    diag_printf("parse_answer() --> after copying from temp_h_addr_list to h_addr_list\n");
                
                    diag_printf("parse_answer() --> tmp_in_addr.s_addr=0x%x == %s\n", 
                        tmp_in_addr.s_addr, inet_ntoa( tmp_in_addr ) );
                
                    diag_printf("parse_answer() --> ptmp_in_addr=0x%x   ptmp_in_addr->s_addr=0x%x == %s\n", 
                        ptmp_in_addr, ptmp_in_addr->s_addr, inet_ntoa( *ptmp_in_addr ) );
                
                    diag_printf("parse_answer() --> hent->h_addr_list[ %x ]=%p == %s\n\n", 
                        addr_idx, hent->h_addr_list[ addr_idx ],
                        inet_ntoa(*(struct in_addr *)hent->h_addr_list[ addr_idx ]) );
                    #endif
                }
                hent->h_addrtype = AF_INET;
                hent->h_length = sizeof(struct in_addr);
            }
            return hent;

        case DNS_TYPE_PTR:
            {
                hent = alloc_hent();
                if( !hent ) 
                {
                    #if DMP_STACK_DEBUG
                    diag_printf("parse_answer() --> alloc_hent() returned NULL!\n");
                    #endif
                    return NULL;
                }

//                hent->h_name = real_name(msg, rr_p->rdata);
                hent->h_name = real_name(msg, first_interesting_rr_p->rdata);
                if (!hent->h_name) 
                {
                    free_hent(hent);
                    return NULL;
                }
                hent->h_addrtype = AF_INET;
                hent->h_length = sizeof(struct in_addr);
            }
            return hent;

//        default:
//            free_hent(hent);
        } // end switch (rr_type) 
    } // end if( addr_idx )
// end cliffmod
    h_errno = NO_DATA;
    return NULL;
}

/* Given an address, find out the hostname. */
struct hostent *
gethostbyaddr(const char *addr, int len, int type) __attribute__ ((weak));
struct hostent *
gethostbyaddr(const char *addr, int len, int type)
{
    unsigned char msg[MAXDNSMSGSIZE];
    char hostname[40];
    struct hostent * hent;

    CYG_REPORT_FUNCNAMETYPE( "gethostbyaddr", "returning %08x" );
    CYG_REPORT_FUNCARG3( "addr=%08x, len=%d, type=%d", addr, len, type );

    if ( !addr || 0 == len) {
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }

    CYG_CHECK_DATA_PTR( addr, "addr is not a valid pointer!" );

    /* Has the socket to the DNS server been opened? */
    if (s < 0) {
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }

    /* See if there is an answer to an old query. If so free the memory
       it uses. */
    free_stored_hent();

    /* Only IPv4 addresses accepted */
    if ((type != AF_INET) || (len != sizeof(struct in_addr))) {
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }

    cyg_drv_mutex_lock(&dns_mutex);

    /* Build the 'hostname' we want to lookup. */
    sprintf(hostname, "%d.%d.%d.%d.IN-ADDR.ARPA.",
            (unsigned char)addr[3],
            (unsigned char)addr[2],
            (unsigned char)addr[1],
            (unsigned char)addr[0]);
  
    memset(msg, 0, sizeof(msg));
  
    /* Build a PTR type request using the hostname */
    len = build_query(msg, hostname, DNS_TYPE_PTR);
    if (len < 0) {
        cyg_drv_mutex_unlock(&dns_mutex);
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }

    /* Send the request and wait for an answer */
    len = send_recv(msg, len, sizeof(msg));
    if (len < 0) {
        cyg_drv_mutex_unlock(&dns_mutex);
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }

    /* Fill in the missing address */
    hent = parse_answer(msg, DNS_TYPE_PTR);
    if (hent) {
        memcpy(hent->h_addr_list[0], addr, sizeof(struct in_addr));
        store_hent(hent);
    }
    cyg_drv_mutex_unlock(&dns_mutex);

    CYG_REPORT_RETVAL( hent );
    return hent;
}

/* Given a hostname find out the IP address */
struct hostent *
gethostbyname(const char * hostname) __attribute__ ((weak));
struct hostent *
gethostbyname(const char * hostname)
{
    unsigned char msg[MAXDNSMSGSIZE];
    char name[256];
    struct hostent *hent;
    int len;

    CYG_REPORT_FUNCNAMETYPE( "gethostbyname", "returning %08x" );
    CYG_REPORT_FUNCARG1( "hostname=%08x", hostname );

    #if DMP_STACK_DEBUG
    diag_printf("gethostbyname() --> entry\n");
    #endif
    if ( !hostname ) {
        CYG_REPORT_RETVAL( NULL );
        diag_printf("gethostbyname() --> error - null target\n");
        return NULL;
    }

    CYG_CHECK_DATA_PTR( hostname, "hostname is not a valid pointer!" );

    /* Has the socket to the DNS server been opened? */
    if (s < 0) {
        CYG_REPORT_RETVAL( NULL );
        diag_printf("gethostbyname() --> error - socket to DNS server not open.\n");
        return NULL;
    }

    /* See if there is an answer to an old query. If so free the memory
       it uses */
    free_stored_hent();
  
    if (!valid_hostname(hostname)) 
    {
        diag_printf("gethostbyname() --> invalid hostname.\n");

        /* It could be a dot address */
        hent = dot_hostname(hostname);
        store_hent(hent);
        CYG_REPORT_RETVAL( hent );
        return hent;
    }

    cyg_drv_mutex_lock(&dns_mutex);

    /* First try the name as passed in */
    #if DMP_STACK_DEBUG
    diag_printf("gethostbyname() --> calling build_query().\n");
    #endif
    memset(msg, 0, sizeof(msg));
    len = build_query(msg, hostname, DNS_TYPE_A);
    if (len < 0) {
        cyg_drv_mutex_unlock(&dns_mutex);
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }
  
    /* Send the query and wait for an answer */
    #if DMP_STACK_DEBUG
    diag_printf("gethostbyname() --> calling send_recv().\n");
    #endif
    len = send_recv(msg, len, sizeof(msg));
    if (len < 0) {
        cyg_drv_mutex_unlock(&dns_mutex);
        CYG_REPORT_RETVAL( NULL );
        return NULL;
    }
  
    /* Decode the answer */
    #if DMP_STACK_DEBUG
    diag_printf("gethostbyname() --> calling parse_answer().\n");
    #endif
    hent = parse_answer(msg, DNS_TYPE_A);
    if (hent) {
        cyg_drv_mutex_unlock(&dns_mutex); 
        store_hent(hent);
        CYG_REPORT_RETVAL( hent );
        return hent;
    }

    /* If no match, try appending the domainname if we have one */
    if (domainname) 
    {
        #if DMP_STACK_DEBUG
        diag_printf("gethostbyname() --> append domainname and retry.\n");
        #endif

        if ((strlen(hostname) + strlen(domainname)) > 254) {
            h_errno = NO_RECOVERY;
            cyg_drv_mutex_unlock(&dns_mutex);
            CYG_REPORT_RETVAL( NULL );
            return NULL;
        }
        strcpy(name, hostname);
        strcat(name, ".");
        strcat(name, domainname);
    
        memset(msg, 0, sizeof(msg));
        len = build_query(msg, name, DNS_TYPE_A);
        if (len < 0) {
            cyg_drv_mutex_unlock(&dns_mutex);
            CYG_REPORT_RETVAL( NULL );
            return NULL;
        }
    
        /* Send the query and wait for an answer */
        len = send_recv(msg, len, sizeof(msg));
        if (len < 0) {
            cyg_drv_mutex_unlock(&dns_mutex);
            CYG_REPORT_RETVAL( NULL );
            return NULL;
        }
    
        /* Decode the answer */
        hent = parse_answer(msg, DNS_TYPE_A);
    }

    cyg_drv_mutex_unlock(&dns_mutex); 
    store_hent(hent);
    CYG_REPORT_RETVAL( hent );
    return hent;
}

/* Set the domain names, as used by the DNS server */
int
setdomainname(const char *name, size_t len) __attribute__ ((weak));
int
setdomainname(const char *name, size_t len)
{
    char * ptr;

    CYG_REPORT_FUNCNAMETYPE( "setdomainname", "returning %d" );
    CYG_REPORT_FUNCARG2( "name=%08x, len=%d", name, len );

    if ((len < 0) || (len > 255)) {
        h_errno = NO_RECOVERY;
        CYG_REPORT_RETVAL( -1 );
        return -1;
    }
    if (len != 0) {
        CYG_CHECK_DATA_PTR( name, "name is not a valid pointer!" );
        ptr = alloc_string(len+1);
        if (!ptr) {         
            CYG_REPORT_RETVAL( -1 );
            return -1;
        } 
        memcpy(ptr, name, len);
        ptr[len]=0;
    } else {
        ptr = NULL;
    }
  
    if (domainname) {
        free_string(domainname);
    }
    domainname = ptr;
    CYG_REPORT_RETVAL( 0 );
    return 0;
}

/* Return the domain name as used by the DNS server */
int
getdomainname(char *name, size_t len) __attribute__ ((weak));
int
getdomainname(char *name, size_t len)
{
    CYG_REPORT_FUNCNAMETYPE( "getdomainname", "returning %d" );
    CYG_REPORT_FUNCARG2( "name=%08x, len=%d", name, len );

    if ( !name || 0 == len) {
        CYG_REPORT_RETVAL( -1 );
        return -1;
    }

    CYG_CHECK_DATA_PTR( name, "name is not a valid pointer!" );

    /* No domainname set, return a 0 */
    if (!domainname) {
        if (len == 0) {
            h_errno = HOST_NOT_FOUND;
            CYG_REPORT_RETVAL( -1 );
            return -1;
        }
        name[0]='\0';
    } else {
        if ((strlen(domainname) + 1) > len) {
            h_errno = NO_RECOVERY;
            CYG_REPORT_RETVAL( -1 );
            return -1;
        }
        strncpy(name, domainname, len);
    }
    CYG_REPORT_RETVAL( 0 );
    return 0;
}

int h_errno = DNS_SUCCESS;

const char*
hstrerror(int err) __attribute__ ((weak));
const char*
hstrerror(int err)
{
    CYG_REPORT_FUNCNAMETYPE( "hstrerror", "returning %08x" );
    CYG_REPORT_FUNCARG1( "err=%d", err );

    switch (err) {
    case DNS_SUCCESS:
        return "No error";
    case HOST_NOT_FOUND:
        return "No such host is known";
    case TRY_AGAIN:
        return "Timeout";
    case NO_RECOVERY:
        return "Server failure or invalid input";
    case NO_DATA:
        return "No data for type";
    default:
        return "Uknown error";
    }
}

//-----------------------------------------------------------------------------
#endif // CYGONCE_NS_DNS_DNS_IMPL_H
// End of dns_impl.h
