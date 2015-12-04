//==========================================================================
//
//      sys/netinet/in_pcb.c
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


/*	$OpenBSD: in_pcb.c,v 1.36 1999/12/08 11:36:40 angelos Exp $	*/
/*	$NetBSD: in_pcb.c,v 1.25 1996/02/13 23:41:53 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
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

#include <sys/param.h>
#ifndef __ECOS
#include <sys/systm.h>
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#ifdef __ECOS
#undef errno
#endif
#include <sys/time.h>
#ifndef __ECOS
#include <sys/proc.h>
#endif
#include <sys/domain.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifndef __ECOS
#include <dev/rndvar.h>
#endif

#ifdef INET6
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#ifdef IPSEC
#include <netinet/ip_ipsp.h>

extern int	check_ipsec_policy  __P((struct inpcb *, u_int32_t));
#endif

#if 0 /*KAME IPSEC*/
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#include <netkey/key_debug.h>
#endif /* IPSEC */

struct	in_addr zeroin_addr;

extern int ipsec_auth_default_level;
extern int ipsec_esp_trans_default_level;
extern int ipsec_esp_network_default_level;

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
int ipport_firstauto = IPPORT_RESERVED;		/* 1024 */
int ipport_lastauto = IPPORT_USERRESERVED;	/* 5000 */
int ipport_hifirstauto = IPPORT_HIFIRSTAUTO;	/* 40000 */
int ipport_hilastauto = IPPORT_HILASTAUTO;	/* 44999 */

#define	INPCBHASH(table, faddr, fport, laddr, lport) \
	&(table)->inpt_hashtbl[(ntohl((faddr)->s_addr) + \
	ntohs((fport)) + ntohs((lport))) & (table->inpt_hash)]

#define	IN6PCBHASH(table, faddr, fport, laddr, lport) \
	&(table)->inpt_hashtbl[(ntohl((faddr)->s6_addr32[0] ^ \
	(faddr)->s6_addr32[3]) + ntohs((fport)) + ntohs((lport))) & \
	(table->inpt_hash)]

void
in_pcbinit(table, hashsize)
	struct inpcbtable *table;
	int hashsize;
{

	CIRCLEQ_INIT(&table->inpt_queue);
	table->inpt_hashtbl = hashinit(hashsize, M_PCB, M_WAITOK, &table->inpt_hash);
	table->inpt_lastport = 0;
}

struct baddynamicports baddynamicports;
 
/*
 * Check if the specified port is invalid for dynamic allocation.
 */
int
in_baddynamic(port, proto)
	u_int16_t port;
	u_int16_t proto;
{

	if (port < IPPORT_RESERVED/2 || port >= IPPORT_RESERVED)
		return(0);

	switch (proto) {
	case IPPROTO_TCP:
		return (DP_ISSET(baddynamicports.tcp, port));
	case IPPROTO_UDP:
		return (DP_ISSET(baddynamicports.udp, port));
	default:
		return (0);
	}
}

int
in_pcballoc(so, v)
	struct socket *so;
	void *v;
{
	struct inpcbtable *table = v;
	register struct inpcb *inp;
	int s;

	MALLOC(inp, struct inpcb *, sizeof(*inp), M_PCB, M_NOWAIT);
	if (inp == NULL)
		return (ENOBUFS);
	bzero((caddr_t)inp, sizeof(*inp));
	inp->inp_table = table;
	inp->inp_socket = so;
	inp->inp_seclevel[SL_AUTH] = ipsec_auth_default_level;
	inp->inp_seclevel[SL_ESP_TRANS] = ipsec_esp_trans_default_level;
	inp->inp_seclevel[SL_ESP_NETWORK] = ipsec_esp_network_default_level;
	s = splnet();
	CIRCLEQ_INSERT_HEAD(&table->inpt_queue, inp, inp_queue);
	LIST_INSERT_HEAD(INPCBHASH(table, &inp->inp_faddr, inp->inp_fport,
	    &inp->inp_laddr, inp->inp_lport), inp, inp_hash);
	splx(s);
	so->so_pcb = inp;
	inp->inp_hops = -1;

#ifdef INET6
	/*
	 * Small change in this function to set the INP_IPV6 flag so routines
	 * outside pcb-specific routines don't need to use sotopf(), and all
	 * of it's pointer chasing, later.
	 */
	if (sotopf(so) == PF_INET6)
		inp->inp_flags = INP_IPV6;
	inp->inp_csumoffset = -1;
#endif /* INET6 */
	return (0);
}

