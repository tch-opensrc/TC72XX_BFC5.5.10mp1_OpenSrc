//==========================================================================
//
//      src/sys/net/if_ethersubr.c
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
 * Copyright (c) 1982, 1989, 1993
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
 *	@(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/net/if_ethersubr.c,v 1.70.2.17 2001/08/01 00:47:49 fenner Exp $
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ethernet.h>

#include <cyg/infra/diag.h>		// cliffmod - For diagnostic printing.

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/if_bcm.h> // cliffmod
#include <netinet/ip.h>     // cliffmod
#endif
#ifdef INET6
#include <netinet6/nd6.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
int (*ef_inputp)(struct ifnet*, struct ether_header *eh, struct mbuf *m);
int (*ef_outputp)(struct ifnet *ifp, struct mbuf **mp,
		struct sockaddr *dst, short *tp, int *hlen);
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
ushort ns_nettype;
int ether_outputdebug = 0;
int ether_inputdebug = 0;
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

#define llc_snap_org_code llc_un.type_snap.org_code
#define llc_snap_ether_type llc_un.type_snap.ether_type

extern u_char	at_org_code[3];
extern u_char	aarp_org_code[3];
#endif /* NETATALK */

#ifdef BRIDGE
#include <net/bridge.h>
#endif

#define NVLAN 0
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

/* netgraph node hooks for ng_ether(4) */
void	(*ng_ether_input_p)(struct ifnet *ifp,
		struct mbuf **mp, struct ether_header *eh);
void	(*ng_ether_input_orphan_p)(struct ifnet *ifp,
		struct mbuf *m, struct ether_header *eh);
int	(*ng_ether_output_p)(struct ifnet *ifp, struct mbuf **mp);
void	(*ng_ether_attach_p)(struct ifnet *ifp);
void	(*ng_ether_detach_p)(struct ifnet *ifp);

static	int ether_resolvemulti __P((struct ifnet *, struct sockaddr **,
				    struct sockaddr *));
u_char	etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#define senderr(e) do { error = (e); goto bad;} while (0)
#define IFP2AC(IFP) ((struct arpcom *)IFP)

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
ether_output(ifp, m, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	short type;
	int error = 0, hdrcmplt = 0;
 	u_char esrc[6], edst[6];
	register struct rtentry *rt;
	register struct ether_header *eh;
	int off, edst_resolved = 0, loop_copy = 0;
	int hlen;	/* link layer header lenght */
	struct arpcom *ac = IFP2AC(ifp);

	if( (ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING) )
    {
		senderr(ENETDOWN);
    }

	rt = rt0;
	if (rt) 
    {
		if ((rt->rt_flags & RTF_UP) == 0) 
        {
			rt0 = rt = rtalloc1(dst, 1, 0UL);
			if (rt0)
            {
				rt->rt_refcnt--;
            }                
			else
            {
				senderr(EHOSTUNREACH);
            }
        } // end if ((rt->rt_flags & RTF_UP) == 0) 

		if (rt->rt_flags & RTF_GATEWAY) 
        {
			if (rt->rt_gwroute == 0)
            {
				goto lookup;
            }
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) 
            {
				rtfree(rt); rt = rt0;
lookup:         
                rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1, 0UL);

				if ((rt = rt->rt_gwroute) == 0)
                {
					senderr(EHOSTUNREACH);
                }
            } // end if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) 
        } // end if (rt->rt_flags & RTF_GATEWAY) 

		if (rt->rt_flags & RTF_REJECT)
        {
			if (rt->rt_rmx.rmx_expire == 0 || time_second < rt->rt_rmx.rmx_expire)
            {
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
            }
        } // end if (rt->rt_flags & RTF_REJECT)
    } // end if (rt)

	hlen = ETHER_HDR_LEN;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
        // begin cliffmod
        {
            struct sockaddr *pdest_ip_sockaddr = dst;   // old pDestAddr
            struct sockaddr_in dest_ip_sockaddr_in;     // old arpAddr
            
            bzero( (caddr_t)&dest_ip_sockaddr_in, sizeof(dest_ip_sockaddr_in) );
            
            if( rt == NULL )
            {
                // explicit route NOT specified.
                // --> use default route.
                edst_resolved = BcmComputeDefaultGatewayIpAndMacAddr( ifp, m,
                    &dest_ip_sockaddr_in, (const char*)edst );
            
                if( !edst_resolved )
                {
                    // default gateway mac address not resolved yet...
                    if( dest_ip_sockaddr_in.sin_addr.s_addr )
                    {
                        // non-zero dest_ip_sockaddr_in.sin_addr.s_addr.
                        // --> arp for it below...
                        pdest_ip_sockaddr = (struct sockaddr *)&dest_ip_sockaddr_in;
                    }
                    else
                    {
                        // else gateway IP address is unknown.
                        printf("ether_output:  No arp target selected; host not reachable!\n");                
                        senderr(EHOSTUNREACH);
                    }
                }
            }

            if( !edst_resolved )
            {
                // dest mac address not resolved...
                // arp for it.
                if (!arpresolve(ac, rt, m, pdest_ip_sockaddr, edst, rt0))
                {           
                    return (0);	/* if not yet resolved */
                }
            }
            
		    off = m->m_pkthdr.len - m->m_len;
		    type = htons(ETHERTYPE_IP);
        }
        // end cliffmod
		break;
