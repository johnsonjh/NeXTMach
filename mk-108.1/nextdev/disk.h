/*	@(#)disk.h	1.0	08/29/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 20-Jul-90  Doug Mitchell
 *	Added DKIOCSFORMAT, DKIOCGFORMAT
 *
 * 16-Apr-90  Doug Mitchell at NeXT
 *	Added DKIOCPANELPRT.
 *
 * 25-Mar-90  John Seamons (jks) at NeXT
 *	Removed obsolete DKIOCNOTIFY and DKIOCINSERT.
 *
 * 23-Mar-90  Doug Mitchell
 *	Added DKIOCEJECT.
 * 
 * 14-Feb-90  Doug Mitchell at NeXT
 *	Added DKIOCMNOTIFY.
 *
 * 16-Mar-88  John Seamons (jks) at NeXT
 *	Cleaned up to support standard disk label definitions.
 *
 * 24-Feb-88  Mike DeMoney (mike) at NeXT
 *	Added defines for dl_bootfile and dl_boot0_blkno.
 *	Reduced NBAD to allow for these entries in disktab.
 *
 * 29-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */

#import	"machine/vm_types.h"
#import	"machine/boolean.h"
#import <sys/types.h>
#import <sys/ioctl.h>
#import <sys/time.h>
#import <sys/disktab.h>

struct disk_label {
#define	DL_V1		0x4e655854	/* version #1: "NeXT" */
#define	DL_V2		0x646c5632	/* version #2: "dlV2" */
#define	DL_V3		0x646c5633	/* version #3: "dlV3" */
#define	DL_VERSION	DL_V3		/* default version */
	int	dl_version;		/* label version number */
	int	dl_label_blkno;		/* block # where this label is */
	int	dl_size;		/* size of media area (sectors) */
#define	MAXLBLLEN	24
	char	dl_label[MAXLBLLEN];	/* media label */
	u_int	dl_flags;		/* flags */
#define	DL_UNINIT	0x80000000	/* label is uninitialized */
	u_int	dl_tag;			/* volume tag */
	struct	disktab dl_dt;		/* common info in disktab */
#define	dl_name		dl_dt.d_name
#define	dl_type		dl_dt.d_type
#define dl_part		dl_dt.d_partitions
#define	dl_front	dl_dt.d_front
#define	dl_back		dl_dt.d_back
#define	dl_ngroups	dl_dt.d_ngroups
#define	dl_ag_size	dl_dt.d_ag_size
#define	dl_ag_alts	dl_dt.d_ag_alts
#define	dl_ag_off	dl_dt.d_ag_off
#define	dl_secsize	dl_dt.d_secsize
#define	dl_ncyl		dl_dt.d_ncylinders
#define	dl_nsect	dl_dt.d_nsectors
#define	dl_ntrack	dl_dt.d_ntracks
#define	dl_rpm		dl_dt.d_rpm
#define	dl_bootfile	dl_dt.d_bootfile
#define	dl_boot0_blkno	dl_dt.d_boot0_blkno
#define	dl_hostname	dl_dt.d_hostname
#define	dl_rootpartition dl_dt.d_rootpartition
#define	dl_rwpartition	dl_dt.d_rwpartition

	/*
	 *  if dl_version >= DL_V3 then the bad block table is relocated
	 *  to a structure separate from the disk label.
	 */
	union {
		u_short	DL_v3_checksum;
#define	NBAD	1670			/* sized to make label ~= 8KB */
		int	DL_bad[NBAD];	/* block number that is bad */
	} dl_un;
#define	dl_v3_checksum	dl_un.DL_v3_checksum
#define	dl_bad		dl_un.DL_bad
	u_short	dl_checksum;		/* ones complement checksum */
	
	/* add things here so dl_checksum stays in a fixed place */
};

#define	BAD_BLK_OFF	4		/* offset of bad blk tbl from label */
struct bad_block {			/* bad block table, sized to be 12KB */
#define	NBAD_BLK	(12 * 1024 / sizeof (int))
	int	bad_blk[NBAD_BLK];
};

/* sector bitmap states (2 bits per sector) */
#define	SB_UNTESTED	0	/* must be zero */
#define	SB_BAD		1
#define	SB_WRITTEN	2
#define	SB_ERASED	3

struct disk_req {
	int	dr_bcount;		/* byte count for data transfers */
	caddr_t	dr_addr;		/* memory addr for data transfers */
	struct	timeval dr_exec_time;	/* execution time of operation */

	/* interpretation of cmdblk and errblk is driver specific */
#define	DR_CMDSIZE	32
#define	DR_ERRSIZE	32
	char	dr_cmdblk[DR_CMDSIZE];
	char	dr_errblk[DR_ERRSIZE];
};

struct drive_info {			/* info about drive hardware */
	char	di_name[MAXDNMLEN];	/* drive type name */
#define	NLABELS	4
	int	di_label_blkno[NLABELS];/* label loc'ns in DEVICE SECTORS */
	int	di_devblklen;		/* device sector size */
	int	di_maxbcount;		/* max bytes per transfer request */
};

struct	disk_stats {
	int	s_ecccnt;	/* avg ECC corrections per sector */
	int	s_maxecc;	/* max ECC corrections observed */

	/* interpretation of s_stats is driver specific */
#define	DS_STATSIZE	32
	char	s_stats[DS_STATSIZE];
};

struct sdc_wire {
	vm_offset_t	start, end;
	boolean_t	new_pageable;
};

#define	DKIOCGLABEL	_IO('d', 0)			/* read label */
#define	DKIOCSLABEL	_IO('d', 1)			/* write label */
#define	DKIOCGBITMAP	_IO('d', 2)			/* read bitmap */
#define	DKIOCSBITMAP	_IO('d', 3)			/* write bitmap */
#define	DKIOCREQ	_IOWR('d', 4, struct disk_req)	/* cmd request */
#define	DKIOCINFO	_IOR('d', 5, struct drive_info)	/* get drive info */
#define	DKIOCZSTATS	_IO('d',7)			/* zero statistics */
#define	DKIOCGSTATS	_IO('d', 8)			/* get statistics */
#define	DKIOCRESET	_IO('d', 9)			/* reset disk */
#define	DKIOCGFLAGS	_IOR('d', 11, int)		/* get driver flags */
#define	DKIOCSFLAGS	_IOW('d', 12, int)		/* set driver flags */
#define	DKIOCSDCWIRE	_IOW('d', 14, struct sdc_wire)	/* sdc wire memory */
#define	DKIOCSDCLOCK	_IO('d', 15)			/* sdc lock */
#define	DKIOCSDCUNLOCK	_IO('d', 16)			/* sdc unlock */
#define	DKIOCGFREEVOL	_IOR('d', 17, int)		/* get free volume # */
#define	DKIOCGBBT	_IO('d', 18)			/* read bad blk tbl */
#define	DKIOCSBBT	_IO('d', 19)			/* write bad blk tbl */
#define	DKIOCMNOTIFY	_IOW('d', 20, int)		/* message on insert */
#define	DKIOCEJECT	_IO('d', 21)			/* eject disk */
#define	DKIOCPANELPRT	_IOW('d', 22, int)		/* register Panel 
							 * Request port */
#define DKIOCSFORMAT	_IOW('d', 23, int)		/* set 'Formatted' flag 
							 */
#define DKIOCGFORMAT	_IOR('d', 23, int)		/* get 'Formatted' flag 
							 */