int
in_pcbbind(v, nam)
	register void *v;
	struct mbuf *nam;
{
	register struct inpcb *inp = v;
	register struct socket *so = inp->inp_socket;
	register struct inpcbtable *table = inp->inp_table;
	u_int16_t *lastport = &inp->inp_table->inpt_lastport;
	register struct sockaddr_in *sin;
#ifndef __ECOS
	struct proc *p = curproc;		/* XXX */
	int error;
#endif
	u_int16_t lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);

#ifdef INET6
	if (sotopf(so) == PF_INET6)
		return in6_pcbbind(inp, nam);
#endif /* INET6 */

	if (in_ifaddr.tqh_first == 0)
		return (EADDRNOTAVAIL);
	if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	     (so->so_options & SO_ACCEPTCONN) == 0))
		wild = INPLOOKUP_WILDCARD;
	if (nam) {
		sin = mtod(nam, struct sockaddr_in *);
		if (nam->m_len != sizeof (*sin))
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
		if (IN_MULTICAST(sin->sin_addr.s_addr)) {
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
			if (in_iawithaddr(sin->sin_addr, NULL) == 0)
				return (EADDRNOTAVAIL);
		}
		if (lport) {
			struct inpcb *t;

			/* GROSS */
#ifndef __ECOS
			if (ntohs(lport) < IPPORT_RESERVED &&
			    (error = suser(p->p_ucred, &p->p_acflag)))
				return (EACCES);
#endif
			if (so->so_euid) {
				t = in_pcblookup(table, &zeroin_addr, 0,
				    &sin->sin_addr, lport, INPLOOKUP_WILDCARD);
				if (t && (so->so_euid != t->inp_socket->so_euid))
					return (EADDRINUSE);
			}
			t = in_pcblookup(table, &zeroin_addr, 0,
			    &sin->sin_addr, lport, wild);
			if (t && (reuseport & t->inp_socket->so_options) == 0)
				return (EADDRINUSE);
		}
		inp->inp_laddr = sin->sin_addr;
	}
	if (lport == 0) {
		u_int16_t first, last, old = 0;
		int count;
		int loopcount = 0;

		if (inp->inp_flags & INP_HIGHPORT) {
			first = ipport_hifirstauto;	/* sysctl */
			last = ipport_hilastauto;
		} else if (inp->inp_flags & INP_LOWPORT) {
#ifndef __ECOS
			if ((error = suser(p->p_ucred, &p->p_acflag)))
				return (EACCES);
#endif
			first = IPPORT_RESERVED-1; /* 1023 */
			last = 600;		   /* not IPPORT_RESERVED/2 */
		} else {
			first = ipport_firstauto;	/* sysctl */
			last  = ipport_lastauto;
		}

		/*
		 * Simple check to ensure all ports are not used up causing
		 * a deadlock here.
		 *
		 * We split the two cases (up and down) so that the direction
		 * is not being tested on each round of the loop.
		 */

portloop:
		if (first > last) {
			/*
			 * counting down
			 */
			if (loopcount == 0) {	/* only do this once. */
				old = first;
				first -= (arc4random() % (first - last));
			}
			count = first - last;
			*lastport = first;		/* restart each time */

			do {
				if (count-- <= 0) {	/* completely used? */
					if (loopcount == 0) {
						last = old;
						loopcount++;
						goto portloop;
					}
					return (EADDRNOTAVAIL);
				}
				--*lastport;
				if (*lastport > first || *lastport < last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_baddynamic(*lastport, so->so_proto->pr_protocol) ||
			    in_pcblookup(table, &zeroin_addr, 0,
			    &inp->inp_laddr, lport, wild));
		} else {
			/*
			 * counting up
			 */
			if (loopcount == 0) {	/* only do this once. */
				old = first;
				first += (arc4random() % (last - first));
			}
			count = last - first;
			*lastport = first;		/* restart each time */

			do {
				if (count-- <= 0) {	/* completely used? */
					if (loopcount == 0) {
						first = old;
						loopcount++;
						goto portloop;
					}
					return (EADDRNOTAVAIL);
				}
				++*lastport;
				if (*lastport < first || *lastport > last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_baddynamic(*lastport, so->so_proto->pr_protocol) ||
			    in_pcblookup(table, &zeroin_addr, 0,
			    &inp->inp_laddr, lport, wild));
		}
	}
	inp->inp_lport = lport;
	in_pcbrehash(inp);
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(v, nam)
	register void *v;
	struct mbuf *nam;
{
	register struct inpcb *inp = v;
	struct sockaddr_in *ifaddr = NULL;
	register struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

#ifdef INET6
	if (sotopf(inp->inp_socket) == PF_INET6)
		return (in6_pcbconnect(inp, nam));
#endif /* INET6 */

	if (nam->m_len != sizeof (*sin))
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	if (in_ifaddr.tqh_first != 0) {
		/*
		 * If the destination address is INADDR_ANY,
		 * use the primary local address.
		 * If the supplied address is INADDR_BROADCAST,
		 * and the primary interface supports broadcast,
		 * choose the broadcast address for that interface.
		 */
		if (sin->sin_addr.s_addr == INADDR_ANY)
			sin->sin_addr = in_ifaddr.tqh_first->ia_addr.sin_addr;
		else if (sin->sin_addr.s_addr == INADDR_BROADCAST &&
		  (in_ifaddr.tqh_first->ia_ifp->if_flags & IFF_BROADCAST))
			sin->sin_addr = in_ifaddr.tqh_first->ia_broadaddr.sin_addr;
	}
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
#if 0
		register struct route *ro;
		struct sockaddr_in *sin2;
		struct in_ifaddr *ia;

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
			satosin(&ro->ro_dst)->sin_addr = sin->sin_addr;
			rtalloc(ro);

			/*
			 * It is important to bzero out the rest of the
			 * struct sockaddr_in when mixing v6 & v4!
			 */
			sin2 = (struct sockaddr_in *)&ro->ro_dst;
			bzero(sin2->sin_zero, sizeof(sin2->sin_zero));
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
			u_int16_t fport = sin->sin_port;

			sin->sin_port = 0;
			ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
			if (ia == 0)
				ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
			sin->sin_port = fport;
			if (ia == 0)
				ia = in_ifaddr.tqh_first;
			if (ia == 0)
				return (EADDRNOTAVAIL);
		}
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(sin->sin_addr.s_addr) &&
#ifdef INET6
		    inp->inp_moptions != NULL &&
		    !(inp->inp_flags & INP_IPV6_MCAST))
#else
		    inp->inp_moptions != NULL)
