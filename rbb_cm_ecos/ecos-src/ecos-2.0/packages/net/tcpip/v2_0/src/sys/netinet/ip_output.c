//==========================================================================
//
//      sys/netinet/ip_output.c
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


/*	$OpenBSD: ip_output.c,v 1.57 1999/12/10 08:55:23 angelos Exp $	*/
/*	$NetBSD: ip_output.c,v 1.28 1996/02/13 23:43:07 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)ip_output.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#ifndef __ECOS
#include <sys/systm.h>
#endif
#include <sys/kernel.h>
#ifndef __ECOS
#include <sys/proc.h>

#include <vm/vm.h>
#include <sys/proc.h>
#endif

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef vax
#include <machine/mtpr.h>
#endif

#include <machine/stdarg.h>

#ifdef IPSEC
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/pfkeyv2.h>

#ifdef ENCDEBUG
#define DPRINTF(x)    do { if (encdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

extern u_int8_t get_sa_require  __P((struct inpcb *));

#endif /* IPSEC */

static struct mbuf *ip_insertoptions __P((struct mbuf *, struct mbuf *, int *));
static void ip_mloopback
	__P((struct ifnet *, struct mbuf *, struct sockaddr_in *));
#if defined(IPFILTER) || defined(IPFILTER_LKM)
int (*fr_checkp) __P((struct ip *, int, struct ifnet *, int, struct mbuf **));
#endif

#ifdef IPSEC
extern int ipsec_auth_default_level;
extern int ipsec_esp_trans_default_level;
extern int ipsec_esp_network_default_level;

extern int pfkeyv2_acquire(struct tdb *, int);
#endif

#ifndef RAMDOM_IP_ID
u_short ip_id;
#endif

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
#if __STDC__
ip_output(struct mbuf *m0, ...)
#else
ip_output(m0, va_alist)
	struct mbuf *m0;
	va_dcl
#endif
{
	register struct ip *ip, *mhip;
	register struct ifnet *ifp;
	register struct mbuf *m = m0;
	register int hlen = sizeof (struct ip);
	int len, off, error = 0;
	struct route iproute;
	struct sockaddr_in *dst;
	struct in_ifaddr *ia;
	struct mbuf *opt;
	struct route *ro;
	int flags;
	struct ip_moptions *imo;
	va_list ap;
#ifdef IPSEC
	union sockaddr_union sunion;
	struct mbuf *mp;
	struct udphdr *udp;
	struct tcphdr *tcp;
	struct inpcb *inp;

	struct route_enc re0, *re = &re0;
	struct sockaddr_encap *ddst, *gw;
	u_int8_t sa_require, sa_have = 0;
	struct tdb *tdb, *t;
	int s, ip6flag;

#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */
#endif /* IPSEC */

	va_start(ap, m0);
	opt = va_arg(ap, struct mbuf *);
	ro = va_arg(ap, struct route *);
	flags = va_arg(ap, int);
	imo = va_arg(ap, struct ip_moptions *);
#ifdef IPSEC
	inp = va_arg(ap, struct inpcb *);
#endif /* IPSEC */
	va_end(ap);

#ifdef	DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("ip_output no HDR");
#endif
	if (opt) {
		m = ip_insertoptions(m, opt, &len);
		hlen = len;
	}
	ip = mtod(m, struct ip *);
	/*
	 * Fill in IP header.
	 */
	if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) {
		ip->ip_v = IPVERSION;
		ip->ip_off &= IP_DF;
#ifdef RANDOM_IP_ID
		ip->ip_id = ip_randomid();
#else
		ip->ip_id = htons(ip_id++);
#endif
		ip->ip_hl = hlen >> 2;
		ipstat.ips_localout++;
	} else {
		hlen = ip->ip_hl << 2;
	}

	/*
	 * Route packet.
	 */
	if (ro == 0) {
		ro = &iproute;
		bzero((caddr_t)ro, sizeof (*ro));
	}
	dst = satosin(&ro->ro_dst);
	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up.  If not, free it and try again.
	 */
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
	    dst->sin_addr.s_addr != ip->ip_dst.s_addr)) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if (ro->ro_rt == 0) {
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_dst;
	}
	/*
	 * If routing to interface only,
	 * short circuit routing lookup.
	 */
	if (flags & IP_ROUTETOIF) {
		if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == 0 &&
		    (ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == 0) {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		ifp = ia->ia_ifp;
		ip->ip_ttl = 1;
	} else {
		if (ro->ro_rt == 0)
			rtalloc(ro);
		if (ro->ro_rt == 0) {
			ipstat.ips_noroute++;
			error = EHOSTUNREACH;
			goto bad;
		}
		ia = ifatoia(ro->ro_rt->rt_ifa);
		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_use++;
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin(ro->ro_rt->rt_gateway);
	}
	if (IN_MULTICAST(ip->ip_dst.s_addr)) {
		struct in_multi *inm;

		m->m_flags |= M_MCAST;
		/*
		 * IP destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = satosin(&ro->ro_dst);
		/*
		 * See if the caller provided any multicast options
		 */
		if (imo != NULL) {
			ip->ip_ttl = imo->imo_multicast_ttl;
			if (imo->imo_multicast_ifp != NULL)
				ifp = imo->imo_multicast_ifp;
		} else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;
		/*
		 * Confirm that the outgoing interface supports multicast.
		 */
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		/*
		 * If source address not specified yet, use address
		 * of outgoing interface.
		 */
		if (ip->ip_src.s_addr == INADDR_ANY) {
			register struct in_ifaddr *ia;

			for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next)
				if (ia->ia_ifp == ifp) {
					ip->ip_src = ia->ia_addr.sin_addr;
					break;
				}
		}

		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);
		if (inm != NULL &&
		   (imo == NULL || imo->imo_multicast_loop)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 */
			ip_mloopback(ifp, m, dst);
		}
