//==========================================================================
//
//      include/netinet/in.h
//
//      
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
// Date:         2000-01-10
// Purpose:      
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================


/*	$OpenBSD: in.h,v 1.27 1999/12/16 21:30:34 deraadt Exp $	*/
/*	$NetBSD: in.h,v 1.20 1996/02/13 23:41:47 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in.h	8.3 (Berkeley) 1/3/94
 */

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, and numerous additions.
 */

#ifndef _NETINET_IN_H_
#define	_NETINET_IN_H_

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define IPPROTO_HOPOPTS		IPPROTO_IP	/* Hop-by-hop option header. */
#define	IPPROTO_ICMP		1		/* control message protocol */
#define	IPPROTO_IGMP		2		/* group mgmt protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define	IPPROTO_IPIP		4		/* IP inside IP */
#define	IPPROTO_IPV4		IPPROTO_IPIP	/* IP inside IP */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define	IPPROTO_PUP		12		/* pup */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define	IPPROTO_IDP		22		/* xns idp */
#define	IPPROTO_TP		29 		/* tp-4 w/ class negotiation */
#define IPPROTO_IPV6		41		/* IPv6 in IPv6 */
#define IPPROTO_ROUTING		43		/* Routing header. */
#define IPPROTO_FRAGMENT	44		/* Fragmentation/reassembly header. */
#define IPPROTO_RSVP		46		/* resource reservation */
#define	IPPROTO_ESP		50		/* Encap. Security Payload */
#define	IPPROTO_AH		51		/* Authentication header */
#define IPPROTO_ICMPV6		58		/* ICMP for IPv6 */
#define IPPROTO_NONE		59		/* No next header */
#define IPPROTO_DSTOPTS		60		/* Destination options header. */
#define	IPPROTO_EON		80		/* ISO cnlp */
#define IPPROTO_ETHERIP		97		/* Ethernet in IPv4 */
#define	IPPROTO_ENCAP		98		/* encapsulation header */
#define IPPROTO_PIM		103		/* Protocol indep. multicast */
#define IPPROTO_IPCOMP		108		/* IP Payload Comp. Protocol */
#define	IPPROTO_RAW		255		/* raw IP packet */

#define	IPPROTO_MAX		256

/*
 * From FreeBSD:
 *
 * Local port number conventions:
 *
 * When a user does a bind(2) or connect(2) with a port number of zero,
 * a non-conflicting local port address is chosen.
 * The default range is IPPORT_RESERVED through
 * IPPORT_USERRESERVED, although that is settable by sysctl.
 *
 * A user may set the IPPROTO_IP option IP_PORTRANGE to change this
 * default assignment range.
 *
 * The value IP_PORTRANGE_DEFAULT causes the default behavior.
 *
 * The value IP_PORTRANGE_HIGH changes the range of candidate port numbers
 * into the "high" range.  These are reserved for client outbound connections
 * which do not want to be filtered by any firewalls.
 *
 * The value IP_PORTRANGE_LOW changes the range to the "low" are
 * that is (by convention) restricted to privileged processes.  This
 * convention is based on "vouchsafe" principles only.  It is only secure
 * if you trust the remote host to restrict these ports.
 *
 * The default range of ports and the high range can be changed by
 * sysctl(3).  (net.inet.ip.port{hi}{first,last})
 *
 * Changing those values has bad security implications if you are
 * using a a stateless firewall that is allowing packets outside of that
 * range in order to allow transparent outgoing connections.
 *
 * Such a firewall configuration will generally depend on the use of these
 * default values.  If you change them, you may find your Security
 * Administrator looking for you with a heavy object.
 */

/*
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */
#define	IPPORT_RESERVED		1024
#define	IPPORT_USERRESERVED	49151

/*
 * Default local port range to use by setting IP_PORTRANGE_HIGH
 */
#define IPPORT_HIFIRSTAUTO	49152
#define IPPORT_HILASTAUTO	65535

/*
 * IP Version 4 Internet address (a structure for historical reasons)
 */
