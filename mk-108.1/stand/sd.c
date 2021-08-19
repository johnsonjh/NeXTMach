/*	@(#)sd.c	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 *
 * sd.c -- implementation of disk specific scsi routines
 * STANDALONE MONITOR VERSION
 *
 * Supports scsi disks meeting mandatory requirements of
 * ANSI X3.131.
 *
 * HISTORY
 * 27-Feb-88   Mike DeMoney (mike) at NeXT
 *	Added support for read capacity, start command, and test unit
 *	ready.  Also added retries for reads that fail.
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#import <sys/param.h>
#ifdef SUN_NFS
#import <sys/time.h>
#import <sys/vnode.h>
#import <ufs/inode.h>
#import <ufs/fs.h>
#import <ufs/fsdir.h>
#else !SUN_NFS
#import <sys/inode.h>
#import <sys/fs.h>
#import <sys/dir.h>
#endif !SUN_NFS
#import <nextdev/dma.h>
#import <stand/saio.h>
#import <nextdev/disk.h>
#import <stand/scsireg.h>
#import <stand/scsivar.h>

/* #define	SD_DEBUG	1	/* */

#define	NSD		1
#define	MAX_SDTRIES	3

/*
 * private per-device sd info
 * this struct parallels scsi_device and bus_device structs
 */
struct scsi_disk_device {
	struct	scsi_device sdd_sd;	/* parallel scsi_device struct */
	int	sdd_blklen;		/* disk logical block length */
	/*
	 * DMA restricts label to longword alignment
	 * sdd_dlp pts into sdd_dlbuf longword aligned
	 */
	struct	disk_label *sdd_dlp;	/* disk label */
	u_char	sdd_dlbuf[sizeof(struct disk_label)+DMA_BEGINALIGNMENT-1];
	union {
		char Sdd_irbuf[sizeof(struct inquiry_reply)
			       + DMA_BEGINALIGNMENT - 1];
		char Sdd_erbuf[sizeof(struct esense_reply)
			       + DMA_BEGINALIGNMENT - 1];
		char Sdd_crbuf[sizeof(struct capacity_reply)
			       + DMA_BEGINALIGNMENT - 1];
	} sdd_replybuf;
	u_char	sdd_labelvalid;		/* label is ok to use */
	u_char	sdd_inuse;
} sd_sdd[NSD];

#ifndef	sizeofA
#define	sizeofA(x)	(sizeof(x)/sizeof((x)[0]))
#endif	!def(sizeofA)

void sdopen();
void sdclose();
int sdstrategy();

static int sdfind();
static int sdslave();
static int sdinit();
static int sdblklen();
static void sdreqsense();
static void sdstart();
static struct scsi_disk_device *sdlookup();
static void sdfreesdd();
static int sdread();
static void sdgetlabel();
int sdchecklabel();
static int sdcmd();
static void sdfail();

void
sdopen(iop)
struct iob *iop;
{
	int unit;
	struct scsi_disk_device *sddp;

#ifdef	SD_DEBUG
	printf("sdopen() i_ctrl = %d  i_unit = %d   i_part = %d\n",
		iop->i_ctrl,iop->i_unit,iop->i_part);
#endif	SD_DEBUG
	scsetup();
	if (iop->i_unit < 0)
		iop->i_unit = 0;
	if (iop->i_part < 0)
		iop->i_part = 0;
	if (iop->i_part >= NPART)
		_stop("bad partition number");
	if (iop->i_ctrl < 0) {
		iop->i_ctrl = sdfind(0, 0);
		iop->i_unit = 0;
		if (iop->i_ctrl < 0)
			_stop("no SCSI disk");
		printf("Booting from SCSI target %d lun %d\n", iop->i_ctrl,
		    iop->i_unit);
	} else {
		if (iop->i_ctrl >= SCSI_NTARGETS)
			_stop("illegal SCSI target");
		if (iop->i_unit >= SCSI_NLUNS)
			_stop("illegal SCSI lun");
		/* 07/12/89 fix ....*/
		iop->i_ctrl = sdfind(iop->i_ctrl, iop->i_unit);
		if (iop->i_ctrl < 0)
			_stop("no response from SCSI disk");
		/* end of fix */
	}
	sddp = sdlookup(iop->i_ctrl, iop->i_unit);

