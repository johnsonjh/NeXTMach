/* 
 * Copyright (c) 1988 NeXT, Inc.
 *
 * HISTORY
 * 22-May-89  Mike DeMoney (mike) at NeXT
 *	Modified to correspond with new ipl_meas.c
 *  1-Feb-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 */
#ifndef	_IPL_MEAS_
#define _IPL_MEAS_
struct ipl_data {
	char *		pc_enter;	/* pc of last ipl entry */
	u_int		us_enter;	/* eventc value of last entry */

	char *		pc_enter_max;	/* pc of entry to longest ipl */
	char *		pc_exit_max;	/* pc of exit from longest ipl */
	u_int		us_max;		/* longest (delta) usecs in ipl */

	u_int		us_total;	/* total usecs in ipl */
	u_int		n_exits;	/* number of times we've exited ipl */
};

struct intr_times {
	int (*func)();			/* interrupt routine address */
	int arg;			/* interrupt routine argument */
	u_int ipl;			/* interrupt level */

	u_int us_enter;			/* eventc value of last entry */

	u_int us_max;			/* max duration in handler */

	u_int us_total;			/* total duration in handler */
	u_int n_exits;			/* number of calls */
};

struct intr_ioctl {
	int entries;
	int used;
	struct intr_times *itp;
};

/* Ioctls */
#define IPLIOCCTRL	_IOW('I', 0, int)/* turn off/on ipl measurement */
#define IPLIOCCLR	_IO('I', 1)	/* clear ipl statistics */
#define IPLIOCGET0	_IOR('I', 2, struct ipl_data)
#define IPLIOCGET1	_IOR('I', 3, struct ipl_data)
#define IPLIOCGET2	_IOR('I', 4, struct ipl_data)
#define IPLIOCGET3	_IOR('I', 5, struct ipl_data)
#define IPLIOCGET4	_IOR('I', 6, struct ipl_data)
#define IPLIOCGET5	_IOR('I', 8, struct ipl_data)
#define IPLIOCGET6	_IOR('I', 9, struct ipl_data)
#define IPLIOCGET7	_IOR('I', 10, struct ipl_data)
#define IPLIOCGETINT	_IOWR('I', 11, struct intr_ioctl)
#endif _IPL_MEAS_
