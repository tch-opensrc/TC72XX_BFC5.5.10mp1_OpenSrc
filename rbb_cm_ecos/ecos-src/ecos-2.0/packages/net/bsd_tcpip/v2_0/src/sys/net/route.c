//==========================================================================
//
//      src/sys/net/route.c
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
 * Copyright (c) 1980, 1986, 1991, 1993
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
 *	@(#)route.c	8.2 (Berkeley) 11/15/93
 * $FreeBSD: src/sys/net/route.c,v 1.59.2.3 2001/07/29 19:18:02 ume Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip_mroute.h>

#include <sys/sockio.h>  // for eCos routing protocols

#define	SA(p) ((struct sockaddr *)(p))

struct route_cb route_cb;
static struct rtstat rtstat;
struct radix_node_head *rt_tables[AF_MAX+1];

static int	rttrash;		/* routes not in table but not freed */

static void rt_maskedcopy __P((struct sockaddr *,
	    struct sockaddr *, struct sockaddr *));
static void rtable_init __P((void **));

static void
rtable_init(table)
	void **table;
{
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&table[dom->dom_family],
			    dom->dom_rtoffset);
}

void
route_init()
{
	rn_init();	/* initialize all zeroes, all ones, mask table */
	rtable_init((void **)rt_tables);
}

// FIXME: This function is only here because BOOTP fails on a second interface.
//        This failure is due to the fact that a route to 0.0.0.0 seems to be
//        incredibly sticky, i.e. can't be deleted.  BOOTP uses this to
//        achieve a generic broadcast.  Sadly it seems that BOOTP servers will
//        only work this way, thus the hack.
//
//        This version enumerates all routes and deletes them - this leaks less
//        store than the previous version.

static int
rt_reinit_rtdelete( struct radix_node *rn, void *vifp )
{
    struct rtentry *rt = (struct rtentry *)rn;
    rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway, rt_mask(rt),
              0, NULL);
    return (0);
}

void
cyg_route_reinit(void)
{
    int i;
    for (i = 0;  i < AF_MAX+1;  i++) {
        struct radix_node_head *rnh;
        rnh = rt_tables[i];
        if (rnh) {
            (*rnh->rnh_walktree)(rnh, rt_reinit_rtdelete, NULL);
        }
    }
}

/*
 * Packet routing routines.
 */
void
rtalloc(ro)
	register struct route *ro;
{
	rtalloc_ign(ro, 0UL);
}

void
rtalloc_ign(ro, ignore)
	register struct route *ro;
	u_long ignore;
{
	struct rtentry *rt;
	int s;

	if ((rt = ro->ro_rt) != NULL) {
		if (rt->rt_ifp != NULL && rt->rt_flags & RTF_UP)
			return;
		/* XXX - We are probably always at splnet here already. */
		s = splnet();
		RTFREE(rt);
		ro->ro_rt = NULL;
		splx(s);
	}
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1, ignore);
}

/*
 * Look up the route that matches the address given
 * Or, at least try.. Create a cloned route if needed.
 */
