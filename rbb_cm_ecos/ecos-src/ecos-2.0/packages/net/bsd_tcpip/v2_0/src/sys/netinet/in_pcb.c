//==========================================================================
//
//      src/sys/netinet/in_pcb.c
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
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//==========================================================================

/*
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
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
 *	@(#)in_pcb.c	8.4 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/in_pcb.c,v 1.59.2.17 2001/08/13 16:26:17 ume Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif /* IPSEC */

#include <netinet/if_bcm.h>     // cliffmod   

struct	in_addr zeroin_addr;

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
int	ipport_lowfirstauto  = IPPORT_RESERVED - 1;	/* 1023 */
int	ipport_lowlastauto = IPPORT_RESERVEDSTART;	/* 600 */
int	ipport_firstauto = IPPORT_RESERVED;		/* 1024 */
int	ipport_lastauto  = IPPORT_USERRESERVED;		/* 5000 */
int	ipport_hifirstauto = IPPORT_HIFIRSTAUTO;	/* 49152 */
int	ipport_hilastauto  = IPPORT_HILASTAUTO;		/* 65535 */


/*
 * in_pcb.c: manage the Protocol Control Blocks.
 *
 * NOTE: It is assumed that most of these functions will be called at
 * splnet(). XXX - There are, unfortunately, a few exceptions to this
 * rule that should be fixed.
 */

/*
 * Allocate a PCB and associate it with the socket.
 */
int
in_pcballoc(so, pcbinfo, p)
	struct socket *so;
	struct inpcbinfo *pcbinfo;
	struct proc *p;
{
	register struct inpcb *inp;
#ifdef IPSEC
	int error;
#endif

	inp = zalloci(pcbinfo->ipi_zone);
	if (inp == NULL)
		return (ENOBUFS);
	bzero((caddr_t)inp, sizeof(*inp));
	inp->inp_gencnt = ++pcbinfo->ipi_gencnt;
	inp->inp_pcbinfo = pcbinfo;
	inp->inp_socket = so;
#ifdef IPSEC
	error = ipsec_init_policy(so, &inp->inp_sp);
	if (error != 0) {
		zfreei(pcbinfo->ipi_zone, inp);
		return error;
	}
#endif /*IPSEC*/
#if defined(INET6)
	if (INP_SOCKAF(so) == AF_INET6 && !ip6_mapped_addr_on)
		inp->inp_flags |= IN6P_IPV6_V6ONLY;
#endif
	LIST_INSERT_HEAD(pcbinfo->listhead, inp, inp_list);
	pcbinfo->ipi_count++;
	so->so_pcb = (caddr_t)inp;
#ifdef INET6
	if (ip6_auto_flowlabel)
		inp->inp_flags |= IN6P_AUTOFLOWLABEL;
#endif
	return (0);
}

int
in_pcbbind(inp, nam, p)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct proc *p;
{
	register struct socket *so = inp->inp_socket;
	unsigned short *lastport;
	struct sockaddr_in *sin;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	u_short lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);

	if (TAILQ_EMPTY(&in_ifaddrhead)) /* XXX broken! */
		return (EADDRNOTAVAIL);
	if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
		wild = 1;
	if (nam) {
		sin = (struct sockaddr_in *)nam;
        // HACK
        if (nam->sa_len == 0) nam->sa_len = sizeof(*sin);
        // HACK
		if (nam->sa_len != sizeof (*sin))
			return (EINVAL);
#ifdef notdef
		/*
		 * We should check the family, but old programs
		 * incorrectly fail to initialize it.
		 */
		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);
