/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 * 29-Aug-90  Julius Smith (julius) at NeXT
 *	Added support for packed-mode reads and writes.
 *
 * 21-Aug-90  Julius Smith (jos) at NeXT
 *	Added protocol message type for setting the protocol from the
 *	dsp command queue.
 *
 *  7-Jul-90  Matt Christiano (mchristo) and Julius Smith (jos) at NeXT
 *	Revised DMA state sequence.
 *
 *  7-Jun-90  Matt Christiano (mchristo) at NeXT
 *	Removed DSP_hm_* defines.
 *	Added DSP_hc_SYSCALL and DSP_SYSCALL_* defines.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _SND_DSP_
#define _SND_DSP_

#import <nextdev/snd_var.h>

#define spldsp() spl4()

/*
 * DSP internal states
 */
enum dsp_state {
	normal,
	penddmain1,
	dmain,
	postdmain,
	penddmaout0,
	penddmaout1,
	dmaout,
	dsp_aborted
};

/*
 * structure describing data send on message port
 */
typedef struct dsp_msg {
	enum {
		condition,			// conj. of flags to await
		data1,				// 1 byte data for xmit regs
		data2,				// 2 byte data for xmit regs
		data3,				// 3 byte data for xmit regs
		data4,				// 4 byte data for xmit regs
		dma_out,			// xfer to dsp
		dma_in,				// xfer from dsp
		host_command,			// host command for cvr
		host_flag,			// state to set HF[01] to
		ret_msg,			// message to return to owner
		reset,				// reset the DSP
		ret_regs,			// return dsp registers
		driver_state,			// set internal state
		rdata1,				// 1 byte data from recv regs
		rdata2,				// 2 byte data from recv regs
		rdata3,				// 3 byte data from recv regs
		rdata4,				// 4 byte data from recv regs
		protocol			// set DSP protocol
	} type;
	union	{
		struct {
			u_int	mask;		// mask of valid flags
			u_int	flags;		// expected state of flags
			port_name_t
				reply_port;	// where to send regs to
		} condition;
		struct {
			vm_address_t	addr;	// address of data to read/write
			vm_size_t	size;	// # bytes to write
			vm_address_t	loc;	// pointer to unused portion
			port_name_t
				reply_port;	// where to send read response
		} data;
		struct {
			u_int		addr;	// .. of dsp buffer
			u_short		size;	// .. of dsp buffer
			u_short		skip;	// dma skip factor
			u_char		space;	// dsp space of buffer
			u_char		mode;	// mode of dma [1..5]
			u_char		chan;	// channel for dma
		} dma;
		u_int	host_command;		// host command to execute
		struct {
			u_int	mask;		// mask of flags to change
			u_int	flags;		// value to set flags to.
#define DSP_MSG_FLAGS_MASK (((ICR_INIT|ICR_HM1|ICR_HM0|ICR_HF0|ICR_HF1)<<24) \
			    |((CVR_HC|CVR_HV)<<16))  // mask of settable bits
		} host_flag;
		msg_header_t	*ret_msg;	// message to send
		enum dsp_state	driver_state;	// driver state change
		u_int	new_protocol;		// arg to dsp_dev_new_proto()
	} u;
	u_int		scratch;		// for ancillary uses.
	queue_chain_t	link;			// next/previous message
	u_char		priority;		// priority of this msg
	u_char		sequence;		// first/last/middle in msg
	u_char		atomic;			// don't preempt
#define DSP_MSG_ONLY		0		// only part of message
#define DSP_MSG_FIRST		1		// first part of message
#define DSP_MSG_LAST		2		// middle of message
#define DSP_MSG_MIDDLE		3		// end of message
#define DSP_MSG_STARTED		4		// message in progress.
	u_char		datafollows;		// data immediately follows us
	u_char		internal;		// msg from driver, ret there
} dsp_msg_t;

#define DSP_NLMSG	20		// # msgs needed locally
#define DSP_LMSG_SIZE	(sizeof(dsp_msg_t) + 32*sizeof(int))

/*
 * DSP device structure.  Keeps track of dma and message activity.
 */
#define DSP_N_CHAN	19	// 16 channels + sin + sout + task_port