#endif // end #ifdef INET
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(&ac->ac_if, rt, m, dst, (u_char *)edst)) {
			/* Something bad happened */
			return(0);
		}
		off = m->m_pkthdr.len - m->m_len;
		type = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef IPX
	case AF_IPX:
		if (ef_outputp) {
		    error = ef_outputp(ifp, &m, dst, &type, &hlen);
		    if (error)
			goto bad;
		} else
		    type = htons(ETHERTYPE_IPX);
 		bcopy((caddr_t)&(((struct sockaddr_ipx *)dst)->sipx_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
	  {
	    struct at_ifaddr *aa;

	    if ((aa = at_ifawithnet((struct sockaddr_at *)dst)) == NULL) {
		    goto bad;
	    }
	    if (!aarpresolve(ac, m, (struct sockaddr_at *)dst, edst))
		    return (0);
	    /*
	     * In the phase 2 case, need to prepend an mbuf for the llc header.
	     * Since we must preserve the value of m, which is passed to us by
	     * value, we m_copy() the first mbuf, and use it for our llc header.
	     */
	    if ( aa->aa_flags & AFA_PHASE2 ) {
		struct llc llc;

		M_PREPEND(m, sizeof(struct llc), M_WAIT);
		llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
		llc.llc_control = LLC_UI;
		bcopy(at_org_code, llc.llc_snap_org_code, sizeof(at_org_code));
		llc.llc_snap_ether_type = htons( ETHERTYPE_AT );
		bcopy(&llc, mtod(m, caddr_t), sizeof(struct llc));
		type = htons(m->m_pkthdr.len);
		hlen = sizeof(struct llc) + ETHER_HDR_LEN;
	    } else {
		type = htons(ETHERTYPE_AT);
	    }
	    break;
	  }
#endif /* NETATALK */
#ifdef NS
	case AF_NS:
		switch(ns_nettype){
		default:
		case 0x8137: /* Novell Ethernet_II Ethernet TYPE II */
			type = 0x8137;
			break;
		case 0x0: /* Novell 802.3 */
			type = htons( m->m_pkthdr.len);
			break;
		case 0xe0e0: /* Novell 802.2 and Token-Ring */
			M_PREPEND(m, 3, M_WAIT);
			type = htons( m->m_pkthdr.len);
			cp = mtod(m, u_char *);
			*cp++ = 0xE0;
			*cp++ = 0xE0;
			*cp++ = 0x03;
			break;
		}
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		/*
		 * XXX if ns_thishost is the same as the node's ethernet
		 * address then just the default code will catch this anyhow.
		 * So I'm not sure if this next clause should be here at all?
		 * [JRE]
		 */
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_thishost, sizeof(edst))){
			m->m_pkthdr.rcvif = ifp;
			schednetisr(NETISR_NS);
			inq = &nsintrq;
			s = splimp();
			if (IF_QFULL(inq)) {
				IF_DROP(inq);
				m_freem(m);
			} else
				IF_ENQUEUE(inq, m);
			splx(s);
			return (error);
		}
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_broadhost, sizeof(edst))){
			m->m_flags |= M_BCAST;
		}
		break;
