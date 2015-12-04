//==========================================================================
//
//      sys/kern/uipc_syscalls.c
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


/*	$OpenBSD: uipc_syscalls.c,v 1.29 1999/12/08 06:50:17 itojun Exp $	*/
/*	$NetBSD: uipc_syscalls.c,v 1.19 1996/02/09 19:00:48 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
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
 *	@(#)uipc_syscalls.c	8.4 (Berkeley) 2/21/94
 */

#include <sys/param.h>
#ifdef __ECOS
#include <cyg/io/file.h>
static int ecos_getsock(int fdes, struct file **fpp);
#define getsock(fdp, fdes, fpp) ecos_getsock(fdes, fpp)
#else
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/buf.h>
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#ifndef __ECOS
#include <sys/signalvar.h>
#include <sys/un.h>
#endif
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#ifndef __ECOS
#include <sys/mount.h>
#endif
#include <sys/syscallargs.h>

/*
 * System call interface to the socket abstraction.
 */
extern	struct fileops socketops;

#ifdef __ECOS
int
sys_socket(struct sys_socket_args *uap, register_t *retval)
{
#else
int
sys_socket(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_socket_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
#endif // __ECOS
	struct socket *so;
	struct file *fp;
	int fd, error;

#ifdef __ECOS
	if ((error = falloc(&fp, &fd)) != 0)
#else
	if ((error = falloc(p, &fp, &fd)) != 0)
#endif
		return (error);
	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	error = socreate(SCARG(uap, domain), &so, SCARG(uap, type),
			 SCARG(uap, protocol));
	if (error) {
#ifndef __ECOS
		fdremove(fdp, fd);
#endif
		ffree(fp);
	} else {
#ifdef __ECOS
		fp->f_data = (CYG_ADDRWORD)so;
#else
		fp->f_data = (caddr_t)so;
#endif
		*retval = fd;
	}
	return (error);
}

#ifdef __ECOS
int
sys_bind(struct sys_bind_args *uap, register_t *retval)
    {
#else
/* ARGSUSED */
int
sys_bind(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_bind_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) name;
		syscallarg(socklen_t) namelen;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	struct mbuf *nam;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = sockargs(&nam, (caddr_t)SCARG(uap, name), SCARG(uap, namelen),
			 MT_SONAME);
	if (error)
		return (error);
	error = sobind((struct socket *)fp->f_data, nam);
	m_freem(nam);
	return (error);
}

/* ARGSUSED */
#ifdef __ECOS
int
sys_listen(struct sys_listen_args *uap, register_t *retval)
{
#else
int
sys_listen(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_listen_args /* {
		syscallarg(int) s;
		syscallarg(int) backlog;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	return (solisten((struct socket *)fp->f_data, SCARG(uap, backlog)));
}

#ifdef __ECOS
int
    sys_accept(struct sys_accept_args *uap, register_t *retval)
{
#else
int
sys_accept(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_accept_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) name;
		syscallarg(socklen_t *) anamelen;
	} */ *uap = v;
#endif
	struct file *fp;
	struct mbuf *nam;
	socklen_t namelen;
	int error, s, tmpfd;
	register struct socket *so;

	if (SCARG(uap, name) && (error = copyin((caddr_t)SCARG(uap, anamelen),
	    (caddr_t)&namelen, sizeof (namelen))))
		return (error);
	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	s = splsoftnet();
	so = (struct socket *)fp->f_data;
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		splx(s);
		return (EINVAL);
	}
	if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
		splx(s);
		return (EWOULDBLOCK);
	}
	while (so->so_qlen == 0 && so->so_error == 0) {
		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
			       netcon, 0);
		if (error) {
			splx(s);
			return (error);
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		splx(s);
		return (error);
	}
#ifdef __ECOS
	if ((error = falloc(&fp, &tmpfd)) != 0) {
#else
	if ((error = falloc(p, &fp, &tmpfd)) != 0) {
#endif
		splx(s);
		return (error);
	}
	*retval = tmpfd;
	{ struct socket *aso = so->so_q;
	  if (soqremque(aso, 1) == 0)
		panic("accept");
	  so = aso;
	}
	fp->f_type = DTYPE_SOCKET;
	fp->f_flag = FREAD|FWRITE;
	fp->f_ops = &socketops;
#ifdef __ECOS
	fp->f_data = (CYG_ADDRWORD)so;
#else
	fp->f_data = (caddr_t)so;
#endif
	nam = m_get(M_WAIT, MT_SONAME);
	(void) soaccept(so, nam);
	if (SCARG(uap, name)) {
		if (namelen > nam->m_len)
			namelen = nam->m_len;
		/* SHOULD COPY OUT A CHAIN HERE */
		if ((error = copyout(mtod(nam, caddr_t),
		    (caddr_t)SCARG(uap, name), namelen)) == 0)
			error = copyout((caddr_t)&namelen,
			    (caddr_t)SCARG(uap, anamelen),
			    sizeof (*SCARG(uap, anamelen)));
	}
	m_freem(nam);
	splx(s);
	return (error);
}

#ifdef __ECOS
int
sys_connect(struct sys_connect_args *uap, register_t *retval)
{
#else
/* ARGSUSED */
int
sys_connect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_connect_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) name;
		syscallarg(socklen_t) namelen;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	register struct socket *so;
	struct mbuf *nam;
	int error, s;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
		return (EALREADY);
	error = sockargs(&nam, (caddr_t)SCARG(uap, name), SCARG(uap, namelen),
			 MT_SONAME);
	if (error)
		return (error);
	error = soconnect(so, nam);
	if (error)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		m_freem(nam);
		return (EINPROGRESS);
	}
	s = splsoftnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
			       netcon, 0);
		if (error)
			break;
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	splx(s);
bad:
	so->so_state &= ~SS_ISCONNECTING;
	m_freem(nam);
