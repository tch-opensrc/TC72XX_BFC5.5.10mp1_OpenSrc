//==========================================================================
//
//      src/sys/kern/uipc_mbuf2.c
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

/*	$KAME: uipc_mbuf2.c,v 1.31 2001/11/28 11:08:53 itojun Exp $	*/
/*	$NetBSD: uipc_mbuf.c,v 1.40 1999/04/01 00:23:25 thorpej Exp $	*/

/*
 * Copyright (C) 1999 WIDE Project.
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
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
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
 *	@(#)uipc_mbuf.c	8.4 (Berkeley) 2/14/95
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#ifdef __bsdi__
#define M_SHAREDCLUSTER(m) \
	(((m)->m_flags & M_EXT) != 0 && \
	 ((m)->m_ext.ext_func || mclrefcnt[mtocl((m)->m_ext.ext_buf)] > 1))
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#define M_SHAREDCLUSTER(m) \
	(((m)->m_flags & M_EXT) != 0 && \
	 ((m)->m_ext.ext_free || MCLISREFERENCED((m))))
#else
#define M_SHAREDCLUSTER(m) \
	(((m)->m_flags & M_EXT) != 0 && \
	 ((m)->m_ext.ext_free || mclrefcnt[mtocl((m)->m_ext.ext_buf)] > 1))
#endif

#ifndef __NetBSD__
/* can't call it m_dup(), as freebsd[34] uses m_dup() with different arg */
static struct mbuf *m_dup1 __P((struct mbuf *, int, int, int));
#endif

/*
 * ensure that [off, off + len) is contiguous on the mbuf chain "m".
 * packet chain before "off" is kept untouched.
 * if offp == NULL, the target will start at <retval, 0> on resulting chain.
 * if offp != NULL, the target will start at <retval, *offp> on resulting chain.
 *
 * on error return (NULL return value), original "m" will be freed.
 *
 * XXX M_TRAILINGSPACE/M_LEADINGSPACE on shared cluster (sharedcluster)
 */
