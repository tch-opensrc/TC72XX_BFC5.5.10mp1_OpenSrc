//==========================================================================
//
//      src/sys/netinet6/ip6_input.c
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

/*	$KAME: ip6_input.c,v 1.242 2001/12/27 17:40:10 jinmei Exp $	*/

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
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
#include <net/intrq.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif /* INET */
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__OpenBSD__) || (defined(__bsdi__) && _BSDI_VERSION >= 199802)
#include <netinet/in_pcb.h>
#endif
#if !((defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__OpenBSD__) || (defined(__bsdi__) && _BSDI_VERSION >= 199802))
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/icmp6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>
#ifdef __bsdi__
#include <netinet6/raw_ip6.h>
#endif
#ifdef MIP6
#include <netinet6/mip6.h>
#endif

#if defined(IPSEC) && !defined(__OpenBSD__)
#include <netinet6/ipsec.h>
#endif

#if defined(IPV6FIREWALL) || (defined(__FreeBSD__) && __FreeBSD__ >= 4)
#include <netinet6/ip6_fw.h>
#endif

#include <netinet6/ip6protosw.h>

/* we need it for NLOOP. */

#if NGIF > 0
#include <netinet6/in6_gif.h>
#endif

#ifdef MIP6
#include <netinet6/mip6.h>
#endif /* MIP6 */

#ifdef __OpenBSD__
#if NPF > 0
#include <net/pfvar.h>
#endif
#endif

extern struct domain inet6domain;
extern struct ip6protosw inet6sw[];
#ifdef __bsdi__
#if _BSDI_VERSION < 199802
extern struct ifnet loif;
#else
extern struct ifnet *loifp;
#endif
#endif

u_char ip6_protox[IPPROTO_MAX];
static int ip6qmaxlen = IFQ_MAXLEN;
struct in6_ifaddr *in6_ifaddr;
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 4)
struct ifqueue ip6intrq;
#endif

#if defined(__NetBSD__)
extern struct ifnet loif[NLOOP];
#endif
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
extern struct callout in6_tmpaddrtimer_ch;
#elif defined(__OpenBSD__)
extern struct timeout in6_tmpaddrtimer_ch;
#endif

int ip6_forward_srcrt;			/* XXX */
int ip6_sourcecheck;			/* XXX */
int ip6_sourcecheck_interval;		/* XXX */
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
const int int6intrq_present = 1;
#endif

#ifdef MEASURE_PERFORMANCE
#define MEASURE_PERFORMANCE_UDPONLY
#define IP6_PERFORM_LOGSIZE 10000
int ip6_logentry;
int ip6_logsize = IP6_PERFORM_LOGSIZE;
unsigned long long ip6_performance_log[IP6_PERFORM_LOGSIZE];
unsigned long long ip6_performance_log2[IP6_PERFORM_LOGSIZE];
struct in6_addr ip6_performance_addrlog[IP6_PERFORM_LOGSIZE];
#endif
#ifdef MEASURE_PERFORMANCE
#define OURS_CHECK_ALG_RTABLE 0
#define OURS_CHECK_ALG_LINEAR 1
#define OURS_CHECK_ALG_HASH 2
#define OURS_CHECK_ALG_LARGEHASH 3
#ifdef OURS_CHECK_LINEAR
int ip6_ours_check_algorithm = OURS_CHECK_ALG_LINEAR;
#elif OURS_CHECK_HASH
int ip6_ours_check_algorithm = OURS_CHECK_ALG_HASH;
#else
int ip6_ours_check_algorithm = OURS_CHECK_ALG_RTABLE;
#endif
#else
int ip6_ours_check_algorithm;
#endif


#if defined(IPV6FIREWALL) || (defined(__FreeBSD__) && __FreeBSD__ >= 4)
/* firewall hooks */
ip6_fw_chk_t *ip6_fw_chk_ptr;
ip6_fw_ctl_t *ip6_fw_ctl_ptr;
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
int ip6_fw_enable = 1;
#endif

struct ip6stat ip6stat;

static void ip6_init2 __P((void *));
static struct mbuf *ip6_setdstifaddr __P((struct mbuf *, struct in6_ifaddr *));

static int ip6_hopopts_input __P((u_int32_t *, u_int32_t *, struct mbuf **, int *));
#ifdef PULLDOWN_TEST
static struct mbuf *ip6_pullexthdr __P((struct mbuf *, size_t, int));
#endif

#ifdef NATPT
extern int ip6_protocol_tr;

int natpt_in6 __P((struct mbuf *, struct mbuf **));
extern void ip_forward __P((struct mbuf *, int));
#endif


#ifdef MEASURE_PERFORMANCE
static unsigned long long ctr_beg, ctr_end;

static __inline unsigned long long read_tsc __P((void));
static __inline void add_performance_log __P((unsigned long long,
					      struct in6_addr *)); 
#endif

#ifdef MEASURE_PERFORMANCE
static __inline unsigned long long 
read_tsc(void)
{
     unsigned int h,l;
     /* read Pentium counter */
     __asm__(".byte 0x0f,0x31" :"=a" (l), "=d" (h));
     return ((unsigned long long)h<<32) | l;
}

static __inline void
add_performance_log(val, addr)
	unsigned long long val;
	struct in6_addr *addr;
{
	ip6_logentry = (ip6_logentry + 1) % ip6_logsize;
	ip6_performance_log[ip6_logentry] = val;
	ip6_performance_addrlog[ip6_logentry] = *addr;
}
#endif

/*
 * IP6 initialization: fill in IP6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP6 protocol handler.
 */