#ifndef __ECOS
	if (error == ERESTART)
		error = EINTR;
#endif
	return (error);
}

#ifndef __ECOS
int
sys_socketpair(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_socketpair_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
		syscallarg(int *) rsv;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error, sv[2];

	error = socreate(SCARG(uap, domain), &so1, SCARG(uap, type),
			 SCARG(uap, protocol));
	if (error)
		return (error);
	error = socreate(SCARG(uap, domain), &so2, SCARG(uap, type),
			 SCARG(uap, protocol));
	if (error)
		goto free1;
	if ((error = falloc(p, &fp1, &fd)) != 0)
		goto free2;
	sv[0] = fd;
	fp1->f_flag = FREAD|FWRITE;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = (caddr_t)so1;
	if ((error = falloc(p, &fp2, &fd)) != 0)
		goto free3;
	fp2->f_flag = FREAD|FWRITE;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = (caddr_t)so2;
	sv[1] = fd;
	if ((error = soconnect2(so1, so2)) != 0)
		goto free4;
	if (SCARG(uap, type) == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 if ((error = soconnect2(so2, so1)) != 0)
			goto free4;
	}
	error = copyout((caddr_t)sv, (caddr_t)SCARG(uap, rsv),
	    2 * sizeof (int));
	if (error == 0)
		return (error);
free4:
	ffree(fp2);
	fdremove(fdp, sv[1]);
free3:
	ffree(fp1);
	fdremove(fdp, sv[0]);
free2:
	(void)soclose(so2);
free1:
	(void)soclose(so1);
	return (error);
}
#endif

#ifdef __ECOS
int
sys_sendto(struct sys_sendto_args *uap, register_t *retval)
{
#else
int
sys_sendto(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_sendto_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(struct sockaddr *) to;
		syscallarg(socklen_t) tolen;
	} */ *uap = v;
#endif // __ECOS
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = (caddr_t)SCARG(uap, to);
	msg.msg_namelen = SCARG(uap, tolen);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	aiov.iov_base = (char *)SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
#ifdef __ECOS
	return (sendit(SCARG(uap, s), &msg, SCARG(uap, flags), retval));
#else
	return (sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval));
#endif
}