struct in_addr {
	in_addr_t s_addr;
};

#if 0	/*NRL IPv6*/
/*
 * IP Version 6 Internet address
 */
struct in6_addr {
	union {
		u_int8_t s6u_addr8[16];
		u_int16_t s6u_addr16[8];
		u_int32_t s6u_addr32[4];
	} s6_u;
#define s6_addr s6_u.s6u_addr8
/*
 * The rest are common, but not guaranteed to be portable. 64 bit access are
 * not available because the in6_addr in a sockaddr_in6 is not 64 bit aligned.
 */
#define s6_addr8 s6_u.s6u_addr8
#define s6_addr16 s6_u.s6u_addr16
#define s6_addr32 s6_u.s6u_addr32
};
#endif

/* last return value of *_input(), meaning "all job for this pkt is done".  */
#define	IPPROTO_DONE		257

/*
 * Definitions of bits in internet address integers.
 * On subnets, the decomposition of addresses to host and net parts
 * is done according to subnet mask, not the masks here.
 *
 * By byte-swapping the constants, we avoid ever having to byte-swap IP
 * addresses inside the kernel.  Unfortunately, user-level programs rely
 * on these macros not doing byte-swapping.
 */
#ifdef _KERNEL
#define	__IPADDR(x)	((u_int32_t) htonl((u_int32_t)(x)))
#else
#define	__IPADDR(x)	((u_int32_t)(x))
#endif

#define	IN_CLASSA(i)		(((u_int32_t)(i) & __IPADDR(0x80000000)) == \
				 __IPADDR(0x00000000))
#define	IN_CLASSA_NET		__IPADDR(0xff000000)
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		__IPADDR(0x00ffffff)
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(i)		(((u_int32_t)(i) & __IPADDR(0xc0000000)) == \
				 __IPADDR(0x80000000))
#define	IN_CLASSB_NET		__IPADDR(0xffff0000)
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		__IPADDR(0x0000ffff)
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(i)		(((u_int32_t)(i) & __IPADDR(0xe0000000)) == \
				 __IPADDR(0xc0000000))
#define	IN_CLASSC_NET		__IPADDR(0xffffff00)
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		__IPADDR(0x000000ff)

#define	IN_CLASSD(i)		(((u_int32_t)(i) & __IPADDR(0xf0000000)) == \
				 __IPADDR(0xe0000000))
/* These ones aren't really net and host fields, but routing needn't know. */
#define	IN_CLASSD_NET		__IPADDR(0xf0000000)
#define	IN_CLASSD_NSHIFT	28
#define	IN_CLASSD_HOST		__IPADDR(0x0fffffff)
#define	IN_MULTICAST(i)		IN_CLASSD(i)

#define	IN_EXPERIMENTAL(i)	(((u_int32_t)(i) & __IPADDR(0xf0000000)) == \
				 __IPADDR(0xf0000000))
#define	IN_BADCLASS(i)		(((u_int32_t)(i) & __IPADDR(0xf0000000)) == \
				 __IPADDR(0xf0000000))

#define	IN_LOCAL_GROUP(i)	(((u_int32_t)(i) & __IPADDR(0xffffff00)) == \
				 __IPADDR(0xe0000000))

#define	INADDR_ANY		__IPADDR(0x00000000)
#define	INADDR_LOOPBACK		__IPADDR(0x7f000001)
#define	INADDR_BROADCAST	__IPADDR(0xffffffff)	/* must be masked */
#ifndef _KERNEL
#define	INADDR_NONE		__IPADDR(0xffffffff)	/* -1 return */
#endif

#define	INADDR_UNSPEC_GROUP	__IPADDR(0xe0000000)	/* 224.0.0.0 */
#define	INADDR_ALLHOSTS_GROUP	__IPADDR(0xe0000001)	/* 224.0.0.1 */
#define INADDR_MAX_LOCAL_GROUP	__IPADDR(0xe00000ff)	/* 224.0.0.255 */

#define	IN_LOOPBACKNET		127			/* official! */

