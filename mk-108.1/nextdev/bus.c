/*	@(#)bus.c	1.0	08/12/87	(c) 1987 NeXT	*/

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/buf.h>
#import <sys/vm.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/conf.h>
#import <sys/kernel.h>
#import <nextdev/busvar.h>

#import <machine/spl.h>

/*
 * setup bus, call driver start routine
 */
busgo (bd)
	register struct bus_device *bd;
{
	register struct bus_ctrl *bc = bd->bd_bc;
	struct bus_hd *bh = bc->bc_hd;
	register int s, unit;

	s = spl6();
	if ((bc->bc_driver->br_flags & BUS_XCLU) && (bh->bh_users > 0)
	    || bh->bh_xclu)
		goto rwait;
	/*
	 * driver must set up transfers with physical addresses
	 */
	bh->bh_users++;
	if (bc->bc_driver->br_flags & BUS_XCLU)
		bh->bh_xclu = 1;
	splx(s);
	bc->bc_device = bd;
	(*bc->bc_driver->br_go) (bc);
	return (1);

rwait:
	if (bd != bh->bh_actf) {
		bd->bd_forw = NULL;	/* link request into list */
		if (bh->bh_actf == NULL)
			bh->bh_actf = bd;
		else
			bh->bh_actl->bd_forw = bd;
		bh->bh_actl = bd;
	}
	splx(s);
	return (0);
}

busdone (bc)	/* operation completed, release bus resources */
	register struct bus_ctrl *bc;
{
	struct bus_hd *bh = bc->bc_hd;
	struct bus_device *bd;
	int s;

	s = spl6();
	if (bc->bc_driver->br_flags & BUS_XCLU)
		bh->bh_xclu = 0;
	bh->bh_users--;
	if ((bd = bh->bh_actf) != NULL)
		busgo(bd);
	splx(s);
	if (bc->bc_driver->br_done)
		(*bc->bc_driver->br_done) (bc);
}