struct mbuf *
m_pulldown(m, off, len, offp)
	struct mbuf *m;
	int off, len;
	int *offp;
{
	struct mbuf *n, *o;
	int hlen, tlen, olen;
	int sharedcluster;
#ifdef PULLDOWN_STAT
	static struct mbuf *prev = NULL;
	int prevlen = 0, prevmlen = 0;
#endif

	/* check invalid arguments. */
	if (m == NULL)
		panic("m == NULL in m_pulldown()");
	if (len > MCLBYTES) {
		m_freem(m);
		return NULL;	/* impossible */
	}

#ifdef PULLDOWN_STAT
	mbstat.m_pulldowns++;
#endif

#ifdef PULLDOWN_STAT
	/* statistics for m_pullup */
	mbstat.m_pullups++;
	if (off + len > MHLEN)
		mbstat.m_pullup_fail++;
	else {
		int dlen, mlen;

		dlen = (prev == m) ? prevlen : m->m_len;
		mlen = (prev == m) ? prevmlen : m->m_len + M_TRAILINGSPACE(m);

		if (dlen >= off + len)
			mbstat.m_pullups--; /* call will not be made! */
		else if ((m->m_flags & M_EXT) != 0) {
			mbstat.m_pullup_alloc++;
			mbstat.m_pullup_copy++;
		} else {
			if (mlen >= off + len)
				mbstat.m_pullup_copy++;
			else {
				mbstat.m_pullup_alloc++;
				mbstat.m_pullup_copy++;
			}
		}

		prevlen = off + len;
		prevmlen = MHLEN;
	}

	/* statistics for m_pullup2 */
	mbstat.m_pullup2++;
	if (off + len > MCLBYTES)
		mbstat.m_pullup2_fail++;
	else {
		int dlen, mlen;

		dlen = (prev == m) ? prevlen : m->m_len;
		mlen = (prev == m) ? prevmlen : m->m_len + M_TRAILINGSPACE(m);
		prevlen = off + len;
		prevmlen = mlen;

		if (dlen >= off + len)
			mbstat.m_pullup2--; /* call will not be made! */
		else if ((m->m_flags & M_EXT) != 0) {
			mbstat.m_pullup2_alloc++;
			mbstat.m_pullup2_copy++;
			prevmlen = (off + len > MHLEN) ? MCLBYTES : MHLEN;
		} else {
			if (mlen >= off + len)
				mbstat.m_pullup2_copy++;
			else {
				mbstat.m_pullup2_alloc++;
				mbstat.m_pullup2_copy++;
				prevmlen = (off + len > MHLEN) ? MCLBYTES
							       : MHLEN;
			}
		}
	}

	prev = m;
#endif

#ifdef PULLDOWN_DEBUG
    {
	struct mbuf *t;
	printf("before:");
	for (t = m; t; t = t->m_next)
		printf(" %d", t->m_len);
	printf("\n");
    }
#endif
	n = m;
	while (n != NULL && off > 0) {
		if (n->m_len > off)
			break;
		off -= n->m_len;
		n = n->m_next;
	}
	/* be sure to point non-empty mbuf */
	while (n != NULL && n->m_len == 0)
		n = n->m_next;
	if (!n) {
		m_freem(m);
		return NULL;	/* mbuf chain too short */
	}

	sharedcluster = M_SHAREDCLUSTER(n);

	/*
	 * the target data is on <n, off>.
	 * if we got enough data on the mbuf "n", we're done.
	 */
	if ((off == 0 || offp) && len <= n->m_len - off && !sharedcluster)
		goto ok;

#ifdef PULLDOWN_STAT
	mbstat.m_pulldown_copy++;
#endif

	/*
	 * when len <= n->m_len - off and off != 0, it is a special case.
	 * len bytes from <n, off> sits in single mbuf, but the caller does
	 * not like the starting position (off).
	 * chop the current mbuf into two pieces, set off to 0.
	 */
	if (len <= n->m_len - off) {
#ifdef __NetBSD__
		o = m_dup(n, off, n->m_len - off, M_DONTWAIT);
#else
		o = m_dup1(n, off, n->m_len - off, M_DONTWAIT);
#endif
		if (o == NULL) {
			m_freem(m);
			return NULL;	/* ENOBUFS */
		}
		n->m_len = off;
		o->m_next = n->m_next;
		n->m_next = o;
		n = n->m_next;
		off = 0;
		goto ok;
	}

	/*
	 * we need to take hlen from <n, off> and tlen from <n->m_next, 0>,
	 * and construct contiguous mbuf with m_len == len.
	 * note that hlen + tlen == len, and tlen > 0.
	 */
	hlen = n->m_len - off;
	tlen = len - hlen;

	/*
	 * ensure that we have enough trailing data on mbuf chain.
	 * if not, we can do nothing about the chain.
	 */
	olen = 0;
	for (o = n->m_next; o != NULL; o = o->m_next)
		olen += o->m_len;
	if (hlen + olen < len) {
		m_freem(m);
		return NULL;	/* mbuf chain too short */
	}

	/*
	 * easy cases first.
	 * we need to use m_copydata() to get data from <n->m_next, 0>.
	 */
	if ((off == 0 || offp) && M_TRAILINGSPACE(n) >= tlen &&
	    !sharedcluster) {
		m_copydata(n->m_next, 0, tlen, mtod(n, caddr_t) + n->m_len);
		n->m_len += tlen;
		m_adj(n->m_next, tlen);
		goto ok;
	}
	if ((off == 0 || offp) && M_LEADINGSPACE(n->m_next) >= hlen &&
	    !sharedcluster) {
		n->m_next->m_data -= hlen;
		n->m_next->m_len += hlen;
		bcopy(mtod(n, caddr_t) + off, mtod(n->m_next, caddr_t), hlen);
		n->m_len -= hlen;
		n = n->m_next;
		off = 0;
		goto ok;
	}

	/*
	 * now, we need to do the hard way.  don't m_copy as there's no room
	 * on both end.
	 */
#ifdef PULLDOWN_STAT
	mbstat.m_pulldown_alloc++;
#endif
	MGET(o, M_DONTWAIT, m->m_type);
	if (o && len > MLEN) {
		MCLGET(o, M_DONTWAIT);
		if ((o->m_flags & M_EXT) == 0) {
			m_free(o);
			o = NULL;
		}
	}
	if (!o) {
		m_freem(m);
		return NULL;	/* ENOBUFS */
	}
	/* get hlen from <n, off> into <o, 0> */
	o->m_len = hlen;
	bcopy(mtod(n, caddr_t) + off, mtod(o, caddr_t), hlen);
	n->m_len -= hlen;
	/* get tlen from <n->m_next, 0> into <o, hlen> */
	m_copydata(n->m_next, 0, tlen, mtod(o, caddr_t) + o->m_len);
	o->m_len += tlen;
	m_adj(n->m_next, tlen);
	o->m_next = n->m_next;
	n->m_next = o;
	n = o;
	off = 0;

ok:
#ifdef PULLDOWN_DEBUG
    {
	struct mbuf *t;
	printf("after:");
	for (t = m; t; t = t->m_next)
		printf("%c%d", t == n ? '*' : ' ', t->m_len);
	printf(" (off=%d)\n", off);
    }
#endif
	if (offp)
		*offp = off;
	return n;
}

