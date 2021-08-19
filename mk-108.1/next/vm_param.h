/* 
 * Copyright (c) 1987 NeXT, Inc.
 */ 

#ifndef	_NeXT_VM_PARAM_
#define	_NeXT_VM_PARAM_

#import <sys/types.h>

#define BYTE_SIZE	8	/* byte size in bits */
#define BYTE_MSF	1	/* Most significant byte first in word */

/*
 *	These are variables so we can change the page size by just rebooting.
 */

#ifndef	ASSEMBLER
extern int
	NeXT_page_size,		/* bytes per NeXT page */
	NeXT_page_mask,		/* mask for page offset */
	NeXT_page_shift,	/* number of bits to shift for pages */
	NeXT_is,		/* initial shift: # of high VA bits to skip */
	NeXT_tia,		/* table index a */
	NeXT_tib,		/* table index b */
	NeXT_pt1_entries,	/* number of entries per level 1 page table */
	NeXT_pt2_entries,
	NeXT_pt1_size,		/* size of a single level 1 page table */
	NeXT_pt2_size,
	NeXT_pt1_shift,		/* bits to shift for pt1 index */
	NeXT_pt2_shift,
	NeXT_pt1_mask,		/* mask to apply for pt1 index */
	NeXT_pt2_mask,
	NeXT_pt2_maps;		/* a single pt2 maps this much VA space */
#endif	ASSEMBLER

/*
 *	Most ports place the kernel in the high half of the total
 *	32-bit virtual address (VA) space, the u-area and kernel stack
 *	just below that and the user space starting at virtual
 *	location zero.  We disagree with this for several reasons
 *	(on the VAX the hardware gives you no choice).  The user
 *	should be able to address the entire 4GB virtual space now
 *	that Mach makes better use of virtual memory concepts
 *	(mapped files, shared memory, copy-on-write etc.) -- we need
 *	the extra 2GB for these things.
 *
 *	Some processes may also want to use the MMU transparent
 *	translation (tt) registers to access devices (e.g. video memory)
 *	without constantly invalidating the MMU address translation cache.
 *	Because the MMU tt registers map chunks of VA space directly to
 *	their corresponding physical address (PA) spaces this fragments
 *	the space even more and we'll need more VA space to compensate for it.
 *	Another goal is catching illegal zero pointer references
 *	in both the kernel and user address spaces.
 *	We'd also like to use the MMU tt registers to bypass address
 *	translation for the kernel text, data, bss and system page table
 *	areas.
 */

#define	NeXT_MIN_PAGE_SIZE	8192
#define	NeXT_MAX_PAGE_SIZE	32768

#define	VM_MIN_ADDRESS	((vm_offset_t) 0)
#define	VM_MAX_ADDRESS	((vm_offset_t) 0xfffffffc)

/* allow 32 MB of kernel virtual space */
#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t) 0x10000000)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t) 0x12000000)

#define	NeXT_KERNEL_TEXT_ADDR	0x04000000

#define INTSTACK_SIZE		4096		/* interrupt stack size */
#define	MAX_REGIONS		8		/* max regions of memory */

/*
 *	Convert bytes to pages and convert pages to bytes.
 *	No rounding is used.
 */

#define	NeXT_btop(x)	(((unsigned)(x)) >> NeXT_page_shift)
#define	NeXT_ptob(x)	(((unsigned)(x)) << NeXT_page_shift)

/*
 *	Round off or truncate to the nearest page.  These will work
 *	for either addresses or counts.  (i.e. 1 byte rounds to 1 page
 *	bytes.
 */

#define NeXT_round_page(x)	((((unsigned)(x)) + NeXT_page_size - 1) & \
					~(NeXT_page_size-1))
#define NeXT_trunc_page(x)	(((unsigned)(x)) & ~(NeXT_page_size-1))

/*
 *	Conversion between NeXT pages and VM pages.
 */

#define trunc_NeXT_to_vm(p)	(atop(trunc_page(NeXT_ptob(p))))
#define round_NeXT_to_vm(p)	(atop(round_page(NeXT_ptob(p))))

#endif	_NeXT_VM_PARAM_