#if 0	/*NRL IPv6*/
/*
 * Tests for IPv6 address types
 */

#define	IN6_IS_ADDR_LINKLOCAL(addr) \
	(((addr)->s6_addr32[0] & htonl(0xffc00000)) == htonl(0xfe800000))

#define	IN6_IS_ADDR_LOOPBACK(addr) \
	(((addr)->s6_addr32[0] == 0) && ((addr)->s6_addr32[1] == 0) && \
	 ((addr)->s6_addr32[2] == 0) && ((addr)->s6_addr32[3] == htonl(1)))

#define	IN6_IS_ADDR_MULTICAST(addr) \
	((addr)->s6_addr8[0] == 0xff)
	
#define	IN6_IS_ADDR_SITELOCAL(addr) \
	(((addr)->s6_addr32[0] & htonl(0xffc00000)) == htonl(0xfec00000))

#define	IN6_IS_ADDR_UNSPECIFIED(addr) \
	(((addr)->s6_addr32[0] == 0) && ((addr)->s6_addr32[1] == 0) && \
	 ((addr)->s6_addr32[2] == 0) && ((addr)->s6_addr32[3] == 0))

#define	IN6_IS_ADDR_V4COMPAT(addr) \
	(((addr)->s6_addr32[0] == 0) && ((addr)->s6_addr32[1] == 0) && \
	 ((addr)->s6_addr32[2] == 0) && ((addr)->s6_addr32[3] & ~htonl(1)))

#define	IN6_IS_ADDR_V4MAPPED(addr) \
	(((addr)->s6_addr32[0] == 0) && ((addr)->s6_addr32[1] == 0) && \
	 ((addr)->s6_addr32[2] == htonl(0xffff)))

#define	IN6_ARE_ADDR_EQUAL(addr1, addr2) \
	(((addr1)->s6_addr32[0] == (addr2)->s6_addr32[0]) && \
	 ((addr1)->s6_addr32[1] == (addr2)->s6_addr32[1]) && \
	 ((addr1)->s6_addr32[2] == (addr2)->s6_addr32[2]) && \
	 ((addr1)->s6_addr32[3] == (addr2)->s6_addr32[3]))

/*
 * IPv6 Multicast scoping.  The scope is stored
 * in the bottom 4 bits of the second byte of the
 * multicast address.
 */
		     /* 0x0 */	/* reserved */
#define	IN6_NODE_LOCAL	0x1	/* node-local scope */
#define	IN6_LINK_LOCAL	0x2	/* link-local scope */
		     /* 0x3 */	/* (unassigned) */
		     /* 0x4 */	/* (unassigned) */
#define	IN6_SITE_LOCAL	0x5	/* site-local scope */
		     /* 0x6 */	/* (unassigned) */
		     /* 0x7 */	/* (unassigned) */
#define	IN6_ORG_LOCAL	0x8	/* organization-local scope */
		     /* 0x9 */	/* (unassigned) */
		     /* 0xA */	/* (unassigned) */
		     /* 0xB */	/* (unassigned) */
		     /* 0xC */	/* (unassigned) */
		     /* 0xD */	/* (unassigned) */
#define	IN6_GLOBAL	0xE	/* global scope */
		     /* 0xF */	/* reserved */

#define	IN6_MSCOPE(addr)	((addr)->s6_addr8[1] & 0x0f)

#define	IN6_IS_ADDR_MC_NODELOCAL(addr) \
	(IN6_IS_ADDR_MULTICAST(addr) && (IN6_MSCOPE(addr) == IN6_NODE_LOCAL))
#define	IN6_IS_ADDR_MC_LINKLOCAL(addr) \
	(IN6_IS_ADDR_MULTICAST(addr) && (IN6_MSCOPE(addr) == IN6_LINK_LOCAL))
#define	IN6_IS_ADDR_MC_SITELOCAL(addr) \
	(IN6_IS_ADDR_MULTICAST(addr) && (IN6_MSCOPE(addr) == IN6_SITE_LOCAL))
