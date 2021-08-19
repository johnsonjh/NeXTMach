/*
 * RAM disk driver.
 * This driver simulates a (small) local disk.  The assumption is that
 * all partitions are 4M and contiguous.  The start address is passed as
 * the controller register address during autoconf.  The 4M is assumed to
 * be in the transparent translation space.
 */
#import <rd.h>
#if NRD > 0

#import <sys/param.h>
#import <sys/systm.h>
#import <machine/cpu.h>
#import <sys/buf.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/ioctl.h>
#import <sys/dk.h>
#import <sys/vmmac.h>
#import <sys/kernel.h>
#import <next/pmap.h>

#import <nextdev/busvar.h>

#define DKUNIT(bp) (minor((bp)->b_dev) >> 3)
#define DRIVE(dev) (minor(dev)>>3)
#define DKBLOCK(bp) ((bp)->b_blkno)

#define RDSIZE	(4*1024*1024)

struct softrd {
	caddr_t	rd_start, rd_stop;
	int	rd_online;
} softrd[NRD];

struct buf rd_rbuf[NRD];

int	rdprobe(), rdslave(), rdattach();

struct	bus_ctrl *rdcinfo[NRD];
struct	bus_device *rddinfo[NRD];
struct	bus_driver rddriver = {
	rdprobe, rdslave, rdattach, 0, 0, 0, 0,
	RDSIZE, "rd", rddinfo, "rdc", rdcinfo
};

rdprobe (reg, ctrl)
	register caddr_t reg;
{
	softrd[ctrl].rd_start = reg;
	softrd[ctrl].rd_stop = reg + rddinfo[ctrl]->bd_flags;
	if (probe_rb (reg) == 0)
		return(0);
	*reg = 0xff;
	if (*reg != -1)
		return(0);
	*reg = 0;
	if (*reg != 0)
		return(0);
	softrd[ctrl].rd_online = 1;
	return(1);
}

rdslave (bd, reg)
	register struct bus_device *bd;
	register caddr_t reg;
{
	return (1);
}

rdattach (bd)
	register struct bus_device *bd;
{
	printf ("\tRAM disk from 0x%x to 0x%x\n",
		softrd[bd->bd_unit].rd_start, softrd[bd->bd_unit].rd_stop);
	return(0);
}

rdopen(dev)
	dev_t dev;
{
	struct softrd *rd = &softrd[DRIVE(dev)];

	if (!rd->rd_online)
		return(ENXIO);
	return(0);
}

rdstrategy(bp)
register struct buf *bp;
{
	register int drive = DRIVE(bp->b_dev);
	register struct softrd *rd = &softrd[drive];
	register bno;
	register caddr_t p;

	bno = bp->b_blkno;
	if (drive > NRD || bno < 0 || bno > (RDSIZE/DEV_BSIZE))
		goto bad;
	/* Check bounds ??? */
	p = rd->rd_start + (bno * DEV_BSIZE);
	if (bp->b_flags & B_PHYS) {
		/* Copy to/from user space */
		if ((bp->b_flags & B_READ) == 0)
			u.u_error = copyin(bp->b_un.b_addr, p, bp->b_bcount);
		else 
			u.u_error = copyout(p, bp->b_un.b_addr, bp->b_bcount);
	} else {
		if ((bp->b_flags & B_READ) == 0)
			bcopy(bp->b_un.b_addr, p, bp->b_bcount);
		else
			bcopy(p, bp->b_un.b_addr, bp->b_bcount);
	}
	if (u.u_error)
		bp->b_flags |= B_ERROR;
	goto done;
bad:
	bp->b_flags |= B_ERROR;
done:
	iodone(bp);
}

rdread(dev, uio)
dev_t dev;
struct uio *uio;
{
	struct softrd *rd = &softrd[DRIVE(dev)];

	if (DRIVE(dev) > NRD)
		return(ENXIO);
	return(physio (rdstrategy, &rd_rbuf[DRIVE(dev)], dev, B_READ,
		minphys, uio, DEV_BSIZE));
}

rdwrite(dev, uio)
dev_t dev;
struct uio *uio;
{
	struct softrd *rd = &softrd[DRIVE(dev)];

	if (DRIVE(dev) > NRD)
		return(ENXIO);
	return(physio (rdstrategy, &rd_rbuf[DRIVE(dev)], dev, B_WRITE,
		minphys, uio, DEV_BSIZE));
}

#endif NRD