#ifdef MROUTING
		else {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IP_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip_mloopback(),
			 * above, will be forwarded by the ip_input() routine,
			 * if necessary.
			 */
			extern struct socket *ip_mrouter;

			if (ip_mrouter && (flags & IP_FORWARDING) == 0) {
				if (ip_mforward(m, ifp) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}
#endif
		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip_ttl == 0 || (ifp->if_flags & IFF_LOOPBACK) != 0) {
			m_freem(m);
			goto done;
		}

		goto sendit;
	}
#ifndef notdef
	/*
	 * If source address not specified yet, use address
	 * of outgoing interface.
	 */
	if (ip->ip_src.s_addr == INADDR_ANY)
		ip->ip_src = ia->ia_addr.sin_addr;
#endif
	/*
	 * Look for broadcast address and
	 * and verify user is allowed to send
	 * such a packet.
	 */
	if (in_broadcast(dst->sin_addr, ifp)) {
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IP_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
		/* don't allow broadcast messages to be fragmented */
		if ((u_int16_t)ip->ip_len > ifp->if_mtu) {
			error = EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} else
		m->m_flags &= ~M_BCAST;

sendit:
#ifdef IPSEC
	/*
	 * Check if the packet needs encapsulation.
	 */
	if (!(flags & IP_ENCAPSULATED) &&
	    (inp == NULL || 
	     inp->inp_seclevel[SL_AUTH] != IPSEC_LEVEL_BYPASS ||
	     inp->inp_seclevel[SL_ESP_TRANS] != IPSEC_LEVEL_BYPASS ||
	     inp->inp_seclevel[SL_ESP_NETWORK] != IPSEC_LEVEL_BYPASS)) {
		if (inp == NULL)
			sa_require = get_sa_require(inp);
		else
			sa_require = inp->inp_secrequire;

		bzero((caddr_t) re, sizeof(*re));

		/*
		 * splnet is chosen over spltdb because we are not allowed to
		 * lower the level, and udp_output calls us in splnet().
		 */
		s = splnet();

		/*
		 * Check if there was an outgoing SA bound to the flow
		 * from a transport protocol.
		 */
		if (inp && inp->inp_tdb &&
		    (inp->inp_tdb->tdb_dst.sin.sin_addr.s_addr == INADDR_ANY ||
		     !bcmp(&inp->inp_tdb->tdb_dst.sin.sin_addr,
			   &ip->ip_dst, sizeof(ip->ip_dst)))) {
			tdb = inp->inp_tdb;
			goto have_tdb;
		}

		if (!ipsec_in_use) {
			splx(s);
			goto no_encap;
		}

		ddst = (struct sockaddr_encap *) &re->re_dst;
		ddst->sen_family = PF_KEY;
		ddst->sen_len = SENT_IP4_LEN;
		ddst->sen_type = SENT_IP4;
		ddst->sen_ip_src = ip->ip_src;
		ddst->sen_ip_dst = ip->ip_dst;
		ddst->sen_proto = ip->ip_p;

		switch (ip->ip_p) {
		case IPPROTO_UDP:
			if (m->m_len < hlen + 2 * sizeof(u_int16_t)) {
				if ((m = m_pullup(m, hlen + 2 *
				    sizeof(u_int16_t))) == 0)
					return ENOBUFS;
				ip = mtod(m, struct ip *);
			}
			udp = (struct udphdr *) (mtod(m, u_char *) + hlen);
			ddst->sen_sport = ntohs(udp->uh_sport);
			ddst->sen_dport = ntohs(udp->uh_dport);
			break;

		case IPPROTO_TCP:
			if (m->m_len < hlen + 2 * sizeof(u_int16_t)) {
				if ((m = m_pullup(m, hlen + 2 *
				    sizeof(u_int16_t))) == 0)
					return ENOBUFS;
				ip = mtod(m, struct ip *);
			}
			tcp = (struct tcphdr *) (mtod(m, u_char *) + hlen);
			ddst->sen_sport = ntohs(tcp->th_sport);
			ddst->sen_dport = ntohs(tcp->th_dport);
			break;

		default:
			ddst->sen_sport = 0;
			ddst->sen_dport = 0;
		}

		rtalloc((struct route *) re);
		if (re->re_rt == NULL) {
			splx(s);
			goto no_encap;
		}

		gw = (struct sockaddr_encap *) (re->re_rt->rt_gateway);

		/* Sanity check */
		if (gw == NULL || ((gw->sen_type != SENT_IPSP) &&
				   (gw->sen_type != SENT_IPSP6))) {
			splx(s);
		        DPRINTF(("ip_output(): no gw or gw data not IPSP\n"));

			if (re->re_rt)
				RTFREE(re->re_rt);
			error = EHOSTUNREACH;
			m_freem(m);
			goto done;
		}

		/*
		 * There might be a specific route, that tells us to avoid
		 * doing IPsec; this is useful for specific routes that we
		 * don't want to have IPsec applied on, like the key
		 * management ports.
		 */

		if ((gw != NULL) && (gw->sen_ipsp_sproto == 0) &&
		    (gw->sen_ipsp_spi == 0)) {
		    if ((gw->sen_family == AF_INET) &&
			(gw->sen_ipsp_dst.s_addr == 0)) {
			splx(s);
			goto no_encap;
		    }

#ifdef INET6
		    if ((gw->sen_family == AF_INET6) &&
			IN6_IS_ADDR_UNSPECIFIED(&gw->sen_ipsp6_dst)) {
			splx(s);
			goto no_encap;
		    }
#endif /* INET6 */
		}

		/*
		 * At this point we have an IPSP "gateway" (tunnel) spec.
		 * Use the destination of the tunnel and the SPI to
		 * look up the necessary Tunnel Control Block. Look it up,
		 * and then pass it, along with the packet and the gw,
		 * to the appropriate transformation.
		 */
		bzero(&sunion, sizeof(sunion));

		if (gw->sen_type == SENT_IPSP) {
		    sunion.sin.sin_family = AF_INET;
		    sunion.sin.sin_len = sizeof(struct sockaddr_in);
		    sunion.sin.sin_addr = gw->sen_ipsp_dst;
		}
#ifdef INET6
		if (gw->sen_type == SENT_IPSP6) {
		    sunion.sin6.sin6_family = AF_INET6;
		    sunion.sin6.sin6_len = sizeof(struct sockaddr_in6);
		    sunion.sin6.sin6_addr = gw->sen_ipsp6_dst;
		}
#endif /* INET6 */

		tdb = (struct tdb *) gettdb(gw->sen_ipsp_spi, &sunion,
					    gw->sen_ipsp_sproto);

		/* 
		 * For VPNs a route with a reserved SPI is used to
		 * indicate the need for an SA when none is established.
		 */
		if (((ntohl(gw->sen_ipsp_spi) == SPI_LOCAL_USE) &&
		     (gw->sen_type == SENT_IPSP)) ||
		    ((ntohl(gw->sen_ipsp6_spi) == SPI_LOCAL_USE) &&
		     (gw->sen_type == SENT_IPSP6))) {
		    if (tdb == NULL) {
			/*
			 * XXX We should construct a TDB from system
			 * default (which should be tunable via sysctl).
			 * For now, drop packet and ignore SPD entry.
			 */
			splx(s);
			goto no_encap;
		    }
		    else {
			if (tdb->tdb_authalgxform)
			  sa_require = NOTIFY_SATYPE_AUTH;
			if (tdb->tdb_encalgxform)
			  sa_require |= NOTIFY_SATYPE_CONF;
			if (tdb->tdb_flags & TDBF_TUNNELING)
			  sa_require |= NOTIFY_SATYPE_TUNNEL;
		    }

		    /* PF_KEYv2 notification message */
		    if (tdb && tdb->tdb_satype != SADB_X_SATYPE_BYPASS)
		            if ((error = pfkeyv2_acquire(tdb, 0)) != 0)
			            return error;

		    splx(s);

		    /* 
		     * When sa_require is set, the packet will be dropped
		     * at no_encap.
		     */
		    goto no_encap;
		}

	     have_tdb:

		ip->ip_len = htons((u_short) ip->ip_len);
		ip->ip_off = htons((u_short) ip->ip_off);
		ip->ip_sum = 0;

		/*
		 * Now we check if this tdb has all the transforms which
		 * are requried by the socket or our default policy.
		 */
		SPI_CHAIN_ATTRIB(sa_have, tdb_onext, tdb);

		if (sa_require & ~sa_have)
			goto no_encap;

		if (tdb == NULL) {
			splx(s);
			if (gw->sen_type == SENT_IPSP)
			  DPRINTF(("ip_output(): non-existant TDB for SA %s/%08x/%u\n", inet_ntoa4(gw->sen_ipsp_dst), ntohl(gw->sen_ipsp_spi), gw->sen_ipsp_sproto));
#ifdef INET6
			else
			  DPRINTF(("ip_output(): non-existant TDB for SA %s/%08x/%u\n", inet6_ntoa4(gw->sen_ipsp6_dst), ntohl(gw->sen_ipsp6_spi), gw->sen_ipsp6_sproto));
#endif /* INET6 */	  

			if (re->re_rt)
                        	RTFREE(re->re_rt);
			error = EHOSTUNREACH;
			m_freem(m);
			goto done;
		}

		for (t = tdb; t != NULL; t = t->tdb_onext)
		    if ((t->tdb_sproto == IPPROTO_ESP && !esp_enable) ||
			(t->tdb_sproto == IPPROTO_AH && !ah_enable)) {
		        DPRINTF(("ip_output(): IPSec outbound packet dropped due to policy\n"));

			if (re->re_rt)
                        	RTFREE(re->re_rt);
			error = EHOSTUNREACH;
			m_freem(m);
			goto done;
		    }

		while (tdb && tdb->tdb_xform) {
			/* Check if the SPI is invalid */
			if (tdb->tdb_flags & TDBF_INVALID) {
				splx(s);
			        DPRINTF(("ip_output(): attempt to use invalid SA %s/%08x/%u\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi), tdb->tdb_sproto));
				m_freem(m);
				if (re->re_rt)
					RTFREE(re->re_rt);
				return ENXIO;
			}

#ifndef INET6
			/* Sanity check */
			if (tdb->tdb_dst.sa.sa_family != AF_INET) {
			    splx(s);
			        DPRINTF(("ip_output(): attempt to use SA %s/%08x/%u for protocol family %d\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi), tdb->tdb_sproto, tdb->tdb_dst.sa.sa_family));
				m_freem(m);
				if (re->re_rt)
					RTFREE(re->re_rt);
				return ENXIO;
			}
#endif /* INET6 */

			/* Register first use, setup expiration timer */
			if (tdb->tdb_first_use == 0) {
				tdb->tdb_first_use = time.tv_sec;
				tdb_expiration(tdb, TDBEXP_TIMEOUT);
			}

			/* Check for tunneling */
			if (((tdb->tdb_dst.sa.sa_family == AF_INET) &&
			     (tdb->tdb_dst.sin.sin_addr.s_addr != 
			      INADDR_ANY) &&
			     (tdb->tdb_dst.sin.sin_addr.s_addr !=
			      ip->ip_dst.s_addr)) ||
			    (tdb->tdb_dst.sa.sa_family == AF_INET6) ||
			    ((tdb->tdb_flags & TDBF_TUNNELING) &&
			     (tdb->tdb_xform->xf_type != XF_IP4))) {
			        /* Fix length and checksum */
			        ip->ip_len = htons(m->m_pkthdr.len);
			        ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
				error = ipe4_output(m, tdb, &mp,
						    ip->ip_hl << 2,
						    offsetof(struct ip, ip_p));
				if (mp == NULL)
					error = EFAULT;
				if (error) {
					splx(s);
					if (re->re_rt)
						RTFREE(re->re_rt);
					return error;
				}
				if (tdb->tdb_dst.sa.sa_family == AF_INET)
				        ip6flag = 0;
#ifdef INET6
				if (tdb->tdb_dst.sa.sa_family == AF_INET6)
				        ip6flag = 1;
#endif /* INET6 */
				m = mp;
				mp = NULL;
			}

			if ((tdb->tdb_xform->xf_type == XF_IP4) &&
			    (tdb->tdb_dst.sa.sa_family == AF_INET)) {
			        ip = mtod(m, struct ip *);
				ip->ip_len = htons(m->m_pkthdr.len);
				ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
			}

#ifdef INET6
			if ((tdb->tdb_xform->xf_type == XF_IP4) &&
			    (tdb->tdb_dst.sa.sa_family == AF_INET6)) {
			    ip6 = mtod(m, struct ip6_hdr *);
			    ip6->ip6_plen = htons(m->m_pkthdr.len);
			}
#endif /* INET6 */

#ifdef INET6
			/*
			 * This assumes that there is only just an IPv6
			 * header prepended.
			 */
			if (ip6flag)
			  error = (*(tdb->tdb_xform->xf_output))(m, tdb, &mp, sizeof(struct ip6_hdr), offsetof(struct ip6_hdr, ip6_nxt));
#endif /* INET6 */

			if (!ip6flag)
			  error = (*(tdb->tdb_xform->xf_output))(m, tdb, &mp, ip->ip_hl << 2, offsetof(struct ip, ip_p));
			if (!error && mp == NULL)
				error = EFAULT;
			if (error) {
				splx(s);
				if (mp != NULL)
					m_freem(mp);
				if (re->re_rt)
					RTFREE(re->re_rt);
				return error;
			}

			m = mp;
			mp = NULL;

			if (!ip6flag) {
			    ip = mtod(m, struct ip *);
			    ip->ip_len = htons(m->m_pkthdr.len);
			}

#ifdef INET6
			if (ip6flag) {
			    ip6 = mtod(m, struct ip6_hdr *);
			    ip6->ip6_plen = htons(m->m_pkthdr.len);
			}
#endif /* INET6 */
			tdb = tdb->tdb_onext;
		}
		splx(s);

		if (!ip6flag)
		  ip->ip_sum = in_cksum(m, ip->ip_hl << 2);

		/*
		 * At this point, m is pointing to an mbuf chain with the
		 * processed packet. Call ourselves recursively, but
		 * bypass the encap code.
		 */
		if (re->re_rt)
			RTFREE(re->re_rt);

		if (!ip6flag) {
		    ip = mtod(m, struct ip *);
		    NTOHS(ip->ip_len);
		    NTOHS(ip->ip_off);

		    return ip_output(m, NULL, NULL,
				     IP_ENCAPSULATED | IP_RAWOUTPUT,
				     NULL, NULL);
		}

#ifdef INET6
		if (ip6flag) {
		    ip6 = mtod(m, struct ip6_hdr *);
		    NTOHS(ip6->ip6_plen);

		    /* Naturally, ip6_output() has to honor those two flags */
		    return ip6_output(m, NULL, NULL,
				     IP_ENCAPSULATED | IP_RAWOUTPUT,
				     NULL, NULL);
		}
#endif /* INET6 */

no_encap:
		/* This is for possible future use, don't move or delete */
		if (re->re_rt)
			RTFREE(re->re_rt);
		/* No IPSec processing though it was required, drop packet */
		if (sa_require) {
			error = EHOSTUNREACH;
			m_freem(m);
			goto done;
		}
	}
#endif /* IPSEC */

#if defined(IPFILTER) || defined(IPFILTER_LKM)
	/*
	 * looks like most checking has been done now...do a filter check
	 */
	{
		struct mbuf *m0 = m;
		if (fr_checkp && (*fr_checkp)(ip, hlen, ifp, 1, &m0)) {
			error = EHOSTUNREACH;
			goto done;
		} else
			ip = mtod(m = m0, struct ip *);
	}
#endif
	/*
	 * If small enough for interface, can just send directly.
	 */
	if ((u_int16_t)ip->ip_len <= ifp->if_mtu) {
		ip->ip_len = htons((u_int16_t)ip->ip_len);
		ip->ip_off = htons((u_int16_t)ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
		error = (*ifp->if_output)(ifp, m, sintosa(dst), ro->ro_rt);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
#if 0
	/*
	 * If IPsec packet is too big for the interface, try fragment it.
	 * XXX This really is a quickhack.  May be inappropriate.
	 * XXX fails if somebody is sending AH'ed packet, with:
	 *	sizeof(packet without AH) < mtu < sizeof(packet with AH)
	 */
	if (sab && ip->ip_p != IPPROTO_AH && (flags & IP_FORWARDING) == 0)
		ip->ip_off &= ~IP_DF;
#endif /*IPSEC*/
	if (ip->ip_off & IP_DF) {
		error = EMSGSIZE;
		ipstat.ips_cantfrag++;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

    {
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_nextpkt;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < (u_int16_t)ip->ip_len; off += len) {
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			ipstat.ips_odropped++;
			goto sendorfree;
		}
		*mnext = m;
		mnext = &m->m_nextpkt;
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= (u_int16_t)ip->ip_len)
			len = (u_int16_t)ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_int16_t)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			ipstat.ips_odropped++;
			goto sendorfree;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = (struct ifnet *)0;
		mhip->ip_off = htons((u_int16_t)mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		ipstat.ips_ofragments++;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m = m0;
	m_adj(m, hlen + firstlen - (u_int16_t)ip->ip_len);
	m->m_pkthdr.len = hlen + firstlen;
	ip->ip_len = htons((u_int16_t)m->m_pkthdr.len);
	ip->ip_off = htons((u_int16_t)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
		if (error == 0)
			error = (*ifp->if_output)(ifp, m, sintosa(dst),
			    ro->ro_rt);
		else
			m_freem(m);
	}

	if (error == 0)
		ipstat.ips_fragmented++;
    }
done:
	if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt)
		RTFREE(ro->ro_rt);
	return (error);
bad:
	m_freem(m0);
	goto done;
}

/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 */
static struct mbuf *
ip_insertoptions(m, opt, phlen)
	register struct mbuf *m;
	struct mbuf *opt;
	int *phlen;
{
	register struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	register struct ip *ip = mtod(m, struct ip *);
	unsigned optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + (u_int16_t)ip->ip_len > IP_MAXPACKET)
		return (m);		/* XXX should fail */
	if (p->ipopt_dst.s_addr)
		ip->ip_dst = p->ipopt_dst;
	if (m->m_flags & M_EXT || m->m_data - optlen < m->m_pktdat) {
		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n == 0)
			return (m);
		n->m_pkthdr.len = m->m_pkthdr.len + optlen;
		m->m_len -= sizeof(struct ip);
		m->m_data += sizeof(struct ip);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ip);
		m->m_data += max_linkhdr;
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	} else {
		m->m_data -= optlen;
		m->m_len += optlen;
		m->m_pkthdr.len += optlen;
		ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	}
	ip = mtod(m, struct ip *);
	bcopy((caddr_t)p->ipopt_list, (caddr_t)(ip + 1), (unsigned)optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_len += optlen;
	return (m);
}

/*
 * Copy options from ip to jp,
 * omitting those not copied during fragmentation.
 */
int
ip_optcopy(ip, jp)
	struct ip *ip, *jp;
{
	register u_char *cp, *dp;
	int opt, optlen, cnt;

	cp = (u_char *)(ip + 1);
	dp = (u_char *)(jp + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP) {
			/* Preserve for IP mcast tunnel's LSRR alignment. */
			*dp++ = IPOPT_NOP;
			optlen = 1;
			continue;
		} else
			optlen = cp[IPOPT_OLEN];
		/* bogus lengths should have been caught by ip_dooptions */
		if (optlen > cnt)
			optlen = cnt;
		if (IPOPT_COPIED(opt)) {
			bcopy((caddr_t)cp, (caddr_t)dp, (unsigned)optlen);
			dp += optlen;
		}
	}
	for (optlen = dp - (u_char *)(jp+1); optlen & 0x3; optlen++)
		*dp++ = IPOPT_EOL;
	return (optlen);
}

/*
 * IP socket option processing.
 */
int
ip_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	register struct inpcb *inp = sotoinpcb(so);
	register struct mbuf *m = *mp;
	register int optval = 0;
#ifdef IPSEC
	struct proc *p = curproc; /* XXX */
	struct tdb *tdb;
	struct tdb_ident *tdbip, tdbi;
	int s;
#endif
	int error = 0;

	if (level != IPPROTO_IP) {
		error = EINVAL;
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
	} else switch (op) {

	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
#ifdef notyet
		case IP_RETOPTS:
			return (ip_pcbopts(optname, &inp->inp_options, m));
#else
			return (ip_pcbopts(&inp->inp_options, m));
#endif

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
			if (m == NULL || m->m_len != sizeof(int))
				error = EINVAL;
			else {
				optval = *mtod(m, int *);
				switch (optname) {

				case IP_TOS:
					inp->inp_ip.ip_tos = optval;
					break;

				case IP_TTL:
					inp->inp_ip.ip_ttl = optval;
					break;
#define	OPTSET(bit) \
	if (optval) \
		inp->inp_flags |= bit; \
	else \
		inp->inp_flags &= ~bit;

				case IP_RECVOPTS:
					OPTSET(INP_RECVOPTS);
					break;

				case IP_RECVRETOPTS:
					OPTSET(INP_RECVRETOPTS);
					break;

				case IP_RECVDSTADDR:
					OPTSET(INP_RECVDSTADDR);
					break;
				}
			}
			break;
#undef OPTSET

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(optname, &inp->inp_moptions, m);
			break;

		case IP_PORTRANGE:
			if (m == 0 || m->m_len != sizeof(int))
				error = EINVAL;
			else {
				optval = *mtod(m, int *);

				switch (optval) {

				case IP_PORTRANGE_DEFAULT:
					inp->inp_flags &= ~(INP_LOWPORT);
					inp->inp_flags &= ~(INP_HIGHPORT);
					break;

				case IP_PORTRANGE_HIGH:
					inp->inp_flags &= ~(INP_LOWPORT);
					inp->inp_flags |= INP_HIGHPORT;
					break;

				case IP_PORTRANGE_LOW:
					inp->inp_flags &= ~(INP_HIGHPORT);
					inp->inp_flags |= INP_LOWPORT;
					break;

				default:

					error = EINVAL;
					break;
				}
			}
			break;
		case IPSEC_OUTSA:
#ifndef IPSEC
			error = EINVAL;
#else
			s = spltdb();
			if (m == 0 || m->m_len != sizeof(struct tdb_ident)) {
				error = EINVAL;
			} else {
				tdbip = mtod(m, struct tdb_ident *);
				tdb = gettdb(tdbip->spi, &tdbip->dst,
				    tdbip->proto);
				if (tdb == NULL)
					error = ESRCH;
				else
					tdb_add_inp(tdb, inp);
			}
			splx(s);
#endif /* IPSEC */
			break;

		case IP_AUTH_LEVEL:
		case IP_ESP_TRANS_LEVEL:
		case IP_ESP_NETWORK_LEVEL:
#ifndef IPSEC
			error = EINVAL;
#else
			if (m == 0 || m->m_len != sizeof(int)) {
				error = EINVAL;
				break;
			}
			optval = *mtod(m, u_char *);

			if (optval < IPSEC_LEVEL_BYPASS || 
			    optval > IPSEC_LEVEL_UNIQUE) {
				error = EINVAL;
				break;
			}
				
			switch (optname) {
			case IP_AUTH_LEVEL:
			        if (optval < ipsec_auth_default_level &&
				    suser(p->p_ucred, &p->p_acflag)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_AUTH] = optval;
				break;

			case IP_ESP_TRANS_LEVEL:
			        if (optval < ipsec_esp_trans_default_level &&
				    suser(p->p_ucred, &p->p_acflag)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_ESP_TRANS] = optval;
				break;

			case IP_ESP_NETWORK_LEVEL:
			        if (optval < ipsec_esp_network_default_level &&
				    suser(p->p_ucred, &p->p_acflag)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_ESP_NETWORK] = optval;
				break;
			}
			if (!error)
				inp->inp_secrequire = get_sa_require(inp);
#endif
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void)m_free(m);
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			if (inp->inp_options) {
				m->m_len = inp->inp_options->m_len;
				bcopy(mtod(inp->inp_options, caddr_t),
				    mtod(m, caddr_t), (unsigned)m->m_len);
			} else
				m->m_len = 0;
			break;

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);
			switch (optname) {

			case IP_TOS:
				optval = inp->inp_ip.ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip.ip_ttl;
				break;

#define	OPTBIT(bit)	(inp->inp_flags & bit ? 1 : 0)

			case IP_RECVOPTS:
				optval = OPTBIT(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				optval = OPTBIT(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				optval = OPTBIT(INP_RECVDSTADDR);
				break;
			}
			*mtod(m, int *) = optval;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_getmoptions(optname, inp->inp_moptions, mp);
			break;

		case IP_PORTRANGE:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);

			if (inp->inp_flags & INP_HIGHPORT)
				optval = IP_PORTRANGE_HIGH;
			else if (inp->inp_flags & INP_LOWPORT)
				optval = IP_PORTRANGE_LOW;
			else
				optval = 0;

			*mtod(m, int *) = optval;
			break;

		case IPSEC_OUTSA:
#ifndef IPSEC
			error = EINVAL;
#else
			s = spltdb();
			if (inp->inp_tdb == NULL) {
				error = ENOENT;
			} else {
				tdbi.spi = inp->inp_tdb->tdb_spi;
				tdbi.dst = inp->inp_tdb->tdb_dst;
				tdbi.proto = inp->inp_tdb->tdb_sproto;
				*mp = m = m_get(M_WAIT, MT_SOOPTS);
				m->m_len = sizeof(tdbi);
				bcopy((caddr_t)&tdbi, mtod(m, caddr_t),
				    (unsigned)m->m_len);
			}
			splx(s);
#endif /* IPSEC */
			break;

		case IP_AUTH_LEVEL:
		case IP_ESP_TRANS_LEVEL:
		case IP_ESP_NETWORK_LEVEL:
#ifndef IPSEC
			*mtod(m, int *) = IPSEC_LEVEL_NONE;
#else
			switch (optname) {
			case IP_AUTH_LEVEL:
				    optval = inp->inp_seclevel[SL_AUTH];
				    break;

			case IP_ESP_TRANS_LEVEL:
				    optval = inp->inp_seclevel[SL_ESP_TRANS];
				    break;

			case IP_ESP_NETWORK_LEVEL:
				    optval = inp->inp_seclevel[SL_ESP_NETWORK];
				    break;
			}
			*mtod(m, int *) = optval;
#endif
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}