#define	IN6_IS_ADDR_MC_ORGLOCAL(addr) \
	(IN6_IS_ADDR_MULTICAST(addr) && (IN6_MSCOPE(addr) == IN6_ORG_LOCAL))
#define	IN6_IS_ADDR_MC_GLOBAL(addr) \
	(IN6_IS_ADDR_MULTICAST(addr) && (IN6_MSCOPE(addr) == IN6_GLOBAL))

/*
 * Definitions of the IPv6 special addresses
 */
extern const struct in6_addr in6addr_any;
#define IN6ADDR_ANY_INIT {{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}}

extern const struct in6_addr in6addr_loopback;
#define IN6ADDR_LOOPBACK_INIT {{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}}
#endif

/*
 * IP Version 4 socket address.
 */
struct sockaddr_in {
	u_int8_t    sin_len;
	sa_family_t sin_family;
	in_port_t   sin_port;
	struct	    in_addr sin_addr;
	int8_t	    sin_zero[8];
};

#if 0	/*NRL IPv6*/
/*
 * IP Version 6 socket address.
 */
#define SIN6_LEN 1
struct sockaddr_in6 {
	u_int8_t	sin6_len;
	sa_family_t	sin6_family;
	in_port_t	sin6_port;
	u_int32_t	sin6_flowinfo;
	struct in6_addr	sin6_addr;
	u_int32_t	sin6_scope_id;
};
#endif

#define INET_ADDRSTRLEN                 16

/*
 * Structure used to describe IP options.
 * Used to store options internally, to pass them to a process,
 * or to restore options retrieved earlier.
 * The ip_dst is used for the first-hop gateway when using a source route
 * (this gets put into the header proper).
 */
struct ip_opts {
	struct in_addr	ip_dst;		/* first hop, 0 w/o src rt */
#if defined(__cplusplus)
	int8_t		Ip_opts[40];	/* cannot have same name as class */
#else
	int8_t		ip_opts[40];	/* actually variable in size */
#endif
};

/*
 * Options for use with [gs]etsockopt at the IP level.
 * First word of comment is data type; bool is stored in int.
 */
#define	IP_OPTIONS		1    /* buf/ip_opts; set/get IP options */
#define	IP_HDRINCL		2    /* int; header is included with data */
#define	IP_TOS			3    /* int; IP type of service and preced. */
#define	IP_TTL			4    /* int; IP time to live */
#define	IP_RECVOPTS		5    /* bool; receive all IP opts w/dgram */
#define	IP_RECVRETOPTS		6    /* bool; receive IP opts for response */
#define	IP_RECVDSTADDR		7    /* bool; receive IP dst addr w/dgram */
#define	IP_RETOPTS		8    /* ip_opts; set/get IP options */
#define	IP_MULTICAST_IF		9    /* in_addr; set/get IP multicast i/f  */
#define	IP_MULTICAST_TTL	10   /* u_char; set/get IP multicast ttl */
#define	IP_MULTICAST_LOOP	11   /* u_char; set/get IP multicast loopback */
#define	IP_ADD_MEMBERSHIP	12   /* ip_mreq; add an IP group membership */
#define	IP_DROP_MEMBERSHIP	13   /* ip_mreq; drop an IP group membership */

/* 14-17 left empty for future compatibility with FreeBSD */

#define IP_PORTRANGE		19   /* int; range to choose for unspec port */
#define IP_AUTH_LEVEL		20   /* u_char; authentication used */
#define IP_ESP_TRANS_LEVEL	21   /* u_char; transport encryption */
#define IP_ESP_NETWORK_LEVEL	22   /* u_char; full-packet encryption */