void
ip6_init()
{
	struct ip6protosw *pr;
	int i;
#ifndef __OpenBSD__
	struct timeval tv;
#endif

#ifdef RADIX_ART
	rt_tables[AF_INET6]->rnh_addrsize = sizeof(struct in6_addr);
#endif

#ifdef DIAGNOSTIC
	if (sizeof(struct protosw) != sizeof(struct ip6protosw))
		panic("sizeof(protosw) != sizeof(ip6protosw)");
#endif
	pr = (struct ip6protosw *)pffindproto(PF_INET6, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip6_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip6_protox[i] = pr - inet6sw;
	for (pr = (struct ip6protosw *)inet6domain.dom_protosw;
	    pr < (struct ip6protosw *)inet6domain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET6 &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip6_protox[pr->pr_protocol] = pr - inet6sw;
	ip6intrq.ifq_maxlen = ip6qmaxlen;
#if (defined(__FreeBSD__) && __FreeBSD__ >= 4)
	register_netisr(NETISR_IPV6, ip6intr);
#endif
	addrsel_policy_init();
	nd6_init();
	frag6_init();
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
#else
#ifdef IPV6FIREWALL
	ip6_fw_init();
#endif
#endif
#ifndef __OpenBSD__
	/*
	 * in many cases, random() here does NOT return random number
	 * as initialization during bootstrap time occur in fixed order.
	 */
	microtime(&tv);
	ip6_flow_seq = random() ^ tv.tv_usec;
	microtime(&tv);
	ip6_desync_factor = (random() ^ tv.tv_usec) % MAX_TEMP_DESYNC_FACTOR;
#else
	ip6_flow_seq = arc4random();
	ip6_desync_factor = arc4random() % MAX_TEMP_DESYNC_FACTOR;
#endif

#ifndef __FreeBSD__
	ip6_init2((void *)0);
#endif

#ifdef MIP6
	mip6_init();
#endif /* MIP6 */

#ifdef MEASURE_PERFORMANCE
	in6h_hashinit();
#endif
}

static void
ip6_init2(dummy)
	void *dummy;
{
#if defined(__bsdi__) && _BSDI_VERSION < 199802
	struct ifnet *loifp = &loif;
#endif

	/*
	 * to route local address of p2p link to loopback,
	 * assign loopback address first.
	 */
#ifdef __bsdi__
	in6_ifattach(loifp, NULL);
#elif defined(__OpenBSD__)
	in6_ifattach(lo0ifp, NULL);
#else
	in6_ifattach(&loif[0], NULL);
#endif

	/* nd6_timer_init */
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
	callout_init(&nd6_timer_ch);
	callout_reset(&nd6_timer_ch, hz, nd6_timer, NULL);
#elif defined(__OpenBSD__)
	bzero(&nd6_timer_ch, sizeof(nd6_timer_ch));
	timeout_set(&nd6_timer_ch, nd6_timer, NULL);
	timeout_add(&nd6_timer_ch, hz);
#else
	timeout(nd6_timer, (caddr_t)0, hz);
#endif

	/* timer for regeneranation of temporary addresses randomize ID */
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
	callout_init(&in6_tmpaddrtimer_ch);
	callout_reset(&in6_tmpaddrtimer_ch,
		      (ip6_temp_preferred_lifetime - ip6_desync_factor -
		       ip6_temp_regen_advance) * hz,
		      in6_tmpaddrtimer, NULL);
#elif defined(__OpenBSD__)
	timeout_set(&in6_tmpaddrtimer_ch, in6_tmpaddrtimer, NULL);
	timeout_add(&in6_tmpaddrtimer_ch,
	    (ip6_temp_preferred_lifetime - ip6_desync_factor -
	    ip6_temp_regen_advance) * hz);
#else
	timeout(in6_tmpaddrtimer, (caddr_t)0,
		(ip6_temp_preferred_lifetime - ip6_desync_factor -
			ip6_temp_regen_advance) * hz);
#endif
}

SYSINIT(netinet6init2, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, ip6_init2, NULL);

/*
 * IP6 input interrupt handling. Just pass the packet to ip6_input.
 */
void
ip6intr()
{
	int s;
	struct mbuf *m;

	for (;;) {
		s = splimp();
		IF_DEQUEUE(&ip6intrq, m);
		splx(s);
		if (m == 0)
			return;
		ip6_input(m);
	}
}

#if (defined(__FreeBSD__) && __FreeBSD__ <= 3)
NETISR_SET(NETISR_IPV6, ip6intr);
#endif

#ifdef NEW_STRUCT_ROUTE
extern struct	route ip6_forward_rt;
#else
extern struct	route_in6 ip6_forward_rt;

/* NetBSD/OpenBSD requires NEW_STRUCT_ROUTE */
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
#error NEW_STRUCT_ROUTE is required
#endif
#endif

void
ip6_input(m)
	struct mbuf *m;
{
	struct ip6_hdr *ip6;
	int off = sizeof(struct ip6_hdr), nest;
	u_int32_t plen;
	u_int32_t rtalert = ~0;
	int nxt, ours = 0;
	struct ifnet *deliverifp = NULL;
#if 0
	struct mbuf *mhist;	/* onion peeling history */
	caddr_t hist;
#endif
#if defined(__bsdi__) && _BSDI_VERSION < 199802
	struct ifnet *loifp = &loif;
#endif
#if defined(__NetBSD__) && defined(PFIL_HOOKS)
	struct packet_filter_hook *pfh;
	struct mbuf *m0;
	int rv;
#endif	/* PFIL_HOOKS */

#if defined(IPSEC) && !defined(__OpenBSD__)
	/*
	 * should the inner packet be considered authentic?
	 * see comment in ah4_input().
	 */
	m->m_flags &= ~M_AUTHIPHDR;
	m->m_flags &= ~M_AUTHIPDGM;
#endif

	/*
	 * make sure we don't have onion peering information into m_aux.
	 */
	ip6_delaux(m);

	/*
	 * mbuf statistics
	 */
	if (m->m_flags & M_EXT) {
		if (m->m_next)
			ip6stat.ip6s_mext2m++;
		else
			ip6stat.ip6s_mext1++;
	} else {
#define M2MMAX	(sizeof(ip6stat.ip6s_m2m)/sizeof(ip6stat.ip6s_m2m[0]))
		if (m->m_next) {
			if (m->m_flags & M_LOOP) {
#ifdef __bsdi__
				ip6stat.ip6s_m2m[loifp->if_index]++; /* XXX */
#elif defined(__OpenBSD__)
				ip6stat.ip6s_m2m[lo0ifp->if_index]++; /* XXX */
#else
				ip6stat.ip6s_m2m[loif[0].if_index]++; /* XXX */
#endif
			} else if (m->m_pkthdr.rcvif->if_index < M2MMAX)
				ip6stat.ip6s_m2m[m->m_pkthdr.rcvif->if_index]++;
			else
				ip6stat.ip6s_m2m[0]++;
		} else
			ip6stat.ip6s_m1++;
#undef M2MMAX
	}

	in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_receive);
	ip6stat.ip6s_total++;

#ifndef PULLDOWN_TEST
	/* 
	 * L2 bridge code and some other code can return mbuf chain
	 * that does not conform to KAME requirement.  too bad.
	 * XXX: fails to join if interface MTU > MCLBYTES.  jumbogram?
	 */
	if (m && m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		struct mbuf *n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n != NULL) {
#ifdef __OpenBSD__
			M_MOVE_PKTHDR(n, m);
#else
			M_COPY_PKTHDR(n, m);
#endif
		}
		if (n != NULL && m->m_pkthdr.len > MHLEN) {
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_freem(n);
				n = NULL;
			}
		}
		if (n == NULL) {
			m_freem(m);
			return;	/* ENOBUFS */
		}

		m_copydata(m, 0, m->m_pkthdr.len, mtod(n, caddr_t));
		n->m_len = m->m_pkthdr.len;
		m_freem(m);
		m = n;
	}
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), /* nothing */);
#endif

#if defined(__OpenBSD__) && NPF > 0 
	/*
	 * Packet filter
	 */
	if (pf_test6(PF_IN, m->m_pkthdr.rcvif, &m) != PF_PASS)
		goto bad;
#endif

	if (m->m_len < sizeof(struct ip6_hdr)) {
		struct ifnet *inifp;
		inifp = m->m_pkthdr.rcvif;
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == 0) {
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			return;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6stat.ip6s_badvers++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
		goto bad;
	}

#if defined(__NetBSD__) && defined(PFIL_HOOKS)
	/*
	 * Run through list of hooks for input packets.  If there are any
	 * filters which require that additional packets in the flow are
	 * not fast-forwarded, they must clear the M_CANFASTFWD flag.
	 * Note that filters must _never_ set this flag, as another filter
	 * in the list may have previously cleared it.
	 */
	/*
	 * let ipfilter look at packet on the wire,
	 * not the decapsulated packet.
	 */
#ifdef IPSEC
	if (!ipsec_getnhist(m))
#else
	if (1 /* CONSTCOND */)
#endif
	{
		m0 = m;
		pfh = pfil_hook_get(PFIL_IN,
		    &inetsw[ip_protox[IPPROTO_IPV6]].pr_pfh);
	} else
		pfh = NULL;
	for (; pfh; pfh = pfh->pfil_link.tqe_next)
		if (pfh->pfil_func) {
			rv = pfh->pfil_func(ip6, sizeof(*ip6),
					    m->m_pkthdr.rcvif, 0, &m0);
			if (rv)
				return;
			m = m0;
			if (m == NULL)
				return;
			ip6 = mtod(m, struct ip6_hdr *);
		}
#endif /* PFIL_HOOKS */

	ip6stat.ip6s_nxthist[ip6->ip6_nxt]++;

#if defined(IPV6FIREWALL) || (defined(__FreeBSD__) && __FreeBSD__ >= 4)
	/*
	 * Check with the firewall...
	 */
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
	if (ip6_fw_enable && ip6_fw_chk_ptr)
#else
	if (ip6_fw_chk_ptr)
#endif
	{
		/* If ipfw says divert, we have to just drop packet */
		/* use port as a dummy argument */
		if ((*ip6_fw_chk_ptr)(&ip6, NULL, &m)) {
			m_freem(m);
			m = NULL;
		}
		if (!m)
			return;
	}
#endif

#ifdef ALTQ
	if (altq_input != NULL && (*altq_input)(m, AF_INET6) == 0) {
		/* packet is dropped by traffic conditioner */
		return;
	}
#endif

	/*
	 * Check against address spoofing/corruption.
	 */
#if 0
            diag_printf("%s.%d\n", __FUNCTION__, __LINE__);
            diag_dump_buf(&ip6->ip6_src, 16);
            diag_dump_buf(&ip6->ip6_dst, 16);
#endif
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst)) {
		/*
		 * XXX: "badscope" is not very suitable for a multicast source.
		 */
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
                log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
		goto bad;
	}
	if ((IN6_IS_ADDR_LOOPBACK(&ip6->ip6_src) ||
	     IN6_IS_ADDR_LOOPBACK(&ip6->ip6_dst)) &&
	    (m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) == 0) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
                log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
		goto bad;
	}
	if (IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) &&
	    !(m->m_flags & M_LOOP)) {
		/*
		 * In this case, the packet should come from the loopback
		 * interface.  However, we cannot just check the if_flags,
		 * because ip6_mloopback() passes the "actual" interface
		 * as the outgoing/incoming interface.
		 */
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
                log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
		goto bad;
	}
	     
	/*
	 * The following check is not documented in specs.  A malicious
	 * party may be able to use IPv4 mapped addr to confuse tcp/udp stack
	 * and bypass security checks (act as if it was from 127.0.0.1 by using
	 * IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * This check chokes if we are in an SIIT cloud.  As none of BSDs
	 * support IPv4-less kernel compilation, we cannot support SIIT
	 * environment at all.  So, it makes more sense for us to reject any
	 * malicious packets for non-SIIT environment, than try to do a
	 * partical support for SIIT environment.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
                log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
		goto bad;
	}
#if 0
	/*
	 * Reject packets with IPv4 compatible addresses (auto tunnel).
	 *
	 * The code forbids auto tunnel relay case in RFC1933 (the check is
	 * stronger than RFC1933).  We may want to re-enable it if mech-xx
	 * is revised to forbid relaying case.
	 */
	if (IN6_IS_ADDR_V4COMPAT(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4COMPAT(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
#endif

	/* drop packets if interface ID portion is already filled */
	if ((m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) == 0) {
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src) &&
		    ip6->ip6_src.s6_addr16[1]) {
			ip6stat.ip6s_badscope++;
                        log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
			goto bad;
		}
		if ((IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) ||
		     IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) &&
		    ip6->ip6_dst.s6_addr16[1]) {
			ip6stat.ip6s_badscope++;
                        log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
			goto bad;
		}
	}

	/*
	 * Embed interface ID as the zone ID for interface-local and
	 * link-local addresses.
	 * XXX: KAME assumes one-to-one mapping between interfaces and
	 * links.
	 */
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
		ip6->ip6_src.s6_addr16[1]
			= htons(m->m_pkthdr.rcvif->if_index);
	if (IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) ||
	    IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1]
			= htons(m->m_pkthdr.rcvif->if_index);

