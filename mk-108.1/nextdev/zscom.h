/*	@(#)zscom.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 **********************************************************************
 */ 

/*
 * zscom.h -- definition of data structures common to all drivers
 * for SCC
 */

struct zs_intrsw {		/* interrupt switch */
	int (*zi_xint)(int chan);	/* transmitter interrupt routine */
	int (*zi_rint)(int chan);	/* receiver interrupt routine */
	int (*zi_sint)(int chan);	/* status change interrupt routine */
};

struct zs_com {			/* common SCC data */
	int zc_user;			/* driver currently using SCC */
	struct zs_intrsw *zc_intrsw;	/* interrupt switch */
};

/*
 * Possible users of SCC
 */
#define	ZCUSER_NONE	0		/* no one currently using SCC */
#define	ZCUSER_TTY	1		/* tty driver */
#define	ZCUSER_MIDI	2		/* midi driver */
#define	ZCUSER_LOADABLE	3		/* loadable SCC driver */

extern struct zs_com zs_com[];
extern struct zs_intrsw zi_null;	/* null interrupt routine */

extern int zsacquire(int unit, int user, struct zs_intrsw *intrsw);
extern void zsrelease(int unit);
