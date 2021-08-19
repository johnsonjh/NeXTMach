/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)sys.c	7.1 (Berkeley) 6/5/86
 */

/*
 * HISTORY
 * 25-Mar-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added C versions of strcmp and strcpy to prevent picking
 *	up C library version that use instructions not availabe on uVaxen (and
 *	the boot program does no emulation).
 *
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

/*
 * Sleazy version of ctype macros
 * -- Mike
 */
#define	isspace(c)	((c) == ' ' || (c) == '\t')
#define	isdigit(c)	((c) >= '0' && (c) <= '9')
#define	islower(c)	((c) >= 'a' && (c) <= 'z')
#define	isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define	isalpha(c)	(islower(c) || isupper(c))
#define	tolower(c)	((c) + ('a' - 'A'))

ino_t	dlook();

struct dirstuff {
	int loc;
	struct iob *io;
};

#define DMA_BUG		1
#ifdef DMA_BUG
char xxbuf[MAXBSIZE+DMA_ENDALIGNMENT-1];
char *xxbufp;
#endif DMA_BUG

static
openi(n, io)
	register struct iob *io;
{
	register struct dinode *dp;
	int cc;

	io->i_offset = 0;
	io->i_bn = fsbtodb(&io->i_fs, itod(&io->i_fs, n));
	io->i_cc = io->i_fs.fs_bsize;
	io->i_ma = io->i_buf;
	cc = devread(io);
	if (cc < 0)
		return (cc);
	dp = (struct dinode *)io->i_buf;
	io->i_ino.i_ic = dp[itoo(&io->i_fs, n)].di_ic;
	return (cc);
}

static
find(path, file)
	register char *path;
	struct iob *file;
{
	register char *q;
	char c;
	int n;

	if (path==NULL || *path=='\0') {
		alert_msg("null path\n");
		return (0);
	}

	if (openi((ino_t) ROOTINO, file) < 0) {
		alert_msg("can't read root inode\n");
		return (0);
	}
	while (*path) {
		while (*path == '/')
			path++;
		q = path;
		while(*q != '/' && *q != '\0')
			q++;
		c = *q;
		*q = '\0';
		if (q == path) path = "." ;	/* "/" means "/." */

		if ((n = dlook(path, file)) != 0) {
			if (c == '\0')
				break;
			if (openi(n, file) < 0)
				return (0);
			*q = c;
			path = q;
			continue;
		} else {
			alert_msg("%s: not found\n", path);
			return (0);
		}
	}
	return (n);
}

static daddr_t
sbmap(io, bn)
	register struct iob *io;
	daddr_t bn;
{
	register struct inode *ip;
	int i, j, sh;
	daddr_t nb, *bap;

	ip = &io->i_ino;
	if (bn < 0) {
		alert_msg("bn negative\n");
		return ((daddr_t)0);
	}

	/*
	 * blocks 0..NDADDR are direct blocks
	 */
	if(bn < NDADDR) {
		nb = ip->i_db[bn];
		return (nb);
	}

	/*
	 * addresses NIADDR have single and double indirect blocks.
	 * the first step is to determine how many levels of indirection.
	 */
	sh = 1;
	bn -= NDADDR;
	for (j = NIADDR; j > 0; j--) {
		sh *= NINDIR(&io->i_fs);
		if (bn < sh)
			break;
		bn -= sh;
	}
	if (j == 0) {
		alert_msg("bn ovf %D\n", bn);
		return ((daddr_t)0);
	}

	/*
	 * fetch the first indirect block address from the inode
	 */
	nb = ip->i_ib[NIADDR - j];
	if (nb == 0) {
		alert_msg("bn void %D\n",bn);
		return ((daddr_t)0);
	}

	/*
	 * fetch through the indirect blocks
	 */
	for (; j <= NIADDR; j++) {
		if (blknos[j] != nb) {
			io->i_bn = fsbtodb(&io->i_fs, nb);
			io->i_ma = b[j];
			io->i_cc = io->i_fs.fs_bsize;
			if (devread(io) != io->i_fs.fs_bsize) {
				if (io->i_error)
					errno = io->i_error;
				alert_msg("bn %D: read error\n", io->i_bn);
				return ((daddr_t)0);
			}
			blknos[j] = nb;
		}
		bap = (daddr_t *)b[j];
		sh /= NINDIR(&io->i_fs);
		i = (bn / sh) % NINDIR(&io->i_fs);
		nb = bap[i];
		if(nb == 0) {
			alert_msg("bn void %D\n",bn);
			return ((daddr_t)0);
		}
	}
	return (nb);
}