#ifndef __ECOS
int
sys_sendmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_sendmsg_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin(SCARG(uap, msg), (caddr_t)&msg, sizeof (msg));
	if (error)
		return (error);
	if (msg.msg_iovlen <= 0 || msg.msg_iovlen > IOV_MAX)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * msg.msg_iovlen, M_IOV, M_WAITOK);
	else
		iov = aiov;
	if (msg.msg_iovlen &&
	    (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)))))
		goto done;
	msg.msg_iov = iov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	error = sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

#ifdef __ECOS
int
sendit(int s, struct msghdr *mp, int flags, register_t *retsize)
{
#else
int
sendit(p, s, mp, flags, retsize)
	register struct proc *p;
	int s;
	register struct msghdr *mp;
	int flags;
	register_t *retsize;
{
#endif // __ECOS
	struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	struct mbuf *to, *control;
	int len, error;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif
	
	if ((error = getsock(p->p_fd, s, &fp)) != 0)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
#ifndef __ECOS
	auio.uio_procp = p;
#endif
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		/* Don't allow sum > SSIZE_MAX */
		if (iov->iov_len > SSIZE_MAX ||
		    (auio.uio_resid += iov->iov_len) > SSIZE_MAX)
			return (EINVAL);
	}
	if (mp->msg_name) {
		error = sockargs(&to, mp->msg_name, mp->msg_namelen,
				 MT_SONAME);
		if (error)
			return (error);
	} else
		to = 0;
	if (mp->msg_control) {
		if (mp->msg_controllen < sizeof(struct cmsghdr)
#ifdef COMPAT_OLDSOCK
		    && mp->msg_flags != MSG_COMPAT
#endif
		) {
			error = EINVAL;
			goto bad;
		}
		error = sockargs(&control, mp->msg_control,
				 mp->msg_controllen, MT_CONTROL);
		if (error)
			goto bad;
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags == MSG_COMPAT) {
			register struct cmsghdr *cm;

			M_PREPEND(control, sizeof(*cm), M_WAIT);
			if (control == 0) {
				error = ENOBUFS;
				goto bad;
			} else {
				cm = mtod(control, struct cmsghdr *);
				cm->cmsg_len = control->m_len;
				cm->cmsg_level = SOL_SOCKET;
				cm->cmsg_type = SCM_RIGHTS;
			}
		}
#endif
	} else
		control = 0;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	error = sosend((struct socket *)fp->f_data, to, &auio,
		       NULL, control, flags);
	if (error) {
#ifdef __ECOS
		if (auio.uio_resid != len && 
                    (error == EINTR || error == EWOULDBLOCK))
#else
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
#endif
			error = 0;
#ifndef __ECOS
		if (error == EPIPE)
			psignal(p, SIGPIPE);
#endif
	}
	if (error == 0)
		*retsize = len - auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, s, UIO_WRITE,
				ktriov, *retsize, error);
		FREE(ktriov, M_TEMP);
	}
#endif
bad:
	if (to)
		m_freem(to);
	return (error);
}

#ifdef __ECOS
int 
sys_recvfrom(struct sys_recvfrom_args *uap, register_t *retval)
{
#else
int
sys_recvfrom(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(struct sockaddr *) from;
		syscallarg(socklen_t *) fromlenaddr;
	} */ *uap = v;
#endif // __ECOS
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (SCARG(uap, fromlenaddr)) {
		error = copyin((caddr_t)SCARG(uap, fromlenaddr),
			       (caddr_t)&msg.msg_namelen,
			       sizeof (msg.msg_namelen));
		if (error)
			return (error);
	} else
		msg.msg_namelen = 0;
	msg.msg_name = (caddr_t)SCARG(uap, from);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = SCARG(uap, flags);
#ifdef __ECOS
	return (recvit(SCARG(uap, s), &msg,
		       (caddr_t)SCARG(uap, fromlenaddr), retval));
#else
	return (recvit(p, SCARG(uap, s), &msg,
		       (caddr_t)SCARG(uap, fromlenaddr), retval));
#endif
}

