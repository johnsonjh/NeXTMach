/* 
 * Copyright (c) 1987, 1988 NeXT, Inc.
 *
 * HISTORY
 * 05-Apr-90	Ted Cohn (tcohn) at NeXT
 *	Added close to video device
 *
 * 27-Mar-90	Doug Mitchell at NeXT
 *	Added fd and vol devices
 *
 * 19-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Purged slot driver.
 *
 * 24-Feb-89	Doug Mitchell at NeXT
 *	Added st (SCSI tape) driver
 *
 * 02-Feb-89	Doug Mitchell at NeXT
 *	Added sg driver
 *
 * 09Dec88 Leo Hourvitz (leo) at NeXT
 *      Added evs driver
 *
 * 22-Dec-87  Gregg Kellogg (gk) at NeXT
 *	Added DSP.
 *
 * 14-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)conf.c	7.1 (Berkeley) 6/5/86
 */

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/buf.h>
#import <sys/ioctl.h>
#import <sys/tty.h>
#import <sys/conf.h>

int	nulldev();
int	nodev();

#import <ldd.h>
#if NLDD > 0
extern int lddopen(), lddclose(), lddstrategy(), lddread(), lddwrite();
extern int lddioctl(), lddstop(), lddreset(), lddselect(), lddmmap();
extern int ldddump(), lddsize();
#else
#define	lddopen		nodev
#define	lddclose	nodev
#define	lddstrategy	nodev
#define	lddread		nodev
#define	lddwrite	nodev
#define	lddioctl	nodev
#define	lddstop		nodev
#define	lddreset	nodev
#define	lddselect	nodev
#define	lddmmap		nodev
#define	ldddump		nodev
#define	lddsize		nodev
#endif


#import <od.h>
#if NOD > 0
int	odstrategy(),odread(),odwrite(),odreset(),oddump(),odsize(),odopen();
int	odclose(),odioctl();
#else
#define	odstrategy	nodev
#define	odread		nodev
#define	odwrite		nodev
#define	odreset		nulldev
#define	oddump		nodev
#define	odsize		nodev
#define odopen		nodev
#define	odioctl		nodev
#define	odclose		nodev
#endif


#import <rd.h>
#if NRD > 0
int	rdstrategy(),rdread(),rdwrite(),rdopen();
#else
#define	rdstrategy	nodev
#define	rdread		nodev
#define	rdwrite		nodev
#define rdopen		nodev
#endif


#import <sd.h>
#if NSD > 0
extern int sdopen(), sdclose(), sdstrategy(), sdread(), sdwrite(), sdioctl(); 
#else
#define	sdopen		nodev
#define	sdclose		nodev
#define	sdstrategy	nodev
#define	sdread		nodev
#define	sdwrite		nodev
#define	sdioctl		nodev
#endif


#import <fd.h>
#if NFD > 0
int fdopen(), fdclose(), fdioctl(), fdread(), fdwrite(), fdstrategy();
#else
#define fdopen		nodev
#define fdclose		nodev
#define fdioctl		nodev
#define fdread		nodev
#define fdwrite		nodev
#define fdstrategy	nodev
#endif	NFD

#define	swstrategy	nodev
#define	swread		nodev
#define	swwrite		nodev

struct bdevsw	bdevsw[] =
{
	/*
	 *	For block devices, every other block of 8 slots is 
	 *	reserved to NeXT.  The other slots are available for
	 *	the user.  This way we can both add new entries without
	 *	running into each other.  Be sure to fill in NeXT's
	 *	8 reserved slots when you jump over us -- we'll do the
	 *	same for you.
	 */

	/* 0 - 7 are reserved to NeXT */