#endif /* NS */

	case pseudo_AF_HDRCMPLT:
		hdrcmplt = 1;
		eh = (struct ether_header *)dst->sa_data;
		(void)memcpy(esrc, eh->ether_shost, sizeof (esrc));
		/* FALLTHROUGH */

	case AF_UNSPEC:
		loop_copy = -1; /* if this is for us, don't do it */
		eh = (struct ether_header *)dst->sa_data;
 		(void)memcpy(edst, eh->ether_dhost, sizeof (edst));
		type = eh->ether_type;
		break;

	default:
		printf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct ether_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	eh = mtod(m, struct ether_header *);
	(void)memcpy(&eh->ether_type, &type,
		sizeof(eh->ether_type));
 	(void)memcpy(eh->ether_dhost, edst, sizeof (edst));
	if (hdrcmplt)
		(void)memcpy(eh->ether_shost, esrc,
			sizeof(eh->ether_shost));
	else
		(void)memcpy(eh->ether_shost, ac->ac_enaddr,
			sizeof(eh->ether_shost));

	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	if ((ifp->if_flags & IFF_SIMPLEX) && (loop_copy != -1)) 
    {
		if ((m->m_flags & M_BCAST) || (loop_copy > 0)) 
        {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

            if ( n == 0 ) 
            {
                error = 0;
                goto bad;
            }

			(void) if_simloop(ifp, n, dst->sa_family, hlen);
		} 
        else if (bcmp(eh->ether_dhost, eh->ether_shost, ETHER_ADDR_LEN) == 0) 
        {
			(void) if_simloop(ifp, m, dst->sa_family, hlen);
			return (0);	/* XXX */
		}
	}

	/* Handle ng_ether(4) processing, if any */
	if (ng_ether_output_p != NULL) 
    {
		if ((error = (*ng_ether_output_p)(ifp, &m)) != 0) 
        {
bad:		
            if (m != NULL)
            {
				m_freem(m);
            }
			return (error);
		}
		if (m == NULL)
			return (0);
	}

	/* Continue with link-layer output */
	return ether_output_frame(ifp, m);
}

/*
 * Ethernet link layer output routine to send a raw frame to the device.
 *
 * This assumes that the 14 byte Ethernet header is present and contiguous
 * in the first mbuf (if BRIDGE'ing).
 */
int
ether_output_frame(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	int s, len, error = 0;
	short mflags;
#ifdef ALTQ
	struct altq_pktattr pktattr;
#endif

#ifdef BRIDGE
	if (do_bridge && BDG_USED(ifp) ) {
		struct ether_header *eh; /* a ptr suffices */

		m->m_pkthdr.rcvif = NULL;
		eh = mtod(m, struct ether_header *);
		m_adj(m, ETHER_HDR_LEN);
		m = bdg_forward(m, eh, ifp);
		if (m != NULL)
			m_freem(m);
		return (0);
	}
#endif
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_etherclassify(&ifp->if_snd, m, &pktattr);
#endif
	mflags = m->m_flags;
	len = m->m_pkthdr.len;
	s = splimp();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
#ifdef ALTQ
	IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
#else
	IFQ_ENQUEUE(&ifp->if_snd, m, error);
#endif
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	ifp->if_obytes += len;
	if (mflags & M_MCAST)
		ifp->if_omcasts++;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	return (error);
}

/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 *
 * First we perform any link layer operations, then continue
 * to the upper layers with ether_demux().
 */
void
ether_input(ifp, eh, m)
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
#ifdef BRIDGE
	struct ether_header save_eh;
#endif

#ifdef BPF
	/* Check for a BPF tap */
	if (ifp->if_bpf != NULL) {
		struct m_hdr mh;

		/* This kludge is OK; BPF treats the "mbuf" as read-only */
		mh.mh_next = m;
		mh.mh_data = (char *)eh;
		mh.mh_len = ETHER_HDR_LEN;
		bpf_mtap(ifp, (struct mbuf *)&mh);
	}
#endif

	/* Handle ng_ether(4) processing, if any */
	if (ng_ether_input_p != NULL) {
		(*ng_ether_input_p)(ifp, &m, eh);
		if (m == NULL)
			return;
	}

#ifdef BRIDGE
	/* Check for bridging mode */
	if (do_bridge && BDG_USED(ifp) ) {
		struct ifnet *bif;

		/* Check with bridging code */
		if ((bif = bridge_in(ifp, eh)) == BDG_DROP) {
			m_freem(m);
			return;
		}
		if (bif != BDG_LOCAL) {
			struct mbuf *oldm = m ;

			save_eh = *eh ; /* because it might change */
			m = bdg_forward(m, eh, bif);	/* needs forwarding */
			/*
			 * Do not continue if bdg_forward() processed our
			 * packet (and cleared the mbuf pointer m) or if
			 * it dropped (m_free'd) the packet itself.
			 */
			if (m == NULL) {
			    if (bif == BDG_BCAST || bif == BDG_MCAST)
				printf("bdg_forward drop MULTICAST PKT\n");
			    return;
			}
			if (m != oldm) /* m changed! */
			    eh = &save_eh ;
		}
		if (bif == BDG_LOCAL
		    || bif == BDG_BCAST
		    || bif == BDG_MCAST)
			goto recvLocal;			/* receive locally */

		/* If not local and not multicast, just drop it */
		if (m != NULL)
			m_freem(m);
		return;
       }