#ifndef __NetBSD__
static struct mbuf *
m_dup1(m, off, len, wait)
	struct mbuf *m;
	int off;
	int len;
	int wait;
{
	struct mbuf *n;
	int l;
	int copyhdr;

	if (len > MCLBYTES)
		return NULL;
	if (off == 0 && (m->m_flags & M_PKTHDR) != 0) {
		copyhdr = 1;
		MGETHDR(n, wait, m->m_type);
		l = MHLEN;
	} else {
		copyhdr = 0;
		MGET(n, wait, m->m_type);
		l = MLEN;
	}
	if (n && len > l) {
		MCLGET(n, wait);
		if ((n->m_flags & M_EXT) == 0) {
			m_free(n);
			n = NULL;
		}
	}
	if (!n)
		return NULL;

	if (copyhdr) {
#ifdef __OpenBSD__
		M_MOVE_PKTHDR(n, m);
#else
		M_COPY_PKTHDR(n, m);
#endif
	}
	m_copydata(m, off, len, mtod(n, caddr_t));
	n->m_len = len;

	return n;
}
#endif

/*
 * pkthdr.aux chain manipulation.
 * we don't allow clusters at this moment. 
 */
struct mbuf *
m_aux_add2(m, af, type, p)
	struct mbuf *m;
	int af, type;
	void *p;
{
	struct mbuf *n;
	struct mauxtag *t;

	if ((m->m_flags & M_PKTHDR) == 0)
		return NULL;

	n = m_aux_find(m, af, type);
	if (n)
		return n;

	MGET(n, M_DONTWAIT, m->m_type);
	if (n == NULL)
		return NULL;

	t = mtod(n, struct mauxtag *);
	bzero(t, sizeof(*t));
	t->af = af;
	t->type = type;
	t->p = p;
	n->m_data += sizeof(struct mauxtag);
	n->m_len = 0;
	n->m_next = m->m_pkthdr.aux;
	m->m_pkthdr.aux = n;
	return n;
}

struct mbuf *
m_aux_find2(m, af, type, p)
	struct mbuf *m;
	int af, type;
	void *p;
{
	struct mbuf *n;
	struct mauxtag *t;

	if ((m->m_flags & M_PKTHDR) == 0)
		return NULL;

	for (n = m->m_pkthdr.aux; n; n = n->m_next) {
		t = (struct mauxtag *)n->m_dat;
		if (n->m_data != ((caddr_t)t) + sizeof(struct mauxtag)) {
			printf("m_aux_find: invalid m_data for mbuf=%p (%p %p)\n", n, t, n->m_data);
			continue;
		}
		if (t->af == af && t->type == type && t->p == p)
			return n;
	}
	return NULL;
}

struct mbuf *
m_aux_find(m, af, type)
	struct mbuf *m;
	int af, type;
{

	return m_aux_find2(m, af, type, NULL);
}

struct mbuf *
m_aux_add(m, af, type)
	struct mbuf *m;
	int af, type;
{

	return m_aux_add2(m, af, type, NULL);
}

void
m_aux_delete(m, victim)
	struct mbuf *m;
	struct mbuf *victim;
{
	struct mbuf *n, *prev, *next;
	struct mauxtag *t;

	if ((m->m_flags & M_PKTHDR) == 0)
		return;

	prev = NULL;
	n = m->m_pkthdr.aux;
	while (n) {
		t = (struct mauxtag *)n->m_dat;
		next = n->m_next;
		if (n->m_data != ((caddr_t)t) + sizeof(struct mauxtag)) {
			printf("m_aux_delete: invalid m_data for mbuf=%p (%p %p)\n", n, t, n->m_data);
			prev = n;
			n = next;
			continue;
		}
		if (n == victim) {
			if (prev)
				prev->m_next = n->m_next;
			else
				m->m_pkthdr.aux = n->m_next;
			n->m_next = NULL;
			m_free(n);
		} else
			prev = n;
		n = next;
	}
}

