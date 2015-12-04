//==========================================================================
//
//      src/sys/netinet/ip_output.c
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
 * $FreeBSD: src/sys/netinet/ip_output.c,v 1.99.2.16 2001/07/19 06:37:26 kris Exp $
 */

#define _IP_VHL

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#ifdef IPSEC_DEBUG
#include <netkey/key_debug.h>
#else
#define	KEYDEBUG(lev,arg)
#endif
#endif /*IPSEC*/

#include <netinet/ip_fw.h>

#ifdef DUMMYNET
#include <netinet/ip_dummynet.h>
#endif

// begin cliffmod
#include <netinet/if_bcm.h> 
#include <cyg/infra/diag.h>		// For diagnostic printing.    
void _show_all_interfaces(void);
typedef void pr_fun(char *fmt, ...);
void show_network_tables(pr_fun *pr);
// end cliffmod

#ifdef IPFIREWALL_FORWARD_DEBUG  
#define print_ip(a)	 printf("%ld.%ld.%ld.%ld",(ntohl(a.s_addr)>>24)&0xFF,\
				 		  (ntohl(a.s_addr)>>16)&0xFF,\
						  (ntohl(a.s_addr)>>8)&0xFF,\
						  (ntohl(a.s_addr))&0xFF);
#endif   

u_short ip_id;

struct mbuf *ip_insertoptions __P((struct mbuf *, struct mbuf *, int *));   // cliffmod
static struct ifnet *ip_multicast_if __P((struct in_addr *, int *));
static void	ip_mloopback
	__P((struct ifnet *, struct mbuf *, struct sockaddr_in *, int));
static int	ip_getmoptions
	__P((struct sockopt *, struct ip_moptions *));
static int	ip_pcbopts __P((int, struct mbuf **, struct mbuf *));
static int	ip_setmoptions
	__P((struct sockopt *, struct ip_moptions **));

int	ip_optcopy __P((struct ip *, struct ip *));
extern int (*fr_checkp) __P((struct ip *, int, struct ifnet *, int, struct mbuf **));

// begin cliffmod
extern u_int in_cksum_hdr(const struct ip *ip);
extern u_short in_cksum_skip(struct mbuf *m, int len, int skip);
// end cliffmod

extern	struct protosw inetsw[];

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
ip_output(m0, opt, ro, flags, imo)
	struct mbuf *m0;
	struct mbuf *opt;
	struct route *ro;
	int flags;
	struct ip_moptions *imo;
{
	struct ip *ip, *mhip;
	struct ifnet *ifp = 0;  // cliffmod
	struct mbuf *m = m0;
	int hlen = sizeof (struct ip);
	int len, off, error = 0;
	struct sockaddr_in *dst;
	struct in_ifaddr *ia = NULL;
	int isbroadcast, sw_csum;
#ifdef IPSEC
	struct route iproute;
	struct socket *so = NULL;
	struct secpolicy *sp = NULL;
#endif
	u_int16_t divert_cookie;		/* firewall cookie */
#ifdef IPFIREWALL_FORWARD
	int fwd_rewrite_src = 0;
#endif
	struct ip_fw_chain *rule = NULL;
  
#ifdef IPDIVERT
	/* Get and reset firewall cookie */
	divert_cookie = ip_divert_cookie;
	ip_divert_cookie = 0;
#else
	divert_cookie = 0;
#endif

    struct pifnet_result rtmod_result;  // cliffmod
    struct route *porig_route = 0;      // cliffmod
    struct route local_route;           // cliffmod          
    
#if defined(IPFIREWALL) && defined(DUMMYNET)
        /*  
         * dummynet packet are prepended a vestigial mbuf with
         * m_type = MT_DUMMYNET and m_data pointing to the matching
         * rule.
         */ 
        if (m->m_type == MT_DUMMYNET) {
            /*
             * the packet was already tagged, so part of the
             * processing was already done, and we need to go down.
             * Get parameters from the header.
             */
            rule = (struct ip_fw_chain *)(m->m_data) ;
	    opt = NULL ;
	    ro = & ( ((struct dn_pkt *)m)->ro ) ;
	    imo = NULL ;
	    dst = ((struct dn_pkt *)m)->dn_dst ;
	    ifp = ((struct dn_pkt *)m)->ifp ;
	    flags = ((struct dn_pkt *)m)->flags ;

            m0 = m = m->m_next ;
#ifdef IPSEC
	    so = ipsec_getsocket(m);
	    (void)ipsec_setsocket(m, NULL);
#endif
            ip = mtod(m, struct ip *);
            hlen = IP_VHL_HL(ip->ip_vhl) << 2 ;
            if (ro->ro_rt != NULL)
                ia = (struct in_ifaddr *)ro->ro_rt->rt_ifa;
            goto sendit;
        } else
            rule = NULL ;
#endif
#ifdef IPSEC
	so = ipsec_getsocket(m);
	(void)ipsec_setsocket(m, NULL);
#endif

#ifdef	DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("ip_output no HDR");
	if (!ro)
		panic("ip_output no route, proto = %d",
		      mtod(m, struct ip *)->ip_p);
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
		ip->ip_vhl = IP_MAKE_VHL(IPVERSION, hlen >> 2);
		ip->ip_off &= IP_DF;
#ifdef RANDOM_IP_ID
		ip->ip_id = ip_randomid();
#else
		ip->ip_id = htons(ip_id++);
#endif
		ipstat.ips_localout++;
	} else {
		hlen = IP_VHL_HL(ip->ip_vhl) << 2;
	}

    // begin cliffmod
    rtmod_result.result_code = RTMODS_NO_CHANGE;
    if( ip->ip_p == 6 )
    {
        // tcp packet...
        // store original route in case custom routing fails.
        porig_route = ro;
        ro = 0;
    }
