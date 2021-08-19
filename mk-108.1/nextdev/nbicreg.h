/*
 *	File:	nextdev/nbicreg.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1989 NeXT, Inc.
 */

#ifndef	_NBICREG_
#define _NBICREG_

/*
 *	Define addresses of NBIC registers.
 */

#define NBIC_CR		0x02020000		/* Control Register */
#define NBIC_IDR	0x02020004		/* ID register */

/*
 *	Control register definitions
 */

#define NBIC_CR_IGNSID0	0x10000000		/* ignore slot ID 0 */
#define NBIC_CR_STFWD	0x08000000		/* store forward */
#define NBIC_CR_RMCOL	0x04000000		/* read-modify  collision */

/*
 *	ID register definitions
 */

#define NBIC_IDR_VALID	0x80000000		/* valid */
#define NBIC_IDR_IDMASK	0x7fff0000		/* Manufacturer ID mask */

#endif	_NBICREG_

