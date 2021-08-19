/*
 * scsiconf.c -- scsi configuration data structures
 * KERNEL VERSION
 *
 * HISTORY
 * 24-Feb-89	Doug Mitchell (dmitch) at NeXT
 *	Added st (SCSI tape) support
 * 26-Jan-89	Doug Mitchell (dmitch) at NeXT
 *	Added sg (generic SCSI) support
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <sys/buf.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/kernel.h>
#import <sys/conf.h>
#import <sys/proc.h>
#import <kern/task.h>
#import <vm/vm_kern.h>
#import <sys/dk.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <next/printf.h>
#import <nextdev/disk.h>
#import <nextdev/busvar.h>
#import <nextdev/dma.h>
#import <nextdev/scsireg.h>
#import <nextdev/scsivar.h>

/*
 * sd -- SCSI disk device
 */
#import <sd.h>
#if NSD > 0
#import <nextdev/sdvar.h>
/*
 * The sd_vol_check thread does I/O via sd_sd, sd_sdd[NSD].
 */
extern struct scsi_dsw sd_sdsw;
struct scsi_device sd_sd[NSD+1];
struct scsi_disk_drive sd_sdd[NSD+1];
#endif NSD > 0

/*
 * st -- SCSI tape device
 */
 
#import <st.h>
#if NST > 0
#import <nextdev/stvar.h>
extern struct scsi_dsw st_sdsw;
struct scsi_device st_sd[NST];
struct scsi_tape_device st_std[NST];
#endif NST > 0

/*
 * sg -- generic SCSI device
 */
 
#import <sg.h>
#if NSG > 0
#import <nextdev/sgvar.h>
extern struct scsi_dsw sg_sdsw;
struct scsi_device sg_sd[NSG];
struct scsi_generic_device sg_sgd[NSG];
#endif NSG > 0

/*
 * device switch list 
 */
struct scsi_dsw *scsi_sdswlist[] = {
#if NSD > 0
	&sd_sdsw,	/* scsi disks */
#endif NSD > 0
#if NST > 0
	&st_sdsw,	/* scsi tape */
#endif NST > 0
#if NSG > 0
	&sg_sdsw,	/* scsi generic */
#endif NSG > 0
	0		/* end of list */
};

/*
 * sc -- NCR53C90 SCSI controller chip
 */
#import <sc.h>
#if NSC > 0
#import <nextdev/scvar.h>
extern struct scsi_csw sc_scsw;
struct scsi_ctrl sc_sc[NSC];
struct scsi_5390_ctrl sc_s5c[NSC];
struct sf_access_head sf_access_head[NSC];
#endif NSC > 0

/* end of scsiconf.c */