struct rtentry *
rtalloc1(dst, report, ignflags)
	register struct sockaddr *dst;
	int report;
	u_long ignflags;
{
	register struct radix_node_head *rnh = rt_tables[dst->sa_family];
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	struct rt_addrinfo info;
	u_long nflags;
	int  s = splnet(), err = 0, msgtype = RTM_MISS;

	/*
	 * Look up the address in the table for that Address Family
	 */
	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		/*
		 * If we find it and it's not the root node, then
		 * get a refernce on the rtentry associated.
		 */
		newrt = rt = (struct rtentry *)rn;
		nflags = rt->rt_flags & ~ignflags;
		if (report && (nflags & (RTF_CLONING | RTF_PRCLONING))) {
			/*
			 * We are apparently adding (report = 0 in delete).
			 * If it requires that it be cloned, do so.
			 * (This implies it wasn't a HOST route.)
			 */
			err = rtrequest(RTM_RESOLVE, dst, SA(0),
					      SA(0), 0, &newrt);
			if (err) {
				/*
				 * If the cloning didn't succeed, maybe
				 * what we have will do. Return that.
				 */
				newrt = rt;
				rt->rt_refcnt++;
				goto miss;
			}
			if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
				/*
				 * If the new route specifies it be
				 * externally resolved, then go do that.
				 */
				msgtype = RTM_RESOLVE;
				goto miss;
			}
		} else
			rt->rt_refcnt++;
	} else {
		/*
		 * Either we hit the root or couldn't find any match,
		 * Which basically means
		 * "caint get there frm here"
		 */
		rtstat.rts_unreach++;
	miss:	if (report) {
			/*
			 * If required, report the failure to the supervising
			 * Authorities.
			 * For a delete, this is not an error. (report == 0)
			 */
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	splx(s);
	return (newrt);
}

/*
 * Remove a reference count from an rtentry.
 * If the count gets low enough, take it out of the routing table
 */
void
rtfree(rt)
	register struct rtentry *rt;
{
	/*
	 * find the tree for that address family
	 */
	register struct radix_node_head *rnh =
		rt_tables[rt_key(rt)->sa_family];
	register struct ifaddr *ifa;

	if (rt == 0 || rnh == 0)
		panic("rtfree");

	/*
	 * decrement the reference count by one and if it reaches 0,
	 * and there is a close function defined, call the close function
	 */
	rt->rt_refcnt--;
	if(rnh->rnh_close && rt->rt_refcnt == 0) {
		rnh->rnh_close((struct radix_node *)rt, rnh);
	}

	/*
	 * If we are no longer "up" (and ref == 0)
	 * then we can free the resources associated
	 * with the route.
	 */
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		/*
		 * the rtentry must have been removed from the routing table
		 * so it is represented in rttrash.. remove that now.
		 */
		rttrash--;

#ifdef	DIAGNOSTIC
		if (rt->rt_refcnt < 0) {
			printf("rtfree: %p not freed (neg refs)\n", rt);
			return;
		}
#endif

		/*
		 * release references on items we hold them on..
		 * e.g other routes and ifaddrs.
		 */
		if((ifa = rt->rt_ifa))
			IFAFREE(ifa);
		if (rt->rt_parent) {
			RTFREE(rt->rt_parent);
		}

		/*
		 * The key is separatly alloc'd so free it (see rt_setgate()).
		 * This also frees the gateway, as they are always malloc'd
		 * together.
		 */
		Free(rt_key(rt));

		/*
		 * and the rtentry itself of course
		 */
		Free(rt);
	}
}

void
ifafree(ifa)
	register struct ifaddr *ifa;
{
	if (ifa == NULL)
		panic("ifafree");
	if (ifa->ifa_refcnt == 0)
		free(ifa, M_IFADDR);
	else
		ifa->ifa_refcnt--;
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 *
 */
void
rtredirect(dst, gateway, netmask, flags, src, rtp)
	struct sockaddr *dst, *gateway, *netmask, *src;
	int flags;
	struct rtentry **rtp;
{
	register struct rtentry *rt;
	int error = 0;
	short *stat = 0;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway)) == 0) {
		error = ENETUNREACH;
		goto out;
	}
	rt = rtalloc1(dst, 0, 0UL);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) \
	((a1)->sa_len == (a2)->sa_len && \
	 bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt &&
	     (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface.
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    netmask, flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			/*
			 * add the key and gateway (in one malloc'd chunk).
			 */
			rt_setgate(rt, rt_key(rt), gateway);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
out:
	if (error)
		rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);
}

/*
* Routing table ioctl interface.
*/
int
rtioctl(req, data, p)
	int req;
	caddr_t data;
	struct proc *p;
{
#ifdef INET
    struct ecos_rtentry *rt;
    int res;

    switch (req) {
    case SIOCADDRT:
        rt = (struct ecos_rtentry *)data;
        res = rtrequest(RTM_ADD, 
                        &rt->rt_dst,
                        &rt->rt_gateway,
                        &rt->rt_genmask,
                        rt->rt_flags,
                        NULL);
        return (res);
    case SIOCDELRT:
        rt = (struct ecos_rtentry *)data;
        res = rtrequest(RTM_DELETE, 
                        &rt->rt_dst,
                        &rt->rt_gateway,
                        &rt->rt_genmask,
                        rt->rt_flags,
                        NULL);
        return (res);
    default:
        break;
    }
	/* Multicast goop, grrr... */
#ifdef MROUTING
	return mrt_ioctl(req, data);
#else
	return mrt_ioctl(req, data, p);
#endif
#else /* INET */
	return ENXIO;
#endif /* INET */
}

