/*	@(#)monreg.h	1.0	10/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 07-Jul-89	Doug Mitchell at NeXT
 *	Added KB_POLL_SETUP.
 * 12-Oct-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */

#ifndef	ASSEMBLER
struct monitor {
	struct mon_status {
		u_int	dmaout_dmaen : 1,
			dmaout_dav : 1,
			dmaout_ovr : 1,
			: 1,
			dmain_dmaen : 1,
			dmain_dav : 1,
			dmain_ovr : 1,
			: 1,
			km_int : 1,
			km_dav : 1,
			km_ovr : 1,
			km_clr_nmi : 1,
			control_int : 1,
			control_dav : 1,
			control_ovr : 1,
			: 1,
			dtx_pend : 1,
			dtx : 1,
			ctx_pend : 1,
			ctx : 1,
			rtx_pend : 1,
			rtx : 1,
			reset : 1,
			txloop : 1,
			cmd : 8;
	} mon_csr;
	int	mon_data;
	int	mon_km_data;
	int	mon_sound_data;
};
#endif	ASSEMBLER

#define	MON_SOUND_OUT		0xc7
#define	MON_KM_POLL		0xc6
#define	MON_KM_USRREQ		0xc5
#define	MON_GP_OUT		0xc4
#define	MON_GP_IN		0x04
#define	MON_SNDOUT_CTRL(opt)	(0x07 | (opt << 3))
#define	MON_SNDIN_CTRL(opt)	(0x03 | (opt << 3))

/* 
 * keyboard polling setup
 */
/*#define KB_POLL_SETUP	0x01fffff6  	/* 2 devices, nops for other 5 */
#define KB_POLL_SETUP 0x01fffff1	/* 2 devices, maximum speed */

#define	MON_NO_RESP		0x40000000
#define	MON_USRREQ		0x20000000
#define	MON_MASTER		0x10000000
#define	MON_DEV_ADRS(data)	(((data) >> 24) & 0xf)

#define	DMAOUT_DMAEN	0x80000000
#define DMAOUT_DAV	0x40000000
#define DMAOUT_OVR	0x20000000

#define DMAIN_DMAEN	0x08000000
#define DMAIN_DAV	0x04000000
#define DMAIN_OVR	0x02000000

#define KM_INT		0x00800000
#define KM_DAV		0x00400000
#define KM_OVR		0x00200000
#define KM_CLR_NMI	0x00100000

#define CONTROL_INT	0x00080000
#define CONTROL_DAV	0x00040000
#define CONTROL_OVR	0x00020000

#define	DTX_PEND	0x00008000
#define DTX		0x00004000
#define CTX_PEND	0x00002000
#define CTX		0x00001000

#define RTX_PEND	0x00000800
#define RTX		0x00000400
#define RESET		0x00000200
#define TXLOOP		0x00000100
