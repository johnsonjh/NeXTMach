/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 11-Sep-90  Gregg Kellogg (gk) at NeXT
 *	Initialize event_h and event_m to point to distinct data locations.
 *	In a debug kernel this isn't a problem, because they're both reset
 *	to point to locations on a mappable page.  In a RELEASE configuration
 *	they also need to point to distinct data locations.
 *
 * 30-Jul-90  Gregg Kellogg (gk) at NeXT
 *	Moved declaration of disr_shadow from od.c to next_init.c (config).
 *
 * 28-Jun-90  John Seamons (jks) at NeXT
 *	Fix upper byte of hostid to be 0x01.  It became 0x00 with the new
 *	machine type definitions for the NeXT_CUBE and this caused backward
 *	compatibility problems.  The machine type doesn't belong in the hostid
 *	anyway -- it is available elsewhere.
 *
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Removed obsolete code to disable realtime clock interrupt.
 *
 *  7-Mar-90  John Seamons (jks) at NeXT
 *	Include next/bmap.h.
 *
 * 01-Mar-90  Brian Pinkerton (bpinker) at NeXT
 *	Updated _edata and _end to new loader symbols.
 *
 * 25-Oct-88  Mike DeMoney (mike) at NeXT
 *	Add cpu_rev global to hold cpu board revision.
 *
 * 19-Feb-88  John Seamons (jks) at NeXT
 *	Updated to Mach release 2.
 *
 * 17-Jan-88  John Seamons (jks) at NeXT
 *	Added a memory region for device space.
 *
 * 02-Dec-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#import <gdb.h>
#import <mallocdebug.h>

#import <sys/types.h>
#import <sys/param.h>
#import <sys/systm.h>
#import <sys/time.h>
#import <sys/kernel.h>
#import <sys/reboot.h>
#import <sys/errno.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/socket.h>
#import <kern/xpr.h>
#import <sys/msgbuf.h>
#import <vm/vm_param.h>
#import <vm/vm_prot.h>
#import <vm/vm_page.h>
#import <next/scb.h>
#import <next/cpu.h>
#import <next/pcb.h>
#import <next/vm_param.h>
#import <next/cons.h>
#import <next/scr.h>
#import <next/clock.h>
#import <next/eventc.h>
#import <nextdev/snd_dspreg.h>
#import <nextdev/kmreg.h>
#import <nextdev/monreg.h>
#import <next/bmap.h>
#import <rpc/types.h>
#import <nfs/nfs.h>
#import <mon/global.h>

volatile u_char		*eventc_latch, *eventc_h, *eventc_m, *eventc_l;

/*
 * this ain't pretty, but it'll work... we need something for event_high and
 * event_middle to point to until the VM system comes up and their memory
 * is allocated, so we make them point somewhere bogus until we have their page
 */
static unsigned int event_junk_h, event_junk_m;
volatile unsigned int	*event_high = &event_junk_h;
					/* high-order 32 bits of eventc */
volatile unsigned int	*event_middle = &event_junk_m;
					/* middle 12 bits of eventc */
u_char disr_shadow;		/* for cubes with floppy protos only */

volatile int	*intrstat;	/* pointer to interrupt status reg */
vm_size_t	mem_size, pagesize, mem;
vm_offset_t	virtual_avail, virtual_end;
struct		mon_global *mon_global;
struct		mem_region mem_region[MAX_REGIONS];
struct		bootp bootp;
int		console_i, console_o;
u_char		etheraddr[6];
#define	NBOOTDEV	8
#define	NBOOTINFO	16
#define	NBOOTFILE	64
char		boot_dev[NBOOTDEV], boot_info[NBOOTINFO], boot_file[NBOOTFILE];
char		*boot_args;
int		default_page_size = 8192;	/* patchable */

#if	GDB
int	breakpoint;
#endif	GDB

#if	MALLOCDEBUG
extern int mallocdebug;
#endif	MALLOCDEBUG

#if	DEBUG
int	show_info;	/* print initilization info */
#endif	DEBUG

