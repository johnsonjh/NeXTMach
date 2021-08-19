/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log:	machine.h,v $
 *  1-Mar-90  John Seamons (jks) at NeXT
 *	Redefined cpu_type and cpu_subtype definitions to indicate processor
 *	architecture instead of product types for the MC680x0.
 *
 * Revision 2.15  89/10/11  14:39:56  dlb
 * 	Removed should_exit - replaced by action thread.
 * 	[89/01/25            dlb]
 * 
 * Revision 2.14  89/07/14  17:21:39  rvb
 * 	Added CPU types and subtypes for MC68030, MC68040, MC88000,
 * 	HPPA, ARM and Sun4-SPARC.
 * 	[89/07/13            mrt]
 * 
 * Revision 2.12  89/05/30  10:38:58  rvb
 * 	Add R2000 machine types.
 * 	[89/05/30  08:28:53  rvb]
 * 
 * Revision 2.11  89/04/18  16:43:32  mwyoung
 * 	Use <machine/vm_types.h> rather than <vm/vm_param.h> to get
 * 	VM types.  Remove old history... none of it was insightful.
 * 
 * 	The variable declarations should be moved elsewhere.
 * 	[89/01/24            mwyoung]
 * 
 */
/*
 *	Machine independent machine abstraction.
 *	Copyright (C) 1986, Avadis Tevanian, Jr.
 */

#ifndef	_SYS_MACHINE_H_
#define _SYS_MACHINE_H_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <cpus.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#import <machine/vm_types.h>
#import <sys/boolean.h>

/*
 *	For each host, there is a maximum possible number of
 *	cpus that may be available in the system.  This is the
 *	compile-time constant NCPUS, which is defined in cpus.h.
 *
 *	In addition, there is a machine_slot specifier for each
 *	possible cpu in the system.
 */

struct machine_info {
	int		major_version;	/* kernel major version id */
	int		minor_version;	/* kernel minor version id */
	int		max_cpus;	/* max number of cpus compiled */
	int		avail_cpus;	/* number actually available */
	vm_size_t	memory_size;	/* size of memory in bytes */
};

typedef struct machine_info	*machine_info_t;
typedef struct machine_info	machine_info_data_t;	/* bogus */

typedef int	cpu_type_t;
typedef int	cpu_subtype_t;

#define CPU_STATE_MAX		3

#define CPU_STATE_USER		0
#define CPU_STATE_SYSTEM	1
#define CPU_STATE_IDLE		2

struct machine_slot {
	boolean_t	is_cpu;		/* is there a cpu in this slot? */
	cpu_type_t	cpu_type;	/* type of cpu */
	cpu_subtype_t	cpu_subtype;	/* subtype of cpu */
	boolean_t	running;	/* is cpu running */
	long		cpu_ticks[CPU_STATE_MAX];
	int		clock_freq;	/* clock interrupt frequency */
};

typedef struct machine_slot	*machine_slot_t;
typedef struct machine_slot	machine_slot_data_t;	/* bogus */

#ifdef	KERNEL
extern struct machine_info	machine_info;
extern struct machine_slot	machine_slot[NCPUS];

extern vm_offset_t		interrupt_stack[NCPUS];
#endif	KERNEL

/*
 *	Machine types known by all.
 */

#define CPU_TYPE_VAX		((cpu_type_t) 1)
#define CPU_TYPE_ROMP		((cpu_type_t) 2)
#define CPU_TYPE_NS32032	((cpu_type_t) 4)
#define CPU_TYPE_NS32332        ((cpu_type_t) 5)
#define	CPU_TYPE_MC680x0	((cpu_type_t) 6)
#define CPU_TYPE_I386		((cpu_type_t) 7)
#define CPU_TYPE_MIPS		((cpu_type_t) 8)
#define CPU_TYPE_NS32532        ((cpu_type_t) 9)
#define CPU_TYPE_HPPA           ((cpu_type_t) 11)
#define CPU_TYPE_ARM		((cpu_type_t) 12)
#define CPU_TYPE_MC88000	((cpu_type_t) 13)
#define CPU_TYPE_SPARC		((cpu_type_t) 14)
		