#if 0 /* this case seems to be unnecessary. (jinmei, 20010401) */
	/*
	 * We use rt->rt_ifp to determine if the address is ours or not.
	 * If rt_ifp is lo0, the address is ours.
	 * The problem here is, rt->rt_ifp for fe80::%lo0/64 is set to lo0,
	 * so any address under fe80::%lo0/64 will be mistakenly considered
	 * local.  The special case is supplied to handle the case properly
	 * by actually looking at interface addresses
	 * (using in6ifa_ifpwithaddr).
	 */
	if ((m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) != 0 &&
	    IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst)) {
		if (!in6ifa_ifpwithaddr(m->m_pkthdr.rcvif, &ip6->ip6_dst)) {
			icmp6_error(m, ICMP6_DST_UNREACH,
			    ICMP6_DST_UNREACH_ADDR, 0);
			/* m is already freed */
			return;
		}

		ours = 1;
		deliverifp = m->m_pkthdr.rcvif;
		goto hbhcheck;
	}
#endif

	/*
	 * Multicast check
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
	  	struct	in6_multi *in6m = 0;

		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mcast);
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN6_LOOKUP_MULTI(ip6->ip6_dst, m->m_pkthdr.rcvif, in6m);
// TEMP
                if (0) {
                    struct in6_addr *_addr = &ip6->ip6_dst;
                    struct ifnet *_ifp = m->m_pkthdr.rcvif;
                    struct ifmultiaddr *_ifma; 
                    diag_printf("Lookup Multi = %p\n", in6m);
                    diag_dump_buf(_addr, 16);
                    diag_printf("=====================================================================\n");
                    for (_ifma = (_ifp)->if_multiaddrs.lh_first; _ifma; 
                         _ifma = _ifma->ifma_link.le_next) { 
                        diag_dump_buf(&((struct sockaddr_in6 *)_ifma->ifma_addr)->sin6_addr, 16);
                        diag_printf("---------------------------------------------------------------------\n");
                    } 
                }
// TEMP
		if (in6m)
			ours = 1;
		else if (!ip6_mrouter) {
			ip6stat.ip6s_notmember++;
			ip6stat.ip6s_cantforward++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
                        log(LOG_FAIL, "%s.%d - wrong multi group\n", 
                            __FUNCTION__, __LINE__);
                        log_dump(LOG_FAIL, &ip6->ip6_dst, 16);
			goto bad;
		}
		deliverifp = m->m_pkthdr.rcvif;
		goto hbhcheck;
	}

	/*
	 *  Unicast check
	 */
#ifdef MEASURE_PERFORMANCE
	ctr_beg = read_tsc();
#endif
	switch (ip6_ours_check_algorithm) {
#ifdef MEASURE_PERFORMANCE
	case OURS_CHECK_ALG_LINEAR:
	/* traditional linear search: just for measurement */
	{
		struct in6_ifaddr *ia;

		for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
			if ((ia->ia6_flags & IN6_IFF_NOTREADY) == 0 &&
			    IN6_ARE_ADDR_EQUAL(&ia->ia_addr.sin6_addr,
					       &ip6->ip6_dst)) {
#ifdef MEASURE_PERFORMANCE
				ctr_end = read_tsc();
#ifdef MEASURE_PERFORMANCE_UDPONLY
				if (ip6->ip6_nxt == IPPROTO_UDP)
#else
				if (1)
#endif
					add_performance_log(ctr_end - ctr_beg,
							    &ip6->ip6_dst);
#endif
				/* record address information into m_aux. */
				(void)ip6_setdstifaddr(m, ia);

				ours = 1;
				deliverifp = ia->ia_ifp;
				goto hbhcheck;
			}
		}
	}
	break;
	case OURS_CHECK_ALG_HASH:
	case OURS_CHECK_ALG_LARGEHASH:
	{
		struct in6_ifaddr *ia;
		struct in6hash *ih = NULL;

		if ((ih = in6h_lookup(&ip6->ip6_dst, m->m_pkthdr.rcvif)) !=
		    NULL && (ia = ih->in6h_ifa) != NULL) {
#ifdef MEASURE_PERFORMANCE
			ctr_end = read_tsc();
#ifdef MEASURE_PERFORMANCE_UDPONLY
			if (ip6->ip6_nxt == IPPROTO_UDP)
#else
			if (1)
#endif
				add_performance_log(ctr_end - ctr_beg,
						    &ip6->ip6_dst);
#endif

			/* record address information into m_aux. */
			(void)ip6_setdstifaddr(m, ia);

			ours = 1;
			deliverifp = m->m_pkthdr.rcvif;
			goto hbhcheck;
		}
	}
	break;
#endif /* MEASURE_PERFORMANCE */
	default:
		/*
		 * XXX: I intentionally broke our indentation rule here,
		 *      since this switch-case is just for measurement and
		 *      therefore should soon be removed.
		 */
	if (ip6_forward_rt.ro_rt != NULL &&
	    (ip6_forward_rt.ro_rt->rt_flags & RTF_UP) != 0 && 
	    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
			       &((struct sockaddr_in6 *)(&ip6_forward_rt.ro_dst))->sin6_addr))
		ip6stat.ip6s_forward_cachehit++;
	else {
		struct sockaddr_in6 *dst6;
#ifdef SCOPEDROUTING
		int64_t dstzone;
#endif

		if (ip6_forward_rt.ro_rt) {
			/* route is down or destination is different */
			ip6stat.ip6s_forward_cachemiss++;
			RTFREE(ip6_forward_rt.ro_rt);
			ip6_forward_rt.ro_rt = 0;
		}

		bzero(&ip6_forward_rt.ro_dst, sizeof(struct sockaddr_in6));
		dst6 = (struct sockaddr_in6 *)&ip6_forward_rt.ro_dst;
		dst6->sin6_len = sizeof(struct sockaddr_in6);
		dst6->sin6_family = AF_INET6;
		dst6->sin6_addr = ip6->ip6_dst;
#ifdef SCOPEDROUTING
		if ((dstzone = in6_addr2zoneid(m->m_pkthdr.rcvif,
					       &ip6->ip6_dst)) < 0) {
			ip6stat.ip6s_badscope++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
                        log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
			goto bad;
		}
		ip6_forward_rt.ro_dst.sin6_scope_id = dstzone;
#endif

#ifdef __FreeBSD__
		rtalloc_ign((struct route *)&ip6_forward_rt, RTF_PRCLONING);
#else
		rtalloc((struct route *)&ip6_forward_rt);
#endif
	}

#define rt6_key(r) ((struct sockaddr_in6 *)((r)->rt_nodes->rn_key))

	/*
	 * Accept the packet if the forwarding interface to the destination
	 * according to the routing table is the loopback interface,
	 * unless the associated route has a gateway.
	 * Note that this approach causes to accept a packet if there is a
	 * route to the loopback interface for the destination of the packet.
	 * But we think it's even useful in some situations, e.g. when using
	 * a special daemon which wants to intercept the packet.
	 *
	 * XXX: some OSes automatically make a cloned route for the destination
	 * of an outgoing packet.  If the outgoing interface of the packet
	 * is a loopback one, the kernel would consider the packet to be
	 * accepted, even if we have no such address assinged on the interface.
	 * We check the cloned flag of the route entry to reject such cases,
	 * assuming that route entries for our own addresses are not made by
	 * cloning (it should be true because in6_addloop explicitly installs
	 * the host route).  However, we might have to do an explicit check
	 * while it would be less efficient.  Or, should we rather install a
	 * reject route for such a case?
	 */
	if (ip6_forward_rt.ro_rt &&
	    (ip6_forward_rt.ro_rt->rt_flags &
	     (RTF_HOST|RTF_GATEWAY)) == RTF_HOST &&
#ifdef RTF_WASCLONED
	    !(ip6_forward_rt.ro_rt->rt_flags & RTF_WASCLONED) &&
#endif
#ifdef RTF_CLONED
	    !(ip6_forward_rt.ro_rt->rt_flags & RTF_CLONED) &&
#endif
#if 0
	    /*
	     * The check below is redundant since the comparison of
	     * the destination and the key of the rtentry has
	     * already done through looking up the routing table.
	     */
	    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
				&rt6_key(ip6_forward_rt.ro_rt)->sin6_addr)