struct ifaddr *
ifa_ifwithroute(flags, dst, gateway)
	int flags;
	struct sockaddr	*dst, *gateway;
{
	register struct ifaddr *ifa;
	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST) {
			ifa = ifa_ifwithdstaddr(dst);
		}
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == 0) {
		struct rtentry *rt = rtalloc1(dst, 0, 0UL);
		if (rt == 0)
			return (0);
		rt->rt_refcnt--;
		if ((ifa = rt->rt_ifa) == 0)
			return (0);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == 0)
			ifa = oifa;
	}
	return (ifa);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

static int rt_fixdelete __P((struct radix_node *, void *));
static int rt_fixchange __P((struct radix_node *, void *));

struct rtfc_arg {
	struct rtentry *rt0;
	struct radix_node_head *rnh;
};

/*
 * Expunges references to a route that's about to be reclaimed.
 * The route must be locked.
 */
int
rtexpunge(struct rtentry *rt)
{
	struct radix_node *rn;
	struct radix_node_head *rnh;
	struct ifaddr *ifa;
	int error = 0;

	//RT_LOCK_ASSERT(rt);
#if 0
	/*
	 * We cannot assume anything about the reference count
	 * because protocols call us in many situations; often
	 * before unwinding references to the table entry.
	 */
	KASSERT(rt->rt_refcnt <= 1, ("bogus refcnt %ld", rt->rt_refcnt));
#endif
	/*
	 * Find the correct routing tree to use for this Address Family
	 */
	rnh = rt_tables[rt_key(rt)->sa_family];
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	//RADIX_NODE_HEAD_LOCK(rnh);

	/*
	 * Remove the item from the tree; it should be there,
	 * but when callers invoke us blindly it may not (sigh).
	 */
	rn = rnh->rnh_deladdr(rt_key(rt), rt_mask(rt), rnh);
	if (rn == NULL) {
		error = ESRCH;
		goto bad;
	}
	//KASSERT((rn->rn_flags & (RNF_ACTIVE | RNF_ROOT)) == 0,
	//	("unexpected flags 0x%x", rn->rn_flags));
	//KASSERT(rt == RNTORT(rn),
	//	("lookup mismatch, rt %p rn %p", rt, rn));

	rt->rt_flags &= ~RTF_UP;

	/*
	 * Now search what's left of the subtree for any cloned
	 * routes which might have been formed from this node.
	 */
	if ((rt->rt_flags & RTF_CLONING) && rt_mask(rt))
		rnh->rnh_walktree_from(rnh, rt_key(rt), rt_mask(rt),
				       rt_fixdelete, rt);

	/*
	 * Remove any external references we may have.
	 * This might result in another rtentry being freed if
	 * we held its last reference.
	 */
	if (rt->rt_gwroute) {
		RTFREE(rt->rt_gwroute);
		rt->rt_gwroute = NULL;
	}

	/*
	 * Give the protocol a chance to keep things in sync.
	 */
	if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest) {
		struct rt_addrinfo info;

		bzero((caddr_t)&info, sizeof(info));
		info.rti_flags = rt->rt_flags;
		info.rti_info[RTAX_DST] = rt_key(rt);
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		ifa->ifa_rtrequest(RTM_DELETE, rt, &info);
	}

	/*
	 * one more rtentry floating around that is not
	 * linked to the routing table.
	 */
	rttrash++;
bad:
	//RADIX_NODE_HEAD_UNLOCK(rnh);
	return (error);
}


/*
 * Do appropriate manipulations of a routing tree given
 * all the bits of info needed
 */
int
rtrequest(req, dst, gateway, netmask, flags, ret_nrt)
	int req, flags;
	struct sockaddr *dst, *gateway, *netmask;
	struct rtentry **ret_nrt;
{
	int s = splnet(); int error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa;
	struct sockaddr *ndst;
#define senderr(x) { error = x ; goto bad; }

