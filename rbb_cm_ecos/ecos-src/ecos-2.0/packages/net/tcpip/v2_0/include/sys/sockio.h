//==========================================================================
//
//      include/sys/sockio.h
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


/*	$OpenBSD: sockio.h,v 1.10 1999/12/08 06:50:24 itojun Exp $	*/
/*	$NetBSD: sockio.h,v 1.5 1995/08/23 00:40:47 thorpej Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993, 1994
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
 *	@(#)sockio.h	8.1 (Berkeley) 3/28/94
 */

#ifndef	_SYS_SOCKIO_H_
#define	_SYS_SOCKIO_H_

#include <sys/ioccom.h>

/* Socket ioctl's. */
#define	SIOCSHIWAT	 _IOW('s',  0, int)		/* set high watermark */
#define	SIOCGHIWAT	 _IOR('s',  1, int)		/* get high watermark */
#define	SIOCSLOWAT	 _IOW('s',  2, int)		/* set low watermark */
#define	SIOCGLOWAT	 _IOR('s',  3, int)		/* get low watermark */
#define	SIOCATMARK	 _IOR('s',  7, int)		/* at oob mark? */
#define	SIOCSPGRP	 _IOW('s',  8, int)		/* set process group */
#define	SIOCGPGRP	 _IOR('s',  9, int)		/* get process group */

#ifdef __ECOS
#define	SIOCADDRT	 _IOW('r', 10, struct ecos_rtentry)	/* add route */
#define	SIOCDELRT	 _IOW('r', 11, struct ecos_rtentry)	/* delete route */
#else
#define	SIOCADDRT	 _IOW('r', 10, struct ortentry)	/* add route */
#define	SIOCDELRT	 _IOW('r', 11, struct ortentry)	/* delete route */
#endif

#define	SIOCSIFADDR	 _IOW('i', 12, struct ifreq)	/* set ifnet address */
#define	OSIOCGIFADDR	_IOWR('i', 13, struct ifreq)	/* get ifnet address */
#define	SIOCGIFADDR	_IOWR('i', 33, struct ifreq)	/* get ifnet address */
#define	SIOCGIFHWADDR	_IOWR('i', 15, struct ifreq)	/* get MAC address */
#ifdef __ECOS
#define	SIOCSIFHWADDR	 _IOW('i',101, struct ifreq)	/* set MAC address */
/* NB these two take a struct ifreq followed by the useful data */
#define SIOCGIFSTATSUD  _IOWR('i',102, struct ifreq)    /* get uptodate if stats */
#define SIOCGIFSTATS    _IOWR('i',103, struct ifreq)    /* get interface stats */
#endif
#define	SIOCSIFDSTADDR	 _IOW('i', 14, struct ifreq)	/* set p-p address */
#define	OSIOCGIFDSTADDR	_IOWR('i', 15, struct ifreq)	/* get p-p address */
#define	SIOCGIFDSTADDR	_IOWR('i', 34, struct ifreq)	/* get p-p address */
#define	SIOCSIFFLAGS	 _IOW('i', 16, struct ifreq)	/* set ifnet flags */
#define	SIOCGIFFLAGS	_IOWR('i', 17, struct ifreq)	/* get ifnet flags */
#define	OSIOCGIFBRDADDR	_IOWR('i', 18, struct ifreq)	/* get broadcast addr */
#define	SIOCGIFBRDADDR	_IOWR('i', 35, struct ifreq)	/* get broadcast addr */
#define	SIOCSIFBRDADDR	 _IOW('i', 19, struct ifreq)	/* set broadcast addr */
#define	OSIOCGIFCONF	_IOWR('i', 20, struct ifconf)	/* get ifnet list */
#define	SIOCGIFCONF	_IOWR('i', 36, struct ifconf)	/* get ifnet list */
#define	OSIOCGIFNETMASK	_IOWR('i', 21, struct ifreq)	/* get net addr mask */
#define	SIOCGIFNETMASK	_IOWR('i', 37, struct ifreq)	/* get net addr mask */
#define	SIOCSIFNETMASK	 _IOW('i', 22, struct ifreq)	/* set net addr mask */
#define	SIOCGIFMETRIC	_IOWR('i', 23, struct ifreq)	/* get IF metric */
#define	SIOCSIFMETRIC	 _IOW('i', 24, struct ifreq)	/* set IF metric */
#define	SIOCDIFADDR	 _IOW('i', 25, struct ifreq)	/* delete IF addr */
#define	SIOCAIFADDR	 _IOW('i', 26, struct ifaliasreq)/* add/chg IF alias */
#define	SIOCGIFDATA	_IOWR('i', 27, struct ifreq)	/* get if_data */