static ino_t
dlook(s, io)
	char *s;
	register struct iob *io;
{
	register struct direct *dp;
	struct direct *readdir();
	register struct inode *ip;
	struct dirstuff dirp;
	int len;

	if (s == NULL || *s == '\0')
		return (0);
	ip = &io->i_ino;
	if ((ip->i_mode&IFMT) != IFDIR) {
		alert_msg("%s: not a directory\n", s);
		return (0);
	}
	if (ip->i_size == 0) {
		alert_msg("%s: zero length directory\n", s);
		return (0);
	}
	len = strlen(s);
	dirp.loc = 0;
	dirp.io = io;
	for (dp = readdir(&dirp); dp != NULL; dp = readdir(&dirp)) {
		if(dp->d_ino == 0)
			continue;
		if (dp->d_namlen == len && !strcmp(s, dp->d_name))
			return (dp->d_ino);
	}
	return (0);
}

/*
 * get next entry in a directory.
 */
struct direct *
readdir(dirp)
	register struct dirstuff *dirp;
{
	register struct direct *dp;
	register struct iob *io;
	daddr_t lbn, d;
	int off;

	io = dirp->io;
	for(;;) {
		if (dirp->loc >= io->i_ino.i_size)
			return (NULL);
		off = blkoff(&io->i_fs, dirp->loc);
		if (off == 0) {
			lbn = lblkno(&io->i_fs, dirp->loc);
			d = sbmap(io, lbn);
			if(d == 0)
				return NULL;
			io->i_bn = fsbtodb(&io->i_fs, d);
			io->i_ma = io->i_buf;
			io->i_cc = blksize(&io->i_fs, &io->i_ino, lbn);
			if (devread(io) < 0) {
				errno = io->i_error;
				alert_msg("bn %D: directory read error\n",
					io->i_bn);
				return (NULL);
			}
		}
		dp = (struct direct *)(io->i_buf + off);
		dirp->loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		return (dp);
	}
}

lseek(fdesc, addr, ptr)
	int fdesc, ptr;
	off_t addr;
{
	register struct iob *io;

	if (ptr != 0) {
		alert_msg("Seek not from beginning of file\n");
		errno = EOFFSET;
		return (-1);
	}
	fdesc -= 3;
	if (fdesc < 0 || fdesc >= NFILES ||
	    ((io = &iob[fdesc])->i_flgs & F_ALLOC) == 0) {
		errno = EBAD;
		return (-1);
	}
	io->i_offset = addr;
	io->i_bn = addr / io->i_secsize;
	io->i_cc = 0;
	return (0);
}

getc(fdesc)
	int fdesc;
{
	register struct iob *io;
	register struct fs *fs;
	register char *p;
	int c, lbn, off, size, diff;


	if (fdesc >= 0 && fdesc <= 2)
		return (getchar());
	fdesc -= 3;
	if (fdesc < 0 || fdesc >= NFILES ||
	    ((io = &iob[fdesc])->i_flgs&F_ALLOC) == 0) {
		errno = EBAD;
		return (-1);
	}
	p = io->i_ma;
	if (io->i_cc <= 0) {
		if ((io->i_flgs & F_FILE) != 0) {
			diff = io->i_ino.i_size - io->i_offset;
			if (diff <= 0)
				return (-1);
			fs = &io->i_fs;
			lbn = lblkno(fs, io->i_offset);
			io->i_bn = fsbtodb(fs, sbmap(io, lbn));
			off = blkoff(fs, io->i_offset);
			size = blksize(fs, &io->i_ino, lbn);
		} else {
			io->i_bn = io->i_offset / io->i_secsize;
			off = 0;
			size = io->i_secsize;
		}
		io->i_ma = io->i_buf;
		io->i_cc = size;
		if (devread(io) < 0) {
			errno = io->i_error;
			return (-1);
		}
		if ((io->i_flgs & F_FILE) != 0) {
			if (io->i_offset - off + size >= io->i_ino.i_size)
				io->i_cc = diff + off;
			io->i_cc -= off;
		}
		p = &io->i_buf[off];
	}
	io->i_cc--;
	io->i_offset++;
	c = (unsigned)*p++;
	io->i_ma = p;
	return (c);
}

