/*	@(#)mmu.h	1.0	11/09/86	(c) 1986 NeXT	*/

/* 
 * HISTORY
 * 09-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#ifndef _MMU_
#define	_MMU_

#if	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <gdb.h>
#else	defined(KERNEL) && !defined(KERNEL_FEATURES)
#import <sys/features.h>
#endif	defined(KERNEL) && !defined(KERNEL_FEATURES)

#ifndef	ASSEMBLER
/* 68030 translation control register (tc) */
struct mmu_030_tc {
	u_int	tc_enable : 1,
		: 5,
		tc_sre : 1,
		tc_fcl : 1,
		tc_ps : 4,
		tc_is : 4,
		tc_tia : 4,
		tc_tib : 4,
		tc_tic : 4,
		tc_tid : 4;
};

/* 68040 translation control register (tc) */
struct mmu_040_tc {
	u_int	:16,
		tc_enable : 1,
		tc_8k_pagesize : 1,
		: 14;
};

/*
 *  Common 68030/68040 root pointer register format (srp & crp/urp).
 *  Only the rp_ptbr portion is loaded by the 68040 in the locore.s routines.
 */
struct mmu_rp {
	u_int	rp_lower : 1,
		rp_limit : 15,
		: 14,
		rp_desc_type : 2,
		rp_ptbr : 28,
		: 4;
};

#define	phys_to_l1ptr(p)	((int)(p) >> 4)
#define	l1ptr_to_phys(p)	((int)(p) << 4)

/* 68030 transparent translation registers (tt0 & tt1) */
struct mmu_030_tt {
	u_int	tt_lbase : 8,
		tt_lmask : 8,
		tt_e : 1,
		: 4,
		tt_ci : 1,
		tt_rw : 1,
		tt_rwmask : 1,
		: 1,
		tt_fcbase : 3,
		: 1,
		tt_fcmask : 3;
};

/* 68040 transparent translation registers (itt0, dtt0, itt1, dtt1) */
struct mmu_040_tt {
	u_int	tt_lbase : 8,
		tt_lmask : 8,
		tt_e : 1,
		tt_super : 2,
		: 3,
		tt_u1 : 1,
		tt_u0 : 1,
		: 1,
		tt_cm : 2,
		: 2,
		tt_wp : 1,
		: 2;
};
#endif	ASSEMBLER

#define	TT_LABASE	0xff000000
#define	TT_LAMASK	0x00ff0000
#define	TT_ENABLE	0x00008000
#define	TT_CI		0x00000400
#define	TT_RW		0x00000200
#define	TT_RWMASK	0x00000100
#define	TT_FCBASE	0x00000070
#define	TT_FCMASK	0x00000007

#define	TT_040_USER		0
#define	TT_040_SUPER		1
#define	TT_040_SUPER_USER	2
#define	TT_040_CM_WRITETHRU	0
#define	TT_040_CM_COPYBACK	1
#define	TT_040_CM_INH_SERIAL	2
#define	TT_040_CM_INH_NONSERIAL	3

/* 68030 cache control register (cacr) */
#define	CACR_WA		0x2000		/* cache write allocate policy */
#define	CACR_DBE	0x1000		/* d-cache burst enable */
#define	CACR_CD		0x0800		/* clear data cache */
#define	CACR_CED	0x0400		/* clear data entry */
#define	CACR_FD		0x0200		/* freeze data cache */
#define	CACR_ED		0x0100		/* enable data cache */
#define	CACR_IBE	0x0010		/* i-cache burst enable */
#define	CACR_CI		0x0008		/* clear instruction cache */
#define	CACR_CEI	0x0004		/* clear instruction entry */
#define	CACR_FI		0x0002		/* freeze instruction cache */
#define	CACR_EI		0x0001		/* enable instruction cache */

#define	CACR_CLR_ENA	(CACR_CD+CACR_CI+CACR_ED+CACR_EI+CACR_DBE+CACR_IBE)
#if	GDB
#define	CACR_CLR_ENA_I	(CACR_CI)	/* no i-cache because of breakpts */
#else	GDB
#define	CACR_CLR_ENA_I	(CACR_CI+CACR_EI+CACR_IBE)
#endif	GDB

/* 68040 cache control register (cacr) */
#define	CACR_040_DE	0x80000000	/* enable data cache */
#define	CACR_040_IE	0x00008000	/* enable instruction cache */

/* 68030 address space function codes */
#define	FC_USERD	1
#define	FC_USERI	2
#define	FC_USERRESV	3
#define	FC_SUPERD	5
#define	FC_SUPERI	6
#define	FC_CPU		7

/* 68040 transfer modes (aka function code) */
#define	TM_PUSH		0
#define	TM_USERD	1
#define	TM_USERI	2
#define	TM_MMUD		3
#define	TM_MMUI		4
#define	TM_SUPERD	5
#define	TM_SUPERI	6

/* 68040 transfer types */
#define	TT_NORM		0
#define	TT_MOVE16	1
#define	TT_ALT		2
#define	TT_ACK		3

/* 68040 transfer size */
#define	SIZE_LONG	0
#define	SIZE_BYTE	1
#define	SIZE_WORD	2
#define	SIZE_LINE	3
#endif _MMU_


