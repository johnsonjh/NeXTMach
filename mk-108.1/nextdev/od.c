/*	@(#)od.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 * HISTORY
 * 22-Aug-90  Doug Mitchell (dmitch) at NeXT
 *	Added recovery from DMA hang after reading blank label sectors.
 *
 * 25-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Moved declaration of disr_shadow from od.c to next_init.c (config).
 *
 * 17-Jul-90  John Seamons (jks) at NeXT
 *	USE_RJ: Remove use of relative jump (RJ) command since it appears to be
 *	flakey again now that we are using a higher speed CPU (results in excessive
 *	spin up/downs).
 *
 * 13-Jul-90  John Seamons (jks) at NeXT
 *	In od_validate_label() check for zero version number since an all zero label
 *	will have a valid checksum.  Reworked logic for writing uninitialized labels.
 *
 * 20-Jun-90  John Seamons (jks) at NeXT
 *	Allow eject if "any command" mode set in ROM.
 *
 * 30-May-90  John Seamons (jks) at NeXT
 *	Use OMD_EJECT instead of OMD_EJECT_NOW in DKIOCEJECT ioctl so
 *	bitmap information gets updated properly!  This started to cause problems
 *	when /etc/disk was changed to use DKIOCEJECT instead of sending OMD_EJECT via
 *	the DKIOCREQ mechanism.
 *
 * 18-May-90  Avadis Tevanian (avie) at NeXT
 *	Changed to use sensible priorities (higher numbers -> higher pri).
 *	Note: We need pneumonics for modules like this (which *should* be using
 *	a formal API).
 *
 * 25-Mar-90  John Seamons (jks) at NeXT
 *	Removed obsolete DKIOCNOTIFY and DKIOCINSERT.
 *
 * 24-Mar-90  Doug Mitchell at NeXT
 *	Implemented disk inertion notification via vol_notify_dev().
 *	Added DKIOCEJECT.
 *
 * 17-Mar-90  John Seamons (jks) at NeXT
 *	Always set OMD_25_MHZ bit in drive initr register.
 *	Used to be set when CPU	clock speed >= 25 MHz
 *	(which is now always true).  This test was broken due to a compiler
 *	bug.  Also, just set bits in r_cntrl1 instead of or'ing them in.
 *
 * 16-Mar-90  John Seamons (jks) at NeXT
 *	Improper abort of I/O requests if 'n' typed to disk insert window
 *	was causing the abort code to loop if there were I/O requests pending.
 *
 * 16-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Changes for new scheduler:
 *		Changed csw_check() to csw_needed(thread, processor)
 *		Changed od_lock to get threads from processor sets, rather
 *		than all_threads.
 *
 *  8-Mar-90  John Seamons (jks) at NeXT
 *	If a disk is uninitialized (no labels can be read) then only write a
 *	dummy label with the DL_UNINIT bit set to the last label location in
 *	case the disk actually has good labels that can't be read because of
 *	excessive dust contamination.  Note that a dummy label must be written
 *	so an uninitialized disk can be reinserted in a single drive system.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Keep track of "owner" (uid) at odopen() time and allow them, in 
 *	addition to root, to eject disk via DKIOCREQ ioctl.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	In od_cmd() timeout facility isn't available during autoconf time, so 
 *	keep a manual timeout interval instead.
 *
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Don't spin in od_sect_to waiting for sector header to show up.
 *
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 *
 */

#define	USE_RJ		0
#define	SCAN_RETRY	0
 
/*
 *	FIXME:
 *	- DISABLED HOS AND CUR_HEAD BECAUSE OF CANON TEST WRITE BUG!
 *	- schedule od_timer events per volume? (for multiple drive systems)
 *	- find some way to improve pageout write performance
 *	- don't spinup just to eject
 *	- get file system to pre-erase blocks
 *	- keep statistics
 *	- multiple mounts (virtual volumes)
 *	- look at spinup issues again (don't spinup at boot?)
 *	- poll "other" units for ATTN & insert in od_timeout()
 *
 *	LATER:
 *	- consider overlapping next seek while read DMA completes
 *	- write oddump() & odsize()
 *	- start write DMA early to precompute ECC?
 *	- for errors, look at DMA count to see what sectors actually
 *	  worked.  Report only failing sector number.
 *	- how to sense standalone ATTNs?
 *	- what about good blks (data & alts) going bad?
 *	- check async attn window again
 *	- add to od_async_attn()?
 *	- daemons: bad blk
 *	- consider cmp instead of verify as an option
 *	- configure to work with ldd?
 *	- overlap seeks across multiple units?
 */

#import <od.h>
#if NOD > 0
#import <mach_nbc.h>

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/buf.h>
#import	<sys/disksort.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <kern/kern_port.h>
#import <kern/task.h>
#import <kern/thread.h>
#import <kern/processor.h>
#import <sys/user.h>
#import <sys/callout.h>
#import <sys/dk.h>
#import <kern/xpr.h>
#import <sys/signal.h>
#import <sys/systm.h>
#import <kern/sched.h>
#import <sys/reboot.h>
#import <sys/fcntl.h>
#import <ufs/fs.h>
#import <ufs/mount.h>
#import <vm/vm_kern.h>
#import <vm/vm_page.h>
#import <next/autoconf.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/pmap.h>
#import <next/printf.h>
#import <next/event_meter.h>
#import	<next/scr.h>
#import <nextdev/odreg.h>
#import <nextdev/odvar.h>
#import <nextdev/dma.h>
#import <nextdev/disk.h>
#import <nextdev/kmreg.h>
#import <nextdev/canon.h>
#import <nextdev/insertmsg.h>
#import <nextdev/voldev.h>
#import <machine/spl.h>
#import <nextdev/sf_access.h>

/*
 *	A number of "controllers" (NOC) can control NUNIT "units" per
 *	controller.  A "drive" (NOD) is an arbitrary controller/unit pair.
 *	NOD and NOC are determined from the kernel configuration file.
 *	In the NeXT implementation, a "controller" is the disk chip and a
 *	"unit" is a Canon disk drive.  A volume is a physical media cartridge
 *	that can be inserted into any drive at any time.  There can be up to
 *	NVOL potential active volumes in the system, only NOD of which can be
 *	inserted in a drive simultaneously.  The system will request and eject
 *	volumes (swapping) as required.
 *	Each volume has a number of logical partitions (NPART).
 *	The minor device number encodes the volume and partition to use:
 *
 *	minor(dev) = vvvvvppp
 *	v = volume 0-31
 *	p = partition 0-7
 *
 *	The last volume (NVOL) is interpreted as a reference to the driver
 *	itself and is used to gain a file descriptor for issuing ioctls()
 *	without actually causing a volume to be requested.
 */
#define	NUNIT	2
#define	NVOL	31
#define	OD_VOL(dev)		(minor(dev) >> 3)
#define OD_PART(dev)		(minor(dev) & 7)
#define	OD_DEV(vol, part)	(((vol) << 3) | (part))

#define OD_SFA_ARB	1	/* use sfa_arbitrate() */

#ifdef	OD_SFA_ARB
static busdone(struct bus_ctrl *bc);
#endif	OD_SFA_ARB

/* autoconfiguration */
int	odprobe(), odslave(), odattach(), od_go(),
	od_done(), odpoll(), odintr(), odopen();
void	od_dma_intr(), od_dmaintr();
struct	bus_ctrl *odcinfo[NOC];
struct	bus_device *oddinfo[NOD];
struct	bus_driver ocdriver = {
	odprobe, odslave, odattach, od_go, od_done, odintr, 0,
	sizeof (struct od_regs), "od", oddinfo, "odc", odcinfo
	/* FIXME: flags? */
};

#define	NDRIVES		1		/* number of supported drive types */
struct drive_info od_drive_info[NDRIVES] = {
#define	DRIVE_TYPE_OMD1	0
	{				/* OMD-1 drive */
		"Canon OMD-1",			/* disk name */
		{ 0*16, 15430*16 , -1, -1 },	/* label locations */
		1024,				/* device sector size */
		256 * 1024,			/* max xfer bcount */
	},
};

/* additional info not kept in standard device_info struct */
struct	more_info {			/* info about media */
	int	mi_reserved_cyls;	/* # cyls to skip at front of media */
	short	mi_nsect;		/* # sectors / track */
	u_int	mi_rv_delay;		/* delay in reporting read/verify */
	u_int	mi_expire;		/* CUR_POS info expires after this time */
	u_int	mi_expire_sh;		/* expire time if also switching head */
	u_int	mi_expire_sh2;		/* expire time if also switching head */
	u_int	mi_expire_jmp;		/* expire time if jumping too far */
	u_int	mi_expire_max;
} od_more_info[NDRIVES] = {
	{ 4149, 16, 2500, 2800, 2800, 2800, 4750, 7500 },
};

u_int	mi_rv_delay = 2500;
u_int	mi_expire = 2800;
u_int	mi_expire_sh = 2800;
u_int	mi_expire_sh2 = 2800;
u_int	mi_expire_jmp = 4750;
u_int	mi_expire_max = 7500;

/*
 * Thectrl and drive structures mirror bus_ctrl and bus_device
 * but contain more specific information for the driver.
 */
struct ctrl {				/* per controller info */
	struct	dma_chan c_dma_chan;	/* DMA info */
	dma_list_t c_dma_list;		/* DMA header list */
	volatile struct	od_regs *c_od_regs;	/* controller registers */
	caddr_t	c_va;			/* virt addr of buffer */
	struct pmap *c_pmap;		/* physical map of buffer */
	daddr_t	c_bno;			/* requested block number */
	volatile int	c_flags;
#define	CF_CUR_POS	0x80000000	/* current head position known */
#define	CF_ISSUE_D_CMD	0x40000000	/* issue drive command */
#define	CF_ISSUE_F_CMD	0x20000000	/* issue formatter command */
#define	CF_DMAINT_EXP	0x10000000	/* DMA interrupt expected */
#define	CF_DEVINT_EXP	0x08000000	/* device interrupt expected */
#define	CF_ATTN_INPROG	0x04000000	/* attn in progress, no new cmds */
#define	CF_ATTN_WAKEUP	0x02000000	/* wake me up when attn finishes */
#define	CF_ATTN_ASYNC	0x01000000	/* attn was asynchronous */
#define	CF_ERR_INPROG	0x00800000	/* error recovery in progress */
#define	CF_ONLINE	0x00400000	/* controller is online */
#define	CF_SOFT_TIMEOUT	0x00200000	/* soft timeout has occured */
#define	CF_NEED_DMA	0x00100000	/* DMA needed for this operation */
#define	CF_PHYS		0x00080000	/* physical disk I/O */
#define	CF_TESTING	0x00040000	/* testing in progress */
#define	CF_ASYNC_INPROG	0x00020000	/* async processing in progress */
#define	CF_DMA_ERROR	0x00010000	/* DMA error has occured */
#define	CF_CANT_SLEEP	0x00008000	/* interrupts can't sleep */
#define	CF_ERR_VALID	0x00004000	/* c_err is valid */
#define	CF_50_50_SEEK	0x00002000	/* first try at a 50/50 seek chance */
#define	CF_READ_GATE	0x00001000	/* read gate error recovery active */
#define	CF_ECC_SCAN	0x00000800	/* scanning for excessive ECCs */
#define	CF_FAIL_IO	0x00000400	/* fail this I/O operation */
#if	SCAN_RETRY
#define	CF_UNTESTED	0x00000001	/* use maxecc for untested sectors */
#endif	SCAN_RETRY
	caddr_t	c_tbuf;			/* aligned pointer to test buffer */
	caddr_t	c_cbuf;			/* aligned pointer to copy buffer */
	int	c_ccount;		/* copy count */
	struct	pmap *c_cmap;		/* copy map */
	short	c_track;		/* requested track */
	short	c_seek_track;		/* seek track (may not be same) */
	int	c_blkno;		/* block number to transfer */
	int	c_test_blkno;		/* block number during testing */
	int	c_pblk;			/* physical block number */
#if	SCAN_RETRY
	int	c_err_pblk;		/* block in error during ECC scan */
#endif	SCAN_RETRY
	u_int	c_curpos_time;		/* time that CUR_POS was captured */
	int	c_nsect;		/* number of sectors to xfer */
	int	c_save_nsect;		/* saved copy during ECC scan */
	int	c_xfer;			/* number actually xfer'd */
	u_short	c_cmd;			/* transfer command */
	u_short	c_rds;			/* drive status */
	u_short	c_res;			/* extended status */
	u_short	c_rhs;			/* hardware status */
	short	c_offset;		/* offset into status bitmap */
	char	c_shift;		/* shift into status bitmap */
	short	c_cur_track;		/* current track under head */
	char	c_cur_sect;		/* current sector under head */
	char	c_err;			/* error code */
	u_short	c_dcmd;			/* current drive command */
	u_short	c_last_dcmd;		/* last issued drive command */
	u_char	c_fcmd;			/* current formatter command */
	char	c_test_pass;		/* pass number during testing */
	u_char	c_retry;		/* number of retries attempted */
	u_char	c_rtz;			/* number of RTZs performed */
	char	c_timeout;		/* ticks since command started */
	char	c_update;		/* label/bitmap update interval */
	u_short	c_head;			/* requested head */
	char	c_sect;			/* requested sector */
	char	c_before;		/* # sectors to write before data */
	char	c_after;		/* # sectors to write after data */
	char	c_fsm;			/* transfer finite state machine */
#define	FSM_START	1		/* start a transfer */
#define	FSM_SEEK	2
#define	FSM_WRITE	3
#define	FSM_VERIFY	4
#define	FSM_WORKED	5
#define	FSM_FAILED	6
#define	FSM_EJECT	7
#define	FSM_ERR_DONE	8
#define	FSM_ATTN_RDS	9
#define	FSM_ATTN_RES	10
#define	FSM_ATTN_RHS	11
#define	FSM_ATTN_RID	12
#define	FSM_INSERT_RID	13
#define	FSM_INSERT_STM	14
#define	FSM_TEST_WRITE	15
#define	FSM_TEST_DONE	16
#define	FSM_TEST_ERASE	17
#define	FSM_WRITE_RETRY	18
#define	FSM_RESPIN_RST	19
#define	FSM_RESPIN_STM	20
	char	c_next_fsm;
	char	c_retry_fsm;
	char	c_fsm_test;
	char	c_err_fsm_save;
	char	c_respin_fsm_save;
	char	c_attn_fsm_save;
	char	c_attn_next_fsm_save;
	char	c_initr;		/* software copy of init reg */
	u_char	c_read_gate;		/* current value of read gate */
	u_char	c_scan_rewrite;		/* count of rewrites during ECC scan */
	char	c_drive;		/* current drive # */
#ifdef	OD_SFA_ARB
	sf_access_head_t c_sfahp;	/* scsi/floppy access struct */
	struct sf_access_device c_sfad;	/* for enqueueing requests */
#endif	OD_SFA_ARB
} od_ctrl[NOC];

struct volume {				/* per volume info */
	ds_queue_t v_queue;		/* buffer queue */
	struct	buf v_rbuf;		/* buffer for raw I/O */
	struct	buf v_cbuf;		/* buffer for special commands */
	struct	disk_label *v_label;	/* aligned pointer to disk label */
	struct	bad_block *v_bad_block;	/* aligned pointer to bad blk tbl */
	struct	drive_info *v_di;	/* drive info */
	struct	more_info *v_mi;	/* drive info */
	int	v_base;			/* base sector number of media area */
	int	v_size;			/* size of media area (sectors) */
	int	*v_bitmap;		/* half-track state bitmap */
	int	v_bitmap_bytes;		/* size in bytes */
	uid_t	v_owner;		/* user-id of volume owner at open time */
	u_short	v_refcnt;		/* reference count */
	u_short	v_drive;		/* drive number attached to */
	u_short	v_cmd;			/* special drive command */
	u_char	v_retry;		/* user specified # of retries */
	u_char	v_rtz;			/* user specified # of restores */
	u_short	v_flags;
#define	VF_BUSY			0x8000	/* volume struct in use */
#define	VF_ONLINE		0x4000	/* volume has label */
#define	VF_INSERTED		0x2000	/* volume inserted in drive */
#define	VF_UPDATE		0x1000	/* update label & bitmap on volume */
#define	VF_EJECT		0x0800	/* eject after update */
#define	VF_LOCK			0x0400	/* locked for real time I/O only */
#define	VF_LOCK_OUT		0x0200	/* non-RT requests locked out */
#define	VF_SPEC_RETRY		0x0100	/* user specified retry/rtz values */
#define	VF_IGNORE_BITMAP	0x0080	/* ignore bitmap information */
#define	VF_WANTED		0x0040	/* volume wants to be inserted */
#define	VF_SPINUP		0x0020	/* volume is spinning up */
#define	VF_SPINUP_WANTED	0x0010	/* wakeup after spinup complete */
#define	VF_MISSING		0x0008	/* volume is missing */
#define	VF_WP			0x0004	/* volume is write protected */
} od_vol[NVOL];

struct drive {
	struct	timeval d_lru;		/* time disk last used */
	struct	volume *d_vol;		/* volume currently inserted */
	struct	ctrl *d_ctrl;		/* controller info */
	struct	bus_ctrl *d_bc;		/* bus controller */
	struct	bus_device *d_bd;	/* bus device */
	u_short	d_flags;
#define	DF_ONLINE		0x8000	/* drive is online */
#define	DF_SPINNING		0x4000	/* disk is inserted */
#define	DF_SPIRALING		0x2000	/* drive is spiraling */
#define	DF_EXISTS		0x1000	/* drive exists */
#define	DF_TRY_ATTACH		0x0800	/* try and attach drive */
#define	DF_EJECTING		0x0400	/* eject in progress */
#define	DF_ATTACH		0x0200	/* drive is being attached */
	u_short	d_cur_head;		/* current head mode */
	char	d_drive_type;		/* type of drive */
	char	d_last_hos;		/* last high order seek addr set */
	char	d_spiral_time;		/* idle time spent spiraling */
} od_drive[NOD];

/* flags */
#define	SPIN		0x01
#define	DONT_SPIN	0x02
#define	INTR		0x04
#define	DONT_INTR	0x08

/* return codes */
#define	OD_BUSY		1
#define	OD_DONE		0
#define	OD_ERR		-1

/* error message filter */
#define	EF_ALWAYS	9
#define	EF_FAILED	8
#define	EF_RESPIN	7
#define	EF_RECOVER	6
#define	EF_REMAP	5
#define	EF_FIXUP	4
#define	EF_RESCAN	3
#define	EF_RTZ		2
#define	EF_RETRY	1
#define	EF_NOTIFY	0
int	od_errmsg_filter = EF_RECOVER;		/* patchable */
char *od_errtype[] = {
	"notify", "retry", "restore", "rescan", "fixup",
	"remap", "recover", "re-spin", "failed", "failed"
};

/* test parameters */
int	od_pattern[] = {	/* FIXME: additional patterns? */
	0x00000000,
	0xffffffff,
	0x49249249,		/* most critical linear density */
/*	0x33333333,		/* greatest laser duty cycle FIXME use? */
};
int	od_test_passes = sizeof od_pattern / sizeof od_pattern[0];

/* flag strategy info */
u_char	od_flgstr[OMD_NFLAG] = { 0x05, 0x00, 0x00, 0x03, 0x04, 0x05, 0x03 };
int	od_frmr = OMD_WRITE_GATE_NOM | OMD_READ_GATE_NOM;

/* statistics */
struct disk_stats od_stats;

/* kernel thread flags */
#define	KT_STOP		-3
#define	KT_DONE		-2
#define	KT_BUSY		-1
#define	KT_UNSCHED	0
#define	KT_SCHED	1

extern u_char disr_shadow;		/* shadow of 2 lsb's of disr; fd, od,
					 * and sd all share this */
/* constants */
#define	NBUF_FREE	2		/* # buffers free before update */
#define	RUNOUT_NOM	(hz * 1/2)	/* nom 500 msec runout to use volume */
#define	RUNOUT_EJECT	(hz * 20)	/* longer runout after root eject */
#define	NINSERT		4
#define	MAX_BITMAP	(64*1024)	/* max size of a bitmap */
#define	MAXBUF		8192		/* test & copy buffer size */
#define	PRI_OMD		PRIBIO
#define	TIMER_INTERVAL	hz		/* once a second */
#define	REG_TIMEOUT	6		/* max drive delay is 2.8 sec */
#define	LONG_TIMEOUT	16		/* for start/stop & load/unload */
#define	RESET_TIMEOUT	10		/* time for a hard reset */
#define	UPDATE_TIMEOUT	30		/* label/bitmap update interval */
#define	IDLE_TIMEOUT	10		/* idle time before daemon is run */
#define	SPIRAL_TIMEOUT	10		/* spiraling turned off after this */
#define	ALIGN(type, addr) \
	((type)(((unsigned)(addr)+DMA_ENDALIGNMENT-1)&~(DMA_ENDALIGNMENT-1)))
#define	SET_BITMAP(p) { \
	int t = v->v_bitmap[c->c_offset]; \
	if ((t & (3 << c->c_shift)) != ((p) << c->c_shift)) { \
		v->v_flags |= VF_UPDATE; \
		if (od_update_time == KT_UNSCHED) \
			od_update_time = KT_SCHED; \
		t &= ~(3 << c->c_shift); \
		v->v_bitmap[c->c_offset] = t | ((p) << c->c_shift); \
	} \
	}

/* misc globals */
caddr_t	od_rathole;
int	od_id_r = OMD_ID_1234;		/* # IDs to match on read */
int	od_id_v = OMD_ID_34;		/* # IDs to match on verify */
int	od_id_o = OMD_ID_34;		/* # IDs to match on other cmds */
thread_t od_try_th, od_req_th;
short	od_runout, od_runout_time;
u_short	od_lock_pid;
int	od_blk_major;
int	od_raw_major;
int	od_label_alloc(), od_try_attach();
struct	disk_label *od_label, *od_readlabel;
struct	bad_block *od_bad_block;
int	*od_bitmap;
int	od_spinup;			/* set when spinup in progress */
struct	volume *od_specific;
int	od_empty;
int	od_spl;
int	od_write_retry = 9;
int	od_maxecc = 12;			/* max soft ECC corrections/sector */
int	od_maxecc_l1 = 13;
int	od_maxecc_h1 = 14;
int	od_maxecc_l2 = 30;
int	od_maxecc_h2 = 37;
int	od_update(), od_requested, od_request(), od_update_thread();
#if	USE_RJ
int	_rj_fail, _off, _lastmode, _dcmd;
u_int	_lastexp;
#endif	USE_RJ
u_int	od_lastexp;			/* last expire time captured */
int	od_lastmode;
int	od_off;
int	od_dcyl = -1;			/* cylinder position display indicator */
int	od_noverify = 0;		/* flag to skip verify after write */
int	od_spin;			/* spin counter in odintr */
int	od_land = 1;			/* distance to land seek before op */
int	od_timer_started = 0;
int	od_update_time = KT_UNSCHED;
int	od_idle_time = KT_UNSCHED;
int	od_alert_abort = 0;		/* set non-zero upon receipt of panel
					 * response message */
int 	od_alert_present;		/* an alert panel is up */
void 	od_panel_abort(void *param, int tag, int response_val);
int 	od_vol_tag;			/* tag associated with alert panel */

static void od_dma_init(struct ctrl *ctrl);

/* debug */
int	od_dbug;
#if	DEBUG
int	od_dbug1;
#define	dbug_fsm(f)	if (od_dbug & 0x001) od_xpr f;
#define	dbug_xfer(f)	if (od_dbug & 0x002) od_xpr f;
#define	DBUG_STATUS	0x004
#define	dbug_status(f)	if (od_dbug & DBUG_STATUS) od_xpr f;
#define	dbug_dma(f)	if (od_dbug & 0x008) od_xpr f;
#define	dbug_intr(f)	if (od_dbug & 0x010) od_xpr f;
#define	dbug_timer(f)	if (od_dbug & 0x020) od_xpr f;
#define	dbug_spiral(f)	if (od_dbug & 0x040) od_xpr f;
#define	dbug_bitmap(f)	if (od_dbug & 0x080) od_xpr f;
#define	dbug_where(f)	if (od_dbug & 0x100) od_xpr f;
#define	dbug_flags(f)	if (od_dbug & 0x200) od_xpr f;
#define	DBUG_CMD	0x400
#define	dbug_cmd(f)	if (od_dbug & DBUG_CMD) od_xpr f;
#define	dbug_starve(f)	if (od_dbug & 0x800) od_xpr f;
#define	DBUG_REMAP	0x1000
#define	dbug_remap(f)	if (od_dbug & DBUG_REMAP) od_xpr f;
#define	dbug_stats(f)	if (od_dbug & 0x2000) od_xpr f;
#define	dbug_lock(f)	if (od_dbug & 0x4000) od_xpr f;
#define	dbug_q(f)	if (od_dbug & 0x8000) od_xpr f;
#define	dbug_update(f)	if (od_dbug & 0x10000) od_xpr f;
#define	DBUG_TIMEOUT	0x20000
#define	dbug_timeout(f)	if (od_dbug & DBUG_TIMEOUT) od_xpr f;
#define	dbug_expire(f)	if ((od_dbug & 0x40000) == 0) od_xpr f;
#define	dbug_vol(f)	if (od_dbug & 0x80000) od_xpr f;
#define	dbug_runout(f)	if (od_dbug & 0x100000) od_xpr f;
#define	dbug_state(f)	if (od_dbug & 0x200000) od_xpr f;
#define	dbug_label(f)	if (od_dbug & 0x400000) od_xpr f;
#define	dbug_ecc(f)	if (od_dbug & 0x800000) od_xpr f;
#define	DBUG_NO_STARVE	0x01000000
#define	DBUG_NO_HEROIC	0x02000000
#define	DBUG_ALLERRS	0x04000000
#define	DBUG_SPARE	0x08000000
#define	DBUG_PRTALLERRS	0x10000000
#define	DBUG_SHOWVOL	0x20000000
#define	DBUG_PTEST	0x40000000
#define	DBUG_PRT	0x80000000

