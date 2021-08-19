/*	@(#)bmap.h	1.0	3/7/90	(c) 1990 NeXT	*/

/* 
 * HISTORY
 * 12-Mar-90  John Seamons (jks) at NeXT
 *	Added more register bit definitions.
 *
 *  7-Mar-90  John Seamons (jks) at NeXT
 *	Created.
 */ 

#ifndef	ASSEMBLER
struct bmap_chip {
	u_int	bm_sid;			/* 0x00: slot ID register for BMAP */
	u_int	bm_rml : 1,		/* 0x04: ROM control register */
		bm_lo : 1,
		: 1,
		bm_rse : 1,
		bm_rmt : 4,
		: 24;
	u_int	bm_paren : 1,		/* 0x08: bus error register */
		bm_parck : 1,
		bm_berr : 1,
		bm_ovrun : 1,
		bm_tt : 2,
		bm_siz : 2,
		bm_rd : 1,
		bm_busto : 1,
		bm_halt : 1,
		bm_cback : 1,
		bm_aenc : 2,
		bm_burst : 1,
		bm_wait : 1,
		: 16;
	u_int	bm_bwe : 1,		/* 0x0c: burst write / DSP int control reg */
		bm_bwtae : 1,
		bm_dsp_hreq_int : 1,
		bm_dsp_txd_int : 1,
		: 28;
	u_int	bm_tstatus[2];		/* 0x10,0x14: timing status register */
	u_int	bm_ast;			/* 0x18: address strobe timing register */
	u_int	bm_asen : 1,		/* 0x1c: address strobe enable register */
		bm_tc : 2,
		bm_scyc : 1,
		: 28;
	u_int	bm_ser;			/* 0x20: sample start register */
	u_int	bm_spare[3];		/* 0x24, 0x28, 0x2c */
	u_int	bm_ddir;		/* 0x30: GPIO data direction register */
	u_int	bm_drw;			/* 0x34: GPIO data read/write register */
	u_int	bm_dma_mask : 4,	/* 0x38: arbitration mask register */
		bm_nbic_mask : 4,
		: 24;
};

volatile struct bmap_chip *bmap_chip;
#endif	ASSEMBLER

/* BMAP registers */
#define BM_SID		0x00
#define BM_RCNTL	0x04
#define BM_BUSERR	0x08
#define BM_BURWREN	0x0c
#define BM_TSTATUS1	0x10
#define BM_TSTATUS0	0x14
#define BM_ASCNTL	0x18
#define BM_ASEN		0x1c
#define BM_STSAMPLE	0x20
#define BM_DDIR		0x30
#define BM_DRW		0x34
#define	BM_AMR		0x38

/* BMAP register bits */
#define	BMAP_RML	0x80000000
#define	BMAP_LO		0x40000000
#define	BMAP_RSE	0x10000000
#define	BMAP_PAREN	0x80000000
#define	BMAP_ASEN	0x80000000
#define	BMAP_TC_AS	0x00000000
#define	BMAP_TC_AST	0x20000000
#define	BMAP_TC_CLK	0x40000000
#define	BMAP_TC_AD31	0x60000000
#define	BMAP_SCYC	0x10000000
#define	BMAP_A31	0x80000000
#define	BMAP_BWE	0x80000000
#define	BMAP_TPE_RXSEL	0x80000000
#define	BMAP_RESET	0x40000000
#define	BMAP_HEARTBEAT	0x20000000
#define	BMAP_TPE_ILBC	0x10000000
#define	BMAP_TPE	(BMAP_TPE_RXSEL | BMAP_TPE_ILBC)