#endif
	    ip6_forward_rt.ro_rt->rt_ifp->if_type == IFT_LOOP) {
		struct in6_ifaddr *ia6 =
			(struct in6_ifaddr *)ip6_forward_rt.ro_rt->rt_ifa;

#ifdef MEASURE_PERFORMANCE
		ctr_end = read_tsc();
#ifdef MEASURE_PERFORMANCE_UDPONLY
		if (ip6->ip6_nxt == IPPROTO_UDP)
#else
		if (1)
#endif
			add_performance_log(ctr_end - ctr_beg, &ip6->ip6_dst);
#endif

		/*
		 * record address information into m_aux.
		 */
		(void)ip6_setdstifaddr(m, ia6);

		/*
		 * packets to a tentative, duplicated, or somehow invalid
		 * address must not be accepted.
		 */
		if (!(ia6->ia6_flags & IN6_IFF_NOTREADY)) {
			/* this address is ready */
			ours = 1;
			deliverifp = ia6->ia_ifp;	/* correct? */
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
			/* Count the packet in the ip address stats */
			ia6->ia_ifa.if_ipackets++;
			ia6->ia_ifa.if_ibytes += m->m_pkthdr.len;
#elif defined(__bsdi__) && _BSDI_VERSION >= 199802
			/* Count the packet in the ip address stats */
			ia6->ia_ifa.ifa_ipackets++;
			ia6->ia_ifa.ifa_ibytes += m->m_pkthdr.len;
#endif
			goto hbhcheck;
		} else {
			/* address is not ready, so discard the packet. */
			nd6log((LOG_INFO,
			    "ip6_input: packet to an unready address %s->%s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst)));
			goto bad;
		}
	}
	} /* XXX indentation (see above) */

#ifdef MEASURE_PERFORMANCE
	/* we detected that packet is not ours thru the basic algorithm */
	ctr_end = read_tsc();
#ifdef MEASURE_PERFORMANCE_UDPONLY
	if (ip6->ip6_nxt == IPPROTO_UDP)
#else
	if (1)
#endif
		add_performance_log(ctr_end - ctr_beg, &ip6->ip6_dst);
#endif

	/*
	 * FAITH(Firewall Aided Internet Translator)
	 */
#if defined(NFAITH) && 0 < NFAITH
	if (ip6_keepfaith) {
		if (ip6_forward_rt.ro_rt && ip6_forward_rt.ro_rt->rt_ifp
		 && ip6_forward_rt.ro_rt->rt_ifp->if_type == IFT_FAITH) {
			/* XXX do we need more sanity checks? */
			ours = 1;
			deliverifp = ip6_forward_rt.ro_rt->rt_ifp; /* faith */
			goto hbhcheck;
		}
	}
#endif

#ifdef NATPT
	/*
	 * NAT-PT (Network Address Translation - Protocol Translation)
	 */
	if (ip6_protocol_tr) {
		struct mbuf *m1 = NULL;

		switch (natpt_in6(m, &m1)) {
		case IPPROTO_IP:
			goto processpacket;
		case IPPROTO_IPV4:
			ip_forward(m1, 0);
			break;
		case IPPROTO_IPV6:
			ip6_forward(m1, 0);
			break;
		case IPPROTO_MAX:		/* discard this packet	*/
		default:
			break;
		case IPPROTO_DONE:		/* discard without free	*/
			return;
		}

		if (m != m1)
			m_freem(m);

		return;
	}

 processpacket:
#endif

#if 0
    {
	/*
	 * Last resort: check in6_ifaddr for incoming interface.
	 * The code is here until I update the "goto ours hack" code above
	 * working right.
	 */
	struct ifaddr *ifa;
	for (ifa = m->m_pkthdr.rcvif->if_addrlist.tqh_first;
	     ifa;
	     ifa = ifa->ifa_list.tqe_next) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa), &ip6->ip6_dst)) {
			ours = 1;
			deliverifp = ifa->ifa_ifp;
			goto hbhcheck;
		}
	}
    }
#endif

	/*
	 * Now there is no reason to process the packet if it's not our own
	 * and we're not a router.
	 */
	if (!ip6_forwarding) {
		ip6stat.ip6s_cantforward++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
                log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
		goto bad;
	}

 hbhcheck:
	/*
	 * record address information into m_aux, if we don't have one yet.
	 * note that we are unable to record it, if the address is not listed
	 * as our interface address (e.g. multicast addresses, addresses
	 * within FAITH prefixes and such).
	 */
	if (deliverifp && !ip6_getdstifaddr(m)) {
		struct in6_ifaddr *ia6;

		ia6 = in6_ifawithifp(deliverifp, &ip6->ip6_dst);
		if (ia6) {
			if (!ip6_setdstifaddr(m, ia6)) {
				/*
				 * XXX maybe we should drop the packet here,
				 * as we could not provide enough information
				 * to the upper layers.
				 */
			}
		}
	}

	/*
	 * Process Hop-by-Hop options header if it's contained.
	 * m may be modified in ip6_hopopts_input().
	 * If a JumboPayload option is included, plen will also be modified.
	 */
	plen = (u_int32_t)ntohs(ip6->ip6_plen);
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
		struct ip6_hbh *hbh;

		if (ip6_hopopts_input(&plen, &rtalert, &m, &off)) {
#if 0	/*touches NULL pointer*/
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
#endif
			return;	/* m have already been freed */
		}

		/* adjust pointer */
		ip6 = mtod(m, struct ip6_hdr *);

		/*
		 * if the payload length field is 0 and the next header field
		 * indicates Hop-by-Hop Options header, then a Jumbo Payload
		 * option MUST be included.
		 */
		if (ip6->ip6_plen == 0 && plen == 0) {
			/*
			 * Note that if a valid jumbo payload option is
			 * contained, ip6_hoptops_input() must set a valid
			 * (non-zero) payload length to the variable plen. 
			 */
			ip6stat.ip6s_badoptions++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (caddr_t)&ip6->ip6_plen - (caddr_t)ip6);
			return;
		}
#ifndef PULLDOWN_TEST
		/* ip6_hopopts_input() ensures that mbuf is contiguous */
		hbh = (struct ip6_hbh *)(ip6 + 1);
#else
		IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m, sizeof(struct ip6_hdr),
			sizeof(struct ip6_hbh));
		if (hbh == NULL) {
			ip6stat.ip6s_tooshort++;
			return;
		}
#endif
		nxt = hbh->ip6h_nxt;

		/*
		 * accept the packet if a router alert option is included
		 * and we act as an IPv6 router.
		 */
		if (rtalert != ~0 && ip6_forwarding)
			ours = 1;
	} else
		nxt = ip6->ip6_nxt;

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		ip6stat.ip6s_tooshort++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
		goto bad;
	}
	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = sizeof(struct ip6_hdr) + plen;
			m->m_pkthdr.len = sizeof(struct ip6_hdr) + plen;
		} else
			m_adj(m, sizeof(struct ip6_hdr) + plen - m->m_pkthdr.len);
	}

	/*
	 * Forward if desirable.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/*
		 * If we are acting as a multicast router, all
		 * incoming multicast packets are passed to the
		 * kernel-level multicast forwarding function.
		 * The packet is returned (relatively) intact; if
		 * ip6_mforward() returns a non-zero value, the packet
		 * must be discarded, else it may be accepted below.
		 */
		if (ip6_mrouter && ip6_mforward(ip6, m->m_pkthdr.rcvif, m)) {
			ip6stat.ip6s_cantforward++;
			m_freem(m);
			return;
		}
		if (!ours) {
			m_freem(m);
			return;
		}
	} else if (!ours) {
		ip6_forward(m, 0);
		return;
	}	

	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Malicious party may be able to use IPv4 mapped addr to confuse
	 * tcp/udp stack and bypass security checks (act as if it was from
	 * 127.0.0.1 by using IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * For SIIT end node behavior, you may want to disable the check.
	 * However, you will  become vulnerable to attacks using IPv4 mapped
	 * source.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
                log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
		goto bad;
	}

	/*
	 * Tell launch routine the next header
	 */
#if defined(__NetBSD__) && defined(IFA_STATS)
	if (deliverifp != NULL) {
		struct in6_ifaddr *ia6;
		ia6 = in6_ifawithifp(deliverifp, &ip6->ip6_dst);
		if (ia6)
			ia6->ia_ifa.ifa_data.ifad_inbytes += m->m_pkthdr.len;
	}
#endif
	ip6stat.ip6s_delivered++;
	in6_ifstat_inc(deliverifp, ifs6_in_deliver);
	nest = 0;

	while (nxt != IPPROTO_DONE) {
		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			ip6stat.ip6s_toomanyhdr++;
                        log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			ip6stat.ip6s_tooshort++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
                        log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
			goto bad;
		}

