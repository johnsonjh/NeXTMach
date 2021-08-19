/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *  1-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Added ndma field to snd_queue structure (stream).  This is used to
 *	dynamically allocate dma descriptors.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _SND_VAR_
#define _SND_VAR_

#import <kern/thread.h>
#import <nextdev/dma.h>
#import <nextdev/snd_msgs.h>

#define splsound() splbio()
/*
 * Structures used to describe data being played
 */
typedef struct snd_region {
	vm_address_t	addr;		// base virtual addr of reg in task map
	int		size;		// size of region in bytes
	vm_address_t	end;		// end virtual addr of reg in task map
	volatile
	vm_address_t	rw_head;	// play/record location for alloc
	volatile
	vm_address_t	rw_tail;	// play/record location for free
	u_int		dma_size;	// number of bytes per dma_desc
	port_name_t	reply_port;	// port to send competion message on
	vm_address_t	first_locked;	// first page locked in task map
	vm_address_t	next_to_lock;	// next page to lock in task map
	int		high_water;	// max # bytes to lock down
#define	SND_DEF_HIGH_WATER_HIGH	((256+512)*1024)
#define	SND_DEF_HIGH_WATER_MED	(256*1024)
#define	SND_DEF_HIGH_WATER_LOW	(64*1024)
	int		low_water;	// min # bytes to lock down
#define	SND_DEF_LOW_WATER_HIGH	(512*1024)
#define	SND_DEF_LOW_WATER_MED	(128*1024)
#define	SND_DEF_LOW_WATER_LOW	(48*1024)
	volatile
	u_int		completed:1,	// want's a "completed" message
			started:1,	// want's a "started" message
			aborted:1,	// want's an "aborted" message
			paused:1,	// want's a "paused" message
			resumed:1,	// want's a "resumed" message
			overflow:1,	// want's overflow error message
			awaiting:1,	// waiting for (recorded) data
			rate:2,		// playback/record data rate
#define SND_RATE_22	0
#define SND_RATE_44	1
			direction:1,	// DIR_PLAY || DIR_RECORD
#define SND_DIR_PLAY	0
#define SND_DIR_RECORD	1
			was_aborted:1,	// early termination
			was_started:1,	// we've started using this region
			is_complete:1,	// we've finished using this region
			discont:1,	// force an underflow before starting
			did_overflow:1,	// overflow/underflow detected in reg
			deactivate:1;	// place on inactive list?
	int		reg_id;		// id used in replies
	queue_chain_t	link;		// queue of linked entries
	struct snd_queue *sound_q;	// sound queue we're contained within
} snd_region_t;

typedef struct snd_dma_desc {
	vm_address_t	vaddr;		// virtual address of data
	vm_size_t	size;		// # bytes in this desc (initially)
	caddr_t		ptr;		// to higher level data structure
	struct dma_hdr	hdr;		// dma header (phys addr, size)
	int (*free)(struct snd_dma_desc *dd); // func called on dma completion
	queue_chain_t	link;		// queue of dma_desc's
	int		flag;		// flags at the end of this dma
#define SDD_OVERFLOW	1		// underrun found at end of dma
} snd_dma_desc_t;

typedef struct snd_queue {
	caddr_t		ptr;		// device specific pointer
	lock_data_t	lock;		// queue lock, for scheduling
	queue_head_t	reg_q;		// queue of regions
	queue_head_t	free_q;		// queue of freed dma descriptors
	thread_t	thread;		// thread managing this queue (paging)
	vm_size_t	total_wired;	// total #bytes wired down in queue
	u_int		exclusive:1,	// exclusive use queue?
			enabled:1,	// go ahead and send stuff down.
			active:1,	// lower level is active, accepting req
			paused:1,	// don't start anything up.
			linked:1,	// stream linked to another.
			will_overflow:1,// lower level couldn't get data
			work:1;		// indication of work to do on queue
	u_int		nxfer;		// number of bytes xfered on this queue
	u_int		dmasize;	// number of bytes per dma_desc
	u_int		ndma;		// number of dma_descs in stream.
	int		def_high_water;	// default max # bytes to lock down
	int		def_low_water;	// default min # bytes to lock down
	boolean_t	(*start)(	// routine to call to startup a dma_d
				snd_dma_desc_t *ddp,
				int		direction,
				int		rate);
} snd_queue_t;

/*
 * High-level per-device data structure
 */
typedef struct snd_var {
	port_name_t		dspowner;	// port of dsp device owner
	port_name_t		sndinowner;	// port of sndin device owner
	port_name_t		sndoutowner;	// port of sndout device owner
	port_name_t		dspnegotiation;	// for negotiating dsp owner
	port_name_t		sndinnegotiation; // for negotiation sin own
	port_name_t		sndoutnegotiation; // for negotiation sout own
	port_set_name_t		portset;	// for recieving msgs
	queue_head_t		new_thread_q;	// q of q's waiting threads
	port_t			dev_port;	// my device port
	task_t			task;		// me
	vm_map_t		task_map;	// my address space
	port_name_t		task_port;	// my task port
	port_name_t		host_priv_self;	// our version of host priv.
} snd_var_t;

/*
 * Port names
 */
#define SND_PN_INDEX_MASK	0x07f		// channel index
#define SND_PN_INDEX_SHIFT	0
#define SND_PN_DEV_MASK		0x80		// raw device, not dsp
#define SND_PN_DEV_SHIFT	7
#define SND_PN_ASSIGNED		0x10000		// indicates renamed port

#define snd_stream_port_name(index, device) \
	((port_name_t)(snd_gd_bitmap(index, device) | SND_PN_ASSIGNED))

#define snd_dsp_cmd_port_name() \
	((port_name_t)snd_stream_port_name(DSP_N_CHAN, FALSE))

/*
 * Routine prototypes
 */
#if	KERNEL
void snd_server_loop(void);
void snd_stream_init(void);
void snd_stream_reset(void);
void snd_stream_queue_init(
	register snd_queue_t *q,
	caddr_t addr,
	boolean_t (*start)(snd_dma_desc_t *ddp, int direction, int rate));
void snd_stream_queue_reset(register snd_queue_t *q);
void snd_stream_enqueue_region(
	snd_region_t *reg,
	int index,
	boolean_t device,
	boolean_t preempt);
void snd_stream_abort(snd_queue_t *q, int data_tag);
void snd_stream_pause(snd_queue_t *q);
void snd_stream_resume(snd_queue_t *q);
kern_return_t snd_stream_await (
	snd_queue_t	*q,
	int		data_tag);
msg_header_t *snd_rcv_alloc_msg_frame();
int snd_dsp_cmd_port_msg(msg_header_t *msg);
void snd_link_init(int direction);
void snd_link_shutdown(int direction);
void snd_link_pause(snd_queue_t *q);
void snd_link_resume(snd_queue_t *q);
#endif	KERNEL
#endif	_SND_VAR_