#endif
		{
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				for (ia = in_ifaddr.tqh_first; ia != 0;
				    ia = ia->ia_list.tqe_next)
					if (ia->ia_ifp == ifp)
						break;
				if (ia == 0)
					return (EADDRNOTAVAIL);
			}
		}
		ifaddr = satosin(&ia->ia_addr);
#else
		int error;
		ifaddr = in_selectsrc(sin, &inp->inp_route,
			inp->inp_socket->so_options, inp->inp_moptions, &error);
		if (ifaddr == NULL) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return error;
		}
#endif
	}
	if (in_pcbhashlookup(inp->inp_table, sin->sin_addr, sin->sin_port,
	    inp->inp_laddr.s_addr ? inp->inp_laddr : ifaddr->sin_addr,
	    inp->inp_lport) != 0)
		return (EADDRINUSE);
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		if (inp->inp_lport == 0 &&
		    in_pcbbind(inp, (struct mbuf *)0) == EADDRNOTAVAIL)
			return (EADDRNOTAVAIL);
		inp->inp_laddr = ifaddr->sin_addr;
	}
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	in_pcbrehash(inp);
#ifdef IPSEC
	return (check_ipsec_policy(inp, 0));
#else
	return (0);
#endif
}

void
in_pcbdisconnect(v)
	void *v;
{
	struct inpcb *inp = v;

#ifdef INET6
	if (sotopf(inp->inp_socket) == PF_INET6) {
		inp->inp_faddr6 = in6addr_any;
		/* Disconnected AF_INET6 sockets cannot be "v4-mapped" */
		inp->inp_flags &= ~INP_IPV6_MAPPED;
	} else
#endif
		inp->inp_faddr.s_addr = INADDR_ANY;

	inp->inp_fport = 0;
	in_pcbrehash(inp);
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
}