try_orig_route:
    rtmod_result.result_code = RTMODS_NO_CHANGE;
	if( ro == 0 ) 
    {
        // ro passed param is NULL. --> point to local var and continue.
		ro = &local_route;
		bzero((caddr_t)ro, sizeof (*ro));
	}
    // end cliffmod

	dst = (struct sockaddr_in *)&ro->ro_dst;
	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up.  If not, free it and try again.
	 */
	if (ro->ro_rt 
        && ( (ro->ro_rt->rt_flags & RTF_UP) == 0 || dst->sin_addr.s_addr != ip->ip_dst.s_addr) ) 
    {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}

	if (ro->ro_rt == 0) 
    {
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_dst;
	}
	/*
	 * If routing to interface only,
	 * short circuit routing lookup.
	 */
#define ifatoia(ifa)	((struct in_ifaddr *)(ifa))
#define sintosa(sin)	((struct sockaddr *)(sin))
	if (flags & IP_ROUTETOIF) 
    {
		if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == 0 
            && (ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == 0) 
        {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}
		ifp = ia->ia_ifp;
		ip->ip_ttl = 1;
		isbroadcast = in_broadcast(dst->sin_addr, ifp);
	} 
    else 
    {
		/*
		 * If this is the case, we probably don't want to allocate
		 * a protocol-cloned route since we didn't get one from the
		 * ULP.  This lets TCP do its thing, while not burdening
		 * forwarding or ICMP with the overhead of cloning a route.
		 * Of course, we still want to do any cloning requested by
		 * the link layer, as this is probably required in all cases
		 * for correct operation (as it is for ARP).
		 */
        // begin cliffmod
		if (ro->ro_rt == 0)
        {
            #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
            diag_printf("ip_output() - about to call rtalloc_ign\n");
            #endif

			rtalloc_ign(ro, RTF_PRCLONING);
            
            if( ro->ro_rt )
            {
                if( ro->ro_rt->rt_ifp )
                {
                    ifp = ro->ro_rt->rt_ifp;

                    #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                    diag_printf("ip_output() - rtalloc_ign selected interface %s%d\n", 
                        ifp->if_name, ifp->if_unit);
                    #endif
                }
                else
                {
                    #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                    diag_printf("ip_output() - rtalloc_ign selected route w/NULL net if...no good can come of this!\n");
                    #endif
                }
            }
            else
            {                
                #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                diag_printf("ip_output() - rtalloc_ign returned NULL ro->ro_rt...no good can come of this!\n");
                #endif                
            }
            
            if( ro->ro_rt == 0 )
            {
                // rtalloc_ign returned NULL ro->ro_rt.  stack doesn't have a clue
                // where to route packet.  --> try custom routing.
                rtmod_result = BcmDefIpOutputRoutingMods( ip, ro );
            }
            else if( (ro->ro_rt != 0) && (strcmp(ro->ro_rt->rt_ifp->if_name, "lo") != 0) )
            {
                // route is specified for a network interface other than
                // the loopback interface.  --> try custom routing.
                //
                rtmod_result = BcmDefIpOutputRoutingMods( ip, ro );
            }

            if( rtmod_result.result_code == RTMODS_DIFF_NET_IF )
            {
                // route has been changed.
                //                    
                if( ro->ro_rt != 0 )
                {
                    // discard previous route.
                    RTFREE(ro->ro_rt);
	                ro->ro_rt = (struct rtentry *)0;
                }
                //
                // note new outbound network interface .
                ifp = rtmod_result.pifnet;
                //
                // compute isbroadcast var.
                isbroadcast = in_broadcast( ip->ip_dst, ifp ); 
                // 
                // setup ia var for use by existing code.
                ia = ifatoia( rtmod_result.pifaddr );
                //
                #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                diag_printf("ip_output() - selected interface %s%d\n", ifp->if_name, ifp->if_unit);
                #endif
            }
            else if( rtmod_result.result_code == RTMODS_NO_CHANGE )
            {
                #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                diag_printf("ip_output() - using rtalloc selected net interface.\n");
                #endif
            }
            else
            {
                // else assume route could not be determined.
                //
                #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                diag_printf("ip_output() - brcm code cannot determine dest net/if.\n");
                #endif
                //
                if( ro->ro_rt != 0 )
                {
                    // discard rtalloc route since we thought it was junk...
                    RTFREE(ro->ro_rt);
	                ro->ro_rt = (struct rtentry *)0;
                }
                //
                if( porig_route )
                {
                    // non-null original route.
                    // --> try original route as last resort...
                    ro = porig_route;
                    porig_route = 0;
                    if( ro->ro_rt )
                    {
                        ifp = ro->ro_rt->rt_ifp;
                    }
                    else
                    {
                        ifp = 0;
                    }
                    //
                    // we haven't changed the original route...
                    rtmod_result.result_code = RTMODS_NO_CHANGE;
                    //
                    #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                    diag_printf("ip_output() - try original route as last resort..\n");
                    #endif
                    //
                    // goto just like the bsd guys!
                    goto try_orig_route; 
                }
                //
                #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
                diag_printf("ip_output() - cannot determine dest net/if.  cannot send packet!\n");
                #endif
                //
                ipstat.ips_noroute++;
                error = EHOSTUNREACH;
                goto bad;

            } // end if if( rtmod_result.result_code == RTMODS_DIFF_NET_IF )
        } // end if (ro->ro_rt == 0) 

        if( rtmod_result.result_code == RTMODS_NO_CHANGE )
        {
            // if route NOT modified by brcm code...
            #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
            diag_printf("ip_output() - route NOT modified by brcm code.\n");
            #endif

		    if (ro->ro_rt == 0) 
            {
		    	ipstat.ips_noroute++;
		    	error = EHOSTUNREACH;
		    	goto bad;
		    }
        }

        if( ro->ro_rt )
        {
            // explicit route specified...
            #if (DMP_STACK_DEBUG & DMP_DEBUG_IP_OUTPUT)
            diag_printf("ip_output() - explicit route specified.\n");
            #endif

		    ia = ifatoia(ro->ro_rt->rt_ifa);
		    ifp = ro->ro_rt->rt_ifp;
		    ro->ro_rt->rt_use++;
		    if (ro->ro_rt->rt_flags & RTF_GATEWAY)
		    	dst = (struct sockaddr_in *)ro->ro_rt->rt_gateway;
		    if (ro->ro_rt->rt_flags & RTF_HOST)
            {
		    	isbroadcast = (ro->ro_rt->rt_flags & RTF_BROADCAST);
            }
		    else
            {
		    	isbroadcast = in_broadcast(dst->sin_addr, ifp);
            }
        } // end if( ro->ro_rt )
        // end cliffmod
    } // end if (flags & IP_ROUTETOIF) 

	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		struct in_multi *inm;

		m->m_flags |= M_MCAST;
		/*
		 * IP destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = (struct sockaddr_in *)&ro->ro_dst;
		/*
		 * See if the caller provided any multicast options
		 */
		if (imo != NULL) {
			ip->ip_ttl = imo->imo_multicast_ttl;
			if (imo->imo_multicast_ifp != NULL)
				ifp = imo->imo_multicast_ifp;
			if (imo->imo_multicast_vif != -1)
				ip->ip_src.s_addr =
				    ip_mcast_src(imo->imo_multicast_vif);
		} else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;
		/*
		 * Confirm that the outgoing interface supports multicast.
		 */
		if ((imo == NULL) || (imo->imo_multicast_vif == -1)) {
			if ((ifp->if_flags & IFF_MULTICAST) == 0) {
				ipstat.ips_noroute++;
				error = ENETUNREACH;
				goto bad;
			}
		}
		/*
		 * If source address not specified yet, use address
		 * of outgoing interface.
		 */
		if (ip->ip_src.s_addr == INADDR_ANY) {
			register struct in_ifaddr *ia1;

			TAILQ_FOREACH(ia1, &in_ifaddrhead, ia_link)
				if (ia1->ia_ifp == ifp) {
					ip->ip_src = IA_SIN(ia1)->sin_addr;
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
			ip_mloopback(ifp, m, dst, hlen);
		}
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
			if (ip_mrouter && (flags & IP_FORWARDING) == 0) {
				/*
				 * Check if rsvp daemon is running. If not, don't
				 * set ip_moptions. This ensures that the packet
				 * is multicast and not just sent down one link
				 * as prescribed by rsvpd.
				 */
				if (!rsvp_on)
				  imo = NULL;
				if (ip_mforward(ip, ifp, m, imo) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}

		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip_ttl == 0 || ifp->if_flags & IFF_LOOPBACK) {
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
	if (ip->ip_src.s_addr == INADDR_ANY) {
		ip->ip_src = IA_SIN(ia)->sin_addr;
#ifdef IPFIREWALL_FORWARD
		/* Keep note that we did this - if the firewall changes
		 * the next-hop, our interface may change, changing the
		 * default source IP. It's a shame so much effort happens
		 * twice. Oh well. 
		 */
		fwd_rewrite_src++;
#endif /* IPFIREWALL_FORWARD */
	}
#endif /* notdef */
#ifdef ALTQ
	/*
	 * disable packet drop hack.
	 * packetdrop should be done by queueing.
	 */
#else /* !ALTQ */
	/*
	 * Verify that we have any chance at all of being able to queue
	 *      the packet or packet fragments
	 */
	if ((ifp->if_snd.ifq_len + ip->ip_len / ifp->if_mtu + 1) >=
		ifp->if_snd.ifq_maxlen) {
			error = ENOBUFS;
			goto bad;
	}
#endif /* !ALTQ */

	/*
	 * Look for broadcast address and
	 * and verify user is allowed to send
	 * such a packet.
	 */
	if (isbroadcast) 
    {
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IP_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
		/* don't allow broadcast messages to be fragmented */
		if ((u_short)ip->ip_len > ifp->if_mtu) {
			error = EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} 
    else 
    {
		m->m_flags &= ~M_BCAST;
	}

sendit:
#ifdef IPSEC
	/* get SP for this packet */
	if (so == NULL)
		sp = ipsec4_getpolicybyaddr(m, IPSEC_DIR_OUTBOUND, flags, &error);
	else
		sp = ipsec4_getpolicybysock(m, IPSEC_DIR_OUTBOUND, so, &error);

	if (sp == NULL) {
		ipsecstat.out_inval++;
		goto bad;
	}

	error = 0;

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		/*
		 * This packet is just discarded.
		 */
		ipsecstat.out_polvio++;
		goto bad;

	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		/* no need to do IPsec. */
		goto skip_ipsec;
	
	case IPSEC_POLICY_IPSEC:
		if (sp->req == NULL) {
			/* acquire a policy */
			error = key_spdacquire(sp);
			goto bad;
		}
		break;

	case IPSEC_POLICY_ENTRUST:
	default:
		printf("ip_output: Invalid policy found. %d\n", sp->policy);
	}
    {
	struct ipsec_output_state state;
	bzero(&state, sizeof(state));
	state.m = m;
	if (flags & IP_ROUTETOIF) {
		state.ro = &iproute;
		bzero(&iproute, sizeof(iproute));
	} else
		state.ro = ro;
	state.dst = (struct sockaddr *)dst;

	ip->ip_sum = 0;

	/*
	 * XXX
	 * delayed checksums are not currently compatible with IPsec
	 */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}

	HTONS(ip->ip_len);
	HTONS(ip->ip_off);

	error = ipsec4_output(&state, sp, flags);

	m = state.m;
	if (flags & IP_ROUTETOIF) {
		/*
		 * if we have tunnel mode SA, we may need to ignore
		 * IP_ROUTETOIF.
		 */
		if (state.ro != &iproute || state.ro->ro_rt != NULL) {
			flags &= ~IP_ROUTETOIF;
			ro = state.ro;
		}
	} else
		ro = state.ro;
	dst = (struct sockaddr_in *)state.dst;
	if (error) {
		/* mbuf is already reclaimed in ipsec4_output. */
		m0 = NULL;
		switch (error) {
		case EHOSTUNREACH:
		case ENETUNREACH:
		case EMSGSIZE:
		case ENOBUFS:
		case ENOMEM:
			break;
		default:
			printf("ip4_output (ipsec): error code %d\n", error);
			/*fall through*/
		case ENOENT:
			/* don't show these error codes to the user */
			error = 0;
			break;
		}
		goto bad;
	}

	/* be sure to update variables that are affected by ipsec4_output() */
	ip = mtod(m, struct ip *);
#ifdef _IP_VHL
	hlen = IP_VHL_HL(ip->ip_vhl) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif
	if (ro->ro_rt == NULL) {
		if ((flags & IP_ROUTETOIF) == 0) {
			printf("ip_output: "
				"can't update route after IPsec processing\n");
			error = EHOSTUNREACH;	/*XXX*/
			goto bad;
		}
	} else {
		if (state.encap) {
			ia = ifatoia(ro->ro_rt->rt_ifa);
			ifp = ro->ro_rt->rt_ifp;
		}
	}
    }

	/* make it flipped, again. */
	NTOHS(ip->ip_len);
	NTOHS(ip->ip_off);
skip_ipsec:
#endif /*IPSEC*/


	/*
	 * IpHack's section.
	 * - Xlate: translate packet's addr/port (NAT).
	 * - Firewall: deny/allow/etc.
	 * - Wrap: fake packet's addr/port <unimpl.>
	 * - Encapsulate: put it in another IP and send out. <unimp.>
	 */ 
	if (fr_checkp) {
		struct  mbuf    *m1 = m;

		if ((error = (*fr_checkp)(ip, hlen, ifp, 1, &m1)) || !m1)
			goto done;
		ip = mtod(m = m1, struct ip *);
	}

	/*
	 * Check with the firewall...
	 */
	if (fw_enable && ip_fw_chk_ptr) {
		struct sockaddr_in *old = dst;

		off = (*ip_fw_chk_ptr)(&ip,
		    hlen, ifp, &divert_cookie, &m, &rule, &dst);
                /*
                 * On return we must do the following:
                 * IP_FW_PORT_DENY_FLAG		-> drop the pkt (XXX new)
                 * 1<=off<= 0xffff   -> DIVERT
                 * (off & IP_FW_PORT_DYNT_FLAG)	-> send to a DUMMYNET pipe
                 * (off & IP_FW_PORT_TEE_FLAG)	-> TEE the packet
                 * dst != old        -> IPFIREWALL_FORWARD
                 * off==0, dst==old  -> accept
                 * If some of the above modules is not compiled in, then
                 * we should't have to check the corresponding condition
                 * (because the ipfw control socket should not accept
                 * unsupported rules), but better play safe and drop
                 * packets in case of doubt.
                 */
		if ( (off & IP_FW_PORT_DENY_FLAG) || m == NULL) {
			if (m)
				m_freem(m);
			error = EACCES ;
			goto done ;
		}
		ip = mtod(m, struct ip *);
		if (off == 0 && dst == old) /* common case */
			goto pass ;
#ifdef DUMMYNET
                if ((off & IP_FW_PORT_DYNT_FLAG) != 0) {
                    /*
                     * pass the pkt to dummynet. Need to include
                     * pipe number, m, ifp, ro, dst because these are
                     * not recomputed in the next pass.
                     * All other parameters have been already used and
                     * so they are not needed anymore. 
                     * XXX note: if the ifp or ro entry are deleted
                     * while a pkt is in dummynet, we are in trouble!
                     */ 
		    error = dummynet_io(off & 0xffff, DN_TO_IP_OUT, m,
				ifp,ro,dst,rule, flags);
		    goto done;
		}
#endif   
#ifdef IPDIVERT
		if (off != 0 && (off & IP_FW_PORT_DYNT_FLAG) == 0) {
			struct mbuf *clone = NULL;

			/* Clone packet if we're doing a 'tee' */
			if ((off & IP_FW_PORT_TEE_FLAG) != 0)
				clone = m_dup(m, M_DONTWAIT);

			/*
			 * XXX
			 * delayed checksums are not currently compatible
			 * with divert sockets.
			 */
			if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
				in_delayed_cksum(m);
				m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
			}

			/* Restore packet header fields to original values */
			HTONS(ip->ip_len);
			HTONS(ip->ip_off);

			/* Deliver packet to divert input routine */
			ip_divert_cookie = divert_cookie;
			divert_packet(m, 0, off & 0xffff);

			/* If 'tee', continue with original packet */
			if (clone != NULL) {
				m = clone;
				ip = mtod(m, struct ip *);
				goto pass;
			}
			goto done;
		}
#endif

#ifdef IPFIREWALL_FORWARD
		/* Here we check dst to make sure it's directly reachable on the
		 * interface we previously thought it was.
		 * If it isn't (which may be likely in some situations) we have
		 * to re-route it (ie, find a route for the next-hop and the
		 * associated interface) and set them here. This is nested
		 * forwarding which in most cases is undesirable, except where
		 * such control is nigh impossible. So we do it here.
		 * And I'm babbling.
		 */
		if (off == 0 && old != dst) {
			struct in_ifaddr *ia;

			/* It's changed... */
			/* There must be a better way to do this next line... */
			static struct route sro_fwd, *ro_fwd = &sro_fwd;
#ifdef IPFIREWALL_FORWARD_DEBUG
			printf("IPFIREWALL_FORWARD: New dst ip: ");
			dump_in_addr(dst->sin_addr);
#endif
			/*
			 * We need to figure out if we have been forwarded
			 * to a local socket. If so then we should somehow 
			 * "loop back" to ip_input, and get directed to the
			 * PCB as if we had received this packet. This is
			 * because it may be dificult to identify the packets
			 * you want to forward until they are being output
			 * and have selected an interface. (e.g. locally
			 * initiated packets) If we used the loopback inteface,
			 * we would not be able to control what happens 
			 * as the packet runs through ip_input() as
			 * it is done through a ISR.
			 */
			TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
				/*
				 * If the addr to forward to is one
				 * of ours, we pretend to
				 * be the destination for this packet.
				 */
				if (IA_SIN(ia)->sin_addr.s_addr ==
						 dst->sin_addr.s_addr)
					break;
			}
			if (ia) {
				/* tell ip_input "dont filter" */
				ip_fw_fwd_addr = dst;
				if (m->m_pkthdr.rcvif == NULL)
					m->m_pkthdr.rcvif = ifunit("lo0");
				if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
					m->m_pkthdr.csum_flags |=
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
					m0->m_pkthdr.csum_data = 0xffff;
				}
				m->m_pkthdr.csum_flags |=
				    CSUM_IP_CHECKED | CSUM_IP_VALID;
				HTONS(ip->ip_len);
				HTONS(ip->ip_off);
				ip_input(m);
				goto done;
			}
			/* Some of the logic for this was
			 * nicked from above.
			 *
			 * This rewrites the cached route in a local PCB.
			 * Is this what we want to do?
			 */
			bcopy(dst, &ro_fwd->ro_dst, sizeof(*dst));

			ro_fwd->ro_rt = 0;
			rtalloc_ign(ro_fwd, RTF_PRCLONING);

			if (ro_fwd->ro_rt == 0) {
				ipstat.ips_noroute++;
				error = EHOSTUNREACH;
				goto bad;
			}

			ia = ifatoia(ro_fwd->ro_rt->rt_ifa);
			ifp = ro_fwd->ro_rt->rt_ifp;
			ro_fwd->ro_rt->rt_use++;
			if (ro_fwd->ro_rt->rt_flags & RTF_GATEWAY)
            {
				dst = (struct sockaddr_in *)ro_fwd->ro_rt->rt_gateway;
            }
			if (ro_fwd->ro_rt->rt_flags & RTF_HOST)
            {
				isbroadcast = (ro_fwd->ro_rt->rt_flags & RTF_BROADCAST);
            }
			else
            {
				isbroadcast = in_broadcast(dst->sin_addr, ifp);
            }
			RTFREE(ro->ro_rt);
			ro->ro_rt = ro_fwd->ro_rt;
			dst = (struct sockaddr_in *)&ro_fwd->ro_dst;

			/*
			 * If we added a default src ip earlier,
			 * which would have been gotten from the-then
			 * interface, do it again, from the new one.
			 */
			if (fwd_rewrite_src)
				ip->ip_src = IA_SIN(ia)->sin_addr;
			goto pass ;
		}
