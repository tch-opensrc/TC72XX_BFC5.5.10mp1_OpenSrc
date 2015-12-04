//==========================================================================
//
//      sys/net/if_loop.c
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


/*	$OpenBSD: if_loop.c,v 1.12 1999/12/08 06:50:18 itojun Exp $	*/
/*	$NetBSD: if_loop.c,v 1.15 1996/05/07 02:40:33 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 */

/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.
*/

/*
 * Loopback interface driver for protocol testing and timing.
 */

#ifndef __ECOS
#include "bpfilter.h"
#include "loop.h"
#endif

#include <sys/param.h>
#ifndef __ECOS
#include <sys/systm.h>
#endif
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet6/ip6.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef ISO
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#endif

#ifdef NETATALK
#include <netinet/if_ether.h>
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if defined(LARGE_LOMTU)
#define LOMTU	(131072 +  MHLEN + MLEN)
#else
#define	LOMTU	(32768 +  MHLEN + MLEN)
#endif

#ifndef __ECOS
#include <stdio.h>    // for 'sprintf()'
#endif
  
struct	ifnet loif[NLOOP];

void
loopattach(n)
	int n;
{
	register int i;
	register struct ifnet *ifp;

	for (i = NLOOP; i--; ) {
		ifp = &loif[i];
#if !defined(__ECOS) || (CYGPKG_NET_NLOOP > 1)
		sprintf(ifp->if_xname, "lo%d", i);
#else
                strcpy(ifp->if_xname, "lo0");
#endif                
		ifp->if_softc = NULL;
		ifp->if_mtu = LOMTU;
		ifp->if_flags = IFF_LOOPBACK | IFF_MULTICAST;
		ifp->if_ioctl = loioctl;
		ifp->if_output = looutput;
		ifp->if_type = IFT_LOOP;
		ifp->if_hdrlen = sizeof(u_int32_t);
		ifp->if_addrlen = 0;
		if_attachhead(ifp);
#if NBPFILTER > 0
		bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	}
}

int
looutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
	register struct rtentry *rt;
{
	int s, isr;
	register struct ifqueue *ifq = 0;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("looutput: no header mbuf");
	ifp->if_lastchange = time;
#if NBPFILTER > 0
	/*
	 * only send packets to bpf if they are real loopback packets;
	 * looutput() is also called for SIMPLEX interfaces to duplicate
	 * packets for local use. But don't dup them to bpf.
	 */
	if (ifp->if_bpf && (ifp->if_flags&IFF_LOOPBACK)) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer to it).
		 */
		struct mbuf m0;
		u_int32_t af = htonl(dst->sa_family);

		m0.m_next = m;
		m0.m_len = sizeof(af);
		m0.m_data = (char *)&af;

		bpf_mtap(ifp->if_bpf, &m0);
	}
#endif
	m->m_pkthdr.rcvif = ifp;

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
			rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}

#ifndef PULLDOWN_TEST
	/*
	 * KAME requires that the packet to be contiguous on the
	 * mbuf.  We need to make that sure.
	 * this kind of code should be avoided.
	 * XXX other conditions to avoid running this part?
	 */
	if (m && m->m_next != NULL) {
		struct mbuf *n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n) {
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				n = NULL;
			}
		}
		if (!n) {
#ifdef __ECOS
			diag_printf("looutput: mbuf allocation failed\n");
#else
			printf("looutput: mbuf allocation failed\n");
#endif
			m_freem(m);
			return ENOBUFS;
		}

		n->m_pkthdr.rcvif = m->m_pkthdr.rcvif;
		n->m_pkthdr.len = m->m_pkthdr.len;
		if (m->m_pkthdr.len <= MCLBYTES) {
			m_copydata(m, 0, m->m_pkthdr.len, mtod(n, caddr_t));
			n->m_len = m->m_pkthdr.len;
			m_freem(m);
		} else {
			m_copydata(m, 0, MCLBYTES, mtod(n, caddr_t));
			m_adj(m, MCLBYTES);
			n->m_len = MCLBYTES;
			n->m_next = m;
			m->m_flags &= ~M_PKTHDR;
		}
		m = n;
	}
#if 0
	if (m && m->m_next != NULL) {
		printf("loop: not contiguous...\n");
		m_freem(m);
		return ENOBUFS;
	}
#endif
#endif

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		break;
#endif /* INET6 */
#ifdef NS
	case AF_NS:
		ifq = &nsintrq;
		isr = NETISR_NS;
		break;
#endif
#ifdef IPX
	case AF_IPX:
		ifq = &ipxintrq;
		isr = NETISR_IPX;
		break;
#endif
#ifdef ISO
	case AF_ISO:
		ifq = &clnlintrq;
		isr = NETISR_ISO;
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
		ifq = &atintrq2;
		isr = NETISR_ATALK;
		break;
#endif
	default:
#ifdef __ECOS
		diag_printf("%s: can't handle af%d\n", ifp->if_xname,
                            dst->sa_family);
#else
		printf("%s: can't handle af%d\n", ifp->if_xname,
			dst->sa_family);
#endif
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(isr);
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	splx(s);
	return (0);
}

/* ARGSUSED */
void
lortrequest(cmd, rt, sa)
	int cmd;
	struct rtentry *rt;
	struct sockaddr *sa;
{

	if (rt)
		rt->rt_rmx.rmx_mtu = LOMTU;
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
loioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	register struct ifaddr *ifa;
	register struct ifreq *ifr;
	register int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		ifa = (struct ifaddr *)data;
		if (ifa != 0 /*&& ifa->ifa_addr->sa_family == AF_ISO*/)
			ifa->ifa_rtrequest = lortrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {

#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif /* INET6 */

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