void
in_pcbdetach(v)
	void *v;
{
	struct inpcb *inp = v;
	struct socket *so = inp->inp_socket;
	int s;

#if 0 /*KAME IPSEC*/
	if (so->so_pcb) {
		KEYDEBUG(KEYDEBUG_KEY_STAMP,
			printf("DP call free SO=%p from in_pcbdetach\n", so));
		key_freeso(so);
	}
	ipsec4_delete_pcbpolicy(inp);
#endif /*IPSEC*/
	so->so_pcb = 0;
	sofree(so);
	if (inp->inp_options)
		(void)m_freem(inp->inp_options);
	if (inp->inp_route.ro_rt)
		rtfree(inp->inp_route.ro_rt);
#ifdef INET6
	if (inp->inp_flags & INP_IPV6)
		ip6_freemoptions(inp->inp_moptions6);
	else 
#endif
		ip_freemoptions(inp->inp_moptions);
#ifdef IPSEC
	/* XXX IPsec cleanup here */
	s = spltdb();
	if (inp->inp_tdb)
		TAILQ_REMOVE(&inp->inp_tdb->tdb_inp, inp, inp_tdb_next);
	splx(s);
#endif
	s = splnet();
	LIST_REMOVE(inp, inp_hash);
	CIRCLEQ_REMOVE(&inp->inp_table->inpt_queue, inp, inp_queue);
	splx(s);
	FREE(inp, M_PCB);
}

void
in_setsockaddr(inp, nam)
	register struct inpcb *inp;
	struct mbuf *nam;
{
	register struct sockaddr_in *sin;
	
