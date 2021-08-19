/* 
 * HISTORY
 *
 * 20-Apr-89  John Seamons (jks) at NeXT
 *	Created.
 */

/* 
 *  Format of Canon control track and PLL defect sector table.
 */
#define	CANON_TABSIZE	1024

struct sector_addr {
	u_char	sa_track[3];
	u_char	sa_sector;
};

/*
 *  Canon spec says 0x4d60 & 16 for these, but they produce ECC errors
 *  when read.  Track numbers are absolute, i.e. must subtract the
 *  number of reserved tracks to convert to drivers concept of
 *  physical track number.  Tables are 1 sector long, duplicated
 *  over the entire track.
 */
#define	CONTROL_TRACK		0x4d6a
#define	CONTROL_NTRACKS		6

struct canon_control {
	u_char	cc_id_code[2];
#define	CC_ID_CODE	0xff01
	char	cc_media_type[16];
	char	cc_lot[16];
	char	cc_serial[16];
	char	cc_prod_date[8];
	char	cc_life_date[8];
	u_char	cc_side[2];
#define	CC_SIDE_A	0x0000
#define	CC_SIDE_B	0x0001
	/* more info here ... */
};

#define	DEFECT_TRACK	0x4d50
#define	DEFECT_NTRACKS	4

struct canon_defect {
	u_char	cd_id_code[2];
#define	CD_ID_CODE	0xfe02
	u_char	cd_defect_sect_num[2];
#define	NDEFECTS	255
	u_char	cd_defects[NDEFECTS][4];	/* struct sector_addr */
};

/* 
 *  Format of NeXT defect sector table.  Occupies an entire track
 *  and is repeated 16 times.
 */
#define	NeXT_TABSIZE		(16*1024)
#define	NeXT_DEFECT_TRACK	0x4c90
#define	NeXT_DEFECT_NTRACKS	16

struct NeXT_defect {
	u_char	nd_id_code[4];
#define	ND_ID_CODE	0x4e655854		/* "NeXT" */
#define	NeXT_NDEFECTS	4095
	u_char	nd_defects[NeXT_NDEFECTS][4];	/* struct sector_addr */
};

