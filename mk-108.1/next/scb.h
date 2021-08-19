/*	@(#)scb.h	1.0	12/06/86	(c) 1986 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 06-Dec-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 *
 **********************************************************************
 */ 

/*
 * NeXT System control block layout
 */

struct scb {
	int	scb_issp;		/* 00 - initial ssp */
	int	(*scb_ipc)();		/* 04 - initial pc */
	int	(*scb_buserr)();	/* 08 - bus error */
	int	(*scb_addrerr)();	/* 0c - address error */
	int	(*scb_illegal)();	/* 10 - illegal instruction */
	int	(*scb_zerodiv)();	/* 14 - zero divide */
	int	(*scb_check)();		/* 18 - CHK, CHK2 instruction */
	int	(*scb_trapv)();		/* 1c - TRAPV, cpTRAPcc, TRAPcc inst */
	int	(*scb_privilege)();	/* 20 - privilege violation */
	int	(*scb_trace)();		/* 24 - trace trap */
	int	(*scb_emu1010)();	/* 28 - 1010 emulator trap */
	int	(*scb_emu1111)();	/* 2c - 1111 emulator trap */
	int	(*scb_stray1)();	/* 30 - reserved */
	int	(*scb_coproc)();	/* 34 - coprocessor protocol error */
	int	(*scb_format)();	/* 38 - stack format error */
	int	(*scb_badintr)();	/* 3c - uninitialized interrupt */
	int	(*scb_stray2[8])();	/* 40-5c - reserved */
	int	(*scb_spurious)();	/* 60 - spurious interrupt */
	int	(*scb_ipl[7])();	/* 64-7c - ipl 1-7 autovectors */
	int	(*scb_trap[16])();	/* 80-bc - trap instruction vectors */
	int	(*scb_stray3[16])();	/* c0-fc - reserved */
	int	(*scb_user[192])();	/* 100-3fc - user interrupt vectors */
};

#ifdef KERNEL
extern	struct scb scb;
#endif