	if (! sdinit(sddp))
		_stop("Couldn't initialize SCSI device\n");
	if (!sddp->sdd_labelvalid)
		sdgetlabel(sddp);
	if (!sddp->sdd_labelvalid)
		_stop("Couldn't read disk label");
	if (sddp->sdd_blklen > sddp->sdd_dlp->dl_secsize
	    || (sddp->sdd_dlp->dl_secsize % sddp->sdd_blklen) != 0) {
		alert_msg("Disk format error\n");
		alert_msg("Device block length (%d)\n", sddp->sdd_blklen);
		alert_msg("and file system sector size (%d) incompatable\n",
		    sddp->sdd_dlp->dl_secsize);
		_stop("format error");
	}
	iop->i_secsize = sddp->sdd_dlp->dl_secsize;
	if (sddp->sdd_dlp->dl_part[iop->i_part].p_size == 0)
		_stop("empty partition");
}

void
sdclose(iop)
struct iob *iop;
{
	sdfreesdd(sdlookup(iop->i_ctrl, iop->i_unit));
}

static int
sdfind(ctrl, lun)
{
	int target, logical_target = -1;

#ifdef	SD_DEBUG
	printf("sdfind() ctrl = %d  lun = %d\n",ctrl,lun);
#endif	SD_DEBUG
	for (target = 0; target < 7; target++) {
		if (sdslave(target, lun))
			logical_target++;
		if (logical_target == ctrl) {
#ifdef	SD_DEBUG
			printf("     returning %d\n",target);
#endif	SD_DEBUG
			return(target);
		}
	}
	return(-1);
}

static int
sdslave(target, lun)
int target, lun;
{
	struct scsi_disk_device sdd;
	struct scsi_device *sdp = &sdd.sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct inquiry_reply *irp;
	int i;

	irp = DMA_ALIGN(struct inquiry_reply *, &sdd.sdd_replybuf);

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do an INQUIRE cmd */
		bzero(&sdd, sizeof(sdd));
		c6p->c6_opcode = C6OP_INQUIRY;
		c6p->c6_lun = lun;
		c6p->c6_len = sizeof(struct inquiry_reply);
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_target = target;
		sdp->sd_lun = lun;
		sdp->sd_addr = (caddr_t)irp;
		sdp->sd_bcount = sizeof(*irp);
		sdp->sd_read = 1;
		/*
		 * if command wasn't successful or it's not a disk out there,
		 * slave fails
		 */
		if (sdcmd(&sdd) && irp->ir_devicetype == DEVTYPE_DISK)
			return(1);
	}
	return(0);
}

static int
sdinit(sddp)
struct scsi_disk_device *sddp;
{
	/*
	 * spin the drive up
	 * wait for it to come ready
	 * read the capacity
	 */
	sdstart(sddp);
	sddp->sdd_blklen = sdblklen(sddp);
	if (!sddp->sdd_blklen)
		return(0);
	return(1);
}

static int
sdblklen(sddp)
struct scsi_disk_device *sddp;
{
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_10 *c10p = &sdp->sd_cdb.cdb_c10;
	struct capacity_reply *crp;
	int i;

	crp = DMA_ALIGN(struct capacity_reply *, &sddp->sdd_replybuf);

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do an READ CAPACITY cmd */
		bzero(c10p, sizeof(*c10p));
		bzero(crp, sizeof(*crp));
		c10p->c10_opcode = C10OP_READCAPACITY;
		c10p->c10_lun = sdp->sd_lun;
		c10p->c10_ctrl = CTRL_NOLINK;
		sdp->sd_addr = (caddr_t)crp;
		sdp->sd_bcount = sizeof(*crp);
		sdp->sd_read = 1;
		if (sdcmd(sddp))
			return(crp->cr_blklen);
	}
	sdfail(sddp, "READ CAPACITY");
	return(0);
}

static void
sdreqsense(sddp)
struct scsi_disk_device *sddp;
{
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct esense_reply *erp;
	int i;

