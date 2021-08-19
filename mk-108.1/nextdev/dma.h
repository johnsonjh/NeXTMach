/*	@(#)dma.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 *
 * 26-Sep-89  John Seamons (jks) at NeXT
 *	Made dd_csr in struct dma_dev an int (and changed the corresponding
 *	register #defines) because this register responds as a 32-bit device
 *	and therefore needs to be accessed this way to work on the 68040.
 *
 * 12-Apr-88  John Seamons (jks) at NeXT
 *	Changes to support DMA chip 313.
 *
 * 12-Aug-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#ifndef	__DMA__
#define	__DMA__
#import <vm/vm_param.h>
#ifdef	KERNEL
#import <kern/xpr.h>
#endif	KERNEL

/*
 * dma.h -- description of NeXT dma structures
 */
#define	MAX_DMA_LEN	(64*1024)	/* max DMA transfer size per chain */

#define	DMA_BEGINALIGNMENT	4	/* initial buffer must be on long */
#define	DMA_ENDALIGNMENT	16	/* DMA must end on quad longword */

#define	DMA_ALIGN(type, addr)	\
	((type)(((unsigned)(addr)+DMA_BEGINALIGNMENT-1) \
		&~(DMA_BEGINALIGNMENT-1)))

#define	DMA_ENDALIGN(type, addr)	\
	((type)(((unsigned)(addr)+DMA_ENDALIGNMENT-1) \
		&~(DMA_ENDALIGNMENT-1)))
#define	DMA_BEGINALIGNED(addr)	(((unsigned)(addr)&(DMA_BEGINALIGNMENT-1))==0)
#define	DMA_ENDALIGNED(addr)	(((unsigned)(addr)&(DMA_ENDALIGNMENT-1))==0)

#if	!ASSEMBLER
struct dma_hdr {		/* header that represents buffers to dma */
	struct dma_hdr *volatile dh_link;/* pts to next dma_hdr in chain */
	char *dh_start;			/* pts to start of buffer for dma */
	char *dh_stop;			/* pts to end of buffer + 1 for dma */
	volatile int dh_flags;		/* see below */
	volatile int dh_state;		/* csr at time of dma interrupt */
	char *volatile dh_next;		/* next at time of dma interrupt */
	int dh_drvarg;			/* for use by driver */
#if	CHAIN_READ
	int dh_count;
#endif	CHAIN_READ
};

/*
 * Flags for per-dma_hdr actions
 * (Might someday want to do flag for ENETR soft ints with these)
 */
#define	DMADH_INITBUF	0x0001		/* initialize internal DMA buffer */
#define	DMADH_SECTCHAIN	0x0002		/* call routine on sector chain */

/*
 * lists of dma_hdr's used by dma_list()
 * +2 => 1 for initial page portion, 1 for trailing quad longword hack
 */
#define	NDMAHDR	(howmany(MAX_DMA_LEN, NeXT_MIN_PAGE_SIZE)+2)
typedef	struct	dma_hdr	dma_list_t[NDMAHDR];

struct dma_queue {		/* header of queue of buffers */
	struct dma_hdr *volatile dq_head;/* pts to head of dma_hdr queue */
	struct dma_hdr *volatile dq_tail;/* pts to tail of dma_hdr queue */
};

struct dma_dev {		/* format of dma device registers */
	int dd_csr;		/* control & status register */
	char dd_pad[0x3fec];	/* csr not contiguous with next */
	char *dd_saved_next;	/* saved pointers for HW restart */
	char *dd_saved_limit;
	char *dd_saved_start;
	char *dd_saved_stop;
	char *dd_next;		/* next word to dma */
	char *dd_limit;		/* dma complete when next == limit */
	char *dd_start;		/* start of 2nd buf to dma */
	char *dd_stop;		/* end of 2nd buf to dma */
	char dd_pad2[0x1f0];
	char *dd_next_initbuf;	/* next register that inits dma buffering */
};

/* writes to DMA scratchpad may not always work -- verify all writes */
#define	DMA_W(reg, val) \
	if (((int)(val) & 0x0fffffff) < P_MAINMEM || \
	    ((int)(val) & 0x0fffffff) >= (P_MAINMEM + P_MEMSIZE)) \
		panic ("TBG in DMA"); \
	do { \
		(reg) = (val); \
	} while ((reg) != (val));
#endif	!ASSEMBLER

/*
 * bits in dd_csr
 */