	/*
	 * Find the correct routing tree to use for this Address Family
	 */
	if ((rnh = rt_tables[dst->sa_family]) == 0)
    {
		senderr(ESRCH);
    }

	/*
	 * If we are adding a host route then we don't want to put
	 * a netmask in the tree
	 */
	if (flags & RTF_HOST)
    {
		netmask = 0;
    }

	switch (req) 
    {
	    case RTM_DELETE:
            {
		        /*
		        * Remove the item from the tree and return it.
		        * Complain if it is not there and do no more processing.
		        */
		        if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == 0)
                {
		        	senderr(ESRCH);
                }
		        if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
                {
		        	panic ("rtrequest delete");
                }
		        rt = (struct rtentry *)rn;
            
		        /*
		        * Now search what's left of the subtree for any cloned
		        * routes which might have been formed from this node.
		        */
		        if ((rt->rt_flags & (RTF_CLONING | RTF_PRCLONING)) && rt_mask(rt)) 
                {
		        	rnh->rnh_walktree_from(rnh, dst, rt_mask(rt), rt_fixdelete, rt);
		        }
                
		        /*
		        * Remove any external references we may have.
		        * This might result in another rtentry being freed if
		        * we held its last reference.
		        */
		        if (rt->rt_gwroute) 
                {
		            if (rt != rt->rt_gwroute)
                    {
		        	    RTFREE( rt->rt_gwroute ); // Free it up as normal
                    }
		            else
                    {
		        	    rt->rt_refcnt--; // Just dec the refcount - freeing
		        			             // it here would be premature
                    }
		            rt->rt_gwroute = NULL;
		        }

		        /*
		        * NB: RTF_UP must be set during the search above,
		        * because we might delete the last ref, causing
		        * rt to get freed prematurely.
		        *  eh? then why not just add a reference?
		        * I'm not sure how RTF_UP helps matters. (JRE)
		        */
		        rt->rt_flags &= ~RTF_UP;
                
		        /*
		        * give the protocol a chance to keep things in sync.
		        */
		        if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
                {
		        	ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
                }
                
		        /*
		        * one more rtentry floating around that is not
		        * linked to the routing table.
		        */
		        rttrash++;

		        /*
		        * If the caller wants it, then it can have it,
		        * but it's up to it to free the rtentry as we won't be
		        * doing it.
		        */
		        if (ret_nrt)
                {
		        	*ret_nrt = rt;
                }
		        else if (rt->rt_refcnt <= 0) 
                {
		        	rt->rt_refcnt++; /* make a 1->0 transition */
		        	rtfree(rt);
		        }
            }
		    break;

	    case RTM_RESOLVE:
            {
	    	    if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
                {
	    	    	senderr(EINVAL);
                }
	    	    ifa = rt->rt_ifa;
	    	    flags = rt->rt_flags & ~(RTF_CLONING | RTF_PRCLONING | RTF_STATIC);
	    	    flags |= RTF_WASCLONED;
	    	    gateway = rt->rt_gateway;
	    	    if ((netmask = rt->rt_genmask) == 0)
                {
	    	    	flags |= RTF_HOST;
                }
	    	    goto makeroute;
            }

	    case RTM_ADD:
            {
	    	    if ((flags & RTF_GATEWAY) && !gateway)
                {
	    	    	panic("rtrequest: GATEWAY but no gateway");
                }
                
	    	    if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == 0)
                {
	    	    	senderr(ENETUNREACH);
                }
            }

