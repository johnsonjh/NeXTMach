/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 05-Dec-88  Avadis Tevanian (avie) at NeXT
 *	Cleaned up cruft.
 *
 *  2-Apr-88  John Seamons (jks) at NeXT
 *	Fixed to work right -- was completely wrong before.
 *
 * 15-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <next/pmap.h>

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the kernel map.
 */
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
{
	pmap_t pmap;
	register pte_t *fpte, *tpte;

	if (size % PAGE_SIZE)
		panic("pagemove");
	pmap = vm_map_pmap(kernel_map);
	while (size > 0) {
		while ((int)(fpte = pmap_pte(pmap, from)) < PTE_ENTRY_NULL)
			pmap_expand_kernel(from, fpte);
		while ((int)(tpte = pmap_pte(pmap, to)) < PTE_ENTRY_NULL)
			pmap_expand_kernel(to, tpte);
		*tpte = *fpte;
		fpte->bits = 0;
		pflush_user();
		pflush_user();
		from += NeXT_page_size;
		to += NeXT_page_size;
		size -= NeXT_page_size;
	}
}