#endif /* IPFIREWALL_FORWARD */
                /*
                 * if we get here, none of the above matches, and 
                 * we have to drop the pkt
                 */
		m_freem(m);
                error = EACCES; /* not sure this is the right error msg */
                goto done;
	}

pass:
	m->m_pkthdr.csum_flags |= CSUM_IP;
	sw_csum = m->m_pkthdr.csum_flags & ~ifp->if_hwassist;
	if (sw_csum & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		sw_csum &= ~CSUM_DELAY_DATA;
	}
	m->m_pkthdr.csum_flags &= ifp->if_hwassist;

	/*
	 * If small enough for interface, or the interface will take
	 * care of the fragmentation for us, can just send directly.
	 */
	if ((u_short)ip->ip_len <= ifp->if_mtu || ifp->if_hwassist & CSUM_FRAGMENT) 
    {
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
		ip->ip_sum = 0;
		if (sw_csum & CSUM_DELAY_IP) {
			if (ip->ip_vhl == IP_VHL_BORING) {
				ip->ip_sum = in_cksum_hdr(ip);   
			} else {
				ip->ip_sum = in_cksum(m, hlen);
			}
		}

		/* Record statistics for this interface address. */
		if (!(flags & IP_FORWARDING) && ia != NULL) {
			ia->ia_ifa.if_opackets++;
			ia->ia_ifa.if_obytes += m->m_pkthdr.len;
		}

#ifdef IPSEC
		/* clean ipsec history once it goes out of the node */
		ipsec_delaux(m);
#endif

		error = (*ifp->if_output)(ifp, m,
				(struct sockaddr *)dst, ro->ro_rt);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
		error = EMSGSIZE;
		/*
		 * This case can happen if the user changed the MTU
		 * of an interface after enabling IP on it.  Because
		 * most netifs don't keep track of routes pointing to
		 * them, there is no way for one to update all its
		 * routes when the MTU is changed.
		 */
		if ((ro->ro_rt->rt_flags & (RTF_UP | RTF_HOST))
		    && !(ro->ro_rt->rt_rmx.rmx_locks & RTV_MTU)
		    && (ro->ro_rt->rt_rmx.rmx_mtu > ifp->if_mtu)) {
			ro->ro_rt->rt_rmx.rmx_mtu = ifp->if_mtu;
		}
		ipstat.ips_cantfrag++;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

	/*
	 * if the interface will not calculate checksums on
	 * fragmented packets, then do it here.
	 */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA &&
	    (ifp->if_hwassist & CSUM_IP_FRAGS) == 0) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}

    {
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_nextpkt;
	int nfrags = 1;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < (u_short)ip->ip_len; off += len) {
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			ipstat.ips_odropped++;
			goto sendorfree;
		}
		m->m_flags |= (m0->m_flags & M_MCAST) | M_FRAG;
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_vhl = IP_MAKE_VHL(IPVERSION, mhlen >> 2);
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= (u_short)ip->ip_len)
			len = (u_short)ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			(void) m_free(m);
			error = ENOBUFS;	/* ??? */
			ipstat.ips_odropped++;
			goto sendorfree;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = (struct ifnet *)0;
		m->m_pkthdr.csum_flags = m0->m_pkthdr.csum_flags;
		HTONS(mhip->ip_off);
		mhip->ip_sum = 0;
		if (sw_csum & CSUM_DELAY_IP) {
			if (mhip->ip_vhl == IP_VHL_BORING) {
				mhip->ip_sum = in_cksum_hdr(mhip);
			} else {
				mhip->ip_sum = in_cksum(m, mhlen);
			}
		}
		*mnext = m;
		mnext = &m->m_nextpkt;
		nfrags++;
	}
	ipstat.ips_ofragments += nfrags;

	/* set first/last markers for fragment chain */
	m->m_flags |= M_LASTFRAG;
	m0->m_flags |= M_FIRSTFRAG | M_FRAG;
	m0->m_pkthdr.csum_data = nfrags;

	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m = m0;
	m_adj(m, hlen + firstlen - (u_short)ip->ip_len);
	m->m_pkthdr.len = hlen + firstlen;
	ip->ip_len = htons((u_short)m->m_pkthdr.len);
	ip->ip_off |= IP_MF;
	HTONS(ip->ip_off);
	ip->ip_sum = 0;
	if (sw_csum & CSUM_DELAY_IP) {
		if (ip->ip_vhl == IP_VHL_BORING) {
			ip->ip_sum = in_cksum_hdr(ip);
		} else {
			ip->ip_sum = in_cksum(m, hlen);
		}
	}
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
#ifdef IPSEC
		/* clean ipsec history once it goes out of the node */
		ipsec_delaux(m);
