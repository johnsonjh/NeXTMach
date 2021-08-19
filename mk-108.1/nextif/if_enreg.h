/*	@(#)if_enreg.h	1.0	11/17/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 17-Nov-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

/*
 * Register description for the Fujitsu Ethernet Data Link Controller (MB8795)
 * and the Fujitsu Manchester Encoder/Decoder (MB502).
 */

/* transmitter status (address 0) */
#define EN_TXSTAT_READY		0x80	/* ready for packet */
#define EN_TXSTAT_BUSY		0x40	/* receive carrier detect */
#define EN_TXSTAT_TXRECV	0x20	/* transmission received */
#define EN_TXSTAT_SHORTED	0x10	/* possible coax short */
#define EN_TXSTAT_UNDERFLOW	0x08	/* underflow on xmit */
#define EN_TXSTAT_COLLERR	0x04	/* collision detected */
#define EN_TXSTAT_COLLERR16	0x02	/* 16th collision error */
#define EN_TXSTAT_PARERR	0x01	/* parity error in tx data */
#define EN_TXSTAT_CLEAR		0xff	/* clear all status bits */

/* transmit masks (address 1) */
#define EN_TXMASK_READYIE	0x80	/* tx int on packet ready */
#define EN_TXMASK_TXRXIE	0x20	/* tx int on transmit rec'd */
#define EN_TXMASK_UNDERFLOWIE	0x08	/* tx int on underflow */
#define EN_TXMASK_COLLIE	0x04	/* tx int on collision */
#define EN_TXMASK_COLL16IE	0x02	/* tx int on 16th collision */
#define EN_TXMASK_PARERRIE	0x01	/* tx int on parity error */

/* cummulative receiver status (address 2) */
#define EN_RXSTAT_OK		0x80	/* packet received is correct */
#define EN_RXSTAT_RESET		0x10	/* reset packet received */
#define EN_RXSTAT_SHORT		0x08	/* < minimum length */
#define EN_RXSTAT_ALIGNERR	0x04	/* alignment error */
#define EN_RXSTAT_CRCERR	0x02	/* CRC error */
#define EN_RXSTAT_OVERFLOW	0x01	/* receiver FIFO overflow */
#define EN_RXSTAT_CLEAR		0xff	/* clear all status bits */

/* receiver masks (address 3) */
#define EN_RXMASK_OKIE		0x80	/* rx int on packet ok */
#define EN_RXMASK_RESETIE	0x10	/* rx int on reset packet */
#define EN_RXMASK_SHORTIE	0x08	/* rx int on short packet */
#define EN_RXMASK_ALIGNERRIE	0x04	/* rx int on align error */
#define EN_RXMASK_CRCERRIE	0x02	/* rx int on CRC error */
#define EN_RXMASK_OVERFLOWIE	0x01	/* rx int on overflow error */

/* transmitter mode (address 4) */
#define EN_TXMODE_COLLMASK	0xF0	/* collision count */
#define EN_TXMODE_PARIGNORE	0x08	/* ignore parity */
#define EN_TXMODE_LB_DISABLE	0x02	/* loop back disabled */
#define EN_TXMODE_DISCONTENT	0x01	/* disable contention (rx carrier) */

/* receiver mode (address 5) */
#define EN_RXMODE_TEST		0x80	/* Must be zero for normal op */
#define EN_RXMODE_ADDRSIZE	0x10	/* reduces NODE match to 5 chars */
#define EN_RXMODE_SHORTENABLE	0x08	/* rx packets >= 10 bytes */
#define EN_RXMODE_RESETENABLE	0x04	/* must be zero */
#define EN_RXMODE_PROMISCUOUS	0x03	/* accept all packets */
#define EN_RXMODE_MULTICAST	0x02	/* accept broad/multicasts */
#define EN_RXMODE_NORMAL	0x01	/* accept broad/limited multicasts */
#define EN_RXMODE_OFF		0x00	/* accept no packets */

/* reset mode (address 6) */
#define EN_RESET_MODE		0x80	/* reset mode */

struct en_regs {
	char	en_txstat;		/* tx status */
	char	en_txmask;		/* tx interrupt condition mask */
	char	en_rxstat;		/* rx status */
	char	en_rxmask;		/* rx interrupt condition mask */
	char	en_txmode;		/* tx control/mode register */
	char	en_rxmode;		/* rx control/mode register */
	char	en_reset;		/* reset mode */
	char	en_fill;		/* pad to 8 bytes */
	char	en_addr[6];		/* physical address */
};

#define	ENRX_EOP	0x80000000	/* end-of-packet flag */
#define	ENRX_BOP	0x40000000	/* beginning-of-packet flag */
#define ENTX_EOP	0x80000000	/* end-of-packet flag */