#if 0
		/*
		 * do we need to do it for every header?  yeah, other
		 * functions can play with it (like re-allocate and copy).
		 */
		mhist = ip6_addaux(m);
		if (mhist && M_TRAILINGSPACE(mhist) >= sizeof(nxt)) {
			hist = mtod(mhist, caddr_t) + mhist->m_len;
			bcopy(&nxt, hist, sizeof(nxt));
			mhist->m_len += sizeof(nxt);
		} else {
			ip6stat.ip6s_toomanyhdr++;
                        log(LOG_FAIL, "%s.%d\n", __FUNCTION__, __LINE__);
			goto bad;
		}
#endif

#if defined(IPSEC) && !defined(__OpenBSD__)
		/*
		 * enforce IPsec policy checking if we are seeing last header.
		 * note that we do not visit this with protocols with pcb layer
		 * code - like udp/tcp/raw ip.
		 */
		if ((inet6sw[ip6_protox[nxt]].pr_flags & PR_LASTHDR) != 0 &&
		    ipsec6_in_reject(m, NULL)) {
			ipsec6stat.in_polvio++;
			goto bad;
		}
#endif
#ifdef MIP6
		/*
		 * XXX
		 * check if the packet was tunneled after all extion
		 * headers have been processed.  get from Ericsson
		 * code.  need more consideration.
		 */
		if ((nxt != IPPROTO_HOPOPTS) && (nxt != IPPROTO_DSTOPTS) &&
		    (nxt != IPPROTO_ROUTING) && (nxt != IPPROTO_FRAGMENT) &&
		    (nxt != IPPROTO_ESP) && (nxt != IPPROTO_AH)) {
			if (mip6_route_optimize(m))
				goto bad;
		}
#endif /* MIP6 */		
		nxt = (*inet6sw[ip6_protox[nxt]].pr_input)(&m, &off, nxt);
	}
	return;
 bad:
	m_freem(m);
}

/*
 * set/grab in6_ifaddr correspond to IPv6 destination address.
 * XXX backward compatibility wrapper
 */
static struct mbuf *
ip6_setdstifaddr(m, ia6)
	struct mbuf *m;
	struct in6_ifaddr *ia6;
{
	struct mbuf *n;

	n = ip6_addaux(m);
	if (n)
		mtod(n, struct ip6aux *)->ip6a_dstia6 = ia6;
	return n;	/* NULL if failed to set */
}

struct in6_ifaddr *
ip6_getdstifaddr(m)
	struct mbuf *m;
{
	struct mbuf *n;

	n = ip6_findaux(m);
	if (n)
		return mtod(n, struct ip6aux *)->ip6a_dstia6;
	else
		return NULL;
}

/*
 * Hop-by-Hop options header processing. If a valid jumbo payload option is
 * included, the real payload length will be stored in plenp.
 */
static int
ip6_hopopts_input(plenp, rtalertp, mp, offp)
	u_int32_t *plenp;
	u_int32_t *rtalertp;	/* XXX: should be stored more smart way */
	struct mbuf **mp;
	int *offp;
{
	struct mbuf *m = *mp;
	int off = *offp, hbhlen;
	struct ip6_hbh *hbh;
	u_int8_t *opt;

	/* validation of the length of the header */
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, sizeof(*hbh), -1);
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	hbhlen = (hbh->ip6h_len + 1) << 3;

	IP6_EXTHDR_CHECK(m, off, hbhlen, -1);
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m,
		sizeof(struct ip6_hdr), sizeof(struct ip6_hbh));
	if (hbh == NULL) {
		ip6stat.ip6s_tooshort++;
		return -1;
	}
	hbhlen = (hbh->ip6h_len + 1) << 3;
	IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m, sizeof(struct ip6_hdr),
		hbhlen);
	if (hbh == NULL) {
		ip6stat.ip6s_tooshort++;
		return -1;
	}
#endif
	off += hbhlen;
	hbhlen -= sizeof(struct ip6_hbh);
	opt = (u_int8_t *)hbh + sizeof(struct ip6_hbh);

	if (ip6_process_hopopts(m, (u_int8_t *)hbh + sizeof(struct ip6_hbh),
				hbhlen, rtalertp, plenp) < 0)
		return(-1);

	*offp = off;
	*mp = m;
	return(0);
}

/*
 * Search header for all Hop-by-hop options and process each option.
 * This function is separate from ip6_hopopts_input() in order to
 * handle a case where the sending node itself process its hop-by-hop
 * options header. In such a case, the function is called from ip6_output().
 *
 * The function assumes that hbh header is located right after the IPv6 header
 * (RFC2460 p7), opthead is pointer into data content in m, and opthead to
 * opthead + hbhlen is located in continuous memory region.
 */
int
ip6_process_hopopts(m, opthead, hbhlen, rtalertp, plenp)
	struct mbuf *m;
	u_int8_t *opthead;
	int hbhlen;
	u_int32_t *rtalertp;
	u_int32_t *plenp;
{
	struct ip6_hdr *ip6;
	int optlen = 0;
	u_int8_t *opt = opthead;
	u_int16_t rtalert_val;
	u_int32_t jumboplen;
	const int erroff = sizeof(struct ip6_hdr) + sizeof(struct ip6_hbh);

	for (; hbhlen > 0; hbhlen -= optlen, opt += optlen) {
		switch (*opt) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_PADN:
			if (hbhlen < IP6OPT_MINLEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			optlen = *(opt + 1) + 2;
			break;
		case IP6OPT_RTALERT:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_RTALERT_LEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_RTALERT_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
					    ICMP6_PARAMPROB_HEADER,
					    erroff + opt + 1 - opthead);
				return(-1);
			}
			optlen = IP6OPT_RTALERT_LEN;
			bcopy((caddr_t)(opt + 2), (caddr_t)&rtalert_val, 2);
			*rtalertp = ntohs(rtalert_val);
			break;
		case IP6OPT_JUMBO:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_JUMBO_LEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_JUMBO_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
					    ICMP6_PARAMPROB_HEADER,
					    erroff + opt + 1 - opthead);
				return(-1);
			}
			optlen = IP6OPT_JUMBO_LEN;

			/*
			 * IPv6 packets that have non 0 payload length
			 * must not contain a jumbo payload option.
			 */
			ip6 = mtod(m, struct ip6_hdr *);
			if (ip6->ip6_plen) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
					    ICMP6_PARAMPROB_HEADER,
					    erroff + opt - opthead);
				return(-1);
			}

			/*
			 * We may see jumbolen in unaligned location, so
			 * we'd need to perform bcopy().
			 */
			bcopy(opt + 2, &jumboplen, sizeof(jumboplen));
			jumboplen = (u_int32_t)htonl(jumboplen);

#if 1
			/*
			 * if there are multiple jumbo payload options,
			 * *plenp will be non-zero and the packet will be
			 * rejected.
			 * the behavior may need some debate in ipngwg -
			 * multiple options does not make sense, however,
			 * there's no explicit mention in specification.
			 */
			if (*plenp != 0) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
					    ICMP6_PARAMPROB_HEADER,
					    erroff + opt + 2 - opthead);
				return(-1);
			}
#endif

			/*
			 * jumbo payload length must be larger than 65535.
			 */
			if (jumboplen <= IPV6_MAXPACKET) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
					    ICMP6_PARAMPROB_HEADER,
					    erroff + opt + 2 - opthead);
				return(-1);
			}
			*plenp = jumboplen;

			break;
		default:		/* unknown option */
			if (hbhlen < IP6OPT_MINLEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			optlen = ip6_unknown_opt(opt, m,
			    erroff + opt - opthead);
			if (optlen == -1)
				return(-1);
			optlen += 2;
			break;
		}
	}

	return(0);

  bad:
	m_freem(m);
	return(-1);
}

/*
 * Unknown option processing.
 * The third argument `off' is the offset from the IPv6 header to the option,
 * which is necessary if the IPv6 header the and option header and IPv6 header
 * is not continuous in order to return an ICMPv6 error.
 */
int
ip6_unknown_opt(optp, m, off)
	u_int8_t *optp;
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6;

	switch (IP6OPT_TYPE(*optp)) {
	case IP6OPT_TYPE_SKIP: /* ignore the option */
		return((int)*(optp + 1));
	case IP6OPT_TYPE_DISCARD:	/* silently discard */
		m_freem(m);
		return(-1);
	case IP6OPT_TYPE_FORCEICMP: /* send ICMP even if multicasted */
		ip6stat.ip6s_badoptions++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION, off);
		return(-1);
	case IP6OPT_TYPE_ICMP: /* send ICMP if not multicasted */
		ip6stat.ip6s_badoptions++;
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    (m->m_flags & (M_BCAST|M_MCAST)))
			m_freem(m);
		else
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_OPTION, off);
		return(-1);
	}

	m_freem(m);		/* XXX: NOTREACHED */
	return(-1);
}

