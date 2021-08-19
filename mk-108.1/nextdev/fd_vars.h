/*	@(#)fd_vars.h	2.0	01/24/90	(c) 1990 NeXT	
 *
 * fd_vars.h -- Internally used data structures and constants for Floppy 
 *		Disk driver
 *
 * KERNEL VERSION
 *
 * HISTORY
 *  2-Aug-90	Gregg Kellogg (gk) at NeXT
 *	Changes for programatic disksort interface.
 * 10-Jul-90	Gregg Kellogg (gk) at NeXT
 *	Added support for procedural access to disk queue.
 * 24-Jan-90	Doug Mitchell at NeXT
 *	Created.
 *
 */

#ifndef	_FDVARS_
#define _FDVARS_

#import <sys/types.h>
#import <kern/queue.h>
#import <kern/lock.h>
#import <kern/xpr.h>
#import <sys/disksort.h>
#import <sys/uio.h>
#import <sys/time.h>
#import <sys/time_stamp.h>
#import <sys/signal.h>
#import <kern/mach_types.h>
#import <next/printf.h>
#import <vm/vm_map.h>
#import <nextdev/dma.h>
#import <nextdev/disk.h>
#import <nextdev/busvar.h>
#import	<nextdev/fd_reg.h>
#import <nextdev/fd_extern.h>
#import <nextdev/ldd.h>
#import <nextdev/sf_access.h>

#define FD_MAX_DMA		0x10000

/*
 * one per controller. Statically allocated. 
 */
struct fd_controller {
	volatile struct fd_cntrl_regs *fcrp;	/* hardware registers */
	struct bus_ctrl		*bcp;		/* parallel bus controller */
	simple_lock_data_t	req_lock;	/* protects access to req_q */
	queue_head_t		req_q;		/* Iput queue, of fd_devices.
						 * This field is protected by
						 * req_lock. */
	simple_lock_data_t	flags_lock;	/* protects access to flags
						 * (these can be written
						 * by the controller 
						 * thread, by device level
						 * code, and by interrupt
						 * code) */
	volatile int		flags;		/* see FCF_xxx, below. This 
						 * field is protected by
						 * req_lock. */
	fd_ioreq_t		fdiop;		/* ptr to current I/O 
						 * request (from driver
						 * level) */
	fd_ioreq_t		fdiop_i;	/* ptr to current I/O 
						 * request (interrupt level) */
	u_char			ipl;
	u_char			flpctl_sh;	/* shadow of FLPCTL register */
	struct	dma_chan 	dma_chan;	/* DMA info */
	dma_list_t		dma_list;	/* DMA segment info */
	u_char			*sczfst;	/* SCSI FIFO status */
	u_char			*sczctl;	/* SCSI dma control */
	sf_access_head_t 	sfahp;		/* scsi/floppy access struct */
	struct sf_access_device sfad;		/* for enqueueing requests */
	simple_lock_data_t	holding_lock;	/* protects acces to 
						 * holding_q */
	queue_head_t		holding_q;	/* queue of requests awaiting
						 * disk insertion */
	int 			last_dens;	/* last value of density 
						 * used */

};

typedef struct fd_controller *fd_controller_t;

/*
 * fd_controller.flags
 */
#define FCF_COMMAND	0x00000001		/* new work to do in fc_req_q
						 */		
#define FCF_THR_ALIVE	0x00000002		/* fc_thread has started */
#define FCF_TIMEOUT	0x00000008		/* timer expired */
#define FCF_TIMER_RUN	0x00000010		/* timer running */
#define FCF_INT_PEND	0x00000040		/* interrupt pending */
#define FCF_INT_EXPECT	0x00000080		/* interrupt expected */
#define FCF_FC_ACCESS	0x00000200		/* arbitration granted by 
						 * sf_access mechanism */
#define FCF_NEEDSINIT	0x00000400		/* 82077 needs reset/init */
#define FCF_INT_INSTALL	0x00002000		/* scanned interrupts 
						 * installed */