#endif
		lport = sin->sin_port;
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow complete duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (sin->sin_addr.s_addr != INADDR_ANY) {
			sin->sin_port = 0;		/* yech... */
			if (ifa_ifwithaddr((struct sockaddr *)sin) == 0)
				return (EADDRNOTAVAIL);
		}
		if (lport) {
			struct inpcb *t;

			/* GROSS */
			t = in_pcblookup_local(pcbinfo, sin->sin_addr, lport, wild);
			if (t &&
			    (reuseport & t->inp_socket->so_options) == 0) {
#if defined(INET6)
				if (ntohl(sin->sin_addr.s_addr) !=
				    INADDR_ANY ||
				    ntohl(t->inp_laddr.s_addr) !=
				    INADDR_ANY ||
				    INP_SOCKAF(so) ==
				    INP_SOCKAF(t->inp_socket))
#endif /* defined(INET6) */
				return (EADDRINUSE);
			}
		}
		inp->inp_laddr = sin->sin_addr;
	}
	if (lport == 0) {
		u_short first, last;
		int count;

		inp->inp_flags |= INP_ANONPORT;

		if (inp->inp_flags & INP_HIGHPORT) {
			first = ipport_hifirstauto;	/* sysctl */
			last  = ipport_hilastauto;
			lastport = &pcbinfo->lasthi;
		} else if (inp->inp_flags & INP_LOWPORT) {
			first = ipport_lowfirstauto;	/* 1023 */
			last  = ipport_lowlastauto;	/* 600 */
			lastport = &pcbinfo->lastlow;
		} else {
			first = ipport_firstauto;	/* sysctl */
			last  = ipport_lastauto;
			lastport = &pcbinfo->lastport;
		}
		/*
		 * Simple check to ensure all ports are not used up causing
		 * a deadlock here.
		 *
		 * We split the two cases (up and down) so that the direction
		 * is not being tested on each round of the loop.
		 */
		if (first > last) {
			/*
			 * counting down
			 */
			count = first - last;

			do {
				if (count-- < 0) {	/* completely used? */
					inp->inp_laddr.s_addr = INADDR_ANY;
					return (EADDRNOTAVAIL);
				}
				--*lastport;
				if (*lastport > first || *lastport < last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup_local(pcbinfo,
				 inp->inp_laddr, lport, wild));
		} else {
			/*
			 * counting up
			 */
			count = last - first;

			do {
				if (count-- < 0) {	/* completely used? */
					inp->inp_laddr.s_addr = INADDR_ANY;
					return (EADDRNOTAVAIL);
				}
				++*lastport;
				if (*lastport < first || *lastport > last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup_local(pcbinfo,
				 inp->inp_laddr, lport, wild));
		}
	}
	inp->inp_lport = lport;
	if (in_pcbinshash(inp) != 0) {
		inp->inp_laddr.s_addr = INADDR_ANY;
		inp->inp_lport = 0;
		return (EAGAIN);
	}
	return (0);
}

/*
 *   Transform old in_pcbconnect() into an inner subroutine for new
 *   in_pcbconnect(): Do some validity-checking on the remote
 *   address (in mbuf 'nam') and then determine local host address
 *   (i.e., which interface) to use to access that remote host.
 *
 *   This preserves definition of in_pcbconnect(), while supporting a
 *   slightly different version for T/TCP.  (This is more than
 *   a bit of a kludge, but cleaning up the internal interfaces would
 *   have forced minor changes in every protocol).
 */

int
in_pcbladdr(inp, nam, plocal_sin)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct sockaddr_in **plocal_sin;
{
	struct in_ifaddr *ia;
	register struct sockaddr_in *sin = (struct sockaddr_in *)nam;

	if (nam->sa_len != sizeof (*sin))
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	if (!TAILQ_EMPTY(&in_ifaddrhead)) {
		/*
		 * If the destination address is INADDR_ANY,
		 * use the primary local address.
		 * If the supplied address is INADDR_BROADCAST,
		 * and the primary interface supports broadcast,
		 * choose the broadcast address for that interface.
		 */
#define	satosin(sa)	((struct sockaddr_in *)(sa))
#define sintosa(sin)	((struct sockaddr *)(sin))
#define ifatoia(ifa)	((struct in_ifaddr *)(ifa))
		if (sin->sin_addr.s_addr == INADDR_ANY)
		    sin->sin_addr = IA_SIN(TAILQ_FIRST(&in_ifaddrhead))->sin_addr;
		else if (sin->sin_addr.s_addr == (u_long)INADDR_BROADCAST &&
		  (TAILQ_FIRST(&in_ifaddrhead)->ia_ifp->if_flags & IFF_BROADCAST))
		    sin->sin_addr = satosin(&TAILQ_FIRST(&in_ifaddrhead)->ia_broadaddr)->sin_addr;
	}
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		register struct route *ro;

		ia = (struct in_ifaddr *)0;
		/*
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */
		ro = &inp->inp_route;
		if (ro->ro_rt &&
		    (satosin(&ro->ro_dst)->sin_addr.s_addr !=
			sin->sin_addr.s_addr ||
		    inp->inp_socket->so_options & SO_DONTROUTE)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
		if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
		    (ro->ro_rt == (struct rtentry *)0 ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
			/* No route yet, so try to acquire one */
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
				sin->sin_addr;
			rtalloc(ro);
		}
		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 * unless it is the loopback (in case a route
		 * to our address on another net goes to loopback).
		 */
		if (ro->ro_rt && !(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
			ia = ifatoia(ro->ro_rt->rt_ifa);
		if (ia == 0) {
			u_short fport = sin->sin_port;

			sin->sin_port = 0;
			ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
			if (ia == 0)
				ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
			sin->sin_port = fport;
			if (ia == 0)
				ia = TAILQ_FIRST(&in_ifaddrhead);
			if (ia == 0)
				return (EADDRNOTAVAIL);
		}
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
		    inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
					if (ia->ia_ifp == ifp)
						break;
				if (ia == 0)
					return (EADDRNOTAVAIL);
			}
		}
	/*
	 * Don't do pcblookup call here; return interface in plocal_sin
	 * and exit to caller, that will do the lookup.
	 */
		*plocal_sin = &ia->ia_addr;

	}
	return(0);
}

/*
 * Outer subroutine:
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(inp, nam, p)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct proc *p;
{
    // begin cliffmod
	struct sockaddr_in *plocal_ifaddr;
	struct sockaddr_in local_ifaddr;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;
	int error;

    unsigned long local_ifaddr_u32 = 0;
    struct ifnet* pifnet = NULL;
    bzero( (caddr_t)(&local_ifaddr), sizeof( local_ifaddr ) );
    plocal_ifaddr = &local_ifaddr;

    if( !BcmTestAndComputeLocalNetIf( inp, pifnet, &local_ifaddr_u32 ) )
    {
        // brcm local net iface not specified or not found...
        // call inner routine, to assign local interface address.
	    if( (error = in_pcbladdr(inp, nam, &plocal_ifaddr)) != 0 )
        {
		    return(error);
        }

        if( inp->inp_laddr.s_addr )
        {
            // non-zero local ip address in inpcb.
            // --> assign it to local_ifaddr_u32.
            local_ifaddr_u32 = inp->inp_laddr.s_addr;
        }
        else
        {
            // else ip address stored in inpcb is zero.
            if( plocal_ifaddr )
            {
                // in_pcbladdr() returned a non-zero net iface ptr.
                // --> assign ip address of net iface returned by in_pcbladdr()
                // to local_ifaddr_u32.
                local_ifaddr_u32 = plocal_ifaddr->sin_addr.s_addr;
            }
        }
    } // else brcm local net iface specified and local_ifaddr_u32 is 
    // loaded with non-zero ip address of local net iface.
    //
    // load local_ifaddr.
    local_ifaddr.sin_family = AF_INET;
    local_ifaddr.sin_port = 0;
    local_ifaddr.sin_len = sizeof( local_ifaddr );
    local_ifaddr.sin_addr.s_addr = local_ifaddr_u32;

    if( in_pcblookup_hash(inp->inp_pcbinfo, sin->sin_addr, sin->sin_port,
	   	local_ifaddr.sin_addr, inp->inp_lport, 0, pifnet) != NULL )
	{	      	
        return (EADDRINUSE);
	}            
		
	if (inp->inp_laddr.s_addr == INADDR_ANY)
    {
		// special case: wildcard (a.k.a. 0) local source IP address
        if (inp->inp_lport == 0)
        {
			(void)in_pcbbind(inp, (struct sockaddr *)0, p);
        }			
		inp->inp_laddr.s_addr = local_ifaddr_u32;
	}
	// end cliffmod
	
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	in_pcbrehash(inp);
#ifdef IPSEC
	if (inp->inp_socket->so_type == SOCK_STREAM)
		ipsec_pcbconn(inp->inp_sp);
#endif
	return (0);
}

void
in_pcbdisconnect(inp)
	struct inpcb *inp;
{

	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_fport = 0;
	in_pcbrehash(inp);
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
#ifdef IPSEC
	ipsec_pcbdisconn(inp->inp_sp);
#endif
}

void
in_pcbdetach(inp)
	struct inpcb *inp;
{
	struct socket *so = inp->inp_socket;
	struct inpcbinfo *ipi = inp->inp_pcbinfo;
	struct rtentry *rt  = inp->inp_route.ro_rt;

#ifdef IPSEC
	ipsec4_delete_pcbpolicy(inp);
#endif /*IPSEC*/
	inp->inp_gencnt = ++ipi->ipi_gencnt;
	in_pcbremlists(inp);
	so->so_pcb = 0;
	sofree(so);
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
	if (rt) {
		/* 
		 * route deletion requires reference count to be <= zero 
		 */
		if ((rt->rt_flags & RTF_DELCLONE) &&
		    (rt->rt_flags & RTF_WASCLONED) &&
		    (rt->rt_refcnt <= 1)) {
			rt->rt_refcnt--;
			rt->rt_flags &= ~RTF_UP;
			rtrequest(RTM_DELETE, rt_key(rt),
				  rt->rt_gateway, rt_mask(rt),
				  rt->rt_flags, (struct rtentry **)0);
		}
		else
			rtfree(rt);
	}
	ip_freemoptions(inp->inp_moptions);
	inp->inp_vflag = 0;
	zfreei(ipi->ipi_zone, inp);
}