/*
 * Create the "control" list for this pcb.
 * The function will not modify mbuf chain at all.
 *
 * with KAME mbuf chain restriction:
 * The routine will be called from upper layer handlers like tcp6_input().
 * Thus the routine assumes that the caller (tcp6_input) have already
 * called IP6_EXTHDR_CHECK() and all the extension headers are located in the
 * very first mbuf on the mbuf chain.
 */
void
ip6_savecontrol(in6p, ip6, m, ctl, prevctlp)
#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(HAVE_NRL_INPCB)
	struct inpcb *in6p;
#else
	struct in6pcb *in6p;
#endif
	struct ip6_hdr *ip6;
	struct mbuf *m;
	struct ip6_recvpktopts *ctl, **prevctlp;
{
#define IS2292(x, y)	((in6p->in6p_flags & IN6P_RFC2292) ? (x) : (y))
	struct mbuf **mp;
	struct cmsghdr *cm = NULL;
	struct ip6_recvpktopts *prevctl = NULL;
#ifdef HAVE_NRL_INPCB
# define in6p_flags	inp_flags
#endif
#ifdef __bsdi__
# define sbcreatecontrol	so_cmsg
#endif
	int privileged = 1;

	if (ctl == NULL)	/* validity check */
		return;
	bzero(ctl, sizeof(*ctl)); /* XXX is it really OK? */
	mp = &ctl->head;

	/*
	 * If caller wanted to keep history, allocate space to store the
	 * history at the first time.
	 */
	if (prevctlp) {
		if (*prevctlp == NULL) {
			MALLOC(prevctl, struct ip6_recvpktopts *,
			       sizeof(*prevctl), M_IP6OPT, M_NOWAIT);
			if (prevctl == NULL) {
				printf("ip6_savecontrol: can't allocate "
				       " enough space for history\n");
				return;
			}
			bzero(prevctl, sizeof(*prevctl));
			*prevctlp = prevctl;
		}
		else
			prevctl = *prevctlp;
	}


#ifdef SO_TIMESTAMP
	if ((in6p->in6p_socket->so_options & SO_TIMESTAMP) != 0) {
		struct timeval tv;

		microtime(&tv);
		*mp = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
				      SCM_TIMESTAMP, SOL_SOCKET);
		if (*mp) {
			/* always set regradless of the previous value */
			ctl->timestamp = *mp;
			mp = &(*mp)->m_next;
		}
	}
#endif

	/* RFC 2292 sec. 5 */
	if ((in6p->in6p_flags & IN6P_PKTINFO) != 0) {
		struct in6_pktinfo pi6, *prevpi = NULL;
		bcopy(&ip6->ip6_dst, &pi6.ipi6_addr, sizeof(struct in6_addr));
		if (IN6_IS_SCOPE_LINKLOCAL(&pi6.ipi6_addr) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL(&pi6.ipi6_addr))
			pi6.ipi6_addr.s6_addr16[1] = 0;
		pi6.ipi6_ifindex = (m && m->m_pkthdr.rcvif)
					? m->m_pkthdr.rcvif->if_index
					: 0;
		if (prevctl && prevctl->pktinfo) {
			cm = mtod(prevctl->pktinfo, struct cmsghdr *);
			prevpi = (struct in6_pktinfo *)CMSG_DATA(cm);
		}

		/*
		 * Make a new option only if this is the first time or if the
		 * option value is chaned from last time.
		 */
		if (prevpi == NULL || bcmp(prevpi, &pi6, sizeof(pi6))) {
			*mp = sbcreatecontrol((caddr_t) &pi6,
			     sizeof(struct in6_pktinfo),
			     IS2292(IPV6_2292PKTINFO, IPV6_PKTINFO),
			     IPPROTO_IPV6);
			if (*mp) {
				ctl->pktinfo = *mp;
				mp = &(*mp)->m_next;
			}
		}
	}

	if ((in6p->in6p_flags & IN6P_HOPLIMIT) != 0) {
		int hlim = ip6->ip6_hlim & 0xff, oldhlim = -1;

		if (prevctl && prevctl->hlim) {
			cm = mtod(prevctl->hlim, struct cmsghdr *);
			bcopy(CMSG_DATA(cm), &oldhlim, sizeof(oldhlim));
			oldhlim &= 0xff;
		}

		if (oldhlim < 0 || hlim != oldhlim) {
			*mp = sbcreatecontrol((caddr_t) &hlim, sizeof(int),
			    IS2292(IPV6_2292HOPLIMIT, IPV6_HOPLIMIT),
			    IPPROTO_IPV6);
			if (*mp) {
				ctl->hlim = *mp;
				mp = &(*mp)->m_next;
			}
		}
	}

	if ((in6p->in6p_flags & IN6P_TCLASS) != 0) {
		u_int32_t flowinfo;
		int oflowinfo = -1;
		int v;

		flowinfo = (u_int32_t)ntohl(ip6->ip6_flow & IPV6_FLOWINFO_MASK);
		flowinfo >>= 20;

		if (prevctl && prevctl->hlim) {
			cm = mtod(prevctl->hlim, struct cmsghdr *);
			bcopy(CMSG_DATA(cm), &v, sizeof(v));
			oflowinfo = v & 0xff;
		}

		if (oflowinfo < 0 || flowinfo != oflowinfo) {
			v = flowinfo & 0xff;
			*mp = sbcreatecontrol((caddr_t) &v, sizeof(v),
			    IPV6_TCLASS, IPPROTO_IPV6);
			if (*mp) {
				ctl->hlim = *mp;
				mp = &(*mp)->m_next;
			}
		}
	}

	/*
	 * IPV6_HOPOPTS socket option. We require super-user privilege
	 * for the option, but it might be too strict, since there might
	 * be some hop-by-hop options which can be returned to normal user.
	 * See RFC 2292 section 6.
	 */
	if ((in6p->in6p_flags & IN6P_HOPOPTS) != 0 && privileged) {
		/*
		 * Check if a hop-by-hop options header is contatined in the
		 * received packet, and if so, store the options as ancillary
		 * data. Note that a hop-by-hop options header must be
		 * just after the IPv6 header, which fact is assured through
		 * the IPv6 input processing.
		 */
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
		if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh, *prevhbh = NULL;
			int hbhlen = 0, prevhbhlen = 0;
#ifdef PULLDOWN_TEST
			struct mbuf *ext;
#endif

#ifndef PULLDOWN_TEST
			hbh = (struct ip6_hbh *)(ip6 + 1);
			hbhlen = (hbh->ip6h_len + 1) << 3;
#else
			ext = ip6_pullexthdr(m, sizeof(struct ip6_hdr),
			    ip6->ip6_nxt);
			if (ext == NULL) {
				ip6stat.ip6s_tooshort++;
				return;
			}
			hbh = mtod(ext, struct ip6_hbh *);
			hbhlen = (hbh->ip6h_len + 1) << 3;
			if (hbhlen != ext->m_len) {
				m_freem(ext);
				ip6stat.ip6s_tooshort++;
				return;
			}
#endif

			if (prevctl && prevctl->hbh) {
				cm = mtod(prevctl->hbh, struct cmsghdr *);
				prevhbh = (struct ip6_hbh *)CMSG_DATA(cm);
				prevhbhlen = (prevhbh->ip6h_len + 1) << 3;
			}
			/*
			 * Check if there's difference between the current
			 * and previous HbH headers.
			 * XXX: should the next header field be ignored?
			 */
			if (prevhbh == NULL || hbhlen != prevhbhlen ||
			    bcmp(prevhbh, hbh, hbhlen)) {
				/*
				 * XXX: We copy whole the header even if a
				 * jumbo payload option is included, which
				 * option is to be removed before returning
				 * in the RFC 2292.
				 * Note: this constraint is removed in
				 * 2292bis.
				 */
				*mp = sbcreatecontrol((caddr_t)hbh, hbhlen,
				    IS2292(IPV6_2292HOPOPTS, IPV6_HOPOPTS),
				    IPPROTO_IPV6);
				if (*mp) {
					ctl->hbh = *mp;
					mp = &(*mp)->m_next;
				}
			}
#ifdef PULLDOWN_TEST
			m_freem(ext);
#endif
		}
	}

	if ((in6p->in6p_flags & (IN6P_RTHDR | IN6P_DSTOPTS)) != 0) {
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
		int nxt = ip6->ip6_nxt, off = sizeof(struct ip6_hdr);

		/*
		 * Search for destination options headers or routing
		 * header(s) through the header chain, and stores each
		 * header as ancillary data.
		 * Note that the order of the headers remains in
		 * the chain of ancillary data.
		 */
		while (1) {	/* is explicit loop prevention necessary? */
			struct ip6_ext *ip6e = NULL;
			int elen;
#ifdef PULLDOWN_TEST
			struct mbuf *ext = NULL;
#endif

			/*
			 * if it is not an extension header, don't try to
			 * pull it from the chain.
			 */
			switch (nxt) {
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;
			default:
				goto loopend;
			}

#ifndef PULLDOWN_TEST
			if (off + sizeof(*ip6e) > m->m_len)
				goto loopend;
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + off);
			if (nxt == IPPROTO_AH)
				elen = (ip6e->ip6e_len + 2) << 2;
			else
				elen = (ip6e->ip6e_len + 1) << 3;
			if (off + elen > m->m_len)
				goto loopend;
#else
			ext = ip6_pullexthdr(m, off, nxt);
			if (ext == NULL) {
				ip6stat.ip6s_tooshort++;
				return;
			}
			ip6e = mtod(ext, struct ip6_ext *);
			if (nxt == IPPROTO_AH)
				elen = (ip6e->ip6e_len + 2) << 2;
			else
				elen = (ip6e->ip6e_len + 1) << 3;
			if (elen != ext->m_len) {
				m_freem(ext);
				ip6stat.ip6s_tooshort++;
				return;
			}
#endif

			switch (nxt) {
			case IPPROTO_DSTOPTS:
			{
				struct ip6_dest *prevdest = NULL;
				int prevdestlen = 0;

				if (!(in6p->in6p_flags & IN6P_DSTOPTS))
					break;

				/*
				 * We also require super-user privilege for
				 * the option.  See comments on IN6_HOPOPTS.
				 */

				if (prevctl && prevctl->dest) {
					cm = mtod(prevctl->dest,
						  struct cmsghdr *);
					prevdest = (struct ip6_dest *)CMSG_DATA(cm);
					prevdestlen =
						(prevdest->ip6d_len + 1) << 3;
				}

				/*
				 * If this is the 1st dst opt header
				 * we enconter and this header is
				 * not different from the previous one,
				 * simply ignore the header.
				 */
				if (ctl->dest == NULL && prevdest &&
				    prevdestlen == elen &&
				    bcmp(ip6e, prevdest, elen) == 0)
					break;

				*mp = sbcreatecontrol((caddr_t)ip6e, elen,
						      IS2292(IPV6_2292DSTOPTS,
							     IPV6_DSTOPTS),
						      IPPROTO_IPV6);
				if (ctl->dest == NULL)
					ctl->dest = *mp;
				if (*mp)
					mp = &(*mp)->m_next;
				break;
			}
			case IPPROTO_ROUTING:
			{
				struct ip6_rthdr *prevrth = NULL;
				int prevrhlen = 0;

				if (!in6p->in6p_flags & IN6P_RTHDR)
					break;

				if (prevctl && prevctl->rthdr) {
					cm = mtod(prevctl->rthdr,
						  struct cmsghdr *);
					prevrth = (struct ip6_rthdr *)CMSG_DATA(cm);
					prevrhlen =
						(prevrth->ip6r_len + 1) << 3;
				}

				/*
				 * Check if the rthdr should be passed to
				 * a user. See the comments for dstopt hdr.
				 */
				if (ctl->rthdr == NULL && prevrth &&
				    prevrhlen == elen &&
				    bcmp(ip6e, prevrth, elen) == 0)
					break;

				*mp = sbcreatecontrol((caddr_t)ip6e, elen,
						      IS2292(IPV6_2292RTHDR,
							     IPV6_RTHDR),
						      IPPROTO_IPV6);
				if (ctl->rthdr == NULL)
					ctl->rthdr = *mp;
				if (*mp)
					mp = &(*mp)->m_next;
				break;
			}
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;

			default:
				/*
			 	 * other cases have been filtered in the above.
				 * none will visit this case.  here we supply
				 * the code just in case (nxt overwritten or
				 * other cases).
				 */
#ifdef PULLDOWN_TEST
				m_freem(ext);
#endif
				goto loopend;

			}

			/* proceed with the next header. */
			off += elen;
			nxt = ip6e->ip6e_nxt;
			ip6e = NULL;
#ifdef PULLDOWN_TEST
			m_freem(ext);
			ext = NULL;
#endif
		}
	  loopend:
		;
	}