#define FCF_DMAING	0x00004000		/* DMA in progress */
#define FCF_DMAREAD	0x00008000		/* DMA read (device to mem) */
#define FCF_DMAERROR	0x00010000		/* DMA error detected */
#define FCF_INITD	0x00020000		/* DMA channel initialized */

/*
 * one per volume. Any thread (system/user) writing to one of these structs
 * must acquire fd_device.lock.
 *
 * We allocate these structs dynamically. When a volume is closed, we eject
 * it and deallocate the fd_volume struct and everything in it.
 */
struct fd_volume {
	simple_lock_data_t	lock;		/* protects all non-atomic 
						 * "read-modify-writes" (e.g.,
						 * (fvp->flags |= xx) to this
						 * struct */
	u_int			drive_num;	/* logical drive # this volume
						 * is in. DRIVE_UNMOUNTED means
						 * not inserted. */
	u_int			intended_drive;	/* the drive we're trying to
						 * mount this volume in */
	struct bus_device	*bdp;		/* parallel bus device of 
						 * drive_num (or last drive
						 * mounted on) */
	int 			volume_num;	/* from minor number */
	struct disk_label 	*labelp;	/* disk label */
	struct buf		local_buf;	/* for enqueueing special
						 * commands by fd_command() */
	fd_ioreq_t		local_ioreq_p;	/* for specifying special
						 * commands by fd_command() */
	struct fd_ioreq		dev_ioreq;	/* every I/O request is
						 * placed here when command is 
						 * passed to controller
						 * thread */
	struct ds_queue		io_queue;	/* all I/O requests queued here
						 *  in fdstrategy() */
	struct buf 		rio_buf;	/* raw i/o buf */
	u_int 			flags;		/* see FDF_xxx, below */
	u_char 			blk_open;	/* block device open - one bit 
						 * per partition */
	u_char 			raw_open;	/* raw device open - one bit 
						 * per partition */
	queue_chain_t		io_link;	/* for enqueueing by controller
						 * thread */
	int 			state;		/* see FVSTATE_xxx, below */
	uid_t			owner;		/* owner at last open */
	int			vol_tag;	/* used for vol_panel calls. */
	
	/*
	 * the following are used for reads and writes only. Start_sect
	 * is the current logical sector of the transfer. Bytes_to_go is the
	 * total byte count yet to be transferred. The current request is
	 * physically at current_sect for current_byte_count. (This may be
	 * in the spare area.)
	 */
	u_int 			start_sect;
	u_int			bytes_to_go;
	caddr_t			start_addrs;
	u_int			current_sect;
	u_int			current_byte_cnt;
	caddr_t			current_addrs;
	int			inner_retry_cnt;
	int			outer_retry_cnt;
	int			io_flags;	/* see FVIOF_xxx, below */
	char			*sect_buf;	/* used during short writes */
	u_int			padded_byte_cnt;
	
	/*
	 * physical device info.
	 */
	struct fd_format_info	format_info;	/* density, #sects, etc. */
	u_int			bratio;		/* FS block size / sect_size */
};

typedef	struct fd_volume *fd_volume_t;

/*
 * fd_volume.flags
 */
#define FVF_NEWVOLUME	0x00000004		/* set by driver level code
						 * on first open of new 
						 * volume */
#define FVF_ALERT	0x00000008		/* an alert panel is present 
						 * asking for this volume */
#define FVF_NOTAVAIL	0x00000010		/* user has aborted alert 
						 * panel's request for new
						 * disk */
/*
 * fd_volume.io_flags
 */
#define FVIOF_READ	0x00000001		/* 1 = read  0 = write */
#define FVIOF_LPART	0x00000002		/* 1 = last partition (disables
						 * bad block mapping) */
						 
#define DRIVE_UNMOUNTED NFD			/* this volume currently is not
						 * present in a drive. */