	nam->m_len = sizeof (*sin);
	sin = mtod(nam, struct sockaddr_in *);
	bzero((caddr_t)sin, sizeof (*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_lport;
	sin->sin_addr = inp->inp_laddr;
}

void
in_setpeeraddr(inp, nam)
	struct inpcb *inp;
	struct mbuf *nam;
{
	register struct sockaddr_in *sin;
	
#ifdef INET6
	if (sotopf(inp->inp_socket) == PF_INET6)
		in6_setpeeraddr(inp, nam);
#endif /* INET6 */

	nam->m_len = sizeof (*sin);
	sin = mtod(nam, struct sockaddr_in *);
	bzero((caddr_t)sin, sizeof (*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_fport;
	sin->sin_addr = inp->inp_faddr;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splsoftnet.
 */
void
in_pcbnotify(table, dst, fport_arg, laddr, lport_arg, errno, notify)
	struct inpcbtable *table;
	struct sockaddr *dst;
	u_int fport_arg, lport_arg;
	struct in_addr laddr;
	int errno;
	void (*notify) __P((struct inpcb *, int));
{
	register struct inpcb *inp, *oinp;
	struct in_addr faddr;
	u_int16_t fport = fport_arg, lport = lport_arg;

#ifdef INET6
	/*
	 * See in6_pcbnotify() for IPv6 codepath.  By the time this
	 * gets called, the addresses passed are either definitely IPv4 or
	 * IPv6; *_pcbnotify() never gets called with v4-mapped v6 addresses.
	 */
#endif /* INET6 */

	if (dst->sa_family != AF_INET)
		return;
	faddr = satosin(dst)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return;

	for (inp = table->inpt_queue.cqh_first;
	    inp != (struct inpcb *)&table->inpt_queue;) {
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == 0 ||
		    inp->inp_fport != fport ||
		    inp->inp_lport != lport ||
		    inp->inp_laddr.s_addr != laddr.s_addr) {
			inp = inp->inp_queue.cqe_next;
			continue;
		}
		oinp = inp;
		inp = inp->inp_queue.cqe_next;
		if (notify)
			(*notify)(oinp, errno);
	}
}

void
in_pcbnotifyall(table, dst, errno, notify)
	struct inpcbtable *table;
	struct sockaddr *dst;
	int errno;
	void (*notify) __P((struct inpcb *, int));
{
	register struct inpcb *inp, *oinp;
	struct in_addr faddr;

#ifdef INET6
	/*
	 * See in6_pcbnotify() for IPv6 codepath.  By the time this
	 * gets called, the addresses passed are either definitely IPv4 or
	 * IPv6; *_pcbnotify() never gets called with v4-mapped v6 addresses.
	 */
#endif /* INET6 */

	if (dst->sa_family != AF_INET)
		return;
	faddr = satosin(dst)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return;

	for (inp = table->inpt_queue.cqh_first;
	    inp != (struct inpcb *)&table->inpt_queue;) {
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == 0) {
			inp = inp->inp_queue.cqe_next;
			continue;
		}
		oinp = inp;
		inp = inp->inp_queue.cqe_next;
		if (notify)
			(*notify)(oinp, errno);
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
		inp->inp_route.ro_rt = 0;
		bzero((caddr_t)&info, sizeof(info));
		info.rti_info[RTAX_DST] = &inp->inp_route.ro_dst;
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
		if (rt->rt_flags & RTF_DYNAMIC)
			(void) rtrequest(RTM_DELETE, rt_key(rt),
				rt->rt_gateway, rt_mask(rt), rt->rt_flags, 
				(struct rtentry **)0);
		else 
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 */
			rtfree(rt);
	}
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
in_rtchange(inp, errno)
	register struct inpcb *inp;
	int errno;
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

struct inpcb *
in_pcblookup(table, faddrp, fport_arg, laddrp, lport_arg, flags)
	struct inpcbtable *table;
	void *faddrp, *laddrp;
	u_int fport_arg, lport_arg;
	int flags;
{
	register struct inpcb *inp, *match = 0;
	int matchwild = 3, wildcard;
	u_int16_t fport = fport_arg, lport = lport_arg;
	struct in_addr faddr = *(struct in_addr *)faddrp;
	struct in_addr laddr = *(struct in_addr *)laddrp;

	for (inp = table->inpt_queue.cqh_first;
	    inp != (struct inpcb *)&table->inpt_queue;
	    inp = inp->inp_queue.cqe_next) {
		if (inp->inp_lport != lport)
			continue;
		wildcard = 0;
#ifdef INET6
		if (flags & INPLOOKUP_IPV6) {
			struct in6_addr *laddr6 = (struct in6_addr *)laddrp;
			struct in6_addr *faddr6 = (struct in6_addr *)faddrp;

			/* 
			 * Always skip AF_INET sockets when looking
			 * for AF_INET6 addresses.  The only problem
			 * with this comes if the PF_INET6 addresses
			 * are v4-mapped addresses.  From what I've
			 * been able to see, none of the callers cause
			 * such a situation to occur.  If such a
			 * situation DID occur, then it is possible to
			 * miss a matching PCB.
			 */
			if (!(inp->inp_flags & INP_IPV6))
				continue;

			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)) {
				if (IN6_IS_ADDR_UNSPECIFIED(laddr6))
					wildcard++;
				else if (!IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, laddr6))
					continue;
			} else {
				if (!IN6_IS_ADDR_UNSPECIFIED(laddr6))
					wildcard++;
			}

			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6)) {
				if (IN6_IS_ADDR_UNSPECIFIED(faddr6))
					wildcard++;
				else if (!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6,
				    faddr6) || inp->inp_fport != fport)
					continue;
			} else {
				if (!IN6_IS_ADDR_UNSPECIFIED(faddr6))
					wildcard++;
			}
		} else