#ifdef __bsdi__
# undef sbcreatecontrol
#endif
#ifdef __OpenBSD__
# undef in6p_flags
#endif
#undef IS2292
}

void
ip6_notify_pmtu(in6p, dst, mtu)
#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(HAVE_NRL_INPCB)
	struct inpcb *in6p;
#else
	struct in6pcb *in6p;
#endif
	struct sockaddr_in6 *dst;
	u_int32_t *mtu;
{
	struct socket *so;
	struct mbuf *m_mtu;
	struct ip6_mtuinfo mtuctl;
#ifdef __bsdi__
# define sbcreatecontrol	so_cmsg
#endif

#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(HAVE_NRL_INPCB)
	so =  in6p->inp_socket;
#else
	so = in6p->in6p_socket;
#endif

	if (mtu == NULL)
		return;

#ifdef DIAGNOSTIC
	if (so == NULL)		/* I believe this is impossible */
		panic("ip6_notify_pmtu: socket is NULL");
#endif

	bzero(&mtuctl, sizeof(mtuctl));	/* zero-clear for safety */
	mtuctl.ip6m_mtu = *mtu;
	mtuctl.ip6m_addr = *dst;
#ifndef SCOPEDROUTING
	in6_recoverscope(&mtuctl.ip6m_addr, &mtuctl.ip6m_addr.sin6_addr, NULL);
#endif

	if ((m_mtu = sbcreatecontrol((caddr_t)&mtuctl, sizeof(mtuctl),
				     IPV6_PATHMTU, IPPROTO_IPV6)) == NULL)
		return;

	if (sbappendaddr(&so->so_rcv, (struct sockaddr *)dst, NULL, m_mtu)
	    == 0) {
		m_freem(m_mtu);
		/* XXX: should count statistics */
	} else
		sorwakeup(so);

	return;

#ifdef __bsdi__
# undef sbcreatecontrol
#endif
}

#ifdef PULLDOWN_TEST
/*
 * pull single extension header from mbuf chain.  returns single mbuf that
 * contains the result, or NULL on error.
 */
static struct mbuf *
ip6_pullexthdr(m, off, nxt)
	struct mbuf *m;
	size_t off;
	int nxt;
{
	struct ip6_ext ip6e;
	size_t elen;
	struct mbuf *n;

#ifdef DIAGNOSTIC
	switch (nxt) {
	case IPPROTO_DSTOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_HOPOPTS:
	case IPPROTO_AH: /* is it possible? */
		break;
	default:
		printf("ip6_pullexthdr: invalid nxt=%d\n", nxt);
	}
#endif

	m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
	if (nxt == IPPROTO_AH)
		elen = (ip6e.ip6e_len + 2) << 2;
	else
		elen = (ip6e.ip6e_len + 1) << 3;

	MGET(n, M_DONTWAIT, MT_DATA);
	if (n && elen >= MLEN) {
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_free(n);
			n = NULL;
		}
	}
	if (!n)
		return NULL;

	n->m_len = 0;
	if (elen >= M_TRAILINGSPACE(n)) {
		m_free(n);
		return NULL;
	}

	m_copydata(m, off, elen, mtod(n, caddr_t));
	n->m_len = elen;
	return n;
}
#endif

/*
 * Merge new IPv6 received options to previous ones.
 * If a new option is not given, just re-link the option chain.
 * If an old option exists but a corresponding new one doesn't, just
 * keep the ole option.
 * If a new option exists but a corresponding old one doesn't, just
 * copy the new option.
 * If both new and old options exist, free old one and overwrite the option
 * with the new option.
 * Otherwise, do nothing for the option.
 * XXX: in any case, options that don't follow the recommend order and
 *      number of extension headers (RFC 2460 Section 4.1) are simply ignored.
 * XXX: We assume that each option is stored in a single mbuf.
 */
#define CLEAN_RECVOPT(old, type) \
do {								\
	if ((old)->type && (old)->type->m_next) {		\
		(old)->type->m_next = NULL;			\
	}							\
} while (0)
#define MERGE_RECVOPT(new, old, type) if ((new)->type) {\
		if ((old)->type)\
			m_free((old)->type);\
		(old)->type = m_copy((new)->type, 0, (new)->type->m_len);\
		if (((old)->type) && ((old)->type->m_next)) {\
			m_freem((old)->type);\
			old->type = NULL;\
		}\
	}
#define LINK_RECVOPTS(opt, type, p) if ((opt)->type) {\
		*(p) = (opt)->type;\
		(p) = &(opt)->type->m_next;\
	}