#define VALID_DRIVE(drive_num) 	(drive_num < NFD) 					 
/*
 * fd_volume.state values 
 */
#define FVSTATE_IDLE		0
#define FVSTATE_EXECUTING	1
#define FVSTATE_RETRYING	2
#define FVSTATE_RECALIBRATING	3

/*
 * retry counts
 *
 * xx_NORM = retry counts used during normal operation
 * xx_CONF = retry counts used during autoconf
 */
#define INNER_RETRY_NORM	3  		/* retries between recals */
#define OUTER_RETRY_NORM	3		/* # of recalibrates */
#define INNER_RETRY_CONF	2  		/* retries between recals */
#define OUTER_RETRY_CONF	1		/* # of recalibrates */
#define FD_ATTACH_TRIES		2		/* # of attempts during 
						 * fd_attach_com I/O */

/*
 * one per drive. Statically allocated.
 */	
struct fd_drive {
	simple_lock_data_t	lock;		/* protects all "read-modify-
						 * writes" (fdp->flags |= xx)
						 * to this struct */
	int 			unit;		/* controller-relative drive
						 * number */
	fd_controller_t		fcp;		/* controller to which this 
						 * drive is attached */
	fd_volume_t		fvp;		/* volume currently inserted
						 * in this drive. NULL means
						 * drive is empty. */
	fd_volume_t		intended_fvp;	/* volume we're waiting for */
	struct bus_device	*bdp;		/* parallel bus device */
	struct tsval 		last_access;	/* time drive last accessed */
	int			flags;		/* see FDF_xxx, below */
	int 			drive_type;	/* DRIVE_TYPE_FD288, etc. 
						 * index into fd_drive_info[])
						 */

};

typedef struct fd_drive *fd_drive_t;

/*
 * fd_drive.flags values
 */
#define FDF_PRESENT		0x00000001	/* drive is physically 
						 * present */
#define FDF_MOTOR_ON		0x00000002	/* motor is currently on */
#define FDF_EJECTING		0x00000004	/* currently ejecting a disk */

/*
 * fd_drive.drive_type values
 */
#define	DRIVE_TYPE_FD288	0

/*
 * One per drive_type (in fd_global.c).
 */
struct fd_drive_info {
	struct drive_info	gen_drive_info;		/* name, block length,
							 * etc. */
	int			seek_rate;		/* in ms */
	int 			head_settle_time;	/* ms */
	int 			head_unload_time;	/* ms */
	boolean_t		is_perpendicular;	/* perp. recording */
	boolean_t		do_precomp;		/* precomp recording */
};

/*
 * struct used to make "Sync and Eject" requests of the disk_eject()
 * thread.
 */ 
struct disk_eject_req {
	queue_chain_t	q_link;		/* for linking on fd_eject_req_q */
	fd_volume_t	fvp;		/* volume to eject */
	fd_volume_t	new_fvp;	/* intended volume */
};

/*
 * struct used for enqueueing "aborted disk insertion" requests. 
 * vol_panel_abort(), which is actually called by the vol driver thread, 
 * generates these and enqueues them on the vol_abort_q. The volume_check 
 * thread dequeues these, and marks the associated volumes in the controller's
 * holding_q with FVF_NOTAVAIL. The volume_check thread then sends a 
 * FDCMD_ABORT message to the controller I/O thread so that the aborted
 * volumes will be sent back to their callers.
 */
struct vol_abort_req {
	queue_chain_t	link;
	fd_volume_t	fvp;
	int 		tag;		/* not used (yet) */
	int 		response;	/* not used (yet). Same as ps_value in
					 * the vol_panel_resp message. */
};

/*
 * Constants for inserting into cdevsw/bdevsw
 */
#define NUM_FV		8			/* max # of volumes */
#define NUM_FVP		(NUM_FV+2)		/* Number of volume structs.
						 * One for each actual volume.
						 * One for controller device.
						 * One for vol_check thread.
						 */
