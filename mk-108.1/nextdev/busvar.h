/* 
 * Copyright (c) 1987 Next, Inc.
 *
 * HISTORY
 * 11-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Changed bc_tab from a struct buf * to a queue_head_t.
 *
 * 04-Mar-87  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#ifndef	_BUSVAR_
#define _BUSVAR_

#import <kern/queue.h>
#import <sys/callout.h>

/*
 * This file contains definitions related to the kernel structures
 * for dealing with the device bus.
 *
 * Each bus has a bus_hd structure.
 * Each bus controller which is not a device has a bus_ctrl structure.
 * Each bus device has a bus_device structure.
 */

#ifndef ASSEMBLER
/*
 * Per-bus structure.
 *
 * At boot time we determine the devices attached to the bus.
 * Additional devices may be added at a later time via the
 * loadable device driver mechanism.
 *
 * During normal operation, resources are allocated and returned
 * to the structures here.
 * 
 * When bus resources are needed and not available, or if a device
 * which can tolerate no other bus activity gets on the bus,
 * then device drivers may have to wait to get to the bus and are
 * queued here.
 */
struct	bus_hd {
	struct	bus_device *bh_actf;	/* head of queue to transfer */
	struct	bus_device *bh_actl;	/* tail of queue to transfer */
	short	bh_users;		/* transient use count */
	short	bh_xclu;		/* exclusive use of bus */
};

/*
 * Per-controller structure.
 * (E.g. one for each disk and tape controller)
 *
 * If a controller has devices attached, then there are
 * cross-referenced bus_drive structures.
 * This structure is the one which is queued in bus resource wait,
 * and saves the information about bus resources which are used.
 * The queue of devices waiting to transfer is also attached here.
 */
struct bus_ctrl {
	/* start of fields initialized by ioconf.c */
	struct	bus_driver *bc_driver;
	short	bc_ctrl;	/* controller index in driver */
	short	bc_ipl;		/* interrupt level */
	caddr_t	bc_addr;	/* address of device in I/O space */
	/* end of fields initialized by ioconf.c */

	struct	bus_hd *bc_hd;	/* bus this controller is on */
	struct	bus_device *bc_device;
/* the driver saves the prototype command here for use in its go routine */
	int	bc_cmd;		/* communication to go() */
	queue_head_t bc_tab;	/* queue of devices for this controller */
	int	bc_active;	/* Bus controller is active */
	short	bc_alive;	/* controller exists */
};

/*
 * Per ``device'' structure.
 * (A controller has devices -- everything else is a ``device''.)
 *
 * If a controller has many drives attached, then there will
 * be several bus_device structures associated with a single bus_ctrl
 * structure.
 *
 * This structure contains all the information necessary
 * to run a bus device.  It also contains information
 * for slaves of bus controllers as to which device on the slave
 * this is.  A flags field here can also be given in the system specification
 * and is used to tell which serial lines are hard wired or other device
 * specific parameters.
 */
struct bus_device {
	/* start of fields initialized by ioconf.c */
	struct	bus_driver *bd_driver;
	short	bd_unit;	/* unit number on the system */
	short	bd_ctrl;	/* mass ctrl number; -1 if none */
	short	bd_slave;	/* slave on controller */
	char	bd_ipl;		/* interrupt level of device */
	short	bd_dk;		/* if init 1 set to number for iostat */
	int	bd_flags;	/* parameter from system specification */
	caddr_t	bd_addr;	/* address of device in I/O space */
	char	*bd_name;	/* device name */
	/* end of fields initialized by ioconf.c */

	short	bd_alive;	/* device exists */
	short	bd_type;	/* driver specific type information */
/* this is the forward link in a list of devices on a controller */
	struct	bus_device *bd_forw;
/* if the device is connected to a controller, this is the controller */
	struct	bus_ctrl *bd_bc;
	struct	bus_hd *bd_hd;
};

/*
 * Per-driver structure.
 *
 * Each device driver defines entries for a set of routines
 * as well as an array of types which are acceptable to it.
 * These are used at boot time by the configuration program.
 */
struct bus_driver {
	int	(*br_probe)();		/* see if a driver is really there */
	int	(*br_slave)();		/* see if a slave is there */
	int	(*br_attach)();		/* setup driver for a slave */
	int	(*br_go)();		/* start transfer */
	int	(*br_done)();		/* complete transfer */
	int	(*br_intr)();		/* service interrupt */
	int	(*br_init)();		/* initialize device */
	int	br_size;		/* device register size */
	char	*br_dname;		/* name of a device */
	struct	bus_device **br_dinfo;	/* backpointers to bus_dinit structs */
	char	*br_cname;		/* name of a controller */
	struct	bus_ctrl **br_cinfo;	/* backpointers to bus_cinit structs */
	short	br_flags;		/* driver flags */
#define	BUS_XCLU	0x0001		/* want exclusive use of bus */
#define	BUS_SCSI	0x0002		/* this is SCSI controller */
};
#endif	!ASSEMBLER

#define	BUS_CANTWAIT	0x01		/* don't block me */

#ifndef ASSEMBLER
#ifdef KERNEL
/*
 * Bus related kernel variables
 */
extern struct	bus_hd bus_hd;

/*
 * bus_cinit and bus_dinit initialize the mass storage controller
 * and ordinary device tables specifying possible devices.
 */
extern	struct	bus_ctrl bus_cinit[];
extern	struct	bus_device bus_dinit[];

/*
 * Support for polled interrupts on the bus.
 */
#define	NPOLL	8			/* max number of polling routines */
extern func poll_intr[];		/* polled routines to call */
#define	POLLED_IPL	5		/* polling ipl */
#define ipltospl(ipl) \
	(SR_SUPER | ((ipl) << 8))
#endif KERNEL

/* pseudo device initialization routine support */
struct pseudo_init {
	int	ps_count;
	int	(*ps_func)();
};
extern struct pseudo_init pseudo_inits[];

#endif !ASSEMBLER
#endif _BUSVAR_