#endif
		if (error == 0) {
			/* Record statistics for this interface address. */
			if (ia != NULL) {
				ia->ia_ifa.if_opackets++;
				ia->ia_ifa.if_obytes += m->m_pkthdr.len;
			}
			
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
		} else
			m_freem(m);
	}

	if (error == 0)
		ipstat.ips_fragmented++;
    }
done:
#ifdef IPSEC
	if (ro == &iproute && ro->ro_rt) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = NULL;
	}
	if (sp != NULL) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ip_output call free SP:%p\n", sp));
		key_freesp(sp);
	}
#endif /* IPSEC */
	return (error);
bad:
	m_freem(m0);
	goto done;
}

void
in_delayed_cksum(struct mbuf *m)
{
	struct ip *ip;
	u_short csum, offset;

	ip = mtod(m, struct ip *);
	offset = IP_VHL_HL(ip->ip_vhl) << 2 ;
	csum = in_cksum_skip(m, ip->ip_len, offset);
	if (m->m_pkthdr.csum_flags & CSUM_UDP && csum == 0)
		csum = 0xffff;
	offset += m->m_pkthdr.csum_data;	/* checksum offset */

	if (offset + sizeof(u_short) > m->m_len) {
		printf("delayed m_pullup, m->len: %d  off: %d  p: %d\n",
		    m->m_len, offset, ip->ip_p);
		/*
		 * XXX
		 * this shouldn't happen, but if it does, the
		 * correct behavior may be to insert the checksum
		 * in the existing chain instead of rearranging it.
		 */
		m = m_pullup(m, offset + sizeof(u_short));
	}
	*(u_short *)(m->m_data + offset) = csum;
}