int	errno;

read(fdesc, buf, count)
	int fdesc, count;
	char *buf;
{
	register i, size;
	register struct iob *file;
	register struct fs *fs;
	int lbn, off;

	errno = 0;
	if (fdesc >= 0 & fdesc <= 2) {
		i = count;
		do {
			*buf = getchar();
		} while (--i && *buf++ != '\n');
		return (count - i);
	}
	fdesc -= 3;
	if (fdesc < 0 || fdesc >= NFILES ||
	    ((file = &iob[fdesc])->i_flgs&F_ALLOC) == 0) {
		errno = EBAD;
		return (-1);
	}
	if ((file->i_flgs&F_READ) == 0) {
		errno = EBAD;
		return (-1);
	}
	if ((file->i_flgs & F_FILE) == 0) {
		file->i_cc = count;
		file->i_ma = buf;
		file->i_bn = (file->i_offset / file->i_secsize);
		i = devread(file);
		file->i_offset += count;
		if (i < 0)
			errno = file->i_error;
		return (i);
	}
	if (file->i_offset+count > file->i_ino.i_size)
		count = file->i_ino.i_size - file->i_offset;
	if ((i = count) <= 0)
		return (0);
	/*
	 * While reading full blocks, do I/O into user buffer.
	 * Anything else uses getc().
	 */
	fs = &file->i_fs;
	while (i) {
		off = blkoff(fs, file->i_offset);
		lbn = lblkno(fs, file->i_offset);
		size = blksize(fs, &file->i_ino, lbn);
		if (off == 0 && size <= i) {
			file->i_bn = fsbtodb(fs, sbmap(file, lbn));
#ifdef DMA_BUG
			file->i_cc = roundup(size, DMA_ENDALIGNMENT);
			file->i_ma = xxbufp;
#else !DMA_BUG
			file->i_cc = size;
			file->i_ma = buf;
#endif DMA_BUG
			if (devread(file) < 0) {
				errno = file->i_error;
				return (-1);
			}
#ifdef DMA_BUG
			bcopy(xxbufp, buf, size);
#endif DMA_BUG
			file->i_offset += size;
			file->i_cc = 0;
			buf += size;
			i -= size;
		} else {
			size -= off;
			if (size > i)
				size = i;
			i -= size;
			do {
				*buf++ = getc(fdesc+3);
			} while (--size);
		}
	}
	return (count);
}

write(fdesc, buf, count)
	int fdesc, count;
	char *buf;
{
	register i;
	register struct iob *file;

	errno = 0;
	if (fdesc >= 0 && fdesc <= 2) {
		i = count;
		while (i--)
			putchar(*buf++);
		return (count);
	}
	fdesc -= 3;
	if (fdesc < 0 || fdesc >= NFILES ||
	    ((file = &iob[fdesc])->i_flgs&F_ALLOC) == 0) {
		errno = EBAD;
		return (-1);
	}
	if ((file->i_flgs&F_WRITE) == 0) {
		errno = EBAD;
		return (-1);
	}
	file->i_cc = count;
	file->i_ma = buf;
	file->i_bn = (file->i_offset / file->i_secsize);
	i = devwrite(file);
	file->i_offset += count;
	if (i < 0)
		errno = file->i_error;
	return (i);
}

int	openfirst = 1;