#endif

#ifdef BRIDGE
recvLocal:
#endif
	/* Continue with upper layer processing */
	ether_demux(ifp, eh, m);
}

/*
 * Upper layer processing for a received Ethernet packet.
 */
void
ether_demux(ifp, eh, m)
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
	struct ifqueue *inq;
	u_short ether_type;
	int s;
#if defined(NETATALK)
	register struct llc *l;
#endif

#ifdef BRIDGE
    if (! (do_bridge && BDG_USED(ifp) ) )
#endif
	/* Discard packet if upper layers shouldn't see it because it was
	   unicast to a different Ethernet address. If the driver is working
	   properly, then this situation can only happen when the interface
	   is in promiscuous mode. */
	if ((ifp->if_flags & IFF_PROMISC) != 0
	    && (eh->ether_dhost[0] & 1) == 0
	    && bcmp(eh->ether_dhost,
	      IFP2AC(ifp)->ac_enaddr, ETHER_ADDR_LEN) != 0) {
		m_freem(m);
		return;
	}

	/* Discard packet if interface is not up */
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	if (eh->ether_dhost[0] & 1) {
		/*
		 * If this is not a simplex interface, drop the packet
		 * if it came from us.
		 */
		if ((ifp->if_flags & IFF_SIMPLEX) == 0) {
			struct ifaddr *ifa;
			struct sockaddr_dl *sdl = NULL;

			/* find link-layer address */
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
				if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr) &&
				    sdl->sdl_family == AF_LINK)
					break;

			if (sdl && bcmp(LLADDR(sdl), eh->ether_shost,
			    ETHER_ADDR_LEN) == 0) {
				m_freem(m);
				return;
			}
		}
		if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
			 sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*eh);

	ether_type = ntohs(eh->ether_type);

#if NVLAN > 0
	if (ether_type == vlan_proto) {
		if (vlan_input(eh, m) < 0)
			ifp->if_data.ifi_noproto++;
		return;
	}
#endif /* NVLAN > 0 */

	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		if (ipflow_fastforward(m))
			return;
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ETHERTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP) {
			/* Discard packet if ARP is disabled on interface */
			m_freem(m);
			return;
		}
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
		break;
#endif
#ifdef IPX
	case ETHERTYPE_IPX:
		if (ef_inputp && ef_inputp(ifp, eh, m) == 0)
			return;
		schednetisr(NETISR_IPX);
		inq = &ipxintrq;
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		schednetisr(NETISR_IPV6);
		inq = &ip6intrq;
		break;
#endif
#ifdef NS
	case 0x8137: /* Novell Ethernet_II Ethernet TYPE II */
		schednetisr(NETISR_NS);
		inq = &nsintrq;
		break;

#endif /* NS */
#ifdef NETATALK
        case ETHERTYPE_AT:
                schednetisr(NETISR_ATALK);
                inq = &atintrq1;
                break;
        case ETHERTYPE_AARP:
		/* probably this should be done with a NETISR as well */
                aarpinput(IFP2AC(ifp), m); /* XXX */
                return;
#endif /* NETATALK */
	default:
#ifdef IPX
		if (ef_inputp && ef_inputp(ifp, eh, m) == 0)
			return;
#endif /* IPX */
#ifdef NS
		checksum = mtod(m, ushort *);
		/* Novell 802.3 */
		if ((ether_type <= ETHERMTU) &&
			((*checksum == 0xffff) || (*checksum == 0xE0E0))){
			if(*checksum == 0xE0E0) {
				m->m_pkthdr.len -= 3;
				m->m_len -= 3;
				m->m_data += 3;
			}
				schednetisr(NETISR_NS);
				inq = &nsintrq;
				break;
		}
#endif /* NS */
#if defined(NETATALK)
		if (ether_type > ETHERMTU)
			goto dropanyway;
		l = mtod(m, struct llc *);
		switch (l->llc_dsap) {
		case LLC_SNAP_LSAP:
		    switch (l->llc_control) {
		    case LLC_UI:
			if (l->llc_ssap != LLC_SNAP_LSAP)
			    goto dropanyway;
	
			if (Bcmp(&(l->llc_snap_org_code)[0], at_org_code,
				   sizeof(at_org_code)) == 0 &&
			     ntohs(l->llc_snap_ether_type) == ETHERTYPE_AT) {
			    inq = &atintrq2;
			    m_adj( m, sizeof( struct llc ));
			    schednetisr(NETISR_ATALK);
			    break;
			}

			if (Bcmp(&(l->llc_snap_org_code)[0], aarp_org_code,
				   sizeof(aarp_org_code)) == 0 &&
			     ntohs(l->llc_snap_ether_type) == ETHERTYPE_AARP) {
			    m_adj( m, sizeof( struct llc ));
			    aarpinput(IFP2AC(ifp), m); /* XXX */
			    return;
			}
		
		    default:
			goto dropanyway;
		    }
		    break;
		dropanyway:
		default:
			if (ng_ether_input_orphan_p != NULL)
				(*ng_ether_input_orphan_p)(ifp, m, eh);
			else
				m_freem(m);
			return;
		}
