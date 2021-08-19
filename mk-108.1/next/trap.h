/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 09-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#ifndef _TRAP_
#define	_TRAP_

/* how to handle failed bus cycles */
#define	TRAP_NORERUN	0	/* neutralize bus error info on stack */
#define	TRAP_RERUN	1	/* page fault worked -- try again */

/* 68030 trap vectors */
#define	T_STRAY		0x00
#define	T_BADTRAP	0x04
#define	T_BUSERR	0x08
#define	T_ADDRERR	0x0c
#define	T_ILLEGAL	0x10
#define	T_ZERODIV	0x14
#define	T_CHECK		0x18
#define	T_TRAPV		0x1c
#define	T_PRIVILEGE	0x20
#define	T_TRACE		0x24
#define	T_EMU1010	0x28
#define	T_EMU1111	0x2c
#define	T_COPROC	0x34		/* 030 only */
#define	T_FORMAT	0x38
#define	T_BADINTR	0x3c
#define	T_SPURIOUS	0x60
#define	T_LEVEL1	0x64
#define	T_LEVEL2	0x68
#define	T_LEVEL3	0x6c
#define	T_LEVEL4	0x70
#define	T_LEVEL5	0x74
#define	T_LEVEL6	0x78
#define	T_LEVEL7	0x7c
#define	T_SYSCALL	0x80
#define	T_MON_BOOT	0xb4
#define	T_KERN_GDB	0xb8
#define	T_USER_BPT	0xbc
#define	T_FPP		0xc0
#define	T_FP_UDT	0xdc		/* 040 only */
#define	T_MMU_CONFIG	0xe0		/* 030 only */
#define	T_MMU_ILL	0xe4		/* 851 only */
#define	T_MMU_ACCESS	0xe8		/* 851 only */

/* MMU psuedo vectors (determined from MMU status reg during T_BUSERR) */
#define	T_MMU_BUSERR	0x400		/* bus error during table walk */
#define	T_MMU_LIMIT	0x404		/* index exceeded limit during walk */
#define	T_MMU_SUPER	0x408		/* supervisor violation during walk */
#define	T_MMU_WRITE	0x40c		/* write protected during walk */
#define	T_MMU_INVALID	0x410		/* invalid descriptor during walk */

/* software generated vectors */
#define	T_USEREXIT	0x600

/* monitor reset event vectors (must have highest vector numbers) */
#define	T_RESET		0x700
#define	T_BOOT		0x704

#endif _TRAP_