open(str, how)
	char *str;
	int how;
{
	register char *cp;
	int i, structure;
	register struct iob *file;
	register struct devsw *dp;
	int fdesc;
	char *atob();

	if (openfirst) {
		for (i = 0; i < NFILES; i++)
			iob[i].i_flgs = 0;
		for (i = 0; i < NBUFS; i++)
			b[i] = (char *)roundup((unsigned)buffers[i],
					       DMA_ENDALIGNMENT);
#ifdef DMA_BUG
		xxbufp = (char *)roundup((unsigned)xxbuf, DMA_ENDALIGNMENT);
#endif DMA_BUG
		openfirst = 0;
	}

	for (fdesc = 0; fdesc < NFILES; fdesc++)
		if (iob[fdesc].i_flgs == 0)
			goto gotfile;
	_stop("No more file slots");
gotfile:
	(file = &iob[fdesc])->i_flgs |= F_ALLOC;
	iob_init(file);

	for (cp = str; *cp && *cp != '('; cp++)
			;
	if (*cp != '(') {
		alert_msg("Bad device\n");
		file->i_flgs = 0;
		errno = EDEV;
		return (-1);
	}
	*cp++ = '\0';
	for (dp = devsw; dp->dv_name; dp++) {
		if (!strcmp(str, dp->dv_name))
			goto gotdev;
	}
	alert_msg("Unknown device\n");
	file->i_flgs = 0;
	errno = ENIO;
	return (-1);
gotdev:
	*(cp-1) = '(';
	file->i_ino.i_dev = dp-devsw;
	file->i_ctrl = -1;
	file->i_unit = -1;
	file->i_part = -1;
	while (isspace(*cp))
		cp++;
	if (*cp == ',') {
		cp++;
		goto checkunit;
	}
	if (*cp == ')')
		goto opendev;
	cp = atob(cp, &file->i_ctrl);
	if (*cp == ',')
		cp++;
checkunit:
	while (isspace(*cp))
		cp++;
	if (*cp == ',') {
		cp++;
		goto checkpart;
	}
	if (*cp == ')')
		goto opendev;
	cp = atob(cp, &file->i_unit);
	if (*cp == ',')
		cp++;
checkpart:
	while (isspace(*cp))
		cp++;
	if (*cp == ')')
		goto opendev;
	if (*cp == ',') {
badsyntax:
		alert_msg("Bad filename syntax, use DEV(CTRL,UNIT,PART)FILE\n");
		file->i_flgs = 0;
		errno = EDEV;
		return (-1);
	}
	cp = atob(cp, &file->i_part);
	while (isspace(*cp))
		cp++;
	if (*cp != ')')
		goto badsyntax;
opendev:
	file->i_secsize = 0;
	file->i_filename = cp+1;
	structure = devopen(file);
	if (file->i_secsize <= 0) {
		errno = EDIO;
		alert_msg("device couldn't set file system sector size\n");
		return(-1);
	}
	if (file->i_secsize > MAXBSIZE) {
		errno = EDIO;
		alert_msg("file system sector size (%d) too large\n",
			file->i_secsize);
		return(-1);
	}
#ifdef DMA_BUG
	file->i_ma = xxbufp;
#else !DMA_BUG
	file->i_ma = (char *)(&file->i_fs);
#endif DMA_BUG
	if (*++cp == '\0' || structure == DS_PACKET) {
		file->i_flgs |= how+1;
		file->i_cc = 0;
		file->i_offset = 0;
		return (fdesc+3);
	}
	file->i_cc = SBSIZE;
	file->i_bn = SBLOCK;
	file->i_offset = 0;
	if (devread(file) < 0) {
		errno = file->i_error;
		alert_msg("super block read error\n");
		return (-1);
	}
#ifdef DMA_BUG
	bcopy(xxbufp, &file->i_fs, SBSIZE);
#endif DMA_BUG
	if ((i = find(cp, file)) == 0) {
		file->i_flgs = 0;
		errno = ESRCHF;
		return (-1);
	}
	if (how != 0) {
		alert_msg("Can't write files yet.. Sorry\n");
		file->i_flgs = 0;
		errno = EDIO;
		return (-1);
	}
	if (openi(i, file) < 0) {
		errno = file->i_error;
		return (-1);
	}
	file->i_offset = 0;
	file->i_cc = 0;
	file->i_flgs |= F_FILE | (how+1);
	return (fdesc+3);
}