/*
 * The calling convention of in_setsockaddr() and in_setpeeraddr() was
 * modified to match the pru_sockaddr() and pru_peeraddr() entry points
 * in struct pr_usrreqs, so that protocols can just reference then directly
 * without the need for a wrapper function.  The socket must have a valid
 * (i.e., non-nil) PCB, but it should be impossible to get an invalid one
 * except through a kernel programming error, so it is acceptable to panic
 * (or in this case trap) if the PCB is invalid.  (Actually, we don't trap
 * because there actually /is/ a programming error somewhere... XXX)
 */
int
in_setsockaddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	register struct inpcb *inp;
	register struct sockaddr_in *sin;

	/*
	 * Do the malloc first in case it blocks.
	 */
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		M_WAITOK | M_ZERO);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);

	s = splnet();
	inp = sotoinpcb(so);
	if (!inp) {
		splx(s);
		free(sin, M_SONAME);
		return ECONNRESET;
	}
	sin->sin_port = inp->inp_lport;
	sin->sin_addr = inp->inp_laddr;
	splx(s);

	*nam = (struct sockaddr *)sin;
	return 0;
}

int
in_setpeeraddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	struct inpcb *inp;
	register struct sockaddr_in *sin;

	/*
	 * Do the malloc first in case it blocks.
	 */
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		M_WAITOK | M_ZERO);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);

	s = splnet();
	inp = sotoinpcb(so);
	if (!inp) {
		splx(s);
		free(sin, M_SONAME);
		return ECONNRESET;
	}
	sin->sin_port = inp->inp_fport;
	sin->sin_addr = inp->inp_faddr;
	splx(s);

	*nam = (struct sockaddr *)sin;
	return 0;
}

