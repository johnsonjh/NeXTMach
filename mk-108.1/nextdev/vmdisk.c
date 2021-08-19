/*
 *	File:	vmdisk.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	A simple virtual memory disk device.
 */

#import <vmdisk.h>

#if	NVMDISK > 0

#import <sys/types.h>
#import <sys/param.h>
#import <sys/buf.h>
#import <sys/uio.h>
#import <sys/errno.h>

#import <vm/vm_param.h>
#import <vm/vm_kern.h>

struct vmdisk_device {
	vm_offset_t	base_addr;	/* base address of map */
	vm_size_t	size;		/* size of map */
};

struct vmdisk_device vmdisk_device[NVMDISK];

#define VMDISK_UNIT(dev)	(minor(dev))

vmdisk_init()
{
	int	i;
	struct vmdisk_device *d;

	/* XXXXXXXXXX */
	d = &vmdisk_device[0];
	for (i = 0; i < NVMDISK; i++) {
		d->base_addr = kmem_alloc_pageable(kernel_map, 4*1024*1024);
		d->size = 4*1024*1024;
		printf("VM disk %d at 0x%x\n", i, d->base_addr);
		d++;
	}
}

vmdisk_open(dev)
	dev_t dev;
{
	int	unit = VMDISK_UNIT(dev);

	if (unit >= NVMDISK)
		return (ENXIO);

	return(0);
}

vmdisk_read(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	int		unit;
	vm_offset_t	v;
	struct vmdisk_device	*d;
	vm_size_t	size;

	unit = VMDISK_UNIT(dev);
	if (unit >= NVMDISK)
		return (ENXIO);

	d = &vmdisk_device[unit];
	v = d->base_addr + uio->uio_offset;
	if ((v < d->base_addr) || (v >= d->base_addr + d->size))
		return(ENXIO);
	if ((v + uio->uio_resid) > (d->base_addr + d->size))
		size = (d->base_addr + d->size) - v;
	else
		size = uio->uio_resid;
	return(uiomove(v, size, UIO_READ, uio));
}

vmdisk_write(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	int		unit;
	vm_offset_t	v;
	struct vmdisk_device	*d;
	vm_size_t	size;

	unit = VMDISK_UNIT(dev);
	if (unit >= NVMDISK)
		return (ENXIO);

	d = &vmdisk_device[unit];
	v = d->base_addr + uio->uio_offset;
	if ((v < d->base_addr) || (v >= d->base_addr + d->size))
		return(ENXIO);
	if ((v + uio->uio_resid) > (d->base_addr + d->size))
		size = (d->base_addr + d->size) - v;
	else
		size = uio->uio_resid;
	return(uiomove(v, size, UIO_WRITE, uio));
}

vmdisk_strategy (bp)
	register struct buf *bp;
{
	int		unit, bno;
	vm_offset_t	vaddr;
	struct vmdisk_device	*d;

	unit = VMDISK_UNIT(bp->b_dev);
	if (unit >= NVMDISK) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone (bp);
		return;
	}

	d = &vmdisk_device[unit];
	bno = bp->b_blkno;

	vaddr = bno * DEV_BSIZE;
	vaddr += d->base_addr;
	if ((bp->b_flags & B_READ) == B_READ)
		bcopy(vaddr, bp->b_un.b_addr, bp->b_bcount);
	else
		bcopy(bp->b_un.b_addr, vaddr, bp->b_bcount);
	bp->b_resid = 0;
	biodone(bp);
	return;
}

vmdisk_ioctl(dev, cmd, data, flag)
	dev_t dev;
	caddr_t data;
{
	return (ENOTTY);
}

#endif	NVMDISK > 0
