//==========================================================================
//
//      sys/netinet/ip_input.c
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


/*	$OpenBSD: ip_input.c,v 1.44 1999/12/08 06:50:20 itojun Exp $	*/
/*	$NetBSD: ip_input.c,v 1.30 1996/03/16 23:53:58 christos Exp $	*/

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
#ifndef __ECOS
#include <sys/systm.h>
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#ifndef __ECOS
#include <sys/syslog.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <sys/sysctl.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>

#ifdef __ECOS
#include <stdio.h>    // for 'sprintf()'
#endif

#ifndef	IPFORWARDING
#ifdef GATEWAY
#define	IPFORWARDING	1	/* forward IP packets not for us */
#else /* GATEWAY */
#define	IPFORWARDING	0	/* don't forward IP packets not for us */
#endif /* GATEWAY */
#endif /* IPFORWARDING */
#ifndef	IPSENDREDIRECTS
#define	IPSENDREDIRECTS	1
#endif

int encdebug = 0;

/*
 * Note: DIRECTED_BROADCAST is handled this way so that previous
 * configuration using this option will Just Work.
 */
#ifndef IPDIRECTEDBCAST
#ifdef DIRECTED_BROADCAST
#define IPDIRECTEDBCAST	1
#else
#define	IPDIRECTEDBCAST	0
#endif /* DIRECTED_BROADCAST */
#endif /* IPDIRECTEDBCAST */
int	ipforwarding = IPFORWARDING;
int	ipsendredirects = IPSENDREDIRECTS;
int	ip_dosourceroute = 0;	/* no src-routing unless sysctl'd to enable */
int	ip_defttl = IPDEFTTL;
int	ip_directedbcast = IPDIRECTEDBCAST;
#ifdef DIAGNOSTIC
int	ipprintfs = 0;
#endif

int	ipsec_auth_default_level = IPSEC_AUTH_LEVEL_DEFAULT;
int	ipsec_esp_trans_default_level = IPSEC_ESP_TRANS_LEVEL_DEFAULT;
int	ipsec_esp_network_default_level = IPSEC_ESP_NETWORK_LEVEL_DEFAULT;

/* Keep track of memory used for reassembly */
int	ip_maxqueue = 300;
int	ip_frags = 0;

/* from in_pcb.c */
extern int ipport_firstauto;
extern int ipport_lastauto;
extern int ipport_hifirstauto;
extern int ipport_hilastauto;
extern struct baddynamicports baddynamicports;

extern	struct domain inetdomain;
extern	struct protosw inetsw[];
u_char	ip_protox[IPPROTO_MAX];
int	ipqmaxlen = IFQ_MAXLEN;
struct	in_ifaddrhead in_ifaddr;
struct	ifqueue ipintrq;
struct ipstat ipstat;
#if defined(IPFILTER) || defined(IPFILTER_LKM)
int	(*fr_checkp) __P((struct ip *, int, struct ifnet *, int,
			  struct mbuf **));
#endif

int	ipq_locked;
static __inline int ipq_lock_try __P((void));
static __inline void ipq_unlock __P((void));

static __inline int
ipq_lock_try()
{
	int s;

	s = splimp();
	if (ipq_locked) {
		splx(s);
		return (0);
	}
	ipq_locked = 1;
	splx(s);
	return (1);
}

#define ipq_lock() ipq_lock_try()

static __inline void
ipq_unlock()
{
	int s;

	s = splimp();
	ipq_locked = 0;
	splx(s);
}

#if 0 // Now in common layer

static char *ui8tod( cyg_uint8 n, char *p )
{
    if( n > 99 ) *p++ = (n/100) + '0';
    if( n >  9 ) *p++ = ((n/10)%10) + '0';
    *p++ = (n%10) + '0';
    return p;
}

char *
inet_ntoa(ina)
	struct in_addr ina;
{
	static char buf[4*sizeof "123"];
        char *p = buf;
	unsigned char *ucp = (unsigned char *)&ina;

//	sprintf(buf, "%d.%d.%d.%d", ucp[0] & 0xff, ucp[1] & 0xff,
//	    ucp[2] & 0xff, ucp[3] & 0xff);

        p = ui8tod( ucp[0] & 0xFF, p);
        *p++ = '.';
        p = ui8tod( ucp[1] & 0xFF, p);
        *p++ = '.';
        p = ui8tod( ucp[2] & 0xFF, p);
        *p++ = '.';
        p = ui8tod( ucp[3] & 0xFF, p);
        *p++ = '\0';
                    
	return (buf);
}
#endif

/*
 * We need to save the IP options in case a protocol wants to respond
 * to an incoming packet over the same route if the packet got here
 * using IP source routing.  This allows connection establishment and
 * maintenance when the remote end is on a network that is not known
 * to us.
 */