static void dump_inputopts __P((char *, struct ip6_recvpktopts *));
static void
dump_inputopts(str, p)
	char *str;
	struct ip6_recvpktopts *p;
{
#if 1
	return;
#else
#define PRINT1(p, name) \
do { \
	if (p->name) { \
		printf(" %s: %p", #name, (p)->name); \
		if (p->name->m_next) \
			printf("[%p]", (p)->name->m_next); \
	} \
} while (0)

	printf("%s p=%p head=%p", str, p, p->head);
	PRINT1(p, hlim);
	PRINT1(p, pktinfo);
	PRINT1(p, hbh);
	PRINT1(p, dest);
	PRINT1(p, rthdr);
	printf("\n");
#undef PRINT1
#endif
}

void
ip6_update_recvpcbopt(old, new)
	struct ip6_recvpktopts *new, *old;
{
	struct mbuf **mp;

	if (old == NULL) {
		printf("ip6_update_recvpcbopt: invalid arguments\n");
		return;
	}

	dump_inputopts("old before", old);
	if (new)
		dump_inputopts("new before", new);

#if 0
	/*
	 * cleanup m->m_next linkage. note that we do it in reverse order
	 * to prevent possible memory leakage.
	 */
	old->head = NULL;
	CLEAN_RECVOPT(old, rthdr);
	CLEAN_RECVOPT(old, dest);
	CLEAN_RECVOPT(old, hbh);
	CLEAN_RECVOPT(old, pktinfo);
	CLEAN_RECVOPT(old, hlim);
#endif

	if (new) {
		MERGE_RECVOPT(new, old, hlim);
		MERGE_RECVOPT(new, old, pktinfo);
		MERGE_RECVOPT(new, old, hbh);
		MERGE_RECVOPT(new, old, dest);
		MERGE_RECVOPT(new, old, rthdr);
	}

	dump_inputopts("old middle", old);
	if (new)
		dump_inputopts("new middle", new);

	/* link options */
	mp = &old->head;
	LINK_RECVOPTS(old, hlim, mp);
	LINK_RECVOPTS(old, pktinfo, mp);
	LINK_RECVOPTS(old, hbh, mp);
	LINK_RECVOPTS(old, dest, mp);
	LINK_RECVOPTS(old, rthdr, mp);
	*mp = NULL;

	dump_inputopts("old after", old);
	if (new)
		dump_inputopts("new after", new);
}

#undef MERGE_RECVOPT
#undef LINK_RECVOPTS

void
ip6_reset_rcvopt(opts, optname)
	struct ip6_recvpktopts *opts;
	int optname;
{
	if (opts == NULL)
		return;

	switch (optname) {
	case IPV6_RECVPKTINFO:
		if (opts->pktinfo) m_free(opts->pktinfo);
		opts->pktinfo = NULL;
		break;
	case IPV6_RECVHOPLIMIT:
		if (opts->hlim) m_free(opts->hlim);
		opts->hlim = NULL;
		break;
	case IPV6_RECVHOPOPTS:
		if (opts->hbh) m_free(opts->hbh);
		opts->hbh = NULL;
		break;
	case IPV6_RECVDSTOPTS:
		if (opts->dest) m_free(opts->dest);
		opts->dest = NULL;
		break;
	case IPV6_RECVRTHDR:
		if (opts->rthdr) m_free(opts->rthdr);
		opts->rthdr = NULL;
		break;
	default:
		printf("ip6_reset_rcvopt: invalid option name (%d)\n",
		       optname);
		return;
	}

	ip6_update_recvpcbopt(opts, NULL); /* re-link the option chain */
}

/*
 * Get pointer to the previous header followed by the header
 * currently processed.
 * XXX: This function supposes that
 *	M includes all headers,
 *	the next header field and the header length field of each header
 *	are valid, and
 *	the sum of each header length equals to OFF.
 * Because of these assumptions, this function must be called very
 * carefully. Moreover, it will not be used in the near future when
 * we develop `neater' mechanism to process extension headers.
 */
char *
ip6_get_prevhdr(m, off)
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

	if (off == sizeof(struct ip6_hdr))
		return(&ip6->ip6_nxt);
	else {
		int len, nxt;
		struct ip6_ext *ip6e = NULL;

		nxt = ip6->ip6_nxt;
		len = sizeof(struct ip6_hdr);
		while (len < off) {
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + len);

			switch (nxt) {
			case IPPROTO_FRAGMENT:
				len += sizeof(struct ip6_frag);
				break;
			case IPPROTO_AH:
				len += (ip6e->ip6e_len + 2) << 2;
				break;
			default:
				len += (ip6e->ip6e_len + 1) << 3;
				break;
			}
			nxt = ip6e->ip6e_nxt;
		}
		if (ip6e)
			return(&ip6e->ip6e_nxt);
		else
			return NULL;
	}
}

/*
 * get next header offset.  m will be retained.
 */
int
ip6_nexthdr(m, off, proto, nxtp)
	struct mbuf *m;
	int off;
	int proto;
	int *nxtp;
{
	struct ip6_hdr ip6;
	struct ip6_ext ip6e;
	struct ip6_frag fh;

	/* just in case */
	if (m == NULL)
		panic("ip6_nexthdr: m == NULL");
	if ((m->m_flags & M_PKTHDR) == 0 || m->m_pkthdr.len < off)
		return -1;

	switch (proto) {
	case IPPROTO_IPV6:
		/* do not chase beyond intermediate IPv6 headers */
		if (off != 0)
			return -1;
		if (m->m_pkthdr.len < off + sizeof(ip6))
			return -1;
		m_copydata(m, off, sizeof(ip6), (caddr_t)&ip6);
		if (nxtp)
			*nxtp = ip6.ip6_nxt;
		off += sizeof(ip6);
		return off;

	case IPPROTO_FRAGMENT:
		/*
		 * terminate parsing if it is not the first fragment,
		 * it does not make sense to parse through it.
		 */
		if (m->m_pkthdr.len < off + sizeof(fh))
			return -1;
		m_copydata(m, off, sizeof(fh), (caddr_t)&fh);
		if ((ntohs(fh.ip6f_offlg) & IP6F_OFF_MASK) != 0)
			return -1;
		if (nxtp)
			*nxtp = fh.ip6f_nxt;
		off += sizeof(struct ip6_frag);
		return off;

	case IPPROTO_AH:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 2) << 2;
		if (m->m_pkthdr.len < off)
			return -1;
		return off;

	case IPPROTO_HOPOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_DSTOPTS:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 1) << 3;
		if (m->m_pkthdr.len < off)
			return -1;
		return off;

	case IPPROTO_NONE:
	case IPPROTO_ESP:
	case IPPROTO_IPCOMP:
		/* give up */
		return -1;

	default:
		return -1;
	}

	return -1;
}

/*
 * get offset for the last header in the chain.  m will be kept untainted.
 */
int
ip6_lasthdr(m, off, proto, nxtp)
	struct mbuf *m;
	int off;
	int proto;
	int *nxtp;
{
	int newoff;
	int nxt;

	if (!nxtp) {
		nxt = -1;
		nxtp = &nxt;
	}
	while (1) {
		newoff = ip6_nexthdr(m, off, proto, nxtp);
		if (newoff < 0)
			return off;
		else if (newoff < off)
			return -1;	/* invalid */
		else if (newoff == off)
			return newoff;

		off = newoff;
		proto = *nxtp;
	}
}

#if !(defined(__FreeBSD__) && __FreeBSD__ >= 4)
void
pfctlinput2(cmd, sa, ctlparam)
	int cmd;
	struct sockaddr *sa;
	void *ctlparam;
{
	struct domain *dp;
	struct protosw *pr;

	if (!sa)
		return;
	for (dp = domains; dp; dp = dp->dom_next) {
		/*
		 * the check must be made by xx_ctlinput() anyways, to
		 * make sure we use data item pointed to by ctlparam in
		 * correct way.  the following check is made just for safety.
		 */
		if (dp->dom_family != sa->sa_family)
			continue;

		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, ctlparam);
	}
}
#endif

struct mbuf *
ip6_addaux(m)
	struct mbuf *m;
{
	struct mbuf *n;

#ifdef DIAGNOSTIC
	if (sizeof(struct ip6aux) > MHLEN)
		panic("assumption failed on sizeof(ip6aux)");
#endif
	n = m_aux_find(m, AF_INET6, -1);
	if (n) {
		if (n->m_len < sizeof(struct ip6aux)) {
			printf("conflicting use of ip6aux");
			return NULL;
		}
	} else {
		n = m_aux_add(m, AF_INET6, -1);
		n->m_len = sizeof(struct ip6aux);
		bzero(mtod(n, caddr_t), n->m_len);
	}
	return n;
}

struct mbuf *
ip6_findaux(m)
	struct mbuf *m;
{
	struct mbuf *n;

	n = m_aux_find(m, AF_INET6, -1);
	if (n && n->m_len < sizeof(struct ip6aux)) {
		printf("conflicting use of ip6aux");
		n = NULL;
	}
	return n;
}

void
ip6_delaux(m)
	struct mbuf *m;
{
	struct mbuf *n;

	n = m_aux_find(m, AF_INET6, -1);
	if (n)
		m_aux_delete(m, n);
}

/*
 * System control for IP6
 */

int inet6ctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