	erp = DMA_ALIGN(struct esense_reply *, &sddp->sdd_replybuf);

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do a REQUEST SENSE cmd */
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_REQSENSE;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_len = sizeof(*erp);
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_addr = (caddr_t)erp;
		sdp->sd_bcount = sizeof(*erp);
		sdp->sd_read = 1;
		if (scpollcmd(sdp))
			return;
		if (sdp->sd_state == SDSTATE_COMPLETED
		    && sdp->sd_status == STAT_BUSY)
			DELAY(1000000);	/* 1 sec */
	}
	sdfail(sddp, "REQ SENSE");
}

static void
sdstart(sddp)
struct scsi_disk_device *sddp;
{
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct cdb_10 *c10p = &sdp->sd_cdb.cdb_c10;
	int tries, i;


	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do a START UNIT cmd */
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_STARTSTOP;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_lba = (1 << 16);	/* IMMEDIATE bit */
		c6p->c6_len = 1;		/* START bit */
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_bcount = 0;
		if (sdcmd(sddp))
			break;
	}

	/* Do TEST UNIT READY cmds until success */
	tries = 0;
	do {
		if (tries) {
			if (tries < 0) {
				printf("Waiting for drive to come ready");
				tries = 1;
			} else {
				printf(".");
				tries++;
			}
			DELAY(1000000);	/* 1 sec */
		} else
			tries = -1;
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_TESTRDY;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_bcount = 0;
	} while (!sdcmd(sddp));
	if (tries > 0)
		printf("\n");
}

static struct scsi_disk_device *
sdlookup(target, lun)
{
	int i;
	int free = -1;

#ifdef	SD_DEBUG
	printf("sdlookup() target = %d  lun = d\n",target,lun);
#endif	SD_DEBUG
	for (i = 0; i < sizeofA(sd_sdd); i++) {
		if (sd_sdd[i].sdd_inuse == 0) {
			free = i;
			continue;
		}
		if (sd_sdd[i].sdd_sd.sd_target == target
		    && sd_sdd[i].sdd_sd.sd_lun == lun)
			return(&sd_sdd[i]);
	}
	if (free >= 0) {
		bzero(&sd_sdd[free], sizeof(struct scsi_disk_device));
		sd_sdd[free].sdd_inuse = 1;
		sd_sdd[free].sdd_sd.sd_target = target;
		sd_sdd[free].sdd_sd.sd_lun = lun;
		return(&sd_sdd[free]);
	}
	_stop("out of SCSI disk slots");
}

static void
sdfreesdd(sddp)
struct scsi_disk_device *sddp;
{
	sddp->sdd_inuse = 0;
}

int
sdstrategy(iop, func)
struct iob *iop;
int func;
{
	struct scsi_disk_device *sddp = sdlookup(iop->i_ctrl, iop->i_unit);
	struct scsi_device *sdp = &sddp->sdd_sd;
	int lba, bn, bratio, ok;

	if (! sddp->sdd_labelvalid) {
		alert_msg("Disk label invalid\n");
		return(-1);
	}
		
	bn = iop->i_bn + sddp->sdd_dlp->dl_part[iop->i_part].p_base;

	/*
	 *  Add in dl_front so partition space starts
	 *  at zero offset.  For backward compatibility
	 *  only do this if the first partition starts
	 *  at zero.  FIXME: remove this someday.
	 */
	if (sddp->sdd_dlp->dl_part[0].p_base == 0)
		bn += sddp->sdd_dlp->dl_front;

	/*
	 * bratio is conversion multiple from file system
	 * blocks to device blocks
	 */
	bratio = iop->i_secsize / sddp->sdd_blklen;
	lba = bn * bratio;
	switch (func) {
	case READ:
		ok = sdread(sddp, lba, iop->i_ma, iop->i_cc);
		break;
	default:
		/*
		 * If you implement writes, you probably ought
		 * to becareful about unit attentions clearing
		 * label valid.
		 */
		_stop("unsupported SCSI disk operation");
	}
	if (ok)
		return(iop->i_cc - sdp->sd_resid);
	return(-1);
}

static int
sdread(sddp, lba, ma, cc)
struct scsi_disk_device *sddp;
char *ma;
{
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	int nblk, i;

	nblk = howmany(cc, sddp->sdd_blklen);

	for (i = 0; i < MAX_SDTRIES; i++) {
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_READ;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_lba = lba;
		c6p->c6_len = (nblk == 256) ? 0 : nblk;
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_bcount = cc;
		sdp->sd_addr = ma;
		sdp->sd_read = 1;
		if (sdcmd(sddp))
			return(1);
	}
	sdfail(sddp, "READ");
	return(0);
}