int	ip_nhops = 0;
static	struct ip_srcrt {
	struct	in_addr dst;			/* final destination */
	char	nop;				/* one NOP to align */
	char	srcopt[IPOPT_OFFSET + 1];	/* OPTVAL, OLEN and OFFSET */
	struct	in_addr route[MAX_IPOPTLEN/sizeof(struct in_addr)];
} ip_srcrt;

static void save_rte __P((u_char *, struct in_addr));
static int ip_weadvertise(u_int32_t);

/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init()
{
	register struct protosw *pr;
	register int i;
	const u_int16_t defbaddynamicports_tcp[] = DEFBADDYNAMICPORTS_TCP;
	const u_int16_t defbaddynamicports_udp[] = DEFBADDYNAMICPORTS_UDP;

	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = pr - inetsw;
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip_protox[pr->pr_protocol] = pr - inetsw;
	LIST_INIT(&ipq);
	ipintrq.ifq_maxlen = ipqmaxlen;
	TAILQ_INIT(&in_ifaddr);

	/* Fill in list of ports not to allocate dynamically. */
	bzero((void *)&baddynamicports, sizeof(baddynamicports));
	for (i = 0; defbaddynamicports_tcp[i] != 0; i++)
		DP_SET(baddynamicports.tcp, defbaddynamicports_tcp[i]);
	for (i = 0; defbaddynamicports_udp[i] != 0; i++)
		DP_SET(baddynamicports.udp, defbaddynamicports_tcp[i]);
}

struct	sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };
struct	route ipforward_rt;

void
ipintr()
{
	register struct mbuf *m;
	int s;

	if (needqueuedrain)
		m_reclaim();
	
	while (1) {
		/*
		 * Get next datagram off input queue and get IP header
		 * in first mbuf.
		 */
		s = splimp();
		IF_DEQUEUE(&ipintrq, m);
		splx(s);
		if (m == 0)
			return;
#ifdef	DIAGNOSTIC
		if ((m->m_flags & M_PKTHDR) == 0)
			panic("ipintr no HDR");
#endif
		ipv4_input(m, 0, NULL, 0);
	}
}

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
void
ipv4_input(struct mbuf *m, ...)
{
	register struct ip *ip;
	register struct ipq *fp;
	struct in_ifaddr *ia;
	struct ipqent *ipqe;
	int hlen, mff;
	va_list ap;
	int extra;

	va_start(ap, m);
	extra = va_arg(ap, int);
	va_end(ap);

	if (extra) {
		struct mbuf *newpacket;

		if (!(newpacket = m_split(m, extra, M_NOWAIT))) {
			m_freem(m);
			return;
		}

		newpacket->m_flags |= m->m_flags;
		m_freem(m);
		m = newpacket;
		extra = 0;
	}

	/*
	 * If no IP addresses have been set yet but the interfaces
	 * are receiving, can't do anything with incoming packets yet.
	 */
	if (in_ifaddr.tqh_first == 0)
		goto bad;
	ipstat.ips_total++;
	if (m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == 0) {
		ipstat.ips_toosmall++;
		return;
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION) {
		ipstat.ips_badvers++;
		goto bad;
	}
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
		ipstat.ips_badhlen++;
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			ipstat.ips_badhlen++;
			return;
		}
		ip = mtod(m, struct ip *);
	}
	if ((ip->ip_sum = in_cksum(m, hlen)) != 0) {
		ipstat.ips_badsum++;
		goto bad;
	}

	/*
	 * Convert fields to host representation.
	 */
	NTOHS(ip->ip_len);
	if (ip->ip_len < hlen) {
		ipstat.ips_badlen++;
		goto bad;
	}
	NTOHS(ip->ip_id);
	NTOHS(ip->ip_off);

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < ip->ip_len) {
		ipstat.ips_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > ip->ip_len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip->ip_len;
			m->m_pkthdr.len = ip->ip_len;
		} else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}

#if defined(IPFILTER) || defined(IPFILTER_LKM)
	 /*
	 * Check if we want to allow this packet to be processed.
	 * Consider it to be bad if not.
	 */
	{
		struct mbuf *m0 = m;
		if (fr_checkp && (*fr_checkp)(ip, hlen, m->m_pkthdr.rcvif, 0, &m0))
			return;
		ip = mtod(m = m0, struct ip *);
	}