/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 *
 * XXX This routine assumes that the packet has no options in place.
 */
struct mbuf *ip_insertoptions(m, opt, phlen)    // cliffmod
	register struct mbuf *m;
	struct mbuf *opt;
	int *phlen;
{
	register struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	register struct ip *ip = mtod(m, struct ip *);
	unsigned optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + (u_short)ip->ip_len > IP_MAXPACKET)
		return (m);		/* XXX should fail */
	if (p->ipopt_dst.s_addr)
		ip->ip_dst = p->ipopt_dst;
	if (m->m_flags & M_EXT || m->m_data - optlen < m->m_pktdat) {
		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n == 0)
			return (m);
		n->m_pkthdr.rcvif = (struct ifnet *)0;
		n->m_pkthdr.len = m->m_pkthdr.len + optlen;
		m->m_len -= sizeof(struct ip);
		m->m_data += sizeof(struct ip);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ip);
		m->m_data += max_linkhdr;
		(void)memcpy(mtod(m, void *), ip, sizeof(struct ip));
	} else {
		m->m_data -= optlen;
		m->m_len += optlen;
		m->m_pkthdr.len += optlen;
		ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	}
	ip = mtod(m, struct ip *);
	bcopy(p->ipopt_list, ip + 1, optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_vhl = IP_MAKE_VHL(IPVERSION, *phlen >> 2);
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
	cnt = (IP_VHL_HL(ip->ip_vhl) << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP) {
			/* Preserve for IP mcast tunnel's LSRR alignment. */
			*dp++ = IPOPT_NOP;
			optlen = 1;
			continue;
		}
#ifdef DIAGNOSTIC
		if (cnt < IPOPT_OLEN + sizeof(*cp))
			panic("malformed IPv4 option passed to ip_optcopy");
#endif
		optlen = cp[IPOPT_OLEN];
#ifdef DIAGNOSTIC
		if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt)
			panic("malformed IPv4 option passed to ip_optcopy");