/*
 * Set up IP options in pcb for insertion in output packets.
 * Store in mbuf with pointer in pcbopt, adding pseudo-option
 * with destination address if source routed.
 */
int
#ifdef notyet
ip_pcbopts(optname, pcbopt, m)
	int optname;
#else
ip_pcbopts(pcbopt, m)
#endif
	struct mbuf **pcbopt;
	register struct mbuf *m;
{
	register int cnt, optlen;
	register u_char *cp;
	u_char opt;

	/* turn off any old options */
	if (*pcbopt)
		(void)m_free(*pcbopt);
	*pcbopt = 0;
	if (m == (struct mbuf *)0 || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (m)
			(void)m_free(m);
		return (0);
	}

#ifndef	vax
	if (m->m_len % sizeof(int32_t))
		goto bad;
#endif
	/*
	 * IP first-hop destination address will be stored before
	 * actual options; move other options back
	 * and clear it when none present.
	 */
	if (m->m_data + m->m_len + sizeof(struct in_addr) >= &m->m_dat[MLEN])
		goto bad;
	cnt = m->m_len;
	m->m_len += sizeof(struct in_addr);
	cp = mtod(m, u_char *) + sizeof(struct in_addr);
	ovbcopy(mtod(m, caddr_t), (caddr_t)cp, (unsigned)cnt);
	bzero(mtod(m, caddr_t), sizeof(struct in_addr));

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= IPOPT_OLEN || optlen > cnt)
				goto bad;
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
		case IPOPT_SSRR:
			/*
			 * user process specifies route as:
			 *	->A->B->C->D
			 * D must be our final destination (but we can't
			 * check that since we may not have connected yet).
			 * A is first hop destination, which doesn't appear in
			 * actual IP option, but is stored before the options.
			 */
			if (optlen < IPOPT_MINOFF - 1 + sizeof(struct in_addr))
				goto bad;
			m->m_len -= sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			optlen -= sizeof(struct in_addr);
			cp[IPOPT_OLEN] = optlen;
			/*
			 * Move first hop before start of options.
			 */
			bcopy((caddr_t)&cp[IPOPT_OFFSET+1], mtod(m, caddr_t),
			    sizeof(struct in_addr));
			/*
			 * Then copy rest of options back
			 * to close up the deleted entry.
			 */
			ovbcopy((caddr_t)(&cp[IPOPT_OFFSET+1] +
			    sizeof(struct in_addr)),
			    (caddr_t)&cp[IPOPT_OFFSET+1],
			    (unsigned)cnt + sizeof(struct in_addr));
			break;
		}
	}
	if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		goto bad;
	*pcbopt = m;
	return (0);

