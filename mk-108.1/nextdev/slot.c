/*
 *	Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Added thread as first argument to pmap_tt().
 *
 * 27-May-89  Avadis Tevanian, Jr. (avie) at NeXT, Inc.
 *	Created (based on video driver).
 */ 

/*
 * Slot device driver
 *
 * Maps the slot space into virtual space with the MMU transparent translation.
 * The virtual space used is slot dependent because transparent translation
 * does not offset the PA and VA (it's always 1:1).
 */

#import <sys/errno.h>
#import <sys/types.h>
#import <sys/task.h>
#import <sys/thread.h>
#import <sys/ioctl.h>
#import <vm/vm_map.h>
#import <vm/pmap.h>
#import <next/cpu.h>
#import <next/pcb.h>
#import <next/vmparam.h>
#import <nextdev/slot.h>

slotopen(dev)
	dev_t	dev;
{
	int	slot = minor(dev);

	/* slot number must be even and disallow slot 0 to be mapped */
	if (((slot % 2) != 0) || ((slot/2) >= SLOTCOUNT) || (slot <= 0))
		return(ENOTTY);
	return (0);
}

slotioctl(dev, cmd, data, flag)
	dev_t dev;
	caddr_t data;
{
	struct pcb	*pcb = current_thread()->pcb;
	vm_map_t	map = current_task()->map;
	vm_offset_t	addr;
	int		slot = minor(dev);

	addr = slot * SLOTSIZE;
	switch (cmd) {

	case SLOTIOCGADDR:
		*(vm_offset_t*) data = addr;
		
		/*
		 *  Do this here instead of slotopen() so the unrelated
		 *  ioctls can be used without actually mapping in
		 *  the slot spaces.
		 */
		if (pcb->pcb_flags & PCB_TT1)
			return (EBUSY);		/* already open */

		/*
		 *  Set the maximum address that can be used to the beginning
		 *  of video memory space.  XXX - Do some checking here?
		 */
		if (vm_map_max(map) > addr)
			vm_map_max(map) = addr;

		/* setup transparent translation */
		pmap_tt (current_thread(), PMAP_TT_ENABLE, addr, SLOTSIZE*2,
			PMAP_TT_NON_CACHEABLE);
		break;

	/* can't do this in a close routine because it's needed per-process */
	case SLOTIOCDISABLE:
		if ((pcb->pcb_flags & PCB_TT1) == 0)
			return (ENXIO);
			
		/* disable transparent translation */
		pmap_tt (current_thread(), PMAP_TT_DISABLE, 0, 0, 0);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}