/* read bits */
#define	DMACSR_ENABLE		0x01000000	/* enable dma transfer */
#define	DMACSR_SUPDATE		0x02000000	/* single update */
#define	DMACSR_COMPLETE		0x08000000	/* current dma has completed */
#define	DMACSR_BUSEXC		0x10000000	/* bus exception occurred */
/* write bits */
#define	DMACSR_SETENABLE	0x00010000 	/* set enable */
#define	DMACSR_SETSUPDATE	0x00020000	/* set single update */
#define	DMACSR_READ		0x00040000	/* dma from dev to mem */
#define	DMACSR_WRITE		0x00000000	/* dma from mem to dev */
#define	DMACSR_CLRCOMPLETE	0x00080000	/* clear complete conditional */
#define	DMACSR_RESET		0x00100000	/* clr cmplt, sup, enable */
#define	DMACSR_INITBUF		0x00200000	/* initialize DMA buffers */

/*
 * dma "commands"
 * (composites of csr bits)
 */
#define	DMACMD_RESET		(DMACSR_RESET)
#define	DMACMD_INITBUF		(DMACSR_INITBUF)
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
	struct dma_hdr *volatile dc_current;/* pts to buf in next/limit regs */
	void (*dc_dmaintr)();		/* routine to call at interrupt level */
	void (*dc_handler)();		/* routine to handle dma cmpl & errs */
	int dc_hndlrarg;		/* passed to handler on cmpl intr */
	int dc_hndlrpri;		/* callout pri to sched handler at */
	volatile struct dma_dev *dc_ddp;/* pts to dma device registers */
	int dc_direction;		/* standard csr bits */
	volatile int dc_state;		/* terminating state */
	char *volatile dc_next;		/* dd_next if error or abort */
	volatile int dc_flags;		/* see below */
	char *dc_tailstart;		/* phys addr for start of tail */
	long dc_taillen;		/* length of tail segment */
	char *dc_tail;			/* pts to quad aligned tail buf */
	vm_offset_t dc_ptail;		/* phys addr for tail buf */
	char dc_tailbuf[DMA_MAXTAIL+2*DMA_ENDALIGNMENT]; /* tail buffer */
	void (*dc_sectchain)();		/* routine to call on sector chain */
};

/*
 * channel software flags in dc_flags
 */

/* dma "option" flags */
#define	DMACHAN_INTR		0x0001	/* interrupt at end of chain */
#define	DMACHAN_PAD		0x0002	/* dma_list should pad for flushes */
#define	DMACHAN_AUTOSTART	0x0004	/* dma whenever anything on queue */
#define	DMACHAN_ENETR		0x0008	/* Ethernet receive processing */
#define	DMACHAN_ENETX		0x0010	/* Ethernet transmit processing */
#define	DMACHAN_DMAINTR		0x0020	/* call routine at interrupt level */
#if	XPR_DEBUG
#define DMACHAN_DBG		0x0040	/* Debug printing on this channel */
#endif	XPR_DEBUG
#define	DMACHAN_CHAININTR	0x0080	/* interrupt on every chain */
#define	DMACHAN_SECTCHAIN	0x0100	/* call routine on sector chain */

/* dma state flags */
#define	DMACHAN_BUSY		0x1000	/* dma channel is active */
#define	DMACHAN_CHAINING	0x2000	/* channel is chaining */
#define	DMACHAN_ERROR		0x4000	/* dma channel in error state */

/*
 * constants for use with dma_dequeue "doneflag" argument
 */
#define	DMADQ_DONE		0	/* only dequeue hdrs with dma done */
#define	DMADQ_ALL		1	/* dequeue all dma_hdrs */

/*
 * dma_enable -- short-hand to enable dma once a queue has
 *	been built
 */
#define	dma_enable(dcp, direction) \
	dma_start(dcp, (dcp)->dc_queue.dq_head, direction)

/*
 * dma support routine entry points
 */
extern void dma_init(/* dcp */);
extern void dma_enqueue(/* dcp, dhp */);
extern struct dma_hdr *dma_dequeue(/* dcp */);
extern void dma_start(/* dcp, dhp, direction */);
extern void dma_abort(/* dcp */);
extern void dma_intr(/* dcp */);
extern void dma_list(/* dcp, vaddr, bcount, pmap_t */);
extern void dma_cleanup(/* dcp, resid */);
#endif	!ASSEMBLER

#endif	__DMA__

