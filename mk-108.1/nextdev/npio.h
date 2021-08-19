/*
 * Copyright (c) 1988 by NeXT, Inc.
 *
 * HISTORY
 *  8-Mar-88  Peter King (king) at NeXT, Inc.
 *	Created.
 */

/*
 * Structures and definitions for NeXT Laser Printer io control commands
 */

/* Paper types */
enum np_papersize { NOCASSETTE, A4, LETTER, B5, LEGAL };

/* Structure for NPIOCPOP - printer op command */
#define	NPIOCPOP	_IOWR('p', 1, struct npop) /* do a printer op */

struct npop {
	short	np_op;		/* operations defined below */
	union {
		int		npd_power; /* Power */
		unsigned char	npd_resolution; /* 300/400 DPI */
		struct {
			int	left; /* # of bits to indent on left */
			int	top;  /* # of lines from top of page */
				/*
				 * NOTE: less than a 200 line top margin
				 * 	is questionable.  Experiment.
				 */
			int	width; /* width in #'s of longwords  */
			int	height;	/* height in lines */
		}		npd_margins;
		struct np_stat {
			u_int	flags;
			u_int	retrans;
		}		npd_status;
		enum np_papersize	npd_size;
		boolean_t		npd_bool;
	} np_Data;
};
#define	np_power	np_Data.npd_power
#define	np_margins	np_Data.npd_margins
#define	np_resolution	np_Data.npd_resolution
#define	np_status	np_Data.npd_status
#define	np_size		np_Data.npd_size
#define	np_bool		np_Data.npd_bool

/* operations */
#define	NPSETPOWER	0	/* turn the printer on/off */
#define	NPSETMARGINS	1	/* Set the printer margins */
#define	NPSETRESOLUTION	2	/* Set the printer resolution */
#define	NPGETSTATUS	3	/* Get the printer status */
#define	NPCLEARRETRANS	4	/* Clear the retransmit counter */
#define	NPGETPAPERSIZE	5	/* Get the paper size */
#define	NPSETMANUALFEED	6	/* Set manual feed based on npop.np_bool */

/* resolutions */
#define	DPI300		0
#define	DPI400		1

/* Status bits */
#define	NPPAPERDELIVERY	0x0001	/* Paper is being processed in the printer */
#define	NPDATARETRANS	0x0002	/* Data should be retransmitted due to jam
				   or poor video signal.  Number of pages
				   is in npop.np_stat.retrans.  Clear this
				   with the NPCLEARRETRANS command. */
#define	NPCOLD		0x0004	/* Fixing assembly not yet hot enough */
#define	NPNOCARTRIDGE	0x0008	/* No cartridge in printer */
#define	NPNOPAPER	0x0010	/* No paper in printer */
#define	NPPAPERJAM	0x0020	/* Paper jam */
#define	NPDOOROPEN	0x0040	/* Door open */
#define	NPNOTONER	0x0080	/* Toner low */
#define	NPHARDWAREBAD	0x0100	/* Hardware failure - see other bad bits */
#define	NPMANUALFEED	0x0200	/* Manual feed selected */
#define	NPFUSERBAD	0x0400	/* Fixing assembly malfunction */
#define NPLASERBAD	0x0800	/* Poor Beam Detect signal */
#define NPMOTORBAD	0x1000	/* Scanning motor malfunction */
