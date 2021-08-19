/*
 *	Copyright (C) 1990,  NeXT, Inc.
 *
 *	File:	next/kern_machdep.c
 *	Author:	John Seamons
 *
 *	Machine-specific kernel routines.
 *
 * HISTORY
 *  5-Mar-90  John Seamons (jks) at NeXT
 *	Created.
 */

#import	<sys/types.h>
#import	<sys/machine.h>
#import	<next/cpu.h>

check_cpu_subtype (cpu_subtype)
	cpu_subtype_t cpu_subtype;
{
	struct machine_slot *ms = &machine_slot[cpu_number()];

	if (ms->cpu_subtype == CPU_SUBTYPE_MC68030 &&
	    cpu_subtype == CPU_SUBTYPE_MC68030)
		return (TRUE);
	if (ms->cpu_subtype == CPU_SUBTYPE_MC68040 &&
	    (cpu_subtype == CPU_SUBTYPE_MC68030 ||
	    cpu_subtype == CPU_SUBTYPE_MC68040))
		return (TRUE);
	return (FALSE);
}