bad:
	(void)m_free(m);
	return (EINVAL);
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
int
ip_setmoptions(optname, imop, m)
	int optname;
	struct ip_moptions **imop;
	struct mbuf *m;
{
	register int error = 0;
	u_char loop;
	register int i;
	struct in_addr addr;
	register struct ip_mreq *mreq;
	register struct ifnet *ifp;
	register struct ip_moptions *imo = *imop;
	struct route ro;
	register struct sockaddr_in *dst;

	if (imo == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		imo = (struct ip_moptions *)malloc(sizeof(*imo), M_IPMOPTS,
		    M_WAITOK);

		if (imo == NULL)
			return (ENOBUFS);
		*imop = imo;
		imo->imo_multicast_ifp = NULL;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_num_memberships = 0;
	}

	switch (optname) {

	case IP_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != sizeof(struct in_addr)) {
			error = EINVAL;
			break;
		}
		addr = *(mtod(m, struct in_addr *));
		/*
		 * INADDR_ANY is used to remove a previous selection.
		 * When no interface is selected, a default one is
		 * chosen every time a multicast packet is sent.
		 */
		if (addr.s_addr == INADDR_ANY) {
			imo->imo_multicast_ifp = NULL;
			break;
		}
		/*
		 * The selected interface is identified by its local
		 * IP address.  Find the interface and confirm that
		 * it supports multicasting.
		 */
		INADDR_TO_IFP(addr, ifp);
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		imo->imo_multicast_ifp = ifp;
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_ttl = *(mtod(m, u_char *));
		break;