#ifndef __ECOS
int
sys_recvmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_recvmsg_args /* {
		syscallarg(int) s;
		syscallarg(struct msghdr *) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	register int error;

	error = copyin((caddr_t)SCARG(uap, msg), (caddr_t)&msg,
		       sizeof (msg));
	if (error)
		return (error);
	if (msg.msg_iovlen <= 0 || msg.msg_iovlen > IOV_MAX)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * msg.msg_iovlen, M_IOV, M_WAITOK);
	else
		iov = aiov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = SCARG(uap, flags) &~ MSG_COMPAT;
#else
	msg.msg_flags = SCARG(uap, flags);
#endif
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	error = copyin((caddr_t)uiov, (caddr_t)iov,
		       (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
	if (error)
		goto done;
	if ((error = recvit(p, SCARG(uap, s), &msg, (caddr_t)0, retval)) == 0) {
		msg.msg_iov = uiov;
		error = copyout((caddr_t)&msg, (caddr_t)SCARG(uap, msg),
		    sizeof(msg));
	}
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

#ifdef __ECOS
int
recvit(int s, struct msghdr *mp, caddr_t namelenp, register_t *retsize)
{
#else
int
recvit(p, s, mp, namelenp, retsize)
	register struct proc *p;
	int s;
	register struct msghdr *mp;
	caddr_t namelenp;
	register_t *retsize;
{
#endif // __ECOS
	struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	size_t len;
	int error;
	struct mbuf *from = 0, *control = 0;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if ((error = getsock(p->p_fd, s, &fp)) != 0)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
#ifndef __ECOS
	auio.uio_procp = p;
#endif
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		/* Don't allow sum > SSIZE_MAX */
		if (iov->iov_len > SSIZE_MAX ||
		    (auio.uio_resid += iov->iov_len) > SSIZE_MAX)
			return (EINVAL);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	error = soreceive((struct socket *)fp->f_data, &from, &auio,
			  NULL, mp->msg_control ? &control : NULL,
			  &mp->msg_flags);
	if (error) {
#ifdef __ECOS
		if (auio.uio_resid != len && 
                    (error == EINTR || error == EWOULDBLOCK))
#else
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
#endif
			error = 0;
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, s, UIO_READ,
				ktriov, len - auio.uio_resid, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	if (error)
		goto out;
	*retsize = len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || from == 0)
			len = 0;
		else {
		        /* save sa_len before it is destroyed by MSG_COMPAT */
			if (len > from->m_len)
				len = from->m_len;
			/* else if len < from->m_len ??? */
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				mtod(from, struct osockaddr *)->sa_family =
				    mtod(from, struct sockaddr *)->sa_family;
#endif
			error = copyout(mtod(from, caddr_t),
			    (caddr_t)mp->msg_name, (unsigned)len);
			if (error)
				goto out;
		}
		mp->msg_namelen = len;
		if (namelenp &&
		    (error = copyout((caddr_t)&len, namelenp, sizeof (int)))) {
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				error = 0;	/* old recvfrom didn't check */
			else
#endif
			goto out;
		}
	}
	if (mp->msg_control) {
#ifdef COMPAT_OLDSOCK
		/*
		 * We assume that old recvmsg calls won't receive access
		 * rights and other control info, esp. as control info
		 * is always optional and those options didn't exist in 4.3.
		 * If we receive rights, trim the cmsghdr; anything else
		 * is tossed.
		 */
		if (control && mp->msg_flags & MSG_COMPAT) {
			if (mtod(control, struct cmsghdr *)->cmsg_level !=
			    SOL_SOCKET ||
			    mtod(control, struct cmsghdr *)->cmsg_type !=
			    SCM_RIGHTS) {
				mp->msg_controllen = 0;
				goto out;
			}
			control->m_len -= sizeof (struct cmsghdr);
			control->m_data += sizeof (struct cmsghdr);
		}
#endif
		len = mp->msg_controllen;
		if (len <= 0 || control == 0)
			len = 0;
		else {
			struct mbuf *m = control;
			caddr_t p = (caddr_t)mp->msg_control;

			do {
				i = m->m_len;
				if (len < i) {
					mp->msg_flags |= MSG_CTRUNC;
					i = len;
				}
				error = copyout(mtod(m, caddr_t), p,
				    (unsigned)i);
				if (m->m_next)
					i = ALIGN(i);
				p += i;
				len -= i;
				if (error != 0 || len <= 0)
					break;
			} while ((m = m->m_next) != NULL);
			len = p - (caddr_t)mp->msg_control;
		}
		mp->msg_controllen = len;
	}