typedef struct dsp_var {
	struct dma_chan		dma_chan;
	struct	chan_data {
		snd_queue_t sndq;	// region management
		queue_head_t ddp_q;	// queue of dma descs
		u_int	flags;
#define	DSP_C_D_ACTIVE	1
		u_int		base_addr;	// .. of dsp buffer
		u_short		buf_size;	// .. of dsp buffer
		u_short		skip;		// dma skip factor
		u_char		space;		// dsp space of buffer
		u_char		mode;		// mode of dma [1..5]
	} *chan[DSP_N_CHAN];			// 1 per chan
	u_short	pend_dma_chan;			// pending dma
	u_int	pend_dma_addr;			// address of pending dma
	queue_head_t	cmd_q;			// queue of DSP commands
	int		cmd_q_size;		// # messages enqueued
#define DSP_CMD_Q_MAX	512			// max # messages enqueued
	u_int		*msgbuf;		// buffer for dsp messages
	u_int		*emsgbuf;		// word past end of msgbuff
	u_int		*msgp;			// current location in msgbuf
	u_int		*errbuf;		// buffer for dsp err msgs
	u_int		*eerrbuf;		// word past end of errbuf
	u_int		*errp;			// current location in errbuf
	u_int		event_mask;		// for sending async event
	u_int		event_condition;	// true flags
	dsp_msg_t	*event_msg;		// message to send
	volatile
	enum dsp_state	state;			// driver state.
	u_short ui_dma_read_state;		// User-initiated read state
#define UIR_PENDING	0x0001			/* UI dma read pending */
#define UIR_INITED	0x0002			/* UI dma init has been done*/
	port_name_t	msg_port;		// where (if) to send messages
	port_name_t	err_port;		// where (if) to send errors
	volatile u_int	flags;
#define F_AWAITHC	0x00001			// waiting to xmit host command
#define F_AWAITRXDF	0x00002			// waiting to receive data
#define F_AWAITTXDE	0x00004			// waiting to xmit data
#define F_AWAITBIT	0x00008			// waiting for DSP HF2 or HF3 

#define F_LINKEDOUT	0x00010			// dsp linked to sound-out
#define F_LINKEDIN	0x00020			// dsp linked to sound-in
#define F_NOQUEUE	0x00040			// don't process command queue

// interface modes
#define F_MODE_RAW	0x00100			// raw mode (for direct read)
#define F_MODE_HFABORT	0x00200			// enable DSP abort on HF2&HF3
#define F_MODE_DSPMSG	0x00400			// DSP messages enabled
#define F_MODE_DSPERR	0x00800			// DSP errors AND msgs enabled
#define F_MODE_S_DMA	0x01000			// simple dma mode
#define F_MODE_C_DMA	0x02000			// complex dma mode
#define F_MODE_SND_HIGH	0x04000			// 44khz sound out.

#define F_SOUNDDATA	0x10000			// all data to dsp is sound
  /*** FIXME - The F_SOUNDDATA bit was used to force programmed writes to
    the DSP instead of DMA for 1.0 (because there was no time to get DMA
    debugged in the driver).  Thus, it should either do something intelligent
    or be deleted!  (It was not documented and was used privately by the
    libsys sound-library snddriver_* functions.) ***/


#define F_SHUTDOWN	0x20000			// device shutting down
#define F_NOCMDPORT	0x40000			// dsp_cmd port not in set

} dsp_var_t;

/* DSP Host Commands */
#define DSP_hc_HOST_RD		(0x24>>1)	/* DMA read done */
#define DSP_hc_XHM		(0x26>>1)	/* Host message (generic) */
#define DSP_hc_HOST_WD		(0x28>>1)	/* DMA write done */
#define DSP_hc_MASK		0x1f

/*
 *	New SYSCALL host command for V2
 */

#define DSP_hc_SYSCALL		(0x2c>>1)	/* 
/*
 *	Parameter values for SYSCALL
 */
#define DSP_SYSCALL_READ	(0x1<<16)
#define DSP_SYSCALL_WRITE	(0x2<<16)
#define DSP_SYSCALL_m_CHANMASK	0x1f		/* 5 bits of channel info */
#define DSP_SYSCALL_m_SWFIX	0x8000		/* DMA write fix for chip <13 */

/* Special return opcode from dsp, to be ignored. */
#define DSP_OPCODEMASK		0xff0000
#define DSP_OPERROR		0x800000
#define DSP_DATAMASK		0x00ffff
#define DSP_CHANMASK		0x007fff	/* For rd/wr requests */
#define DSP_PARITYMASK		0x008000	/* odd or even buffer */
#define DSP_dm_KERNEL_ACK	0x010000
#define DSP_dm_W_REQ		0x040000
#define DSP_dm_R_REQ		0x050000
#define DSP_dm_TMQ_LWM		0x0a0000       /* "LWM crossed" message */

/* DMA constants. */
#define mode_size(mode) ((mode) == DSP_MODE2416 ? DSP_MODE16 : (mode))

/*
 * Routine prototypes
 */
#if	KERNEL
void dsp_dev_init(void);
void dsp_dev_reset(void);
void dsp_dev_reset_chip(void);
void dsp_dev_reset_hard(void);
void dsp_dev_new_proto(int new_proto);
void dsp_dev_loop(void);
void dspq_enqueue(queue_t q);
void dspq_reset_lmsg(void);
void dspq_enqueue_hc(int host_command);
void dspq_enqueue_hm(int host_message);
void dspq_enqueue_hm1(int host_message, int arg);
void dspq_enqueue_hm3(int host_message, int a1, int a2, int a3);
void dspq_enqueue_hf(int mask, int flags);
void dspq_enqueue_cond(int mask, int flags);
void dspq_enqueue_state(enum dsp_state state);
boolean_t dspq_check(void);
void dspq_execute(void);
void dspq_free_msg(dsp_msg_t *msg);
int dspq_awaited_conditions(void);
boolean_t dspq_start_simple(snd_dma_desc_t *ddp, int direction, int rate);
boolean_t dspq_start_complex(snd_dma_desc_t *ddp, int direction, int rate);
int snd_dspcmd_def_dmasize(int chan);
int snd_dspcmd_def_high_water(int chan);
int snd_dspcmd_def_low_water(int chan);
#endif	KERNEL
#endif _SND_DSP_

