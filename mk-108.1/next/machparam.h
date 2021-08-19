/* 
 * Copyright (c) 1987 NeXT, Inc.
 *
 * HISTORY
 * 21-Nov-88  Avadis Tevanian (avie) at NeXT
 *	Moved assertion definitions to machine independent <kern/assert.h>.
 *
 * 09-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#import <kern/assert.h>		/* SHOULD GO AWAY! */
#import <next/vm_param.h>
#import <next/eventc.h>

/*
 * Machine dependent constants for NeXT.
 */
#if	KERNEL
#define	NBPG	NeXT_page_size	/* bytes/page */
#else	KERNEL
#define	NBPG	NeXT_MAX_PAGE_SIZE	/* must be constant for utilities */
#endif	KERNEL
#define	PGOFSET	NeXT_page_mask	/* byte offset into page */
#define	PGSHIFT	NeXT_page_shift	/* LOG2(NBPG) */

#define	CLSIZE			1
#define	CLSIZELOG2		0
#define	NeXT_MIN_CLBYTES	(CLSIZE * NeXT_MIN_PAGE_SIZE)

/*
 * Some macros for units conversion
 */
/* Core clicks (NeXT_page_size bytes) to segments and vice versa */
#define	ctos(x)	(x)
#define	stoc(x)	(x)

/* clicks to bytes */
#define	ctob(x)	((x) << NeXT_page_shift)

/* bytes to clicks */
#define	btoc(x)	((((unsigned)(x)+NeXT_page_mask) >> NeXT_page_shift))

/*
 * Macros to decode processor status word.
 */
#define	USERMODE(ps)	(((ps) & SR_SUPER) == 0)
#define	BASEPRI(ps)	(((ps) & SR_IPL) == 0)

#if	defined(KERNEL) || defined(STANDALONE)
#define	DELAY(n) delay(n)

#else	defined(KERNEL) || defined(STANDALONE)
#define	DELAY(n)	{ register int N = (n); while (--N > 0); }
#endif	defined(KERNEL) || defined(STANDALONE)