void
in_pcbnotifyall(head, faddr, _errno, notify)
	struct inpcbhead *head;
	struct in_addr faddr;
	void (*notify) __P((struct inpcb *, int));
{
	struct inpcb *inp, *ninp;
	int s;

	s = splnet();
	for (inp = LIST_FIRST(head); inp != NULL; inp = ninp) {
		ninp = LIST_NEXT(inp, inp_list);
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV4) == 0)
			continue;
#endif
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == NULL)
				continue;
		(*notify)(inp, _errno);
	}
	splx(s);
}

void
in_pcbpurgeif0(head, ifp)
	struct inpcb *head;
	struct ifnet *ifp;
{
	struct inpcb *inp;
	struct ip_moptions *imo;
	int i, gap;

	for (inp = head; inp != NULL; inp = LIST_NEXT(inp, inp_list)) {
		imo = inp->inp_moptions;
		if ((inp->inp_vflag & INP_IPV4) &&
		    imo != NULL) {
			/*
			 * Unselect the outgoing interface if it is being
			 * detached.
			 */
			if (imo->imo_multicast_ifp == ifp)
				imo->imo_multicast_ifp = NULL;

			/*
			 * Drop multicast group membership if we joined
			 * through the interface being detached.
			 */
			for (i = 0, gap = 0; i < imo->imo_num_memberships;
			    i++) {
				if (imo->imo_membership[i]->inm_ifp == ifp) {
					in_delmulti(imo->imo_membership[i]);
					gap++;
				} else if (gap != 0)
					imo->imo_membership[i - gap] =
					    imo->imo_membership[i];
			}
			imo->imo_num_memberships -= gap;
		}
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(inp)
	struct inpcb *inp;
{
	register struct rtentry *rt;
	struct rt_addrinfo info;

	if ((rt = inp->inp_route.ro_rt)) {
		bzero((caddr_t)&info, sizeof(info));
		info.rti_info[RTAX_DST] =
			(struct sockaddr *)&inp->inp_route.ro_dst;
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
		if (rt->rt_flags & RTF_DYNAMIC)
			(void) rtrequest(RTM_DELETE, rt_key(rt),
				rt->rt_gateway, rt_mask(rt), rt->rt_flags,
				(struct rtentry **)0);
		inp->inp_route.ro_rt = 0;
		rtfree(rt);
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 */
	}
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
in_rtchange(inp, _errno)
	register struct inpcb *inp;
	int _errno;
{
	if (inp->inp_route.ro_rt) {
		rtfree(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
}

/*
 * Lookup a PCB based on the local address and port.
 */
struct inpcb *
in_pcblookup_local(pcbinfo, laddr, lport_arg, wild_okay)
	struct inpcbinfo *pcbinfo;
	struct in_addr laddr;
	u_int lport_arg;
	int wild_okay;
{
	register struct inpcb *inp;
	int matchwild = 3, wildcard;
	u_short lport = lport_arg;

	if (!wild_okay) {
		struct inpcbhead *head;
		/*
		 * Look for an unconnected (wildcard foreign addr) PCB that
		 * matches the local address and port we're looking for.
		 */
		head = &pcbinfo->hashbase[INP_PCBHASH(INADDR_ANY, lport, 0, pcbinfo->hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_faddr.s_addr == INADDR_ANY &&
			    inp->inp_laddr.s_addr == laddr.s_addr &&
			    inp->inp_lport == lport) {
				/*
				 * Found.
				 */
				return (inp);
			}
		}
		/*
		 * Not found.
		 */
		return (NULL);
	} else {
		struct inpcbporthead *porthash;
		struct inpcbport *phd;
		struct inpcb *match = NULL;
		/*
		 * Best fit PCB lookup.
		 *
		 * First see if this local port is in use by looking on the
		 * port hash list.
		 */
		porthash = &pcbinfo->porthashbase[INP_PCBPORTHASH(lport,
		    pcbinfo->porthashmask)];
		LIST_FOREACH(phd, porthash, phd_hash) {
			if (phd->phd_port == lport)
				break;
		}
		if (phd != NULL) {
			/*
			 * Port is in use by one or more PCBs. Look for best
			 * fit.
			 */
			LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
				wildcard = 0;
#ifdef INET6
				if ((inp->inp_vflag & INP_IPV4) == 0)
					continue;
#endif
				if (inp->inp_faddr.s_addr != INADDR_ANY)
					wildcard++;
				if (inp->inp_laddr.s_addr != INADDR_ANY) {
					if (laddr.s_addr == INADDR_ANY)
						wildcard++;
					else if (inp->inp_laddr.s_addr != laddr.s_addr)
						continue;
				} else {
					if (laddr.s_addr != INADDR_ANY)
						wildcard++;
				}
				if (wildcard < matchwild) {
					match = inp;
					matchwild = wildcard;
					if (matchwild == 0) {
						break;
					}
				}
			}
		}
		return (match);
	}
}

/*
 * Lookup PCB in hash list.
 */
struct inpcb *
in_pcblookup_hash(pcbinfo, faddr, fport_arg, laddr, lport_arg, wildcard,
		  ifp)
	struct inpcbinfo *pcbinfo;
	struct in_addr faddr, laddr;
	u_int fport_arg, lport_arg;
	int wildcard;
	struct ifnet *ifp;
{
	struct inpcbhead *head;
	register struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->hashbase[INP_PCBHASH(faddr.s_addr, lport, fport, pcbinfo->hashmask)];
	LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV4) == 0)
			continue;
#endif
		if (inp->inp_faddr.s_addr == faddr.s_addr &&
		    inp->inp_laddr.s_addr == laddr.s_addr &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport) {
			/*
			 * Found.
			 */
			return (inp);
		}
	}
	if (wildcard) {
		struct inpcb *local_wild = NULL;
#if defined(INET6)
		struct inpcb *local_wild_mapped = NULL;
#endif /* defined(INET6) */

		head = &pcbinfo->hashbase[INP_PCBHASH(INADDR_ANY, lport, 0, pcbinfo->hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_faddr.s_addr == INADDR_ANY &&
			    inp->inp_lport == lport) {
#if defined(NFAITH) && NFAITH > 0
				if (ifp && ifp->if_type == IFT_FAITH &&
				    (inp->inp_flags & INP_FAITH) == 0)
					continue;
#endif
				if (inp->inp_laddr.s_addr == laddr.s_addr)
					return (inp);
				else if (inp->inp_laddr.s_addr == INADDR_ANY) {
#if defined(INET6)
					if (INP_CHECK_SOCKAF(inp->inp_socket,
							     AF_INET6))
						local_wild_mapped = inp;
					else
#endif /* defined(INET6) */
					local_wild = inp;
				}
			}
		}
#if defined(INET6)
		if (local_wild == NULL)
			return (local_wild_mapped);
#endif /* defined(INET6) */
		return (local_wild);
	}

	/*
	 * Not found.
	 */
	return (NULL);
}

/*
 * Insert PCB onto various hash lists.
 */
int
in_pcbinshash(inp)
	struct inpcb *inp;
{
	struct inpcbhead *pcbhash;
	struct inpcbporthead *pcbporthash;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbport *phd;
	u_int32_t hashkey_faddr;

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6)
		hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX */;
	else
#endif /* INET6 */
	hashkey_faddr = inp->inp_faddr.s_addr;