NeXT_init (ra, mg, cons_i, cons_o, dev, args, info, sid, mon_pagesize,
	mon_nr, mon_rp, clientetheraddr, file)
	struct mon_global *mg;
	char *dev, *args, *info, *file;
	struct mon_region *mon_rp;
	u_char *clientetheraddr;
{
	register int 	i;
	vm_offset_t	avail_start, avail_end;
	extern char	start, _DATA__bss__begin, _DATA__end;
	struct		scb *get_vbr(), *mon_scb;
	register	mem_region_t rp;
	extern int	tickadj;
	int		see_msgs, skip, size;
	char		*ap;
	extern		int cache;

	bzero (&_DATA__bss__begin, &_DATA__end - &_DATA__bss__begin);
	slot_id = sid;
	slot_id_bmap = sid;

	/* set up SCB */
	mon_scb = get_vbr();
	scb.scb_issp = mon_scb->scb_issp;	/* saved from monitor */
	scb.scb_ipc = mon_scb->scb_ipc;
#if	GDB
	scb.scb_trap[14] = mon_scb->scb_trap[14];	/* gdb trap */
#endif	GDB
	scb.scb_trap[13] = mon_scb->scb_trap[13];	/* monitor exit */
	set_vbr (&scb);

	/* figure out chip type installed */
	dma_chip = 313;
	disk_chip = 310;
	scr1 = (struct scr1*) P_SCR1;
	bmap_chip = 0;
	machine_type = MACHINE_TYPE(scr1->s_cpu_rev);
	board_rev = BOARD_REV(scr1->s_cpu_rev);

	switch (machine_type) {
	
	case NeXT_CUBE:
		cpu_type = MC68030;
		break;

	case NeXT_WARP9:
	case NeXT_X15:
	case NeXT_WARP9C:
		cpu_type = MC68040;
		slot_id_bmap += 0x00100000;	// remap byte devices to BMAP chip
		bmap_chip = (struct bmap_chip*) P_BMAP;
		break;
	}
	
	switch (cpu_type) {
	
	case MC68030:
		cache = CACR_CLR_ENA;
		break;
		
	case MC68040:
	default:
		cache = CACR_040_IE | CACR_040_DE;
		break;
	}

	intrmask = (int*) P_INTRMASK;
	intrstat = (int*) P_INTRSTAT;
	*intrmask = 0;		/* disable interrupts */
	intr_mask = 0;
	eventc_latch = (u_char*) P_EVENTC;
	eventc_h = (u_char*) (P_EVENTC + 1);
	eventc_m = (u_char*) (P_EVENTC + 2);
	eventc_l = (u_char*) (P_EVENTC + 3);
	scr2 = (int*) P_SCR2;
	*scr2 &= ~(DSP_RESET|SCR2_EKG_LED);	/* Can't take any interrupts */
	brightness = (u_char*) P_BRIGHTNESS;
	timer_csr = (u_char*) P_TIMER_CSR;
	timer_high = (u_char*) P_TIMER;
	timer_low = (u_char*) (P_TIMER+1);

	/* copy boot parameters from monitor stack */
	mon_global = mg;
	bcopy (clientetheraddr, etheraddr, 6);
	hostid = 0x01000000 | ((u_int)etheraddr[3] << 16) |
		  ((u_int)etheraddr[4] << 8) | (u_int)etheraddr[5];
	strncpy (boot_dev, dev, NBOOTDEV);
	strncpy (boot_file, file, NBOOTFILE);

	ap = args;
	if (ap) {
		while (*ap) {
			if (strncmp("pagesize=", ap, 9) == 0) {
				getval(ap+8, &pagesize);
				break;
			}
			if (strncmp("mem=", ap, 4) == 0) {
				getval(ap+3, &mem);
				mem *= 1024;
				break;
			}
			ap++;
		}
	}
	if (pagesize == 2048 || pagesize == 4096 || pagesize == 8192)
		NeXT_page_size = pagesize;
	else
		NeXT_page_size = default_page_size;

	/*
	 *	Virtual page size must be at least the physical page size.
	 */
	pmap_set_page_size();
	if (page_size < NeXT_page_size)
		page_size = NeXT_page_size;
	vm_set_page_size();

	/* configure memory regions */
	num_regions = mon_nr;
	for (i = 0, skip = 0; i < num_regions && skip == 0; i++) {
		rp = &mem_region[i];
		rp->region_type = REGION_MEM;
		rp->first_phys_addr = mon_rp[i].first_phys_addr;
		rp->last_phys_addr = mon_rp[i].last_phys_addr;
		size = rp->last_phys_addr - rp->first_phys_addr;
		
		/* limit memory used if requested */
		if (mem && (mem_size + size > mem)) {
			rp->last_phys_addr = rp->first_phys_addr + (mem - mem_size);
			skip = 1;
		}
		mem_size += rp->last_phys_addr - rp->first_phys_addr;
		if (rp->first_phys_addr) {

			/*
			 *  Always keep the msgbuf in the same place so
			 *  panic messages will be logged on the next reboot.
			 *  Overlay the monitor stack that is just beyond
			 *  the end of the last memory region.  The monitor
			 *  guarantees this stack area is big enough.
			 */
			pmsgbuf = (struct msgbuf *)
				NeXT_trunc_page (rp->last_phys_addr);
		}
	}

	for (; i < num_regions; i++) {
		rp = &mem_region[i];
		rp->region_type = REGION_MEM;
		rp->first_phys_addr = 0;
		rp->last_phys_addr = 0;
	}

	/* configure console device */
	cons_tp = &cons;
	console_i = cons_i;
	console_o = cons_o;

	switch (console_i) {

	case CONS_I_KBD:
	default:
		cons.t_dev = makedev(12, 0);		/* km */
		break;

	case CONS_I_SCC_A:
		cons.t_dev = makedev(11, 0);		/* zs */
		break;

#if 0
	case CONS_I_SCC_B:
		cons.t_dev = makedev(11, 1);		/* zs */
		break;

	case CONS_I_NET:
		break;
#endif
	}

	switch (console_o) {

	case CONS_O_BITMAP:
	default:
		cons.t_dev = makedev(12, 0);		/* km */

		/* see msgs if booting single user or asking for root dev */
		see_msgs = 0;
		ap = args;
		if (ap) {
			while (isargsep (*ap))
				ap++;
			if (*ap == '-') do {
				switch (*ap) {
					case 'a':
					case 's':
					case 'w':
						see_msgs = 1;
						break;
				}
			} while (*ap && !isargsep (*ap++));
		}

		/* if the monitor has a popup we should too */
		if (*MG (short*, MG_km_flags) & KMF_SEE_MSGS)
			see_msgs = 1;
		kminit();
		if (see_msgs)
			kmpopup (mach_title, POPUP_FULL, 0, 0, 0);
		break;

	case CONS_O_SCC_A:
		cons.t_dev = makedev(11, 0);		/* zs */
		break;

#if 0
	case CONS_O_SCC_B:
		cons.t_dev = makedev(11, 1);		/* zs */
		break;

	case CONS_O_NET:
		break;
#endif
	}
	boot_args = args;
	getargs(args, 0);

#if	GDB
	_nextdbginit();
	if (breakpoint)
		asm ("trap #14");	/* Initial early breakpoint */
#endif	GDB
#if	MALLOCDEBUG
	if (mallocdebug)
		mallocdebuginit();
#endif	MALLOCDEBUG

	printf ("NeXT ROM Monitor %d.%d v%d\n",
		mg->mg_major, mg->mg_minor, mg->mg_seq);
	
	/* hz may have been specified as a kernel argument */
	tick = 1000000 / hz;
	tickadj = 240000 / (60 * hz);		/* can adjust 240ms in 60s */

	/* Initialize keyboard monitor and line printer interface */
	mon_reset();

	/*
	 *	We won't really know our cpu number till after
	 *	we configure... but we still need to have a valid
	 *	cpu number for routines that use it.  Fortunately,
	 *	set_cpu_number will do the right thing for us here
	 *	because cpu_number doesn't know we have multi-port
	 *	memory yet.  FIXME: what does this mean for NeXT?
	 */
	set_cpu_number();

	/*
	 *	Bootstrap the pmap system.
	 *	Kernel virtual address starts at VM_KERNEL_MIN_ADDRESS.
	 */
	rp = &mem_region[0];
	avail_start = (vm_offset_t) &_DATA__end;
	avail_end = trunc_page (rp->last_phys_addr);
	pmap_bootstrap(&avail_start, &avail_end, &virtual_avail, &virtual_end);
	rp->first_phys_addr = avail_start;
}