makeroute:
		    R_Malloc(rt, struct rtentry *, sizeof(*rt));
		    if (rt == 0)
            {
		    	senderr(ENOBUFS);
            }
		    Bzero(rt, sizeof(*rt));
		    rt->rt_flags = RTF_UP | flags;
		    /*
		    * Add the gateway. Possibly re-malloc-ing the storage for it
		    * also add the rt_gwroute if possible.
		    */
		    if ((error = rt_setgate(rt, dst, gateway)) != 0) 
            {
		    	Free(rt);
		    	senderr(error);
		    }

		    /*
		    * point to the (possibly newly malloc'd) dest address.
		    */
		    ndst = rt_key(rt);
            
		    /*
		    * make sure it contains the value we want (masked if needed).
		    */
		    if (netmask) 
            {
		    	rt_maskedcopy(dst, ndst, netmask);
		    } 
            else
            {
		    	Bcopy(dst, ndst, dst->sa_len);
            }

		    /*
		    * Note that we now have a reference to the ifa.
		    * This moved from below so that rnh->rnh_addaddr() can
		    * examine the ifa and  ifa->ifa_ifp if it so desires.
		    */
		    ifa->ifa_refcnt++;
		    rt->rt_ifa = ifa;
		    rt->rt_ifp = ifa->ifa_ifp;
		    /* XXX mtu manipulation will be done in rnh_addaddr -- itojun */
            
		    rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask, rnh, rt->rt_nodes);

		    if (rn == 0) 
            {
		    	struct rtentry *rt2;
		    	/*
		    	* Uh-oh, we already have one of these in the tree.
		    	* We do a special hack: if the route that's already
		    	* there was generated by the protocol-cloning
		    	* mechanism, then we just blow it away and retry
		    	* the insertion of the new one.
		    	*/
		    	rt2 = rtalloc1(dst, 0, RTF_PRCLONING);
		    	if (rt2 && rt2->rt_parent) 
                {
		    		rtrequest(RTM_DELETE, (struct sockaddr *)rt_key(rt2),
		    			  rt2->rt_gateway, rt_mask(rt2), rt2->rt_flags, 0);
		    		RTFREE(rt2);
		    		rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask, rnh, rt->rt_nodes);
		    	} 
                else if (rt2) 
                {
		    		/* undo the extra ref we got */
		    		RTFREE(rt2);
		    	}
		    }

		    /*
		    * If it still failed to go into the tree,
		    * then un-make it (this should be a function)
		    */
		    if (rn == 0) 
            {
		    	if (rt->rt_gwroute)
                {
		    		rtfree(rt->rt_gwroute);
                }
		    	if (rt->rt_ifa) 
                {
		    		IFAFREE(rt->rt_ifa);
		    	}

		    	Free(rt_key(rt));
		    	Free(rt);

                // begin cliffmod
                //senderr(EEXIST);
//                printf("rtrequest: allow interface on same subnet.\n");
                break;
                // end cliffmod
		    }
            
		    rt->rt_parent = 0;
            
		    /*
		    * If we got here from RESOLVE, then we are cloning
		    * so clone the rest, and note that we
		    * are a clone (and increment the parent's references)
		    */
		    if (req == RTM_RESOLVE) 
            {
		    	rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
		    	if ((*ret_nrt)->rt_flags & (RTF_CLONING | RTF_PRCLONING)) 
                {
		    		rt->rt_parent = (*ret_nrt);
		    		(*ret_nrt)->rt_refcnt++;
		    	}
		    }

		    /*
		    * if this protocol has something to add to this then
		    * allow it to do that as well.
		    */
		    if (ifa->ifa_rtrequest)
            {
		    	ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));
            }

		    /*
		    * We repeat the same procedure from rt_setgate() here because
		    * it doesn't fire when we call it there because the node
		    * hasn't been added to the tree yet.
		    */
		    if (!(rt->rt_flags & RTF_HOST) && rt_mask(rt) != 0) 
            {
		    	struct rtfc_arg arg;
		    	arg.rnh = rnh;
		    	arg.rt0 = rt;
		    	rnh->rnh_walktree_from(rnh, rt_key(rt), rt_mask(rt), rt_fixchange, &arg);
		    }
            
		    /*
		    * actually return a resultant rtentry and
		    * give the caller a single reference.
		    */
		    if (ret_nrt) 
            {
		    	*ret_nrt = rt;
		    	rt->rt_refcnt++;
		    }
		    break;
    } // end switch (req) 
bad:
	splx(s);
	return (error);
}

/*
 * Called from rtrequest(RTM_DELETE, ...) to fix up the route's ``family''
 * (i.e., the routes related to it by the operation of cloning).  This
 * routine is iterated over all potential former-child-routes by way of
 * rnh->rnh_walktree_from() above, and those that actually are children of
 * the late parent (passed in as VP here) are themselves deleted.
 */
