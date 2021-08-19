/*	@(#)dma.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 *
 * 12-Aug-87 Mike DeMoney (mike) at NeXT
 *	Created
 *
 **********************************************************************
 */ 

/*
 * dma.h -- description of NeXT dma structures
 */
#define	MAX_DMA_LEN	(64*1024)	/* max DMA transfer size per chain */

#define	DMA_BEGINALIGNMENT	4	/* initial buffer must be on long */
#define	DMA_ENDALIGNMENT	16	/* DMA must end on quad longword */

#define	DMA_ALIGN(type, addr)	\
	((type)(((unsigned)(addr)+DMA_BEGINALIGNMENT-1) \
		&~(DMA_BEGINALIGNMENT-1)))

#define	DMA_BEGINALIGNED(addr)	(((unsigned)(addr)&(DMA_BEGINALIGNMENT-1))==0)
#define	DMA_ENDALIGNED(addr)	(((unsigned)(addr)&(DMA_ENDALIGNMENT-1))==0)

#if	!ASSEMBLER
struct dma_hdr {		/* header that represents buffers to dma */
	struct dma_hdr *dh_link;	/* pts to next dma_hdr in chain */
	char *dh_start;			/* pts to start of buffer for dma */
	char *dh_stop;			/* pts to end of buffer + 1 for dma */
	int dh_state;			/* csr at time of dma interrupt */
	int dh_drvarg;			/* for use by driver */
};

/*
 * lists of dma_hdr's used by dma_list()
 * +2 => 1 for initial page portion, 1 for trailing quad longword hack
 */
#define	NDMAHDR	3
typedef	struct	dma_hdr	dma_list_t[NDMAHDR];

struct dma_queue {		/* header of queue of buffers */
	struct dma_hdr *dq_head;	/* pts to head of dma_hdr queue */
	struct dma_hdr *dq_tail;	/* pts to tail of dma_hdr queue */
};

struct dma_dev {		/* format of dma device registers */
	char dd_csr;			/* control & status register */
	char dd_pad[0x3fff];		/* csr not contiguous with next */
	char *dd_next;			/* next word to dma */
	char *dd_limit;			/* dma complete when next == limit */
	char *dd_start;			/* start of 2nd buf to dma */
	char *dd_stop;			/* end of 2nd buf to dma */
};
#endif	!ASSEMBLER

/*
 * bits in dd_csr
 */
/* read bits */
#define	DMACSR_ENABLE		0x01	/* enable dma transfer */
#define	DMACSR_SUPDATE		0x02	/* single update */
#define	DMACSR_COMPLETE		0x08	/* current dma has completed */
#define	DMACSR_BUSEXC		0x10	/* bus exception occurred */
/* write bits */
#define	DMACSR_SETENABLE	0x01 	/* set enable */
#define	DMACSR_SETSUPDATE	0x02	/* set single update */
#define	DMACSR_READ		0x04	/* dma from dev to mem */
#define	DMACSR_WRITE		0x00	/* dma from mem to dev */
#define	DMACSR_CLRCOMPLETE	0x08	/* clear complete conditional */
#define	DMACSR_RESET		0x10	/* clr cmplt, sup, enable */

/*
 * dma "commands"
 * (composites of csr bits)
 */
#define	DMACMD_RESET		(DMACSR_RESET)
#define	DMACMD_WRAPUP		(DMACSR_CLRCOMPLETE)
#define	DMACMD_CHAIN		(DMACSR_SETSUPDATE | DMACSR_CLRCOMPLETE)
#define	DMACMD_START		(DMACSR_SETENABLE)
#define	DMACMD_STARTCHAIN	(DMACSR_SETENABLE | DMACSR_SETSUPDATE)

/*
 * dma "states"
 */
#define	DMASTATE_IDLE		0
#define	DMASTATE_SINGLE		(DMACSR_ENABLE)
#define	DMASTATE_CHAINING	(DMACSR_ENABLE | DMACSR_SUPDATE)
#define	DMASTATE_1STDONE	(DMACSR_COMPLETE | DMACSR_ENABLE)
#define	DMASTATE_DONE		(DMACSR_COMPLETE)
#define	DMASTATE_ERR1		(DMACSR_BUSEXC | DMACSR_COMPLETE \
				   | DMACSR_SUPDATE | DMACSR_ENABLE)
#define	DMASTATE_ERR2		(DMACSR_BUSEXC | DMACSR_COMPLETE)
#define	DMASTATE_XDONE		(DMACSR_COMPLETE | DMACSR_SUPDATE)
#define	DMASTATE_XERR2		(DMACSR_BUSEXC | DMACSR_COMPLETE \
				   | DMACSR_SUPDATE)

/*
 * masks to extract state from csr
 */
#define	DMASTATE_MASK		(DMACSR_BUSEXC | DMACSR_COMPLETE \
				   | DMACSR_SUPDATE | DMACSR_ENABLE)
#define	DMAIDLE_MASK		(DMACSR_COMPLETE | DMACSR_SUPDATE \
				   | DMACSR_ENABLE)

#define	DMA_MAXTAIL		128

#if	!ASSEMBLER
struct dma_chan {		/* information for each dma channel */
	struct dma_queue dc_queue;	/* queue of buffers to dma */
	struct dma_hdr *dc_current;	/* pts to buf in next/limit regs */
	void (*dc_handler)();		/* routine to handle dma cmpl & errs */
	int dc_hndlrarg;		/* passed to handler on cmpl intr */
	struct dma_dev *dc_ddp;		/* pts to dma device registers */
	int dc_direction;		/* standard csr bits */
	int dc_state;			/* terminating state */
	char *dc_next;			/* dd_next if error or abort */
	int dc_flags;			/* see below */
	char *dc_tailstart;		/* phys addr for start of tail */
	long dc_taillen;		/* length of tail segment */
	char *dc_tail;			/* pts to quad aligned tail buf */
	char dc_tailbuf[DMA_MAXTAIL+2*DMA_ENDALIGNMENT]; /* tail buffer */
};
#endif !ASSEMBLER

/*
 * channel software flags in dc_flags
 */
/* dma "option" flags */
#define	DMACHAN_INTR		0x0001	/* interrupt at end of chain */
#define	DMACHAN_PAD		0x0002	/* dma_list should pad for flushes */
#define	DMACHAN_AUTOSTART	0x0004	/* dma whenever anything on queue */
/* dma state flags */
#define	DMACHAN_BUSY		0x0008	/* dma channel is active */
#define	DMACHAN_CHAINING	0x0010	/* channel is chaining */
#define	DMACHAN_ERROR		0x0020	/* dma channel in error state */

extern void dma_abort();
extern void dma_init();
extern void dma_list();
extern void dma_cleanup();
extern void dma_start();

/*
 * dma_enable -- short-hand to enable dma once a queue has
 *	been built
 */
#define	dma_enable(dcp, direction) \
	dma_start(dcp, (dcp)->dc_queue.dq_head, direction)