#else /* NETATALK */
		if (ng_ether_input_orphan_p != NULL)
			(*ng_ether_input_orphan_p)(ifp, m, eh);
		else
			m_freem(m);
		return;
#endif /* NETATALK */
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(ifp, bpf)
	register struct ifnet *ifp;
	int bpf;
{
	register struct ifaddr *ifa;
	register struct sockaddr_dl *sdl;

	if_attach(ifp);
	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 14;
	ifp->if_mtu = ETHERMTU;
	ifp->if_resolvemulti = ether_resolvemulti;
	if (ifp->if_baudrate == 0)
	    ifp->if_baudrate = 10000000;
	ifa = ifnet_addrs[ifp->if_index - 1];
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy((IFP2AC(ifp))->ac_enaddr, LLADDR(sdl), ifp->if_addrlen);
#ifdef BPF
	if (bpf)
		bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	if (ng_ether_attach_p != NULL)
		(*ng_ether_attach_p)(ifp);
}

/*
 * Perform common duties while detaching an Ethernet interface
 */
void
ether_ifdetach(ifp, bpf)
	struct ifnet *ifp;
	int bpf;
{
	if (ng_ether_detach_p != NULL)
		(*ng_ether_detach_p)(ifp);
#ifdef BPF
	if (bpf)
		bpfdetach(ifp);
#endif
	if_detach(ifp);
}

int
ether_ioctl(ifp, command, data)
	struct ifnet *ifp;
	int command;
	caddr_t data;
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0;

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ifp->if_init(ifp->if_softc);	/* before arpwhohas */
			arp_ifinit(IFP2AC(ifp), ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_IPX:
			{
			register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);
			struct arpcom *ac = IFP2AC(ifp);

			if (ipx_nullhost(*ina))
				ina->x_host =
				    *(union ipx_host *)
			            ac->ac_enaddr;
			else {
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) ac->ac_enaddr,
				      sizeof(ac->ac_enaddr));
			}

			/*
			 * Set new address
			 */
			ifp->if_init(ifp->if_softc);
			break;
			}
#endif
#ifdef NS
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_NS:
		{
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
			struct arpcom *ac = IFP2AC(ifp);

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *) (ac->ac_enaddr);
			else {
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) ac->ac_enaddr,
				      sizeof(ac->ac_enaddr));
			}

			/*
			 * Set new address
			 */
			ifp->if_init(ifp->if_softc);
			break;
		}
#endif
		default:
			ifp->if_init(ifp->if_softc);
			break;
		}
		break;

	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) & ifr->ifr_data;
			bcopy(IFP2AC(ifp)->ac_enaddr,
			      (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
		}
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;
	}
	return (error);
}

int
ether_resolvemulti(ifp, llsa, sa)
	struct ifnet *ifp;
	struct sockaddr **llsa;
	struct sockaddr *sa;
{
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
	u_char *e_addr;

	switch(sa->sa_family) {
	case AF_LINK:
		/*
		 * No mapping needed. Just check that it's a valid MC address.
		 */
		sdl = (struct sockaddr_dl *)sa;
		e_addr = LLADDR(sdl);
		if ((e_addr[0] & 1) != 1)
			return EADDRNOTAVAIL;
		*llsa = 0;
		return 0;

#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK|M_ZERO);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IP_MULTICAST(&sin->sin_addr, e_addr);
		*llsa = (struct sockaddr *)sdl;
		return 0;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/*
			 * An IP6 address of 0 means listen to all
			 * of the Ethernet multicast address used for IP6.
			 * (This is used for multicast routers.)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			*llsa = 0;
			return 0;
		}
		if (!IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK|M_ZERO);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, e_addr);
		*llsa = (struct sockaddr *)sdl;
		return 0;
#endif

	default:
		/*
		 * Well, the text isn't quite right, but it's the name
		 * that counts...
		 */
		return EAFNOSUPPORT;
	}
}
