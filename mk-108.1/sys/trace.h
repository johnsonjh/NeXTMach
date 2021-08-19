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
 * $Log:	trace.h,v $
 * Revision 2.5  89/03/09  22:08:54  rpd
 * 	More cleanup.
 * 
 * Revision 2.4  89/02/25  17:57:17  gm0w
 * 	Made all CMUCS conditional stuff always true. Changed
 * 	romp to ibmrt.
 * 	[89/02/14            mrt]
 * 
 * Revision 2.3  88/11/23  16:36:57  rpd
 * 	Added fd driver trace points from Acis.
 * 	[88/11/04  17:56:32  rpd]
 * 
 * Revision 2.2  88/08/24  02:49:44  mwyoung
 * 	Adjusted include file references.
 * 	[88/08/17  02:26:00  mwyoung]
 *
 * 24-Aug-87  Richard Sanzi (sanzi) at Carnegie-Mellon University
 *	Added several trace values for the romp from the 4.3 ACIS code.
 *
 * 14-Oct-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Conditionalize the previous bug fix on CS_BUGFIX.
 *
 *  7-Oct-86  David L. Black (dlb) at Carnegie-Mellon University
 *	trace(a,b,b) -> trace(a,b,c) bug fix.
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)trace.h	7.1 (Berkeley) 6/4/86
 */

#ifndef	_SYS_TRACE_H_
#define _SYS_TRACE_H_

/*
 * File system buffer tracing points; all trace <pack(dev, size), bn>
 */
#define TR_BREADHIT	0	/* buffer read found in cache */
#define TR_BREADMISS	1	/* buffer read not in cache */
#define TR_BWRITE	2	/* buffer written */
#define TR_BREADHITRA	3	/* buffer read-ahead found in cache */
#define TR_BREADMISSRA	4	/* buffer read-ahead not in cache */
#define TR_XFODMISS	5	/* exe fod read */
#define TR_XFODHIT	6	/* exe fod read */
#define TR_BRELSE	7	/* brelse */
#define TR_BREALLOC	8	/* expand/contract a buffer */

/*
 * Memory allocator trace points; all trace the amount of memory involved
 */
#define TR_MALL		10	/* memory allocated */

/*
 * Paging trace points: all are <vaddr, pid>
 */
#define TR_INTRANS	20	/* page intransit block */
#define TR_EINTRANS	21	/* page intransit wait done */
#define TR_FRECLAIM	22	/* reclaim from free list */
#define TR_RECLAIM	23	/* reclaim from loop */
#define TR_XSFREC	24	/* reclaim from free list instead of drum */
#define TR_XIFREC	25	/* reclaim from free list instead of fsys */
#define TR_WAITMEM	26	/* wait for memory in pagein */
#define TR_EWAITMEM	27	/* end memory wait in pagein */
#define TR_ZFOD		28	/* zfod page fault */
#define TR_EXFOD	29	/* exec fod page fault */
#define TR_VRFOD	30	/* vread fod page fault */
#define TR_CACHEFOD	31	/* fod in file system cache */
#define TR_SWAPIN	32	/* drum page fault */
#define TR_PGINDONE	33	/* page in done */
#define TR_SWAPIO	34	/* swap i/o request arrives */

/*
 * System call trace points.
 */
#define TR_VADVISE	40	/* vadvise occurred with <arg, pid> */

/*
 * Miscellaneous
 */
#define TR_STAMP	45	/* user said vtrace(VTR_STAMP, value); */
#ifdef	ibmrt
#define TR_RWIP		49	/* rwip() */

/*
 * Disk device driver (EESDI only at present) trace points
 */
#define TR_D_ENQ	50	/* enqueue - strategy called */
#define TR_D_GO		51	/* go - cmd given to adapter */
#define TR_D_INT	52	/* interrupt - from adapter */
/*
 * Floppy device driver trace points
 */
#define TR_F_STR	60	/* enqueue - fdstrategy called */
#define TR_F_STA	61	/* fdstart called */
#define TR_F_DGO	62	/* fddgo called */
#define TR_F_INT	63	/* interrupt from adapter - fdint() */
#define TR_F_AUT	64	/* fdautodensity - top of loop */
#define TR_F_REL	65	/* releasing a buffer */
#define TR_F_OPE	66	/* fdopen */
#define TR_F_CLO	67	/* fdclose */
#define TR_F_WAI	68	/* waiting for i/o complete on a buffer */
#define TR_F_FOR	69	/* fdformat */
#endif	ibmrt

/*
 * This defines the size of the trace flags array.
 */
#define TR_NFLAGS	100	/* generous */

#define TRCSIZ		4096

/*
 * Specifications of the vtrace() system call, which takes one argument.
 */
#define VTRACE		64+51

#define VTR_DISABLE	0		/* set a trace flag to 0 */
#define VTR_ENABLE	1		/* set a trace flag to 1 */
#define VTR_VALUE	2		/* return value of a trace flag */
#define VTR_UALARM	3		/* set alarm to go off (sig 16) */
					/* in specified number of hz */
#define VTR_STAMP	4		/* user specified stamp */

#ifdef	KERNEL
#ifdef	TRACE
extern char	traceflags[TR_NFLAGS];
extern struct	proc *traceproc;
extern int	tracebuf[TRCSIZ];
extern unsigned tracex;
extern int	tracewhich;

#define pack(a,b)	((a)<<16)|(b)
#define trace(a,b,c)	if (traceflags[a]) trace1(a,b,c)
#else	TRACE
#define trace(a,b,c)
#endif	TRACE

#endif	KERNEL
#endif	_SYS_TRACE_H_