#if 0 /* NRL IPv6 */
#define IPV6_MULTICAST_IF	23   /* u_int; set/get multicast interface */
#define IPV6_MULTICAST_HOPS	24   /* int; set/get multicast hop limit */
#define IPV6_MULTICAST_LOOP	25   /* u_int; set/get multicast loopback */
#define IPV6_JOIN_GROUP		26   /* ipv6_mreq; join multicast group */
#define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP /* XXX - for compatibility */
#define IPV6_LEAVE_GROUP	27   /* ipv6_mreq: leave multicast group */
#define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP /* XXX - for compatibility */
#define IPV6_ADDRFORM		28   /* int; get/set form of returned addrs */
#define IPV6_UNICAST_HOPS	29   /* int; get/set unicast hop limit */
#define IPV6_PKTINFO		30   /* int; receive in6_pktinfo as cmsg */
#define IPV6_HOPLIMIT		31   /* int; receive int hoplimit as cmsg */
#define IPV6_NEXTHOP		32   /* int; receive sockaddr_in6 as cmsg */
#define IPV6_HOPOPTS		33   /* int; receive hop options as cmsg */
#define IPV6_DSTOPTS		34   /* int; receive dst options as cmsg */
#define IPV6_RTHDR		35   /* int; receive routing header as cmsg */
#define IPV6_PKTOPTIONS		36   /* int; send/receive cmsgs for TCP */
#define IPV6_CHECKSUM		37   /* int; offset to place send checksum */
#define ICMPV6_FILTER		38   /* struct icmpv6_filter; get/set filter */
#define ICMP6_FILTER		ICMP6_FILTER
#endif

#define IPSEC_OUTSA		39   /* set the outbound SA for a socket */

#if 0 /*KAME IPSEC*/
#define IP_IPSEC_POLICY		?? /* struct; get/set security policy */
#endif

/*
 * Security levels - IPsec, not IPSO
 */

#define IPSEC_LEVEL_BYPASS      0x00    /* Bypass policy altogether */
#define IPSEC_LEVEL_NONE        0x00    /* Send clear, accept any */
#define IPSEC_LEVEL_AVAIL       0x01    /* Send secure if SA available */
#define IPSEC_LEVEL_USE         0x02    /* Send secure, accept any */
#define IPSEC_LEVEL_REQUIRE     0x03    /* Require secure inbound, also use */
#define IPSEC_LEVEL_UNIQUE      0x04    /* Use outbound SA that is unique */
#define IPSEC_LEVEL_DEFAULT     IPSEC_LEVEL_AVAIL

#define IPSEC_AUTH_LEVEL_DEFAULT IPSEC_LEVEL_DEFAULT
#define IPSEC_ESP_TRANS_LEVEL_DEFAULT IPSEC_LEVEL_DEFAULT
#define IPSEC_ESP_NETWORK_LEVEL_DEFAULT IPSEC_LEVEL_DEFAULT

#if 0 /* NRL IPv6 */
/*
 * IPv6 Routing header types
 */
#define IPV6_RTHDR_TYPE_0	0 /* IPv6 Routing header type 0 */   

#define IPV6_RTHDR_LOOSE	0 /* this hop need not be a neighbor */
#define IPV6_RTHDR_STRICT	1 /* this hop must be a neighbor */
#endif

/*
 * Defaults and limits for options
 */
#define	IP_DEFAULT_MULTICAST_TTL  1	/* normally limit m'casts to 1 hop  */
#define	IP_DEFAULT_MULTICAST_LOOP 1	/* normally hear sends if a member  */
#define	IP_MAX_MEMBERSHIPS	20	/* per socket; must fit in one mbuf */

/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
	struct	in_addr imr_multiaddr;	/* IP multicast address of group */
	struct	in_addr imr_interface;	/* local IP address of interface */
};

#if 0 /* NRL IPv6 */
/*
 * Argument structure for IPV6_ADD_MEMBERSHIP and IPV6_DROP_MEMBERSHIP.
 */
struct ipv6_mreq {
	struct	in6_addr	ipv6mr_multiaddr; /* IPv6 multicast addr */
	unsigned int		ipv6mr_interface; /* Interface index */
};

/*
 * Argument structure for IPV6_PKTINFO control messages
 */
struct in6_pktinfo {
	struct in6_addr ipi6_addr;
	unsigned int ipi6_ifindex;
};
#endif

/*
 * Argument for IP_PORTRANGE:
 * - which range to search when port is unspecified at bind() or connect()
 */