	pcbhash = &pcbinfo->hashbase[INP_PCBHASH(hashkey_faddr,
		 inp->inp_lport, inp->inp_fport, pcbinfo->hashmask)];

	pcbporthash = &pcbinfo->porthashbase[INP_PCBPORTHASH(inp->inp_lport,
	    pcbinfo->porthashmask)];

	/*
	 * Go through port list and look for a head for this lport.
	 */
	LIST_FOREACH(phd, pcbporthash, phd_hash) {
		if (phd->phd_port == inp->inp_lport)
			break;
	}
	/*
	 * If none exists, malloc one and tack it on.
	 */
	if (phd == NULL) {
		MALLOC(phd, struct inpcbport *, sizeof(struct inpcbport), M_PCB, M_NOWAIT);
		if (phd == NULL) {
			return (ENOBUFS); /* XXX */
		}
		phd->phd_port = inp->inp_lport;
		LIST_INIT(&phd->phd_pcblist);
		LIST_INSERT_HEAD(pcbporthash, phd, phd_hash);
	}
	inp->inp_phd = phd;
	LIST_INSERT_HEAD(&phd->phd_pcblist, inp, inp_portlist);
	LIST_INSERT_HEAD(pcbhash, inp, inp_hash);
	return (0);
}

/*
 * Move PCB to the proper hash bucket when { faddr, fport } have  been
 * changed. NOTE: This does not handle the case of the lport changing (the
 * hashed port list would have to be updated as well), so the lport must
 * not change after in_pcbinshash() has been called.
 */