static int
rt_fixdelete(rn, vp)
	struct radix_node *rn;
	void *vp;
{
	struct rtentry *rt = (struct rtentry *)rn;
	struct rtentry *rt0 = vp;

	if (rt->rt_parent == rt0 && !(rt->rt_flags & RTF_PINNED)) {
		return rtrequest(RTM_DELETE, rt_key(rt),
				 (struct sockaddr *)0, rt_mask(rt),
				 rt->rt_flags, (struct rtentry **)0);
	}
	return 0;
}

/*
 * This routine is called from rt_setgate() to do the analogous thing for
 * adds and changes.  There is the added complication in this case of a
 * middle insert; i.e., insertion of a new network route between an older
 * network route and (cloned) host routes.  For this reason, a simple check
 * of rt->rt_parent is insufficient; each candidate route must be tested
 * against the (mask, value) of the new route (passed as before in vp)
 * to see if the new route matches it.
 *
 * XXX - it may be possible to do fixdelete() for changes and reserve this
 * routine just for adds.  I'm not sure why I thought it was necessary to do
 * changes this way.
 */
#ifdef DEBUG
static int rtfcdebug = 0;
#endif

static int
rt_fixchange(rn, vp)
	struct radix_node *rn;
	void *vp;
{
	struct rtentry *rt = (struct rtentry *)rn;
	struct rtfc_arg *ap = vp;
	struct rtentry *rt0 = ap->rt0;
	struct radix_node_head *rnh = ap->rnh;
	u_char *xk1, *xm1, *xk2, *xmp;
	int i, len, mlen;

#ifdef DEBUG
	if (rtfcdebug)
		printf("rt_fixchange: rt %p, rt0 %p\n", rt, rt0);
#endif

	if (!rt->rt_parent || (rt->rt_flags & RTF_PINNED)) {
#ifdef DEBUG
		if(rtfcdebug) printf("no parent or pinned\n");
#endif
		return 0;
	}

	if (rt->rt_parent == rt0) {
#ifdef DEBUG
		if(rtfcdebug) printf("parent match\n");
#endif
		return rtrequest(RTM_DELETE, rt_key(rt),
				 (struct sockaddr *)0, rt_mask(rt),
				 rt->rt_flags, (struct rtentry **)0);
	}

	/*
	 * There probably is a function somewhere which does this...
	 * if not, there should be.
	 */
	len = imin(((struct sockaddr *)rt_key(rt0))->sa_len,
		   ((struct sockaddr *)rt_key(rt))->sa_len);

	xk1 = (u_char *)rt_key(rt0);
	xm1 = (u_char *)rt_mask(rt0);
	xk2 = (u_char *)rt_key(rt);

	/* avoid applying a less specific route */
	xmp = (u_char *)rt_mask(rt->rt_parent);
	mlen = ((struct sockaddr *)rt_key(rt->rt_parent))->sa_len;
	if (mlen > ((struct sockaddr *)rt_key(rt0))->sa_len) {
#ifdef DEBUG
		if (rtfcdebug)
			printf("rt_fixchange: inserting a less "
			       "specific route\n");
#endif
		return 0;
	}
	for (i = rnh->rnh_treetop->rn_offset; i < mlen; i++) {
		if ((xmp[i] & ~(xmp[i] ^ xm1[i])) != xmp[i]) {
#ifdef DEBUG
			if (rtfcdebug)
				printf("rt_fixchange: inserting a less "
				       "specific route\n");
#endif
			return 0;
		}
	}

	for (i = rnh->rnh_treetop->rn_offset; i < len; i++) {
		if ((xk2[i] & xm1[i]) != xk1[i]) {
#ifdef DEBUG
			if(rtfcdebug) printf("no match\n");
#endif
			return 0;
		}
	}

	/*
	 * OK, this node is a clone, and matches the node currently being
	 * changed/added under the node's mask.  So, get rid of it.
	 */
#ifdef DEBUG
	if(rtfcdebug) printf("deleting\n");
#endif
	return rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0,
			 rt_mask(rt), rt->rt_flags, (struct rtentry **)0);
}

