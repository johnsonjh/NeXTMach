/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 **********************************************************************
 * HISTORY
 * 27-Mar-90	Doug Mitchell
 *	Added MT_ISGS, MT_ISEXB
 * 
 * 27-Feb-89	Doug Mitchell at NeXT
 *	Added extended error fields in mtget
 *
 * 23-Feb-89	Doug Mitchell at NeXT
 *	Added NeXT-specific SCSI mt_ops
 *
 * 25-Jan-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Upgraded to 4.3
 *
 ************************************************************************
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)mtio.h	7.1 (Berkeley) 6/4/86
 */

/*
 * Structures and definitions for mag tape io control commands
 */
#ifndef	_MTIO_
#define _MTIO_

/* structure for MTIOCTOP - mag tape op command */
struct	mtop	{
	short	mt_op;		/* operations defined below */
	daddr_t	mt_count;	/* how many of them */
};

/* operations */
#define MTWEOF	0	/* write an end-of-file record */
#define MTFSF	1	/* forward space file */
#define MTBSF	2	/* backward space file */
#define MTFSR	3	/* forward space record */
#define MTBSR	4	/* backward space record */
#define MTREW	5	/* rewind */
#define MTOFFL	6	/* rewind and put the drive offline */
#define MTNOP	7	/* no operation, sets status only */
#define MTCACHE	8	/* enable controller cache */
#define MTNOCACHE 9	/* disable controller cache */
/*
 * Additional NeXT-specific SCSI ops.
 */
#ifdef	NeXT
#define MTRETEN 10      /* retension the tape */
#define MTERASE 11      /* erase the entire tape */
#endif	NeXT

/* structure for MTIOCGET - mag tape get status command */

struct	mtget	{
	short	mt_type;	/* type of magtape device */
/* the following four registers are grossly device dependent */
	u_short	mt_dsreg;	/* ``drive status'' register. SCSI sense byte
				 * 0x02.  */
	u_short	mt_erreg;	/* ``error'' register. SCSI sense byte 0x0C. */
	u_short mt_ext_err0;	/* SCSI sense bytes 0x13..0x14 */
	u_short mt_ext_err1;	/* SCSI sense bytes 0x15..0x16 */
/* end device-dependent registers */
	u_int	mt_resid;	/* residual count. SCSI Info bytes. */
/* the following two are not yet implemented */
	daddr_t	mt_fileno;	/* file number of current position */
	daddr_t	mt_blkno;	/* block number of current position */
/* end not yet implemented */
};

/*
 * Constants for mt_type byte.  These are the same
 * for controllers compatible with the types listed.
 */
#define	MT_ISTS		0x01		/* TS-11 */
#define	MT_ISHT		0x02		/* TM03 Massbus: TE16, TU45, TU77 */
#define	MT_ISTM		0x03		/* TM11/TE10 Unibus */
#define	MT_ISMT		0x04		/* TM78/TU78 Massbus */
#define	MT_ISUT		0x05		/* SI TU-45 emulation on Unibus */
#define	MT_ISCPC	0x06		/* SUN */
#define	MT_ISAR		0x07		/* SUN */
#define	MT_ISTMSCP	0x08		/* DEC TMSCP protocol (TU81, TK50) */
#define MT_ISGS		0x09		/* Generic SCSI Tape */
#define MT_ISEXB	0x0A		/* Exabyte Tape */

/* mag tape io control commands */
#define	MTIOCTOP	_IOW('m', 1, struct mtop)	/* do a mag tape op */
#define	MTIOCGET	_IOR('m', 2, struct mtget)	/* get tape status */
#define MTIOCIEOT	_IO('m',  3)			/* ignore EOT error */
#define MTIOCEEOT	_IO('m',  4)			/* enable EOT error */

#ifndef KERNEL
#define	DEFTAPE	"/dev/rxt0"
#endif

#endif	_MTIO_