out:
	if (from)
		m_freem(from);
	if (control)
		m_freem(control);
	return (error);
}

/* ARGSUSED */
#ifdef __ECOS
int
sys_shutdown(struct sys_shutdown_args *uap, register_t *retval)
{
#else
int
sys_shutdown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_shutdown_args /* {
		syscallarg(int) s;
		syscallarg(int) how;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	return (soshutdown((struct socket *)fp->f_data, SCARG(uap, how)));
}

#ifdef __ECOS
int
sys_setsockopt(struct sys_setsockopt_args *uap, register_t *retval)
{
#else
/* ARGSUSED */
int
sys_setsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_setsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) name;
		syscallarg(caddr_t) val;
		syscallarg(socklen_t) valsize;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	if (SCARG(uap, valsize) > MCLBYTES)
		return (EINVAL);
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (SCARG(uap, valsize) > MLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				return (ENOBUFS);
			}
		}
		if (m == NULL)
			return (ENOBUFS);
		error = copyin(SCARG(uap, val), mtod(m, caddr_t),
			       SCARG(uap, valsize));
		if (error) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = SCARG(uap, valsize);
	}
	return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
			 SCARG(uap, name), m));
}

#ifdef __ECOS
int
sys_getsockopt(struct sys_getsockopt_args *uap, register_t *retval)
{
#else
/* ARGSUSED */
int
sys_getsockopt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) name;
		syscallarg(caddr_t) val;
		syscallarg(socklen_t *) avalsize;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	struct mbuf *m = NULL;
	socklen_t valsize;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	if (SCARG(uap, val)) {
		error = copyin((caddr_t)SCARG(uap, avalsize),
			       (caddr_t)&valsize, sizeof (valsize));
		if (error)
			return (error);
	} else
		valsize = 0;
	if ((error = sogetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), &m)) == 0 && SCARG(uap, val) && valsize &&
	    m != NULL) {
		if (valsize > m->m_len)
			valsize = m->m_len;
		error = copyout(mtod(m, caddr_t), SCARG(uap, val), valsize);
		if (error == 0)
			error = copyout((caddr_t)&valsize,
			    (caddr_t)SCARG(uap, avalsize), sizeof (valsize));
	}
	if (m != NULL)
		(void) m_free(m);
	return (error);
}

#ifndef __ECOS
int
sys_pipe(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_pipe_args /* {
		syscallarg(int *) fdp;
	} */ *uap = v;
	int error, fds[2];
	register_t rval[2];

	if ((error = sys_opipe(p, v, rval)) == -1)
		return (error);
	
	fds[0] = rval[0];
	fds[1] = rval[1];
	error = copyout((caddr_t)fds, (caddr_t)SCARG(uap, fdp),
	    2 * sizeof (int));
	if (error) {
		fdrelease(p, retval[0]);
		fdrelease(p, retval[1]);
	}
	return (error);
}

#ifdef OLD_PIPE

/* ARGSUSED */
int
sys_opipe(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	struct file *rf, *wf;
	struct socket *rso, *wso;
	int fd, error;

	if ((error = socreate(AF_UNIX, &rso, SOCK_STREAM, 0)) != 0)
		return (error);
	if ((error = socreate(AF_UNIX, &wso, SOCK_STREAM, 0)) != 0)
		goto free1;
	if ((error = falloc(p, &rf, &fd)) != 0)
		goto free2;
	retval[0] = fd;
	rf->f_flag = FREAD;
	rf->f_type = DTYPE_SOCKET;
	rf->f_ops = &socketops;
	rf->f_data = (caddr_t)rso;
	if ((error = falloc(p, &wf, &fd)) != 0)
		goto free3;
	wf->f_flag = FWRITE;
	wf->f_type = DTYPE_SOCKET;
	wf->f_ops = &socketops;
	wf->f_data = (caddr_t)wso;
	retval[1] = fd;
	if ((error = unp_connect2(wso, rso)) != 0)
		goto free4;
	return (0);
free4:
	ffree(wf);
	fdremove(fdp, retval[1]);
free3:
	ffree(rf);
	fdremove(fdp, retval[0]);
free2:
	(void)soclose(wso);
free1:
	(void)soclose(rso);
	return (error);
}
#endif
#endif // __ECOS