	{ lddopen,	lddclose,	lddstrategy,	ldddump,	/* 0*/
	  lddsize,	0 },
	{ fdopen,	fdclose,	fdstrategy,	nodev,		/* 1*/
	  nodev,	0 },
	{ odopen,	odclose,	odstrategy,	oddump,		/* 2*/
	  odsize,	0 },
	{ nodev,	nodev,		nodev,		nodev,		/* 3*/
	  0,		0 },
	{ nodev,	nodev,		swstrategy,	nodev,		/* 4*/
	  0,		0 },
	{ rdopen,	nulldev,	rdstrategy,	nodev,		/* 5*/
	  nodev,	0 },
	{ sdopen,	sdclose,	sdstrategy,	nodev,		/* 6*/
	  nodev,	0 },
	{ nodev,	nodev,		nodev,		nodev,		/* 7*/
	  nodev,	0 },			/* 7 reserved for VM disk */

	/* 8 - 15 are reserved to the user */

	/* 16 - 23 are reserved to NeXT */
};

int	nblkdev = sizeof (bdevsw) / sizeof (bdevsw[0]);

int	cnopen(),cnread(),cnwrite(),cnioctl(), cnselect(), cnputc(),cngetc();

#import <zs.h>
#if NZS > 0
int	zsopen(),zsclose(),zsread(),zswrite(),zsioctl(),zsselect(),
	zsputc(),zsgetc();
#else
#define	zsopen		nodev
#define	zsclose		nodev
#define	zsread		nodev
#define	zswrite		nodev
#define	zsioctl		nodev
#define	zsselect	nodev
#define	zsputc		nodev
#define	zsgetc		nodev
#endif


#import <km.h>
#if NKM > 0
int	kmopen(),kmclose(),kmread(),kmwrite(),kmioctl(),kmselect(),
	kmputc(),kmgetc();
#else
#define	kmopen		nodev
#define	kmclose		nodev
#define	kmread		nodev
#define	kmwrite		nodev
#define	kmioctl		nodev
#define	kmselect	nodev
#define	kmputc		nodev
#define	kmgetc		nodev
#endif

#import <sg.h>
#if NSG > 0
int	sgopen(),sgclose(), sgioctl();
#else
#define sgopen		nodev
#define sdclose		nodev
#define sdioctl		nodev
#endif

#import <st.h>
#if NST > 0
int	stopen(),stclose(), stread(), stwrite(), stioctl();
#else
#define stopen		nodev
#define stclose		nodev
#define stread		nodev
#define stwrite		nodev
#define stioctl		nodev
#endif

#import <ev.h>
#if NEV > 0
int	evopen(),evclose(),evioctl(),evselect(),evmmap();
int	evsioctl();
#else
#define	evopen		nodev
#define	evclose		nodev
#define	evioctl		nodev
#define	evsioctl	nodev
#define	evselect	nodev
#define evmmap		nodev
#endif

#import <vol.h>
#if NVOL > 0
int	volopen(),volclose(),volioctl();
#else
#define	volopen		nodev
#define	volclose	nodev
#define	volioctl	nodev
#endif

#import <sound.h>
#if NSOUND > 0
extern int snd_unix_ioctl();
#define snd_unix_open	nulldev
#else
#define snd_unix_open	nodev
#define snd_unix_ioctl	nodev
#endif


#import <iplmeas.h>
#if NIPLMEAS > 0
int	ipl_open(), ipl_read(), ipl_ioctl();
#else
#define ipl_open	nodev
#define ipl_read	nodev
#define ipl_ioctl	nodev
#endif

#import <nfsmeas.h>
#if NNFSMEAS > 0
int	nfsmeas_open(), nfsmeas_read(), nfsmeas_ioctl();
#else
#define nfsmeas_open	nodev
#define nfsmeas_read	nodev
#define nfsmeas_ioctl	nodev
#endif

int	syopen(),syread(),sywrite(),syioctl(),syselect();

int 	mmread(),mmwrite(),mmmmap();
#define	mmselect	seltrue


#import <np.h>
#if NNP > 0
int np_open(), np_close(), np_ioctl(), np_select(), np_write();
int nps_open(), nps_ioctl(), nps_select();
#else
#define np_open		nodev
#define	np_close	nodev
#define	np_ioctl	nodev
#define np_select	nodev
#define	np_write	nodev
#define nps_open	nodev
#define	nps_ioctl	nodev
#define nps_select	nodev
#endif

