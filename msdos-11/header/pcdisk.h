/*****************************************************************************
*Filename: PCDISK.H - Defines & structures for ms-dos utilities
*					   
*
* EBS - Embedded file manager -
*
* Copyright Peter Van Oudenaren , 1989
* This code may not be redistributed in source or linkable object form
* without the consent of its author.
*
*
*
* Description: 
*	
*
*
*
*
*****************************************************************************/

#ifndef __PCDISK__
#define __PCDISK__ 1


/* Un-comment this to use supplied versions of malloc/free */
/* #define USEEBMALLOC 1 */
/* Un-comment this to use supplied version of printf */
/* #define USEEBPRINTF 1 */
/* Un-comment this to use FAT buffering */
/* #define USEFATBUF 1 */

/* Define ANSIFND to use ANSI style function definitions. Comment it out to 
   use K & R style declarations. 

   We do this because some ansi compilers complain about using prototypes with
   old style function declarations. We don't want to remove the K & R 
   style yet. Not everyone has an ansi compiler */

#define ANSIFND 1

/* The system forgets about DOS 4 if this is undefined. That means block
   numbers will be handled as 16 bit quantities instead of 32. Unless code
   space is at a real premium leave this as is.The package has worked fine
   with this support on and off, but most recent work has been done with it
   left on. Be a little careful if you disable it. */
#define SUPPORTDOS4 1

/* If defined prototyping is supported. 
	Note: If prototyping is not supported you will have some work to do */
#define PROTO 1

/* Handy heap diagnostic tool. See pc_malloc et al */
/* #define DBALLOC 1 */


/* Pseudo types to  eliminate confusion about the sizes of native types */
#define ULONG unsigned long int			/* 32 BIT unsigned */
#define LONG long int				/* 32 BIT signed */
#define UCOUNT unsigned short int		/* 16 BIT unsigned */
#define COUNT short int				/* 16 BIT signed */
#define VOID void				/* void if you have it */
#define BOOL int				/* native int */
#define INT  int				/* native int */

#define UTINY char				/* unsigned 8 bit */
#define TEXT char				/* char */
#define FAIL 0					/* OS exit error status */
#define YES 1
#define NO 0
#define IMPORT extern
#define GLOBAL /* extern */
#define LOCAL  static
#define FAST register


#define PROCL int 	/* processor interupt level for pcdisk modules */

#define PCFD int	/* file desc */

#ifdef	NeXT
#define BACKSLASH	'/'
#define BACKSLASH_STR	"/"
#else	NeXT
#define BACKSLASH '\\' 
#define BACKSLASH_STR	"\\"
#endif	NeXT

#ifdef SUPPORTDOS4
#define BLOCKT unsigned long int		/* 32 BIT unsigned */
#define BLOCKEQ0 0L
	typedef struct extdiskreq
		{
		ULONG sector;
		UCOUNT count;
		UCOUNT offs;
		UCOUNT seg;
		} EXTDISKREQ;
#else
#define BLOCKT unsigned int		     	/* 16 BIT unsigned */
#define BLOCKEQ0 0
#endif

/* For our developement env (80x6) Null is a function of MODEL NO.*/
#ifdef MSDOS
#if (defined(M_I86SM) || defined(M_I86MM))
#define  NULL    0
#elif (defined(M_I86CM) || defined(M_I86LM) || defined(M_I86HM))
#define  NULL    0L
#endif
#else
#include <stddef.h>
#endif


#define HIGHESTDRIVE 4        		/* Set this to the MAX drive logical
					 * unit number you wish to support -*/
#define EMAXPATH 64			/* Maximum path length Change if you 
					 * like */
#define NBLKBUFFS 20		  	/* Blocks in the buffer pool you may
					 * change */
#define MAXFPAGE 10			/* Size (in blocks) of in-memory fat
					 * portion if USEFATBUF is defined. You
					 * may change this too. */
#define PCDELETE (UTINY) 0xE5		/* MS-DOS file deleted char */
/* MS-DOS File attribs */
#define ARDONLY 0x1
#define AHIDDEN 0x2
#define ASYSTEM 0x4
#define AVOLUME 0x8 
#define ADIRENT 0x10
#define ARCHIVE 0x20
#define ANORMAL 0x00

