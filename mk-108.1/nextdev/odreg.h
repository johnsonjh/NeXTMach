/*	@(#)odreg.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

struct od_regs {
	u_char	r_track_h, r_track_l;		/* track number (rw) */
	u_char	r_incr_sect;		/* sector incr (w) & sector # (rw) */
	u_char	r_seccnt;		/* sector count (rw) */
	u_char	r_disr;			/* disk interrupt status (r, rw) */
#define	OMD_DATA_ERR	0x80
#define	OMD_PARITY_ERR	0x40
#define	OMD_READ_FAULT	0x20
#define	OMD_TIMEOUT	0x10
#define	OMD_ECC_DONE	0x08
#define	OMD_OPER_COMPL	0x04
#define	OMD_ATTN	0x02		/* attention (ro) */
#define	OMD_CMD_COMPL	0x01		/* command complete (ro) */
#define	OMD_SDFDA	0x02		/* SCSI/floppy acesss 1 = floppy;
					 *   0 = SCSI (wo) */
#define	OMD_RESET	0x01		/* drive hard reset (wo) */
#define	OMD_CLR_INT	0xfc		/* clear interrupts except ATTN */
#define	OMD_ENA_INT	0xf2		/* error interrupts incl ATTN */
	u_char	r_dimr;			/* disk interrupt mask reg (rw) */
	u_char	r_cntrl2;		/* controller csr #2 (rw) */
#define	OMD_SECT_TIMER	0x80
#define	OMD_ECC_DIS	0x40
#define	OMD_ECC_MODE	0x20
#define	OMD_ECC_BLOCKS	0x10
#define	OMD_CLR_BUFP	0x08
#define	OMD_BUF_TOGGLE	0x04
#define	OMD_ECC_CMP	0x02
#define	OMD_DRIVE_SEL	0x01
	u_char	r_cntrl1;		/* controller csr #1 (rw) */
					/* see odvar.h for commands */
	u_char	r_csr_h, r_csr_l;	/* drive csr (rw) */
					/* see od_var.h for commands */
	u_char	r_desr;			/* data error status (r) */
#define	OMD_ERR_STARVE	0x08
#define	OMD_ERR_TIMING	0x04
#define	OMD_ERR_CMP	0x02
#define	OMD_ERR_ECC	0x01
	u_char	r_ecccnt;		/* error correction count (r) */
	u_char	r_initr;		/* initialization reg (w) */
#define	OMD_SEC_GREATER	0x80		/* sector > enable */
#define	OMD_ECC_STV_DIS	0x40		/* ECC starve disable */
#define	OMD_ID_CMP_TRK	0x20		/* ID cmp on track, not sector */
#define	OMD_25_MHZ	0x10		/* 25 MHz ECC clk for 3600 RPM */
#define	OMD_DMA_STV_ENA	0x08		/* DMA starve enable */
#define	OMD_EVEN_PAR	0x04		/* diag: generate bad parity */
#define	OMD_ID_34	0		/* how many IDs must match */
#define	OMD_ID_234	1
#define	OMD_ID_1234	3
#define	OMD_ID_0	2
#define	OMD_ID_MASK	3
	u_char	r_frmr;			/* format reg (w) */
#define	OMD_WRITE_GATE_NOM	0x30
#define	OMD_READ_GATE_NOM	0x06
#define	OMD_READ_GATE_MIN	0x00
#define	OMD_READ_GATE_MAX	0x0f
#define	OMD_READ_GATE_MASK	0x0f
	u_char	r_dmark;		/* data mark (w) */
	u_char	r_spare;
#define	OMD_NFLAG	7		/* number of flag strategy nibbles */
	u_char	r_flgstr[OMD_NFLAG];	/* flag strategy (rw) */
};

/* OMD1_RJ: head select arg */
#define	RJ_READ		0x10
#define	RJ_VERIFY	0x20
#define	RJ_WRITE	0x30
#define	RJ_ERASE	0x40