struct reg_values od_dbug_fsm[] = {
	{ FSM_START,		"START" },
	{ FSM_SEEK,		"SEEK" },
	{ FSM_WRITE,		"WRITE" } ,
	{ FSM_VERIFY,		"VERIFY" },
	{ FSM_WORKED,		"WORKED" },
	{ FSM_FAILED,		"FAILED" },
	{ FSM_EJECT,		"EJECT" },
	{ FSM_ERR_DONE,		"ERR_DONE" },
	{ FSM_ATTN_RDS,		"ATTN_RDS" },
	{ FSM_ATTN_RES,		"ATTN_RES" },
	{ FSM_ATTN_RHS,		"ATTN_RHS" },
	{ FSM_ATTN_RID,		"ATTN_RID" },
	{ FSM_INSERT_RID,	"INSERT_RID" },
	{ FSM_INSERT_STM,	"INSERT_STM" },
	{ FSM_TEST_WRITE, 	"TEST_WRITE" },
	{ FSM_TEST_ERASE,	"TEST_ERASE" },
	{ FSM_TEST_DONE,	"TEST_DONE" },
	{ FSM_WRITE_RETRY,	"WRITE_RETRY" },
	{ FSM_RESPIN_RST,	"RESPIN_RST" },
	{ FSM_RESPIN_STM,	"RESPIN_STM" },
	{0},
};

struct reg_desc od_dbug_c_flags[] = {
	{ CF_CUR_POS,		0, "CUR_POS" },
	{ CF_ISSUE_D_CMD,	0, "ISSUE_D_CMD" },
	{ CF_ISSUE_F_CMD,	0, "ISSUE_F_CMD" },
	{ CF_DMAINT_EXP,	0, "DMAINT_EXP" },
	{ CF_DEVINT_EXP,	0, "DEVINT_EXP" },
	{ CF_ATTN_INPROG,	0, "ATTN_INPROG" },
	{ CF_ATTN_WAKEUP,	0, "ATTN_WAKEUP" },
	{ CF_ATTN_ASYNC,	0, "ATTN_ASYNC" },
	{ CF_ERR_INPROG,	0, "ERR_INPROG" },
	{ CF_ONLINE,		0, "ONLINE" },
	{ CF_SOFT_TIMEOUT,	0, "SOFT_TIMEOUT" },
	{ CF_NEED_DMA,		0, "NEED_DMA" },
	{ CF_PHYS,		0, "PHYS" },
	{ CF_TESTING,		0, "TESTING" },
	{ CF_ASYNC_INPROG,	0, "ASYNC_INPROG" },
	{ CF_DMA_ERROR,		0, "DMA_ERROR" },
	{ CF_CANT_SLEEP,	0, "CANT_SLEEP" },
	{ CF_ERR_VALID,		0, "ERR_VALID" },
	{0},
};

struct reg_desc od_dbug_disr[] = {
	{ OMD_DATA_ERR,		0, "DATA_ERR" },
	{ OMD_PARITY_ERR,	0, "PARITY_ERR" },
	{ OMD_READ_FAULT,	0, "READ_FAULT" },
	{ OMD_TIMEOUT,		0, "TIMEOUT" },
	{ OMD_ECC_DONE,		0, "ECC_DONE" },
	{ OMD_OPER_COMPL,	0, "OPER_COMPL" },
	{ OMD_ATTN,		0, "ATTN" },
	{ OMD_CMD_COMPL,	0, "CMD_COMPL" },
	{0},
};
#else	DEBUG
#define	dbug_fsm(f)
#define	dbug_xfer(f)
#define	DBUG_STATUS	0x004
#define	dbug_status(f)
#define	dbug_dma(f)
#define	dbug_intr(f)
#define	dbug_timer(f)
#define	dbug_spiral(f)
#define	dbug_bitmap(f)
#define	dbug_where(f)
#define	dbug_flags(f)
#define	DBUG_CMD	0x400
#define	dbug_cmd(f)
#define	dbug_starve(f)
#define	DBUG_REMAP	0x1000
#define	dbug_remap(f)
#define	dbug_stats(f)
#define	dbug_lock(f)
#define	dbug_q(f)
#define	dbug_update(f)
#define	DBUG_TIMEOUT	0x20000
#define	dbug_timeout(f)
#define	dbug_expire(f)
#define	dbug_vol(f)
#define	dbug_runout(f)
#define	dbug_state(f)
#define	dbug_label(f)
#define	dbug_ecc(f)
#define	DBUG_NO_STARVE	0x01000000
#define	DBUG_NO_HEROIC	0x02000000
#define	DBUG_ALLERRS	0x04000000
#define	DBUG_SPARE	0x08000000
#define	DBUG_PRTALLERRS	0x10000000
#define	DBUG_SHOWVOL	0x20000000
#define	DBUG_PTEST	0x40000000
#define	DBUG_PRT	0x80000000
#endif	DEBUG

/*
 *  The following nonsense is required to prevent overrunning the
 *  kernel stack when referencing the variable number of arguments.
 *  Strip the local stack frame and pass the arguments through to
 *  printf(), xpr() and alert() as required.
 */
int	od_save_ra;


#if	DEBUG
od_xpr()
{
	asm ("unlk a6");
	asm ("movl sp@+,_od_save_ra");
	if (od_dbug & DBUG_PRT)
		asm ("jsr _printf");
	if (xprflags & XPR_OD)
		asm ("jsr _xpr");
	asm ("movl _od_save_ra,sp@-");
	asm ("rts");
}
#endif	DEBUG

od_xpr_alert()
{
	asm ("unlk a6");
	asm ("movl sp@+,_od_save_ra");
	asm ("jsr _printf");
#if	DEBUG
	if (xprflags & XPR_OD)
		asm ("jsr _xpr");
#endif	DEBUG
	asm ("movl _od_save_ra,sp@-");
	asm ("rts");
}

odprobe (reg, ctrl)
	register caddr_t reg;
{
	register volatile struct ctrl *c = &od_ctrl[ctrl];
	register volatile struct od_regs *r;
	register volatile struct dma_chan *dcp = &c->c_dma_chan;
	register int i;
	int od_timer();
	struct bdevsw *bd;
	struct cdevsw *cd;
#ifdef	OD_SFA_ARB
	extern struct sf_access_head sf_access_head[];
#endif	OD_SFA_ARB

	if (machine_type != NeXT_CUBE && machine_type != NeXT_X15)
		return (0);

	/* FIXME: not needed unless driver is loadable? */
	bzero (c, sizeof (struct ctrl));	/* init ctrl info */
	bzero (&od_stats, sizeof (struct disk_stats));
	reg += slot_id_bmap;
	r = (struct od_regs*) reg;

	/* FIXME: more extensive test than this? */
	if (probe_rb ((char*) &r->r_cntrl1) == 0)
		return (0);
	c->c_od_regs = r;
	c->c_flags |= CF_ONLINE;
	od_readlabel = (struct disk_label*) kmem_alloc (kernel_map,
		sizeof (struct disk_label));
	if (od_readlabel == 0)
		panic ("od: probe alloc");
	od_rathole = ALIGN(caddr_t, kalloc (1024 + DMA_ENDALIGNMENT));

	/* clear error interrupts */
	if (odcinfo[ctrl]->bc_ipl == 0)
		odcinfo[ctrl]->bc_ipl = I_IPL (I_DISK);
	od_spl = ipltospl (odcinfo[ctrl]->bc_ipl);
	r->r_disr = disr_shadow | OMD_CLR_INT;
	r->r_dimr = 0;

	/* remember major device numbers */
	for (bd = bdevsw; bd < &bdevsw[nblkdev]; bd++)
		if (bd->d_open == odopen)
			od_blk_major = bd - bdevsw;
	for (cd = cdevsw; cd < &cdevsw[nchrdev]; cd++)
		if (cd->d_open == odopen)
			od_raw_major = cd - cdevsw;

	/* set various flags */
	r->r_cntrl1 = 0;		/* make sure formatter is idle */
#if	MHZ_20
	c->c_initr = OMD_SEC_GREATER | OMD_ECC_STV_DIS;
	if (scr1->s_cpu_clock >= CPU_25MHz)
		c->c_initr |= OMD_25_MHZ;
#else	MHZ_20
	c->c_initr = OMD_SEC_GREATER | OMD_ECC_STV_DIS | OMD_25_MHZ;
#endif	MHZ_20
	r->r_initr = c->c_initr;
	r->r_frmr = (u_char) od_frmr;
	r->r_dmark = 0x01;
	for (i = 0; i < OMD_NFLAG; i++) 
		r->r_flgstr[i] = od_flgstr[i];
	od_runout_time = RUNOUT_NOM;

	/* initialize DMA */
	dcp->dc_handler = od_dma_intr;
	dcp->dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	dcp->dc_dmaintr = od_dmaintr;
	dcp->dc_ddp = (struct dma_dev*) P_DISK_CSR;
	dcp->dc_queue.dq_head = (struct dma_hdr *volatile) c->c_dma_list;
	dma_init (dcp, I_DISK_DMA);

#ifdef	OD_SFA_ARB
	/* initialize arbitration logic */
	c->c_sfahp = &sf_access_head[ctrl];
	c->c_sfad.sfad_flags = 0;
#endif	OD_SFA_ARB

	/* install interrupt routine */
	install_scanned_intr (I_DISK, odintr, (void *)c);

	/* allocate test buffer */
	c->c_tbuf = (caddr_t) kmem_alloc (kernel_map, MAXBUF);
	if (c->c_tbuf == 0)
		panic ("od: test alloc");

	/* start watchdog timer */
	if (od_timer_started == 0) {
		timeout (od_timer, 0, TIMER_INTERVAL);
		od_timer_started = 1;
	}
	return ((int)reg);
}

odslave (bd, reg)
	register struct bus_device *bd;
	register caddr_t reg;
{
	/* FIXME: actually poll for slave drive? */
	return (1);
}

odattach (bd)
	register struct bus_device *bd;
{
	int err, polled_attach, i;
	register struct ctrl *c = &od_ctrl[bd->bd_ctrl];
	register struct drive *d = &od_drive[bd->bd_unit];
	register volatile struct od_regs *r = c->c_od_regs;

	/* capture attach flag if called from od_try_attach() */
	polled_attach = d->d_flags & DF_TRY_ATTACH;
	
	/* allocate buffers if we are not polled and can wait */
	if (!polled_attach && od_label == 0)
		od_buf_alloc();
	
	/* initialize drive info */
	bzero (d, sizeof (struct drive));

	/* link everything together */
	d->d_bc = bd->bd_bc;
	d->d_bd = bd;
	d->d_ctrl = c;

	/* FIXME: somehow, determine drive type */
	d->d_drive_type = DRIVE_TYPE_OMD1;
	d->d_last_hos = -1;		/* won't ever match a valid hos */
	d->d_cur_head = -1;		/* won't ever match a valid head */

	/* enable error interrupts, clear and enable attention interrupt */
	od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR);
	od_drive_cmd (c, d, OMD1_RID, SPIN | DONT_INTR);
	r->r_dimr = OMD_ENA_INT;
	
	/* print drive version information */
	if (!polled_attach) {
		i = od_status (c, d, r, OMD1_RVI, SPIN | DONT_INTR);
		if (i != -1)
			printf ("drive ROM v%d, servo ROM v%d\n",
				(i >> 8) & 0xf, (i >> 4) & 0xf);
	}

	/* read label */
	dbug_vol (("attach: d%d ", d - od_drive, 0, 0, 0, 0));
	d->d_flags |= DF_ATTACH;
	if (err = od_read_label (c, d, polled_attach)) {
		if (d->d_flags & DF_SPINNING)
			printf ("%s%d: no valid disk label found\n",
				d->d_bd->bd_driver->br_dname, d - od_drive);
	}
	d->d_flags &= ~DF_ATTACH;
	return (err);
}

od_timer()
{
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct volume *v;
	register int flags = CF_ONLINE | CF_DEVINT_EXP, s;
	int od_daemon(), od_spiral(), request = 0;
	static od_thread_init;

	if (kernel_task && od_thread_init == 0) {
		kernel_thread_noblock (kernel_task, od_label_alloc);
		kernel_thread_noblock (kernel_task, od_try_attach);
		kernel_thread_noblock (kernel_task, od_request);
		od_thread_init = 1;
	}

	for (c = od_ctrl; c < &od_ctrl[NOC]; c++) {
		if (c->c_timeout) {
			c->c_timeout--;
			if (c->c_timeout == 0) {
				dbug_timer (("SOFT_TIMEOUT ", 0, 0, 0, 0, 0));
				s = spln (od_spl);
				c->c_flags |= CF_SOFT_TIMEOUT;
				odintr (c);
				splx (s);
			}
		}
	}

	for (d = od_drive; d < &od_drive[NOD]; d++) {
	
		/* stop spiraling if idle too long */
		if (d->d_flags & DF_SPINNING && d->d_flags & DF_SPIRALING &&
		    d->d_spiral_time >= 0 &&
		    d->d_spiral_time++ > SPIRAL_TIMEOUT) {
			dbug_spiral (("od_timer: sched od_spiral %d\n",
				d->d_spiral_time, 0, 0, 0, 0));
			d->d_spiral_time = KT_BUSY;
			kernel_thread_noblock (kernel_task, od_spiral);
		}
	}

	/* limit updates to a fixed interval */
	if (od_update_time >= KT_SCHED && od_update_time++ > UPDATE_TIMEOUT) {
		od_update_time = KT_BUSY;
		kernel_thread_noblock (kernel_task, od_update_thread);
	}

#if 0
	/* FIXME!!!!!  figure out why this doesn't work! */
	/* keep track of idle time so we know when to start daemon */
	if (od_idle_time >= KT_SCHED && od_idle_time++ > IDLE_TIMEOUT) {
		od_idle_time = KT_BUSY;
		kernel_thread_noblock (kernel_task, od_daemon);
	}
#endif
	timeout (od_timer, 0, TIMER_INTERVAL);
}

odopen (dev, flag)
	dev_t dev;
{
	int vol = OD_VOL(dev), part = OD_PART(dev), s, raw, cbuf = 0;
	struct volume *v = &od_vol[vol];
	register struct partition *p;
	register struct ctrl *c = od_ctrl; /* FIXME: what about multiple ctrls? */
	
	/* check for open of controller device */
	if (vol == NVOL)
		return (0);
	/* was..if (vol >= NVOL || (v->v_flags & VF_MISSING))
	 ...is: */
	if(vol > NVOL || (c->c_flags & CF_ONLINE) == 0)
		return (ENXIO);
	s = spln (od_spl);

	/* if volume is spinning up, must wait until spinup complete */
	while (v->v_flags & VF_SPINUP) {
		v->v_flags |= VF_SPINUP_WANTED;
		sleep (&v->v_flags, PRI_OMD);
	}
	if ((v->v_flags & VF_BUSY) == 0) {
		if (flag & O_NDELAY) {
			splx (s);
			return (EWOULDBLOCK);
		}
		dbug_vol (("open: make empty for v%d ", vol, 0, 0, 0, 0));
		od_empty = vol + 1;
		od_make_empty();
		if (od_requested == 0 && od_spinup == 0) {
			dbug_vol (("odopen: WAKEUP req ", 0, 0, 0, 0, 0));
			od_requested = 1;
			wakeup (&od_requested);
		} else
		if (od_requested) {
			dbug_vol (("odopen: req BUSY ", 0, 0, 0, 0, 0));
		} else {
			dbug_vol (("odopen: req SPINUP ", 0, 0, 0, 0, 0));
		}
		while (od_empty > 0)
			sleep (&od_empty, PRI_OMD);
		if (od_empty < 0)
			return (ENXIO);
	}
	if ((flag & O_NDELAY) && (v->v_flags & VF_INSERTED) == 0) {
		splx (s);
		return (EWOULDBLOCK);
	}
	splx (s);
	raw = cdevsw[major(dev)].d_open == odopen;

	/* allocate copy buffer on first raw open */
	if (c->c_cbuf == 0 && raw && (v->v_refcnt & (NPART - 1)) == 0) {
		c->c_cbuf = (caddr_t) kmem_alloc (kernel_map, MAXBUF);
		if (c->c_cbuf == 0)
			panic ("od: copy alloc");
		cbuf = 1;
	}
	v->v_refcnt |= (raw? 1 : (1 << NPART)) << part;
	v->v_owner = u.u_ruid;
	dbug_vol (("open: v%d dev 0x%x refcnt 0x%x\n", vol, dev, v->v_refcnt,
		0, 0));
	
	/* check for drive online, partition not empty */
	p = &v->v_label->dl_part[part];
	if (p->p_size > 0)
		return (0);

	/* allow open of empty raw partitions for disk utilities */
	if (cdevsw[major(dev)].d_open == odopen)
		return (0);
	v->v_refcnt &= ~((raw? 1 : (1 << NPART)) << part);
	if (cbuf)
		kmem_free (kernel_map, c->c_cbuf, MAXBUF);
	dbug_vol (("open: ENXIO "));
	return (ENXIO);
}

odclose (dev, flag)
	dev_t dev;
{
	register int vol = OD_VOL(dev), part = OD_PART(dev), raw, s;
	struct volume *v = &od_vol[vol];
	register struct ctrl *c = od_ctrl; /* FIXME: what about multiple ctrls? */

	/* skip if controller device */
	if (vol == NVOL)
		return;
		
	/* last close of device -- eject disk and free volume structure */
	raw = cdevsw[major(dev)].d_open == odopen;
	v->v_refcnt &= ~((raw? 1 : (1 << NPART)) << part);

	/* deallocate copy buffer on last raw close */
	if (c->c_cbuf && raw && (v->v_refcnt & (NPART - 1)) == 0) {
		kmem_free (kernel_map, c->c_cbuf, MAXBUF);
		c->c_cbuf = 0;
	}
	if (v->v_refcnt) {
		dbug_vol (("odclose: v%d dev 0x%x refcnt 0x%x\n",
			vol, dev, v->v_refcnt, 0, 0));
		return;
	}
	
	/* FIXME: better way to guard against close of blk dev during fsck! */
	if (vol == OD_VOL(rootdev) && major(rootdev) == od_blk_major) {
		dbug_vol (("odclose: v%d ROOT VOL\n", vol, 0, 0, 0, 0));
		return;
	}
	
	/* free volume if ejected on last close */
	s = spln (od_spl);
	dbug_vol (("odclose: v%d LAST CLOSE\n", vol, 0, 0, 0, 0));
	if ((v->v_flags & VF_INSERTED) == 0 || (v->v_flags & VF_EJECT)) {
	
		/*
		 *  Can't free volume until eject actually happens!
		 *  (might be delayed by od_update)
		 */
		while (v->v_flags & VF_EJECT)
			sleep (&v->v_flags, PRI_OMD);
		dbug_vol (("odclose: FREE VOLUME v%d\n", vol, 0, 0, 0, 0));
		if (v->v_label) {
			kmem_free (kernel_map, v->v_label,
				sizeof (*v->v_label));
			v->v_label = 0;
		}
		if (v->v_bad_block) {
			kmem_free (kernel_map, v->v_bad_block,
				sizeof (*v->v_bad_block));
			v->v_bad_block = 0;
		}
		if (v->v_bitmap) {
			kmem_free (kernel_map, v->v_bitmap, MAX_BITMAP);
			v->v_bitmap = 0;
		}
		v->v_flags = 0;
	}
	splx (s);
}

odread (dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register int vol = OD_VOL(dev);
	int odstrategy(), odminphys();

	if (vol < 0 || vol >= NVOL)
		return (ENXIO);
	return (physio (odstrategy, &od_vol[vol].v_rbuf, dev, B_READ,
		odminphys, uio, DEV_BSIZE));
}

odwrite (dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register int vol = OD_VOL(dev);
	int odstrategy(), odminphys();

	if (vol < 0 || vol >= NVOL)
		return (ENXIO);
	return (physio (odstrategy, &od_vol[vol].v_rbuf, dev, B_WRITE,
		odminphys, uio, DEV_BSIZE));
}