#import <pty.h>
#if NPTY > 0
int	ptsopen(),ptsclose(),ptsread(),ptswrite(),ptsselect(),
	ptsstop(),ptsputc();
int	ptcopen(),ptcclose(),ptcread(),ptcwrite(),ptcselect();
int	ptyioctl();
#else
#define ptsopen		nodev
#define ptsclose	nodev
#define ptsread		nodev
#define ptswrite	nodev
#define ptsselect	nodev
#define ptcopen		nodev
#define ptcclose	nodev
#define ptcread		nodev
#define ptcwrite	nodev
#define ptyioctl	nodev
#define	ptcselect	nodev
#define	ptsstop		nulldev
#define ptsputc		nulldev
#endif

int	logopen(),logclose(),logread(),logioctl(),logselect();

int	seltrue();

int	vidopen(), vidioctl(), vidclose();

#if	NeXT && DEBUG
extern int	clockmmap();
#endif	NeXT && DEBUG

#define	 NO_CDEVICE							\
    {									\
	nodev,		nodev,		nodev,		nodev,		\
	nodev,		nodev,		nodev,		seltrue,	\
	nodev,		nodev,		nodev,				\
    }

struct cdevsw	cdevsw[] =
{
	/*
	 *	For character devices, every other block of 16 slots is
	 *	reserved to NeXT.  The other slots are available for
	 *	the user.  This way we can both add new entries without
	 *	running into each other.  Be sure to fill in NeXT's
	 *	16 reserved slots when you jump over us -- we'll do the
	 *	same for you.
	 */

	/* 0 - 15 are reserved to NeXT */

    {
	cnopen,		nulldev,	cnread,		cnwrite,	/* 0*/
	cnioctl,	nulldev,	nulldev,	cnselect,
	nodev,		cngetc,		cnputc,
    },
    {
	lddopen,	lddclose,	lddread,	lddwrite,	/* 1*/
	lddioctl,	lddstop,	lddreset,	lddselect,
	lddmmap,	nodev,		nodev,
    },
    {
	syopen,		nulldev,	syread,		sywrite,	/* 2*/
	syioctl,	nulldev,	nulldev,	syselect,
	nodev,		nodev,		nodev,
    },
    {
	nulldev,	nulldev,	mmread,		mmwrite,	/* 3*/
	nodev,		nulldev,	nulldev,	mmselect,
	mmmmap,		nodev,		nodev
    },
    {
	ptsopen,	ptsclose,	ptsread,	ptswrite,	/* 4*/
	ptyioctl,	ptsstop,	nulldev,	ptsselect,
	nodev,		kmgetc,		ptsputc,
    },
    {
	ptcopen,	ptcclose,	ptcread,	ptcwrite,	/* 5*/
	ptyioctl,	nulldev,	nulldev,	ptcselect,
	nodev,		nodev,		nodev
    },
    {
	logopen,	logclose,	logread,	nodev,		/* 6*/
	logioctl,	nodev,		nulldev,	logselect,
	nodev,		nodev,		nodev
    },
    {
	nulldev,	nulldev,	swread,		swwrite,	/* 7*/
	nodev,		nodev,		nulldev,	nodev,
	nodev,		nodev,		nodev
    },
    {
	np_open,	np_close,	nodev,		np_write,	/* 8*/
	np_ioctl,	nodev,		nodev,		np_select,
	nodev,		nodev,		nodev
    },
    {
	odopen,		odclose,	odread,		odwrite,	/* 9*/
	odioctl,	nodev,		nulldev,	seltrue,
	nodev,		nodev,		nodev
    },
    {
	evopen,		evclose,	nodev,		nodev,		/*10*/
	evioctl,	nulldev,	nulldev,	evselect,
	evmmap,		nodev,		nodev
    },
    {
	zsopen,		zsclose,	zsread,		zswrite,	/*11*/
	zsioctl,	nulldev,	nulldev,	zsselect,
	nodev,		zsgetc,		zsputc,
    },
    {
	kmopen,		kmclose,	kmread,		kmwrite,	/*12*/
	kmioctl,	nulldev,	nulldev,	kmselect,
	nodev,		kmgetc,		kmputc,
    },
    {
	vidopen,	vidclose,	nodev,		nodev,		/*13*/
	vidioctl,	nodev,		nodev,		seltrue,
	nodev,		nodev,		nodev
    },
    {
	sdopen,		sdclose,	sdread,		sdwrite,	/*14*/
	sdioctl,	nodev,		nulldev,	seltrue,
	nodev,		nodev,		nodev
    },
    NO_CDEVICE,		/* reserved for slot device */			/*15*/

