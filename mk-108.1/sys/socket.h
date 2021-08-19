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
 * $Log:	socket.h,v $
 * Revision 2.5  89/03/09  22:07:36  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  17:56:16  gm0w
 * 	Made CMUCS branches always true, made MACH_VMTP
 * 	and VICE conditional defines unconditional.
 * 	[89/02/14            mrt]
 * 
 * Revision 2.3  88/08/24  02:44:39  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:23:09  mwyoung]
 *
 * 09-Apr-88  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added SO_USEPRIV definition (based on ECE implementation);
 *	CS_SOCKET => CMUCS.
 *	[ V5.1(XF23) ]
 *
 * 06-Jan-88  Jay Kistler (jjk) at Carnegie Mellon University
 *	Made file reentrant.  Added declarations for __STDC__.
 *
 *  1-Jul-87  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Updated from new VMTP sources from Stanford (June 87).
 *
 * 28-May-87  Daniel Julin (dpj) at Carnegie-Mellon University
 *	Added VMTP.
 *
 *  7-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Merge VICE changes -- include vice.h and change to #if VICE.
 *
 *  2-Dec-86  Jay Kistler (jjk) at Carnegie-Mellon University
 *	VICE:  added SO_GREEDY option.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 16-Oct-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CMUCS:  added SO_CANTSIG definition.
 *	[V1(1)]
 *
 */
/*
 * Copyright (c) 1982,1985, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)socket.h	7.1 (Berkeley) 6/4/86
 */

/*
 * Definitions related to sockets: types, address families, options.
 */

#ifndef	_SYS_SOCKET_H_
#define _SYS_SOCKET_H_

#import <sys/types.h>

/*
 * Types
 */
#define SOCK_STREAM	1		/* stream socket */
#define SOCK_DGRAM	2		/* datagram socket */
#define SOCK_RAW	3		/* raw-protocol interface */
#define SOCK_RDM	4		/* reliably-delivered message */
#define SOCK_SEQPACKET	5		/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define SO_DEBUG	0x0001		/* turn on debugging info recording */
#define SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define SO_REUSEADDR	0x0004		/* allow local address reuse */
#define SO_KEEPALIVE	0x0008		/* keep connections alive */
#define SO_DONTROUTE	0x0010		/* just use interface addresses */
#define SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define SO_LINGER	0x0080		/* linger on close if data present */
#define SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define SO_USEPRIV	0x4000		/* allocate from privileged port area */
#define SO_CANTSIG	0x8000		/* prevent SIGPIPE on SS_CANTSENDMORE */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define SO_ERROR	0x1007		/* get error status and clear */
#define SO_TYPE		0x1008		/* get socket type */

/*
 * Structure used for manipulating linger option.
 */
struct	linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */
#define AF_UNSPEC	0		/* unspecified */
#define AF_UNIX		1		/* local to host (pipes, portals) */
#define AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define AF_IMPLINK	3		/* arpanet imp addresses */
#define AF_PUP		4		/* pup protocols: e.g. BSP */
#define AF_CHAOS	5		/* mit CHAOS protocols */
#define AF_NS		6		/* XEROX NS protocols */
#define AF_NBS		7		/* nbs protocols */
#define AF_ECMA		8		/* european computer manufacturers */
#define AF_DATAKIT	9		/* datakit protocols */
#define AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define AF_SNA		11		/* IBM SNA */
#define AF_DECnet	12		/* DECnet */
#define AF_DLI		13		/* Direct data link interface */
#define AF_LAT		14		/* LAT */
#define AF_HYLINK	15		/* NSC Hyperchannel */
#define AF_APPLETALK	16		/* Apple Talk */

#define AF_MAX		17

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
	u_short	sa_family;		/* address family */
	char	sa_data[14];		/* up to 14 bytes of direct address */
};

/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct sockproto {
	u_short	sp_family;		/* address family */
	u_short	sp_protocol;		/* protocol */
};

/*
 * Protocol families, same as address families for now.
 */
#define PF_UNSPEC	AF_UNSPEC
#define PF_UNIX		AF_UNIX
#define PF_INET		AF_INET
#define PF_IMPLINK	AF_IMPLINK
#define PF_PUP		AF_PUP
#define PF_CHAOS	AF_CHAOS
#define PF_NS		AF_NS
#define PF_NBS		AF_NBS
#define PF_ECMA		AF_ECMA
#define PF_DATAKIT	AF_DATAKIT
#define PF_CCITT	AF_CCITT
#define PF_SNA		AF_SNA
#define PF_DECnet	AF_DECnet
#define PF_DLI		AF_DLI
#define PF_LAT		AF_LAT
#define PF_HYLINK	AF_HYLINK
#define PF_APPLETALK	AF_APPLETALK

#define PF_MAX		AF_MAX

/*
 * Maximum queue length specifiable by listen.
 */
#define SOMAXCONN	5

/*
 * Message header for recvmsg and sendmsg calls.
 */
struct msghdr {
	caddr_t	msg_name;		/* optional address */
	int	msg_namelen;		/* size of address */
	struct	iovec *msg_iov;		/* scatter/gather array */
	int	msg_iovlen;		/* # elements in msg_iov */
	caddr_t	msg_accrights;		/* access rights sent/received */
	int	msg_accrightslen;
};

#define MSG_OOB		0x1		/* process out-of-band data */
#define MSG_PEEK	0x2		/* peek at incoming message */
#define MSG_DONTROUTE	0x4		/* send without using routing tables */

#define MSG_MAXIOVLEN	16

#if	defined(__STDC__) && !defined(KERNEL)
extern int accept(int, struct sockaddr *, int *);
extern int bind(int, struct sockaddr *, int);
extern int connect(int, struct sockaddr *, int);
extern int getpeername(int, struct sockaddr *, int *);
extern int getsockname(int, struct sockaddr *, int *);
extern int getsockopt(int, int, int, void *optval, int *);
extern int setsockopt(int, int, int, void *optval, int);
extern int listen(int, int);
extern int recv(int, void *, int, int);
extern int recvfrom(int, void *, int, int, struct sockaddr *, int *);
extern int recvmsg(int, struct msghdr *, int);
extern int send(int, void *, int, int);
extern int sendto(int, void *, int, int, struct sockaddr *, int);
extern int sendmsg(int, struct msghdr *, int);
extern int socket(int, int, int);
extern int socketpair(int, int, int, int *);
#endif	defined(__STDC__) && !defined(KERNEL)

#endif	_SYS_SOCKET_H_

