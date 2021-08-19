/*
 * Mach Operating System
 * Copyright (c) 1986 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 25-Oct-88  Gregg Kellogg (gk) at NeXT, Inc.
 *	Added	TBL_MACHFACTOR
 *		TBL_DISKINFO
 *		TBL_CPUINFO
 *		TBL_IOINFO
 *		TBL_DISKINFO
 *		TBL_NETINFO
 *
 * 15-Mar-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added TBL_PROCINFO (MACH only).
 *
 * 15-Apr-87  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added TBL_LOADAVG.
 *	[ V5.1(F8) ]
 *
 *  7-Nov-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added TBL_ARGUMENTS, also for MACH_VM only.
 *
 * 24-Jul-86  David Golub (dbg) at Carnegie-Mellon University
 *	Added TBL_UAREA.  For now it only works under MACH virtual
 *	memory.
 *
 * 24-Jul-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added TBL_INCLUDE_VERSION and TBL_FSPARAM.
 *
 * 30-Aug-85  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added TBL_U_TTYD.
 *	[V1(1)]
 *
 * 30-Mar-83  Mike Accetta (mja) at Carnegie-Mellon University
 *	Created (V3.06h).
 */

#import <sys/dk.h>
#import <machine/table.h>

#define	TBL_TTYLOC		0	/* index by device number */
#define	TBL_U_TTYD		1	/* index by process ID */
#define	TBL_UAREA		2	/* index by process ID */
#define	TBL_LOADAVG		3	/* (no index) */
#define	TBL_INCLUDE_VERSION	4	/* (no index) */
#define	TBL_FSPARAM		5	/* index by device number */
#define	TBL_ARGUMENTS		6	/* index by process ID */
#define	TBL_PROCINFO		10	/* index by proc table slot */
#define	TBL_MACHFACTOR		11	/* index by cpu number */
#define TBL_CPUINFO		12	/* (no index), generic CPU info */
#define TBL_IOINFO		13	/* (no index), generic I/O info */
#define	TBL_DISKINFO		14	/* indexed by dk slot */
#define TBL_NETINFO		15	/* index by network slot */

/*
 * Machine specific table id base
 */
#define TBL_MACHDEP_BASE	0x4000	/* Machine dependent codes start here */

/*
 * Return codes from machine dependent calls
 */
#define TBL_MACHDEP_NONE	0	/* Not handled by machdep code */
#define	TBL_MACHDEP_OKAY	1	/* Handled by machdep code */
#define	TBL_MACHDEP_BAD		-1	/* Bad status from machdep code */

/*
 *  TBL_FSPARAM data layout
 */
struct tbl_fsparam
{
    long tf_used;		/* free fragments */
    long tf_iused;		/* free inodes */
    long tf_size;		/* total fragments */
    long tf_isize;		/* total inodes */
};

/*
 *  TBL_LOADAVG data layout
 *  (used by TBL_MACHFACTOR too)
 */
struct tbl_loadavg
{
    long   tl_avenrun[3];
    int    tl_lscale;		/* 0 scale when floating point */
};

/*
 *	TBL_PROCINFO data layout
 */
#define	PI_COMLEN	19	/* length of command string */
struct tbl_procinfo
{
    int		pi_uid;		/* user ID */
    int		pi_pid;		/* proc ID */
    int		pi_ppid;	/* parent proc ID */
    int		pi_pgrp;	/* proc group ID */
    int		pi_ttyd;	/* controlling terminal number */
    int		pi_status;	/* process status: */
#define	PI_EMPTY	0	    /* no process */
#define	PI_ACTIVE	1	    /* active process */
#define	PI_EXITING	2	    /* exiting */
#define	PI_ZOMBIE	3	    /* zombie */
    int		pi_flag;	/* other random flags */
    char	pi_comm[PI_COMLEN+1];
				/* short command name */
};

/*
 * TBL_CPUINFO data layout
 */
struct tbl_cpuinfo
{
    int		ci_swtch;		/* # context switches */
    int		ci_intr;		/* # interrupts */
    int		ci_syscall;		/* # system calls */
    int		ci_traps;		/* # system traps */
    int		ci_hz;			/* # ticks per second */
    int		ci_phz;			/* profiling hz */
    int		ci_cptime[CPUSTATES];	/* cpu state times */
};

/*
 * TBL_IOINFO data layout
 */
struct tbl_ioinfo
{
    int		io_ttin;	/* # bytes of tty input */
    int		io_ttout;	/* # bytes of tty output */
    int		io_dkbusy;	/* drives active? */
    int		io_ndrive;	/* number disk drives configured */
    int		io_nif;		/* number of network interfaces */
};

/*
 * TBL_DISKINFO data layout (not generically implemented)
 */
struct tbl_diskinfo
{
    int		di_time;	/* ticks drive active */
    int		di_seek;	/* # drive seeks */
    int		di_xfer;	/* # drive transfers */
    int		di_wds;		/* # sectors transfered */
    int		di_bps;		/* drive transfer rate (bytes per second) */
    char	di_name[8];	/* drive name */
};

/*
 * TBL_NETINFO data layout
 */
struct tbl_netinfo
{
    int		ni_ipackets;	/* # input packets */
    int		ni_ierrors;	/* # input errors */
    int		ni_opackets;	/* # output packets */
    int		ni_oerrors;	/* # output errors */
    int		ni_collisions;	/* # # collisions on csma if's */
    char	ni_name[8];	/* interface name */
};

#ifdef KERNEL
/*
 * Machine specific procedure prototypes.
 */
int machine_table(int id, int index, caddr_t addr, int nel, u_int lel, int set);
int machine_table_setokay(int id);
#endif KERNEL
