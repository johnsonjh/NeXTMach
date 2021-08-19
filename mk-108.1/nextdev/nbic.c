/*
 *	File:	nextdev/nbic.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1989 NeXT, Inc.
 */

#import <sys/types.h>
#import <next/scr.h>
#import <nextdev/nbicreg.h>

int	nbic_present = 0;

nbic_configure()
{
	/* turn off ROM overlay mode so NBIC will be enabled */
	*scr2 |= SCR2_OVERLAY;

	if (probe_rb(NBIC_CR)) {
		nbic_present = 1;
		printf("NBIC present\n");
		*(volatile int *)NBIC_IDR = NBIC_IDR_VALID;
		*(volatile int *)NBIC_CR = NBIC_CR_STFWD;
	}
}