#endif

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	ip_nhops = 0;		/* for source routed packets */
	if (hlen > sizeof (struct ip) && ip_dooptions(m))
		return;

	/*
	 * Check our list of addresses, to see if the packet is for us.
	 */
	if ((ia = in_iawithaddr(ip->ip_dst, m)) != NULL &&
	    (ia->ia_ifp->if_flags & IFF_UP))
		goto ours;

	if (IN_MULTICAST(ip->ip_dst.s_addr)) {
		struct in_multi *inm;
#ifdef MROUTING
		extern struct socket *ip_mrouter;

		if (m->m_flags & M_EXT) {
			if ((m = m_pullup(m, hlen)) == 0) {
				ipstat.ips_toosmall++;
				return;
			}
			ip = mtod(m, struct ip *);
		}

		if (ip_mrouter) {
			/*
			 * If we are acting as a multicast router, all
			 * incoming multicast packets are passed to the
			 * kernel-level multicast forwarding function.
			 * The packet is returned (relatively) intact; if
			 * ip_mforward() returns a non-zero value, the packet
			 * must be discarded, else it may be accepted below.
			 *
			 * (The IP ident field is put in the same byte order
			 * as expected when ip_mforward() is called from
			 * ip_output().)
			 */
			ip->ip_id = htons(ip->ip_id);
			if (ip_mforward(m, m->m_pkthdr.rcvif) != 0) {
				ipstat.ips_cantforward++;
				m_freem(m);
				return;
			}
			ip->ip_id = ntohs(ip->ip_id);

			/*
			 * The process-level routing demon needs to receive
			 * all multicast IGMP packets, whether or not this
			 * host belongs to their destination groups.
			 */
			if (ip->ip_p == IPPROTO_IGMP)
				goto ours;
			ipstat.ips_forward++;
		}
#endif
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN_LOOKUP_MULTI(ip->ip_dst, m->m_pkthdr.rcvif, inm);
		if (inm == NULL) {
			ipstat.ips_cantforward++;
			m_freem(m);
			return;
		}
		goto ours;
	}
	if (ip->ip_dst.s_addr == INADDR_BROADCAST ||
	    ip->ip_dst.s_addr == INADDR_ANY)
		goto ours;

	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (ipforwarding == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
	} else
		ip_forward(m, 0);
	return;

ours:
	/*
	 * If offset or IP_MF are set, must reassemble.
	 * Otherwise, nothing need be done.
	 * (We could look in the reassembly queue to see
	 * if the packet was previously fragmented,
	 * but it's not worth the time; just let them time out.)
	 */
	if (ip->ip_off &~ (IP_DF | IP_RF)) {
		if (m->m_flags & M_EXT) {		/* XXX */
			if ((m = m_pullup(m, hlen)) == 0) {
				ipstat.ips_toosmall++;
				return;
			}
			ip = mtod(m, struct ip *);
		}

		/*
		 * Look for queue of fragments
		 * of this datagram.
		 */
		ipq_lock();
		for (fp = ipq.lh_first; fp != NULL; fp = fp->ipq_q.le_next)
			if (ip->ip_id == fp->ipq_id &&
			    ip->ip_src.s_addr == fp->ipq_src.s_addr &&
			    ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
			    ip->ip_p == fp->ipq_p)
				goto found;
		fp = 0;
found:

		/*
		 * Adjust ip_len to not reflect header,
		 * set ipqe_mff if more fragments are expected,
		 * convert offset of this to bytes.
		 */
		ip->ip_len -= hlen;
		mff = (ip->ip_off & IP_MF) != 0;
		if (mff) {
			/*
			 * Make sure that fragments have a data length
			 * that's a non-zero multiple of 8 bytes.
			 */
			if (ip->ip_len == 0 || (ip->ip_len & 0x7) != 0) {
				ipstat.ips_badfrags++;
				ipq_unlock();
				goto bad;
			}
		}
		ip->ip_off <<= 3;

		/*
		 * If datagram marked as having more fragments
		 * or if this is not the first fragment,
		 * attempt reassembly; if it succeeds, proceed.
		 */
		if (mff || ip->ip_off) {
			ipstat.ips_fragments++;
			if (ip_frags + 1 > ip_maxqueue) {
				ip_flush();
				ipstat.ips_rcvmemdrop++;
				ipq_unlock();
				goto bad;
			}
			    
			MALLOC(ipqe, struct ipqent *, sizeof (struct ipqent),
			    M_IPQ, M_NOWAIT);
			if (ipqe == NULL) {
				ipstat.ips_rcvmemdrop++;
				ipq_unlock();
				goto bad;
			}
			ip_frags++;
			ipqe->ipqe_mff = mff;
			ipqe->ipqe_ip = ip;
			ip = ip_reass(ipqe, fp);
			if (ip == 0) {
				ipq_unlock();
				return;
			}
			ipstat.ips_reassembled++;
			m = dtom(ip);
			hlen = ip->ip_hl << 2;
		} else
			if (fp)
				ip_freef(fp);
		ipq_unlock();
	} else
		ip->ip_len -= hlen;

	/*
	 * Switch out to protocol's input routine.
	 */
	ipstat.ips_delivered++;
	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, hlen, NULL, 0);
	return;
bad:
	m_freem(m);
}

