#if	CMU
/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1986 Carnegie-Mellon University
 *  
 * This software was developed by the Mach operating system
 * project at Carnegie-Mellon University's Department of Computer
 * Science. Software contributors as of May 1986 include Mike Accetta, 
 * Robert Baron, William Bolosky, Jonathan Chew, David Golub, 
 * Glenn Marcy, Richard Rashid, Avie Tevanian and Michael Young. 
 * 
 * Some software in these files are derived from sources other
 * than CMU.  Previous copyright and other source notices are
 * preserved below and permission to use such software is
 * dependent on licenses from those institutions.
 * 
 * Permission to use the CMU portion of this software for 
 * any non-commercial research and development purpose is
 * granted with the understanding that appropriate credit
 * will be given to CMU, the Mach project and its authors.
 * The Mach project would appreciate being notified of any
 * modifications and of redistribution of this software so that
 * bug fixes and enhancements may be distributed to users.
 *
 * All other rights are reserved to Carnegie-Mellon University.
 **********************************************************************
 */ 
 
#endif	CMU
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)saio.h	7.1 (Berkeley) 6/5/86
 */
 
#import <sys/param.h>
#import <sys/time.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <ufs/fs.h>
#import <nextdev/dma.h>

/*
 * Header file for standalone package
 */

/*
 * Io block: includes an
 * inode, cells for the use of seek, etc,
 * and a buffer.
 */
struct	iob {
	int	i_flgs;		/* see F_ below */
	struct	inode i_ino;	/* inode, if file */
	int	i_ctrl;		/* device ctrlr number */
	int	i_unit;		/* device unit number */
	int	i_part;		/* device partition number */
	int	i_secsize;	/* file system block size */
	off_t	i_offset;	/* seek offset in file */
	daddr_t	i_bn;		/* 1st block # of next read */
	char	*i_ma;		/* memory address of i/o buffer */
	int	i_cc;		/* character count of transfer */
	int	i_error;	/* error # return */
	int	i_errcnt;	/* error count for driver retries */
	int	i_errblk;	/* block # in error for error reporting */
	char	*i_filename;	/* filename passed in at open time */
	char	*i_buf;		/* floating i/o buffer */
	char	i_buffer[MAXBSIZE+DMA_ENDALIGNMENT-1];/* i/o buffer */
	union {
		struct fs ui_fs;	/* file system super block info */
		char dummy[SBSIZE];
	} i_un;
};
#define i_fs i_un.ui_fs
#define NULL 0

#define F_READ		0x1	/* file opened for reading */
#define F_WRITE		0x2	/* file opened for writing */
#define F_ALLOC		0x4	/* buffer allocated */
#define F_FILE		0x8	/* file instead of device */
#define F_NBSF		0x10	/* no bad sector forwarding */
#define F_SSI		0x40	/* set skip sector inhibit */
/* io types */
#define	F_RDDATA	0x0100	/* read data */
#define	F_WRDATA	0x0200	/* write data */
#define F_HDR		0x0400	/* include header on next i/o */
#define F_CHECK		0x0800	/* perform check of data read/write */
#define F_HCHECK	0x1000	/* perform check of header and data */

#define	F_TYPEMASK	0xff00

/*
 * Device switch.
 */
struct devsw {
	char	*dv_name;
	int	(*dv_strategy)();
	int	(*dv_open)();
	int	(*dv_close)();
	int	(*dv_ioctl)();
	int	dv_structure;
#define	DS_FILE		0	/* device is file structured */
#define	DS_PACKET	1	/* device is packet structured */
};

struct devsw devsw[];

/*
 * Request codes. Must be the same a F_XXX above
 */
#define	READ	1
#define	WRITE	2

#define	NBUFS	4

char	*b[NBUFS];
char	buffers[NBUFS][MAXBSIZE+DMA_ENDALIGNMENT-1];
daddr_t	blknos[NBUFS];

#define	NFILES	1
struct	iob iob[NFILES];

extern	int errno;	/* just like unix */

/* error codes */
#undef	EBAD
#define	EBAD	1	/* bad file descriptor */
#undef	EOFFSET
#define	EOFFSET	2	/* relative seek not supported */
#undef	EDEV
#define	EDEV	3	/* improper device specification on open */
#undef	ENIO
#define	ENIO	4	/* unknown device specified */
#undef	EUNIT
#define	EUNIT	5	/* improper unit specification */
#undef	ESRCHF
#define	ESRCHF	6	/* directory search for file failed */
#undef	EDIO
#define	EDIO	7	/* generic error */
#undef	ECMD
#define	ECMD	10	/* undefined driver command */
#undef	EBSE
#define	EBSE	11	/* bad sector error */
#undef	EWCK
#define	EWCK	12	/* write check error */
#undef	EECC
#define	EECC	13	/* uncorrectable ecc error */
#undef	EHER
#define	EHER	14	/* hard error */


