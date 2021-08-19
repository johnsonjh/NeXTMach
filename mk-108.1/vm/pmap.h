/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vm/pmap.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1985, Avadis Tevanian, Jr.
 *
 *	Machine address mapping definitions -- machine-independent
 *	section.  [For machine-dependent section, see "machine/pmap.h".]
 *
 * HISTORY
 * 15-May-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Coalesced includes.
 *
 * 30-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Export kernel_pmap here... all implementations are expected to
 *	have one of these.
 *
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added export of pmap_clear_reference and pmap_is_referenced.
 *
 * 24-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Changed to directly import declaration of boolean.
 *
 *  6-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */

#ifndef	_PMAP_VM_
#define	_PMAP_VM_

#import <machine/pmap.h>
#import <machine/vm_types.h>
#import <sys/boolean.h>

void		pmap_bootstrap();
void		pmap_init();
vm_offset_t	pmap_map();
pmap_t		pmap_create();
pmap_t		pmap_kernel();
void		pmap_destroy();
void		pmap_reference();
void		pmap_remove();
void		pmap_remove_all();
void		pmap_copy_on_write();
void		pmap_protect();
void		pmap_enter();
vm_offset_t	pmap_extract();
void		pmap_update();
void		pmap_collect();
void		pmap_activate();
void		pmap_deactivate();
void		pmap_copy();
void		pmap_statistics();
void		pmap_clear_reference();
boolean_t	pmap_is_referenced();
void		pmap_change_wiring();
void		pmap_clear_modify();
void		pmap_page_protect();

void		pmap_redzone();
boolean_t	pmap_access();

extern pmap_t	kernel_pmap;

#endif	_PMAP_VM_
