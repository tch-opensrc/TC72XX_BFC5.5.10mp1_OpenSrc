//==========================================================================
//
//      include/sys/param.h
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


/*	$OpenBSD: param.h,v 1.25 1999/09/23 08:25:01 deraadt Exp $	*/
/*	$NetBSD: param.h,v 1.23 1996/03/17 01:02:29 thorpej Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)param.h	8.2 (Berkeley) 1/21/94
 */

#ifndef _SYS_PARAM_H_
#define _SYS_PARAM_H_

#define	BSD	199306		/* System version (year & month). */
#define BSD4_3	1
#define BSD4_4	1

#define OpenBSD	199912		/* OpenBSD version (year & month). */
#define OpenBSD2_6 1		/* OpenBSD 2.6 */

#ifndef NULL
#ifdef 	__GNUG__
#define	NULL	__null
#else
#define	NULL	0
#endif
#endif

#ifndef _LOCORE
#include <sys/types.h>
#ifndef __ECOS
#include <sys/simplelock.h>
#endif
#endif

#ifndef __ECOS
/*
 * Machine-independent constants (some used in following include files).
 * Redefined constants are from POSIX 1003.1 limits file.
 *
 * MAXCOMLEN should be >= sizeof(ac_comm) (see <acct.h>)
 * MAXLOGNAME should be >= UT_NAMESIZE (see <utmp.h>)
 */
#include <sys/syslimits.h>

#define	MAXCOMLEN	16		/* max command name remembered */
#define	MAXINTERP	64		/* max interpreter file name length */
#define	MAXLOGNAME	12		/* max login name length */
#define	MAXUPRC		CHILD_MAX	/* max simultaneous processes */
#define	NCARGS		ARG_MAX		/* max bytes for an exec function */
#define	NGROUPS		NGROUPS_MAX	/* max number groups */
#define	NOFILE		OPEN_MAX	/* max open files per process (soft) */
#define	NOFILE_MAX	1024		/* max open files per process (hard) */
#define	NOGROUP		65535		/* marker for empty group set member */
#endif
#define MAXHOSTNAMELEN	256		/* max hostname size */

/* More types and definitions used throughout the kernel. */
#ifdef _KERNEL
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/time.h>
#ifdef __ECOS
#include <cyg/io/file.h>
#else
#include <sys/resource.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#endif
#endif

#ifndef __ECOS
/* Signals. */
#include <sys/signal.h>
#endif

#ifndef __ECOS
/*
 * Priorities.  Note that with 32 run queues, differences less than 4 are
 * insignificant.
 */
#define	PSWP	0
#define	PVM	4
#define	PINOD	8
#define	PRIBIO	16
#define	PVFS	20
#define	PZERO	22		/* No longer magic, shouldn't be here.  XXX */
#define	PSOCK	24
#define	PWAIT	32
#define	PLOCK	36
#define	PPAUSE	40
#define	PUSER	50
#define	MAXPRI	127		/* Priorities range from 0 through MAXPRI. */

#define	PRIMASK	0x0ff
#define	PCATCH	0x100		/* OR'd with pri for tsleep to check signals */
#endif

#define	NBPW	sizeof(int)	/* number of bytes per word (integer) */

#ifndef __ECOS
#define	CMASK	022		/* default file mask: S_IWGRP|S_IWOTH */
#define	NODEV	(dev_t)(-1)	/* non-existent device */

/*
 * Clustering of hardware pages on machines with ridiculously small
 * page sizes is done here.  The paging subsystem deals with units of
 * CLSIZE pte's describing NBPG (from machine/machparam.h) pages each.
 */
#define	CLBYTES		(CLSIZE*NBPG)
#define	CLOFSET		(CLSIZE*NBPG-1)	/* for clusters, like PGOFSET */
#define	claligned(x)	((((int)(x))&CLOFSET)==0)
#define	CLOFF		CLOFSET
#define	CLSHIFT		(PGSHIFT+CLSIZELOG2)

#if CLSIZE==1
#define	clbase(i)	(i)
#define	clrnd(i)	(i)
#else
/* Give the base virtual address (first of CLSIZE). */
#define	clbase(i)	((i) &~ (CLSIZE-1))
/* Round a number of clicks up to a whole cluster. */
#define	clrnd(i)	(((i) + (CLSIZE-1)) &~ (CLSIZE-1))
#endif

#define	CBLOCK	64		/* Clist block size, must be a power of 2. */
#define CBQSIZE	(CBLOCK/NBBY)	/* Quote bytes/cblock - can do better. */
				/* Data chars/clist. */
#define	CBSIZE	(CBLOCK - sizeof(struct cblock *) - CBQSIZE)
#define	CROUND	(CBLOCK - 1)	/* Clist rounding. */

/*
 * File system parameters and macros.
 *
 * The file system is made out of blocks of at most MAXBSIZE units, with
 * smaller units (fragments) only in the last direct block.  MAXBSIZE
 * primarily determines the size of buffers in the buffer pool.  It may be
 * made larger without any effect on existing file systems; however making
 * it smaller makes some file systems unmountable.
 */
#ifndef MAXBSIZE	/* XXX temp until sun3 DMA chaining */
#define	MAXBSIZE	MAXPHYS
#endif
#define MAXFRAG 	8

/*
 * MAXPATHLEN defines the longest permissable path length after expanding
 * symbolic links. It is used to allocate a temporary buffer from the buffer
 * pool in which to do the name expansion, hence should be a power of two,
 * and must be less than or equal to MAXBSIZE.  MAXSYMLINKS defines the
 * maximum number of symbolic links that may be expanded in a path name.
 * It should be set high enough to allow all legitimate uses, but halt
 * infinite loops reasonably quickly.
 */
