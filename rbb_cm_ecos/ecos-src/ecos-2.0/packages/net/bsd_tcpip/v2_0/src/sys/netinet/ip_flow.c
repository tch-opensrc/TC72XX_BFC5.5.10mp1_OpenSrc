//==========================================================================
//
//      src/sys/netinet/ip_flow.c
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the 3am Software Foundry ("3am").  It was developed by Matt Thomas.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netinet/ip_flow.c,v 1.9.2.1 2001/08/08 08:20:35 ru Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_flow.h>

#define	IPFLOW_TIMER		(5 * PR_SLOWHZ)
#define IPFLOW_HASHBITS		6	/* should not be a multiple of 8 */
#define	IPFLOW_HASHSIZE		(1 << IPFLOW_HASHBITS)
static LIST_HEAD(ipflowhead, ipflow) ipflows[IPFLOW_HASHSIZE];
static int ipflow_inuse;
#define	IPFLOW_MAX		256

static int ipflow_active = 0;

static unsigned
ipflow_hash(
	struct in_addr dst,
	struct in_addr src,
	unsigned tos)
{
	unsigned hash = tos;
	int idx;
	for (idx = 0; idx < 32; idx += IPFLOW_HASHBITS)
		hash += (dst.s_addr >> (32 - idx)) + (src.s_addr >> idx);
	return hash & (IPFLOW_HASHSIZE-1);
}

static struct ipflow *
ipflow_lookup(
	const struct ip *ip)
{
	unsigned hash;
	struct ipflow *ipf;

	hash = ipflow_hash(ip->ip_dst, ip->ip_src, ip->ip_tos);

	ipf = LIST_FIRST(&ipflows[hash]);
	while (ipf != NULL) {
		if (ip->ip_dst.s_addr == ipf->ipf_dst.s_addr
		    && ip->ip_src.s_addr == ipf->ipf_src.s_addr
		    && ip->ip_tos == ipf->ipf_tos)
			break;
		ipf = LIST_NEXT(ipf, ipf_next);
	}
	return ipf;
}

int
ipflow_fastforward(
	struct mbuf *m)
{
	struct ip *ip;
	struct ipflow *ipf;
	struct rtentry *rt;
	struct sockaddr *dst;
	int error;

	/*
	 * Are we forwarding packets?  Big enough for an IP packet?
	 */
	if (!ipforwarding || !ipflow_active || m->m_len < sizeof(struct ip))
		return 0;
	/*
	 * IP header with no option and valid version and length
	 */
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION || ip->ip_hl != (sizeof(struct ip) >> 2)
	    || ntohs(ip->ip_len) > m->m_pkthdr.len)
		return 0;
	/*
	 * Find a flow.
	 */
	if ((ipf = ipflow_lookup(ip)) == NULL)
		return 0;

	/*
	 * Route and interface still up?
	 */
	rt = ipf->ipf_ro.ro_rt;
	if ((rt->rt_flags & RTF_UP) == 0 || (rt->rt_ifp->if_flags & IFF_UP) == 0)
		return 0;

	/*
	 * Packet size OK?  TTL?
	 */
	if (m->m_pkthdr.len > rt->rt_ifp->if_mtu || ip->ip_ttl <= IPTTLDEC)
		return 0;

	/*
	 * Everything checks out and so we can forward this packet.
	 * Modify the TTL and incrementally change the checksum.
	 */
	ip->ip_ttl -= IPTTLDEC;
	if (ip->ip_sum >= htons(0xffff - (IPTTLDEC << 8))) {
		ip->ip_sum += htons(IPTTLDEC << 8) + 1;
	} else {
		ip->ip_sum += htons(IPTTLDEC << 8);
	}

	/*
	 * Send the packet on its way.  All we can get back is ENOBUFS
	 */
	ipf->ipf_uses++;
	ipf->ipf_timer = IPFLOW_TIMER;

	if (rt->rt_flags & RTF_GATEWAY)
		dst = rt->rt_gateway;
	else
		dst = &ipf->ipf_ro.ro_dst;
	if ((error = (*rt->rt_ifp->if_output)(rt->rt_ifp, m, dst, rt)) != 0) {
		if (error == ENOBUFS)
			ipf->ipf_dropped++;
		else
			ipf->ipf_errors++;
	}
	return 1;
}

static void
ipflow_addstats(
	struct ipflow *ipf)
{
	ipf->ipf_ro.ro_rt->rt_use += ipf->ipf_uses;
	ipstat.ips_cantforward += ipf->ipf_errors + ipf->ipf_dropped;
	ipstat.ips_forward += ipf->ipf_uses;
	ipstat.ips_fastforward += ipf->ipf_uses;
}