int
rt_setgate(rt0, dst, gate)
	struct rtentry *rt0;
	struct sockaddr *dst, *gate;
{
	caddr_t new, old;
	int dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);
	register struct rtentry *rt = rt0;
	struct radix_node_head *rnh = rt_tables[dst->sa_family];

	/*
	 * A host route with the destination equal to the gateway
	 * will interfere with keeping LLINFO in the routing
	 * table, so disallow it.
	 */
	if (((rt0->rt_flags & (RTF_HOST|RTF_GATEWAY|RTF_LLINFO)) ==
					(RTF_HOST|RTF_GATEWAY)) &&
	    (dst->sa_len == gate->sa_len) &&
	    (bcmp(dst, gate, dst->sa_len) == 0)) {
		/*
		 * The route might already exist if this is an RTM_CHANGE
		 * or a routing redirect, so try to delete it.
		 */
		if (rt_key(rt0))
			rtrequest(RTM_DELETE, (struct sockaddr *)rt_key(rt0),
			    rt0->rt_gateway, rt_mask(rt0), rt0->rt_flags, 0);
		return EADDRNOTAVAIL;
	}

	/*
	 * Both dst and gateway are stored in the same malloc'd chunk
	 * (If I ever get my hands on....)
	 * if we need to malloc a new chunk, then keep the old one around
	 * till we don't need it any more.
	 */
	if (rt->rt_gateway == 0 || glen > ROUNDUP(rt->rt_gateway->sa_len)) {
		old = (caddr_t)rt_key(rt);
		R_Malloc(new, caddr_t, dlen + glen);
		if (new == 0)
			return ENOBUFS;
		rt->rt_nodes->rn_key = new;
	} else {
		/*
		 * otherwise just overwrite the old one
		 */
		new = rt->rt_nodes->rn_key;
		old = 0;
	}

	/*
	 * copy the new gateway value into the memory chunk
	 */
	Bcopy(gate, (rt->rt_gateway = (struct sockaddr *)(new + dlen)), glen);

	/*
	 * if we are replacing the chunk (or it's new) we need to
	 * replace the dst as well
	 */
	if (old) {
		Bcopy(dst, new, dlen);
		Free(old);
	}

	/*
	 * If there is already a gwroute, it's now almost definitly wrong
	 * so drop it.
	 */
	if (rt->rt_gwroute) {
		rt = rt->rt_gwroute; RTFREE(rt);
		rt = rt0; rt->rt_gwroute = 0;
	}
	/*
	 * Cloning loop avoidance:
	 * In the presence of protocol-cloning and bad configuration,
	 * it is possible to get stuck in bottomless mutual recursion
	 * (rtrequest rt_setgate rtalloc1).  We avoid this by not allowing
	 * protocol-cloning to operate for gateways (which is probably the
	 * correct choice anyway), and avoid the resulting reference loops
	 * by disallowing any route to run through itself as a gateway.
	 * This is obviously mandatory when we get rt->rt_output().
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		rt->rt_gwroute = rtalloc1(gate, 1, RTF_PRCLONING);
		if (rt->rt_gwroute == rt) {
			RTFREE(rt->rt_gwroute);
			rt->rt_gwroute = 0;
#define EDQUOT ENFILE
			return EDQUOT; /* failure */
		}
	}

	/*
	 * This isn't going to do anything useful for host routes, so
	 * don't bother.  Also make sure we have a reasonable mask
	 * (we don't yet have one during adds).
	 */
	if (!(rt->rt_flags & RTF_HOST) && rt_mask(rt) != 0) {
		struct rtfc_arg arg;
		arg.rnh = rnh;
		arg.rt0 = rt;
		rnh->rnh_walktree_from(rnh, rt_key(rt), rt_mask(rt),
				       rt_fixchange, &arg);
	}

	return 0;
}

