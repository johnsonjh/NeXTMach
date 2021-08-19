/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 * 29-Aug-90  Julius Smith (julius) at NeXT
 *	Added support for packed-mode reads and writes.
 *
 * 23-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Remove obsolete condition message type.
 *
 * 21-Aug-90  Julius Smith (jos) at NeXT
 *	Added protocol message type for setting the protocol from the
 *	dsp command queue.
 *
 *  7-Jun-90  Matt Christiano (mchristo) at NeXT
 *	More DMA changes.
 *
 *  9-May-90  Matt Christiano (mchristo) at NeXT
 *	Miscellaneous changes for DMA out to DSP and user initiated DMA.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <dsp.h>
#if	NDSP > 0

/*
 * Respond to messages destined for the dsp device interface.
 */

#import <kern/xpr.h>
#import <sys/buf.h>
#import <sys/param.h>
#import <kern/thread.h>
#import <sys/callout.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <next/cpu.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_dsp.h>
#import <nextdev/snd_dspreg.h>

#import <machine/spl.h>

extern struct snd_var snd_var;
extern struct dsp_var dsp_var;

static int snd_dspcmd_msg(snd_dspcmd_msg_t *msg);
static int snd_dspcmd_event(snd_dspcmd_event_t *msg);
static int snd_dspcmd_chandata(snd_dspcmd_chandata_t *msg);
static int snd_dspcmd_dma_out(snd_dspcmd_dma_t *msg);
static int snd_dspcmd_dma_in(snd_dspcmd_dma_t *msg);
static int snd_dspcmd_abortdma(snd_dspcmd_abortdma_t *msg);
static int snd_dspcmd_req_msg(snd_dspcmd_req_msg_t *msg);
static int snd_dspcmd_req_err(snd_dspcmd_req_err_t *msg);

#if	DEBUG || XPR_DEBUG
extern int dspdbgflag;
#endif	DEBUG || XPR_DEBUG
#if	XPR_DEBUG
#define dspdbug(f) {if (dspdbgflag&1)printf f; else XPR(XPR_DSP, f);}
#define dspdbug2(f) { if (dspdbgflag&2) { \
			if (dspdbgflag&1) printf f; else XPR(XPR_DSP, f);} \
		    }
#else	XPR_DEBUG
#define dspdbug(f)
#define dspdbug2(f)
#endif	XPR_DEBUG

/*
 * Generic command port message.
 */
int snd_dsp_cmd_port_msg(msg_header_t *msg)
{
	switch (msg->msg_id) {
	case SND_MSG_DSP_MSG:
		return snd_dspcmd_msg((snd_dspcmd_msg_t *)msg);
	case SND_MSG_DSP_EVENT:
		return snd_dspcmd_event((snd_dspcmd_event_t *)msg);
	case SND_MSG_DSP_CHANDATA:
		return snd_dspcmd_chandata((snd_dspcmd_chandata_t *)msg);
	case SND_MSG_DSP_DMAOUT:
		return snd_dspcmd_dma_out((snd_dspcmd_dma_t *)msg);
	case SND_MSG_DSP_DMAIN:
		return snd_dspcmd_dma_in((snd_dspcmd_dma_t *)msg);
	case SND_MSG_DSP_ABORTDMA:
		return snd_dspcmd_abortdma((snd_dspcmd_abortdma_t *)msg);
	case SND_MSG_DSP_REQ_MSG:
		return snd_dspcmd_req_msg((snd_dspcmd_req_msg_t *)msg);
	case SND_MSG_DSP_REQ_ERR:
		return snd_dspcmd_req_err((snd_dspcmd_req_err_t *)msg);
	default:
		return SND_BAD_MSG;
	}
}

/*
 *	Check dma command parameters
 */
static kern_return_t dspq_check_param(snd_dspcmd_dma_t *msg)
{
	/*
	 * We must be in complex DMA mode for this to work.
	 */
	if (!dsp_var.flags & F_MODE_C_DMA)
		return SND_BAD_CHAN;
	/*
	 * Don't allow bogus mode values
	 */
	else if ((msg->mode > SND_MODE_MAX) || (msg->mode < SND_MODE_MIN))
		return SND_BAD_PARM;

	else
		return 0;
}
/*
 *	Set up channel data	(M.C.  3/26/90)
 */