iob_init(file)
struct iob *file;
{
	file->i_buf = (char *)roundup((unsigned)file->i_buffer,
	    DMA_ENDALIGNMENT);
}

close(fdesc)
	int fdesc;
{
	struct iob *file;

	fdesc -= 3;
	if (fdesc < 0 || fdesc >= NFILES ||
	    ((file = &iob[fdesc])->i_flgs&F_ALLOC) == 0) {
		errno = EBAD;
		return (-1);
	}
	if ((file->i_flgs&F_FILE) == 0)
		devclose(file);
	file->i_flgs = 0;
	return (0);
}

#ifdef	notdef
ioctl(fdesc, cmd, arg)
	int fdesc, cmd;
	char *arg;
{
	register struct iob *file;
	int error = 0;

	fdesc -= 3;
	if (fdesc < 0 || fdesc >= NFILES ||
	    ((file = &iob[fdesc])->i_flgs&F_ALLOC) == 0) {
		errno = EBAD;
		return (-1);
	}
	switch (cmd) {

	case SAIOHDR:
		file->i_flgs |= F_HDR;
		break;

	case SAIOCHECK:
		file->i_flgs |= F_CHECK;
		break;

	case SAIOHCHECK:
		file->i_flgs |= F_HCHECK;
		break;

	case SAIONOBAD:
		file->i_flgs |= F_NBSF;
		break;

	case SAIODOBAD:
		file->i_flgs &= ~F_NBSF;
		break;

	default:
		error = devioctl(file, cmd, arg);
		break;
	}
	if (error < 0)
		errno = file->i_error;
	return (error);
}
#endif notdef

exit(code)
{
	if (code != 0)
		_stop("Command failed");
	_exit(0);
}

_stop(s)
	char *s;
{
	int i;

	for (i = 0; i < NFILES; i++)
		if (iob[i].i_flgs != 0)
			close(i);
	alert (s);
	alert ("\n");
}

#if 0
strlen(s)
	register char	*s;
{
	register n;

	n = 0;
	while (*s++)
		n++;
	return(n);
}

strcmp(s1, s2)
	register char	*s1, *s2;
{
	while (*s1 == *s2) {
		if (*s1++ == '\0')
			return(0);
		s2++;
	}
	return(*s1 - *s2);
}

strcpy(s1, s2)
	register char	*s1, *s2;
{
	while (*s2)
		*s1++ = *s2++;
}
#endif

unsigned digit();

/*
 * atob -- convert ascii to binary.  Accepts all C numeric formats.
 */
char *
atob(cp, iptr)
char *cp;
int *iptr;
{
	int minus = 0;
	register int value = 0;
	unsigned base = 10;
	unsigned d;

	*iptr = 0;
	if (!cp)
		return(0);

	while (isspace(*cp))
		cp++;

	while (*cp == '-') {
		cp++;
		minus = !minus;
	}

	/*
	 * Determine base by looking at first 2 characters
	 */
	if (*cp == '0') {
		switch (*++cp) {
		case 'X':
		case 'x':
			base = 16;
			cp++;
			break;

		case 'B':	/* a frill: allow binary base */
		case 'b':
			base = 2;
			cp++;
			break;
		
		default:
			base = 8;
			break;
		}
	}

	while ((d = digit(*cp)) < base) {
		value *= base;
		value += d;
		cp++;
	}

	if (minus)
		value = -value;

	*iptr = value;
	return(cp);
}

/*
 * digit -- convert the ascii representation of a digit to its
 * binary representation
 */
unsigned
digit(c)
char c;
{
	unsigned d;

	if (isdigit(c))
		d = c - '0';
	else if (isalpha(c)) {
		if (isupper(c))
			c = tolower(c);
		d = c - 'a' + 10;
	} else
		d = 999999; /* larger than any base to break callers loop */

	return(d);
}