struct in_ifaddr *
in_iawithaddr(ina, m)
	struct in_addr ina;
	register struct mbuf *m;
{
	register struct in_ifaddr *ia;

	for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next) {
		if ((ina.s_addr == ia->ia_addr.sin_addr.s_addr) ||
		    ((ia->ia_ifp->if_flags & (IFF_LOOPBACK|IFF_LINK1)) ==
			(IFF_LOOPBACK|IFF_LINK1) &&
		     ia->ia_subnet == (ina.s_addr & ia->ia_subnetmask)))
			return ia;
		if (m && ((ip_directedbcast == 0) || (ip_directedbcast &&
		    ia->ia_ifp == m->m_pkthdr.rcvif)) &&
		    (ia->ia_ifp->if_flags & IFF_BROADCAST)) {
			if (ina.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
			    ina.s_addr == ia->ia_netbroadcast.s_addr ||
			    /*
			     * Look for all-0's host part (old broadcast addr),
			     * either for subnet or net.
			     */
			    ina.s_addr == ia->ia_subnet ||
			    ina.s_addr == ia->ia_net) {
				/* Make sure M_BCAST is set */
				m->m_flags |= M_BCAST;
				return ia;
			    }
		}
	}

	return NULL;
}

/*
 * Take incoming datagram fragment and try to
 * reassemble it into whole datagram.  If a chain for
 * reassembly of this datagram already exists, then it
 * is given as fp; otherwise have to make a chain.
 */
struct ip *
ip_reass(ipqe, fp)
	register struct ipqent *ipqe;
	register struct ipq *fp;
{
	register struct mbuf *m = dtom(ipqe->ipqe_ip);
	register struct ipqent *nq, *p, *q;
	struct ip *ip;
	struct mbuf *t;
	int hlen = ipqe->ipqe_ip->ip_hl << 2;
	int i, next;

	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	m->m_data += hlen;
	m->m_len -= hlen;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == 0) {
		if ((t = m_get(M_DONTWAIT, MT_FTABLE)) == NULL)
			goto dropfrag;
		fp = mtod(t, struct ipq *);
		LIST_INSERT_HEAD(&ipq, fp, ipq_q);
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ipqe->ipqe_ip->ip_p;
		fp->ipq_id = ipqe->ipqe_ip->ip_id;
		LIST_INIT(&fp->ipq_fragq);
		fp->ipq_src = ipqe->ipqe_ip->ip_src;
		fp->ipq_dst = ipqe->ipqe_ip->ip_dst;
		p = NULL;
		goto insert;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = fp->ipq_fragq.lh_first; q != NULL;
	    p = q, q = q->ipqe_q.le_next)
		if (q->ipqe_ip->ip_off > ipqe->ipqe_ip->ip_off)
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		i = p->ipqe_ip->ip_off + p->ipqe_ip->ip_len -
		    ipqe->ipqe_ip->ip_off;
		if (i > 0) {
			if (i >= ipqe->ipqe_ip->ip_len)
				goto dropfrag;
			m_adj(dtom(ipqe->ipqe_ip), i);
			ipqe->ipqe_ip->ip_off += i;
			ipqe->ipqe_ip->ip_len -= i;
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL && ipqe->ipqe_ip->ip_off + ipqe->ipqe_ip->ip_len >
	    q->ipqe_ip->ip_off; q = nq) {
		i = (ipqe->ipqe_ip->ip_off + ipqe->ipqe_ip->ip_len) -
		    q->ipqe_ip->ip_off;
		if (i < q->ipqe_ip->ip_len) {
			q->ipqe_ip->ip_len -= i;
			q->ipqe_ip->ip_off += i;
			m_adj(dtom(q->ipqe_ip), i);
			break;
		}
		nq = q->ipqe_q.le_next;
		m_freem(dtom(q->ipqe_ip));
		LIST_REMOVE(q, ipqe_q);
		FREE(q, M_IPQ);
		ip_frags--;
	}

insert:
	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 */
	if (p == NULL) {
		LIST_INSERT_HEAD(&fp->ipq_fragq, ipqe, ipqe_q);
	} else {
		LIST_INSERT_AFTER(p, ipqe, ipqe_q);
	}
	next = 0;
	for (p = NULL, q = fp->ipq_fragq.lh_first; q != NULL;
	    p = q, q = q->ipqe_q.le_next) {
		if (q->ipqe_ip->ip_off != next)
			return (0);
		next += q->ipqe_ip->ip_len;
	}
	if (p->ipqe_mff)
		return (0);

	/*
	 * Reassembly is complete.  Check for a bogus message size and
	 * concatenate fragments.
	 */
	q = fp->ipq_fragq.lh_first;
	ip = q->ipqe_ip;
	if ((next + (ip->ip_hl << 2)) > IP_MAXPACKET) {
		ipstat.ips_toolong++;
		ip_freef(fp);
		return (0);
	}
	m = dtom(q->ipqe_ip);
	t = m->m_next;
	m->m_next = 0;
	m_cat(m, t);
	nq = q->ipqe_q.le_next;
	FREE(q, M_IPQ);
	ip_frags--;
	for (q = nq; q != NULL; q = nq) {
		t = dtom(q->ipqe_ip);
		nq = q->ipqe_q.le_next;
		FREE(q, M_IPQ);
		ip_frags--;
		m_cat(m, t);
	}

	/*
	 * Create header for new ip packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip->ip_len = next;
	ip->ip_ttl = 0;	/* xxx */
	ip->ip_sum = 0;
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;
	LIST_REMOVE(fp, ipq_q);
	(void) m_free(dtom(fp));
	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);
	/* some debugging cruft by sklower, below, will go away soon */
	if (m->m_flags & M_PKTHDR) { /* XXX this should be done elsewhere */
		register int plen = 0;
		for (t = m; m; m = m->m_next)
			plen += m->m_len;
		t->m_pkthdr.len = plen;
	}
	return (ip);