static void
rt_maskedcopy(src, dst, netmask)
	struct sockaddr *src, *dst, *netmask;
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit(ifa, cmd, flags)
	register struct ifaddr *ifa;
	int cmd, flags;
{
	register struct rtentry *rt;
	register struct sockaddr *dst;
	register struct sockaddr *deldst;
	struct mbuf *m = 0;
	struct rtentry *nrt = 0;
	int error;

	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	/*
	 * If it's a delete, check that if it exists, it's on the correct
	 * interface or we might scrub a route to another ifa which would
	 * be confusing at best and possibly worse.
	 */
	if (cmd == RTM_DELETE) {
		/*
		 * It's a delete, so it should already exist..
		 * If it's a net, mask off the host bits
		 * (Assuming we have a mask)
		 */
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_DONTWAIT, MT_SONAME);
			if (m == NULL)
				return(ENOBUFS);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		/*
		 * Get an rtentry that is in the routing tree and
		 * contains the correct info. (if this fails, can't get there).
		 * We set "report" to FALSE so that if it doesn't exist,
		 * it doesn't report an error or clone a route, etc. etc.
		 */
		rt = rtalloc1(dst, 0, 0UL);
		if (rt) {
			/*
			 * Ok so we found the rtentry. it has an extra reference
			 * for us at this stage. we won't need that so
			 * lop that off now.
			 */
			rt->rt_refcnt--;
			if (rt->rt_ifa != ifa) {
				/*
				 * If the interface in the rtentry doesn't match
				 * the interface we are using, then we don't
				 * want to delete it, so return an error.
				 * This seems to be the only point of
				 * this whole RTM_DELETE clause.
				 */
				if (m)
					(void) m_free(m);
				return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
			}
		}
		/* XXX */
#if 0
		else {
			/*
			 * One would think that as we are deleting, and we know
			 * it doesn't exist, we could just return at this point
			 * with an "ELSE" clause, but apparently not..
			 */
			return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
		}
#endif
	}
	/*
	 * Do the actual request
	 */
	error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
			flags | ifa->ifa_flags, &nrt);
	if (m)
		(void) m_free(m);
	/*
	 * If we are deleting, and we found an entry, then
	 * it's been removed from the tree.. now throw it away.
	 */
	if (cmd == RTM_DELETE && error == 0 && (rt = nrt)) {
		/*
		 * notify any listenning routing agents of the change
		 */
		rt_newaddrmsg(cmd, ifa, error, nrt);
		if (rt->rt_refcnt <= 0) {
			rt->rt_refcnt++; /* need a 1->0 transition to free */
			rtfree(rt);
		}
	}

	/*
	 * We are adding, and we have a returned routing entry.
	 * We need to sanity check the result.
	 */
	if (cmd == RTM_ADD && error == 0 && (rt = nrt)) {
		/*
		 * We just wanted to add it.. we don't actually need a reference
		 */
		rt->rt_refcnt--;
		/*
		 * If it came back with an unexpected interface, then it must
		 * have already existed or something. (XXX)
		 */
		if (rt->rt_ifa != ifa) {
			if (!(rt->rt_ifa->ifa_ifp->if_flags &
			    (IFF_POINTOPOINT|IFF_LOOPBACK)))
				printf("rtinit: wrong ifa (%p) was (%p)\n",
				    ifa, rt->rt_ifa);
			/*
			 * Ask that the protocol in question
			 * remove anything it has associated with
			 * this route and ifaddr.
			 */
			if (rt->rt_ifa->ifa_rtrequest)
			    rt->rt_ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
			/*
			 * Remove the reference to its ifaddr.
			 */
			IFAFREE(rt->rt_ifa);
			/*
			 * And substitute in references to the ifaddr
			 * we are adding.
			 */
			rt->rt_ifa = ifa;
			rt->rt_ifp = ifa->ifa_ifp;
			rt->rt_rmx.rmx_mtu = ifa->ifa_ifp->if_mtu;	/*XXX*/
			ifa->ifa_refcnt++;
			/*
			 * Now ask the protocol to check if it needs
			 * any special processing in its new form.
			 */
			if (ifa->ifa_rtrequest)
			    ifa->ifa_rtrequest(RTM_ADD, rt, SA(0));
		}
		/*
		 * notify any listenning routing agents of the change
		 */
		rt_newaddrmsg(cmd, ifa, error, nrt);
	}
	return (error);
}

/* This must be before ip6_init2(), which is now SI_ORDER_MIDDLE */
SYSINIT(route, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, route_init, 0);