static void
sdgetlabel(sddp)
struct scsi_disk_device *sddp;
{
	int tries, ok, lba;

	sddp->sdd_dlp = DMA_ALIGN(struct disk_label *, sddp->sdd_dlbuf);
	for (tries = 0; tries < NLABELS; tries++) {
		bzero(sddp->sdd_dlp, sizeof(*sddp->sdd_dlp));
		lba = howmany(sizeof(*sddp->sdd_dlp), sddp->sdd_blklen) * tries;
		ok = sdread(sddp,lba,sddp->sdd_dlp,sizeof(*sddp->sdd_dlp));
		if (ok && sdchecklabel(sddp->sdd_dlp, lba)) {
			sddp->sdd_labelvalid = 1;
			return;
		}
	}
}

int
sdchecklabel(dlp, blkno)
struct disk_label *dlp;
{
	u_short checksum, cksum(), *dl_cksum, size;

	if (dlp->dl_version == DL_V1 || dlp->dl_version == DL_V2) {
		size = sizeof (struct disk_label);
		dl_cksum = &dlp->dl_checksum;
	} else
	if (dlp->dl_version == DL_V3) {
		size = sizeof (struct disk_label) - sizeof (dlp->dl_un);
		dl_cksum = &dlp->dl_v3_checksum;
	} else {
		alert_msg("bad magic\n");
		return (0);
	}
	if (dlp->dl_label_blkno != blkno) {
		alert_msg("bad blkno\n");
		return (0);	/* label not where it's supposed to be */
	}
	dlp->dl_label_blkno = 0;
	checksum = *dl_cksum;
	*dl_cksum = 0;
	if (cksum(dlp, size) != checksum) {
		printf("Bad cksum\n");
		return (0);
	}
	*dl_cksum = checksum;
	return (1);
}

static int
sdcmd(sddp)
struct scsi_disk_device *sddp;
{
	struct scsi_device *sdp = &sddp->sdd_sd;

#ifdef SD_DEBUG
	printf("Running cmd 0x%x  target 0x%x\n",
		 sdp->sd_cdb.cdb_opcode, sdp->sd_target);
#endif SD_DEBUG
	if (scpollcmd(sdp))
		return(1);

#ifdef SD_DEBUG
	printf("Cmd failed: state=%d status=0x%x\n", sdp->sd_state,
	    sdp->sd_status);
#endif SD_DEBUG
	switch (sdp->sd_state) {
	case SDSTATE_SELTIMEOUT:
	case SDSTATE_ABORTED:
	case SDSTATE_UNIMPLEMENTED:
		break;
	case SDSTATE_COMPLETED:
		switch (sdp->sd_status) {
		case STAT_CHECK:
			sdreqsense(sddp);
			break;
		case STAT_BUSY:
			DELAY(1000000);		/* 1 sec */
			break;
		}
		break;
	default:
		alert_msg("bad state detected in sdcmd: %d\n", sdp->sd_state);
		break;
	}
	return(0);
}

static void
sdfail(sddp, msg)
struct scsi_disk_device *sddp;
char *msg;
{
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct	esense_reply *erp;

	erp = DMA_ALIGN(struct esense_reply *, &sddp->sdd_replybuf);

	alert_msg("%s: ", msg);
	switch (sdp->sd_state) {
	case SDSTATE_SELTIMEOUT:
		alert_msg("Selection timeout on target\n");
		break;
	case SDSTATE_COMPLETED:
		switch (sdp->sd_status) {
		case STAT_CHECK:
			alert_msg("failed, sense key: 0x%x\n", erp->er_sensekey);
			break;
		case STAT_BUSY:
			alert_msg("target busy\n");
			break;
		default:
			alert_msg("bad status detected in sdcmd: %d\n",
			    sdp->sd_status);
			break;
		}
		break;
	case SDSTATE_ABORTED:
		alert_msg("Target disconnected\n");
		break;
	case SDSTATE_UNIMPLEMENTED:
		alert_msg("sc driver refused command\n");
		break;
	default:
		alert_msg("bad state detected in sdfail: %d\n", sdp->sd_state);
		break;
	}
}