dropfrag:
	ipstat.ips_fragdropped++;
	m_freem(m);
	FREE(ipqe, M_IPQ);
	ip_frags--;
	return (0);
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
ip_freef(fp)
	struct ipq *fp;
{
	register struct ipqent *q, *p;

	for (q = fp->ipq_fragq.lh_first; q != NULL; q = p) {
		p = q->ipqe_q.le_next;
		m_freem(dtom(q->ipqe_ip));
		LIST_REMOVE(q, ipqe_q);
		FREE(q, M_IPQ);
		ip_frags--;
	}
	LIST_REMOVE(fp, ipq_q);
	(void) m_free(dtom(fp));
}

/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
ip_slowtimo()
{
	register struct ipq *fp, *nfp;
	int s = splsoftnet();

	ipq_lock();
	for (fp = ipq.lh_first; fp != NULL; fp = nfp) {
		nfp = fp->ipq_q.le_next;
		if (--fp->ipq_ttl == 0) {
			ipstat.ips_fragtimeout++;
			ip_freef(fp);
		}
	}
	ipq_unlock();
	splx(s);
}

/*
 * Drain off all datagram fragments.
 */
void
ip_drain()
{

	if (ipq_lock_try() == 0)
		return;
	while (ipq.lh_first != NULL) {
		ipstat.ips_fragdropped++;
		ip_freef(ipq.lh_first);
	}
	ipq_unlock();
}

/*
 * Flush a bunch of datagram fragments, till we are down to 75%.
 */
void
ip_flush()
{
	int max = 50;

	/* ipq already locked */
	while (ipq.lh_first != NULL && ip_frags > ip_maxqueue * 3 / 4 && --max) {
		ipstat.ips_fragdropped++;
		ip_freef(ipq.lh_first);
	}
}

/*
 * Do option processing on a datagram,
 * possibly discarding it if bad options are encountered,
 * or forwarding it if source-routed.
 * Returns 1 if packet has been forwarded/freed,
 * 0 if the packet should be processed further.
 */
int
ip_dooptions(m)
	struct mbuf *m;
{
	register struct ip *ip = mtod(m, struct ip *);
	register u_char *cp;
	register struct ip_timestamp *ipt;
	register struct in_ifaddr *ia;
	int opt, optlen, cnt, off, code, type = ICMP_PARAMPROB, forward = 0;
	struct in_addr *sin, dst;
	n_time ntime;

