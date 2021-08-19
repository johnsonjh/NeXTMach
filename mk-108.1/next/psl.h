/* 
 * HISTORY
 * 21-Jul-88  Avadis Tevanian, Jr. (avie) at NeXT
 *	Allow SR_TSINGLE and SR_TFLOW status register bits to be
 *	set by the user (remove from SR_USERCLR).
 *
 * 09-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#ifndef _PSL_
#define	_PSL_

/*
 *  68030 status register definitions
 */

#define	SR_TSINGLE	0x8000		/* trace single instruction */
#define	SR_TFLOW	0x4000		/* trace on flow change */
#define	SR_SUPER	0x2000		/* super/user state */
#define	SR_MASTER	0x1000		/* master/interrupt state */
#define	SR_IPL		0x0700		/* interrupt priority level */
#define	SR_X		0x0010		/* extend */
#define	SR_N		0x0008		/* negative */
#define	SR_Z		0x0004		/* zero */
#define	SR_V		0x0002		/* overflow */
#define	SR_C		0x0001		/* carry */
#define	SR_CC		0x001f		/* all of them */

/* shorthand */
#define	SR_HIGH		0x2700		/* super mode, highest ipl */
#define	SR_LOW		0x2000		/* super mode, lowest ipl */
#define	SR_USER		0x0000		/* user mode, lowest ipl */
#define	SR_USERCLR	0x3f00		/* throw away user bits */
#define srtoipl(x)	(((x)&SR_IPL)>>8)	/* integral ipl level */

/* used by machine independent code (shifts are used because
   machine independent code accesses as ints and we have a
   short pad before SR */
#define	PSL_USERSET	(SR_USER << 16)
#define	PSL_USERCLR	(SR_USERCLR << 16)
#define	PSL_ALLCC	(SR_CC << 16)
#define	PSL_T		(SR_TSINGLE << 16)

#endif _PSL_