odstrategy (bp)
	register struct buf *bp;
{
	register int vol = OD_VOL(bp->b_dev), s, dev, doit = 0;
	register struct volume *v = &od_vol[vol];
	register struct drive *d;
	register struct disk_label *l;
	register struct partition *p;
	register daddr_t bno;
	register ds_queue_t *q;

	/* was...
	if (vol < 0 || vol >= NVOL || v->v_flags == 0 ||
	    (v->v_flags & VF_MISSING)) {
	...is: */
	if (vol < 0 || vol >= NVOL || v->v_flags == 0) {
		bp->b_error = ENXIO;
		goto bad;
	}
	
	/* catch write protected disks */
	if ((bp->b_flags & B_READ) == 0 && (v->v_flags & VF_WP)) {
		bp->b_error = EROFS;
		goto bad;
	}
	
	l = v->v_label;
	p = &l->dl_part[OD_PART(bp->b_dev)];
	bno = bp->b_blkno;
	bp->b_sort_key = bno;
	if (bp->b_bcount % l->dl_secsize) {
		bp->b_error = EINVAL;
		goto bad;
	}
	if (bp != &v->v_cbuf) {		/* don't check special I/O */
		if ((v->v_flags & VF_ONLINE) == 0) {
			bp->b_error = ENXIO;
			goto bad;
		}
		if (p->p_size == 0 || bno < 0 || bno > p->p_size) {
			bp->b_error = EINVAL;
			goto bad;
		}
		if (bno == p->p_size) {	/* end-of-file */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		bp->b_sort_key += p->p_base;

		/* first regular I/O schedules daemon */
		if (od_idle_time == KT_UNSCHED)
			od_idle_time = KT_SCHED;

		/* non-special I/O causes daemon to stop */
		if (od_idle_time >= KT_SCHED)
			od_idle_time = KT_SCHED;
		if (od_idle_time == KT_BUSY)
			od_idle_time = KT_STOP;
	}
	bp->b_sort_key /= l->dl_nsect;

	/* add request to queue */
	q = &v->v_queue;
	s = spln (od_spl);

	/* implement crude locking of realtime requests */
	while ((v->v_flags & VF_LOCK) && bp->b_rtpri <= RTPRI_MIN) {
		v->v_flags |= VF_LOCK_OUT;
		sleep (v, PRI_OMD);
	}
	
	/* always put special requests on the front of the queue */
	if (bp == &v->v_cbuf && (v->v_flags & VF_WANTED) &&
	    v->v_cmd != OMD_EJECT) {
		disksort_enter_head(q, bp);
		doit = 1;
	} else
	
	/* always put eject requests on the end of an inserted volume */
	if (bp == &v->v_cbuf && (v->v_cmd == OMD_EJECT_NOW ||
	    v->v_cmd == OMD_EJECT)) {
		dbug_lock (("strat: EJECT_NOW on tail\n"));
		disksort_enter_tail(q, bp);
	} else {
		disksort_enter(q, bp);
	}
#if	DEBUG
	if (current_thread()) {
		static int foo;
		
		dbug_q (("strat: %s th 0x%x v%d %c bf 0x%x **********\n", 
			current_thread()->task->u_address->uu_comm,
			current_thread(),
			v - od_vol, bp->b_flags & B_READ? 'R' : 'W', 
			bp->b_flags));
		if ((v - od_vol) == 1)
			foo = 1;
		if ((v - od_vol) == 0)
			foo = 0;
	}
#endif	DEBUG
	if (q->active == 0 || doit) {
		od_drive_start (v);
		d = &od_drive[v->v_drive];
		if (!queue_empty(&d->d_bc->bc_tab) && d->d_bc->bc_active == 0)
			od_ctrl_start (d->d_bc);
	}
	splx (s);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	if (!(bp->b_flags & B_DONE))
		biodone (bp);
	return;
}

/* start an operation on a drive */
od_drive_start (v)
	register struct volume *v;
{
	register struct bus_ctrl *bc;
	register struct bus_device *bd;
	register ds_queue_t *q;
	register struct drive *d;

	q = &v->v_queue;
	if (disksort_first(q) == NULL)
		return;
	if ((v->v_flags & VF_INSERTED) == 0) {
		dbug_vol (("drive_start: v%d WANTED ", v - od_vol,
			0, 0, 0, 0));
		v->v_flags |= VF_WANTED;
	}
	if (q->active == 0)
		q->active = 1;

	/* put request on controller queue */
	if (q->active != 2) {
		d = &od_drive[v->v_drive];
		bc = d->d_bc;
		queue_enter(&bc->bc_tab, q, ds_queue_t *, ds_link);
		q->active = 2;
	}
}

od_run_out (bc)
	struct bus_ctrl *bc;
{
	int s;
	
	s = spln(od_spl);
	dbug_runout (("RUNOUT\n", 0, 0, 0, 0, 0));
	od_runout = 2;
	od_ctrl_start(bc);
	splx(s);
}

/* start an operation on a controller */
od_ctrl_start (bc)
	register struct bus_ctrl *bc;
{
	register struct buf *bp;
	register ds_queue_t *q, *wanted = 0;
	register volatile struct ctrl *c;
	register struct volume *v;

	c = &od_ctrl[bc->bc_ctrl];
	if (bc->bc_active) {
		dbug_vol (("c_s: STILL ACTIVE ", 0, 0, 0, 0, 0));
		return;
	}
loop:
	q = (ds_queue_t *)queue_first(&bc->bc_tab);
	if (queue_end(&bc->bc_tab, (queue_t)q)) {
		dbug_vol (("c_s: q NULL ", 0, 0, 0, 0, 0));
		return;
	}
	bp = disksort_first(q);

	if (bp == NULL) {
		dbug_vol (("c_s: q buf NULL ", 0, 0, 0, 0, 0));
		queue_remove(&bc->bc_tab, q, ds_queue_t *, ds_link);
		goto loop;
	}
	/*
	 * Mark bp busy in the queue.
	 */
	disksort_qbusy(q);
	v = &od_vol[OD_VOL(bp->b_dev)];

	/* request missing volume when there's nothing else left to do */
	if (wanted == q) {
		if (od_runout == 0) {
			timeout (od_run_out, bc, od_runout_time);
			od_runout = 1;
			dbug_runout(("runout %d hz started\n",
				od_runout_time, 0, 0, 0, 0));
			return;
		}
		if (od_runout == 1) {
			dbug_runout(("waiting for runout\n", 0, 0, 0, 0, 0));
			return;
		}
		dbug_runout(("runout done (was %d)\n", od_runout, 0, 0, 0, 0));
		od_runout = 0;
		if (od_runout_time == RUNOUT_EJECT)
			od_runout_time = RUNOUT_NOM;
		if (od_requested == 0 && od_spinup == 0) {
			dbug_vol (("c_s: WAKEUP req ", 0, 0, 0, 0, 0));
			od_requested = 1;
			wakeup (&od_requested);
		} else
		if (od_requested) {
			dbug_vol (("c_s: req BUSY ", 0, 0, 0, 0, 0));
		} else
		if (od_spinup) {
			dbug_vol (("c_s: req SPINUP ", 0, 0, 0, 0, 0));
		}
		return;
	}

	/* an eject doesn't need the volume inserted */
	if (bp == &v->v_cbuf && (v->v_cmd == OMD_EJECT ||
	    v->v_cmd == OMD_EJECT_NOW)) {
dbug_vol (("c_s: EJECT "));
		goto doit;
	}

	/* skip volumes that aren't inserted -- put at end of queue */
	if (v->v_flags & VF_WANTED) {
		queue_remove(&bc->bc_tab, q, ds_queue_t *, ds_link);
		queue_enter(&bc->bc_tab, q, ds_queue_t *, ds_link);
		if (wanted == 0)
			wanted = q;
		dbug_vol (("c_s: v%d WANTED ", OD_VOL(bp->b_dev), 0, 0, 0, 0));
		goto loop;
	}
doit:
	if (od_runout && (bp != &v->v_cbuf || od_runout_time == RUNOUT_NOM)) {
		if (od_runout_time == RUNOUT_EJECT)
			od_runout_time = RUNOUT_NOM;
		if (od_runout == 1)
			untimeout(od_run_out, bc);
		od_runout = 0;
		dbug_runout(("runout cancelled\n", 0, 0, 0, 0, 0));
	}
	bc->bc_active++;
	dbug_q (("c_s: START I/O v%d bf 0x%x bp 0x%x\n",
		v - od_vol, bp->b_flags, bp, 0, 0));
#ifdef	OD_SFA_ARB
	c->c_sfad.sfad_start = (void *)od_go;
	c->c_sfad.sfad_arg = bc;
	sfa_arbitrate((sf_access_head_t)c->c_sfahp, 
		(sf_access_device_t)&c->c_sfad);
#else	OD_SFA_ARB
	busgo (oddinfo[od_vol[OD_VOL(bp->b_dev)].v_drive]);
#endif	OD_SFA_ARB
}

/* called from busgo() after bus locked */
od_go (bc)
	register struct bus_ctrl *bc;
{
	register struct buf *bp;
	register ds_queue_t *q;
	register volatile struct ctrl *c;
	register struct drive *d;
	register int dk;

	if (queue_empty(&bc->bc_tab))
		panic ("od: queue empty");
	q = (ds_queue_t *)queue_first(&bc->bc_tab);
	if ((bp = disksort_first(q)) == NULL)
		panic ("od: queue empty");
	c = &od_ctrl[bc->bc_ctrl];
	d = &od_drive[od_vol[OD_VOL(bp->b_dev)].v_drive];
	event_meter (EM_DISK);
	if ((dk = d->d_bd->bd_dk) >= 0) {
		dk_busy |= 1 << dk;
		dk_xfer[dk]++;
		dk_wds[dk] += bp->b_bcount >> 6;
	}
	od_setup (c, d, bp);
}

/* setup transfer parameters in od_ctrl from info in bp */
od_setup (c, d, bp)
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct buf *bp;
{
	register struct disk_label *l;
	register struct partition *p;
	register nblk;
	register struct volume *v = d->d_vol;

	/* check to see if drive has gone away */
	if (bp != &v->v_cbuf && (d->d_flags & DF_ONLINE) == 0) {
		bp->b_error = ENXIO;
		goto err;
	}

	/*
	 *  Keep daemon from testing a cartridge that was ejected.
	 *  When not spinning, can only issue spin up comand.
 	 */
	if (bp == &v->v_cbuf && (d->d_flags & DF_SPINNING) == 0 &&
	    v->v_cmd != OMD_SPINUP) {
		bp->b_error = ENXIO;
		goto err;
	}
	l = v->v_label;
	p = &l->dl_part[OD_PART(bp->b_dev)];
	c->c_blkno = bp->b_blkno;
	c->c_va = bp->b_un.b_addr;
	c->c_pmap = bp->b_flags & B_PHYS?
		vm_map_pmap (bp->b_proc->task->map) : pmap_kernel();

	/* for now force alignment by copying to intermediate kernel buffer */
	if ((bp->b_flags & B_PHYS) && !DMA_ENDALIGNED(c->c_va)) {
		if (bp->b_bcount > MAXBUF)
			panic ("od: copy > MAXBUF");
		if (c->c_cbuf == 0)
			panic ("od: no copy buf");
		if ((bp->b_flags & B_READ) == 0) {
			int resid, len;
			caddr_t va, ka;

			/*
			 * Use bcopy with a kernel buffer instead of
			 * copyin because user process may not be mapped in
			 * when od_setup is called at interrupt time.
			 */
			resid = bp->b_bcount;
			va = bp->b_un.b_addr;
			ka = c->c_cbuf;
			while (resid) {
				len = MIN(resid, NeXT_page_size -
					((int)va % NeXT_page_size));
				bcopy (pmap_resident_extract (c->c_pmap, va),
					ka, len);
				ka += len;
				va += len;
				resid -= len;
			}
		}
		c->c_va = c->c_cbuf;
		c->c_cmap = c->c_pmap;
		c->c_pmap = pmap_kernel();
		c->c_ccount = bp->b_bcount;
		bp->b_flags |= B_PHYS_COPY;
	}
	nblk = howmany (bp->b_bcount, l->dl_secsize);
	if (bp == &v->v_cbuf) {
		c->c_cmd = v->v_cmd;
		c->c_nsect = nblk;
		c->c_flags |= CF_PHYS;
	} else {
		c->c_blkno += p->p_base;
		c->c_cmd = bp->b_flags & B_READ? OMD_READ : OMD_WRITE;
		c->c_nsect = MIN(nblk, p->p_size - bp->b_blkno);
		c->c_flags &= ~CF_PHYS;
	}
	bp->b_resid = bp->b_bcount - c->c_nsect * l->dl_secsize;
	c->c_retry = c->c_rtz = 0;
	c->c_before = c->c_after = 0;
	c->c_od_regs->r_initr = c->c_initr;
	c->c_scan_rewrite = 0;
#if	SCAN_RETRY
	c->c_flags &= ~CF_UNTESTED;
#endif	SCAN_RETRY
dbug_q (("setup %c blk %d bf 0x%x bp 0x%x\n", bp->b_flags & B_READ? 'R' : 'W', c->c_blkno, bp->b_flags, bp, 0));
	od_fsm (c, d, FSM_START);
	microboot (&d->d_lru);
	return;
err:
	bp->b_flags |= B_ERROR;
	busdone (odcinfo[c - od_ctrl]);
}

#ifdef	OD_SFA_ARB
/*
 * Avoid calling the real busdone; since we never busgo()...
 */
static busdone(struct bus_ctrl *bc)
{
	od_done(bc);
}
#endif	OD_SFA_ARB
 
/* called from busdone() after I/O completes */
od_done (bc)
	register struct bus_ctrl *bc;
{
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct volume *v;
	register struct buf *bp;
	register ds_queue_t *q;
	register int dk, s;

	s = spln (od_spl);
	if (queue_empty(&bc->bc_tab))
		panic ("od: empty q");
	q = (ds_queue_t *)queue_first(&bc->bc_tab);
	bp = disksort_first(q);
	c = &od_ctrl[bc->bc_ctrl];
	v = &od_vol[OD_VOL(bp->b_dev)];
	d = &od_drive[v->v_drive];
	if ((dk = d->d_bd->bd_dk) >= 0)
		dk_busy &= ~(1 << dk);
	bc->bc_active = 0;
	disksort_remove(q, bp);

	/* finish all I/O on this volume first before looking at next volume */
	if (disksort_first(q) == NULL) {
		queue_remove(&bc->bc_tab, q, ds_queue_t *, ds_link);
		q->active = 0;
	}
	splx (s);
	
#ifdef	OD_SFA_ARB
	sfa_relinquish((sf_access_head_t)c->c_sfahp, 
		(sf_access_device_t)&c->c_sfad, 
		SF_LD_NONE);
#endif	OD_SFA_ARB
	/* if volume is now missing then don't do anything more with it */
	if (v->v_flags & VF_MISSING)
		goto out;

	/* copy out intermediate buffer to user space */
	if ((bp->b_error == 0) && (bp->b_flags & B_PHYS_COPY)) {
		if (bp->b_flags & B_READ) {
			int resid, len;
			caddr_t va, ka;

			/*
			 * Use bcopy with phys addr of user buffer instead of
			 * copyout because user process may not be mapped in
			 * at interrupt time.
			 */
			resid = c->c_ccount;
			va = bp->b_un.b_addr;
			ka = c->c_cbuf;
			while (resid) {
				len = MIN(resid, NeXT_page_size -
					((int)va % NeXT_page_size));
				bcopy (ka, pmap_resident_extract
					(c->c_cmap, va), len);
				ka += len;
				va += len;
				resid -= len;
			}
		}
		bp->b_flags &= ~B_PHYS_COPY;
	}
	if (!(bp->b_flags & B_DONE))
		biodone (bp);
	dbug_q (("done: DONE I/O v%d bf 0x%x bp 0x%x\n",
		v - od_vol, bp->b_flags, bp, 0, 0));
out:
	s = spln (od_spl);
	/* start next request if still inserted */
	if (disksort_first(q))
		od_drive_start (v);
	if (!queue_empty(&bc->bc_tab) && bc->bc_active == 0)
		od_ctrl_start (bc);
	splx (s);
}

char od_next_state[] = {
	/* next */	/* fcmd */
	0,		/* 0 */
	FSM_VERIFY,	/* OMD_WRITE */
	FSM_WORKED,	/* OMD_READ */
	0,		/* 3 */
	FSM_WRITE,	/* OMD_ERASE */
	0,		/* 4 */
	0,		/* 5 */
	0,		/* 6 */
	FSM_WORKED,	/* OMD_VERIFY */
};

/*
 * Start or continue a transfer via finite state machine.
 *
 * Confusing terminology: because current optical drives
 * only have one media surface the term "track" is used instead
 * of "cylinder".  Also, "head" means which head mode is in effect
 * instead of which surface is being selected.
 */
od_fsm (c, d, state)
	register volatile struct ctrl *c;
	register struct drive *d;
	register int state;
{
	register struct volume *v = d->d_vol;
	struct bus_ctrl *bc = odcinfo[c - od_ctrl];
	struct bus_device *bd = d->d_bd;
	struct buf *bp;
	register struct disk_label *l = v->v_label;
	register volatile struct od_regs *r = c->c_od_regs;
	register struct od_err *e;
	register int bme, dirty = 0, be;
	int track, rel_trk, max_be, off, limit, i, pass, ag, status, rj_head;
	int use_rj, dk, s, sft;
	int norm_sect;				/* normalized sector # */
	int spht = l->dl_nsect >> 1;		/* sectors per half track */
	int spbe;				/* sectors per bitmap entry */
	int apag;				/* alternates per alt group */
	int g_usable = l->dl_ag_size - l->dl_ag_alts;

	dbug_fsm (("[%n] ", state, od_dbug_fsm, 0, 0, 0));
	c->c_fsm = state;
	c->c_flags &= ~(CF_ISSUE_D_CMD|CF_ISSUE_F_CMD);
	if (d->d_spiral_time >= 0)
		d->d_spiral_time = 0;		/* reset spiral timeout */
	if (l->dl_version == DL_V1)
		spbe = l->dl_nsect >> 1;
	else
		spbe = 1;
	apag = l->dl_ag_alts / spbe;

	switch (state) {

	case FSM_START:
		c->c_retry_fsm = FSM_START;
		i = v->v_di->di_maxbcount / v->v_di->di_devblklen;
		c->c_xfer = MIN(i, c->c_nsect);

		/*
		 *  On retries, do one sector at a time (but not
		 *  for the first sector timeout).
		 */
		if (l->dl_version != DL_V1 && (((c->c_retry || c->c_rtz) &&
		    !(c->c_retry == 1 && c->c_err == E_TIMEOUT)) ||
		    (c->c_flags & CF_ECC_SCAN)))
			c->c_xfer = 1;

		/* if not phys I/O, skip over alternates */
		if ((c->c_flags & (CF_PHYS | CF_TESTING)) == 0) {
			off = c->c_blkno % g_usable;
			if (off >= l->dl_ag_off)
				off += l->dl_ag_alts;
			if (off < l->dl_ag_off)
				limit = l->dl_ag_off - off;
			else
				limit = l->dl_ag_size - off + l->dl_ag_off;
			c->c_xfer = MIN(c->c_xfer, limit);
			c->c_pblk = c->c_blkno/g_usable*l->dl_ag_size + off +
				l->dl_front;
		} else {
			/* FIXME: bound upper limit of xfer? */
			c->c_pblk = c->c_blkno;
		}
forward:
		c->c_pblk += v->v_base;	/* skip reserved area */
		c->c_sect = c->c_pblk % l->dl_nsect;
		norm_sect = c->c_sect % spbe;
		c->c_track = c->c_pblk / l->dl_nsect;	/* really cyl # */

		if (l->dl_version == DL_V1) {
			bme = (c->c_track - v->v_base / l->dl_nsect) << 1;
			if (c->c_sect >= spht)
				bme |= 1;
		} else
			bme = c->c_pblk - v->v_base;
		max_be = howmany (norm_sect + c->c_xfer, spbe) + bme;
		c->c_fcmd = c->c_cmd;

		/* bypass status bitmap check if testing or special I/O */
		if ((c->c_flags & CF_TESTING) || ((c->c_flags & CF_PHYS) &&
		    (v->v_flags & VF_IGNORE_BITMAP))) {
			dirty = 1;	/* can't tell if pre-erased */

			/* make sure writes are reflected in bitmap */
			if ((c->c_fcmd == OMD_WRITE || c->c_fcmd == OMD_ERASE)
			    && (c->c_flags & CF_TESTING) == 0) {
				for (be = bme; be < max_be; be++) {
					c->c_offset = be >> 4;
					c->c_shift = (be & 0xf) << 1;
					if (((v->v_bitmap[c->c_offset] >>
					    c->c_shift) & 3) != SB_BAD)
						if (c->c_cmd == OMD_ERASE) {
							SET_BITMAP(SB_ERASED);
						} else {
							SET_BITMAP(SB_WRITTEN);
						}
				}
			}
		} else {

		dirty = 0;
		for (be = 0; bme < max_be; be++, bme++) {

		/* bitmap is stored 2 bits per entry */
		c->c_offset = bme >> 4;
		c->c_shift = (bme & 0xf) << 1;
		dbug_where (("xfer %d blkno %d pblk %d sect %d track %d ",
			c->c_xfer, c->c_blkno, c->c_pblk, c->c_sect,
			c->c_track));
		dbug_where (("bme %d offset %d shift %d\n",
			bme, c->c_offset, c->c_shift, 0, 0));
		dbug_bitmap (("%d|%d<%d,%d,%d>", c->c_track, c->c_sect,
			bme, c->c_offset, c->c_shift));
		switch ((v->v_bitmap[c->c_offset] >> c->c_shift) & 3) {

		case SB_UNTESTED:
			dbug_bitmap (("U ", 0, 0, 0, 0, 0));
			if (be != 0 && (l->dl_version == DL_V1 ||
			    c->c_cmd == OMD_READ)) {

				/* do sectors ahead of untested block */
				c->c_xfer = be * spbe - norm_sect;
				goto cont;
			}

			if (c->c_cmd == OMD_READ) {
				for (i = 0; bme < max_be; bme++) {
					off = bme >> 4;
					sft = (bme & 0xf) << 1;
					if (((v->v_bitmap[off] >> sft) & 3) !=
					    SB_UNTESTED)
						break;
					i++;
				}
				od_zero_fill (c, v, i);
				goto worked;
			}

			/* test only for daemon requests if not version 1 */
			if (l->dl_version != DL_V1 && c->c_cmd != OMD_TEST) {
				if (c->c_cmd == OMD_ERASE) {
					SET_BITMAP (SB_ERASED);
				} else {
					SET_BITMAP (SB_WRITTEN);
				}
				dirty = 1;
#if	SCAN_RETRY
				c->c_flags |= CF_UNTESTED;
#endif	SCAN_RETRY
				break;
			}
test:
			c->c_flags |= CF_TESTING;
			c->c_va = c->c_tbuf;
			c->c_pmap = pmap_kernel();
			c->c_cmd = OMD_WRITE;
			c->c_test_pass = 0;

			/* truncate to # of sectors in  bitmap entry */
			c->c_test_blkno = c->c_pblk - v->v_base - norm_sect;
			od_fsm (c, d, FSM_TEST_WRITE);
			return;

		case SB_BAD:
			dbug_bitmap (("B ", 0, 0, 0, 0, 0));

			/* keep a delayed daemon request from screwing us */
			if (c->c_cmd == OMD_TEST)
				goto worked;

			if (be != 0) {
				/* do sectors ahead of bad block */
				c->c_xfer = be * spbe - norm_sect;
				goto cont;
			}

			/*
			 * Forward a bad block.  Scan the bad block
			 * table by alternate group and then take any entry.
			 * Alternate blocks can be recursively bad.
			 */
			if ((i = od_locate_alt (c, d, v,
			    c->c_pblk - v->v_base - norm_sect, c->c_pblk))
			    == -1) {

				/* FIXME! need to recover from this! */
				c->c_err = E_BAD_BAD;
				goto failed;
			}

			/* alternate is located by its table position */
			ag = i / apag;
			if (ag >= l->dl_ngroups) {

				/* overflow alternate area */
				c->c_pblk = l->dl_front +
				    l->dl_ngroups*l->dl_ag_size +
				    (i - apag*l->dl_ngroups) * spbe;
			} else {
				c->c_pblk = l->dl_front +
				    ag*l->dl_ag_size + l->dl_ag_off +
				    (i % apag) * spbe;
			}
			dbug_remap (("remapped to %d\n",
				c->c_pblk, 0, 0, 0, 0));

			/* do proper amount in the alternate */
			c->c_pblk += norm_sect;
			c->c_xfer = MIN(c->c_xfer, spbe - norm_sect);
			goto forward;

		case SB_ERASED:
			dbug_bitmap (("E ", 0, 0, 0, 0, 0));

			/*
			 * Update bitmap if we're going to write.
			 * This has the side-effect of causing write
			 * retries to always erase in case the bitmap
			 * is out-of-sync from a crash.
			 */
			if (c->c_cmd == OMD_WRITE) {
				SET_BITMAP (SB_WRITTEN);

				/*
				 * Have to write the entire bitmap entry if
				 * writing for the first time so a subsequent
				 * read won't fail with erased sectors.
				 */
				if (be == 0)
					c->c_before = norm_sect;
				if (spbe != 1 && bme == (max_be - 1)) {
					c->c_after = spbe - ((norm_sect +
						c->c_xfer) % spbe);
					if (c->c_after == spbe)
						c->c_after = 0;
				}
			} else

			if (c->c_cmd == OMD_READ) {

				/* do sectors ahead of erased block */
				if (be != 0) {
					c->c_xfer = be * spbe - norm_sect;
					goto cont;
				}
				for (i = 0; bme < max_be; bme++) {
					off = bme >> 4;
					sft = (bme & 0xf) << 1;
					if (((v->v_bitmap[off] >> sft) & 3) !=
					    SB_ERASED)
						break;
					i++;
				}
				od_zero_fill (c, v, i);
				goto worked;
			} else

			/*
			 * If this is an erase request, and we're already
			 * erased, then get out.  This happens when the
			 * the daemon is running.
			 */
			if (c->c_cmd == OMD_TEST)
				goto worked;
			else
			if (c->c_cmd == OMD_ERASE)
				goto worked;
			break;

		case SB_WRITTEN:
			dbug_bitmap (("W ", 0, 0, 0, 0, 0));

			/* keep a delayed daemon request from screwing us */
			if (c->c_cmd == OMD_TEST)
				goto worked;

			dirty = 1;
			break;
		} /* switch */
		} /* for */
		} /* if */
cont:
		/* if any sector was previously written do an erase pass */
		/* FIXME: really erase a large, multi track request? */
		if (c->c_fcmd == OMD_WRITE && dirty)
			c->c_fcmd = OMD_ERASE;

		switch (c->c_fcmd) {

		case OMD_READ:
			c->c_head = OMD1_SRH;
			rj_head = RJ_READ;
			break;

		case OMD_WRITE:
			c->c_head = OMD1_SWH;
			rj_head = RJ_WRITE;
			break;

		case OMD_TEST:
			c->c_fcmd = OMD_ERASE;
			/* fall into ... */

		case OMD_ERASE:
			c->c_head = OMD1_SEH;
			rj_head = RJ_ERASE;
			break;

		case OMD_VERIFY:
			c->c_head = OMD1_SVH;
			rj_head = RJ_VERIFY;
			break;

		case OMD_SEEK:
			/* c_blkno is seek address */
			if (1 /* d->d_last_hos != (u_int)c->c_blkno >> 12 */) {
				dbug_xfer (("HOS %d ",
					c->c_blkno>>12, 0, 0, 0, 0));
				od_drive_cmd (c, d, OMD1_HOS |
					(c->c_blkno >> 12), SPIN | DONT_INTR);
				d->d_last_hos = c->c_blkno >> 12;
			}
			if ((d->d_flags & DF_SPIRALING) == 0) {
				od_drive_cmd (c, d, OMD1_SOO, SPIN|DONT_INTR);
				d->d_flags |= DF_SPIRALING;
			}

			s = spl7();		/* FIXME */
			od_drive_cmd (c, d, OMD1_RCA, DONT_SPIN | DONT_INTR);
			DELAY (40);
			r->r_cntrl1 = 0;	/* reset formatter first */
			r->r_cntrl1 = OMD_RD_STAT;
			splx (s);
			while ((r->r_disr & OMD_CMD_COMPL) == 0)
				;
			track = (r->r_csr_h << 8) | r->r_csr_l;
			rel_trk = c->c_blkno - track;
			if (rel_trk <= 7 && rel_trk >= -8 && rel_trk) {
				c->c_dcmd = OMD1_RJ | (rel_trk & 0xf);
			} else
				c->c_dcmd = OMD1_SEK | (c->c_blkno & 0xfff);
			c->c_flags |= CF_ISSUE_D_CMD;
			od_issue_cmd (c, d);
			c->c_next_fsm = FSM_WORKED;
			return;
	
		case OMD_SPINUP:
			if (od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR) ==
			    -1)
				goto failed;
			c->c_dcmd = OMD1_RID;
			c->c_flags |= CF_ISSUE_D_CMD;
			if (od_issue_cmd (c, d) < 0)
				goto failed;
			c->c_next_fsm = FSM_INSERT_RID;
			return;

		case OMD_EJECT:

			/* update bitmap before actually ejecting! */
			if (v->v_flags & VF_UPDATE) {
dbug_vol (("eject kt ", 0, 0, 0, 0, 0));
				v->v_flags |= VF_EJECT;
				od_update_time = KT_BUSY;
				kernel_thread_noblock (kernel_task, od_update_thread);
				goto worked;
			}

			/* if not inserted then already ejected */
			if ((v->v_flags & VF_INSERTED) == 0) {
dbug_vol (("*ALREADY EJECTED* "));
				goto eject;
			}

			/* keep root drive empty long enough for insert */
			if (major(rootdev) == od_blk_major && v == &od_vol[0])
				od_runout_time = RUNOUT_EJECT;

			/* fall through ... */

		case OMD_EJECT_NOW:
dbug_vol (("eject NOW ", 0, 0, 0, 0, 0));
dbug_q (("EJECT v%d\n", d->d_vol - od_vol, 0, 0, 0, 0));
			d->d_flags |= DF_EJECTING;
			c->c_flags |= CF_ERR_INPROG;
			c->c_dcmd = OMD1_EC;
			c->c_flags |= CF_ISSUE_D_CMD;
			od_issue_cmd (c, d);
			c->c_next_fsm = FSM_EJECT;
			return;

		case OMD_SPIRAL_OFF:
			c->c_dcmd = OMD1_SOF;
			c->c_flags |= CF_ISSUE_D_CMD;
			od_issue_cmd (c, d);
			c->c_next_fsm = FSM_WORKED;
			return;

		case OMD_RESPIN:
			od_reset (c, d, r);
			return;

		default:
			c->c_err = E_CMD;
			c->c_flags |= CF_ERR_VALID;
			goto failed;
		}

		/* make sure we are spiraling */
		if ((d->d_flags & DF_SPIRALING) == 0) {
			c->c_dcmd = OMD1_SOO;
			c->c_flags |= CF_ISSUE_D_CMD;
			c->c_flags &= ~CF_ISSUE_F_CMD;

			/* resume at current state */
			c->c_next_fsm = c->c_fsm;
			d->d_flags |= DF_SPIRALING;
			break;
		}
		dbug_xfer (("xfer %d:%d %d ",
			c->c_track, c->c_sect, c->c_xfer, 0, 0));

		if (c->c_flags & CF_CUR_POS)
			dbug_xfer (("cur_pos %d:%d ",
				c->c_cur_track, c->c_cur_sect, 0, 0, 0));
#if	USE_RJ
		/*
		 *  Don't use RJ cmd during retries because they don't
		 *  seem to work when debugging (ipl7) or if the ipl is
		 *  high for some other reason.
		 */
		if (c->c_retry || c->c_rtz)
			goto seek;

		/*
		 *  Don't use RJ cmd on verify-head to read-head transitions
		 *  because, if a sector timeout occurs, the next SEH will
		 *  hang because of a Canon firmware bug.  Same thing for
		 *  erase-head to erase-head transitions.
		 */
		if ((c->c_flags & CF_CUR_POS) &&
		    !(d->d_cur_head == OMD1_SVH && c->c_head == OMD1_SRH) &&
		    !(d->d_cur_head == OMD1_SEH && c->c_head == OMD1_SEH)) {
			/* compute sector offset to beginning of request */
			od_off = (c->c_track * l->dl_nsect + c->c_sect) -
				(c->c_cur_track * l->dl_nsect + c->c_cur_sect);

			/* determine number of relative tracks to jump */
			if (od_off >= 0)
				rel_trk = (od_off + spht) / l->dl_nsect - 1;
			else
				rel_trk = (od_off - (spht - 1)) / l->dl_nsect - 1;
			if (rel_trk > 7 || rel_trk < -8)
				goto seek;

			/*
			 *  Can't use relative jump (RJ) command if
			 *  too much time has past since acquiring
			 *  the head position and we're expected to hit
			 *  in less than one rotation.
			 */
			od_lastexp = event_get() - c->c_curpos_time;
			if (rel_trk && (od_off % l->dl_nsect) != 0) {
				if (od_lastexp > mi_expire)
					goto seek;

				/*
				 *  The time window for successful completion
				 *  of a relative jump command is smaller if
				 *  changing between heads.
				 */
				if (rel_trk /*c->c_head != d->d_cur_head */ &&
				    od_lastexp > mi_expire_sh)
					goto seek;
			}

			if (rel_trk /* && c->c_head != d->d_cur_head */ &&
			    od_lastexp > mi_expire_sh2)
				goto seek;

			/* jumping too many tracks causes additional delay */
			/* FIXME: rethink this whole thing! */
			if ((rel_trk >= 3 || rel_trk <= -3) /*
			    od_lastexp > mi_expire_jmp */ )
				goto seek;

			if (od_lastexp > mi_expire_max)
				goto seek;
			dbug_xfer (("off %d rel_trk %d ",
				od_off, rel_trk, 0, 0, 0));
			c->c_dcmd = OMD1_RJ;
			if (rel_trk) {
				c->c_dcmd |= rel_trk & 0xf;
				c->c_flags |= CF_ISSUE_D_CMD;
			}
			if (1 /* c->c_head != d->d_cur_head */) {
				c->c_dcmd |= rj_head;

				/* FIXME: wrong if RJ fails! */
				d->d_cur_head = c->c_head;
				c->c_flags |= CF_ISSUE_D_CMD;
				od_lastmode = 1;
			} else
				od_lastmode = 0;

			c->c_flags |= CF_ISSUE_F_CMD;
			c->c_next_fsm = od_next_state[c->c_fcmd];
			goto count_seek;
		}
#endif	USE_RJ
seek:
		/* select head mode & seek to track */
		track = c->c_track;
		use_rj = 0;
		if (1 || c->c_sect < spht || c->c_retry) {
			track -= od_land;	/* land before first half-track */
			if (c->c_flags & CF_50_50_SEEK)
				use_rj = 1;
			c->c_flags &= ~CF_50_50_SEEK;
		} else {
			c->c_flags |= CF_50_50_SEEK;
		}

#if 0
		/* HOS should complete almost immediately */
		if (1 /* d->d_last_hos != track >> 12 */) {
			dbug_xfer (("HOS %d ", track>>12, 0, 0, 0, 0));
			od_drive_cmd (c, d, OMD1_HOS | (track >> 12),
				SPIN|DONT_INTR);
			d->d_last_hos = track >> 12;
		}
		if (1 /* c->c_head != d->d_cur_head */) {
#endif

			/* select head before seeking */
			c->c_seek_track = track;
			c->c_dcmd = c->c_head;
			c->c_flags |= CF_ISSUE_D_CMD;
			c->c_flags &= ~CF_ISSUE_F_CMD;
			dbug_xfer (("sel head ", 0, 0, 0, 0, 0));
			c->c_next_fsm = FSM_SEEK;

			/* FIXME: wrong if sel head fails! */
			d->d_cur_head = c->c_head;
#if 0
		} else {
			dbug_xfer (("seek %d (head ok) ", track, 0, 0, 0, 0));
			if (use_rj) {
				c->c_dcmd = OMD1_RJ | (od_land & 0xf);
				use_rj = 0;
			} else
				c->c_dcmd = OMD1_SEK | (track&0xfff);
			c->c_flags |= CF_ISSUE_D_CMD | CF_ISSUE_F_CMD;
			c->c_next_fsm = od_next_state[c->c_fcmd];
		}
#endif
count_seek:
		/* seek position indicator */
#if	EVENTMETER
		if (d->d_flags & DF_ONLINE) {
			event_disk (EM_OD, EM_Y_OD,
				c->c_track - v->v_mi->mi_reserved_cyls,
				l->dl_ncyl * l->dl_ntrack,
				c->c_cmd == OMD_WRITE? EM_WRITE : EM_READ);
		}
#endif	EVENTMETER
		if ((dk = d->d_bd->bd_dk) >= 0)
			dk_seek[dk]++;

		/* need to establish a new position on completion */
		c->c_flags &= ~CF_CUR_POS;
		break;

	case FSM_SEEK:

		/* FIXME: always send HOS */
		od_drive_cmd (c, d, OMD1_HOS | (c->c_seek_track >> 12),
			SPIN|DONT_INTR);
		dbug_xfer (("SEEK %d ", c->c_seek_track, 0, 0, 0, 0));
		c->c_dcmd = OMD1_SEK | (c->c_seek_track & 0xfff);
		c->c_flags |= CF_ISSUE_D_CMD | CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		break;

	case FSM_WRITE:
		/* detect erase-only requests */
		if (c->c_cmd == OMD_ERASE || c->c_cmd == OMD_TEST)
			goto worked;
		c->c_fcmd = OMD_WRITE;
		c->c_flags |= CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		c->c_retry_fsm = FSM_WRITE_RETRY;
		goto cont;

	case FSM_VERIFY:
		if ((c->c_flags & CF_TESTING) == 0 && od_noverify)
			goto worked;
		c->c_fcmd = OMD_VERIFY;
		c->c_flags |= CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		c->c_retry_fsm = FSM_WRITE_RETRY;
		goto cont;

	case FSM_WRITE_RETRY:
		c->c_fcmd = OMD_WRITE;
		c->c_flags |= CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		c->c_retry_fsm = FSM_WRITE_RETRY;
		dirty = 1;

		/*
		 *  On retries, do only one sector at a time.
		 */
		if (l->dl_version != DL_V1)
			c->c_xfer = 1;
		goto cont;

	case FSM_TEST_WRITE:
		if (c->c_flags & CF_ERR_VALID) {
			od_fsm (c, d, FSM_TEST_DONE);
			return;
		}

		c->c_blkno = c->c_test_blkno;
		if (l->dl_version == DL_V1)
			c->c_nsect = spht;
		else
			c->c_nsect = 1;

		if (++c->c_test_pass > od_test_passes) {

			/* might as well pre-erase it too */
			c->c_cmd = OMD_ERASE;
			c->c_fsm_test = FSM_TEST_ERASE;
			od_fsm (c, d, FSM_START);
			return;
		}

		for (i = 0; i < c->c_nsect * l->dl_secsize / sizeof(int); i++)
			((int*)c->c_tbuf)[i] = od_pattern[c->c_test_pass-1];
		c->c_fsm_test = FSM_TEST_WRITE;
		od_fsm (c, d, FSM_START);
		return;

	case FSM_TEST_ERASE:
		if ((c->c_flags & CF_ERR_VALID) == 0) {

			/*
			 * FIXME: If we crash before the update
			 * we will retest and destroy what was
			 * written.  Can this be minimized?
			 * e.g. attempt read and replace?
			 */
			SET_BITMAP (SB_ERASED);
		}

		/* fall into ... */

	case FSM_TEST_DONE:

		/* if remap failed then abort request */
		if ((c->c_flags & CF_ERR_VALID) &&
		    od_remap (c, d, v, c->c_pblk) == 0) {
			c->c_flags &= ~CF_TESTING;
			goto failed;
		}
		c->c_flags &= ~(CF_TESTING | CF_ERR_VALID);

		/* resume original request */
		bp = disksort_first((ds_queue_t *)queue_first(&bc->bc_tab));
		od_setup (c, d, bp);
		return;

	case FSM_ERR_DONE:
		c->c_flags &= ~CF_ERR_INPROG;
		od_fsm (c, d, c->c_err_fsm_save);
		return;

	case FSM_ATTN_RDS:
		c->c_rds = (r->r_csr_h << 8) | r->r_csr_l;
		dbug_status (("rds 0x%x ", c->c_rds, 0, 0, 0, 0));
		if ((c->c_rds & 1) == 0)
			goto attn_rid;
		od_status (c, d, r, OMD1_RES, DONT_SPIN | INTR);
		c->c_next_fsm = FSM_ATTN_RES;
		return (OD_BUSY);

	case FSM_ATTN_RES:
		c->c_res = (r->r_csr_h << 8) | r->r_csr_l;
		dbug_status (("res 0x%x ", c->c_res, 0, 0, 0, 0));
		if ((c->c_res & 1) == 0)
			goto attn_rid;
		od_status (c, d, r, OMD1_RHS, DONT_SPIN | INTR);
		c->c_next_fsm = FSM_ATTN_RHS;
		return (OD_BUSY);

	case FSM_ATTN_RHS:
		c->c_rhs = (r->r_csr_h << 8) | r->r_csr_l;
		dbug_status (("rhs 0x%x ", c->c_rhs, 0, 0, 0, 0));
attn_rid:
		if ((c->c_flags & CF_ERR_VALID) == 0 && c->c_rhs) {
			for (i = 15; i >= 0; i--)
				if (c->c_rhs & (1 << i)) {
					e = &od_err[E_HARDWARE_BASE+i];
					if (e->e_action != EA_ADV) {
						c->c_err = E_HARDWARE_BASE+i;
						c->c_flags |= CF_ERR_VALID;
						dbug_status (("-ERR- %d ",
							c->c_err, 0, 0, 0, 0));
						break;
					}
				}
		}
		if ((c->c_flags & CF_ERR_VALID) == 0 && (c->c_res & 0xfffe)) {
			for (i = 15; i > 0; i--)
				if (c->c_res & (1 << i)) {
					e = &od_err[E_EXTENDED_BASE+i-1];
					if (e->e_action != EA_ADV) {
						c->c_err = E_EXTENDED_BASE +
							i - 1;
						c->c_flags |= CF_ERR_VALID;
						dbug_status (("-ERR- %d ",
							c->c_err, 0, 0, 0, 0));
						break;
					}
				}
		}
		if ((c->c_flags & CF_ERR_VALID) == 0 && (c->c_rds & 0xfffe)) {
			for (i = 15; i > 0; i--)
				if (c->c_rds & (1 << i)) {
					e = &od_err[E_STATUS_BASE+i-1];
					if (e->e_action != EA_ADV) {
						c->c_err = E_STATUS_BASE +
							i - 1;
						c->c_flags |= CF_ERR_VALID;
						dbug_status (("ERR %d ",
							c->c_err, 0, 0, 0, 0));
						break;
					}
				}
		}
#if	DEBUG
		if (od_dbug & DBUG_STATUS)
			od_note (c, CF_ERR_VALID, od_xpr);
#endif	DEBUG

		/* RID must be issued to reset status bits and ATTN signal */
		while (od_drive_cmd (c, d, OMD1_RID, DONT_SPIN | INTR) == -1)
			;
		c->c_next_fsm = FSM_ATTN_RID;
		return (OD_BUSY);

	case FSM_ATTN_RID:

		/* indicate c_err is finally valid */
		c->c_fsm = c->c_attn_fsm_save;
		c->c_next_fsm = c->c_attn_next_fsm_save;
		dbug_status (("[%n/%n] ",
			c->c_fsm, od_dbug_fsm, c->c_next_fsm, od_dbug_fsm, 0));
		c->c_flags &= ~CF_ATTN_INPROG;

		/* safe to reenable ATTN interrupts */
		r->r_dimr |= OMD_ATTN;
		return (OD_ERR);

	case FSM_INSERT_RID:
		i = od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR);
		dbug_status (("{0x%x} ", i, 0, 0, 0, 0));
		if ((i & (1 << (E_STOPPED - E_STATUS_BASE + 1))) == 0)
			goto spinning;		/* already spinning */
		od_drive_cmd (c, d, OMD1_STM, DONT_SPIN | INTR);
		c->c_next_fsm = FSM_INSERT_STM;
		return (OD_BUSY);

	case FSM_INSERT_STM:
spinning:
		/* allow daemon to run on the cartridge just inserted */
		od_idle_time = KT_UNSCHED;

		/* can be called from od_cmd also */
		if ((c->c_flags & CF_ATTN_ASYNC) == 0)
			goto worked;	/* FIXME: what if it failed? */

		/* attach will happen on first open */
		c->c_flags &= ~(CF_ATTN_ASYNC | CF_ASYNC_INPROG);
		return (OD_DONE);

	case FSM_RESPIN_RST:
		od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR);
		od_drive_cmd (c, d, OMD1_RID, SPIN | DONT_INTR);
		od_drive_cmd (c, d, OMD1_STM, DONT_SPIN | INTR);
		c->c_next_fsm = FSM_RESPIN_STM;
		return;

	case FSM_RESPIN_STM:
		c->c_flags &= ~(CF_ERR_INPROG | CF_ERR_VALID);
		d->d_last_hos = -1;
		d->d_cur_head = -1;
		d->d_flags &= ~DF_SPIRALING;
		od_fsm (c, d, c->c_respin_fsm_save);
		return;

	case FSM_EJECT:
eject:
		d->d_flags &= ~(DF_ONLINE | DF_SPINNING | DF_EJECTING);
		if (v->v_flags & VF_EJECT) {
			v->v_flags &= ~VF_EJECT;
			wakeup (&v->v_flags);
		}
		v->v_flags &= ~(VF_INSERTED | VF_UPDATE);
		dbug_vol (("eject: v%d d%d ", v - od_vol, v->v_drive,
			0, 0, 0));
		c->c_flags &= ~(CF_ERR_INPROG | CF_ERR_VALID);
		if (c->c_flags & CF_FAIL_IO) {
			c->c_flags &= ~CF_FAIL_IO;
			goto failed;
		}
		goto worked;

	case FSM_WORKED:
worked:
		c->c_before = c->c_after = 0;
		c->c_nsect -= c->c_xfer;
		c->c_va += c->c_xfer * l->dl_secsize;
		c->c_blkno += c->c_xfer;

		/* restart fragmented requests (crossing alternates, etc.) */
		if (c->c_nsect > 0) {
			od_fsm (c, d, FSM_START);
			return;
		}

		if (c->c_flags & CF_ECC_SCAN) {
			c->c_flags &= ~CF_ECC_SCAN;
			if (c->c_save_nsect) {
				c->c_retry = 0;
				c->c_cmd = OMD_WRITE;
				c->c_nsect = c->c_save_nsect;
				od_fsm (c, d, FSM_START);
				return;
			}
		}

		/* resume a testing operation */
		if (c->c_flags & CF_TESTING)
			od_fsm (c, d, c->c_fsm_test);
		else
			busdone (bc);
		return;

	case FSM_FAILED:
failed:
		bp = disksort_first((ds_queue_t *)queue_first(&bc->bc_tab));
		od_perror (c, d, EF_FAILED, bp->b_blkno, c->c_pblk);
		bp->b_flags |= B_ERROR;

		if (c->c_flags & CF_READ_GATE) {
			r->r_frmr = od_frmr;
			c->c_flags &= ~CF_READ_GATE;
		}

		/* save internal error code for special I/O */
		if (bp == &v->v_cbuf)
			bp->b_error = c->c_err;
		busdone (bc);
		return;
	}

	/* setup sector info */
	r->r_track_h = c->c_track >> 8;
	r->r_track_l = c->c_track & 0xff;

	/* sector increment = 1 */
	if (l->dl_version != DL_V1 && (c->c_before || c->c_after))
		panic ("od: before/after");
	if (c->c_before)
		r->r_incr_sect = (1 << 4) | (c->c_sect - c->c_before);
	else
		r->r_incr_sect = (1 << 4) | c->c_sect;

	/* c_xfer = 256 will set r_seccnt = 0 */
	r->r_seccnt = c->c_xfer + c->c_before + c->c_after;
	dbug_cmd (("<%d:%d secnt %d> ",
		(r->r_track_h << 8) | r->r_track_l, r->r_incr_sect & 0xff, r->r_seccnt, 
		0, 0));
	if (od_issue_cmd (c, d) < 0)
		goto failed;
}
#define OD_SECT_TO_WAIT		100000		/* us to wait for sector n-1 */

od_sect_to (c, d)
	register volatile struct ctrl *c;
	register struct drive *d;
{
	register volatile struct od_regs *r = c->c_od_regs;
	int seek_track, s, od_id_r_save, track, sect, i;
	
	/*
	 *  Position head just after sector N-1.  Do this by setting up
	 *  a read wthout DMA enabled and wait for r_seccnt to be
	 *  decremented just after the N-1 sector ID is read.
	 */
	track = c->c_track;
	sect = c->c_sect;
	if (c->c_sect == 0) {
		r->r_track_h = (c->c_track - 1) >> 8;
		r->r_track_l = (c->c_track - 1) & 0xff;
		seek_track = c->c_track - 2;
		r->r_incr_sect = (1 << 4) | 15;
	} else {
		r->r_track_h = c->c_track >> 8;
		r->r_track_l = c->c_track & 0xff;
		seek_track = c->c_track - 1;
		r->r_incr_sect = (1 << 4) | (c->c_sect - 1);
	}
	r->r_seccnt = 1;
	od_drive_cmd (c, d, OMD1_HOS | (seek_track >> 12), SPIN|DONT_INTR);
	s = spl7();
	od_drive_cmd (c, d, OMD1_SEK | (c->c_seek_track & 0xfff), 
		SPIN|DONT_INTR);
	r->r_dimr &= ~(OMD_ECC_DONE | OMD_OPER_COMPL | OMD_CMD_COMPL);
	r->r_initr = (c->c_initr & ~OMD_ID_MASK) | od_id_r;
	r->r_cntrl2 = OMD_SECT_TIMER | (d - od_drive) | OMD_ECC_DIS;
	r->r_cntrl1 = 0;
	r->r_cntrl1 = OMD_READ;
	for (i = 0; i < OD_SECT_TO_WAIT; i++) {		/* 21-Aug-90 dmitch */
		delay (1);
		if (r->r_seccnt != 1)
			break;
	}
	r->r_disr = disr_shadow | OMD_CLR_INT;
	r->r_cntrl1 = 0;
	
	/* now do sector N with ID match shut off */
	r->r_track_h = track >> 8;
	r->r_track_l = track & 0xff;
	r->r_incr_sect = (1 << 4) | sect;
	r->r_seccnt = 1;
	c->c_flags &= ~CF_ISSUE_D_CMD;
	od_id_r_save = od_id_r;
	if(i == OD_SECT_TO_WAIT) 
		printf("od_sect_to: N-1 Not Found\n");
 	else
 		od_id_r = OMD_ID_0;
	od_issue_cmd (c, d);
	od_id_r = od_id_r_save;
	splx (s);
}

od_issue_cmd (c, d)
	register volatile struct ctrl *c;
	register struct drive *d;
{
	register volatile struct od_regs *r = c->c_od_regs;
	register volatile struct dma_chan *dcp = &c->c_dma_chan;
	register int direction;
	register struct volume *v = d->d_vol;

	dbug_flags (("issue: c_flags %R ",
		c->c_flags, od_dbug_c_flags, 0, 0, 0));
	if (c->c_flags & CF_ISSUE_F_CMD) {
#if	DEBUG
		if (od_dbug & DBUG_CMD)
			od_note (c, CF_ISSUE_F_CMD, od_xpr);
#endif	DEBUG
		if (c->c_fcmd & (OMD_ECC_READ | OMD_ECC_WRITE |
		    OMD_READ | OMD_WRITE)) {
			c->c_flags |= CF_NEED_DMA;
			if (c->c_fcmd & (OMD_ECC_READ|OMD_READ))
				direction = DMACSR_READ;
			else
				direction = DMACSR_WRITE;

			/* od_dma_intr shouldn't be called unless reading */
			dcp->dc_flags = (direction == DMACSR_READ)?
				DMACHAN_INTR | DMACHAN_DMAINTR : 0;

			/* start DMA */
			dbug_dma (("DMA va 0x%x size %d ",
				c->c_va, c->c_xfer*v->v_label->dl_secsize,
				0, 0, 0));
			dma_list (dcp, c->c_dma_list,
				((c->c_flags & CF_ECC_SCAN) &&
				c->c_fcmd == OMD_READ)?
				od_rathole : c->c_va,
				(c->c_xfer + c->c_before + c->c_after) *
				v->v_label->dl_secsize,
				((c->c_flags & CF_ECC_SCAN) &&
				c->c_fcmd == OMD_READ)?
				pmap_kernel() : c->c_pmap, direction, NDMAHDR,
				c->c_before * v->v_label->dl_secsize,
				c->c_after * v->v_label->dl_secsize);
			dcp->dc_hndlrarg = (int) c;
			dcp->dc_queue.dq_head =
			        (struct dma_hdr *volatile) c->c_dma_list;
			dma_enable (dcp, direction);
		}
	}

	c->c_drive = d - od_drive;	/* remember current drive # */
	if (c->c_flags & CF_ISSUE_D_CMD) {
		if (od_drive_cmd (c, d, c->c_dcmd, DONT_SPIN |
		    (c->c_flags & CF_ISSUE_F_CMD? DONT_INTR : INTR)) < 0)
			return (-1);
	} else {
		/* select drive */
		r->r_cntrl2 = OMD_SECT_TIMER | (d - od_drive);
	}

	/* formatter commands can be issued after drive command issues */
	if (c->c_flags & CF_ISSUE_F_CMD) {
		if ((c->c_flags & CF_ISSUE_D_CMD) == 0)
			od_block_async (c, d);
		c->c_timeout = REG_TIMEOUT;
		if (c->c_fcmd & (OMD_READ | OMD_ECC_READ)) {

			/* use DMA complete interrupt on reads */
			r->r_dimr &= ~(OMD_ECC_DONE | OMD_OPER_COMPL |
				OMD_CMD_COMPL);
			c->c_flags |= CF_DMAINT_EXP | CF_DEVINT_EXP;
			c->c_initr &= ~OMD_ID_MASK;
			c->c_initr |= od_id_r;
			r->r_initr = c->c_initr;
		} else
		if (c->c_fcmd & OMD_VERIFY) {

			/* wait for ECC check to complete on verifies */
			c->c_flags |= CF_DEVINT_EXP;
			r->r_dimr |= OMD_ECC_DONE;
			r->r_dimr &= ~(OMD_OPER_COMPL | OMD_CMD_COMPL);
			c->c_initr &= ~OMD_ID_MASK;
			c->c_initr |= od_id_v;
			r->r_initr = c->c_initr;
		} else {
			c->c_flags |= CF_DEVINT_EXP;
			r->r_dimr |= OMD_OPER_COMPL;
			r->r_dimr &= ~(OMD_ECC_DONE | OMD_CMD_COMPL);
			c->c_initr &= ~OMD_ID_MASK;
			c->c_initr |= od_id_o;
			r->r_initr = c->c_initr;
		}
		r->r_cntrl1 = 0;	/* reset formatter first */
		r->r_cntrl1 = c->c_fcmd;
	}
	return (0);
}