	dst = ip->ip_dst;
	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
		}
		switch (opt) {

		default:
			break;

		/*
		 * Source routing with record.
		 * Find interface with current destination address.
		 * If none on this machine then drop if strictly routed,
		 * or do nothing if loosely routed.
		 * Record interface address and bring up next address
		 * component.  If strictly routed make sure next
		 * address is on directly accessible net.
		 */
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if (!ip_dosourceroute) {
#ifndef __ECOS
				char buf[4*sizeof "123"];

				strcpy(buf, inet_ntoa(ip->ip_dst));
				log(LOG_WARNING,
				    "attempted source route from %s to %s\n",
				    inet_ntoa(ip->ip_src), buf);
#endif
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			ipaddr.sin_addr = ip->ip_dst;
			ia = ifatoia(ifa_ifwithaddr(sintosa(&ipaddr)));
			if (ia == 0) {
				if (opt == IPOPT_SSRR) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				/*
				 * Loose routing, and not at next destination
				 * yet; nothing to do except forward.
				 */
				break;
			}
			off--;			/* 0 origin */
			if (off > optlen - sizeof(struct in_addr)) {
				/*
				 * End of source route.  Should be for us.
				 */
				save_rte(cp, ip->ip_src);
				break;
			}

			/*
			 * locate outgoing interface
			 */
			bcopy((caddr_t)(cp + off), (caddr_t)&ipaddr.sin_addr,
			    sizeof(ipaddr.sin_addr));
			if (opt == IPOPT_SSRR) {
#define	INA	struct in_ifaddr *
#define	SA	struct sockaddr *
			    if ((ia = (INA)ifa_ifwithdstaddr((SA)&ipaddr)) == 0)
				ia = (INA)ifa_ifwithnet((SA)&ipaddr);
			} else
				ia = ip_rtaddr(ipaddr.sin_addr);
			if (ia == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			ip->ip_dst = ipaddr.sin_addr;
			bcopy((caddr_t)&ia->ia_addr.sin_addr,
			    (caddr_t)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			/*
			 * Let ip_intr's mcast routing check handle mcast pkts
			 */
			forward = !IN_MULTICAST(ip->ip_dst.s_addr);
			break;

		case IPOPT_RR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}

			/*
			 * If no space remains, ignore.
			 */
			off--;			/* 0 origin */
			if (off > optlen - sizeof(struct in_addr))
				break;
			bcopy((caddr_t)(&ip->ip_dst), (caddr_t)&ipaddr.sin_addr,
			    sizeof(ipaddr.sin_addr));
			/*
			 * locate outgoing interface; if we're the destination,
			 * use the incoming interface (should be same).
			 */
			if ((ia = (INA)ifa_ifwithaddr((SA)&ipaddr)) == 0 &&
			    (ia = ip_rtaddr(ipaddr.sin_addr)) == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_HOST;
				goto bad;
			}
			bcopy((caddr_t)&ia->ia_addr.sin_addr,
			    (caddr_t)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;

		case IPOPT_TS:
			code = cp - (u_char *)ip;
			ipt = (struct ip_timestamp *)cp;
			if (ipt->ipt_ptr < 5 || ipt->ipt_len < 5)
				goto bad;
			if (ipt->ipt_ptr - 1 + sizeof(n_time) > ipt->ipt_len) {
				if (++ipt->ipt_oflw == 0)
					goto bad;
				break;
			}
			sin = (struct in_addr *)(cp + ipt->ipt_ptr - 1);
			switch (ipt->ipt_flg) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (ipt->ipt_ptr - 1 + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				ipaddr.sin_addr = dst;
				ia = (INA)ifaof_ifpforaddr((SA)&ipaddr,
							    m->m_pkthdr.rcvif);
				if (ia == 0)
					continue;
				bcopy((caddr_t)&ia->ia_addr.sin_addr,
				    (caddr_t)sin, sizeof(struct in_addr));
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (ipt->ipt_ptr - 1 + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				bcopy((caddr_t)sin, (caddr_t)&ipaddr.sin_addr,
				    sizeof(struct in_addr));
				if (ifa_ifwithaddr((SA)&ipaddr) == 0)
					continue;
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				goto bad;
			}
			ntime = iptime();
			bcopy((caddr_t)&ntime, (caddr_t)cp + ipt->ipt_ptr - 1,
			    sizeof(n_time));
			ipt->ipt_ptr += sizeof(n_time);
		}
	}
	if (forward && ipforwarding) {
		ip_forward(m, 1);
		return (1);
	}
	return (0);
bad:
	ip->ip_len -= ip->ip_hl << 2;   /* XXX icmp_error adds in hdr length */
	HTONS(ip->ip_len);	/* XXX because ip_input changed these three */
	HTONS(ip->ip_id);
	HTONS(ip->ip_off);
	icmp_error(m, type, code, 0, 0);
	ipstat.ips_badoptions++;
	return (1);
}

/*
 * Given address of next destination (final or next hop),
 * return internet address info of interface to be used to get there.
 */
struct in_ifaddr *
ip_rtaddr(dst)
	 struct in_addr dst;
{
	register struct sockaddr_in *sin;

	sin = satosin(&ipforward_rt.ro_dst);

	if (ipforward_rt.ro_rt == 0 || dst.s_addr != sin->sin_addr.s_addr) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = dst;

		rtalloc(&ipforward_rt);
	}
	if (ipforward_rt.ro_rt == 0)
		return ((struct in_ifaddr *)0);
	return (ifatoia(ipforward_rt.ro_rt->rt_ifa));
}

/*
 * Save incoming source route for use in replies,
 * to be picked up later by ip_srcroute if the receiver is interested.
 */
void
save_rte(option, dst)
	u_char *option;
	struct in_addr dst;
{
	unsigned olen;

	olen = option[IPOPT_OLEN];
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("save_rte: olen %d\n", olen);
#endif /* 0 */
	if (olen > sizeof(ip_srcrt) - (1 + sizeof(dst)))
		return;
	bcopy((caddr_t)option, (caddr_t)ip_srcrt.srcopt, olen);
	ip_nhops = (olen - IPOPT_OFFSET - 1) / sizeof(struct in_addr);
	ip_srcrt.dst = dst;
}

/*
 * Check whether we do proxy ARP for this address and we point to ourselves.
 * Code shamelessly copied from arplookup().
 */
static int
ip_weadvertise(addr)
	u_int32_t addr;
{
	register struct rtentry *rt;
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct sockaddr_inarp sin;

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_other = SIN_PROXY;
	rt = rtalloc1(sintosa(&sin), 0);
	if (rt == 0)
	  return 0;
	
	RTFREE(rt);
	
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK)
	  return 0;

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
		for (ifa = ifp->if_addrlist.tqh_first; ifa != 0;
		    ifa = ifa->ifa_list.tqe_next) {
			if (ifa->ifa_addr->sa_family != rt->rt_gateway->sa_family)
				continue;

			if (!bcmp(LLADDR((struct sockaddr_dl *)ifa->ifa_addr), 
			    LLADDR((struct sockaddr_dl *)rt->rt_gateway),
			    ETHER_ADDR_LEN))
				return 1;
		}

	return 0;
}