static void dspq_setup_user_chan(int chan)
{
	register struct chan_data *cdp;

	cdp = dsp_var.chan[chan];
	if (!cdp) {
		cdp = (struct chan_data *)kalloc(sizeof(*cdp));
		snd_stream_queue_init(&cdp->sndq, (caddr_t)cdp,
					dspq_start_complex);
		queue_init(&cdp->ddp_q);
		dsp_var.chan[chan] = cdp;
/*
 *		Initialize some of the fields if this is the initial setup
 */
		cdp->sndq.def_high_water = SND_DEF_HIGH_WATER_LOW;
		cdp->sndq.def_low_water = SND_DEF_LOW_WATER_LOW;
		cdp->flags = 0;
	}

}
/*
 * Setup a multi-part message and enqueue it for the dsp_loop.
 * Run the dsp_loop.
 */
static int snd_dspcmd_msg(snd_dspcmd_msg_t *msg)
{
	snd_dsp_type_t *msg_type = (snd_dsp_type_t *)(msg+1);
	register int size = msg->header.msg_size - sizeof(*msg);
	queue_head_t dq;
	dsp_msg_t *dmsg;
	int pri = msg->pri;
	int atomic = msg->atomic;
	port_name_t from_port = msg->header.msg_remote_port;

	if (msg->header.msg_id != SND_MSG_DSP_MSG)
		return (SND_BAD_MSG);

	if (dsp_var.flags&F_SHUTDOWN)
		return SND_NODATA;

	/*
	 * Walk through the passed message and compose a dsp command
	 * message to enqueue.
	 */
	queue_init(&dq);
	while (size > 0) {
		switch (msg_type->type) {
		default:
			return SND_BAD_MSG;

		case SND_DSP_MT_CONDITION: {
			snd_dsp_condition_t *mt_condition =
				(snd_dsp_condition_t *)msg_type;
			int type = msg_type->type;

			msg_type = (snd_dsp_type_t *)(mt_condition+1);
			size -= sizeof(*mt_condition);

			dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
			ASSERT(dmsg);
			dmsg->type = condition;
			dmsg->u.condition.mask = mt_condition->mask;
			dmsg->u.condition.flags = mt_condition->flags;

			dmsg->u.condition.reply_port =
				mt_condition->reply_port;
			dmsg->datafollows = 0;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_DATA: {
			snd_dsp_data_t *mt_data = (snd_dsp_data_t *)msg_type;
			register int data_size = 
				  mt_data->dataType.msg_type_long_number
				* mt_data->dataType.msg_type_long_size/8;

			if (mt_data->dataType.msg_type_header.msg_type_inline)
			{
			    int msg_size = data_size + sizeof(dsp_msg_t);

			    dmsg = (dsp_msg_t *) kalloc(msg_size);
			    dmsg->datafollows = 1;
			    bcopy((char *)&mt_data->data, (char *)(dmsg+1),
				  data_size);
			    dmsg->u.data.addr = (vm_address_t)(dmsg+1);
			    size -= sizeof(*mt_data)
			      - sizeof(mt_data->data)
				+ data_size;
			    msg_type = (snd_dsp_type_t *)
				    (  (char *)(mt_data+1)
					- sizeof(mt_data->data)
					+ data_size);
			} else {
				vm_address_t src_addr =
					trunc_page(mt_data->data);
				int src_offset =
					(int)(mt_data->data)%PAGE_SIZE;
				int src_size =
					round_page(data_size + src_offset);
				
				dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
				ASSERT(dmsg);
				dmsg->datafollows = 0;
				
				/*
				 * Create a buffer in the kernel's address
				 * space to receive a copy of the write data.
				 */
				if (    vm_allocate(kernel_map,
					    &dmsg->u.data.addr,
					    src_size, TRUE)
				    != KERN_SUCCESS)
				{
					return SND_NO_MEMORY;
				}

				/*
				 * Copy from our task map into the allocated
				 * range in the kernel map.  We'll deallocate
				 * the source so we don't have to deal with
				 * it anymore.
				 */
				if (    vm_map_copy(kernel_map,
						    snd_var.task_map,
						    dmsg->u.data.addr,
						    src_size,
						    src_addr, FALSE, TRUE)
				    != KERN_SUCCESS)
				{
					(void)vm_deallocate(kernel_map,
						dmsg->u.data.addr, src_size);
					return SND_NO_MEMORY;
				}
				
				/*
				 * Wire down the pages in the kernel map.
				 */
				(void)vm_map_pageable(kernel_map,
					dmsg->u.data.addr,
					dmsg->u.data.addr + src_size,
					FALSE);
				dmsg->u.data.addr += src_offset;
				size -= sizeof(*mt_data);
				msg_type = (snd_dsp_type_t *)(mt_data+1);
			}
			switch (mt_data->dataType.msg_type_long_size) {
			case 32:
				dmsg->type = data4;
				break;
			case 24:
				dmsg->type = data3;
				break;
			case 16:
				dmsg->type = data2;
				break;
			case 8:
				dmsg->type = data1;
				break;
			}
			dmsg->u.data.size = data_size;
			dmsg->u.data.loc = dmsg->u.data.addr;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_RDATA: {
			snd_dsp_data_t *mt_data = (snd_dsp_data_t *)msg_type;
			register int data_size = mt_data->data;

			if (data_size > MSG_SIZE_MAX - 
						sizeof(snd_recorded_data_t))
				return SND_NO_MEMORY;

			dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
			ASSERT(dmsg);
			dmsg->u.data.addr = kalloc(data_size);
			dmsg->datafollows = 0;
			dmsg->u.data.reply_port = from_port;

			switch (mt_data->dataType.msg_type_long_size) {
			case 32:
				dmsg->type = rdata4;
				break;
			case 24:
				dmsg->type = rdata3;
				break;
			case 16:
				dmsg->type = rdata2;
				break;
			case 8:
				dmsg->type = rdata1;
				break;
			}
			dmsg->u.data.size = data_size;
			dmsg->u.data.loc = dmsg->u.data.addr;
			msg_type = (snd_dsp_type_t *)
				(  (char *)(mt_data+1));
			size -= sizeof(*mt_data);
			
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_HOST_COMMAND: {
			snd_dsp_host_command_t *mt_host_command =
				(snd_dsp_host_command_t *)msg_type;

			msg_type = (snd_dsp_type_t *)(mt_host_command+1);
			size -= sizeof(*mt_host_command);
			dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
			ASSERT(dmsg);
			dmsg->type = host_command;
			dmsg->u.host_command = mt_host_command->hc;
			dmsg->datafollows = 0;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_HOST_FLAG: {
			snd_dsp_host_flag_t *mt_host_flag =
				(snd_dsp_host_flag_t *)msg_type;

			msg_type = (snd_dsp_type_t *)(mt_host_flag+1);
			size -= sizeof(*mt_host_flag);
			dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
			ASSERT(dmsg);
			dmsg->type = host_flag;
			dmsg->u.host_flag.mask =
				(mt_host_flag->mask&DSP_MSG_FLAGS_MASK);
			dmsg->u.host_flag.flags =
				(mt_host_flag->flags&DSP_MSG_FLAGS_MASK);
			dmsg->datafollows = 0;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_RET_MSG: {
			snd_dsp_ret_msg_t *mt_ret_msg =
				(snd_dsp_ret_msg_t *)msg_type;
			register int data_size = mt_ret_msg->ret_msg.msg_size;

			if (mt_ret_msg->ret_msgType
				.msg_type_header.msg_type_inline)
			{
				dmsg = (dsp_msg_t *)
					kalloc(sizeof(dsp_msg_t) + data_size);
				ASSERT(dmsg);
				dmsg->datafollows = 1;
				bcopy((char *)&mt_ret_msg->ret_msg,
					(char *)(dmsg+1), data_size);
				dmsg->u.ret_msg = (msg_header_t *)(dmsg+1);
			} else {
				dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
				ASSERT(dmsg);
				dmsg->datafollows = 0;
				ASSERT(0);	// to hard for now.
			}

			dmsg->type = ret_msg;
			if (mt_ret_msg->ret_port)
				dmsg->u.ret_msg->msg_remote_port =
					mt_ret_msg->ret_port;
			else
				dmsg->u.ret_msg->msg_remote_port =
					snd_var.dspowner;

			msg_type = (snd_dsp_type_t *)
				(((char *)&mt_ret_msg->ret_msg)+data_size);
			size -= sizeof(*mt_ret_msg) -
				sizeof(mt_ret_msg->ret_msg) + data_size;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_RESET: {
			snd_dsp_reset_t *mt_reset =
				(snd_dsp_reset_t *)msg_type;
			msg_type = (snd_dsp_type_t *)(mt_reset+1);
			size -= sizeof(*mt_reset);
			dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
			ASSERT(dmsg);
			dmsg->type = reset;
			dmsg->datafollows = 0;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_GET_REGS: {
			snd_dsp_get_regs_t *mt_get_regs =
				(snd_dsp_get_regs_t *)msg_type;
			msg_type = (snd_dsp_type_t *)(mt_get_regs+1);
			size -= sizeof(*mt_get_regs);
			dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
			ASSERT(dmsg);
			dmsg->type = ret_regs;
			dmsg->datafollows = 0;
			dmsg->priority = pri;
			dmsg->sequence = DSP_MSG_MIDDLE;
			dmsg->atomic = atomic;
			dmsg->internal = 0;
			queue_enter(&dq, dmsg, dsp_msg_t *, link);
			break;
		}
		case SND_DSP_MT_PROTO: {
		    snd_dsp_mt_proto_t *mt_dsp_proto = 
		      (snd_dsp_mt_proto_t *)msg_type;
		    msg_type = (snd_dsp_type_t *)(mt_dsp_proto+1);
		    size -= sizeof(*mt_dsp_proto);
		    dmsg = (dsp_msg_t *)kalloc(sizeof(dsp_msg_t));
		    ASSERT(dmsg);
		    dmsg->type = protocol;
		    dmsg->u.new_protocol = mt_dsp_proto->proto;
		    dmsg->datafollows = 0;
		    dmsg->priority = pri;
		    dmsg->sequence = DSP_MSG_MIDDLE;
		    dmsg->atomic = atomic;
		    dmsg->internal = 0;
		    queue_enter(&dq, dmsg, dsp_msg_t *, link);
		    break;
		}
		}
	}

	if (queue_empty(&dq))
		dspq_execute();
	else if (queue_first(&dq) == queue_last(&dq)) {
		ASSERT((queue_entry_t)dmsg == queue_first(&dq));
		dmsg->sequence = DSP_MSG_ONLY;
		dspq_enqueue(&dq);
	} else {
		((dsp_msg_t *)queue_last(&dq))->sequence = DSP_MSG_LAST;
		((dsp_msg_t *)queue_first(&dq))->sequence = DSP_MSG_FIRST;
		dspq_enqueue(&dq);
	}
	return SND_NO_ERROR;
}

/*
 * Setup the enclosed message to be sent  when the indicated conditions
 * are met.
 */
static int snd_dspcmd_event(snd_dspcmd_event_t *msg)
{
	register int s;
	dsp_msg_t *rmsg =
		(dsp_msg_t *)kalloc(sizeof(dsp_msg_t) + msg->ret_msg.msg_size);

	rmsg->type = ret_msg;
	rmsg->datafollows = 1;
	rmsg->internal = 0;
	bcopy(msg->ret_msg, rmsg+1, msg->ret_msg.msg_size);

	s = spldsp();
	if (dsp_var.event_msg)
		dspq_free_msg(dsp_var.event_msg);
	if (msg->ret_port != PORT_NULL)
		rmsg->u.ret_msg->msg_remote_port = msg->ret_port;
	else
		rmsg->u.ret_msg->msg_remote_port = snd_var.dspowner;

	dsp_var.event_msg = rmsg;
	dsp_var.event_mask = msg->mask;
	dsp_var.event_condition = msg->flags;
	splx(s);
	dspq_execute();
	return SND_NO_ERROR;
}

/*
 * Set up information to be used for subsequent dma's on a channel
 * (read and write).  COMPLEX-DMA mode only.
 */
static int snd_dspcmd_chandata(snd_dspcmd_chandata_t *msg)
{
	register struct chan_data *cdp;
	register int chan;

	if (msg->header.msg_size != sizeof(snd_dspcmd_chandata_t))
		return SND_BAD_PARM;

	chan = msg->chan;
	if (   chan < DSP_SO_CHAN || chan > DSP_N_CHAN
	    || msg->size*mode_size(msg->mode) > PAGE_SIZE
	    || PAGE_SIZE%(msg->size*mode_size(msg->mode)))
		return SND_BAD_PARM;

	cdp = dsp_var.chan[chan];
	if (!cdp) {
		cdp = (struct chan_data *)kalloc(sizeof(*cdp));
		snd_stream_queue_init(&cdp->sndq, (caddr_t)cdp,
			(dsp_var.flags&F_MODE_C_DMA)
				? dspq_start_complex
				: dspq_start_simple);
		queue_init(&cdp->ddp_q);
		dsp_var.chan[chan] = cdp;
	}

	cdp->flags = 0;
	cdp->base_addr = msg->addr;
	cdp->buf_size =  msg->size;
	cdp->skip =  msg->skip;
	cdp->space = msg->space;
	cdp->mode = msg->mode;
	cdp->sndq.dmasize = snd_dspcmd_def_dmasize(chan);
	cdp->sndq.def_high_water = snd_dspcmd_def_high_water(chan);
	cdp->sndq.def_low_water = snd_dspcmd_def_low_water(chan);

	return SND_NO_ERROR;
}

int snd_dspcmd_def_dmasize(int chan)
{
	register struct chan_data *cdp = dsp_var.chan[chan];

	ASSERT(cdp);
	return cdp->buf_size * mode_size(cdp->mode);
}

int snd_dspcmd_def_high_water(int chan)
{
	switch (chan) {
	case DSP_SO_CHAN:
		return SND_DEF_HIGH_WATER_HIGH;
	case DSP_SI_CHAN:
		return SND_DEF_HIGH_WATER_LOW;
	default:
		return SND_DEF_HIGH_WATER_LOW;
	}
}

int snd_dspcmd_def_low_water(int chan)
{
	switch (chan) {
	case DSP_SO_CHAN:
		return SND_DEF_LOW_WATER_HIGH;
	case DSP_SI_CHAN:
		return SND_DEF_LOW_WATER_LOW;
	default:
		return SND_DEF_LOW_WATER_LOW;
	}
}

/*
 * User initiated dma to dsp address space.
 */
static int snd_dspcmd_dma_out(snd_dspcmd_dma_t *msg)
{
	dsp_msg_t *dmsg;
	queue_head_t q;
	kern_return_t r;

	struct  dsp_dma_out {
		snd_stream_msg_t	mh;
		snd_stream_play_data_t	pd;
	} m;
	static struct dsp_dma_out M = {
	    {
		{
		    /* no name */		0,
		    /* msg_simple */		FALSE,
		    /* msg_size */		sizeof(struct  dsp_dma_out),
		    /* msg_type */		MSG_TYPE_NORMAL,
		    /* msg_remote_port */	0,
		    /* msg_reply_port */	0,
		    /* msg_id */		SND_MSG_STREAM_MSG
		},
		{
		    /* msg_type_name = */		MSG_TYPE_INTEGER_32,
		    /* msg_type_size = */		32,
		    /* msg_type_number = */		1,
		    /* msg_type_inline = */		TRUE,
		    /* msg_type_longform = */		FALSE,
		    /* msg_type_deallocate = */		FALSE,
		},
		0	// data tag
	    },	// snd_stream_msg_t mh
	    {
		{{
		    /* msg_type_name = */		MSG_TYPE_INTEGER_32,
		    /* msg_type_size = */		32,
		    /* msg_type_number = */		1,
		    /* msg_type_inline = */		TRUE,
		    /* msg_type_longform = */		FALSE,
		    /* msg_type_deallocate = */		FALSE,
		},
		SND_MT_PLAY_DATA},
		{
		    /* msg_type_name = */		MSG_TYPE_INTEGER_32,
		    /* msg_type_size = */		32,
		    /* msg_type_number = */		1,
		    /* msg_type_inline = */		TRUE,
		    /* msg_type_longform = */		FALSE,
		    /* msg_type_deallocate = */		FALSE,
		},
		SND_DM_COMPLETED_MSG,	// play options
		{
			/* msg_type_name = */		MSG_TYPE_PORT,
			/* msg_type_size = */		32,
			/* msg_type_number = */		1,
			/* msg_type_inline = */		TRUE,
			/* msg_type_longform = */	FALSE,
			/* msg_type_deallocate = */	FALSE,
		},
		PORT_NULL,	// reply port for this region
		{
			{
				/* msg_type_name = */		0,
				/* msg_type_size = */		0,
				/* msg_type_number = */		0,
				/* msg_type_inline = */		FALSE,
				/* msg_type_longform = */	TRUE,
				/* msg_type_deallocate = */	TRUE
			},
			/* msg_type_long_name = */	MSG_TYPE_INTEGER_8,
			/* msg_type_long_size = */	8,
			/* msg_type_long_number = */	0,
		},
		0,		// pointer to data to play.
	    }
	};		

	/*
	 * We must be in complex DMA mode for this to work.
	 */
	if (!dsp_var.flags & F_MODE_C_DMA)
		return SND_BAD_PROTO;
	dspq_setup_user_chan(DSP_USER_REQ_CHAN);

	/*
	 * Add a region to the task_port stream containing the
	 * data we were passed.  Set up so that the user gets
	 * a completion message when it's been completed.
	 */
	m = M;
	m.mh.header.msg_remote_port =
		snd_stream_port_name(DSP_USER_REQ_CHAN, 0);
	m.mh.header.msg_local_port = msg->header.msg_remote_port;
	m.pd.reg_port = msg->header.msg_remote_port;
	m.pd.data = msg->data;
	m.pd.dataType.msg_type_long_name = msg->dataType.msg_type_long_name;
	m.pd.dataType.msg_type_long_size = msg->dataType.msg_type_long_size;
	m.pd.dataType.msg_type_long_number =msg->dataType.msg_type_long_number;
	r = msg_send((msg_header_t *)&m, MSG_OPTION_NONE, 0);
	if (r != KERN_SUCCESS)
		return r;

	/*
	 * Send a dma_out command down the dsp command queue
	 * which will get it's stuff from the stream.
	 */
	queue_init(&q);
	dmsg = (dsp_msg_t *)kalloc(sizeof(*dmsg));
	dmsg->type = dma_out;
	dmsg->u.dma.addr = msg->addr;
	dmsg->u.dma.size = msg->size;
	dmsg->u.dma.skip = msg->skip;
	dmsg->u.dma.space = msg->space;
	dmsg->u.dma.mode = msg->mode;
	dmsg->u.dma.chan = DSP_USER_REQ_CHAN;
	dmsg->datafollows = 0;
	dmsg->internal = 0;
	queue_enter(&q, dmsg, dsp_msg_t *, link);
	dspq_enqueue(&q);

	return 0;
}

/*
 * User initiated dma from dsp address space.
 */
static int snd_dspcmd_dma_in(snd_dspcmd_dma_t *msg)
{
	dsp_msg_t *dmsg;
	queue_head_t q;
	kern_return_t r;

	struct  dsp_dma_in {
		snd_stream_msg_t		mh;
		snd_stream_record_data_t	rd;
	} m;
	static struct dsp_dma_in M = {
	    {
		{
		    /* no name */		0,
		    /* msg_simple */		FALSE,
		    /* msg_size */		sizeof(struct  dsp_dma_in),
		    /* msg_type */		MSG_TYPE_NORMAL,
		    /* msg_remote_port */	0,
		    /* msg_reply_port */	0,
		    /* msg_id */		SND_MSG_STREAM_MSG
		},
		{
		    /* msg_type_name = */		MSG_TYPE_INTEGER_32,
		    /* msg_type_size = */		32,
		    /* msg_type_number = */		1,
		    /* msg_type_inline = */		TRUE,
		    /* msg_type_longform = */		FALSE,
		    /* msg_type_deallocate = */		FALSE,
		},
		0	// data tag
	    },	// snd_stream_msg_t mh
	    {
		{{
		    /* msg_type_name = */		MSG_TYPE_INTEGER_32,
		    /* msg_type_size = */		32,
		    /* msg_type_number = */		1,
		    /* msg_type_inline = */		TRUE,
		    /* msg_type_longform = */		FALSE,
		    /* msg_type_deallocate = */		FALSE,
		},
		SND_MT_RECORD_DATA},
		{
		    /* msg_type_name = */		MSG_TYPE_INTEGER_32,
		    /* msg_type_size = */		32,
		    /* msg_type_number = */		2,
		    /* msg_type_inline = */		TRUE,
		    /* msg_type_longform = */		FALSE,
		    /* msg_type_deallocate = */		FALSE,
		},
		SND_DM_COMPLETED_MSG,	// record options
		0,			// number of bytes to record
		{
			/* msg_type_name = */		MSG_TYPE_PORT,
			/* msg_type_size = */		32,
			/* msg_type_number = */		1,
			/* msg_type_inline = */		TRUE,
			/* msg_type_longform = */	FALSE,
			/* msg_type_deallocate = */	FALSE,
		},
		PORT_NULL,	// reply port for this region
		{
			/* msg_type_name = */		MSG_TYPE_CHAR,
			/* msg_type_size = */		8,
			/* msg_type_number = */		0,
			/* msg_type_inline = */		TRUE,
			/* msg_type_longform = */	FALSE,
			/* msg_type_deallocate = */	FALSE,
		}	// filename ""
	    }
	};		

	/*
	 * We must be in complex DMA mode for this to work.
	 */
	if (!dsp_var.flags & F_MODE_C_DMA)
		return SND_BAD_PROTO;
	dspq_setup_user_chan(DSP_USER_REQ_CHAN);

	/*
	 * Add a region to the task_port stream containing the
	 * data we were passed.  Set up so that the user gets
	 * a completion message when it's been completed.
	 */
	m = M;
	m.mh.header.msg_remote_port =
		snd_stream_port_name(DSP_USER_REQ_CHAN, 0);
	m.mh.header.msg_local_port = msg->header.msg_remote_port;
	m.rd.reg_port = msg->header.msg_remote_port;
	m.rd.nbytes = mode_size(msg->mode) * msg->size;
	r = msg_send((msg_header_t *)&m, MSG_OPTION_NONE, 0);
	if (r != KERN_SUCCESS)
	  return r;

	/*
	 * Send a dma_out command down the dsp command queue
	 * which will get it's stuff from the stream.
	 */
	queue_init(&q);
	dmsg = (dsp_msg_t *)kalloc(sizeof(*dmsg));
	dmsg->type = dma_in;
	dmsg->u.dma.addr = msg->addr;
	dmsg->u.dma.size = msg->size;
	dmsg->u.dma.skip = msg->skip;
	dmsg->u.dma.space = msg->space;
	dmsg->u.dma.mode = msg->mode;
	dmsg->u.dma.chan = DSP_USER_REQ_CHAN;
	dmsg->datafollows = 0;
	dmsg->internal = 0;
	queue_enter(&q, dmsg, dsp_msg_t *, link);
	dspq_enqueue(&q);

	return 0;
}

/*
 * Abort any pending dma and return it's status.
 */
static int snd_dspcmd_abortdma(snd_dspcmd_abortdma_t *msg)
{
	return SND_BAD_PARM;
}

/*
 * Return (or wait for) any messages pending.
 */
static int snd_dspcmd_req_msg(snd_dspcmd_req_msg_t *msg)
{
	port_t r_port;
	int was_full = (dsp_var.msgp == dsp_var.emsgbuf);

	/*
	 * Port defaults to owner port.
	 */
	if (msg->msg_remote_port)
		r_port = msg->msg_remote_port;
	else
		r_port = snd_var.dspowner;

	/*
	 * Send what we've got, register to get it when we got it.
	 */
	if (dsp_var.msgp != dsp_var.msgbuf) {
		dspdbug2(("snd_dc_req_msg: reply now\n"));
		snd_reply_dsp_msg(r_port);
	} else {
		dspdbug2(("snd_dc_req_msg: save port\n"));
		dsp_var.msg_port = r_port;
	}

	/*
	 * This might re-enable conditions in the low-level device loop.
	 */
	if (was_full)
		dsp_dev_loop();
	return 0;
}

/*
 * Return (or wait for) any error messages pending.
 */
static int snd_dspcmd_req_err(snd_dspcmd_req_err_t *msg)
{
	port_t r_port;
	int was_full = (dsp_var.errp == dsp_var.eerrbuf);

	/*
	 * Port defaults to owner port.
	 */
	if (msg->msg_remote_port)
		r_port = msg->msg_remote_port;
	else
		r_port = snd_var.dspowner;

	/*
	 * Send what we've got, register to get it when we got it.
	 */
	if (dsp_var.errp != dsp_var.errbuf) {
		dspdbug2(("snd_dc_req_err: reply now\n"));
		snd_reply_dsp_err(r_port);
	} else {
		dspdbug2(("snd_dc_req_err: save port\n"));
		dsp_var.err_port = r_port;
	}

	/*
	 * This might re-enable conditions in the low-level device loop.
	 */
	if (was_full)
		dsp_dev_loop();
	return 0;
}

#endif	NDSP > 0