#define IP_PORTRANGE_DEFAULT	0	/* default range */
#define IP_PORTRANGE_HIGH	1	/* "high" - request firewall bypass */
#define IP_PORTRANGE_LOW	2	/* "low" - vouchsafe security */

/*
 * Buffer lengths for strings containing printable IP addresses
 */
#define INET_ADDRSTRLEN		16
#if 0 /* NRL IPv6 */
#define INET6_ADDRSTRLEN	46
#endif

/*
 * Definitions for inet sysctl operations.
 *
 * Third level is protocol number.
 * Fourth level is desired variable within that protocol.
 */
#define	IPPROTO_MAXID	(IPPROTO_AH + 1)	/* don't list to IPPROTO_MAX */

#define	CTL_IPPROTO_NAMES { \
	{ "ip", CTLTYPE_NODE }, \
	{ "icmp", CTLTYPE_NODE }, \
	{ "igmp", CTLTYPE_NODE }, \
	{ "ggp", CTLTYPE_NODE }, \
	{ "ip4", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ "tcp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ "egp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "pup", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "udp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "esp", CTLTYPE_NODE }, \
	{ "ah", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "etherip", CTLTYPE_NODE }, \
}

/*
 * Names for IP sysctl objects
 */
#define	IPCTL_FORWARDING	1	/* act as router */
#define	IPCTL_SENDREDIRECTS	2	/* may send redirects when forwarding */
#define	IPCTL_DEFTTL		3	/* default TTL */
#ifdef notyet
#define	IPCTL_DEFMTU		4	/* default MTU */
#endif
#define	IPCTL_SOURCEROUTE	5	/* may perform source routes */
#define	IPCTL_DIRECTEDBCAST	6	/* default broadcast behavior */
#define IPCTL_IPPORT_FIRSTAUTO	7
#define IPCTL_IPPORT_LASTAUTO	8
#define IPCTL_IPPORT_HIFIRSTAUTO 9
#define IPCTL_IPPORT_HILASTAUTO	10
#define	IPCTL_IPPORT_MAXQUEUE	11
#define	IPCTL_ENCDEBUG		12
#define IPCTL_GIF_TTL		13	/* default TTL for gif encap packet */
#define	IPCTL_MAXID		14

#define	IPCTL_NAMES { \
	{ 0, 0 }, \
	{ "forwarding", CTLTYPE_INT }, \
	{ "redirect", CTLTYPE_INT }, \
	{ "ttl", CTLTYPE_INT }, \
	/* { "mtu", CTLTYPE_INT }, */ { 0, 0 }, \
	{ "sourceroute", CTLTYPE_INT }, \
	{ "directed-broadcast", CTLTYPE_INT }, \
	{ "portfirst", CTLTYPE_INT }, \
	{ "portlast", CTLTYPE_INT }, \
	{ "porthifirst", CTLTYPE_INT }, \
	{ "porthilast", CTLTYPE_INT }, \
	{ "maxqueue", CTLTYPE_INT }, \
	{ "encdebug", CTLTYPE_INT }, \
	{ "gifttl", CTLTYPE_INT }, \
}

/* INET6 stuff */
#include <netinet6/in6.h>

#ifndef _KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	   bindresvport __P((int, struct sockaddr_in *));
int	   bindresvport_af __P((int, struct sockaddr *, int af));
__END_DECLS

#else
int	   in_broadcast __P((struct in_addr, struct ifnet *));
int	   in_canforward __P((struct in_addr));
int	   in_cksum __P((struct mbuf *, int));
int	   in_localaddr __P((struct in_addr));
void	   in_socktrim __P((struct sockaddr_in *));
char	  *inet_ntoa __P((struct in_addr));

#define	satosin(sa)	((struct sockaddr_in *)(sa))
#define	sintosa(sin)	((struct sockaddr *)(sin))
#define	ifatoia(ifa)	((struct in_ifaddr *)(ifa))
#endif
#endif /* !_NETINET_IN_H_ */