	case IP_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL || m->m_len != 1 ||
		   (loop = *(mtod(m, u_char *))) > 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_loop = loop;
		break;

	case IP_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(mreq->imr_multiaddr.s_addr)) {
			error = EINVAL;
			break;
		}
		/*
		 * If no interface address was provided, use the interface of
		 * the route to the given multicast address.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY) {
			ro.ro_rt = NULL;
			dst = satosin(&ro.ro_dst);
			dst->sin_len = sizeof(*dst);
			dst->sin_family = AF_INET;
			dst->sin_addr = mreq->imr_multiaddr;
			rtalloc(&ro);
			if (ro.ro_rt == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
			ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		} else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
		}
		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast.
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * See if the membership already exists or if all the
		 * membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    imo->imo_membership[i]->inm_addr.s_addr
						== mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = EADDRINUSE;
			break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			break;
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq->imr_multiaddr, ifp)) == NULL) {
			error = ENOBUFS;
			break;
		}
		++imo->imo_num_memberships;
		break;

	case IP_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(mreq->imr_multiaddr.s_addr)) {
			error = EINVAL;
			break;
		}
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY)
			ifp = NULL;
		else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
			if (ifp == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
		}
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     imo->imo_membership[i]->inm_addr.s_addr ==
			     mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		in_delmulti(imo->imo_membership[i]);
		/*
		 * Remove the gap in the membership array.
		 */
		for (++i; i < imo->imo_num_memberships; ++i)
			imo->imo_membership[i-1] = imo->imo_membership[i];
		--imo->imo_num_memberships;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp == NULL &&
	    imo->imo_multicast_ttl == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_multicast_loop == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_num_memberships == 0) {
		free(*imop, M_IPMOPTS);
		*imop = NULL;
	}

	return (error);
}