/* locate specified entry in bad block table for the current bitmap entry */
od_locate_alt (c, d, v, entry, pblk)
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct volume *v;
	register int entry;
{
	register int i, ag;
	register struct disk_label *l = v->v_label;
	int *bbt, nbad;
	int apag;			/* alternates per alt group */

	if (l->dl_version == DL_V1 || l->dl_version == DL_V2) {
		bbt = l->dl_bad;
		nbad = NBAD;
	} else {
		bbt = (int*)v->v_bad_block;
		nbad = NBAD_BLK;
	}
	apag = l->dl_ag_alts;
	if (l->dl_version == DL_V1)
		apag /= l->dl_nsect >> 1;
	ag = (pblk - v->v_base - l->dl_front) / l->dl_ag_size;
	dbug_remap (("locate a %d pblk %d ag %d ",
		entry, pblk - v->v_base, ag, 0, 0));
	
	/* search alternate group first -- don't let index exceed nbad! */
	if (ag >= 0 && ag < l->dl_ngroups && (ag * apag + apag) < nbad) {
		ag *= apag;
		for (i = ag; i < ag + apag; i++)
			if (bbt[i] == entry) {
				dbug_remap (("found @ alt #%d ",
					i, 0, 0, 0, 0));
				return (i);
			}
	}

	/* if alternate group is full then take any entry */
	for (i = 0; i < nbad && bbt[i] != -1; i++) {
		if (bbt[i] == entry) {
			dbug_remap (("found anywhere @ %d ", i, 0, 0, 0, 0));
			return (i);
		}
	}
	dbug_remap (("*not found* ", 0, 0, 0, 0, 0));
	return (-1);
}

/* remap the current bad bitmap entry to an alternate */
od_remap (c, d, v, pblk)
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct volume *v;
{
	register int i, bme, offset, shift, t;
	register struct disk_label *l = v->v_label;
	int norm_sect = 0, *bbt;

	if (l->dl_version == DL_V1 || l->dl_version == DL_V2) {
		bbt = l->dl_bad;
	} else {
		bbt = (int*)v->v_bad_block;
	}
	if (l->dl_version == DL_V1)
		norm_sect = c->c_sect % (l->dl_nsect >> 1);
	if ((i = od_locate_alt (c, d, v, 0, pblk)) == -1) {
		/* out of alternates! */
		c->c_err = E_BAD_REMAP;
		return (0);
	}
	bbt[i] = pblk - v->v_base - norm_sect;
	dbug_remap (("*REMAP* %d to alt #%d\n",
		bbt[i], i, 0, 0, 0));

	/* mark the bitmap EXACTLY where the error occured */
	if (l->dl_version != DL_V1) {
		bme = pblk - v->v_base;
		offset = bme >> 4;
		shift = (bme & 0xf) << 1;
	}
	t = v->v_bitmap[offset];
	v->v_flags |= VF_UPDATE;
	if (od_update_time == KT_UNSCHED)
		od_update_time = KT_SCHED;
	t &= ~(3 << shift);
	v->v_bitmap[offset] = t | (SB_BAD << shift);
	dbug_bitmap (("remap <%d,%d,%d>", bme, offset, shift));
#if 0
	/* update disk info right now since remaps don't happen that often */
	od_update_time = KT_BUSY;
	kernel_thread_noblock (kernel_task, od_update_thread);
#endif
	return (1);
}

/* block against any asynchronous interrupts in progress */
od_block_async (c, d)
	register volatile struct ctrl *c;
	register struct drive *d;
{
	register int s;

	/* don't sleep if called from interrupt level */
	/* FIXME: make this work during autoconf!!! */
	if (1 || c->c_flags & CF_CANT_SLEEP)
		return;

	s = spln (od_spl);
	while (c->c_flags & CF_ATTN_INPROG) {
		c->c_flags |= CF_ATTN_WAKEUP;
		sleep (c + CF_ATTN_INPROG, PRI_OMD);
	}
	splx (s);
}

od_drive_cmd (c, d, cmd, flags)
	register volatile struct ctrl *c;
	register struct drive *d;
{
	register volatile struct od_regs *r = c->c_od_regs;
	register int i, j, s;

	od_block_async (c, d);

	/* select drive */
	r->r_cntrl1 = 0;	/* make sure formatter is reset first */
	r->r_cntrl2 = OMD_SECT_TIMER | (d - od_drive);
	DELAY(2);	/* wait for CMD_COMPL to be synchronized */

#define	TOUT	(8*1024)
	for (i = 1, j = 1; i <= TOUT && (r->r_disr & OMD_CMD_COMPL) == 0; i++)
		delay (1);
		if ((i % j) == 0) {
#if	notdef
	printf ("od_drive_cmd: %d busy1? disr 0x%x c2 0x%x drive %d\n",
		i, r->r_disr, r->r_cntrl2, d - od_drive);
#endif
			r->r_cntrl1 = 0;
			j <<= 1;
		}
	if (i > TOUT) {
		dbug_status (("od_drive_cmd: E_BUSY_TIMEOUT1\n",
			0, 0, 0, 0, 0));
		c->c_err = E_BUSY_TIMEOUT1;
		c->c_timeout = 0;
		return (-1);
	}
	c->c_last_dcmd = cmd;
#if	DEBUG
	if (od_dbug & DBUG_CMD) {
		od_note (c, CF_ISSUE_D_CMD, od_xpr);
		dbug_cmd (("trk/sec %d:%d secnt %d\n",
			(r->r_track_h << 8) | r->r_track_l, r->r_incr_sect & 0xff,
			r->r_seccnt, 0, 0));
	}
#endif	DEBUG
	s = spl7();		/* FIXME */
	r->r_csr_h = cmd >> 8;
	r->r_csr_l = cmd & 0xff;	/* triggers on write of low byte */
	for (i = 1, j = 1; i <= TOUT && (r->r_disr & OMD_CMD_COMPL) == 1; i++)
		delay (1);
		if ((i % j) == 0) {
#ifdef	notdef
			printf ("od_drive_cmd: %d busy2? disr 0x%x c1 0x%x\n",
				j, r->r_disr, r->r_cntrl1);
#endif
			r->r_cntrl1 = 0;
			j <<= 1;
		}
	splx (s);
	if (i > TOUT) {
		dbug_status (("od_drive_cmd: E_BUSY_TIMEOUT2\n",
			0, 0, 0, 0, 0));
		c->c_err = E_BUSY_TIMEOUT2;
		c->c_timeout = 0;
		return (-1);
	}
#undef	TOUT

	/* OMD_CMD_COMPL goes away instantly, so can safely set dimr here */
	if (flags & (SPIN|DONT_INTR))
		r->r_dimr &= ~(OMD_ECC_DONE | OMD_CMD_COMPL | OMD_OPER_COMPL);
	else {
		c->c_flags |= CF_DEVINT_EXP;
		r->r_dimr |= OMD_CMD_COMPL;
		r->r_dimr &= ~(OMD_ECC_DONE | OMD_OPER_COMPL);

		/* longer timeout for start/stop & eject */
		if (cmd == OMD1_SPM || cmd == OMD1_STM || cmd == OMD1_EC)
			c->c_timeout = LONG_TIMEOUT;
		else
			c->c_timeout = REG_TIMEOUT;
	}
	if (flags & SPIN)
		while ((r->r_disr & OMD_CMD_COMPL) == 0)
			;	/* FIXME: timeout & recover? */
	return (0);
}

/*
 * Called by generic DMA routines when a DMA operation completes.
 * If the operation was a read (CF_DMAINT_EXP set) then call odintr()
 * to complete the request.  This is called at soft interrupt level
 * (not DMA level) so it's safe to spl to the device interrupt level
 * before calling odintr.
 */
void
od_dma_intr (c)
	register volatile struct ctrl *c;
{
	register volatile struct dma_chan *dcp = &c->c_dma_chan;
	register int s;

	/* may also get called on DMA errors */
	dbug_dma (("DMA_INTR: c 0x%x ", c, 0, 0, 0, 0));
	if (dcp->dc_flags & DMACHAN_ERROR)
		c->c_flags |= CF_DMA_ERROR | CF_DMAINT_EXP;
	if (c->c_flags & CF_DMAINT_EXP) {
		s = spln (od_spl);
		c->c_flags &= ~CF_DMAINT_EXP;
		odintr (c);
		splx (s);
	}
}

void
od_dmaintr (c)
	register volatile struct ctrl *c;
{
	struct bus_ctrl *bc = odcinfo[c - od_ctrl];
	ds_queue_t *q = (ds_queue_t *)queue_first(&bc->bc_tab);
	register struct buf *bp = disksort_first(q);
	register struct drive *d;

	d = &od_drive[od_vol[OD_VOL(bp->b_dev)].v_drive];
	c->c_curpos_time = event_get() - mi_rv_delay;
}
#define DMA_INIT_ON_ERR	1

