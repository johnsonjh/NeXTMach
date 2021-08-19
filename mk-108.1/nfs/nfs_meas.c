/*	@(#)nfs_meas.c	1.0	9/15/89		(c) 1989 NeXT	*/

/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 15-Sep-89  Morris Meyer (mmeyer) at NeXT
 *	Created.
 *
 */ 
#import <nfsmeas.h>
#if NNFSMEAS

#import <sys/types.h>
#import <sys/ioctl.h>
#import <sys/time.h>
#import <sys/errno.h>
#import <sys/uio.h>
#import <kern/xpr.h>
#import <kern/assert.h>
#import <next/cframe.h>
#import <next/machparam.h>
#import <next/cframe.h>
#import <nfs/nfs_meas.h>
#import <next/psl.h>
#import <next/eventc.h>
#import <next/vm_types.h>

#define meas_debug(f) { XPR(XPR_MEAS, f); }
#define meas_debug1(f) { if (ipl_debug & 1) { XPR(XPR_MEAS, f); } }

static struct nfsdatastat nfs_server_data [NNFSOPS];
static struct nfsdatastat nfs_client_data [NNFSOPS];

static struct nfstimestat nfs_server_times[NNFS];
static struct nfstimestat nfs_client_times[NNFS];


static inline struct nfstimestat *nfs_find(u_long xid, int nfsop, int which)
{
	register int i;
	register struct nfstimestat *nfstp;

	if (which == NFSMEAS_SERVER)  {
		nfstp = &nfs_server_times[((int)xid ^ nfsop) & (NNFS-1)];
		i = 0;
		while ((nfstp->xid != xid) && (nfstp->nfsop != nfsop) && 
		        nfstp->xid && (i < NNFS))  {
			i++;
			if (++nfstp >= &nfs_server_times[NNFS])
				nfstp = nfs_server_times;
		}
	}
	else if (which == NFSMEAS_CLIENT)  {
		nfstp = &nfs_client_times[((int)xid ^ nfsop) & (NNFS-1)];
		i = 0;
		while ((nfstp->xid != xid) && (nfstp->nfsop != nfsop) && 
		        nfstp->xid && (i < NNFS))  {
			i++;
			if (++nfstp >= &nfs_client_times[NNFS])
				nfstp = nfs_client_times;
		}
	}
	else
		panic ("nfs_find: bad which");
		
	if (i >= NNFS)
		panic("out of nfs_times slots");
	else if (nfstp->xid == 0) {
		nfstp->xid = xid;
		nfstp->nfsop = nfsop;
	}
	return (nfstp);
}

void nfsmeas_begin (int nfsop, u_long xid, int nfswhich)
{
	struct nfstimestat *nfstp;

	nfstp = nfs_find (xid, nfsop, nfswhich);
	nfstp->us_enter = event_get();
	nfstp->nfsop = nfsop;
	nfstp->xid = xid;

	return;
}

void nfsmeas_end (int nfsop, u_long xid, int nfswhich)
{
	register struct nfstimestat *nfstp;
	register u_int delta;
	
	nfstp = nfs_find (xid, nfsop, nfswhich);
	delta = event_delta (nfstp->us_enter);
	nfstp->xid = 0;		/* Remove it from the table */
	ASSERT((int)delta >= 0);
	
	if (nfswhich == NFSMEAS_SERVER)  {
		nfs_server_data[nfsop].calls++;
		nfs_server_data[nfsop].us_total += delta;
		if (delta > nfs_server_data[nfsop].us_max)
			nfs_server_data[nfsop].us_max = delta;
	}
	else if (nfswhich == NFSMEAS_CLIENT)  {
		nfs_client_data[nfsop].calls++;
		nfs_client_data[nfsop].us_total += delta;
		if (delta > nfs_client_data[nfsop].us_max)
			nfs_client_data[nfsop].us_max = delta;
	}
	else
		panic ("nfsmeas_end: bad nfswhich");			
}