#define FVP_CONTROLLER	NUM_FV			/* index of controller device's
						 * fvp */
#define FVP_VOLCHECK	(NUM_FV+1)		/* vol_check's fvp */
#define NUM_FP		2			/* # of partitions per volume.
					 	 * partition NUM_FP-1 is
						 * special "live" (versus
						 * cooked or even raw)
						 * partition. */
#define FD_LIVE_PART	(NUM_FP-1)

/*
 * dev_t to volume, partition conversion
 */
#define	FD_VOLUME(dev)		(minor(dev) >> 3)
#define	FD_PART(dev)		(minor(dev) & 0x7)
#define	FD_DEVT(major,volume,part)	(((major) << 8) | ((volume) << 3) | ((part) & 0x7))


/*
 * standard timeouts, in milliseconds
 */
#define FD_LONG_TIMEOUT	1

#define TO_EJECT	2000
#define TO_MOTORON	500			/* just a delay */
#define TO_MOTOROFF	10			/* just a delay */
#define TO_IDLE		2000			/* max motor on idle time */
#ifdef	FD_LONG_TIMEOUT
#define TO_SIMPLE	10000			/* get status, etc. */
#define TO_RW		10000			/* r/w timeout */
#define TO_RECAL	10000			/* recal */
#define TO_SEEK		10000			/* seek */
#else	FD_LONG_TIMEOUT
#define TO_SIMPLE	10			/* get status, etc. */
#define TO_RW		1000			/* r/w timeout */
#define TO_RECAL	1000			/* recal */
#define TO_SEEK		400			/* seek */
#endif	FD_LONG_TIMEOUT
#define TO_SFA_ARB	30000			/* wait 30 seconds for 
						 * sfa_arbitrate() to succeed 
						 */

/*
 * low-level timeouts in microseconds
 */
#define TO_EJECT_PULSE	3			/* width of eject pulse */
#define TO_FIFO_RW	10000			/* max time to wait to r/w 
						 * cmd/status bytes from/to
						 * FIFO. This timeout covers
						 * all 'n' command bytes or
						 * all 'n' status bytes. A 
						 * value of 2000 sometimes 
						 * times out in 500 us! */
#define VC_DELAY_NORM	1000000			/* normal volume_check delay */
#define VC_DELAY_WAIT	100000			/* volume_check delay, imminent
						 * disk insertion */

/*
 * timeouts in loops for polling mode. Use value appropriate for fastest 
 * hardware we have.
 */
/* #define LOOPS_PER_MS 	800			/* for fc_wait_intr */
#define LOOPS_PER_MS 	2000			/* for fc_wait_intr */
#define FIFO_RW_LOOP	(500 * LOOPS_PER_MS)
#define EJECT_LOOP	(2000 * LOOPS_PER_MS)
#define MOTOR_ON_LOOP	(500 * LOOPS_PER_MS)
#define MOTOR_OFF_LOOP	(50 * LOOPS_PER_MS)
#define INTR_RQM_LOOPS	400			/* loops to wait in fc_intr
					 	 * for RQM */

/*
 * private fd_ioreq.command values 
 */
#define FDCMD_NEWVOLUME		0x80	/* sent by volume_check thread to
					 * controller thread to inform of newly
					 * mounted volume */ 
#define FDCMD_ABORT		0x81	/* User has aborted request for new
					 * disk */ 

/*
 * Map from density to appropriate sector size info
 */
struct fd_density_sectsize {
	u_int	density;
	struct	fd_sectsize_info *ssip;
};

/*
 * misc. constants
 */
#ifndef	NULL
#define NULL 0
#endif	NULL

/*
 * tsval macros (should be in sys/timestamp.h?)
 */
