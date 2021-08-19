/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 *  15-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Created.
 */
#ifndef	_NFS_MEAS_
#define _NFS_MEAS_

struct nfstimestat {
	u_int us_enter;			/* eventc value of last entry    */
	u_int xid;			/* transaction id; 		 */
	u_int nfsop;			/* nfs operation for this stat	 */
};

struct nfsdatastat {
	u_int calls;			/* count of the valid rfscalls() */
	u_int us_max;			/* max duration in rfscall()     */
	u_int us_total;			/* total duration in rfscall()   */
};	

#define NNFSOPS 32 			/* ?? */
#define	NNFS	64

struct dbuf {
	struct nfsdatastat nd [NNFSOPS];
};
struct tbuf {
	struct nfstimestat nt [NNFS];
};
#define NFSMEAS_SERVER		0
#define NFSMEAS_CLIENT		1

/* Ioctls */
#define NFSIOCCTRL	_IOW('N', 0, int) /* turn off/on nfs measurement */
#define NFSIOCGCD	_IOR('N', 1, struct dbuf) /* get nfs data */
#define NFSIOCGSD	_IOR('N', 2, struct dbuf) /* get nfs data */
#define NFSIOCGCT	_IOR('N', 3, struct tbuf) /* get nfs data */
#define NFSIOCGST	_IOR('N', 4, struct tbuf) /* get nfs data */
#define NFSIOCCLR	_IO('N', 2)	/* clear nfs statistics */
#endif _NFS_MEAS_