#ifdef __OpenBSD__
/* Get a packet tag structure along with specified data following. */
struct m_tag *
m_tag_get(type, len, wait)
	int type;
	int len;
	int wait;
{
	struct m_tag *t;

	if (len < 0)
		return (NULL);
	MALLOC(t, struct m_tag *, len + sizeof(struct m_tag), M_PACKET_TAGS,
	    wait);
	if (t == NULL)
		return (NULL);
	t->m_tag_id = type;
	t->m_tag_len = len;
	return (t);
}

/* Free a packet tag. */
void
m_tag_free(t)
	struct m_tag *t;
{
	FREE(t, M_PACKET_TAGS);
}

/* Prepend a packet tag. */
void
m_tag_prepend(m, t)
	struct mbuf *m;
	struct m_tag *t;
{
	SLIST_INSERT_HEAD(&m->m_pkthdr.tags, t, m_tag_link);
}

/* Unlink a packet tag. */
void
m_tag_unlink(m, t)
	struct mbuf *m;
	struct m_tag *t;
{
	SLIST_REMOVE(&m->m_pkthdr.tags, t, m_tag, m_tag_link);
}

/* Unlink and free a packet tag. */
void
m_tag_delete(m, t)
	struct mbuf *m;
	struct m_tag *t;
{
	m_tag_unlink(m, t);
	m_tag_free(t);
}

/* Unlink and free a packet tag chain, starting from given tag. */
void
m_tag_delete_chain(m, t)
	struct mbuf *m;
	struct m_tag *t;
{
	struct m_tag *p, *q;

	if (t != NULL)
		p = t;
	else
		p = SLIST_FIRST(&m->m_pkthdr.tags);
	if (p == NULL)
		return;
	while ((q = SLIST_NEXT(p, m_tag_link)) != NULL)
		m_tag_delete(m, q);
	m_tag_delete(m, p);
}

/* Find a tag, starting from a given position. */
struct m_tag *
m_tag_find(m, type, t)
	struct mbuf *m;
	int type;
	struct m_tag *t;
{
	struct m_tag *p;

	if (t == NULL)
		p = SLIST_FIRST(&m->m_pkthdr.tags);
	else
		p = SLIST_NEXT(t, m_tag_link);
	while (p != NULL) {
		if (p->m_tag_id == type)
			return (p);
		p = SLIST_NEXT(p, m_tag_link);
	}
	return (NULL);
}

/* Copy a single tag. */
struct m_tag *
m_tag_copy(t)
	struct m_tag *t;
{
	struct m_tag *p;

	p = m_tag_get(t->m_tag_id, t->m_tag_len, M_NOWAIT);
	if (p == NULL)
		return (NULL);
	bcopy(t + 1, p + 1, t->m_tag_len); /* Copy the data */
	return (p);
}

/*
 * Copy two tag chains. The destination mbuf (to) loses any attached
 * tags even if the operation fails. This should not be a problem, as
 * m_tag_copy_chain() is typically called with a newly-allocated
 * destination mbuf.
 */
int
m_tag_copy_chain(to, from)
	struct mbuf *to;
	struct mbuf *from;
{
	struct m_tag *p, *t, *tprev = NULL;

	m_tag_delete_chain(to, NULL);
	SLIST_FOREACH(p, &from->m_pkthdr.tags, m_tag_link) {
		t = m_tag_copy(p);
		if (t == NULL) {
			m_tag_delete_chain(to, NULL);
			return (0);
		}
		if (tprev == NULL)
			SLIST_INSERT_HEAD(&to->m_pkthdr.tags, t, m_tag_link);
		else {
			SLIST_INSERT_AFTER(tprev, t, m_tag_link);
			tprev = t;
		}
	}
	return (1);
}

/* Initialize tags on an mbuf. */
void
m_tag_init(m)
	struct mbuf *m;
{
	SLIST_INIT(&m->m_pkthdr.tags);
}

/* Get first tag in chain. */
struct m_tag *
m_tag_first(m)
	struct mbuf *m;
{
	return (SLIST_FIRST(&m->m_pkthdr.tags));
}

/* Get next tag in chain. */
struct m_tag *
m_tag_next(m, t)
	struct mbuf *m;
	struct m_tag *t;
{
	return (SLIST_NEXT(t, m_tag_link));
}
#endif /* OpenBSD */
