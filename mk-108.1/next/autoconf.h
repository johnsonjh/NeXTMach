/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 22-Jan-90  Gregg Kellogg (gk) at NeXT
 *	Created
 */ 

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the bus device tables.
 * Available devices are determined (from possibilities mentioned
 * in ioconf.c), and the drivers are initialized.
 */

#import <sys/types.h>
#import <kern/mach_types.h>
#import <nextdev/busvar.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern int	dkn;		// number of iostat dk numbers assigned so far
extern struct	bus_hd bus_hd;	// bus information
extern func	poll_intr[];
extern func ipl3_scan[], ipl4_scan[], ipl6_scan[], ipl7_scan[];
extern void *ipl3_arg[], *ipl4_arg[], *ipl6_arg[], *ipl7_arg[];

void configure(void);
int intr_spurious(void);
int install_polled_intr (int which, func intr);
int install_scanned_intr (int which, func intr, void *arg);
int uninstall_polled_intr(int which, func intr);
int uninstall_scanned_intr(int which);
caddr_t map_addr(register caddr_t addr, int size);
int dev_find (
	register caddr_t addr,
	int unit,
	int ipl,
	register struct bus_driver *bdr,
	char *devname);
caddr_t ioaccess (vm_offset_t phys, vm_offset_t virt, vm_size_t size);
#if	MACH
void slave_config(void);
#endif	MACH
void swapconf(void);