#ifdef USEFATBUF
typedef struct fatswap
	{
	COUNT  pf_indlow;		/* Low FAT enry in the current page */
	COUNT  pf_indhigh;		/* High FAT enry in the current page */
	COUNT  pf_pagesize;		/* Number of blocks per page page */
	BLOCKT pf_blocklow;		/* Block number of first block in page
					 */
	UTINY  *pf;			/* Page buffer area */
	UTINY  *pdirty;			/* BYTE-map of blocks in page needing flushing */
	} FATSWAP;
#endif

/* Structure to contain block 0 image from the disk */
typedef struct ddrive {
		COUNT	opencount;
		UCOUNT	bytespcluster;		/*  */
		UCOUNT	fasize;			/* Nibbles per fat entry. (2 or 
						 * 4) */
		BLOCKT	rootblock;		/* First block of root dir */
		BLOCKT	firstclblock;		/* First block of cluster area 
						 */
		UCOUNT	driveno;		/* Driveno. Set when open 
						 * succeeds */
		UCOUNT	maxfindex;		/* Last element in the fat */
		BLOCKT	fatblock;		/* First block in fat */
		UCOUNT	secproot;		/* blocks in root dir */
#ifdef USEFATBUF
		FATSWAP *pfs;
#else
		UTINY	*pf;			/* Fat array goes here */
		UTINY	*pdirty;		/* Array to hold what fat 
						 * blocks to flush*/
#endif
		ULONG	bootaddr;
		TEXT	oemname[9];
		UCOUNT	bytspsector;		/* Must be 512 for this 
						 * implementation */
		UTINY	secpalloc;		/* Sectors per cluster */
		UCOUNT	secreserved;		/* Reserved sectors before the
						 * FAT */
		UTINY   numfats;		/* Number of FATS on the disk 
						 */
		UCOUNT	numroot;		/* Maximum # of root dir 
						 * entries */
		BLOCKT	numsecs;		/* Total # sectors on the disk 
						 */
		UTINY	mediadesc;		/* Media descriptor byte */
		UCOUNT	secpfat;		/* Size of each fat */
		UCOUNT	secptrk;		/* sectors per track */
		UCOUNT	numhead;		/* number of heads */
		BLOCKT	numhide;		/* # hidden sectors */
		} DDRIVE;

/* Data structure used to represent a directory structure in memory */
#define INOPBLOCK 16				/* 16 of these fit in a block 
						 */
typedef struct dosinode {
		TEXT	fname[8];
		TEXT	fext[3];
		UTINY	fattribute;		/* File attributes */
		TEXT	resarea[0x16-0xc];
		UCOUNT	ftime;			/* time & date lastmodified */
		UCOUNT	fdate;
		UCOUNT	fcluster;		/* Cluster for data file */
		ULONG	fsize;			/* File size */
		} DOSINODE;


typedef struct finode {
		TEXT	fname[8];
		TEXT	fext[3];
		UTINY	fattribute;		/* File attributes */
		TEXT	resarea[0x16-0xc];
		UCOUNT	ftime;			/* time & date lastmodified */
		UCOUNT	fdate;
		UCOUNT	fcluster;		/* Cluster for data file */
		ULONG	fsize;			/* File size */
		COUNT 	opencount;
		DDRIVE  *my_drive;
		BLOCKT	my_block;
		COUNT	my_index;
		struct finode *pnext;
		struct finode *pprev;
		} FINODE;


/* contain location information for a directory */
typedef struct dirblk                
		{
		BLOCKT	my_frstblock;		/* First block in this 
						 * directory */
		BLOCKT	my_block;		/* Current block number */
		UCOUNT	my_index;		/* dirent number in my cluster
						 */
		} DIRBLK;


/* Block buffer */
typedef struct blkbuff {
		struct blkbuff *pnext;
		ULONG lru;
		COUNT locked;		
		DDRIVE *pdrive;
		BLOCKT blockno;
		UTINY  *pdata;
		} BLKBUFF;

/* Object used to find a dirent on a disk and its parent's */
typedef struct drobj
		{
		DDRIVE	*pdrive;
		FINODE	*finode;
		DIRBLK	blkinfo;
		BOOL	isroot;			/* True if this is the root */
		BLKBUFF *pblkbuff;
		} DROBJ;