/*
 * Retrieve incoming source route for use in replies,
 * in the same form used by setsockopt.
 * The first hop is placed before the options, will be removed later.
 */
struct mbuf *
ip_srcroute()
{
	register struct in_addr *p, *q;
	register struct mbuf *m;

	if (ip_nhops == 0)
		return ((struct mbuf *)0);
	m = m_get(M_DONTWAIT, MT_SOOPTS);
	if (m == 0)
		return ((struct mbuf *)0);

#define OPTSIZ	(sizeof(ip_srcrt.nop) + sizeof(ip_srcrt.srcopt))

	/* length is (nhops+1)*sizeof(addr) + sizeof(nop + srcrt header) */
	m->m_len = ip_nhops * sizeof(struct in_addr) + sizeof(struct in_addr) +
	    OPTSIZ;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("ip_srcroute: nhops %d mlen %d", ip_nhops, m->m_len);
#endif

	/*
	 * First save first hop for return route
	 */
	p = &ip_srcrt.route[ip_nhops - 1];
	*(mtod(m, struct in_addr *)) = *p--;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf(" hops %x", ntohl(mtod(m, struct in_addr *)->s_addr));
#endif

	/*
	 * Copy option fields and padding (nop) to mbuf.
	 */
	ip_srcrt.nop = IPOPT_NOP;
	ip_srcrt.srcopt[IPOPT_OFFSET] = IPOPT_MINOFF;
	bcopy((caddr_t)&ip_srcrt.nop,
	    mtod(m, caddr_t) + sizeof(struct in_addr), OPTSIZ);
	q = (struct in_addr *)(mtod(m, caddr_t) +
	    sizeof(struct in_addr) + OPTSIZ);
#undef OPTSIZ
	/*
	 * Record return path as an IP source route,
	 * reversing the path (pointers are now aligned).
	 */
	while (p >= ip_srcrt.route) {
#ifdef DIAGNOSTIC
		if (ipprintfs)
			printf(" %x", ntohl(q->s_addr));
#endif
		*q++ = *p--;
	}
	/*
	 * Last hop goes to final destination.
	 */
	*q = ip_srcrt.dst;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf(" %x\n", ntohl(q->s_addr));
#endif
	return (m);
}

/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 * Second argument is buffer to which options
 * will be moved, and return value is their length.
 * XXX should be deleted; last arg currently ignored.
 */
void
ip_stripoptions(m, mopt)
	register struct mbuf *m;
	struct mbuf *mopt;
{
	register int i;
	struct ip *ip = mtod(m, struct ip *);
	register caddr_t opts;
	int olen;

	olen = (ip->ip_hl<<2) - sizeof (struct ip);
	opts = (caddr_t)(ip + 1);
	i = m->m_len - (sizeof (struct ip) + olen);
	bcopy(opts  + olen, opts, (unsigned)i);
	m->m_len -= olen;
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len -= olen;
	ip->ip_hl = sizeof(struct ip) >> 2;
}

int inetctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
void
ip_forward(m, srcrt)
	struct mbuf *m;
	int srcrt;
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct sockaddr_in *sin;
	register struct rtentry *rt;
	int error, type = 0, code = 0;
	struct mbuf *mcopy;
	n_long dest;
	struct ifnet *destifp;
#if 0 /*KAME IPSEC*/
	struct ifnet dummyifp;
#endif

	dest = 0;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("forward: src %x dst %x ttl %x\n", ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, ip->ip_ttl);