odintr (c)
	register volatile struct ctrl *c;
{
	struct bus_ctrl *bc = odcinfo[c - od_ctrl];
	register struct buf *bp;
	register struct drive *d = &od_drive[c->c_drive];
	register struct volume *v;
	struct disk_label *l;
	register struct od_err *e;
	register volatile struct od_regs *r = c->c_od_regs;
	register int i, disr, desr, bno, err, nsect_ok, bytes, blkno, pblk;
	int retry_max, rtz_max;
	struct od_stats *s = (struct od_stats*) od_stats.s_stats;
	register volatile struct dma_chan *dcp = &c->c_dma_chan;
	struct dma_hdr *dhp;
	int sw;
	
	dbug_intr (("INTR: ", 0, 0, 0, 0, 0));
	c->c_flags |= CF_CANT_SLEEP;

	if ((c->c_flags & (CF_SOFT_TIMEOUT | CF_DMA_ERROR)) == 0) {

		/* clearing the interrupt clears status bits so cache them */
		disr = r->r_disr;
		dbug_status (("disr %R dimr 0x%x desr 0x%x ", disr,
			od_dbug_disr, r->r_dimr, r->r_desr, 0, 0));
		desr = r->r_desr;
		sw = spldma();
		r->r_disr = disr_shadow | OMD_CLR_INT;	/* clear interrupt */
		splx(sw);
		r->r_dimr &= ~OMD_CMD_COMPL;	/* prevent further cmd ints */
		dbug_intr (("@ %d:%d:%d\n",
			(r->r_track_h << 8) | r->r_track_l,
			r->r_incr_sect, r->r_seccnt, 0, 0));
	}

	/*
	 * An unexpected interrupt may be an asynchronous attention.
	 * If so, any new request must be kept from issuing until the
	 * the attention status is acquired.  Routines that issue
	 * drive commands must inspect CF_ATTN_INPROG and sleep on
	 * CF_ATTN_WAKEUP.  If the attention occurs after a command
	 * is issued the command will keep others from issuing.
	 */
	if ((c->c_flags & CF_DEVINT_EXP) == 0) {
		if ((i = od_attn (c, d, r, disr)) == OD_DONE) {
			if (c->c_flags & CF_SOFT_TIMEOUT)
				c->c_flags &= ~CF_SOFT_TIMEOUT;
			printf ("odintr: unexpected device interrupt\n");
		} else
		if (i == OD_BUSY)
			c->c_flags |= CF_ATTN_ASYNC;
		goto out;
	}

	c->c_flags &= ~CF_DEVINT_EXP;
	c->c_timeout = 0;

	/*
	 * Determine error code.  Since all error bits are presented
	 * in parallel we scan them in priority order and report the
	 * first error encountered.  FIXME: is there a better way?
	 * FIXME: this doesn't work at all if there are coupled error bits!
	 */
	err = 0;
	if (c->c_flags & CF_SOFT_TIMEOUT) {
		err = E_SOFT_TIMEOUT;
		c->c_flags &= ~CF_SOFT_TIMEOUT;
#if	USE_RJ
if (_rj_fail)
	printf ("off %d lastexp %d lastmode %d RJ 0x%x\n",
			_off, _lastexp, _lastmode, _dcmd);
#endif	USE_RJ
	} else
	if (c->c_flags & CF_DMA_ERROR) {
		err = E_DMA_ERROR;
		c->c_flags &= ~CF_DMA_ERROR;
	} else
	if (disr & OMD_DATA_ERR) {
		if (desr & OMD_ERR_STARVE)
			err = E_STARVE;
		else
		if (desr & OMD_ERR_TIMING)
			err = E_TIMING;
		else
		if (desr & OMD_ERR_CMP)
			err = E_CMP;
		else
		if (desr & OMD_ERR_ECC)
			err = E_ECC;
		else
			printf ("od: DATA_ERR but no desr?\n");
	} else
	if (disr & OMD_TIMEOUT) {

		/* FIXME: spiraling may have been lost */
		d->d_flags &= ~DF_SPIRALING;
		d->d_last_hos = -1;	/* in case HOS is screwed up */
		err = E_TIMEOUT;
		dbug_timeout (("T/O @ %d:%d wanted %d:%d ",
			(r->r_track_h << 8) | r->r_track_l, r->r_incr_sect,
			c->c_track, c->c_sect, 0));
	} else
	if (disr & OMD_READ_FAULT)
		err = E_FAULT;
	else
	if (disr & OMD_PARITY_ERR)
		err = E_I_PARITY;

	if (err && (c->c_flags & CF_ERR_VALID) == 0) {
		c->c_err = err;
		c->c_flags |= CF_ERR_VALID;
	}

#if 1
	/* clear formatter (only really needed on ECC errors) */
	r->r_cntrl1 = 0;
#endif

	if ((i = od_attn (c, d, r, disr)) == OD_BUSY)
		goto out;
	
	/* certain errors take precedence over others */
	if (c->c_rds & (1 << (E_INSERT - E_STATUS_BASE + 1)))
		c->c_err = E_INSERT;
	if (c->c_rds & (1 << (E_EMPTY - E_STATUS_BASE + 1)))
		c->c_err = E_EMPTY;
	
	if (c->c_flags & CF_ATTN_ASYNC) {
		od_async_attn (c, d, blkno, pblk);
		c->c_flags &= ~CF_ERR_VALID;
		goto out;
	}

	if (c->c_flags & CF_ATTN_WAKEUP) {
		wakeup (c + CF_ATTN_INPROG);
		c->c_flags &= ~CF_ATTN_WAKEUP;
	}

	if (queue_empty(&bc->bc_tab)) {
		printf ("odintr: null bc_tab.b_actf?\n");
		goto out;
	}
	bp = disksort_first((ds_queue_t *)queue_first(&bc->bc_tab));
	if (bp == NULL) {
		printf ("odintr: null bp?\n");
		goto out;
	}
	blkno = bp->b_blkno;
	pblk = c->c_pblk;
	v = &od_vol[OD_VOL(bp->b_dev)];
	l = v->v_label;
	
	/* resume an error recovery operation */
	/* FIXME: what do we do about errors during error recovery? */
	if (c->c_flags & (CF_ERR_INPROG | CF_ATTN_INPROG)) {
		od_fsm (c, d, c->c_next_fsm);
		goto out;
	}

	if ((c->c_flags & CF_ERR_VALID) == 0) {

		/* compute current position of the head */
		if (l && (c->c_flags & CF_ISSUE_F_CMD)) {
			bno = c->c_pblk + c->c_xfer + c->c_before + c->c_after;
			c->c_cur_sect = bno % l->dl_nsect;
			c->c_cur_track = bno / l->dl_nsect;
			c->c_flags |= CF_CUR_POS;
			if (c->c_fcmd & OMD_VERIFY)
				c->c_curpos_time = event_get() - mi_rv_delay;
			else
			if ((c->c_fcmd & (OMD_ECC_READ|OMD_READ)) == 0)
				c->c_curpos_time = event_get();
		}

		/* undo read gate, remap sector & copy data */
		if ((c->c_flags & CF_READ_GATE) &&
		    (c->c_flags & CF_ISSUE_F_CMD) && c->c_fcmd == OMD_READ) {
			r->r_frmr = od_frmr;
			c->c_flags &= ~CF_READ_GATE;
		}
		
		/* record number of ECC corrections during reads */
		if ((c->c_fcmd & (OMD_ECC_READ | OMD_READ)) &&
		    (c->c_flags & CF_NEED_DMA) && r->r_ecccnt) {
			struct disk_stats *s = &od_stats;

			dbug_stats (("ECC corr %d ",
				r->r_ecccnt, 0, 0, 0, 0));
			if (r->r_ecccnt > s->s_maxecc)
				s->s_maxecc = r->r_ecccnt;
			if (s->s_ecccnt == 0)
				s->s_ecccnt = r->r_ecccnt;
			else
				s->s_ecccnt = (s->s_ecccnt + r->r_ecccnt) / 2;
			if (bp == &v->v_cbuf)
				bp->b_resid = r->r_ecccnt;
		}
		c->c_flags &= ~CF_NEED_DMA;

		/*
		 *  Detect scan failure.  If a sector is untested then
		 *  remap it the very first time the maxecc threshold is
		 *  exceeded.  On subsequent writes require that the ECC
		 *  threshold be exceeded on three consecutive read scans
		 *  and only remap after an addition write/scan pass fails.
		 *  This strategy prevents spurious remaps if the ECC
		 *  correction count is noisy.
		 */
		if ((c->c_flags & CF_ECC_SCAN) &&
		    (c->c_flags & CF_ISSUE_F_CMD) &&
		    c->c_fcmd == OMD_READ) {
#if	SCAN_RETRY
			if (r->r_ecccnt > ((c->c_flags & CF_UNTESTED)?
			    od_maxecc : od_maxecc2)) {
#else	SCAN_RETRY
			dbug_ecc (("ECC correction count %d block %d phys block %d ",
				r->r_ecccnt, blkno, pblk - v->v_base, 0, 0));
			dbug_ecc (("(%d:%d:%d)\n", pblk / l->dl_nsect, 0,
				pblk % l->dl_nsect, 0, 0));
			if (r->r_ecccnt > od_maxecc) {
#endif	SCAN_RETRY
				/*
				 *  If the ECC correction count falls between
				 *  od_maxecc_l to od_maxecc_h then allow a
				 *  write retry.
				 */
				if (((r->r_ecccnt < od_maxecc_l1 ||
				    r->r_ecccnt > od_maxecc_h1) &&
				    (r->r_ecccnt < od_maxecc_l2 ||
				    r->r_ecccnt > od_maxecc_h2)) ||
				    c->c_scan_rewrite > od_write_retry) {
					c->c_err = E_EXCESSIVE_ECC;
					goto error;
				} else {
					c->c_err = E_EXCESSIVE_ECC;
					od_perror (c, d, EF_RETRY,
						blkno, pblk);
					c->c_retry = 0;
					c->c_cmd = OMD_WRITE;
					od_fsm (c, d, FSM_START);
					goto out;
				}
#if	SCAN_RETRY
				if (c->c_retry >= 3) {
					if (c->c_scan_rewrite > 1) {
						c->c_err = E_EXCESSIVE_ECC;
						goto error;
					} else {
					}
				}
				if (c->c_err_pblk == -1 ||
				    c->c_err_pblk == c->c_pblk) {
					c->c_err_pblk = c->c_pblk;
					c->c_retry++;
				}
				od_fsm (c, d, FSM_START);
				goto out;
			} else {
				if (c->c_err_pblk == c->c_pblk) {
					c->c_err_pblk = -1;
					c->c_retry = 0;
				}
#endif	SCAN_RETRY
			}
		}

		/* scan for errors if excessive ECC errors during verify */
		if ((c->c_fcmd & OMD_VERIFY) &&
		    (c->c_flags & CF_ISSUE_F_CMD) && l->dl_version != DL_V1 &&
		    (!((c->c_flags & CF_PHYS) &&
		    (v->v_flags & VF_IGNORE_BITMAP))) &&
#if	SCAN_RETRY
		    r->r_ecccnt > ((c->c_flags & CF_UNTESTED)?
		    od_maxecc : od_maxecc2) {
#else	SCAN_RETRY
		    r->r_ecccnt > od_maxecc) {
#endif	SCAN_RETRY
			c->c_err = E_EXCESSIVE_ECC;
			od_perror (c, d, EF_RESCAN, blkno, pblk);
			
			/* remember any fragment the first time through */
			if ((c->c_flags & CF_ECC_SCAN) == 0) {
				c->c_save_nsect = c->c_nsect - c->c_xfer;
				c->c_nsect = c->c_xfer;
			}
			c->c_flags |= CF_ECC_SCAN;
			c->c_retry = 0;
			c->c_scan_rewrite++;
#if	SCAN_RETRY
			c->c_err_pblk = -1;
#endif	SCAN_RETRY
			c->c_cmd = OMD_READ;
			od_fsm (c, d, FSM_START);
			goto out;
		}

#if	USE_RJ
if ((c->c_flags & CF_ISSUE_D_CMD) && (c->c_dcmd & 0xff00) == OMD1_RJ)
	_rj_fail = 0;
#endif	USE_RJ

		/* command completed successfully -- advance state machine */
		dbug_xfer (("*WORKED* ", 0, 0, 0, 0, 0));
		od_fsm (c, d, c->c_next_fsm);
		goto out;
	}
error:
	/* process errors */
	dbug_xfer (("*ERROR* %d ", c->c_err, 0, 0, 0, 0));
	if (c->c_flags & CF_ECC_SCAN) {
		c->c_cmd = OMD_WRITE;
		c->c_nsect += c->c_save_nsect;
		c->c_flags &= ~CF_ECC_SCAN;
	}

	/* abort the DMA if there was an error */
	if (c->c_flags & CF_NEED_DMA) {
		dma_abort (&c->c_dma_chan);
		c->c_flags &= ~(CF_DMAINT_EXP|CF_NEED_DMA);
		/* 
		 * DMA hang kludge. This is per Kevin Grundy's suggestion,
		 * but it is ineffectual as of 22-Aug-90. Left in for future
		 * analysis.
		 */
#ifdef	DMA_INIT_ON_ERR
		r->r_cntrl1 = 0;
		od_dma_init((struct ctrl *)c);
#endif	DMA_INIT_ON_ERR
	}

	/* figure out how far we went before an error occured on a write */
	nsect_ok = 0;
	if ((c->c_flags & CF_ISSUE_F_CMD) && c->c_cmd == OMD_WRITE)
	switch (c->c_fcmd) {
	
	case OMD_ERASE:
	case OMD_WRITE:
	case OMD_VERIFY:
		nsect_ok = c->c_xfer - r->r_seccnt - 1;
		if (c->c_err == E_TIMEOUT)
			nsect_ok++;
		break;
	}
	
	/* point at sector that caused error */
	if (nsect_ok) {
		blkno += nsect_ok;
		pblk += nsect_ok;
	}
	c->c_flags &= ~CF_ERR_VALID;
	switch ((e = &od_err[c->c_err])->e_action) {

	case EA_RETRY:

		/* abort a testing operation */
		/* FIXME: ignore first sector timeout for now */
		if ((c->c_flags & CF_TESTING) &&
		    (c->c_err != E_TIMEOUT || c->c_retry)) {
			od_perror (c, d, EF_FAILED, blkno, pblk);
			c->c_flags |= CF_ERR_VALID;
			od_fsm (c, d, c->c_fsm_test);
			goto out;
		}

		/*
		 *  Accept NO errors on erase/write/verify, but
		 *  retry after first sector timeout.
		 */
		if (c->c_cmd == OMD_WRITE &&
		    l->dl_version != DL_V1 && (!((c->c_flags & CF_PHYS) &&
		    (v->v_flags & VF_IGNORE_BITMAP))) &&
		    (c->c_err != E_TIMEOUT || c->c_retry)) {
			if (od_remap (c, d, v, pblk) != 0) {
				od_perror (c, d, EF_REMAP, blkno, pblk);
				od_fsm (c, d, FSM_START);
			} else
				od_fsm (c, d, FSM_FAILED);
			break;
		}
		if ((c->c_flags & CF_PHYS) && (v->v_flags & VF_SPEC_RETRY)) {
			retry_max = v->v_retry;
			rtz_max = v->v_rtz;
		} else {
			retry_max = e->e_retry;
			rtz_max = e->e_rtz;
		}
		if (c->c_flags & CF_READ_GATE) {
			retry_max = 4;
			rtz_max = 0;
		}

		/* keep stats on errors during verifies */
		if (c->c_fcmd == OMD_VERIFY && c->c_retry == 0)
			s->s_vfy_retry++;
		if (c->c_retry++ >= retry_max) {
			c->c_retry = 0;
			if (c->c_rtz++ >= rtz_max) {

				/*
				 *  Take special action for stubborn
				 *  sector timeout errors.
				 */
				if (c->c_err == E_TIMEOUT &&
				    l->dl_version != DL_V1 &&
				    c->c_fcmd == OMD_READ &&
				    c->c_xfer == 1) {
					od_perror (c, d, EF_FIXUP,
						blkno, pblk);
					c->c_retry = c->c_rtz = 0;
				        od_sect_to (c, d);
					break;
				}

				/*
				 *  As a last resort, try recovering
				 *  read data from uncorrectable ECC
				 *  errors by incrementing the read gate
				 *  and retrying.
				 */
				if ((c->c_err == E_ECC || c->c_err == E_PLL) &&
				    l->dl_version != DL_V1 &&
				    c->c_fcmd == OMD_READ &&
				    c->c_xfer == 1 &&
				    (od_dbug & DBUG_NO_HEROIC) == 0) {
					if (!(c->c_flags & CF_READ_GATE)) {
						c->c_read_gate =
							OMD_READ_GATE_MIN;
						c->c_flags |= CF_READ_GATE;
					}
					if (c->c_read_gate >
					    OMD_READ_GATE_MAX) {
						r->r_frmr = od_frmr;
						c->c_flags &=
							~CF_READ_GATE;
						goto fail;
					}
					r->r_frmr = (od_frmr &
					    ~OMD_READ_GATE_MASK) |
					    c->c_read_gate;
					c->c_read_gate++;
					c->c_retry = 1;
					od_perror (c, d, EF_RECOVER,
						blkno, pblk);
					goto retry;
				}
fail:
				if (c->c_fcmd == OMD_VERIFY)
					s->s_vfy_failed++;
				bp->b_blkno = blkno;
				c->c_pblk = pblk;
				od_fsm (c, d, FSM_FAILED);

				/* FIXME: go offline if unit not ready? */
				/* or is this obviated by eject capability? */
				break;
			} else {
				/* issue recalibrate */
				c->c_flags |= CF_ERR_INPROG;
				od_perror (c, d, EF_RTZ, blkno, pblk);
				od_drive_cmd (c, d, OMD1_REC, DONT_SPIN|INTR);
				c->c_err_fsm_save = c->c_retry_fsm;
				c->c_next_fsm = FSM_ERR_DONE;

				/*
				 * RTZ occasionally corrupts the drives
				 * idea of what the HOS is.
				 */
				d->d_last_hos = -1;
				d->d_flags &= ~DF_SPIRALING;
				goto out;
			}
		}

#if	USE_RJ
if (c->c_err == E_TIMEOUT && (c->c_dcmd & 0xff00) == OMD1_RJ &&
    (c->c_flags & CF_ISSUE_D_CMD)) {
	dbug_expire (("off %d lastexp %d lastmode %d RJ 0x%x\n",
		od_off, od_lastexp, od_lastmode, c->c_dcmd, 0));
	_rj_fail = 1;
	_off = od_off;
	_lastexp = od_lastexp;
	_lastmode = od_lastmode;
	_dcmd = c->c_dcmd;
}
#endif	USE_RJ

retry:
		/* retry the original command */
		od_perror (c, d, EF_RETRY, blkno, pblk);
		od_fsm (c, d, c->c_retry_fsm);
		break;

	case EA_RTZ:

		/* must RTZ immediately otherwise subsequent cmds will fail */
		if (c->c_retry++ > e->e_retry) {

			if (c->c_rtz++ > e->e_rtz) {
				bp->b_blkno = blkno;
				c->c_pblk = pblk;
				if (c->c_flags & CF_TESTING) {
					od_perror (c, d, EF_FAILED,
						blkno, pblk);
					c->c_flags |= CF_ERR_VALID;
					od_fsm (c, d, c->c_fsm_test);
					goto out;
				}
				od_fsm (c, d, FSM_FAILED);
				break;
			}
			c->c_retry = 0;
			od_perror (c, d, EF_RESPIN, blkno, pblk);
			od_reset (c, d, r);
			goto out;			
		}
		c->c_flags |= CF_ERR_INPROG;
		c->c_err_fsm_save = c->c_retry_fsm;
		od_perror (c, d, EF_RTZ, blkno, pblk);
		od_drive_cmd (c, d, OMD1_REC, DONT_SPIN|INTR);
		d->d_last_hos = -1;
		d->d_flags &= ~DF_SPIRALING;
		c->c_next_fsm = FSM_ERR_DONE;
		break;

	case EA_EJECT:

		/* nothing we can do but eject the disk and go offline */
		c->c_flags |= CF_ERR_INPROG;
		od_perror (c, d, EF_ALWAYS, blkno, pblk);
		od_drive_cmd (c, d, OMD1_EC, DONT_SPIN | INTR);
		c->c_next_fsm = FSM_EJECT;
		c->c_flags |= CF_FAIL_IO;
		v->v_flags = 0;
		break;

	case EA_RESPIN:
		if (c->c_retry++ > e->e_retry) {
			od_fsm (c, d, FSM_FAILED);
			break;
		}
		od_perror (c, d, EF_RESPIN, blkno, pblk);
		od_reset (c, d, r);
		break;
	}
out:
	c->c_flags &= ~CF_CANT_SLEEP;
}

od_reset (c, d, r)
	register volatile struct ctrl *c;
	register struct drive *d;
	register volatile struct od_regs *r;
{
	int i;
	int s;
	
	c->c_flags |= CF_ERR_INPROG;
	s = spldma();
	r->r_disr = (r->r_disr & ~(OMD_RESET|OMD_SDFDA)) | disr_shadow |
	 	OMD_RESET;
	disr_shadow |= OMD_RESET;
	splx(s);
	DELAY(80);
	s = spldma();
	r->r_disr = disr_shadow = disr_shadow & ~OMD_RESET;
	splx(s);
	c->c_flags |= CF_DEVINT_EXP;
	c->c_respin_fsm_save = c->c_retry_fsm;
	c->c_next_fsm = FSM_RESPIN_RST;
	if ((d->d_flags & DF_ATTACH) && !(d->d_flags & DF_TRY_ATTACH)) {

		/* timeout service isn't available yet (neither is DELAY) */
		delay (RESET_TIMEOUT * 1000000);
		c->c_flags |= CF_SOFT_TIMEOUT;
		odintr (c);
	} else {

		/* use timeout mechanism to get interrupted */
		c->c_timeout = RESET_TIMEOUT;
	}
}

od_async_attn (c, d, blkno, pblk)
	register volatile struct ctrl *c;
	register struct drive *d;
{
	register int i;

	/* continue where we left off */
	if (c->c_flags & CF_ASYNC_INPROG)
		return (od_fsm (c, d, c->c_next_fsm));
	c->c_flags |= CF_ASYNC_INPROG;

	switch (c->c_err) {

	case E_INSERT:
		/* reset drive, spin it up and run diagnostics */
		od_drive_cmd (c, d, OMD1_RID, DONT_SPIN | INTR);
		c->c_next_fsm = FSM_INSERT_RID;
		return (OD_BUSY);

	default:
	Default:
		od_perror (c, d, EF_FAILED, blkno, pblk);
		break;
	}
	c->c_flags &= ~CF_ASYNC_INPROG;
	return (OD_DONE);
}

od_perror (c, d, errtype, blkno, pblk)
	register volatile struct ctrl *c;
	register struct drive *d;
	register int errtype;
{
	register struct od_err *e = &od_err[c->c_err];
	register struct buf *bp;
	register dev_t dev = 0;
	register struct od_dcmd *dc;
	register struct od_fcmd *fc;
	register int i, (*pf)();
	register struct volume *v = d->d_vol;
	register struct disk_label *l = v->v_label;
	register queue_t bc_q = &odcinfo[c - od_ctrl]->bc_tab;

	if (odcinfo[c - od_ctrl] && !queue_empty(bc_q)) {
		bp = disksort_first((ds_queue_t *)queue_first(bc_q));
		if (bp)
			dev = bp->b_dev;
	}
#if	0
	/* stop collecting XPR info */
	if (errtype >= EF_FAILED)
		xprflags &= ~XPR_OD;
#endif	DEBUG
	if ((errtype >= od_errmsg_filter || (od_dbug & DBUG_PRTALLERRS)) &&
	    ((c->c_flags & CF_TESTING) == 0
#if	DEBUG
	    || od_dbug & DBUG_PTEST
#endif	DEBUG
	    ))
		pf = od_xpr_alert;
	else
		return;
	(*pf) ("od%d%c: %s %s ",
		v->v_drive, dev? OD_PART(dev) + 'a' : '?',
		c->c_cmd == OMD_READ? "read" :
		c->c_cmd == OMD_WRITE? "write" :
		c->c_cmd == OMD_ERASE? "erase" : "drive command",
		od_errtype[errtype], 0);
	(*pf) ("(%s%s) block %d phys block %d ", 
		e->e_msg, c->c_flags & CF_TESTING? ", testing" : "",
		blkno, pblk - v->v_base, 0);
	(*pf) ("(%d:%d:%d)\n", pblk / l->dl_nsect, 0, pblk % l->dl_nsect,
		0, 0);
	if (od_dbug & DBUG_ALLERRS)
		od_note (c, CF_ERR_VALID, pf);
}

od_note (c, info, pf)
	register volatile struct ctrl *c;
	register int (*pf)();
{
	register struct od_err *e;
	register struct od_dcmd *dc;
	register struct od_fcmd *fc;
 	register int i;

	if (info & CF_ISSUE_D_CMD) {
		(*pf) ("drive cmd: ");
		for (dc = od_dcmd; dc->dc_msg; dc++)
			if ((c->c_last_dcmd & dc->dc_mask) == dc->dc_cmd) {
				(*pf) ("%s (0x%x)\n", dc->dc_msg,
					c->c_last_dcmd);
				goto fcmd;
			}
		(*pf) ("unknown (0x%x)\n", c->c_last_dcmd);
	}
fcmd:
	if (info & CF_ISSUE_F_CMD) {
		(*pf) ("formatter cmd: ");
		for (fc = od_fcmd; fc->fc_msg; fc++)
			if (c->c_fcmd == fc->fc_cmd) {
				(*pf) ("%s\n", fc->fc_msg);
				goto stat;
			}
		(*pf) ("unknown (0x%x)\n", c->c_fcmd);
	}
stat:
	if (info & CF_ERR_VALID) {
	if (c->c_rds & 0xfffe) {
		for (i = 15; i > 0; i--)
			if (c->c_rds & (1 << i)) {
				e = &od_err[E_STATUS_BASE+i-1];
				(*pf) ("\t%s\n", e->e_msg);
			}
	}
	if (c->c_res & 0xfffe) {
		for (i = 15; i > 0; i--)
			if (c->c_res & (1 << i)) {
				e = &od_err[E_EXTENDED_BASE+i-1];
				(*pf) ("\t%s\n", e->e_msg);
			}
	}
	if (c->c_rhs) {
		for (i = 15; i >= 0; i--)
			if (c->c_rhs & (1 << i)) {
				e = &od_err[E_HARDWARE_BASE+i];
				(*pf) ("\t%s\n", e->e_msg);
			}
	}
	}
}

/*
 * Check for attention condition.
 * It will take another interrupt or two to get the drive status.
 * Try a "read drive status" then "read extended status"
 * and finally "read hardware status" to find the error.
 * Return value:
 *	OD_BUSY = attention pending, more status info on next interrupt
 *	OD_DONE = no attention pending
 *	OD_ERR = error found
 * FIXME: do we handle errors in the attention gathering commands properly?
 */
od_attn (c, d, r, disr, flag)
	register volatile struct ctrl *c;
	register struct drive *d;
	register volatile struct od_regs *r;
	register int disr;
{

	/* ATTN forces spiraling off */
	if (disr & OMD_ATTN)
		d->d_flags &= ~DF_SPIRALING;

	/* continue where we left off */
	if (c->c_flags & CF_ATTN_INPROG)
		return (od_fsm (c, d, c->c_next_fsm));

	if ((disr & OMD_ATTN) == 0) {
		c->c_rds = c->c_res = c->c_rhs = 0;
		return (OD_DONE);
	}

	/*
	 * Disable ATTN interrupt during status acquisition
	 * to prevent retriggering.
	 */
	r->r_dimr &= ~OMD_ATTN;

	/* ATTN forces spiraling off */
	if (disr & OMD_ATTN)
		d->d_flags &= ~DF_SPIRALING;

	c->c_flags |= CF_ATTN_INPROG;
	od_status (c, d, r, OMD1_RDS, DONT_SPIN | INTR);
	c->c_attn_fsm_save = c->c_fsm;
	c->c_attn_next_fsm_save = c->c_next_fsm;
	c->c_rds = c->c_res = c->c_rhs = 0;
	c->c_next_fsm = FSM_ATTN_RDS;
	return (OD_BUSY);
}

od_status (c, d, r, cmd, flags)
	register volatile struct ctrl *c;
	register struct drive *d;
	register volatile struct od_regs *r;
{
	register int s;

	s = spl7();		/* FIXME */
	if (od_drive_cmd (c, d, cmd, DONT_SPIN | (flags & (INTR|DONT_INTR)))) {
		splx (s);
		return (-1);
	}
	DELAY (150);
	r->r_cntrl1 = 0;	/* reset formatter first */
	r->r_cntrl1 = OMD_RD_STAT;
	splx (s);
	if (flags & SPIN) {
#define	TOUT	10000000	/* must be long enough for sleep wakeup */
		for (s = 1; s <= TOUT && (r->r_disr & OMD_CMD_COMPL) == 0;) {
			delay (1);
			s++;
		}
		if (s > TOUT) {
			c->c_err = E_BUSY_TIMEOUT1;
			c->c_timeout = 0;
			return (-1);
		}
		return ((r->r_csr_h << 8) | r->r_csr_l);
	}
#undef	TOUT
}

od_cmd (dev, cmd, bno, bufp, bytes, err, dc, de, use_bitmap, rtpri)
	dev_t dev;
	u_short cmd;
	char *bufp;
	char *err;
	struct dr_cmdmap *dc;
	struct dr_errmap *de;
{
	register struct volume *v = &od_vol[OD_VOL(dev)];
	register struct disk_label *l = v->v_label;
	register volatile struct buf *bp = &v->v_cbuf;	/* spin loop later */
	register int s, bcount, i;
	register struct ctrl *c = od_ctrl; /* FIXME: what about multiple ctrls? */

	/*
	 *  Lock against multiple requests.
	 *  WARNING: cbuf must already have been locked before calling
	 *  od_cmd with rtpri != RTPRI_NONE to prevent deadlock.
	 */
	s = spln (od_spl);
	if (rtpri == RTPRI_NONE) {
		while (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			sleep (bp, PRI_OMD);
		}
	}
	bp->b_flags = B_BUSY | B_READ;
	v->v_cmd = cmd;
	if (use_bitmap == 0)
		v->v_flags |= VF_IGNORE_BITMAP;
	if (dc && (dc->dc_flags & DRF_SPEC_RETRY)) {
		v->v_retry = dc->dc_retry;
		v->v_rtz = dc->dc_rtz;
		v->v_flags |= VF_SPEC_RETRY;
	}
	splx (s);
	bp->b_rtpri = rtpri;
	bp->b_dev = dev;
	bp->b_blkno = bno;
	bcount = howmany (bytes, l->dl_secsize) * l->dl_secsize;
	bp->b_bcount = bcount;
	bp->b_un.b_addr = bufp;
	if (dc && bufp) {	/* request from user mode */
		if (useracc (bufp, bcount, B_WRITE) == NULL)
			return (EFAULT);
		bp->b_flags |= B_PHYS;
		bp->b_proc = current_task()->proc;
		bp->b_proc->p_flag |= SPHYSIO;
		vslock (bufp, bcount);
	}
	odstrategy (bp);

	/* can't use biowait during autoconf */
	if (current_thread() == NULL) {
	
		/* don't have timeout() during autoconf -- do it manually */
		for (i = 0; i < 10000000; i++) {
			delay (1);
			if (bp->b_flags & B_DONE)
				break;
		}
		if (i == 10000000) {
			dbug_timer (("SOFT_TIMEOUT AUTOCONF ", 0, 0, 0, 0, 0));
			s = spln (od_spl);
			c->c_flags |= CF_SOFT_TIMEOUT;
			odintr (c);
			splx (s);
		}
	} else
		biowait (bp);
	s = spln (od_spl);
	v->v_flags &= ~(VF_SPEC_RETRY | VF_IGNORE_BITMAP);
	splx (s);
	if (dc && bufp) {
		vsunlock (bufp, bcount);
		bp->b_proc->p_flag &= ~SPHYSIO;
	}
out:
	if (rtpri == RTPRI_NONE) {
		bp->b_flags &= ~B_BUSY;
		if (bp->b_flags & B_WANTED)
			wakeup (bp);
	}
	if (bp->b_flags & B_ERROR) {
		if (err)
			*err = bp->b_error;
		return (EIO);
	}
	if (de)
		de->de_ecc_count = bp->b_resid;		/* return ECC count */
	return (0);
}

int inhibit_label_errs=1;
int dma_recover_rl=1;		/* clear DMA on read label errors */
int dma_recover_wl=1;		/* clear DMA on write label, uninit'd disks */

od_read_label (c, d, polled_attach)
	register struct ctrl *c;
	register struct drive *d;
{
	register int i, blkno, dk, bytes, dev;
	register struct disk_label *l, *rl = od_readlabel;
	register struct drive_info *di;
	register struct more_info *mi;
	int e, save_filter, s, reinsert = 0;
	char err;
	register struct volume *v, *vp;
	struct buf *bp;
	register ds_queue_t *q;
	ds_queue_t save_queue;
	struct mount *mp;
	struct fs *fs;
	struct timeval tv, rtc_get();
	extern int boothowto;
	extern char boot_dev[];
	u_int old_tag;
	
	if (od_empty > 0) {
		v = &od_vol[od_empty - 1];
	} else {
		for (v = od_vol; v < &od_vol[NVOL]; v++)
			if ((v->v_flags & VF_BUSY) == 0)
				break;
		if (v == &od_vol[NVOL])
			panic ("od: out of vols");
	}
	/*
	 * Re-initialize the disksort structure.
	 */
	disksort_free(&v->v_queue);
	bzero (v, sizeof (*v));
	disksort_init(&v->v_queue);
	v->v_flags = VF_BUSY | VF_SPINUP;
	v->v_drive = d - od_drive;
	d->d_vol = v;
	dev = OD_DEV(v - od_vol, 0);
	dbug_vol (("r_l: new v%d d%d ", v - od_vol, v->v_drive, 0, 0, 0));

	/* don't show errors during label read */
	save_filter = od_errmsg_filter;
	if(inhibit_label_errs)
		od_errmsg_filter = EF_ALWAYS;

	/* search for label */
	v->v_di = di = &od_drive_info[d->d_drive_type];
	v->v_mi = mi = &od_more_info[d->d_drive_type];
	for (i = 0; i < NLABELS; i++) {
		if ((blkno = di->di_label_blkno[i]) == -1)
			continue;
		l = v->v_label = od_label;
		if (l == 0)
			panic ("od: no label alloc");

		/* fake parameters actually contained in the label */
		bzero (l, sizeof (struct disk_label));
		v->v_base = mi->mi_reserved_cyls * mi->mi_nsect;
		l->dl_secsize = di->di_devblklen;
		l->dl_nsect = mi->mi_nsect;
		l->dl_ntrack = 1;
		
		/* have to assume it's inserted to begin with */
		v->v_flags |= VF_INSERTED;

		/* spin up drive if necessary */
		if ((d->d_flags & DF_SPINNING) == 0) {
			od_spinup = 1;
			e = od_cmd (dev, OMD_SPINUP,
				0, 0, 0, &err, 0, 0, 0, RTPRI_NONE);
			if (e == EIO && (err == E_BUSY_TIMEOUT1 ||
			    err == E_BUSY_TIMEOUT2)) {
				v->v_flags &= ~VF_INSERTED;
				
				/*
				 *  If this is a polling attach (instead
				 *  of an autoconf attach) then say the
				 *  drive exists even if it seems to be
				 *  disconnected.
				 */
				if (polled_attach)
					d->d_flags |= DF_EXISTS;
				e = ENOENT;
				goto bad;
			}
			if (e == EIO && (err == E_EMPTY || err == E_INSERT)) {
				v->v_flags &= ~VF_INSERTED;
				d->d_flags |= DF_EXISTS;
				e = ENOENT;
				goto bad;
			}
			if (e) {
				v->v_flags &= ~VF_INSERTED;
				d->d_flags |= DF_EXISTS;
				goto bad;
			}
		}
		d->d_flags |= DF_SPINNING | DF_EXISTS;
		e = od_cmd (dev, OMD_READ, blkno, rl, sizeof *rl, &err,
			0, 0, 0, RTPRI_NONE);
		if (e) {
			/* read 1 block to clear DMA */
			if(dma_recover_rl) {
#ifdef	DEBUG
				printf("od_read_label: CLEARING DMA\n");
#endif	DEBUG
				od_cmd (dev, 
					OMD_READ, 
					0,		/* blkno */
					rl,		/* addrs */
					1024, 
					&err,
					0, 0, 0, RTPRI_NONE);
			}
			continue;
		}
		if (od_validate_label (rl, blkno))
			continue;
		bcopy (rl, l, sizeof (struct disk_label));
		
		/* if it's a label version we don't understand then leave disk alone */
		if (l->dl_version != DL_V1 && l->dl_version != DL_V2 &&
		    l->dl_version != DL_V3) {
			e = EINVAL;
			goto bad;
		}

		/* reality check in case it's not an optical label */
		if (l->dl_nsect != mi->mi_nsect)
			continue;

		/* read in bad block table */
		v->v_bad_block = od_bad_block;
		if (l->dl_version != DL_V1 && l->dl_version != DL_V2) {
			dbug_label (("r_l: read bbt\n", 0, 0, 0, 0, 0));
			if (od_cmd (dev, OMD_READ, blkno + BAD_BLK_OFF,
			    v->v_bad_block, sizeof (struct bad_block),
			    0, 0, 0, 0, RTPRI_NONE)) {
				continue;
			}
		}
		
		/* allocate 2 bits per bitmap entry */
		v->v_size = l->dl_ncyl * l->dl_ntrack * l->dl_nsect;
		if (l->dl_version == DL_V1)
			v->v_bitmap_bytes = (v->v_size / l->dl_nsect) >> 1;
		else
			v->v_bitmap_bytes = v->v_size >> 2;
		v->v_bitmap = od_bitmap;

		/* bitmap follows label on next track */
		if (od_cmd (dev, OMD_READ, blkno + l->dl_nsect,
		    v->v_bitmap, v->v_bitmap_bytes, 0, 0, 0, 0, RTPRI_NONE)) {
			continue;
		}

		if ((dk = d->d_bd->bd_dk) >= 0)
			dk_bps[dk] = l->dl_secsize * l->dl_nsect *
				l->dl_rpm / 60;
		if (!polled_attach) {
			printf ("\tDisk Label: %s  Label Version: %d\n",
				l->dl_label, l->dl_version == DL_V1? 1 :
				l->dl_version == DL_V2? 2 : 3);
			dbug_vol (("Disk Label: %s\n\tLabel Version: %d\n",
				l->dl_label, l->dl_version == DL_V1? 1 :
				l->dl_version == DL_V2? 2 : 3,
				0, 0, 0));
			od_canon_label (c, d, v);
		}

		/* check if this is just a reinsert */
		for (vp = od_vol; vp < &od_vol[NVOL]; vp++) {
			if ((vp->v_flags & VF_ONLINE) &&
			    vp->v_label->dl_tag == l->dl_tag) {
				v->v_flags = 0;
				v = vp;
				v->v_drive = d - od_drive;
				d->d_vol = v;
				v->v_flags |= VF_INSERTED;
				dev = OD_DEV(v - od_vol, 0);
				dbug_vol (("r_l: reinsert v%d tag 0x%x ",
					v - od_vol, l->dl_tag, 0, 0, 0));
				reinsert = 1;
				e = 0;
				goto check;
			}
		}

		/* tag volume with time of day */
		dbug_vol (("r_l: old tag 0x%x ", l->dl_tag, 0, 0, 0, 0));
		tv = rtc_get();
		old_tag = l->dl_tag;
		l->dl_tag = tv.tv_sec;
		v->v_flags |= VF_ONLINE | VF_INSERTED;
		if (e = od_write_label (v, RTPRI_NONE)) {
			if (e != E_WP) {
				e = EIO;
				goto bad;
			}
				
			/* if write protected use existing tag (assume unique) */
			l->dl_tag = old_tag;
			v->v_flags |= VF_WP;
		}
		dbug_vol (("r_l: new tag 0x%x ",
			l->dl_tag, 0, 0, 0, 0));
		e = 0;
		goto check;
	}
	
	/*
	 *  Give uninitialized volumes a valid label with a proper tag
	 *  so reinserts can be recognized correctly, but mark the
	 *  label as uninitialized so the automounter will init it.
	 *  Disk must not be write protected.
	 *
	 *  A disk may contains valid data, but all of the labels might be
	 *  unreadable if severe dust contamination exists.  To avoid
	 *  destroying all of the labels (assuming the write laser power level
	 *  will penetrate the dust layer) just write the last label.  If the
	 *  disk is later cleaned the now valid first label will be accepted 
	 *  and the disk will remain intact.
	 */
	if (d->d_flags & DF_SPINNING) {
		dbug_label (("r_l: UNINIT vol ", 0, 0, 0, 0, 0));
		v->v_flags |= VF_INSERTED;

		/* allocate 2 bits per bitmap entry */
		v->v_size = l->dl_ncyl * l->dl_ntrack * l->dl_nsect;
		if (l->dl_version == DL_V1)
			v->v_bitmap_bytes = (v->v_size / l->dl_nsect) >> 1;
		else
			v->v_bitmap_bytes = v->v_size >> 2;
		v->v_bad_block = od_bad_block;
		bzero (v->v_bad_block, sizeof (struct bad_block));
		v->v_bitmap = od_bitmap;
		bzero (v->v_bitmap, v->v_bitmap_bytes);
		tv = rtc_get();
		l->dl_tag = tv.tv_sec;
		l->dl_flags = DL_UNINIT;
		l->dl_version = DL_VERSION;
		if (e = od_write_label (v, RTPRI_NONE)) {
			e = EIO;
			goto bad;
		}
		v->v_flags |= VF_ONLINE;
		e = 0;
		goto check;
	}
	e = ENOENT;
bad:
	/* force reboot if this would have been the root device */
	if (v == od_vol && !polled_attach && strcmp (boot_dev, "od") == 0 &&
	    (km.flags & KMF_SEE_MSGS) == 0 && (boothowto & RB_ASKNAME) == 0)
		mon_boot (0);
	
	/* wakeup anyone waiting for a spinup */
	if (v->v_flags & VF_SPINUP_WANTED) {
		v->v_flags &= ~VF_SPINUP_WANTED;
		wakeup (&v->v_flags);
	}
	
	/* always make sure disk is ejected if there is a problem */
	od_cmd (dev, OMD_EJECT_NOW, 0, 0, 0, 0, 0, 0, 0, RTPRI_NONE);
	
	/* allow a way out if volume is not available */
	if (polled_attach && (od_empty > 0 || od_specific) &&
	    (alert_key == 'n' || 
	    (od_alert_abort) ||
	    (d->d_flags & DF_SPINNING))) {
		if (od_empty > 0) {
			dbug_vol (("r_l: ABORT empty\n"));
			od_empty = -1;
			wakeup (&od_empty);
		} else
		if (od_specific) {
			s = spln (od_spl);
			dbug_vol (("r_l: ABORT fail all I/O\n"));
			while (bp = disksort_first(&od_specific->v_queue)) {
				bp->b_error = ENXIO;
				bp->b_flags |= B_ERROR;
				if (!(bp->b_flags & B_DONE))
					biodone (bp);
				disksort_remove(&od_specific->v_queue, bp);
			}
			/* mark volume as missing so subsequent I/O fails */
			od_specific->v_flags = VF_BUSY | VF_MISSING;
			od_ctrl_start (d->d_bc);
			splx (s);
		}
		if(od_alert_present) {
			vol_panel_remove(od_vol_tag);
			od_alert_abort = 0;
			od_alert_present = 0;
		}
		else
			alert_done();
		if (od_requested == 1) {
			od_specific = 0;
			od_requested = 2;
			wakeup (&od_requested);
		}
	}
	
	/* free volume structure */
	if (reinsert == 0)
		v->v_flags = 0;

	od_spinup = 0;
	dbug_vol (("r_l: BAD ", 0, 0, 0, 0, 0));
out:
	if (v->v_flags & VF_SPINUP_WANTED) {
		v->v_flags &= ~VF_SPINUP_WANTED;
		wakeup (&v->v_flags);
	}
	v->v_flags &= ~VF_SPINUP;
	od_errmsg_filter = save_filter;
	dbug_vol (("r_l: OUT ", 0, 0, 0, 0, 0));
	return (e);
check:
	if (od_empty > 0) {
		if (reinsert) {
			int ej_drive = v->v_drive;
			
			od_cmd (dev, OMD_EJECT_NOW,
				0, 0, 0, 0, 0, 0, 0, RTPRI_NONE);
			if(((major(rootdev) == od_blk_major) &&
			   (!(od_vol[OD_VOL(rootdev)].v_flags&VF_INSERTED))) ||
			    (panel_req_port == PORT_NULL)) {
			   	/*
				 * use old-style alert...
				 */
				od_alert ("Insert Disk", "Wrong disk -- "
					"that was \"%s\" on volume %d\n"
					"Please insert new disk for volume "
					"%d\n",
					v->v_label->dl_label, v - od_vol,
					od_empty - 1, 0, 0, 0, 0, 0);
			}
			else {
				/*
				 * first cancel existing panel for this volume;
				 * then put up a new one.
				 */
				struct volume *desired_vol = 
					&od_vol[od_empty - 1];
					
				vol_panel_remove(od_vol_tag);
				vol_panel_disk_num(od_panel_abort,
					od_empty - 1,
					PR_DRIVE_OPTICAL,
					ej_drive,	/* drive we just 
							 * ejected from */
					desired_vol,
					TRUE,		/* wrong_disk */
					&od_vol_tag);
				od_alert_present = TRUE;
			}
			e = ENOENT;
			goto bad;
		} else {
			dbug_vol (("r_l: empty drive d%d for v%d ",
				d - od_drive, od_empty - 1, 0, 0, 0));
			od_empty = 0;
			wakeup (&od_empty);
		}
	} else

	/* eject wrong volumes if expecting a specific one */
	if (od_specific && (v->v_label == 0 || od_specific != v)) {
	
		int ej_drive = v->v_drive;
		
		dbug_vol (("r_l: wrong specific EJECT "));
		od_cmd (dev, OMD_EJECT_NOW, 0, 0, 0, 0, 0, 0, 0, RTPRI_NONE);
		if(((major(rootdev) == od_blk_major) &&
		    (!(od_vol[OD_VOL(rootdev)].v_flags & VF_INSERTED))) ||
		     (panel_req_port == PORT_NULL)) {
			od_alert ("Insert Disk",
				"Wrong disk -- please insert disk \"%s\" "
				"on volume %d\n",
				od_specific->v_label->dl_label, 
				od_specific - od_vol,
				0, 0, 0, 0, 0, 0);
		}
		else {
			/* 
			 * first remove existing panel.
			 */
			vol_panel_remove(od_vol_tag);
			vol_panel_disk_label(od_panel_abort,
				od_specific->v_label->dl_label,
				PR_DRIVE_OPTICAL,
				ej_drive,
				od_specific,
				TRUE,			/* wrong_disk */
				&od_vol_tag);
			od_alert_present = TRUE;
		}
		e = ENOENT;
		goto bad;
	} else
	if (reinsert == 0) {
		char dev_str[OID_DEVSTR_LEN];
		int flags;
		
		/* if someone wants to know about insert notify them */
		dbug_vol (("r_l: vol_notify v%d ",
			v - od_vol,
			0, 0, 0, 0));
		sprintf(dev_str, "od%d", v-od_vol);
		flags = IND_FLAGS_REMOVABLE;
		if(v->v_flags & VF_WP)
			flags |= IND_FLAGS_WP;
		vol_notify_dev(makedev(od_blk_major, OD_DEV(v - od_vol, 0)), 
				makedev(od_raw_major, OD_DEV(v - od_vol, 0)),
				"",
				v->v_label->dl_flags & DL_UNINIT ?
				    IND_VS_FORMATTED : IND_VS_LABEL,
				dev_str,
				flags);
	}
	if(od_alert_present) {
		vol_panel_remove(od_vol_tag);
		od_alert_abort = 0;
		od_alert_present = 0;
	}
	else
		alert_done();
	od_spinup = 0;
	if (od_requested == 1) {
		od_specific = 0;
		od_requested = 2;
		wakeup (&od_requested);
	}
	od_runout_time = RUNOUT_NOM;
	od_runout = 0;
	dbug_runout(("runout cancelled by insert\n", 0, 0, 0, 0, 0));
	
	/* using the new label -- tell daemon to alloc another one */
	if (reinsert == 0 && (v->v_flags & VF_ONLINE)) {
		od_label = 0;
		wakeup (&od_label);
	}

	/*
	 *  Mark any file system that was previously mounted as dirty
	 *  since we may have marked it clean when the volume was ejected.
	 */
	dev = makedev(od_blk_major, OD_DEV(v - od_vol, 0)) & ~(NPART - 1);
	for (mp = mounttab; mp; mp = mp->m_nxt) {
		if (dev != (mp->m_dev & ~(NPART - 1)))
			continue;
		if (mp->m_bufp == NULL || mp->m_dev == NODEV)
			continue;
		fs = mp->m_bufp->b_un.b_fs;
		if (!fs->fs_ronly && fs->fs_state == FS_STATE_CLEAN) {
			fs->fs_state = FS_STATE_DIRTY;
			fs->fs_fmod = 1;
			dbug_state (("dev 0x%x marked DIRTY\n",
				mp->m_dev, 0, 0, 0, 0));
		}
	}
	
	/* restart any pending I/O on this volume */
	s = spln (od_spl);
	d->d_flags |= DF_ONLINE;
	v->v_flags &= ~VF_WANTED;
	q = &v->v_queue;
	dbug_vol (("r_l: v%d/act%d insert restart\n",
		v - od_vol, q->active, 0, 0, 0));
	dbug_q (("r_l: INSERT v%d\n", v - od_vol, 0, 0, 0, 0));
	od_drive_start (v);
	d = &od_drive[v->v_drive];
	dbug_vol (("r_l: b_actf 0x%x b_active %d d%d ",
		queue_first(&d->d_bc->bc_tab), d->d_bc->bc_active,
			d - od_drive, 0, 0));
	/*
	 *  Always call od_ctrl_start() in case another volume was
	 *  blocked while od_request() was running for this one.
	 */
	od_ctrl_start (d->d_bc);
	splx (s);
	goto out;
}

od_write_label (v, rtpri)
	register struct volume *v;
{
	register int i, dev, bno, e, first, last;
	int failed = 1;
	char err;
	register struct disk_label *l = v->v_label;
	u_short checksum_16(), *dl_cksum, size;
	int good_block=0;
	
	if (l->dl_version == DL_V1 || l->dl_version == DL_V2) {
		size = sizeof (struct disk_label);
		dl_cksum = &l->dl_checksum;
	} else {
		size = sizeof (struct disk_label) - sizeof (l->dl_un);
		dl_cksum = &l->dl_v3_checksum;
	}
	dev = OD_DEV(v - od_vol, 0);
	if (l->dl_flags & DL_UNINIT) {
		first = NLABELS - 1;
		while (v->v_di->di_label_blkno[first] == -1)
			first--;
		last = first + 1;
	} else {
		first = 0;
		last = NLABELS;
	}
	for (i = first; i < last; i++) {
		if ((bno = v->v_di->di_label_blkno[i]) == -1)
			continue;
		
		/* volume is gone */
		if ((v->v_flags & VF_BUSY) == 0)
			return (E_EMPTY);

		/*
		 *  Compatibility workaround:  don't write a label if
		 *  it would land in the filesystem area.  Happens when
		 *  old media is used when the default label locations have
		 *  moved downward, in a new kernel, into the FS area.
		 */
		if (l->dl_front && bno >= l->dl_front &&
		    bno <= l->dl_ncyl * l->dl_ntrack *
		    l->dl_nsect - l->dl_back)
			continue;
		l->dl_label_blkno = bno;
		*dl_cksum = 0;
		*dl_cksum = checksum_16 (l, size >> 1);
		dbug_label (("w_l: dl_cksum 0x%x @ 0x%x size %d version 0x%x\n",
			*dl_cksum, (int)dl_cksum - (int)l, size,
			l->dl_version, 0));
		dbug_update (("Ul-%d ", bno, 0, 0, 0, 0));
		e = od_cmd (dev, OMD_WRITE, bno, l, sizeof *l,
			&err, 0, 0, 0, rtpri);
		if (e) {
			if (e == EIO && err == E_WP) {
				failed = err;
				break;
			}
			continue;
		}
		good_block = bno;
		
 		/* write out bad block table */
		if (l->dl_version != DL_V1 && l->dl_version != DL_V2) {
			dbug_label (("w_l: write bbt\n", 0, 0, 0, 0, 0));
			e = od_cmd (dev, OMD_WRITE, bno + BAD_BLK_OFF,
				v->v_bad_block, sizeof (struct bad_block),
				&err, 0, 0, 0, rtpri);
			if (e) {
				if (e == EIO && err == E_WP) {
					failed = err;
					break;
				}
				continue;
			}
		}

		/* bitmap follows label on next track */
		dbug_update (("Ub-%d ", bno + l->dl_nsect, 0, 0, 0, 0));
		e = od_cmd (dev, OMD_WRITE, bno + l->dl_nsect,
			v->v_bitmap, v->v_bitmap_bytes, &err, 0, 0, 0, rtpri);
		if (e) {
			if (e == EIO && err == E_WP) {
				failed = err;
				break;
			}
			continue;
		}
		/* read 1 block to clear DMA for uninitialized disks */
		if((l->dl_flags & DL_UNINIT) && dma_recover_wl) {
			char *p;
			int err;
			
#ifdef	DEBUG
			printf("od_write_label: CLEARING DMA\n");
#endif	DEBUG
			p = (char *)kalloc(1024);
			od_cmd (dev, 
				OMD_READ, 
				good_block,		/* blkno */
				p,			/* addrs */
				1024, 
				&err,
				0, 0, 0, rtpri);
			kfree(p, 1024);
		}
		failed = 0;
	}
	return (failed);
}

od_update()
{
	register struct volume *v;
	int s;
	register struct buf *bp;

	/*
	 * Update label (including bad block table) and status bitmap
	 * at all disk locations indicated by media type.
	 */
	for (v = od_vol; v < &od_vol[NVOL]; v++) {
		dbug_timer (("update: v%d v_flags 0x%x ",
			v - od_vol, v->v_flags, 0, 0, 0));
		if ((v->v_flags & (VF_BUSY|VF_UPDATE|VF_ONLINE|VF_INSERTED)) ==
		    (VF_BUSY|VF_UPDATE|VF_ONLINE|VF_INSERTED) &&
		    (v->v_flags & VF_WP) == 0) {
			s = spln (od_spl);
			bp = &v->v_cbuf;
			while (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				sleep (bp, PRI_OMD);
			}
			bp->b_flags = B_BUSY;
			v->v_flags |= VF_LOCK;
			splx (s);
			od_write_label (v, RTPRI_MAX);
			if (v->v_flags & VF_LOCK_OUT)
				wakeup (v);
			s = spln (od_spl);
			v->v_flags &= ~(VF_LOCK | VF_LOCK_OUT);
			bp->b_flags &= ~B_BUSY;
			if (bp->b_flags & B_WANTED)
				wakeup (bp);
			splx (s);
		}
		if (!(v->v_flags & VF_BUSY))
			continue;
		s = spln(od_spl);
		v->v_flags &= ~VF_UPDATE;
		splx(s);

		/* eject if requested */
		if (v->v_flags & VF_EJECT) {
			od_cmd (OD_DEV(v - od_vol, 0), OMD_EJECT_NOW,
			0, 0, 0, 0, 0, 0, 0, RTPRI_NONE);
		}
	}
}

od_update_thread()
{
	int old_priority, old_sched_pri;

	/*
	 * Update label (including bad block table) and status bitmap
	 * at all disk locations indicated by media type.
	 */
	dbug_timer (("<update> ", 0, 0, 0, 0, 0));
	old_priority = current_thread()->priority;
	old_sched_pri = current_thread()->sched_pri;
	current_thread()->priority = 31;		/* high priority */
	current_thread()->sched_pri = 31;
	od_update();
	current_thread()->priority = old_priority;
	current_thread()->sched_pri = old_sched_pri;
	dbug_timer (("<update done> ", 0, 0, 0, 0, 0));
	od_update_time = KT_UNSCHED;
	thread_terminate (current_thread());
	thread_halt_self();
}

od_validate_label (l, blkno)
	register struct disk_label *l;
{
	u_short cksum, checksum_16(), *dl_cksum, got, size;

	/* when label is all zeros the checksum will pass, so prevent that here */
	if (l->dl_version == 0)
		return (-1);
	if (l->dl_version == DL_V1 || l->dl_version == DL_V2) {
		size = sizeof (struct disk_label);
		dl_cksum = &l->dl_checksum;
	} else {
		size = sizeof (struct disk_label) - sizeof (l->dl_un);
		dl_cksum = &l->dl_v3_checksum;
	}
	dbug_label (("dl_cksum 0x%x @ 0x%x size %d version 0x%x\n", *dl_cksum,
		(int)dl_cksum - (int)l, size, l->dl_version, 0));
	if (l->dl_label_blkno != blkno) {
		dbug_label (("label_blkno %d != blkno %d\n",
			l->dl_label_blkno, blkno, 0, 0, 0));
		return (-1);	/* label not where it's supposed to be */
	}
	cksum = *dl_cksum;
	*dl_cksum = 0;
	if ((got = checksum_16 (l, size >> 1)) != cksum) {
		dbug_label (("cksum: expected 0x%x got 0x%x\n",
			cksum, got, 0, 0, 0));
		return (-1);
	}
	dbug_label (("cksum OK\n", 0, 0, 0, 0, 0));
	return (0);
}

od_daemon() {
	register struct ctrl *c = od_ctrl; /* FIXME: what about multiple ctrls? */
	register struct drive *d;
	register struct disk_label *l;
	register int dev, be, max_be, off, shift;
	int be_len, bno, spbe, state;
	char dummy_buf[1];
	register char *buf = dummy_buf;
	register struct volume *v;

#if	NeXT
	current_thread()->priority = 0;	/* low priority */
	current_thread()->sched_pri = 0;
#else	NeXT
	current_thread()->priority = 127;	/* low priority */
	current_thread()->sched_pri = 127;
#endif	NeXT
	for (d = od_drive; d < &od_drive[NOD]; d++) {
		v = d->d_vol;
		if ((d->d_flags & DF_ONLINE) == 0)
			continue;
		dev = OD_DEV(v - od_vol, 0);
		dbug_timer (("<daemon> dev 0x%x ", dev, 0, 0, 0, 0));
		l = v->v_label;
		if (l->dl_version == DL_V1)
			spbe = l->dl_nsect >> 1;
		else
			spbe = 1;
		be_len = spbe * l->dl_secsize;
		bno = 0;
		max_be = v->v_size / spbe;
		for (be = 0; be < max_be; be++) {
			if ((d->d_flags & DF_ONLINE) == 0)
				break;

			/* stop when regular I/O requests appear */
			if (od_idle_time != KT_BUSY) {
				dbug_timer (("stop-daemon ", 0, 0, 0, 0, 0));
				od_idle_time = KT_SCHED;
				goto out;
			}
			off = be >> 4;
			shift = (be & 0xf) << 1;
			state = (v->v_bitmap[off] >> shift) & 3;

			/* test erased blocks from a bulk erase also */
			if (state == SB_UNTESTED /* || state == SB_ERASED */) {
				dbug_timer (("D%d ", be, 0, 0, 0, 0));

				/*
				 *  Note: force use of bitmap so testing
				 *  will occur.
				 */
				od_cmd (dev, OMD_TEST, bno, buf, be_len,
					0, 0, 0, 1, RTPRI_NONE);

				/* let others run */
				if (csw_needed(current_thread(), current_processor()))
					thread_block();
			}
			bno += spbe;
		}
	}
	dbug_timer (("daemon-done ", 0, 0, 0, 0, 0));
	od_idle_time = KT_DONE;		/* no need to run again */
out:
	thread_terminate (current_thread());
	thread_halt_self();
}

od_spiral()
{
	register struct drive *d;
	register int dev;

	/* stop spiraling on drive that have been idle too long */
	for (d = od_drive; d < &od_drive[NOD]; d++)
		if (d->d_spiral_time == KT_BUSY &&
		    (d->d_vol->v_flags & VF_INSERTED)) {
			dbug_spiral (("dev 0x%x: *spiral off*\n",
				dev, 0, 0, 0, 0));
			dev = OD_DEV(d->d_vol - od_vol, 0);
			od_cmd (dev, OMD_SPIRAL_OFF, 0, 0, 0, 0, 0, 0, 0,
				RTPRI_NONE);
			d->d_flags &= ~DF_SPIRALING;
			d->d_spiral_time = 0;
		}
	thread_terminate (current_thread());
	thread_halt_self();
}

od_creq_timeout (dc)
	register struct dr_cmdmap *dc;
{
	dc->dc_wait = 0;
	wakeup (&od_creq_timeout);
}

odioctl (dev, cmd, data, flag)
	dev_t dev;
	caddr_t data;
{
	register struct ctrl *c = od_ctrl; /* FIXME: what about multiple ctrls? */
	register int e = 0, s, drive, old_tag;
	int vol = OD_VOL(dev), old_bitmap_bytes, new_bitmap_bytes;
	register struct volume *v = &od_vol[vol];
	register struct drive *d;
	register struct disk_label *l;
	register caddr_t bp = *(caddr_t*) data;
	register struct disk_req *dr;
	struct dr_cmdmap *dc;
	struct dr_errmap *de;
	struct timeval start, stop;
	struct sdc_wire wire;
	struct timeval tv, rtc_get();
	dev_t bdev;
	
	/* first, do ioctls that don't require a specific volume */
	switch (cmd) {
	
	case DKIOCINFO:
		*(struct drive_info*) data =
			od_drive_info[od_drive[v->v_drive].d_drive_type];
		return (0);

	case DKIOCSDCWIRE:
		wire = *(struct sdc_wire*) data;
		e = vm_map_pageable (current_task()->map,
			wire.start, wire.end, wire.new_pageable);
		if (wire.new_pageable == FALSE)
			od_make_free_pages();
		return (e);
	
	case DKIOCSDCLOCK:
		return (od_lock(DKIOCSDCLOCK));

	case DKIOCSDCUNLOCK:
		return (od_lock(DKIOCSDCUNLOCK));

	case DKIOCGFREEVOL:
		for (v = od_vol; v < &od_vol[NVOL]; v++)
			if ((v->v_flags & VF_BUSY) == 0)
				break;
		if (v == &od_vol[NVOL])
			*(int*) data = -1;
		else
			*(int*) data = v - od_vol;
		return (0);

	case DKIOCZSTATS:
		if (!suser())
			return (u.u_error);
		bzero (&od_stats, sizeof (struct disk_stats));
		return (0);

	case DKIOCGSTATS:
		e = copyout (&od_stats, bp, sizeof (struct disk_stats));
		return (0);

	case DKIOCGFLAGS:
		*(int*) data = od_dbug;
		return (0);
		
	case DKIOCSFLAGS:
		if (!suser())
			return (u.u_error);
		od_dbug = *(int*) data;
		if (od_dbug & DBUG_NO_STARVE)
			c->c_initr &= ~OMD_ECC_STV_DIS;
		else
			c->c_initr |= OMD_ECC_STV_DIS;
		return (0);
	}

	/* the remaining ioctls require a valid volume */
	if (vol < 0 || vol >= NVOL || (v->v_flags & VF_BUSY) == 0)
		return (ENXIO);
	l = v->v_label;

	switch (cmd) {

	case DKIOCGLABEL:
		if ((v->v_flags & VF_ONLINE) && (l->dl_flags & DL_UNINIT) == 0)
			e = copyout (l, bp, sizeof (*l));
		else
			e = ENXIO;
		break;

	/* FIXME?  Synchronization problem updating active label/bitmap! */
	case DKIOCSLABEL:
		if (!suser())
			return (u.u_error);
		old_bitmap_bytes = v->v_bitmap_bytes;
		if (l == 0) {
			l = (struct disk_label*) kmem_alloc (kernel_map,
				sizeof (struct disk_label));
			if (l == 0)
				panic ("od: SLABEL alloc");
			old_tag = 0;
		} else
			old_tag = l->dl_tag;
		dbug_vol (("SL: old_tag 0x%x ", old_tag));
		if (e = copyin (bp, l, sizeof (*l)))
			break;
		v->v_label = l;
		
		/* create a bad block table if we need one */
		if (v->v_bad_block == 0) {
			if ((v->v_bad_block = (struct bad_block*) kmem_alloc
			    (kernel_map, sizeof (struct bad_block))) == 0)
				return (ENOMEM);
			bzero (v->v_bad_block, sizeof (struct bad_block));
		}

		/* create a bitmap if we need one */
		v->v_base = v->v_mi->mi_reserved_cyls * l->dl_nsect;
		v->v_size = l->dl_ncyl * l->dl_ntrack * l->dl_nsect;
		if (l->dl_version == DL_V1)
			new_bitmap_bytes = (v->v_size / l->dl_nsect) >> 1;
		else
			new_bitmap_bytes = v->v_size >> 2;
		if (v->v_bitmap == 0 ||
		    old_bitmap_bytes != new_bitmap_bytes) {
			if (v->v_bitmap)
				kmem_free (kernel_map, v->v_bitmap,
					MAX_BITMAP);
			v->v_bitmap_bytes = new_bitmap_bytes;
			if ((v->v_bitmap = (int*) kmem_alloc (kernel_map,
			    MAX_BITMAP)) == 0)
				return (ENOMEM);

			/* set all entires to SB_UNTESTED */
			bzero (v->v_bitmap, v->v_bitmap_bytes);
		}

		/* tag volume with time of day */
		if (old_tag == 0) {
			tv = rtc_get();
			old_tag = tv.tv_sec;
			dbug_vol (("SL: v%d new tag 0x%x ",
				v - od_vol, old_tag, 0, 0, 0));
		}
		l->dl_tag = old_tag;
		l->dl_flags &= ~DL_UNINIT;
		if (od_write_label (v, RTPRI_NONE)) {
			e = EIO;
			break;		/* can't write label */
		}
		s = spln (od_spl);
		v->v_flags |= VF_ONLINE;
		od_drive[v->v_drive].d_flags |= DF_ONLINE;
		splx (s);
		od_canon_remap (c, d, v, 0);
		break;

	case DKIOCGBITMAP:
		if (v->v_flags & VF_ONLINE)
			e = copyout (v->v_bitmap, bp, v->v_bitmap_bytes);
		else
			e = ENXIO;
		break;

	case DKIOCSBITMAP:
		if (!suser())
			return (u.u_error);
		if (v->v_bitmap == 0)
			return (0);
		e = copyin (bp, v->v_bitmap, v->v_bitmap_bytes);
		s = spln (od_spl);
		v->v_flags |= VF_UPDATE;
		splx (s);
		if (od_update_time == KT_UNSCHED)
			od_update_time = KT_SCHED;
		break;

	case DKIOCGBBT:
		if (v->v_flags & VF_ONLINE)
			e = copyout (v->v_bad_block, bp,
				sizeof (struct bad_block));
		else
			e = ENXIO;
		break;

	case DKIOCSBBT:
		if (!suser())
			return (u.u_error);
		if (v->v_bad_block == 0)
			return (0);
		e = copyin (bp, v->v_bad_block, sizeof (struct bad_block));
		s = spln (od_spl);
		v->v_flags |= VF_UPDATE;
		splx (s);
		od_canon_remap (c, d, v, 0);
		if (od_update_time == KT_UNSCHED)
			od_update_time = KT_SCHED;
		break;

	case DKIOCREQ:
		dr = (struct disk_req*) data;
		dc = (struct dr_cmdmap*) dr->dr_cmdblk;
		de = (struct dr_errmap*) dr->dr_errblk;
	
		/* allow owner of volume to eject it */
		if ((dc->dc_cmd != OMD_EJECT && !suser()) ||
		    (dc->dc_cmd == OMD_EJECT && (u.u_ruid != v->v_owner && !suser())))
			return (u.u_error);
		microtime (&start);
		e = od_cmd (dev, dc->dc_cmd, dc->dc_blkno, dr->dr_addr,
			dr->dr_bcount, &de->de_err, dc, de, 0, RTPRI_NONE);
		microtime (&stop);
		timevalsub (&stop, &start);
		dr->dr_exec_time = stop;
		if (dc->dc_wait) {
			struct timeval tv;

			tv.tv_sec = 0;
			tv.tv_usec = dc->dc_wait * 1000;
			timevalfix (&tv);
			us_timeout (&od_creq_timeout, dc, &tv,
				CALLOUT_PRI_SOFTINT0);
			while (dc->dc_wait)
				sleep (&od_creq_timeout, PRI_OMD);
		}
		break;
		
	case DKIOCEJECT:
		/* only allow suser and owner of volume to eject */
		if (!suser() && (u.u_ruid != v->v_owner))
			return (u.u_error);
		/* sync block device for this volume before eject */
		bdev = makedev(od_blk_major, OD_DEV(v - od_vol, 0)) &
			~(NPART - 1);
		od_sync (bdev);
		e = od_cmd (dev, OMD_EJECT, 0, 0, 0, 0, 0, 0, 0, 
			RTPRI_NONE);
		break;
		
	default:
		return (ENOTTY);
	}
	return (e);
}

/* remap bad sectors from Canon PLL defect table */
od_canon_remap (c, d, v, just_count)
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct volume *v;
{
	int dev = OD_DEV(v - od_vol, 0), bno, i, count = 0, save_filter;
	struct canon_defect *cd;
	struct NeXT_defect *nd;
	struct sector_addr *sa;

	save_filter = od_errmsg_filter;
	od_errmsg_filter = EF_ALWAYS;
	cd = (struct canon_defect*) kmem_alloc (kernel_map, CANON_TABSIZE);
	bno = (DEFECT_TRACK - v->v_mi->mi_reserved_cyls) * v->v_mi->mi_nsect;
	if (od_cmd (dev, OMD_READ, bno, cd, CANON_TABSIZE,
	    0, 0, 0, 0, RTPRI_NONE))
		goto next_table;
	if (*(u_short*)&cd->cd_id_code != CD_ID_CODE)
		goto next_table;
	count += *(u_short*)cd->cd_defect_sect_num;
	if (just_count)
		goto next_table;
	for (i = 0; i < count; i++) {
		sa = (struct sector_addr*) &cd->cd_defects[i][0];
		bno = (((sa->sa_track[0] << 16) + (sa->sa_track[1] << 8) +
			sa->sa_track[2]) * v->v_mi->mi_nsect) + sa->sa_sector;
		if (bno == 0)
			continue;
		if (bno < v->v_base)
			continue;
		if (od_locate_alt (c, d, v, bno - v->v_base, bno) != -1)
			continue;
		if (od_remap (c, d, v, bno) == 0)
			break;
	}
next_table:
	kmem_free (kernel_map, cd, CANON_TABSIZE);
#if 0
	nd = (struct NeXT_defect*) kmem_alloc (kernel_map, NeXT_TABSIZE);
	bno = (NeXT_DEFECT_TRACK - v->v_mi->mi_reserved_cyls) * 
		v->v_mi->mi_nsect;
	if (od_cmd (dev, OMD_READ, bno, nd, NeXT_TABSIZE,
	    0, 0, 0, 0, RTPRI_NONE))
		goto out;
	if (*(u_int*)&nd->nd_id_code != ND_ID_CODE)
		goto out;
	for (i = 0; i < NeXT_NDEFECTS; i++) {
		if (just_count) {
			if (*(u_int*)&nd->nd_defects[i][0])
				count += 0x00010000;
			continue;
		}
		sa = (struct sector_addr*) &nd->nd_defects[i][0];
		bno = (((sa->sa_track[0] << 16) + (sa->sa_track[1] << 8) +
			sa->sa_track[2]) * v->v_mi->mi_nsect) + sa->sa_sector;
		if (bno == 0)
			continue;
		if (bno < v->v_base)
			continue;
		if (od_locate_alt (c, d, v, bno - v->v_base, bno) != -1)
			continue;
		if (od_remap (c, d, v, bno) == 0)
			break;
	}
out:
	kmem_free (kernel_map, nd, NeXT_TABSIZE);
#endif
	od_errmsg_filter = save_filter;
	return (count);
}

od_canon_label (c, d, v)
	register volatile struct ctrl *c;
	register struct drive *d;
	register struct volume *v;
{
	int dev = OD_DEV(v - od_vol, 0), bno, save_filter, defects;
	struct canon_control *cc;
	char date[9];

	save_filter = od_errmsg_filter;
	od_errmsg_filter = EF_ALWAYS;
	cc = (struct canon_control*) kmem_alloc (kernel_map, CANON_TABSIZE);
	bno = (CONTROL_TRACK - v->v_mi->mi_reserved_cyls) * v->v_mi->mi_nsect;
	if (od_cmd (dev, OMD_READ, bno, cc, CANON_TABSIZE,
	    0, 0, 0, 0, RTPRI_NONE))
		goto out;
	if (*(u_short*)&cc->cc_id_code != CC_ID_CODE)
		goto out;
	bcopy (&cc->cc_prod_date[4], date, 2);
	date[2] = '/';
	bcopy (&cc->cc_prod_date[6], &date[3], 2);
	date[5] = '/';
	bcopy (&cc->cc_prod_date[2], &date[6], 2);
	date[8] = 0;
	defects = od_canon_remap (c, d, v, 1);
	printf ("\tLot: %s  Serial: %s  Date: %s  Canon: %d  NeXT: %d\n",
		cc->cc_lot, cc->cc_serial, date,
		defects & 0xffff, defects >> 16);
out:
	kmem_free (kernel_map, cc, CANON_TABSIZE);
	od_errmsg_filter = save_filter;
}

od_lock (type)
{
	thread_t th;
	processor_set_t	pset;
	char *comm;
	u_short pid;
	register int dev;
	struct drive *d;
	struct volume *v;
	
	pid = current_thread()->task->proc->p_pid;
	if (type == DKIOCSDCLOCK && od_lock_pid)
		return (EBUSY);
	if (type == DKIOCSDCUNLOCK && pid != od_lock_pid)
		return (EBUSY);
	od_lock_pid = pid;
	dbug_lock (("%s IN\n", type == DKIOCSDCLOCK? "LOCK" : "UNLOCK"));

	/* sync any inserted volumes before unlock */
	if (type == DKIOCSDCUNLOCK) {
		for (d = od_drive; d < &od_drive[NOD]; d++) {
			if ((d->d_flags & (DF_SPINNING|DF_ONLINE)) !=
			    (DF_SPINNING|DF_ONLINE))
				continue;
			v = d->d_vol;
			if (v == 0 || (v->v_flags & VF_INSERTED) == 0)
				continue;
			dev = makedev(od_blk_major, OD_DEV(v - od_vol, 0));
			dbug_lock (("unlock: update dev 0x%x mask 0x%x IN ",
				dev & ~(NPART - 1), ~(NPART - 1), 0, 0, 0));
			if (getnewbuf_count() > NBUF_FREE)
				update (dev & ~(NPART - 1), ~(NPART - 1));
#if	DEBUG
			else
				printf ("od: no bufs for update\n");
#endif	DEBUG
			dbug_lock (("unlock: update OUT ", 0, 0, 0, 0, 0));
			
		}
	}
	
	simple_lock(&all_psets_lock);
	pset = (processor_set_t) queue_first(&all_psets);
	while (!queue_end(&all_psets, (queue_entry_t) pset)) {
		pset_lock(pset);
		th = (thread_t) queue_first(&pset->threads);
		while (!queue_end(&pset->threads, (queue_entry_t) th)) {
			comm = th->task->u_address->uu_comm;
			if (strcmp(comm, kernel_task->u_address->uu_comm)
			    && strcmp(comm, "biod")
			    && th->task->proc->p_pid != od_lock_pid
			    && *comm != 0)
			{
				if (type == DKIOCSDCLOCK) {
					dbug_lock (("suspend th 0x%x %s\n",
						th, comm, 0, 0, 0));
					thread_suspend(th);
				} else {
					dbug_lock (("resume th 0x%x %s\n",
						th, comm, 0, 0, 0));
					thread_resume(th);
				}
			}
			th = (thread_t) queue_next(&th->pset_threads);
		}
		pset_unlock(pset);
		pset = (processor_set_t) queue_next(&pset->all_psets);
	}
	simple_lock(&all_psets_lock);

	/* create some free pages to keep pageout thread from running */
	if (type == DKIOCSDCLOCK) {
		/* od_make_free_pages(); */
#if	MACH_NBC
		mfs_cache_clear();
#endif	MACH_NBC
	}
	if (type == DKIOCSDCUNLOCK)
		od_lock_pid = 0;
	dbug_lock (("%s OUT\n", type == DKIOCSDCLOCK? "LOCK" : "UNLOCK"));
	return (0);
}

od_make_free_pages() {
	int va, size;

	dbug_lock (("mfp: vmpf %d BEFORE\n", vm_page_free_count));
	size = vm_page_free_target * 2 * PAGE_SIZE;
	va = kmem_alloc (kernel_map, size);
	kmem_free (kernel_map, va, size);
	dbug_lock (("mfp: vmpf %d AFTER\n", vm_page_free_count));
}

/* called from exit() -- make sure unlock happens if proc terminates */
od_unlock_check (pid)
	u_short pid;
{
	if (pid == od_lock_pid)
		od_lock(DKIOCSDCUNLOCK);
}

odminphys (bp)
	register struct buf *bp;
{
	extern int maxphys;
	
	/* limit the size of unaligned transfers to MAXBUF */
	if (!DMA_ENDALIGNED(bp->b_un.b_addr) && bp->b_bcount > MAXBUF)
		bp->b_bcount = MAXBUF;

	/* limit the size of the transfer due to DMA bug */
	if (bp->b_bcount > maxphys)
		bp->b_bcount = maxphys;
	minphys (bp);
}

/* note: assumes sector size <= physical page size */
od_zero_fill (c, v, n)
	register volatile struct ctrl *c;
	register struct volume *v;
{
	int bcount, rem;
	register struct disk_label *l = v->v_label;
	vm_offset_t va = (vm_offset_t) c->c_va, pa;
	
	c->c_xfer = n;
	while (n--) {
		bcount = l->dl_secsize;
		while (bcount) {
			pa = pmap_resident_extract (c->c_pmap, va);
			rem = NeXT_page_size - (va & NeXT_page_mask);
			if (rem > bcount)
				rem = bcount;
			bzero (pa, rem);
			va += rem;
			bcount -= rem;
		}
	}
}

oddump() {}

odsize() {}

od_buf_alloc()
{
	od_label = (struct disk_label*) kmem_alloc (kernel_map,
		sizeof (struct disk_label));
	if (od_label == 0)
		panic ("od: label alloc");
	od_bad_block = (struct bad_block*) kmem_alloc (kernel_map,
		sizeof (struct bad_block));
	if (od_bad_block == 0)
		panic ("od: bad_block alloc");
	od_bitmap = (int*) kmem_alloc (kernel_map, MAX_BITMAP);
	if (od_bitmap == 0)
		panic ("od: bitmap alloc");
}

od_label_alloc()
{
	while (1) {
		if (od_label == 0)
			od_buf_alloc();
		sleep (&od_label, PRI_OMD);
	}
}

od_try_attach()
{
	register struct drive *d;
	register struct volume *v;
	register int s, request = 0;

	od_try_th = current_thread();
	while (1) {
		s = spln (od_spl);
		for (d = od_drive; d < &od_drive[NOD]; d++)
			if ((d->d_flags & DF_EXISTS) &&
			    (d->d_flags & DF_SPINNING) == 0) {
				d->d_flags |= DF_TRY_ATTACH;
				dbug_vol (("try_attach: d%d ",
					d - od_drive, 0, 0, 0, 0));
				odattach (d->d_bd);
				d->d_flags &= ~DF_TRY_ATTACH;
			}
		splx (s);
		assert_wait(0, FALSE);
		thread_set_timeout(od_requested? hz/2 : hz);
		thread_block();
	}
	
}

od_make_empty()
{
	register int dev;
	struct drive *d, *lru;
	
	lru = 0;
	for (d = od_drive; d < &od_drive[NOD]; d++) {
		if ((d->d_flags & DF_SPINNING) == 0 &&
		    (d->d_flags & DF_EXISTS))
			break;
		if ((d->d_flags & DF_SPINNING) == 0)
			continue;
		if (lru == 0 || (d->d_lru.tv_sec < lru->d_lru.tv_sec))
			lru = d;
	}
	if (d == &od_drive[NOD]) {
		if (lru == 0) {
			dbug_vol (("make empty: NO lru? ", 0, 0, 0, 0, 0));
			return (0);
		}
		if (lru->d_flags & DF_EJECTING) {
			dbug_vol (("make empty: already ejecting! ",
				0, 0, 0, 0, 0));
			return (0);
		}
		dev = makedev(od_blk_major, OD_DEV(lru->d_vol - od_vol, 0)) &
			~(NPART - 1);
		dbug_vol (("make empty: lru eject d%d ",
			lru - od_drive, 0, 0, 0, 0));

		/* sync volume before eject */
		od_sync (dev);
		od_cmd (dev, OMD_EJECT, 0, 0, 0, 0, 0, 0, 0, RTPRI_NONE);
	} else
		dbug_vol (("make empty: empty d%d ", 
			d - od_drive, 0, 0, 0, 0));
	return (1);
}

od_sync (dev)
	dev_t dev;
{
	struct mount *mp;
	struct fs *fs;

	dbug_vol (("od_sync: update dev 0x%x mask 0x%x IN ",
		dev, ~(NPART - 1), 0, 0, 0));
	if (getnewbuf_count() > NBUF_FREE) {
		update (dev, ~(NPART - 1));
		
		/*
		 *  Mark file systems as clean now to avoid an
		 *  extra swap when they are unmounted later.
		 */
		for (mp = mounttab; mp; mp = mp->m_nxt) {
			if (dev != (mp->m_dev & ~(NPART - 1)))
				continue;
			if (mp->m_bufp == NULL || mp->m_dev == NODEV)
				continue;
			fs = mp->m_bufp->b_un.b_fs;
			if (!fs->fs_ronly &&
			    fs->fs_state == FS_STATE_DIRTY) {
				fs->fs_state = FS_STATE_CLEAN;
				sbupdate(mp);
				dbug_state (("dev 0x%x marked CLEAN\n",
					mp->m_dev, 0, 0, 0, 0));
			}
		}
	}
#if	DEBUG
	else
		printf ("od: no bufs for update\n");
#endif	DEBUG
	dbug_vol (("od_sync: update OUT ", 0, 0, 0, 0, 0));
}

od_request()
{
	spln(od_spl);
	od_req_th = current_thread();
	while (1) {
		dbug_vol (("req: WAITING ", 0, 0, 0, 0, 0));
		od_requested = 0;
		while (od_requested == 0)
			sleep (&od_requested, PRI_OMD);
		dbug_vol (("req: RUNNING ", 0, 0, 0, 0, 0));
		if (od_spinup == 0)
			od_request_vol();
		dbug_vol (("req: spinup IN ", 0, 0, 0, 0, 0));
		while (od_requested == 1)
			sleep (&od_requested, PRI_OMD);
		if(od_alert_present) {
			vol_panel_remove(od_vol_tag);
			od_alert_abort = 0;
			od_alert_present = 0;
		}
		else
			alert_done();
		dbug_vol (("req: spinup OUT ", 0, 0, 0, 0, 0));
	}
}

od_request_vol()
{
	struct volume *v;
	register int s;

	dbug_vol (("req_vol: RUNNING ", 0, 0, 0, 0, 0));
	s = spln (od_spl);

	/* allow a new disk to be inserted into an empty drive */
	if (od_empty > 0) {
		splx (s);
		if(((major(rootdev) == od_blk_major) &&
		    (!(od_vol[OD_VOL(rootdev)].v_flags & VF_INSERTED))) ||
		     (panel_req_port == PORT_NULL)) {
			od_alert ("Insert Disk",
				"Please insert new disk for volume %d\n",
				od_empty - 1, 0, 0, 0, 0, 0, 0, 0);
		}
		else {
			struct volume *desired_vol = &od_vol[od_empty - 1];
			
			vol_panel_disk_num(od_panel_abort,
				od_empty - 1,
				PR_DRIVE_OPTICAL,
				/* FIXME: ho do we know which drive? */
				0,		/* drive number */
				desired_vol,
				FALSE,		/* wrong_disk */
				&od_vol_tag);
			od_alert_present = TRUE;
		}
		s = spln (od_spl);
		goto out;
	}

	/* continue to request specific volume until inserted */
	if (od_specific) {
		v = od_specific;
		dbug_vol (("req_vol: specific v%d ",
			v - od_vol, 0, 0, 0, 0));	
	} else {
		for (v = od_vol; v < &od_vol[NVOL]; v++)
			if (v->v_flags & VF_WANTED)
				break;
		if (v == &od_vol[NVOL]) {
			dbug_vol (("req_vol: NO VOL? ", 0, 0, 0, 0, 0));
			goto out;
		}
		dbug_vol (("req_vol: found v%d ",
			v - od_vol, 0, 0, 0, 0));	
	}
	dbug_vol (("req_vol: make_empty IN ", 0, 0, 0, 0, 0));
	if (od_make_empty() == 0) {
		dbug_vol (("req_vol: make_empty == 0 OUT ",
			0, 0, 0, 0, 0));
		goto out;
	}
	dbug_vol (("req_vol: make_empty OUT ", 0, 0, 0, 0, 0));
	od_specific = v;
	if(((major(rootdev) == od_blk_major) &&
	    (!(od_vol[OD_VOL(rootdev)].v_flags & VF_INSERTED))) ||
	    (panel_req_port == PORT_NULL)) {
		splx (s);
		od_alert ("Insert Disk", "Please insert disk \"%s\" for volume" 
			" %d\n",
			v->v_label->dl_label, v - od_vol, 0, 0, 0, 0, 0, 0);
		s = spln (od_spl);
	}
	else {
		vol_panel_disk_label(od_panel_abort,
			v->v_label->dl_label,
			PR_DRIVE_OPTICAL,
			/* FIXME: ho do we know which drive? */
			0,			/* drive number */
			v,
			FALSE,			/* wrong_disk */
			&od_vol_tag);
		od_alert_present = TRUE;
	}
out:
	dbug_vol (("req_vol: OUT\n", 0, 0, 0, 0, 0));
	splx (s);
}

od_alert (title, msg, p1, p2, p3, p4, p5, p6, p7, p8)
{
	register struct volume *v;

	alert (60, 8, title, msg, p1, p2, p3, p4, p5, p6, p7, p8);
	printf ("%L(press 'n' key if disk is not available)\n");

#if	DEBUG
	if ((od_dbug & DBUG_SHOWVOL) == 0)
		return;
	printf ("%L\n");
	for (v = od_vol; v < &od_vol[NVOL]; v++) {
		if ((v->v_flags & VF_BUSY) == 0)
			continue;
		printf ("%Lv%d \"%s\", refcnt 0x%x", v - od_vol,
			v->v_label->dl_label, v->v_refcnt);
		/* printf ("%L, tag 0x%x", v->v_label->dl_tag); */
		if (v->v_flags & VF_INSERTED)
			printf ("%L, in d%d", v->v_drive);
		if (v->v_flags & VF_WANTED)
			printf ("%L, WANTED");
		printf ("%L\n");
	}
#endif	DEBUG
}

od_eject()
{
	register struct drive *d;
	register int dev;
	struct nvram_info ni;

	nvram_check (&ni);
	for (d = od_drive; d < &od_drive[NOD]; d++) {
		if ((d->d_flags & (DF_EXISTS|DF_SPINNING)) ==
		    (DF_EXISTS|DF_SPINNING)) {
		    
			/* don't eject vol #0 if hardware password set */
			if (d->d_vol - od_vol == 0 && ni.ni_hw_pwd == HW_PWD &&
			    ni.ni_allow_eject == 0 && ni.ni_any_cmd == 0)
				continue;
			dev = OD_DEV(d->d_vol - od_vol, 0);
			od_sync (dev);
			od_cmd (dev, OMD_EJECT_NOW, 0, 0, 0, 0, 0, 0, 0,
				RTPRI_NONE);
		}
	}
}

void od_panel_abort(void *param, int tag, int response_val) {
	/*
	 * Called by vol driver upon receipt of a "disk not available"
	 * message. param is actually a struct volume *.
	 */
	od_alert_abort = (int)param;
}

#ifdef	DMA_INIT_ON_ERR
static void od_dma_init(struct ctrl *ctrl)
{
	/*
	 * Kludge: inititialize DMA channel. Per K. Grundy's
	 * suggestion, we have to enable a short DMA transfer from
	 * device to memory, then disable it. 
	 */
	char dummy_array[0x30];
	char *dummy_p;
	
	dummy_p = DMA_ENDALIGN(char *, dummy_array);
	dma_list(&ctrl->c_dma_chan, 
		ctrl->c_dma_list, 
		(caddr_t)dummy_p,
		0x20,
		pmap_kernel(), 
		DMACSR_READ,
		NDMAHDR, 
		0, 		/* before ?? */
		0,		/* after  ?? */
		0,		/* secsize */
		0);		/* rathole_va */
	dma_start(&ctrl->c_dma_chan, 
		ctrl->c_dma_list, 
		DMACSR_READ);
	DELAY(10);
	dma_abort(&ctrl->c_dma_chan);
}
#endif	DMA_INIT_ON_ERR

#endif	NOD






