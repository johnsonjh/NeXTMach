/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	mach_net_tcp.c,v $
 * Revision 2.2  89/02/25  18:05:50  gm0w
 * 	Changes for cleanup.
 * 
 * 16-Jan-88  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Created.
 *
 */
/*
 * File:	mach_net_tcp.c
 * Purpose:
 *	Message interface to TCP. To be used primarily by the network server,
 *	in cooperation with the kernel IPC code.
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/mbuf.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/uio.h>
#import <sys/kern_return.h>
#import <sys/port.h>
#import <kern/parallel.h>
#import <sys/errno.h>
#import <kern/xpr.h>

/*
 * Global variable to hold the current error code (a.k.a. u.u_error).
 */
int	np_error;

kern_return_t
mach_tcp_init(ServPort, tcp_port)
port_t		ServPort;
port_t		*tcp_port;
{
	/*
	 * Nothing yet ...
	 */
	return KERN_SUCCESS;
}


kern_return_t
mach_tcp_socket(ServPort, so)
	port_t		ServPort;
	struct socket	**so;
{
	unix_master();
	np_error = socreate(AF_INET,so,SOCK_STREAM,0);
	unix_release();
	if (np_error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


kern_return_t
mach_tcp_close(ServPort, so)
	port_t		ServPort;
	struct socket	*so;
{
	unix_master();
	np_error = soclose(so);
	XPR(XPR_TCP,("mach_tcp_close: np_error=%d, so=0x%x",np_error,so));
	unix_release();
	if (np_error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


kern_return_t
mach_tcp_bind(ServPort, so, name, namelen)
	port_t		ServPort;
	struct socket	*so;
	caddr_t		name;
	int		namelen;
{
	struct mbuf	*m;

	unix_master();
	m = m_get(M_WAIT, MT_SONAME);
	m->m_next = NULL;
	m->m_off = MMINOFF;
	m->m_len = namelen;
	bcopy(name,mtod(m,caddr_t),namelen);
	np_error = sobind(so,m);
	m_free(m);
	so->so_options |= SO_CANTSIG;
	unix_release();
	if (np_error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


kern_return_t
mach_tcp_listen(ServPort, so, backlog)
	port_t		ServPort;
	struct socket	*so;
	int		backlog;
{
	unix_master();
	np_error = solisten(so,backlog);
	unix_release();
	if (np_error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


kern_return_t
mach_tcp_accept(ServPort, so, name, namelen, newso)
	port_t		ServPort;
	struct socket	*so;
	caddr_t		name;
	int		*namelen;
	struct socket	**newso;
{
	struct mbuf	*nam;
	int		s;

	unix_master();
	s = splnet();
	XPR(XPR_TCP,("mach_tcp_accept entered, so=0x%x, newso=0x%x",so,newso));
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		np_error = EINVAL;
		XPR(XPR_TCP,("mach_tcp_accept failed, no SO_ACCEPTCONN, so=0x%x",so));
		splx(s);
		unix_release();
		return KERN_FAILURE;
	}
	if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
		np_error = EWOULDBLOCK;
		XPR(XPR_TCP,("mach_tcp_accept failed, EWOULDBLOCK, so=0x%x",so));
		splx(s);
		unix_release();
		return KERN_FAILURE;
	}
	while (so->so_qlen == 0 && so->so_error == 0) {
		XPR(XPR_TCP,("mach_tcp_accept top of sleep loop, so=0x%x",so));
		if (so->so_state & SS_CANTRCVMORE) {
			XPR(XPR_TCP,("mach_tcp_accept SS_CANTRCVMORE in sleep loop, so=0x%x",so));
			so->so_error = ECONNABORTED;
			break;
		}
		sleep((caddr_t)&so->so_timeo, PZERO+1);
	}
	XPR(XPR_TCP,("mach_tcp_accept out of sleep loop, so=0x%x",so));
	if (so->so_error) {
		np_error = so->so_error;
		so->so_error = 0;
		XPR(XPR_TCP,("mach_tcp_accept so_error=%d, so=0x%x",np_error,so));
		splx(s);
		unix_release();
		return KERN_FAILURE;
	}
	{ struct socket *aso = so->so_q;
	  if (soqremque(aso, 1) == 0)
		panic("accept");
	  *newso = aso;
	}
	nam = m_get(M_WAIT, MT_SONAME);
	(void) soaccept(*newso, nam);
	XPR(XPR_TCP,("mach_tcp_accept out of soaccept, np_error=%d, so=0x%x, *newso=0x%x",np_error,so,*newso));
	if (name) {
		if (*namelen > nam->m_len)
			*namelen = nam->m_len;
		/* SHOULD COPY OUT A CHAIN HERE */
		bcopy(mtod(nam, caddr_t), name, *namelen);
	}
	m_freem(nam);
	splx(s);
	unix_release();
	if (np_error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


kern_return_t
mach_tcp_connect(ServPort, so, name, namelen)
	port_t		ServPort;
	struct socket	*so;
	caddr_t		name;
	int		namelen;
{
	struct mbuf	*nam;
	int		s;

	unix_master();
	if ((so->so_state & SS_NBIO) &&
	    (so->so_state & SS_ISCONNECTING)) {
		np_error = EALREADY;
		XPR(XPR_TCP,("mach_tcp_connect: already connecting, so=0x%x",so));
		unix_release();
		return KERN_FAILURE;
	}
	nam = m_get(M_WAIT, MT_SONAME);
	if (nam == NULL) {
		np_error = ENOBUFS;		
		XPR(XPR_TCP,("mach_tcp_connect: no buffers, so=0x%x",so));
		unix_release();
		return KERN_FAILURE;
	}
	nam->m_len = namelen;
	bcopy(name,mtod(nam,caddr_t),namelen);
	np_error = soconnect(so, nam);
	XPR(XPR_TCP,("mach_tcp_connect: back from soconnect, np_error=%d, so=0x%x",np_error,so));
	if (np_error)
		goto bad;
	if ((so->so_state & SS_NBIO) &&
	    (so->so_state & SS_ISCONNECTING)) {
		np_error = EINPROGRESS;
		XPR(XPR_TCP,("mach_tcp_connect: in progress, so=0x%x",so));
		m_freem(nam);
		unix_release();
		return KERN_FAILURE;
	}
	s = splnet();
/*
	if (setjmp(&u.u_qsave)) {
		if (u.u_error == 0)
			u.u_error = EINTR;
		goto bad2;
	}
*/
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
		sleep((caddr_t)&so->so_timeo, PZERO+1);
	np_error = so->so_error;
	so->so_error = 0;
/* bad2: */
	splx(s);
	XPR(XPR_TCP,("mach_tcp_connect: woke up, np_error=%d, so=0x%x",np_error,so));
bad:
	so->so_state &= ~SS_ISCONNECTING;
	m_freem(nam);
	unix_release();
	if (np_error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}



kern_return_t
mach_tcp_send(ServPort, so, buf, len, flags)
	port_t		ServPort;
	struct socket	*so;
	caddr_t		buf;
	int		*len;
	int		flags;
	/*
	 * Note: len is an in/out argument, returning the number
	 * of bytes actually sent.
	 */
{
	int		error;
	struct iovec	aiov;
	struct uio	auio;

	unix_master();
	aiov.iov_base = buf;
	aiov.iov_len = *len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = *len;
	error = sosend(so, 0, &auio, flags, 0);
	*len = *len - auio.uio_resid;
	unix_release();
	if (error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}


kern_return_t
mach_tcp_recv(ServPort, so, buf, len, flags)
	port_t		ServPort;
	struct socket	*so;
	caddr_t		buf;
	int		*len;
	int		flags;
	/*
	 * Note: len is an in/out argument, returning the number
	 * of bytes actually received.
	 */
{
	int		error;
	struct iovec	aiov;
	struct uio	auio;
	struct mbuf	*from, *rights;

	unix_master();
	aiov.iov_base = buf;
	aiov.iov_len = *len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = *len;
	error = soreceive(so, &from, &auio, flags, &rights);
	*len = *len - auio.uio_resid;
	if (rights)
		m_freem(rights);
	if (from)
		m_freem(from);
	unix_release();
	if (error)
		return KERN_FAILURE;
	else
		return KERN_SUCCESS;
}




