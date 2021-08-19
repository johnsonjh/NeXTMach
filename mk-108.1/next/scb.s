/*	@(#)scb.s	1.0	11/20/86	(c) 1986 NeXT	*/

/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 29-Mar-90  John Seamons (jks) at NeXT
 *	Added back the cache flush trap for Sun binary compatibility.
 *
 * 20-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 */ 

/*
 * System Control Block (SCB)
 */

#define	STRAY	.long	stray		/* illegal exception */
#define	BADTRAP	.long	badtrap		/* unexpected trap instruction */
#define	KS(a)	.long	a		/* take exception on kernel stack */
#define	FPP	.long	fpp		/* floating-point coproc */

	.data
_scb:	.globl	_scb

/* vector
   offset */
/* 0x000 */	STRAY;		STRAY;		KS(buserr);	KS(addrerr);
/* 0x010 */	KS(illegal);	KS(zerodiv);	KS(check);	KS(trapv);
/* 0x020 */	KS(privilege);	KS(trace);	KS(emu1010);	KS(fline);
/* 0x030 */	STRAY;		KS(coproc);	KS(format);	KS(badintr);
/* 0x040 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x050 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x060 */	KS(spurious);	KS(ipl1);	KS(ipl2);	KS(ipl3);
/* 0x070 */	KS(ipl4);	KS(ipl5);	KS(ipl6);	KS(ipl7);
/* 0x080 */	KS(trap0);	BADTRAP;	KS(trap2);	KS(trap3);
/* 0x090 */	KS(trap4);	KS(trap5);	KS(trap6);	BADTRAP;
/* 0x0a0 */	BADTRAP;	BADTRAP;	BADTRAP;	BADTRAP;
/* 0x0b0 */	BADTRAP;	BADTRAP;	BADTRAP;	KS(breakpt);
/* 0x0c0 */	KS(fpsp_bsun);	KS(real_inex);	KS(real_dz);	KS(fpsp_unfl);
/* 0x0d0 */	KS(fpsp_operr);	KS(fpsp_ovfl);	KS(fpsp_snan); KS(fpsp_unsupp);
/* 0x0e0 */	KS(mmu_config);	KS(mmu_ill);	KS(mmu_access);	STRAY;
/* 0x0f0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x100 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x110 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x120 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x130 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x140 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x150 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x160 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x170 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x180 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x190 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x1a0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x1b0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x1c0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x1d0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x1e0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x1f0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x200 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x210 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x220 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x230 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x240 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x250 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x260 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x270 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x280 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x290 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x2a0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x2b0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x2c0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x2d0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x2e0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x2f0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x300 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x310 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x320 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x330 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x340 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x350 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x360 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x370 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x380 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x390 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x3a0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x3b0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x3c0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x3d0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x3e0 */	STRAY;		STRAY;		STRAY;		STRAY;
/* 0x3f0 */	STRAY;		STRAY;		STRAY;		STRAY;
