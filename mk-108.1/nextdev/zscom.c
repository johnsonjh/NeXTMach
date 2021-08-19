/*	@(#)zscom.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 * HISTORY
 */ 
#import <zs.h>

#import <next/autoconf.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <nextdev/zsreg.h>
#import <nextdev/zscom.h>
#import <sys/errno.h>
#import <next/spl.h>
#import <kern/xpr.h>
#import	<mon/global.h>

#ifdef notdef
#define	XDBG(x)	XPR(XPR_SCC, x)
#else
#define	XDBG(x)
#endif

int zsnullintr();

struct zs_intrsw zi_null = { zsnullintr, zsnullintr, zsnullintr };
struct zs_com zs_com[2] = {
	{ ZCUSER_NONE, &zi_null },
	{ ZCUSER_NONE, &zi_null }
};

int
zsacquire(int unit, int user, struct zs_intrsw *intrsw)
{
	struct zs_com *zcp = &zs_com[unit];

	if (zcp->zc_intrsw != &zi_null && zcp->zc_intrsw != intrsw)
		return(EBUSY);
	zcp->zc_user = user;
	zcp->zc_intrsw = intrsw;
	return(0);
}

void
zsrelease(int unit)
{
	struct zs_com *zcp = &zs_com[unit];

	zcp->zc_user = ZCUSER_NONE;
	zcp->zc_intrsw = &zi_null;
}

/*
 * zsint -- the  8530 generated an interrupt
 * Figure out why and vector to the current handler
 * MUST BE CALLED AT splscc()
 */
/*ARGSUSED*/
zsint()
{
	register int ipend, serviced = 0;

	XDBG(("zsint enter\n"));
	ASSERT(curipl() == IPLSCC);
	for (;;) {
		ZSREAD_A(ipend, 3);

		XDBG(("zsint ipend=0x%x\n", ipend));
		if (ipend == 0)		/* no interrupts pending */
			break;

		/*
		 * must process status change before recv intrs
		 * due to tricky handling of break's
		 */
		if (ipend & RR3_A_STATIP) {
			(*zs_com[0].zc_intrsw->zi_sint)(0);
			serviced = 1;
		}
		if (ipend & RR3_B_STATIP) {
			(*zs_com[1].zc_intrsw->zi_sint)(1);
			serviced = 1;
		}
		if (ipend & RR3_A_RXIP) {
			(*zs_com[0].zc_intrsw->zi_rint)(0);
			serviced = 1;
		}
		if (ipend & RR3_B_RXIP) {
			(*zs_com[1].zc_intrsw->zi_rint)(1);
			serviced = 1;
		}
		if (ipend & RR3_A_TXIP) {
			(*zs_com[0].zc_intrsw->zi_xint)(0);
			serviced = 1;
		}
		if (ipend & RR3_B_TXIP) {
			(*zs_com[1].zc_intrsw->zi_xint)(1);
			serviced = 1;
		}
	}
#ifdef notdef
	/*
	 * FIXME: is this necessary
	 * IF it is, we better initialize zsaddr!!!
	 */
	ZSWRITE(zsaddr, 0, WR0_RESETIUS);
	DELAY(1);
	zsaddr->zs_ctrl = WR0_RESETIUS;
#endif notdef
	XDBG(("zsint exit\n"));
	return (serviced);
}

zsnullintr(chan)
{
	register volatile struct zsdevice *zsaddr;

	zsaddr = (chan == 0) ? ZSADDR_A : ZSADDR_B;
	ZSWRITE(zsaddr, 1, 0);
}

/* called at autoconf time */
zsintsetup()
{
	extern int console_i, console_o;
	int wr5;
	
	install_polled_intr (I_SCC, zsint);

	/*
	 * Enable the uart outputs so they don't
	 * float about tri-stated (but only if ROM
	 * hasn't already set the UART up).
	 */
	if (console_i != CONS_I_SCC_A && console_i != CONS_I_SCC_B &&
	    console_o != CONS_O_SCC_A && console_o != CONS_O_SCC_B) {
		switch (machine_type) {
		case NeXT_CUBE:
			switch (board_rev) {
			case 0:
			case 1:
			case 2:
				wr5 = WR5_RTS;
				break;
			case 3:
			default:
				wr5 = 0;
				break;
			}
			break;
		default:
			wr5 = 0;
			break;
		}
		ZSWRITE(ZSADDR_A, 5, wr5);
		ZSWRITE(ZSADDR_B, 5, wr5);
	}
}

