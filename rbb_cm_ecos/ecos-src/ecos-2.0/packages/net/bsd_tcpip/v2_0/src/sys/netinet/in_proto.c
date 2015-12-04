//==========================================================================
//
//      src/sys/netinet/in_proto.c
//
//==========================================================================
//####BSDCOPYRIGHTBEGIN####
//
// -------------------------------------------
//
// Portions of this software may have been derived from OpenBSD, 
// FreeBSD or other sources, and are covered by the appropriate
// copyright disclaimers included herein.
//
// Portions created by Red Hat are
// Copyright (C) 2002 Red Hat, Inc. All Rights Reserved.
//
// -------------------------------------------
//
//####BSDCOPYRIGHTEND####
//==========================================================================

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)in_proto.c	8.2 (Berkeley) 2/9/95
 * $FreeBSD: src/sys/netinet/in_proto.c,v 1.53.2.4 2001/07/24 19:10:18 brooks Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_encap.h>

/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ah.h>
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#endif
#include <netinet6/ipcomp.h>
#endif /* IPSEC */

#ifdef IPXIP
#include <netipx/ipx_ip.h>
#endif

#ifdef NSIP
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef NATPT
void	natpt_init	__P((void));
int	natpt_ctloutput	__P((int, struct socket *, int, int, struct mbuf **));
struct pr_usrreqs natpt_usrreqs;
#endif

extern	struct domain inetdomain;
static	struct pr_usrreqs nousrreqs;

struct protosw inetsw[] = {
{ 0,		&inetdomain,	0,		0,
  0,		0,		0,		0,
  0,
  ip_init,	0,		ip_slowtimo,	ip_drain,
  &nousrreqs
},
{ SOCK_DGRAM,	&inetdomain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR,
  udp_input,	0,		udp_ctlinput,	ip_ctloutput,
  0,
  udp_init,	0,		0,		0,
  &udp_usrreqs
},
{ SOCK_STREAM,	&inetdomain,	IPPROTO_TCP,
	PR_CONNREQUIRED|PR_IMPLOPCL|PR_WANTRCVD,
  tcp_input,	0,		tcp_ctlinput,	tcp_ctloutput,
  0,
  tcp_init,	0,		tcp_slowtimo,	tcp_drain,
  &tcp_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip_input,	0,		rip_ctlinput,	rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_ICMP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  icmp_input,	0,		0,		rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_IGMP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  igmp_input,	0,		0,		rip_ctloutput,
  0,
  igmp_init,	igmp_fasttimo,	igmp_slowtimo,	0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RSVP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  rsvp_input,	0,		0,		rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
#ifdef IPSEC
{ SOCK_RAW,	&inetdomain,	IPPROTO_AH,	PR_ATOMIC|PR_ADDR,
  ah4_input,	0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
  &nousrreqs
},
#ifdef IPSEC_ESP
{ SOCK_RAW,	&inetdomain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  esp4_input,	0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
  &nousrreqs
},
#endif
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipcomp4_input, 0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
  &nousrreqs
},
#endif /* IPSEC */
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap4_input,	0,	 	0,		rip_ctloutput,
  0,
  encap_init,		0,		0,		0,
  &rip_usrreqs
},
# ifdef INET6
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap4_input,	0,	 	0,		rip_ctloutput,
  0,
  encap_init,	0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef IPDIVERT
{ SOCK_RAW,	&inetdomain,	IPPROTO_DIVERT,	PR_ATOMIC|PR_ADDR,
  div_input,	0,	 	0,		ip_ctloutput,
  0,
  div_init,	0,		0,		0,
  &div_usrreqs,
},
#endif
#ifdef IPXIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  ipxip_input,	0,		ipxip_ctlinput,	0,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef NSIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  idpip_input,	0,		nsip_ctlinput,	0,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef NATPT
{ SOCK_RAW,	&inetdomain,	IPPROTO_AHIP,	PR_ATOMIC|PR_ADDR,
  0,		0,		0,		0,
  0,
  natpt_init,	0,		0,		0,
 &natpt_usrreqs
},
#endif
	/* raw wildcard */
{ SOCK_RAW,	&inetdomain,	0,		PR_ATOMIC|PR_ADDR,
  rip_input,	0,		0,		rip_ctloutput,
  0,
  rip_init,	0,		0,		0,
  &rip_usrreqs
},
};

extern int in_inithead __P((void **, int));

struct domain inetdomain =
    { AF_INET, "internet", 0, 0, 0, 
      (struct protosw *)inetsw,
      (struct protosw *)&inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
      in_inithead, 32, sizeof(struct sockaddr_in)
    };

DOMAIN_SET(inet);