/*
 * Get socket name.
 */
#ifdef __ECOS
int
sys_getsockname(struct sys_getsockname_args *uap, register_t *retval)
{
#else
/* ARGSUSED */
int
sys_getsockname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getsockname_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(socklen_t *) alen;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	register struct socket *so;
	struct mbuf *m;
	socklen_t len;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, fdes), &fp)) != 0)
		return (error);
	error = copyin((caddr_t)SCARG(uap, alen), (caddr_t)&len, sizeof (len));
	if (error)
		return (error);
	so = (struct socket *)fp->f_data;
	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return (ENOBUFS);
	error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, 0, m, 0);
	if (error)
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
	error = copyout(mtod(m, caddr_t), (caddr_t)SCARG(uap, asa), len);
	if (error == 0)
		error = copyout((caddr_t)&len, (caddr_t)SCARG(uap, alen),
		    sizeof (len));
bad:
	m_freem(m);
	return (error);
}

/*
 * Get name of peer for connected socket.
 */
/* ARGSUSED */
#ifdef __ECOS
int
sys_getpeername(struct sys_getpeername_args *uap, register_t *retval)
{
#else
int
sys_getpeername(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getpeername_args /* {
		syscallarg(int) fdes;
		syscallarg(caddr_t) asa;
		syscallarg(socklen_t *) alen;
	} */ *uap = v;
#endif // __ECOS
	struct file *fp;
	register struct socket *so;
	struct mbuf *m;
	socklen_t len;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, fdes), &fp)) != 0)
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0)
		return (ENOTCONN);
	error = copyin((caddr_t)SCARG(uap, alen), (caddr_t)&len, sizeof (len));
	if (error)
		return (error);
	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return (ENOBUFS);
	error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, 0, m, 0);
	if (error)
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
	error = copyout(mtod(m, caddr_t), (caddr_t)SCARG(uap, asa), len);
	if (error == 0)
		error = copyout((caddr_t)&len, (caddr_t)SCARG(uap, alen),
		    sizeof (len));
bad:
	m_freem(m);
	return (error);
}

int
sockargs(mp, buf, buflen, type)
	struct mbuf **mp;
	caddr_t buf;
	socklen_t buflen;
	int type;
{
	register struct sockaddr *sa;
	register struct mbuf *m;
	int error;

	if (buflen > MLEN) {
#ifdef COMPAT_OLDSOCK
		if (type == MT_SONAME && buflen <= 112)
			buflen = MLEN;		/* unix domain compat. hack */
		else
#endif
		return (EINVAL);
	}
	m = m_get(M_WAIT, type);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = buflen;
	error = copyin(buf, mtod(m, caddr_t), buflen);
	if (error) {
		(void) m_free(m);
		return (error);
	}
	*mp = m;
	if (type == MT_SONAME) {
		sa = mtod(m, struct sockaddr *);

#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = buflen;
	}
	return (0);
}

#ifdef __ECOS
static int
ecos_getsock(int fdes, struct file **fpp)
{
    struct file *fp;
    if (getfp(fdes, &fp))
        return (EBADF);
    if (fp->f_type != DTYPE_SOCKET)
        return (ENOTSOCK);
    *fpp = fp;
    return (0);
}
#else
int
getsock(fdp, fdes, fpp)
	struct filedesc *fdp;
	int fdes;
	struct file **fpp;
{
	register struct file *fp;

	if ((unsigned)fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fdes]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_SOCKET)
		return (ENOTSOCK);
	*fpp = fp;
	return (0);
}
#endif