#endif
		/* bogus lengths should have been caught by ip_dooptions */
		if (optlen > cnt)
			optlen = cnt;
		if (IPOPT_COPIED(opt)) {
			bcopy(cp, dp, optlen);
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
ip_ctloutput(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	struct	inpcb *inp = sotoinpcb(so);
	int	error, optval;

	error = optval = 0;
	if (sopt->sopt_level != IPPROTO_IP) {
		return (EINVAL);
	}

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
		case IP_OPTIONS:
#ifdef notyet
		case IP_RETOPTS:
#endif
		{
			struct mbuf *m;
			if (sopt->sopt_valsize > MLEN) {
				error = EMSGSIZE;
				break;
			}
			MGET(m, sopt->sopt_p ? M_WAIT : M_DONTWAIT, MT_HEADER);
			if (m == 0) {
				error = ENOBUFS;
				break;
			}
			m->m_len = sopt->sopt_valsize;
			error = sooptcopyin(sopt, mtod(m, char *), m->m_len,
					    m->m_len);
			
			return (ip_pcbopts(sopt->sopt_name, &inp->inp_options,
					   m));
		}

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
#if defined(NFAITH) && NFAITH > 0
		case IP_FAITH:
#endif
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			switch (sopt->sopt_name) {
			case IP_TOS:
				inp->inp_ip_tos = optval;
				break;

			case IP_TTL:
				inp->inp_ip_ttl = optval;
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

			case IP_RECVIF:
				OPTSET(INP_RECVIF);
				break;

#if defined(NFAITH) && NFAITH > 0
			case IP_FAITH:
				OPTSET(INP_FAITH);
				break;
#endif
			}
			break;
#undef OPTSET

		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(sopt, &inp->inp_moptions);
			break;

		case IP_PORTRANGE:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

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
			break;

#ifdef IPSEC
		case IP_IPSEC_POLICY:
		{
			caddr_t req;
			size_t len = 0;
			int priv;
			struct mbuf *m;
			int optname;

			if ((error = soopt_getm(sopt, &m)) != 0) /* XXX */
				break;
			if ((error = soopt_mcopyin(sopt, m)) != 0) /* XXX */
				break;
			priv = (sopt->sopt_p != NULL &&
				suser(sopt->sopt_p) != 0) ? 0 : 1;
			req = mtod(m, caddr_t);
			len = m->m_len;
			optname = sopt->sopt_name;
			error = ipsec4_set_policy(inp, optname, req, len, priv);
			m_freem(m);
			break;
		}
#endif /*IPSEC*/

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case SOPT_GET:
		switch (sopt->sopt_name) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			if (inp->inp_options)
				error = sooptcopyout(sopt, 
						     mtod(inp->inp_options,
							  char *),
						     inp->inp_options->m_len);
			else
				sopt->sopt_valsize = 0;
			break;

		case IP_TOS:
		case IP_TTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
		case IP_PORTRANGE:
#if defined(NFAITH) && NFAITH > 0
		case IP_FAITH:
#endif
			switch (sopt->sopt_name) {

			case IP_TOS:
				optval = inp->inp_ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip_ttl;
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

			case IP_RECVIF:
				optval = OPTBIT(INP_RECVIF);
				break;

			case IP_PORTRANGE:
				if (inp->inp_flags & INP_HIGHPORT)
					optval = IP_PORTRANGE_HIGH;
				else if (inp->inp_flags & INP_LOWPORT)
					optval = IP_PORTRANGE_LOW;
				else
					optval = 0;
				break;

#if defined(NFAITH) && NFAITH > 0
			case IP_FAITH:
				optval = OPTBIT(INP_FAITH);
				break;
#endif
			}
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_VIF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_getmoptions(sopt, inp->inp_moptions);
			break;

#ifdef IPSEC
		case IP_IPSEC_POLICY:
		{
			struct mbuf *m = NULL;
			caddr_t req = NULL;
			size_t len = 0;

			if (m != 0) {
				req = mtod(m, caddr_t);
				len = m->m_len;
			}
			error = ipsec4_get_policy(sotoinpcb(so), req, len, &m);
			if (error == 0)
				error = soopt_mcopyout(sopt, m); /* XXX */
			if (error == 0)
				m_freem(m);
			break;
		}
#endif /*IPSEC*/

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
static int
ip_pcbopts(optname, pcbopt, m)
	int optname;
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
			if (cnt < IPOPT_OLEN + sizeof(*cp))
				goto bad;
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt)
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
 * XXX
 * The whole multicast option thing needs to be re-thought.
 * Several of these options are equally applicable to non-multicast
 * transmission, and one (IP_MULTICAST_TTL) totally duplicates a
 * standard option (IP_TTL).
 */

/*
 * following RFC1724 section 3.3, 0.0.0.0/8 is interpreted as interface index.
 */
static struct ifnet *
ip_multicast_if(a, ifindexp)
	struct in_addr *a;
	int *ifindexp;
{
	int ifindex;
	struct ifnet *ifp;

	if (ifindexp)
		*ifindexp = 0;
	if (ntohl(a->s_addr) >> 24 == 0) {
		ifindex = ntohl(a->s_addr) & 0xffffff;
		if (ifindex < 0 || if_index < ifindex)
			return NULL;
		ifp = ifindex2ifnet[ifindex];
		if (ifindexp)
			*ifindexp = ifindex;
	} else {
		INADDR_TO_IFP(*a, ifp);
	}
	return ifp;
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
static int
ip_setmoptions(sopt, imop)
	struct sockopt *sopt;
	struct ip_moptions **imop;
{
	int error = 0;
	int i;
	struct in_addr addr;
	struct ip_mreq mreq;
	struct ifnet *ifp;
	struct ip_moptions *imo = *imop;
	struct route ro;
	struct sockaddr_in *dst;
	int ifindex;
	int s;

	if (imo == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		imo = (struct ip_moptions*)malloc(sizeof(*imo), M_IPMOPTS,
		    M_WAITOK);

		if (imo == NULL)
			return (ENOBUFS);
		*imop = imo;
		imo->imo_multicast_ifp = NULL;
		imo->imo_multicast_addr.s_addr = INADDR_ANY;
		imo->imo_multicast_vif = -1;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_num_memberships = 0;
	}

	switch (sopt->sopt_name) {
	/* store an index number for the vif you wanna use in the send */
	case IP_MULTICAST_VIF:
		if (legal_vif_num == 0) {
			error = EOPNOTSUPP;
			break;
		}
		error = sooptcopyin(sopt, &i, sizeof i, sizeof i);
		if (error)
			break;
		if (!legal_vif_num(i) && (i != -1)) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_vif = i;
		break;

	case IP_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		error = sooptcopyin(sopt, &addr, sizeof addr, sizeof addr);
		if (error)
			break;
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
		s = splimp();
		ifp = ip_multicast_if(&addr, &ifindex);
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			splx(s);
			error = EADDRNOTAVAIL;
			break;
		}
		imo->imo_multicast_ifp = ifp;
		if (ifindex)
			imo->imo_multicast_addr = addr;
		else
			imo->imo_multicast_addr.s_addr = INADDR_ANY;
		splx(s);
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 * The original multicast API required a char argument,
		 * which is inconsistent with the rest of the socket API.
		 * We allow either a char or an int.
		 */
		if (sopt->sopt_valsize == 1) {
			u_char ttl;
			error = sooptcopyin(sopt, &ttl, 1, 1);
			if (error)
				break;
			imo->imo_multicast_ttl = ttl;
		} else {
			u_int ttl;
			error = sooptcopyin(sopt, &ttl, sizeof ttl, 
					    sizeof ttl);
			if (error)
				break;
			if (ttl > 255)
				error = EINVAL;
			else
				imo->imo_multicast_ttl = ttl;
		}
		break;

	case IP_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.  The original multicast API required a
		 * char argument, which is inconsistent with the rest
		 * of the socket API.  We allow either a char or an int.
		 */
		if (sopt->sopt_valsize == 1) {
			u_char loop;
			error = sooptcopyin(sopt, &loop, 1, 1);
			if (error)
				break;
			imo->imo_multicast_loop = !!loop;
		} else {
			u_int loop;
			error = sooptcopyin(sopt, &loop, sizeof loop,
					    sizeof loop);
			if (error)
				break;
			imo->imo_multicast_loop = !!loop;
		}
		break;

	case IP_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		error = sooptcopyin(sopt, &mreq, sizeof mreq, sizeof mreq);
		if (error)
			break;

		if (!IN_MULTICAST(ntohl(mreq.imr_multiaddr.s_addr))) {
			error = EINVAL;
			break;
		}
		s = splimp();
		/*
		 * If no interface address was provided, use the interface of
		 * the route to the given multicast address.
		 */
		if (mreq.imr_interface.s_addr == INADDR_ANY) {
			bzero((caddr_t)&ro, sizeof(ro));
			dst = (struct sockaddr_in *)&ro.ro_dst;
			dst->sin_len = sizeof(*dst);
			dst->sin_family = AF_INET;
			dst->sin_addr = mreq.imr_multiaddr;
			rtalloc(&ro);
			if (ro.ro_rt == NULL) {
				error = EADDRNOTAVAIL;
				splx(s);
				break;
			}
			ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		}
		else {
			ifp = ip_multicast_if(&mreq.imr_interface, NULL);
		}

		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast.
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			splx(s);
			break;
		}
		/*
		 * See if the membership already exists or if all the
		 * membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    imo->imo_membership[i]->inm_addr.s_addr
						== mreq.imr_multiaddr.s_addr)
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = EADDRINUSE;
			splx(s);
			break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			splx(s);
			break;
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq.imr_multiaddr, ifp)) == NULL) {
			error = ENOBUFS;
			splx(s);
			break;
		}
		++imo->imo_num_memberships;
		splx(s);
		break;

	case IP_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		error = sooptcopyin(sopt, &mreq, sizeof mreq, sizeof mreq);
		if (error)
			break;

		if (!IN_MULTICAST(ntohl(mreq.imr_multiaddr.s_addr))) {
			error = EINVAL;
			break;
		}

		s = splimp();
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (mreq.imr_interface.s_addr == INADDR_ANY)
			ifp = NULL;
		else {
			ifp = ip_multicast_if(&mreq.imr_interface, NULL);
			if (ifp == NULL) {
				error = EADDRNOTAVAIL;
				splx(s);
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
			     mreq.imr_multiaddr.s_addr)
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			splx(s);
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
		splx(s);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp == NULL &&
	    imo->imo_multicast_vif == -1 &&
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
static int
ip_getmoptions(sopt, imo)
	struct sockopt *sopt;
	register struct ip_moptions *imo;
{
	struct in_addr addr;
	struct in_ifaddr *ia;
	int error, optval;
	u_char coptval;

	error = 0;
	switch (sopt->sopt_name) {
	case IP_MULTICAST_VIF: 
		if (imo != NULL)
			optval = imo->imo_multicast_vif;
		else
			optval = -1;
		error = sooptcopyout(sopt, &optval, sizeof optval);
		break;

	case IP_MULTICAST_IF:
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			addr.s_addr = INADDR_ANY;
		else if (imo->imo_multicast_addr.s_addr) {
			/* return the value user has set */
			addr = imo->imo_multicast_addr;
		} else {
			IFP_TO_IA(imo->imo_multicast_ifp, ia);
			addr.s_addr = (ia == NULL) ? INADDR_ANY
				: IA_SIN(ia)->sin_addr.s_addr;
		}
		error = sooptcopyout(sopt, &addr, sizeof addr);
		break;

	case IP_MULTICAST_TTL:
		if (imo == 0)
			optval = coptval = IP_DEFAULT_MULTICAST_TTL;
		else
			optval = coptval = imo->imo_multicast_ttl;
		if (sopt->sopt_valsize == 1)
			error = sooptcopyout(sopt, &coptval, 1);
		else
			error = sooptcopyout(sopt, &optval, sizeof optval);
		break;

	case IP_MULTICAST_LOOP:
		if (imo == 0)
			optval = coptval = IP_DEFAULT_MULTICAST_LOOP;
		else
			optval = coptval = imo->imo_multicast_loop;
		if (sopt->sopt_valsize == 1)
			error = sooptcopyout(sopt, &coptval, 1);
		else
			error = sooptcopyout(sopt, &optval, sizeof optval);
		break;

	default:
		error = ENOPROTOOPT;
		break;
	}
	return (error);
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
 * pointer that might NOT be a loopback interface -- evil, but easier than
 * replicating that code here.
 */
