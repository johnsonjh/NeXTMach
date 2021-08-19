/*	@(#)sf_access.h 	1.0	02/07/90	(c) 1990 NeXT	*/

/* 
 **********************************************************************
 * sf_access.h -- Structure definition for Floppy/SCSI access mechanism
 *
 * HISTORY
 * 07-Feb-89	Doug Mitchell at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#ifndef	_SF_ACCESS_
#define _SF_ACCESS_

#import <sys/types.h>
#import <kern/queue.h>
#import <kern/xpr.h>

/*
 * 	Structures for arbitrating between SCSI, floppy and OD for access to
 *	hardware. 
 *
 *	A device may request exclusive use of the hardware, in which case no
 *	other device may use the hadrware in conjunction with this device.
 *	Normally, devices may operate concurrently. In the current 
 *	implementation, on flopyy needs exclusive access.
 *
 *	Before each driver begins an operation on the hardware, it must call
 *	sfa_arbitrate(). This function determines whether or not the hardware
 *	is available (depending on other existing accesses and this device's
 *	intentions); if not, the current request is enqueued on
 *	a sf_access_head. Otherwise, the current request is executed.
 *
 *	When a driver is finished with the hardware, it calls sfa_relinquish().
 *	This checks to see if any requests are pending; if so, all 
 *	non-colliding requests are dequeued and started.
 */
 
struct sf_access_device {
	void 			(*sfad_start)(void *sfad_arg);	
						/* function to call to start
						 * I/O */
	void 			*sfad_arg;	/* arg to pass to sfad_start */
	queue_chain_t		sfad_link;	/* for enqueuing */
	int 			sfad_flags;	/* See SFDF_xxx, below */
};

typedef struct sf_access_device *sf_access_device_t;

#define SFDF_OWNER	0x00000001		/* This device currently has
						 * access to the hardware */
#define SFDF_XDBG	0x00000002		/* XDBG enable */
#define SFDF_EXCL	0x00000004		/* device requires exclusive
						 * access */

/* 
 * One of these per controller set (sc0, fc0, od0, etc...).
 */
struct sf_access_head {
	simple_lock_data_t	sfah_lock;	/* all accesses to this struct
						 * must use this lock */
	u_char 			sfah_wait_cnt;	/* # of devices awaiting 
						 * access */
	queue_head_t		sfah_q;		/* queue of sf_access_device's
						 */
	int			sfah_flags;	/* See SFHF_xxx, below */
	int			sfah_last_dev;	/* for maintining DMA interrupt
						 * linkage by SCSI and floppy 
						 */
	int			sfah_excl_q;	/* # of enqueued devices 
						 * needing exclusive access */
	int			sfah_busy;	/* # of devices currently using
						 * bus */
};

typedef struct sf_access_head *sf_access_head_t;

/*
 * sfah_flags
 */
#define SFHF_EXCL	0x00000001		/* current device has
						 * exclusive access */
/*
 * sfah_last_dev values. 
 */
#define SF_LD_SCSI	0x00000001		/* last active device = SCSI */
#define SF_LD_FD	0x00000002		/* last active device = fd */
#define SF_LD_NONE	0x00000000		/* anyone else */

extern struct sf_access_head sf_access_head[];	/* in nextdev/scsiconf.c */
int sfa_arbitrate(sf_access_head_t sfah, sf_access_device_t sfad);
void sfa_relinquish(sf_access_head_t sfah, 
	sf_access_device_t sfad, 
	int last_device);
void sfa_abort(sf_access_head_t sfah, 
	sf_access_device_t sfad,
	int last_device);

#ifdef	DEBUG
#define	XPR_SFA(sfad, x) \
	if(sfad->sfad_flags & SFDF_XDBG) { \
		XPR(XPR_SCSI, x); XPR(XPR_FD, x); \
	} 
#else	DEBUG
#define XPR_SFA(sfad, x)
#endif	DEBUG
#endif _SF_ACCESS_