	/* 16 - 31 are reserved to the user */
    NO_CDEVICE,								/*16*/
    NO_CDEVICE,								/*17*/
    NO_CDEVICE,								/*18*/
    NO_CDEVICE,								/*19*/
    NO_CDEVICE,								/*20*/
    NO_CDEVICE,								/*21*/
    NO_CDEVICE,								/*22*/
    NO_CDEVICE,								/*23*/
    NO_CDEVICE,								/*24*/
    NO_CDEVICE,								/*25*/
    NO_CDEVICE,								/*26*/
    NO_CDEVICE,								/*27*/
    NO_CDEVICE,								/*28*/
    NO_CDEVICE,								/*29*/
    NO_CDEVICE,								/*30*/
    NO_CDEVICE,								/*31*/

	/* 32 - 47 are reserved to NeXT */
    {
	nulldev,	nulldev,	nulldev,	nulldev,	/*32*/
	nodev,		nulldev,	nulldev,	nodev,
#if	NeXT && DEBUG
	clockmmap,	nodev,		nodev
#else	NeXT && DEBUG
	nodev,		nodev,		nodev
#endif	NeXT && DEBUG
    },
    {
	sgopen,		sgclose,	nodev,		nodev,		/*33*/
	sgioctl,	nodev,		nodev,		nodev,
	nodev,		nodev,		nodev
    },
    {
	stopen,		stclose,	stread,		stwrite,	/*34*/
	stioctl,	nodev,		nodev,		nodev,
	nodev,		nodev,		nodev
    },
    {
	ipl_open,	nulldev,	ipl_read,	nodev,		/*35*/
	ipl_ioctl,	nodev,		nulldev,	seltrue,
	nodev,		nodev,		nodev
    },
    {
	snd_unix_open,	nodev,		nodev,		nodev,		/*36*/
	snd_unix_ioctl,	nulldev,	nulldev,	nodev,
	nodev,		nodev,		nodev
    },
    NO_CDEVICE,		/* reserved for vmdisk device */		/*37*/
    {
	nfsmeas_open,	nulldev,	nfsmeas_read,	nodev,		/*38*/
	nfsmeas_ioctl,	nodev,		nulldev,	seltrue,
	nodev,		nodev,		nodev
    },
    {
	nps_open,	nulldev,	nodev,		nodev,		/*39*/
	nps_ioctl,	nulldev,	nulldev,	nps_select,
	nodev,		nodev,		nodev
    },
    {
	nulldev,	nulldev,	nodev,		nodev,		/*40*/
	evsioctl,	nulldev,	nulldev,	nodev,
	nodev,		nodev,		nodev
    },
    {
	fdopen,		fdclose,	fdread,		fdwrite,	/*41*/
	fdioctl,	nodev,		nulldev,	seltrue,
	nodev,		nodev,		nodev
    },
    {
	volopen,	volclose,	nodev,		nodev,		/*42*/
	volioctl,	nodev,		nulldev,	seltrue,
	nodev,		nodev,		nodev
    },
};
int	nchrdev = sizeof (cdevsw) / sizeof (cdevsw[0]);

int	mem_no = 3; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(4, 0);