/*
 *	Machine subtypes (these are defined here, instead of in a machine
 *	dependent directory, so that any program can get all definitions
 *	regardless of where is it compiled).
 */

/*
 *	VAX subtypes (these do *not* necessary conform to the actual cpu
 *	ID assigned by DEC available via the SID register).
 */
 
#define CPU_SUBTYPE_VAX780	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_VAX785	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_VAX750	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_VAX730	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_UVAXI	((cpu_subtype_t) 5)
#define CPU_SUBTYPE_UVAXII	((cpu_subtype_t) 6)
#define CPU_SUBTYPE_VAX8200	((cpu_subtype_t) 7)
#define CPU_SUBTYPE_VAX8500	((cpu_subtype_t) 8)
#define CPU_SUBTYPE_VAX8600	((cpu_subtype_t) 9)
#define CPU_SUBTYPE_VAX8650	((cpu_subtype_t) 10)
#define CPU_SUBTYPE_VAX8800	((cpu_subtype_t) 11)
#define CPU_SUBTYPE_UVAXIII	((cpu_subtype_t) 12)

/*
 *	ROMP subtypes.
 */

#define CPU_SUBTYPE_RT_PC	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_RT_APC	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_RT_135	((cpu_subtype_t) 3)

/*
 *	32032/32332/32532 subtypes.
 */

#define CPU_SUBTYPE_MMAX_DPC	    ((cpu_subtype_t) 1)	/* 032 CPU */
#define CPU_SUBTYPE_SQT		    ((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MMAX_APC_FPU    ((cpu_subtype_t) 3)	/* 32081 FPU */
#define CPU_SUBTYPE_MMAX_APC_FPA    ((cpu_subtype_t) 4)	/* Weitek FPA */
#define CPU_SUBTYPE_MMAX_XPC	    ((cpu_subtype_t) 5)	/* 532 CPU */

/*
 *	80386 subtypes.
 */

#define CPU_SUBTYPE_AT386	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_EXL		((cpu_subtype_t) 2)

/*
 *	Mips subtypes.
 */

#define CPU_SUBTYPE_MIPS_R2300	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MIPS_R2600	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MIPS_R2800	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_MIPS_R2000a	((cpu_subtype_t) 4)

/*
 * 	680x0 subtypes
 */

#define CPU_SUBTYPE_MC68030		((cpu_subtype_t) 1) 
#define CPU_SUBTYPE_MC68040		((cpu_subtype_t) 2) 

/*
 *	HPPA subtypes  Hewlett-Packard HP-PA family of
 *	risc processors 800 series workstations.
 *	Port done by Hewlett-Packard
 */

#define CPU_SUBTYPE_HPPA_825	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_HPPA_835	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_HPPA_840	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_HPPA_850	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_HPPA_855	((cpu_subtype_t) 5)

/* 
 * 	Acorn subtypes - Acorn Risc Machine port done by
 *		Olivetti System Software Laboratory
 */

#define CPU_SUBTYPE_ARM_A500_ARCH	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_ARM_A500		((cpu_subtype_t) 2)
#define CPU_SUBTYPE_ARM_A440		((cpu_subtype_t) 3)
#define CPU_SUBTYPE_ARM_M4		((cpu_subtype_t) 4)
#define CPU_SUBTYPE_ARM_A680		((cpu_subtype_t) 5)

/*
 *	MC88000 subtypes - Encore doing port.
 */

#define CPU_SUBTYPE_MMAX_JPC	((cpu_subtype_t) 1)

/*
 *	Sun4 subtypes - port done at CMU
 */

#define CPU_SUBTYPE_SUN4_260		((cpu_subtype_t) 1)
#define CPU_SUBTYPE_SUN4_110		((cpu_subtype_t) 2)


#endif	_SYS_MACHINE_H_