#endif
	if (m->m_flags & M_BCAST || in_canforward(ip->ip_dst) == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
		return;
	}
	HTONS(ip->ip_id);
	if (ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, dest, 0);
		return;
	}
	ip->ip_ttl -= IPTTLDEC;

	sin = satosin(&ipforward_rt.ro_dst);
	if ((rt = ipforward_rt.ro_rt) == 0 ||
	    ip->ip_dst.s_addr != sin->sin_addr.s_addr) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;

		rtalloc(&ipforward_rt);
		if (ipforward_rt.ro_rt == 0) {
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, dest, 0);
			return;
		}
		rt = ipforward_rt.ro_rt;
	}

	/*
	 * Save at most 68 bytes of the packet in case
	 * we need to generate an ICMP message to the src.
	 */
	mcopy = m_copy(m, 0, imin((int)ip->ip_len, 68));

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 * Don't send redirect if we advertise destination's arp address
	 * as ours (proxy arp).
	 */
	if (rt->rt_ifp == m->m_pkthdr.rcvif &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
	    satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
	    ipsendredirects && !srcrt &&
	    !ip_weadvertise(satosin(rt_key(rt))->sin_addr.s_addr)) {
		if (rt->rt_ifa &&
		    (ip->ip_src.s_addr & ifatoia(rt->rt_ifa)->ia_subnetmask) ==
		    ifatoia(rt->rt_ifa)->ia_subnet) {
		    if (rt->rt_flags & RTF_GATEWAY)
			dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
		    else
			dest = ip->ip_dst.s_addr;
		    /* Router requirements says to only send host redirects */
		    type = ICMP_REDIRECT;
		    code = ICMP_REDIRECT_HOST;
#ifdef DIAGNOSTIC
		    if (ipprintfs)
			printf("redirect (%d) to %x\n", code, (u_int32_t)dest);
#endif
		}
	}

#if 0 /*KAME IPSEC*/
	m->m_pkthdr.rcvif = NULL;
#endif /*IPSEC*/
	error = ip_output(m, (struct mbuf *)0, &ipforward_rt,
	    (IP_FORWARDING | (ip_directedbcast ? IP_ALLOWBROADCAST : 0)), 
	    0, NULL, NULL);
	if (error)
		ipstat.ips_cantforward++;
	else {
		ipstat.ips_forward++;
		if (type)
			ipstat.ips_redirectsent++;
		else {
			if (mcopy)
				m_freem(mcopy);
			return;
		}
	}
	if (mcopy == NULL)
		return;
	destifp = NULL;

	switch (error) {

	case 0:				/* forwarded, but need redirect */
		/* type, code set above */
		break;

	case ENETUNREACH:		/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;
#if 1 /*KAME IPSEC*/
		if (ipforward_rt.ro_rt)
			destifp = ipforward_rt.ro_rt->rt_ifp;
#else
		/*
		 * If the packet is routed over IPsec tunnel, tell the
		 * originator the tunnel MTU.
		 *	tunnel MTU = if MTU - sizeof(IP) - ESP/AH hdrsiz
		 * XXX quickhack!!!
		 */
		if (ipforward_rt.ro_rt) {
			struct secpolicy *sp;
			int ipsecerror;
			int ipsechdr;
			struct route *ro;

			sp = ipsec4_getpolicybyaddr(mcopy,
						    IP_FORWARDING,
						    &ipsecerror);

			if (sp == NULL)
				destifp = ipforward_rt.ro_rt->rt_ifp;
			else {
				/* count IPsec header size */
				ipsechdr = ipsec4_hdrsiz(mcopy, NULL);

				/*
				 * find the correct route for outer IPv4
				 * header, compute tunnel MTU.
				 *
				 * XXX BUG ALERT
				 * The "dummyifp" code relies upon the fact
				 * that icmp_error() touches only ifp->if_mtu.
				 */
				/*XXX*/
				destifp = NULL;
				if (sp->req != NULL
				 && sp->req->sa != NULL) {
					ro = &sp->req->sa->saidx->sa_route;
					if (ro->ro_rt && ro->ro_rt->rt_ifp) {
						dummyifp.if_mtu =
						    ro->ro_rt->rt_ifp->if_mtu;
						dummyifp.if_mtu -= ipsechdr;
						destifp = &dummyifp;
					}
				}

				key_freesp(sp);
			}
		}
#endif /*IPSEC*/
		ipstat.ips_cantfrag++;
		break;

	case ENOBUFS:
		type = ICMP_SOURCEQUENCH;
		code = 0;
		break;
	}

	icmp_error(mcopy, type, code, dest, destifp);
}

#ifdef CYGPKG_NET_SYSCTL
int
ip_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IPCTL_FORWARDING:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ipforwarding));
	case IPCTL_SENDREDIRECTS:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
			&ipsendredirects));
	case IPCTL_DEFTTL:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ip_defttl));
#ifdef notyet
	case IPCTL_DEFMTU:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ip_mtu));
#endif
	case IPCTL_SOURCEROUTE:
		/*
		 * Don't allow this to change in a secure environment.
		 */
		if (newp && securelevel > 0)
			return (EPERM);
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ip_dosourceroute));
	case IPCTL_DIRECTEDBCAST:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ip_directedbcast));
	case IPCTL_IPPORT_FIRSTAUTO:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ipport_firstauto));
	case IPCTL_IPPORT_LASTAUTO:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ipport_lastauto));
	case IPCTL_IPPORT_HIFIRSTAUTO:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ipport_hifirstauto));
	case IPCTL_IPPORT_HILASTAUTO:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ipport_hilastauto));
	case IPCTL_IPPORT_MAXQUEUE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ip_maxqueue));
	case IPCTL_ENCDEBUG:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &encdebug));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif // CYGPKG_NET_SYSCTL