static void
ipflow_free(
	struct ipflow *ipf)
{
	int s;
	/*
	 * Remove the flow from the hash table (at elevated IPL).
	 * Once it's off the list, we can deal with it at normal
	 * network IPL.
	 */
	s = splimp();
	LIST_REMOVE(ipf, ipf_next);
	splx(s);
	ipflow_addstats(ipf);
	RTFREE(ipf->ipf_ro.ro_rt);
	ipflow_inuse--;
	FREE(ipf, M_IPFLOW);
}

static struct ipflow *
ipflow_reap(
	void)
{
	struct ipflow *ipf, *maybe_ipf = NULL;
	int idx;
	int s;

	for (idx = 0; idx < IPFLOW_HASHSIZE; idx++) {
		ipf = LIST_FIRST(&ipflows[idx]);
		while (ipf != NULL) {
			/*
			 * If this no longer points to a valid route
			 * reclaim it.
			 */
			if ((ipf->ipf_ro.ro_rt->rt_flags & RTF_UP) == 0)
				goto done;
			/*
			 * choose the one that's been least recently used
			 * or has had the least uses in the last 1.5 
			 * intervals.
			 */
			if (maybe_ipf == NULL
			    || ipf->ipf_timer < maybe_ipf->ipf_timer
			    || (ipf->ipf_timer == maybe_ipf->ipf_timer
				&& ipf->ipf_last_uses + ipf->ipf_uses <
				      maybe_ipf->ipf_last_uses +
					maybe_ipf->ipf_uses))
				maybe_ipf = ipf;
			ipf = LIST_NEXT(ipf, ipf_next);
		}
	}
	ipf = maybe_ipf;
    done:
	/*
	 * Remove the entry from the flow table.
	 */
	s = splimp();
	LIST_REMOVE(ipf, ipf_next);
	splx(s);
	ipflow_addstats(ipf);
	RTFREE(ipf->ipf_ro.ro_rt);
	return ipf;
}

void
ipflow_slowtimo(
	void)
{
	struct ipflow *ipf;
	int idx;

	for (idx = 0; idx < IPFLOW_HASHSIZE; idx++) {
		ipf = LIST_FIRST(&ipflows[idx]);
		while (ipf != NULL) {
			struct ipflow *next_ipf = LIST_NEXT(ipf, ipf_next);
			if (--ipf->ipf_timer == 0) {
				ipflow_free(ipf);
			} else {
				ipf->ipf_last_uses = ipf->ipf_uses;
				ipf->ipf_ro.ro_rt->rt_use += ipf->ipf_uses;
				ipstat.ips_forward += ipf->ipf_uses;
				ipstat.ips_fastforward += ipf->ipf_uses;
				ipf->ipf_uses = 0;
			}
			ipf = next_ipf;
		}
	}
}

void
ipflow_create(
	const struct route *ro,
	struct mbuf *m)
{
	const struct ip *const ip = mtod(m, struct ip *);
	struct ipflow *ipf;
	unsigned hash;
	int s;

	/*
	 * Don't create cache entries for ICMP messages.
	 */
	if (!ipflow_active || ip->ip_p == IPPROTO_ICMP)
		return;
	/*
	 * See if an existing flow struct exists.  If so remove it from it's
	 * list and free the old route.  If not, try to malloc a new one
	 * (if we aren't at our limit).
	 */
	ipf = ipflow_lookup(ip);
	if (ipf == NULL) {
		if (ipflow_inuse == IPFLOW_MAX) {
			ipf = ipflow_reap();
		} else {
			ipf = (struct ipflow *) malloc(sizeof(*ipf), M_IPFLOW,
						       M_NOWAIT);
			if (ipf == NULL)
				return;
			ipflow_inuse++;
		}
		bzero((caddr_t) ipf, sizeof(*ipf));
	} else {
		s = splimp();
		LIST_REMOVE(ipf, ipf_next);
		splx(s);
		ipflow_addstats(ipf);
		RTFREE(ipf->ipf_ro.ro_rt);
		ipf->ipf_uses = ipf->ipf_last_uses = 0;
		ipf->ipf_errors = ipf->ipf_dropped = 0;
	}

	/*
	 * Fill in the updated information.
	 */
	ipf->ipf_ro = *ro;
	ro->ro_rt->rt_refcnt++;
	ipf->ipf_dst = ip->ip_dst;
	ipf->ipf_src = ip->ip_src;
	ipf->ipf_tos = ip->ip_tos;
	ipf->ipf_timer = IPFLOW_TIMER;
	/*
	 * Insert into the approriate bucket of the flow table.
	 */
	hash = ipflow_hash(ip->ip_dst, ip->ip_src, ip->ip_tos);
	s = splimp();
	LIST_INSERT_HEAD(&ipflows[hash], ipf, ipf_next);
	splx(s);
}
