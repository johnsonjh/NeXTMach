/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 19-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Made variable definitions extern.  Panicstr is const char *.
 *
 * 27-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	NFS 4.0 Changes: added dump_vp (so we can get core dumps working)
 *			 removed extranious cruft from 3.2 port.
 *
 * 11-Aug-87  Peter King (king) at NeXT
 *	SUN_VFS: Added *rootvp and *specvp for the nfs routines.
 *	     Added *bdevp for the specfs routines.
 *
 * 19-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Make redefinitions of insque/remque always ifdef'ed on lint.
 *
 * 24-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed to directly import declaration of boolean.
 *
 * 29-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Converted from "bool" type to "boolean_t" where necessary.
 *
 *  4-Nov-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Add sy_parallel flag to the system call entries to specify
 *	whether or not the system call can be executed in parallel.
 *
 * 03-Aug-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	CS_RPAUSE:  Added rpause() and fspause() declarations.
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)systm.h	7.1 (Berkeley) 6/4/86
 */

/* @(#)systm.h	1.3 87/05/29 3.2/4.3NFSSRC */

/*
 * Random set of variables
 * used by more than one
 * routine.
 */
extern	char version[];		/* system version */

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch.
 * Used in bounds checking on major
 * device numbers.
 */
extern int	nblkdev;

/*
 * Number of character switch entries.
 */
extern int	nchrdev;

extern int	mpid;			/* generic for unique process id's */

extern daddr_t	rablock;		/* block to be read ahead */
extern int	rasize;			/* size of block in rablock */
extern dev_t	rootdev;		/* device of the root */
extern struct vnode *rootvp;           /* vnode of root filesystem */
extern struct vnode *dumpvp;		/* vnode to dump on */
extern long	dumpsize;
extern dev_t	dumpdev;		/* device to take dumps on */
extern long	dumplo;			/* offset into dumpdev */

extern daddr_t	bmap();
#if	NeXT
#else	NeXT
caddr_t	calloc();
#endif	NeXT
unsigned max();			/* SUN_VFS */
unsigned min();			/* SUN_VFS */
int	uchar(), schar();	/* SUN_VFS */
struct vnode *bdevvp();
struct vnode *specvp();
#import <sys/boolean.h>	/* CS_RPAUSE */
int	rpause();		/* CS_RPAUSE */
boolean_t	fspause();	/* CS_RPAUSE */

/*
 * Structure of the system-entry table
 */
extern struct sysent
{
	short	sy_narg;		/* total number of arguments */
	short	sy_parallel;		/* can execute in parallel */
	int	(*sy_call)();		/* handler */
} sysent[];

extern const char *panicstr;
extern int	boothowto;	/* reboot flags, from console subsystem */
extern int	show_space;
extern int	selwait;

#ifdef	lint
/* casts to keep lint happy */
#define	insque(q,p)	_insque((caddr_t)q,(caddr_t)p)
#define	remque(q)	_remque((caddr_t)q)
#endif	lint