/* Date stamping buffer */
typedef struct datestr {
		UCOUNT date;		
		UCOUNT time;
		} DATESTR;

/* Internal file representation */
typedef struct pc_file {
	DROBJ *pobj; 	  	/* Info for getting at the inode */
	UCOUNT	flag;	  	/* Acces flags from po_open(). */
	LONG	fptr;	  	/* Current file pointer */
	UCOUNT ccl;		/* Cluster no associated with fptr. If this is
				 * 0 means the file is new and needs a cluster
				 */
	UCOUNT bccl;	  	/* Cluster # of stuf in buffer. 0 if buffer is 
				 * empty */
	BOOL   bdirty;	  	/* YES if buffer has been written too */
	BOOL   error;	  	/* If YES file must be closed */
	TEXT   *bbase; 	  	/* base address of I/O buffer */
	TEXT   *bend; 	  	/* end address of I/O buffer */
	} PC_FILE;

/* Structure for use by pc_stat and pc_gfirst, pc_gnext */
typedef struct dstat {
		TEXT	fname[9];		/* Null terminated file and 
						 * extension */
		TEXT	fext[4];
		UTINY	fattribute;		/* File attributes */
		UCOUNT	ftime;			/* time & date lastmodified see
						 * date */
		UCOUNT	fdate;			/* And time handlers for 
						 * getting info */
		ULONG	fsize;			/* File size */

		/* INTERNAL */
		TEXT	pname[9];		/* Pattern. */
		TEXT	pext[4];
		TEXT	path[EMAXPATH];
		DROBJ   *pobj;  		/* Info for getting at the 
						 * inode */
		DROBJ   *pmom;  		/* Info for getting at parent
						 * inode */
		} DSTAT;

/* User supplied parameter block for formatting */
typedef struct fmtparms {
		TEXT	oemname[9];		/* Only first 8 bytes are used
						 */
		UTINY	secpalloc;		/* Sectors per cluster */
		UCOUNT	secreserved;		/* Reserved sectors before the 
						 * FAT */
		UTINY   numfats;		/* Number of FATS on the disk
						 */
		COUNT   secpfat;		/* Sectors per fat */
		UCOUNT	numroot;		/* Maximum # of root dir
						 * entries */
		UTINY	mediadesc;		/* Media descriptor byte */
		UCOUNT	secptrk;		/* sectors per track */
		UCOUNT	numhead;		/* number of heads */
		UCOUNT	numtrk;			/* number of tracks */
		} FMTPARMS;


/* INTERNAL !! */
/* Structure to contain block 0 image from the disk */
struct pcblk0 {
		UTINY	jump;			/* Should be E9 or EB on 
						 * formatted disk */
		TEXT	oemname[9];
		UCOUNT	bytspsector;		/* Must be 512 for this
						 * implementation */
		UTINY	secpalloc;		/* Sectors per cluster */
		UCOUNT	secreserved;		/* Reserved sectors before the 
						 * FAT */
		UTINY   numfats;		/* Number of FATS on the disk
						 */
		UCOUNT	numroot;		/* Maximum # of root dir 
						 * entries */
		UCOUNT	numsecs;		/* Total # sectors on the disk 
						 */
		UTINY	mediadesc;		/* Media descriptor byte */
		UCOUNT	secpfat;		/* Size of each fat */
		UCOUNT	secptrk;		/* sectors per track */
		UCOUNT	numhead;		/* number of heads */
		UCOUNT	numhide;		/* # hidden sectors High word 
						 * if DOS4 */
		UCOUNT	numhide2;		/* # hidden sectors Low word if 
						 * DOS 4 */
		ULONG	numsecs2;		/* # secs if numhid+numsec > 
						 * 32M (4.0) */
		UTINY	physdrv;		/* Physical Drive No. (4.0) */
		UTINY	filler;			/* Reserved (4.0) */
		UTINY	xtbootsig;		/* Extended signt 29H if 4.0 
						 * stuf valid */
		ULONG	volid;			/* Unique number per volume 
						 * (4.0) */
		TEXT	vollabel[11];		/* Volume label (4.0) */
		UTINY   filler2[8];		/* Reserved (4.0) */
		};


#ifdef PROTO
#include <proto.h>
#endif

#endif
