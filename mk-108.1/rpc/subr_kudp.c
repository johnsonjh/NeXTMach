/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.26 88/02/08 
 */


/*
 * subr_kudp.c
 * Subroutines to do UDP/IP sendto and recvfrom in the kernel
 */
#import <sys/param.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/mbuf.h>
#import <net/if.h>
#import <net/route.h>
#import <netinet/in.h>
#import <netinet/in_pcb.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>

#if	NeXT
/*
 * ku_fastsend() is turned off by default on NeXT. It isn't really
 * that much faster anyway.
 */
#import <sys/errno.h>

#ifndef SLOWSEND
#define SLOWSEND
#endif

#endif	NeXT

/*
 * General kernel udp stuff.
 * The routines below are used by both the client and the server side
 * rpc code.
 */

/*
 * Kernel recvfrom.
 * Pull address mbuf and data mbuf chain off socket receive queue.
 */
struct mbuf *
ku_recvfrom(so, from)
	struct socket *so;
	struct sockaddr_in *from;
{
	register struct mbuf	*m;
	register struct mbuf	*m0;
	struct mbuf		*nextrecord;
	register struct sockbuf	*sb = &so->so_rcv;
	register int		len = 0;

#ifdef RPCDEBUG
	rpc_debug(4, "urrecvfrom so=%X\n", so);
#endif
	m = sb->sb_mb;
	if (m == NULL) {
		return (m);
	}
	nextrecord = m->m_act;

	*from = *mtod(m, struct sockaddr_in *);

	/*
	 * Advance to the data part of the packet,
	 * freeing the address part (and rights if present).
	 */
	for (m0 = m; m0 && m0->m_type != MT_DATA; ) {
		sbfree(sb, m0);
		m0 = m_free(m0);
	}
	if (m0 == NULL) {
		printf("ku_recvfrom: no body!\n");
		sb->sb_mb = nextrecord;
		return (m0);
	}

	/*
	 * Transfer ownership of the remainder of the packet
	 * record away from the socket and advance the socket
	 * to the next record.  Calculate the record's length
	 * while we're at it.
	 */
	for (m = m0; m; m = m->m_next) {
		sbfree(sb, m);
		len += m->m_len;
	}
	sb->sb_mb = nextrecord;

	if (len > UDPMSGSIZE) {
		printf("ku_recvfrom: len = %d\n", len);
	}

#ifdef RPCDEBUG
	rpc_debug(4, "urrecvfrom %d from %X\n", len, from->sin_addr.s_addr);
#endif
	return (m0);
}

int Sendtries = 0;
int Sendok = 0;

/*
 * Kernel sendto.
 * Set addr and send off via UDP.
 * Use ku_fastsend if possible.
 */
int
ku_sendto_mbuf(so, m, addr)
	struct socket *so;
	struct mbuf *m;
	struct sockaddr_in *addr;
{
#ifdef SLOWSEND
	register struct inpcb *inp = sotoinpcb(so);
	int s;
#endif
	int error;

#ifdef RPCDEBUG
	rpc_debug(4, "ku_sendto_mbuf %X\n", addr->sin_addr.s_addr);
#endif
	Sendtries++;
#ifdef SLOWSEND
	{
		struct mbuf *to;
		struct in_addr laddr;

		to = m_get(M_WAIT, MT_SONAME);
		if (to == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		to->m_len = sizeof(*addr);
		*(mtod(to, struct sockaddr_in *)) = *addr;

		s = splnet();
		laddr = inp->inp_laddr;
		if (error = in_pcbconnect(inp, to)) {
			printf("pcbsetaddr failed %d\n", error);
			(void) splx(s);
			m_freem(m);
			m_free(to);
			return (error);
		}
		error = udp_output(inp, m);
		in_pcbdisconnect(inp);
		inp->inp_laddr = laddr;
		(void) splx(s);
		m_free(to);
	}
#else
	error = ku_fastsend(so, m, addr);
#endif
	Sendok++;
#ifdef RPCDEBUG
	rpc_debug(4, "ku_sendto returning %d\n", error);
#endif
	return (error);
}

#ifdef RPCDEBUG
int rpcdebug = 2;

/*VARARGS2*/
rpc_debug(level, str, a1, a2, a3, a4, a5, a6, a7, a8, a9)
        int level;
        char *str;
        int a1, a2, a3, a4, a5, a6, a7, a8, a9;
{

        if (level <= rpcdebug)
                printf(str, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}
#endif