#endif /* INET6 */
		{
			if (inp->inp_faddr.s_addr != INADDR_ANY) {
				if (faddr.s_addr == INADDR_ANY)
					wildcard++;
				else if (inp->inp_faddr.s_addr != faddr.s_addr ||
				    inp->inp_fport != fport)
					continue;
			} else {
				if (faddr.s_addr != INADDR_ANY)
					wildcard++;
			}
			if (inp->inp_laddr.s_addr != INADDR_ANY) {
				if (laddr.s_addr == INADDR_ANY)
					wildcard++;
				else if (inp->inp_laddr.s_addr != laddr.s_addr)
					continue;
			} else {
				if (laddr.s_addr != INADDR_ANY)
					wildcard++;
			}
		}
		if ((!wildcard || (flags & INPLOOKUP_WILDCARD)) &&
		    wildcard < matchwild) {
			match = inp;
			if ((matchwild = wildcard) == 0)
				break;
		}
	}
	return (match);
}

struct sockaddr_in *
in_selectsrc(sin, ro, soopts, mopts, errorp)
	struct sockaddr_in *sin;
	struct route *ro;
	int soopts;
	struct ip_moptions *mopts;
	int *errorp;
{
	struct sockaddr_in *sin2;
	struct in_ifaddr *ia;

	ia = (struct in_ifaddr *)0;
	/* 
	 * If route is known or can be allocated now,
	 * our src addr is taken from the i/f, else punt.
	 */
	if (ro->ro_rt &&
	    (satosin(&ro->ro_dst)->sin_addr.s_addr !=
		sin->sin_addr.s_addr || 
	    soopts & SO_DONTROUTE)) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if ((soopts & SO_DONTROUTE) == 0 && /*XXX*/
	    (ro->ro_rt == (struct rtentry *)0 ||
	    ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
		/* No route yet, so try to acquire one */
		ro->ro_dst.sa_family = AF_INET;
		ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
		satosin(&ro->ro_dst)->sin_addr = sin->sin_addr;
		rtalloc(ro);

		/*
		 * It is important to bzero out the rest of the
		 * struct sockaddr_in when mixing v6 & v4!
		 */
		sin2 = (struct sockaddr_in *)&ro->ro_dst;
		bzero(sin2->sin_zero, sizeof(sin2->sin_zero));
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
		u_int16_t fport = sin->sin_port;

		sin->sin_port = 0;
		ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
		if (ia == 0)
			ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
		sin->sin_port = fport;
		if (ia == 0)
			ia = in_ifaddr.tqh_first;
		if (ia == 0) {
			*errorp = EADDRNOTAVAIL;
			return NULL;
		}
	}
	/*
	 * If the destination address is multicast and an outgoing
	 * interface has been set as a multicast option, use the
	 * address of that interface as our source address.
	 */
	if (IN_MULTICAST(sin->sin_addr.s_addr) &&
#if 0 /*def INET6*/
	    mopts != NULL &&
	    !(inp->inp_flags & INP_IPV6_MCAST))
#else
	    mopts != NULL)
#endif
	{
		struct ip_moptions *imo;
		struct ifnet *ifp;

		imo = mopts;
		if (imo->imo_multicast_ifp != NULL) {
			ifp = imo->imo_multicast_ifp;
			for (ia = in_ifaddr.tqh_first; ia != 0;
			    ia = ia->ia_list.tqe_next)
				if (ia->ia_ifp == ifp)
					break;
			if (ia == 0) {
				*errorp = EADDRNOTAVAIL;
				return NULL;
			}
		}
	}
	return satosin(&ia->ia_addr);
}

