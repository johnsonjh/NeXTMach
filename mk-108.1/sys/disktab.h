/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)disktab.h	5.2 (Berkeley) 10/1/85
 * HISTORY:
 * 16-Mar-88  John Seamons (jks) at NeXT
 *	Cleaned up to support standard disk label definitions.
 *
 * 24-Feb-88  Mike DeMoney (mike) at NeXT
 *	Added d_boot0_blkno to indicate logical block number
 *	of "block 0" boot.  This blkno is in d_secsize sectors.
 *	Added d_bootfile to indicate the default operating system
 *	image to be booted by the blk 0 boot.
 *	Changed d_name and d_type to be char arrays rather than ptrs
 *	so they are part of label.  This limits length of info in
 *	/etc/disktab, sorry.
 */

/*
 * Disk description table, see disktab(5)
 */
#ifndef KERNEL
#define	DISKTAB		"/etc/disktab"
#endif	!KERNEL

struct	disktab {
#define	MAXDNMLEN	24
	char	d_name[MAXDNMLEN];	/* drive name */
#define	MAXTYPLEN	24
	char	d_type[MAXTYPLEN];	/* drive type */
	int	d_secsize;		/* sector size in bytes */
	int	d_ntracks;		/* # tracks/cylinder */
	int	d_nsectors;		/* # sectors/track */
	int	d_ncylinders;		/* # cylinders */
	int	d_rpm;			/* revolutions/minute */
	short	d_front;		/* size of front porch (sectors) */
	short	d_back;			/* size of back porch (sectors) */
	short	d_ngroups;		/* number of alt groups */
	short	d_ag_size;		/* alt group size (sectors) */
	short	d_ag_alts;		/* alternate sectors / alt group */
	short	d_ag_off;		/* sector offset to first alternate */
#define	NBOOTS	2
	int	d_boot0_blkno[NBOOTS];	/* "blk 0" boot locations */
#define	MAXBFLEN 24
	char	d_bootfile[MAXBFLEN];	/* default bootfile */
#define	MAXHNLEN 32
	char	d_hostname[MAXHNLEN];	/* host name */
	char	d_rootpartition;	/* root partition e.g. 'a' */
	char	d_rwpartition;		/* r/w partition e.g. 'b' */
#define	NPART	8
	struct	partition {
		int	p_base;		/* base sector# of partition */
		int	p_size;		/* #sectors in partition */
		short	p_bsize;	/* block size in bytes */
		short	p_fsize;	/* frag size in bytes */
		char	p_opt;		/* 's'pace/'t'ime optimization pref */
		short	p_cpg;		/* cylinders per group */
		short	p_density;	/* bytes per inode density */
		char	p_minfree;	/* minfree (%) */
		char	p_newfs;	/* run newfs during init */
#define	MAXMPTLEN	16
		char	p_mountpt[MAXMPTLEN];/* mount point */
		char	p_automnt;	/* auto-mount when inserted */
#define	MAXFSTLEN	8
		char	p_type[MAXFSTLEN];/* file system type */
	} d_partitions[NPART];
};

#ifndef KERNEL
struct	disktab *getdiskbyname(), *getdiskbydev();
#endif	!KERNEL