void nfsmeas_remove (int nfsop, u_long xid, int nfswhich)
{
	register struct nfstimestat *nfstp = nfs_find (xid, nfsop, nfswhich);
	nfstp->xid = 0;
}

int nfsmeas_open(dev)
	dev_t dev;
{
	if (minor(dev) != 0)
		return(EINVAL);

	return(0);
}

int nfsmeas_read(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	static int called;
	register int error = 0;
	register int s, i;
	struct nfsdatastat ncd[32], nsd[32];
	struct nfstimestat nts, *nfstp;

	if (called == 0) 
		printf ("nfsmeas_read called\n");
	else
		called++;
	s = splhigh();
	bcopy(nfs_client_data, ncd, sizeof(ncd));
	bcopy(nfs_server_data, nsd, sizeof(nsd));
	splx(s);
	error = uiomove((caddr_t)ncd, sizeof(ncd), UIO_READ, uio);
	error = uiomove((caddr_t)nsd, sizeof(nsd), UIO_READ, uio);
	for (nfstp = nfs_client_times;
	    nfstp < &nfs_client_times[NNFS] && uio->uio_resid && !error;
	    nfstp++) {
		if (nfstp->xid) {
			s = splhigh();
			bcopy(nfstp, &nts, sizeof(nts));
			splx(s);
			error =  uiomove ((caddr_t)&nts, sizeof(nts), 
					   UIO_READ, uio);
		}
	}
	for (nfstp = nfs_server_times;
	    nfstp < &nfs_server_times[NNFS] && uio->uio_resid && !error;
	    nfstp++) {
		if (nfstp->xid) {
			s = splhigh();
			bcopy(nfstp, &nts, sizeof(nts));
			splx(s);
			error =  uiomove ((caddr_t)&nts, sizeof(nts), 
					   UIO_READ, uio);
		}
	}
	return (error);
}

int nfsmeas_ioctl (dev, cmd, data)
	dev_t dev;
	int cmd;
	caddr_t data;
{
	register int i = 0;
	register int s, error;
	struct nfstimestat *nfstp;

	switch(cmd) {
	case NFSIOCCLR:
		s = splhigh();
		for (nfstp = nfs_server_times; 
		     nfstp < &nfs_server_times[NNFS]; nfstp++) {
			nfstp->us_enter = 0;
			nfstp->xid = 0;
			nfstp->nfsop = 0;
		}
		for (nfstp = nfs_client_times; 
		     nfstp < &nfs_client_times[NNFS]; nfstp++) {
			nfstp->us_enter = 0;
			nfstp->xid = 0;
			nfstp->nfsop = 0;
		}
		for (i = 0; i <= 31; i++) {
			nfs_server_data[i].calls = 0;
			nfs_server_data[i].us_max = 0;
			nfs_server_data[i].us_total = 0;
		}
		for (i = 0; i <= 31; i++) {
			nfs_client_data[i].calls = 0;
			nfs_client_data[i].us_max = 0;
			nfs_client_data[i].us_total = 0;
		}
		splx(s);
		break;
	case NFSIOCGCD:
		s = splhigh();
		bcopy ((caddr_t)&nfs_client_data, data, sizeof (struct dbuf));
		splx(s);
		break;
	case NFSIOCGSD:
		s = splhigh();
		bcopy ((caddr_t)&nfs_server_data, data, sizeof (struct dbuf));
		splx(s);
		break;
	case NFSIOCGCT:
		s = splhigh();
		bcopy ((caddr_t)&nfs_client_times, data, sizeof (struct tbuf));
		splx(s);
		break;
	case NFSIOCGST:
		s = splhigh();
		bcopy ((caddr_t)&nfs_server_times, data, sizeof (struct tbuf));
		splx(s);
		break;
	default:
		return(EINVAL);
	}

	return(0);
}

#endif	NNFSMEAS