void
in_pcbrehash(inp)
	struct inpcb *inp;
{
	struct inpcbhead *head;
	u_int32_t hashkey_faddr;

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6)
		hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX */;
	else
#endif /* INET6 */
	hashkey_faddr = inp->inp_faddr.s_addr;

	head = &inp->inp_pcbinfo->hashbase[INP_PCBHASH(hashkey_faddr,
		inp->inp_lport, inp->inp_fport, inp->inp_pcbinfo->hashmask)];

	LIST_REMOVE(inp, inp_hash);
	LIST_INSERT_HEAD(head, inp, inp_hash);
}

/*
 * Remove PCB from various lists.
 */
void
in_pcbremlists(inp)
	struct inpcb *inp;
{
	inp->inp_gencnt = ++inp->inp_pcbinfo->ipi_gencnt;
	if (inp->inp_lport) {
		struct inpcbport *phd = inp->inp_phd;

		LIST_REMOVE(inp, inp_hash);
		LIST_REMOVE(inp, inp_portlist);
		if (LIST_FIRST(&phd->phd_pcblist) == NULL) {
			LIST_REMOVE(phd, phd_hash);
			free(phd, M_PCB);
		}
	}
	LIST_REMOVE(inp, inp_list);
	inp->inp_pcbinfo->ipi_count--;
}