#ifdef	notdef
#define ts_greater(ts1, ts2) 				\
(							\
	((ts1)->high_val == (ts2)->high_val) ?		\	
		((ts1)->low_val  > (ts2)->low_val) : 	\
		((ts1)->high_val > (ts2)->high_val)		\
)
#endif	notdef

/*
 * 	prototypes for functions globally visible
 *
 * in fd_driver.c:
 */
int fd_slave(fd_controller_t fcp, struct bus_device *bdp);
void fd_attach(struct bus_device *bdp);
static int fd_attach_com(fd_volume_t fvp);
int fc_probe(caddr_t hw_reg_ptr, int ctrl_num);
int fc_slave(struct bus_device *bdp, volatile caddr_t hw_reg_ptr);
void fd_attach(struct bus_device *bdp);
int fd_command(fd_volume_t fvp,
	fd_ioreq_t fdiop);
/*
 * in fd_subr.c:
 */
fd_return_t fd_start(fd_volume_t fvp);
void fd_intr(fd_volume_t fvp);
void fd_done(fd_volume_t fvp);
int fd_get_label(fd_volume_t fvp);
int fd_write_label(fd_volume_t fvp);
int fd_eject(fd_volume_t fvp);
fd_return_t fd_readid(fd_volume_t fvp, int head, struct fd_rw_stat *statp);
int fd_get_status(fd_volume_t fvp, struct fd_drive_stat *dstatp);
int fd_recal(fd_volume_t fvp);
void fd_gen_write_cmd(fd_volume_t fvp,
	struct fd_ioreq *fdiop,
	u_int block,
	u_int byte_count,
	caddr_t dma_addrs,
	int timeout);
void fc_thread_timer();
void volume_check();
void v2d_map(fd_volume_t fvp);
fd_volume_t fd_new_fv(int vol_num);
void fd_free_fv(fd_volume_t fvp);
void fd_assign_dv(fd_volume_t volp, int drive_num);
void ts_diff(struct tsval *ts1, struct tsval *ts2, struct tsval *ts_diff);
void ts_add(struct tsval *ts1, u_int micros);
boolean_t ts_greater(struct tsval *ts1, struct tsval *ts2);
void fd_thread_block(int *eventp, int event_bits, simple_lock_t lockp);
void vol_check_timeout();
void fd_setbratio(fd_volume_t fvp);
void fd_gen_seek(int density, fd_ioreq_t fdiop, int track, int head);
int fd_seek(fd_volume_t fvp, int cyl, int head);
int fd_basic_cmd(fd_volume_t fvp, int command);
int fd_live_rw(fd_volume_t fvp,
	int fs_sector,	
	int byte_count,
	caddr_t addrs,	
	boolean_t read,
	int *resid_sects);
void fd_set_density_info(fd_volume_t fvp, int density);
int fd_set_sector_size(fd_volume_t fvp, int sector_size);
struct fd_sectsize_info *fd_get_sectsize_info(int density);
int fd_raw_rw(fd_volume_t fvp,
	int sector,	
	int sect_count,
	caddr_t addrs,	
	boolean_t read);
void volume_notify(fd_volume_t fvp);

/*
 * in fd_io.c:
 */
void fc_go(struct bus_ctrl *bcp);
void fc_init(caddr_t reg);
fd_return_t fc_start(fd_volume_t fvp);
void fc_thread();
fd_return_t fc_send_byte(fd_controller_t fcp, u_char byte);
fd_return_t fc_get_byte(fd_controller_t fcp, u_char *bp);
void fc_flpctl_bset(fd_controller_t fcp, u_char bits);
void fc_flpctl_bclr(fd_controller_t fcp, u_char bits);
fd_return_t fc_specify (fd_controller_t fcp,
	int density,
	struct fd_drive_info *fdip);
fd_return_t fc_82077_reset(fd_controller_t fcp, char *error_str);
void fc_flags_bset(fd_controller_t fcp, u_int bits);
void fc_flags_bclr(fd_controller_t fcp, u_int bits);
fd_return_t fc_configure(fd_controller_t fcp, u_char density);