void
in_pcbrehash(inp)
	struct inpcb *inp;
{
	struct inpcbtable *table = inp->inp_table;
	int s;

	s = splnet();
	LIST_REMOVE(inp, inp_hash);
#ifdef INET6
	if (inp->inp_flags & INP_IPV6) {
		LIST_INSERT_HEAD(IN6PCBHASH(table, &inp->inp_faddr6,
		    inp->inp_fport, &inp->inp_laddr6, inp->inp_lport),
		    inp, inp_hash);
	} else {
#endif /* INET6 */
		LIST_INSERT_HEAD(INPCBHASH(table, &inp->inp_faddr,
		    inp->inp_fport, &inp->inp_laddr, inp->inp_lport),
		    inp, inp_hash);
#ifdef INET6
	}
#endif /* INET6 */
	splx(s);
}

#ifdef DIAGNOSTIC
int	in_pcbnotifymiss = 0;
#endif

struct inpcb *
in_pcbhashlookup(table, faddr, fport_arg, laddr, lport_arg)
	struct inpcbtable *table;
	struct in_addr faddr, laddr;
	u_int fport_arg, lport_arg;
{
	struct inpcbhead *head;
	register struct inpcb *inp;
	u_int16_t fport = fport_arg, lport = lport_arg;

	head = INPCBHASH(table, &faddr, fport, &laddr, lport);
	for (inp = head->lh_first; inp != NULL; inp = inp->inp_hash.le_next) {
		if (inp->inp_faddr.s_addr == faddr.s_addr &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport &&
		    inp->inp_laddr.s_addr == laddr.s_addr) {
			/*
			 * Move this PCB to the head of hash chain so that
			 * repeated accesses are quicker.  This is analogous to
			 * the historic single-entry PCB cache.
			 */
			if (inp != head->lh_first) {
				LIST_REMOVE(inp, inp_hash);
				LIST_INSERT_HEAD(head, inp, inp_hash);
			}
			break;
		}
	}
#ifdef DIAGNOSTIC
	if (inp == NULL && in_pcbnotifymiss) {
		printf("in_pcbhashlookup: faddr=%08x fport=%d laddr=%08x lport=%d\n",
		    ntohl(faddr.s_addr), ntohs(fport),
		    ntohl(laddr.s_addr), ntohs(lport));
	}
#endif
	return (inp);
}

#ifdef INET6
struct inpcb *
in6_pcbhashlookup(table, faddr, fport_arg, laddr, lport_arg)
	struct inpcbtable *table;
	struct in6_addr *faddr, *laddr;
	u_int fport_arg, lport_arg;
{
	struct inpcbhead *head;
	register struct inpcb *inp;
	u_int16_t fport = fport_arg, lport = lport_arg;

	head = IN6PCBHASH(table, faddr, fport, laddr, lport);
	for (inp = head->lh_first; inp != NULL; inp = inp->inp_hash.le_next) {
		if (!(inp->inp_flags & INP_IPV6))
			continue;
		if (IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6, faddr) &&
		    inp->inp_fport == fport && inp->inp_lport == lport &&
		    IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, laddr)) {
			/*
			 * Move this PCB to the head of hash chain so that
			 * repeated accesses are quicker.  This is analogous to
			 * the historic single-entry PCB cache.
			 */
			if (inp != head->lh_first) {
				LIST_REMOVE(inp, inp_hash);
				LIST_INSERT_HEAD(head, inp, inp_hash);
			}
			break;
		}
	}
#ifdef DIAGNOSTIC
	if (inp == NULL && in_pcbnotifymiss) {
		printf("in6_pcblookup_connect: faddr=");
		printf(" fport=%d laddr=", ntohs(fport));
		printf(" lport=%d\n", ntohs(lport));
	}
#endif
	return (inp);
}
#endif /* INET6 */