#define	MAXPATHLEN	PATH_MAX
#define MAXSYMLINKS	32

#endif // __ECOS

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define powerof2(x)	((((x)-1)&(x))==0)

/* Macros for min/max. */
#ifndef _KERNEL
# ifndef MIN
#  define	MIN(a,b) (((a)<(b))?(a):(b))
# endif
# ifndef MAX
#  define	MAX(a,b) (((a)>(b))?(a):(b))
# endif
#endif

#ifndef __ECOS
/*
 * Constants for setting the parameters of the kernel memory allocator.
 *
 * 2 ** MINBUCKET is the smallest unit of memory that will be
 * allocated. It must be at least large enough to hold a pointer.
 *
 * Units of memory less or equal to MAXALLOCSAVE will permanently
 * allocate physical memory; requests for these size pieces of
 * memory are quite fast. Allocations greater than MAXALLOCSAVE must
 * always allocate and free physical memory; requests for these
 * size allocations should be done infrequently as they will be slow.
 *
 * Constraints: CLBYTES <= MAXALLOCSAVE <= 2 ** (MINBUCKET + 14), and
 * MAXALLOCSIZE must be a power of two.
 */
#define MINBUCKET	4		/* 4 => min allocation of 16 bytes */
#define MAXALLOCSAVE	(2 * CLBYTES)

/*
 * Scale factor for scaled integers used to count %cpu time and load avgs.
 *
 * The number of CPU `tick's that map to a unique `%age' can be expressed
 * by the formula (1 / (2 ^ (FSHIFT - 11))).  The maximum load average that
 * can be calculated (assuming 32 bits) can be closely approximated using
 * the formula (2 ^ (2 * (16 - FSHIFT))) for (FSHIFT < 15).
 *
 * For the scheduler to maintain a 1:1 mapping of CPU `tick' to `%age',
 * FSHIFT must be at least 11; this gives us a maximum load avg of ~1024.
 */
#define	FSHIFT	11		/* bits to right of fixed binary point */
#define FSCALE	(1<<FSHIFT)

/*
 * rfork() options.
 *
 * XXX currently, operations without RFPROC set are not supported.
 */
#define RFNAMEG		(1<<0)	/* UNIMPL new plan9 `name space' */
#define RFENVG		(1<<1)	/* UNIMPL copy plan9 `env space' */
#define RFFDG		(1<<2)	/* copy fd table */
#define RFNOTEG		(1<<3)	/* UNIMPL create new plan9 `note group' */
#define RFPROC		(1<<4)	/* change child (else changes curproc) */
#define RFMEM		(1<<5)	/* share `address space' */
#define RFNOWAIT	(1<<6)	/* parent need not wait() on child */ 
#define RFCNAMEG	(1<<10) /* UNIMPL zero plan9 `name space' */
#define RFCENVG		(1<<11) /* UNIMPL zero plan9 `env space' */
#define RFCFDG		(1<<12)	/* zero fd table */
#endif // __ECOS

#ifdef __ECOS
// Function mappings

extern int  cyg_tsleep(void *, int, char *, int);
extern void cyg_wakeup(void *);
#define tsleep(e,p,w,t) cyg_tsleep(e,0,w,t)
#define wakeup(e)       cyg_wakeup(e)

#ifdef CYGIMPL_TRACE_SPLX   
extern cyg_uint32  cyg_splimp(const char *file, const int line);
extern cyg_uint32  cyg_splnet(const char *file, const int line);
extern cyg_uint32  cyg_splclock(const char *file, const int line);
extern cyg_uint32  cyg_splsoftnet(const char *file, const int line);
extern void        cyg_splx(cyg_uint32, const char *file, const int line);
#define splimp()   cyg_splimp(__FUNCTION__, __LINE__)
#define splnet()   cyg_splnet(__FUNCTION__, __LINE__)
#define splclock() cyg_splclock(__FUNCTION__, __LINE__)
#define splsoftnet() cyg_splsoftnet(__FUNCTION__, __LINE__)
#define splx(x)    cyg_splx(x, __FUNCTION__, __LINE__)
#define cyg_scheduler_lock() _cyg_scheduler_lock(__FUNCTION__, __LINE__)
#define cyg_scheduler_safe_lock() _cyg_scheduler_safe_lock(__FUNCTION__, __LINE__)
#define cyg_scheduler_unlock() _cyg_scheduler_unlock(__FUNCTION__, __LINE__)
#else
extern cyg_uint32  cyg_splimp(void);
extern cyg_uint32  cyg_splnet(void);
extern cyg_uint32  cyg_splclock(void);
extern cyg_uint32  cyg_splsoftnet(void);
extern cyg_uint32  cyg_splhigh(void);
extern void        cyg_splx(cyg_uint32);
#define splimp     cyg_splimp
#define splnet     cyg_splnet
#define splclock   cyg_splclock
#define splsoftnet cyg_splsoftnet
#define splhigh    cyg_splhigh
#define splx       cyg_splx
#endif

extern void cyg_panic(const char *msg, ...);
#define panic cyg_panic

// Namespace changes
#define route_reinit cyg_route_reinit
#define arc4random cyg_arc4random
#endif

/* Machine type dependent parameters. */
#include <machine/param.h>
#include <machine/limits.h>

#endif // __SYS_PARAM_H_
