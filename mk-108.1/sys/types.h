/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: make uid_t and gid_t signed
 *
 * 02-Mar-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_BUGFIX:  Restored previous unsigned fixes for _quad and
 *	off_t types by Bill Bolosky although this time under a
 *	different conditional and only under -DKERNEL so that, sigh,
 *	user programs which import the file but don't use the type
 *	consistently won't break.  Of course, lseek takes an off_t as
 *	its second paramter which can be legitimately signed.  This
 *	whole thing is a hack and I suspect that eventually someone is
 *	going to have to end up rewriting some kernel code.
 *	[ V5.1(F4) ]
 *
 * 24-Oct-86  Jonathan J. Chew (jjc) at Carnegie-Mellon University
 *	Added 68000 dependent definition of "label_t" and "physadr".
 *	Conditionalized on whether ASSEMBLER is undefined.
 *
 *  7-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	Merged Multimax changes.
 *
 * 24-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Moved boolean declaration to its own file.
 *
 * 23-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Renamed "machtypes.h" to "types.h".
 *
 * 16-Jul-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	CS_GENERIC: changed type quad from longs to u_longs,
 *	and off_t from int to u_long.
 *
 * 19-Feb-86  Bill Bolosky (bolosky) at Carnegie-Mellon University
 *	romp: Added alternate definitions of label_t and physaddr.
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3.
 *
 * 15-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Allow for nested calls of types.h.
 */

#ifndef _TYPES_
#define	_TYPES_

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)types.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	ASSEMBLER

/*
 * Basic system types and major/minor device constructing/busting macros.
 */

/* major part of a device */
#define	major(x)	((int)(((unsigned)(x)>>8)&0377))

/* minor part of a device */
#define	minor(x)	((int)((x)&0377))

/* make a device number */
#define	makedev(x,y)	((dev_t)(((x)<<8) | (y)))

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef	unsigned short	ushort;		/* sys III compat */

#if	defined(vax) || defined(ns32000)
typedef	struct	_physadr { int r[1]; } *physadr;
typedef	struct	label_t	{
	int	val[14];
} label_t;
#endif	defined(vax) || defined(ns32000)
#ifdef	romp
typedef	struct	_physadr { int r[1]; } *physadr;
typedef	struct	label_t	{
	int	val[16];
} label_t;
#endif	romp
#ifdef	mc68000
typedef struct  _physadr { short r[1]; } *physadr;
typedef struct  label_t {
        int     val[13];
} label_t;
#endif	mc68000
#ifdef	KERNEL
typedef	struct	_quad { u_long val[2]; } quad;
#else	KERNEL
typedef	struct	_quad { long val[2]; } quad;
#endif	KERNEL
typedef	long	daddr_t;
typedef	char *	caddr_t;
typedef	u_long	ino_t;
#ifndef _SIZE_T
#define _SIZE_T
#ifdef __STRICT_BSD__
typedef long	size_t;
#else
typedef unsigned long size_t;
#endif /* __STRICT_BSD__ */
#endif /* _SIZE_T */
#ifndef _TIME_T
#define _TIME_T
typedef	long	time_t;
#endif /* _TIME_T */
typedef	short	dev_t;
#if	KERNEL
typedef	u_long	off_t;
#else	KERNEL
typedef	long	off_t;
#endif	KERNEL
typedef	short	uid_t;
typedef	short	gid_t;

#define	NBBY	8		/* number of bits in a byte */
/*
 * Select uses bit masks of file descriptors in longs.
 * These macros manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here
 * should be >= NOFILE (param.h).
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	256
#endif

typedef long	fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))

#endif

#endif	ASSEMBLER