/* KAME IPv6 */
/* SIOCAIFALIAS? */
#define SIOCALIFADDR	 _IOW('i', 28, struct if_laddrreq) /* add IF addr */
#define SIOCGLIFADDR	_IOWR('i', 29, struct if_laddrreq) /* get IF addr */
#define SIOCDLIFADDR	 _IOW('i', 30, struct if_laddrreq) /* delete IF addr */

#define	SIOCADDMULTI	 _IOW('i', 49, struct ifreq)	/* add m'cast addr */
#define	SIOCDELMULTI	 _IOW('i', 50, struct ifreq)	/* del m'cast addr */
#define	SIOCGETVIFCNT	_IOWR('u', 51, struct sioc_vif_req)/* vif pkt cnt */
#define	SIOCGETSGCNT	_IOWR('u', 52, struct sioc_sg_req) /* sg pkt cnt */

#define	SIOCSIFMEDIA	_IOWR('i', 53, struct ifreq)	/* set net media */
#define	SIOCGIFMEDIA	_IOWR('i', 54, struct ifmediareq) /* get net media */

#define SIOCSIFPHYADDR   _IOW('i', 70, struct ifaliasreq) /* set gif addres */
#define	SIOCGIFPSRCADDR	_IOWR('i', 71, struct ifreq)	/* get gif psrc addr */
#define	SIOCGIFPDSTADDR	_IOWR('i', 72, struct ifreq)	/* get gif pdst addr */

#define	SIOCBRDGADD	_IOWR('i', 60, struct ifbreq)	/* add bridge ifs */
#define	SIOCBRDGDEL	_IOWR('i', 61, struct ifbreq)	/* del bridge ifs */
#define	SIOCBRDGGIFFLGS	_IOWR('i', 62, struct ifbreq)	/* get brdg if flags */
#define	SIOCBRDGSIFFLGS	_IOWR('i', 63, struct ifbreq)	/* set brdg if flags */
#define	SIOCBRDGSCACHE	_IOWR('i', 64, struct ifbcachereq) /* set cache size */
#define	SIOCBRDGGCACHE	_IOWR('i', 65, struct ifbcachereq) /* get cache size */
#define	SIOCBRDGIFS	_IOWR('i', 66, struct ifbreq)	/* get member ifs */
#define	SIOCBRDGRTS	_IOWR('i', 67, struct ifbaconf)	/* get addresses */
#define	SIOCBRDGSADDR	_IOWR('i', 68, struct ifbareq)	/* set addr flags */
#define	SIOCBRDGSTO	_IOWR('i', 69, struct ifbcachetoreq) /* cache timeout */
#define	SIOCBRDGGTO	_IOWR('i', 70, struct ifbcachetoreq) /* cache timeout */
#define	SIOCBRDGDADDR	_IOWR('i', 71, struct ifbareq)	/* delete addr */
#define	SIOCBRDGFLUSH	_IOWR('i', 72, struct ifbreq)	/* flush addr cache */

#define SIOCBRDGARL     _IOWR('i', 77, struct ifbrlreq) /* add bridge rule */
#define SIOCBRDGFRL     _IOWR('i', 78, struct ifbrlreq) /* flush brdg rules */
#define SIOCBRDGGRL     _IOWR('i', 79, struct ifbrlconf)/* get bridge rules */

#define	SIOCSIFMTU	 _IOW('i', 127, struct ifreq)	/* set ifnet mtu */
#define	SIOCGIFMTU	_IOWR('i', 126, struct ifreq)	/* get ifnet mtu */
#define	SIOCSIFASYNCMAP  _IOW('i', 125, struct ifreq)	/* set ppp asyncmap */
#define	SIOCGIFASYNCMAP _IOWR('i', 124, struct ifreq)	/* get ppp asyncmap */

#ifdef __ECOS
#define FIONBIO          _IOW('s', 80, int)             /* set non-blocking I/O */
#define FIOASYNC         _IOW('s', 81, int)             /* set async I/O */
#define FIONREAD         _IOR('s', 82, int)             /* get number of avail chars */
#endif

#endif /* !_SYS_SOCKIO_H_ */