static void
ip_mloopback(ifp, m, dst, hlen)
	struct ifnet *ifp;
	register struct mbuf *m;
	register struct sockaddr_in *dst;
	int hlen;
{
	register struct ip *ip;
	struct mbuf *copym;

	copym = m_copy(m, 0, M_COPYALL);
	if (copym != NULL && (copym->m_flags & M_EXT || copym->m_len < hlen))
		copym = m_pullup(copym, hlen);
	if (copym != NULL) {
		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
		ip->ip_sum = 0;
		if (ip->ip_vhl == IP_VHL_BORING) {
			ip->ip_sum = in_cksum_hdr(ip);
		} else {
			ip->ip_sum = in_cksum(copym, hlen);
		}
		/*
		 * NB:
		 * It's not clear whether there are any lingering
		 * reentrancy problems in other areas which might
		 * be exposed by using ip_input directly (in
		 * particular, everything which modifies the packet
		 * in-place).  Yet another option is using the
		 * protosw directly to deliver the looped back
		 * packet.  For the moment, we'll err on the side
		 * of safety by using if_simloop().
		 */
#if 1 /* XXX */
		if (dst->sin_family != AF_INET) {
			printf("ip_mloopback: bad address family %d\n",
						dst->sin_family);
			dst->sin_family = AF_INET;
		}
#endif

#ifdef notdef
		copym->m_pkthdr.rcvif = ifp;
		ip_input(copym);
#else
		/* if the checksum hasn't been computed, mark it as valid */
		if (copym->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
			copym->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			copym->m_pkthdr.csum_data = 0xffff;
		}
		if_simloop(ifp, copym, dst->sin_family, 0);
#endif
	}
}