/*
 * Return the IP multicast options in response to user getsockopt().
 */
int
ip_getmoptions(optname, imo, mp)
	int optname;
	register struct ip_moptions *imo;
	register struct mbuf **mp;
{
	u_char *ttl;
	u_char *loop;
	struct in_addr *addr;
	struct in_ifaddr *ia;

	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (optname) {

	case IP_MULTICAST_IF:
		addr = mtod(*mp, struct in_addr *);
		(*mp)->m_len = sizeof(struct in_addr);
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			addr->s_addr = INADDR_ANY;
		else {
			IFP_TO_IA(imo->imo_multicast_ifp, ia);
			addr->s_addr = (ia == NULL) ? INADDR_ANY
					: ia->ia_addr.sin_addr.s_addr;
		}
		return (0);

	case IP_MULTICAST_TTL:
		ttl = mtod(*mp, u_char *);
		(*mp)->m_len = 1;
		*ttl = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
				     : imo->imo_multicast_ttl;
		return (0);

	case IP_MULTICAST_LOOP:
		loop = mtod(*mp, u_char *);
		(*mp)->m_len = 1;
		*loop = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
				      : imo->imo_multicast_loop;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IP multicast options.
 */
void
ip_freemoptions(imo)
	register struct ip_moptions *imo;
{
	register int i;

	if (imo != NULL) {
		for (i = 0; i < imo->imo_num_memberships; ++i)
			in_delmulti(imo->imo_membership[i]);
		free(imo, M_IPMOPTS);
	}
}

/*
 * Routine called from ip_output() to loop back a copy of an IP multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be &loif -- easier than replicating that code here.
 */
static void
ip_mloopback(ifp, m, dst)
	struct ifnet *ifp;
	register struct mbuf *m;
	register struct sockaddr_in *dst;
{
	register struct ip *ip;
	struct mbuf *copym;

	copym = m_copy(m, 0, M_COPYALL);
	if (copym != NULL) {
		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		ip->ip_len = htons((u_int16_t)ip->ip_len);
		ip->ip_off = htons((u_int16_t)ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(copym, ip->ip_hl << 2);
		(void) looutput(ifp, copym, sintosa(dst), NULL);
	}
}