/*
 * in fd_cmds.c:
 */
void fc_cmd_xfr(fd_controller_t fcp, fd_drive_t fdp);
void fc_eject(fd_controller_t fcp);
void fc_motor_on(fd_controller_t fcp);
void fc_motor_off(fd_controller_t fcp);
fd_return_t fc_send_cmd(fd_controller_t fcp, fd_ioreq_t fdiop);
void fc_start_timer(fd_controller_t fcp, int microseconds);
void fc_timeout(fd_controller_t fcp);
void fc_stop_timer(fd_controller_t fcp);

/*
 * 	variables in fd_global.c
 */
extern boolean_t fd_polling_mode;
extern struct fd_controller fd_controller[];
extern struct fd_drive fd_drive[];
extern fd_volume_t fd_volume_p[];
extern struct bus_ctrl *fc_bcp[];
extern int vol_check_event;
extern char vol_check_alive;
extern int vol_check_delay;
extern simple_lock_data_t vol_check_lock;
extern queue_head_t disk_eject_q;
extern int disk_eject_event;
extern simple_lock_data_t disk_eject_lock;
extern boolean_t fc_thread_init;
extern boolean_t fc_thread_timer_started;	
extern struct fd_drive_info fd_drive_info[];
extern struct fd_disk_info fd_disk_info[];
extern struct fd_density_info fd_density_info[];
extern struct fd_density_sectsize fd_density_sectsize[];
extern lock_data_t fd_open_lock;
extern queue_head_t vol_abort_q;
extern simple_lock_data_t vol_abort_lock;
extern struct reg_values fd_return_values[];
extern int fd_inner_retry;
extern int fd_outer_retry;
extern int fd_blk_major;
extern int fd_raw_major;

#ifdef	DEBUG
extern struct reg_values fd_command_values[];
extern struct reg_values fv_state_values[];
extern struct reg_values fc_opcode_values[];
#endif	DEBUG

/*
 * prototypes for global function declarations accessed via [cb]devsw
 */
int fdopen(dev_t dev, int flag);
int fdclose(dev_t dev);
int fdread(dev_t dev, struct uio *uiop);
int fdwrite(dev_t dev, struct uio *uiop);
int fdstrategy(struct buf *bp);
int fdioctl(dev_t dev, 
	int cmd, 
	caddr_t data, 
	int flag);

/*
 *	XPR stuff
 */
#ifdef	DEBUG
extern boolean_t fd_xpr_cmd;
extern boolean_t fd_xpr_all;
extern boolean_t fd_xpr_pio;
extern boolean_t fd_xpr_addrs;
extern boolean_t fd_xpr_thrblock;
extern boolean_t fd_xpr_io;
#define	XPRFD(x)	XPR(XPR_FD, x)
#define XDBG(x)		if(fd_xpr_all) { \
				XPRFD(x); \
			}
#define XCDBG(x)	if(fd_xpr_all | fd_xpr_cmd) { \
				XPRFD(x); \
			}
#define XPDBG(x)	if(fd_xpr_pio) { \
				XPRFD(x); \
			}
#define XADDBG(x)	if(fd_xpr_addrs) { \
				XPRFD(x); \
			}
#define XTBDBG(x)	if(fd_xpr_thrblock) { \
				XPRFD(x); \
			}
#define XIODBG(x)	if(fd_xpr_io) { \
				XPRFD(x); \
			}
#else	DEBUG
#define XDBG(x)
#define XCDBG(x)
#define XPDBG(x)
#define XADDBG(x)
#define XTBDBG(x)
#define XIODBG(x)
#endif	DEBUG

/*
 * Round a byte count up to the next sector-aligned boundary.
 */
#define	FD_SECTALIGN(count, sect_size)	\
	((((unsigned)(count) + sect_size - 1) &~ (sect_size - 1)))

#endif	_FDVARS_ 
