/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 * 13-Sep-90  Gregg Kellogg (gk) at NeXT
 *	Added old code in (#if JOSBUG) to get TREQ set more often.
 *	This caused mulaw playback to hang.
 *
 * 30-Aug-90  Julius Smith (julius) at NeXT
 *	Fixed dspq_check() and dspq_execute to ensure that commands can't get
 *	out of order.
 *
 * 29-Aug-90  Julius Smith (julius) at NeXT
 *	Added support for packed-mode reads and writes.
 *
 * 16-Aug-90  Julius Smith (jos) and Matt Christiano (mchristo) at NeXT
 *	Changed rdata to use callout.
 *
 * 27-Jul-90  Gregg Kellogg (gk) at NeXT
 *	More 040 changes.
 *
 *  2-Jul-90  Matt Christiano (mchristo) and Julius Smith (jos) at NeXT
 *	More DMA changes.  Also rdata case modified.
 *
 *  7-Jun-90  Matt Christiano (mchristo) at NeXT
 *	More DMA changes.
 *
 *  9-May-90  Matt Christiano (mchristo) at NeXT
 *	Miscellaneous changes for DMA out to DSP and user initiated DMA.
 *
 * 12-Mar-90  John Seamons (jks) at NeXT
 *	Must only do byte accesses on the 68040 to the byte-wide DSP device.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT, Inc.
 *	Created.
 *
 * 25-Jun-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	release the port after doing a object_copyin.  This was resulting
 *	in a leak in the port_zone.
 *
 */ 

#import <dsp.h>
#if	NDSP > 0

/*
 * Manipulate queue of commands pending for the dsp.
 */

#import <kern/xpr.h>
#import <sys/buf.h>
#import <sys/param.h>
#import <kern/thread.h>
#import <sys/callout.h>
#import <vm/vm_param.h>
#import <vm/vm_kern.h>
#import <next/pmap.h>
#import <nextdev/busvar.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_dsp.h>
#import <nextdev/snd_dspreg.h>

#import <machine/spl.h>

extern snd_var_t snd_var;
dsp_var_t dsp_var;

#if	DEBUG
extern volatile struct dsp_regs *regs;
static int dsp_cmd_q_max;
#else	DEBUG
#define regs ((volatile struct dsp_regs *)P_DSP)
#endif	DEBUG

#if	DEBUG || XPR_DEBUG
extern int dspdbgflag;
#endif	DEBUG || XPR_DEBUG
#if	XPR_DEBUG

#define dspdbug(f) { if (dspdbgflag&1) printf f; else XPR(XPR_DSP, f);}
#define dspdbug2(f) { if (dspdbgflag&2) { \
			if (dspdbgflag&1) printf f; else XPR(XPR_DSP, f);} \
		    }
#define dspdmadbug(f) { if (dspdbgflag&4) XPR(XPR_SOUND, f);}
#define DSPREGS ((struct dsp_regs *)P_DSP)
#define DSPREGSFMT "icr %x cvr %x isr %x ivr %x\n", \
		DSPREGS->icr, DSPREGS->cvr, DSPREGS->isr, DSPREGS->ivr
#else	XPR_DEBUG
#define dspdbug(f)
#define dspdbug2(f)
#define dspdmadbug(f)
#endif	XPR_DEBUG

/* Transmit word through xmit registers (with timeout) */
static inline int xmit(unsigned int val)
{
	register int cnt;
	for (cnt = 25; cnt && (regs->isr&ISR_TXDE) == 0; cnt--) {
		DELAY(1);
	}
	if (cnt)
		/* 68040 must write as bytes */
		if (cpu_type != MC68030) {
			/*
			 * Must write least-signficant byte last!
			 */
			regs->txh = (val >> 16) & 0xff;
			regs->txm = (val >> 8) & 0xff;
			regs->txl = val & 0xff;
		} else
			regs->transmit = val;
	return(!cnt);
}

/* Receive word from recv registers (with timeout) */

static inline int recv(int *val, int size)
{
    register unsigned int cnt;
    register char *bp = (char *)val;
    for (cnt = 25; cnt && ((regs->isr&ISR_RXDF) == 0); cnt--) {
	DELAY(1);
    }
    if (cnt)
    {
	/* 68040 must read as bytes */
	if (cpu_type != MC68030) {
	    register unsigned int lo,mid,hi;
	    /*
	     * Must read least-signficant byte last!
	     */
	    switch (size) {
	    case 4:
	    default:
		hi = regs->rxh;
		mid = regs->rxm;
		lo = regs->rxl;
		*val = ((hi << 16) & 0xFF0000) |
		  ((mid << 8) & 0x00FF00) | (lo & 0xFF);
		break;
	    case 2:
		mid = regs->rxm;
		lo = regs->rxl;
		* (short *) val = ((mid << 8) & 0x00FF00) | (lo & 0xFF);
		break;
	    case 3:
		*bp++ = regs->rxh;
		*bp++ = regs->rxm;
		*bp = regs->rxl;
		break;
	    case 1:
		lo = regs->rxl;
		* (char *) val = lo & 0x0ff;
		break;
	    }
	} else {
	    register unsigned int intval;
	    /*
	     *		Move the data out
	     */
	    switch (size) {
	    case 4:
	    default:
		intval = regs->receive;
		*val = intval & 0xffffff;
		break;
	    case 3:
		*bp++ = regs->rxh;
		*bp++ = regs->rxm;
		*bp = regs->rxl;
		break;
	    case 2:
		intval = regs->receive;
		* (short *) val = intval & 0x0ffff;
		break;
	    case 1:
		* (char *) val = regs->rxl & 0x0ff;
		break;
	    }
	}
    } else
      dspdbug(("recv timed out\n"));
    return(!cnt);
}

/*
 * Facilities used local to the driver for messaging.
 */

static queue_head_t lmsgq;
static queue_head_t msg_free_q;

static dsp_msg_t *dspq_hc(int host_command);
static dsp_msg_t *dspq_data(void);
static dsp_msg_t *dspq_condition(int mask, int flags);
static dsp_msg_t *dspq_hf(int mask, int flags);
static void dspq_msg_send(dsp_msg_t *msg);
static void dspq_output_stream(struct chan_data *cdp);
static kern_return_t dspq_reply_rdata(dsp_msg_t *msg);

static inline dsp_msg_t *dspq_alloc_lmsg(void)
{
	register dsp_msg_t *lmsg;

	ASSERT(!queue_empty(&lmsgq));
	queue_remove_first(&lmsgq, lmsg, dsp_msg_t *, link);
	dspdbug2(("dspq_alloc_lmsg: 0x%x\n", lmsg));
	return(lmsg);
}

static inline void dspq_free_lmsg(dsp_msg_t *lmsg)
{
	dspdbug2(("dspq_free_lmsg: 0x%x\n", lmsg));
	queue_enter(&lmsgq, lmsg, dsp_msg_t *, link);
}

void dspq_init_lmsg(void)
{
	register int i;

	queue_init(&dsp_var.cmd_q);
	dsp_var.cmd_q_size = 0;
	queue_init(&lmsgq);
	queue_init(&msg_free_q);
	for (i = DSP_NLMSG; i; i--)
		dspq_free_lmsg((dsp_msg_t *)kalloc(DSP_LMSG_SIZE));

#if	DEBUG
	dsp_cmd_q_max = DSP_CMD_Q_MAX << ((dspdbgflag&0xff00)>>8);
#undef DSP_CMD_Q_MAX
#define DSP_CMD_Q_MAX dsp_cmd_q_max
#endif	DEBUG
}

void dspq_reset_lmsg(void)
{
	register dsp_msg_t *msg;
	int s;

	while (!queue_empty(&dsp_var.cmd_q)) {
		s = spldsp();
		queue_remove_first(&dsp_var.cmd_q, msg, dsp_msg_t *, link);
		splx(s);
		dspq_free_msg(msg);
	}

	while (!queue_empty(&lmsgq))
		kfree(dspq_alloc_lmsg(), DSP_LMSG_SIZE);

	if (!queue_empty(&msg_free_q))
		dspq_free_msg(0);
}

static dsp_msg_t *dspq_hc(int hc)
{
	register dsp_msg_t *msg = dspq_alloc_lmsg();

	msg->type = host_command;
	msg->u.host_command = hc;
	msg->priority = DSP_MSG_HIGH;
	msg->internal = 1;
	msg->datafollows = 0;
	msg->atomic = 1;
	return msg;
}

static dsp_msg_t *dspq_data(void)
{
	register dsp_msg_t *msg = dspq_alloc_lmsg();

	msg->type = data4;
	msg->u.data.addr = (vm_address_t)(msg+1);
	msg->u.data.loc = msg->u.data.addr;
	msg->priority = DSP_MSG_HIGH;
	msg->internal = 1;
	msg->datafollows = 1;
	msg->atomic = 1;
	return msg;
}

static dsp_msg_t *dspq_condition(int mask, int flags)
{
	register dsp_msg_t *msg = dspq_alloc_lmsg();

	msg->type = condition;
	msg->u.condition.mask = mask;
	msg->u.condition.flags = flags;
	msg->u.condition.reply_port = PORT_NULL;
	msg->priority = DSP_MSG_HIGH;
	msg->internal = 1;
	msg->datafollows = 0;
	msg->atomic = 1;
	return msg;
}

static dsp_msg_t *dspq_hf(int mask, int flags)
{
	register dsp_msg_t *msg = dspq_alloc_lmsg();

	msg->type = host_flag;
	msg->u.host_flag.mask = mask;
	msg->u.host_flag.flags = flags;
	msg->priority = DSP_MSG_HIGH;
	msg->internal = 1;
	msg->datafollows = 0;
	msg->atomic = 1;
	return msg;
}

/*
 * Enqueue the following host command in priority order into the
 * queue.  (Used for internal communication).
 */
void dspq_enqueue_hc(int host_command)
{
	register dsp_msg_t *msg;
	queue_head_t qh;

	queue_init(&qh);
	msg = dspq_hc(host_command);
	msg->sequence = DSP_MSG_ONLY;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	dspq_enqueue(&qh);
}

/*
 * Enqueue the following host message in priority order into the
 * queue.  (Used for internal communication).
 */
void dspq_enqueue_hm(int host_message)
{
	register dsp_msg_t *msg;
	int *wp;
	queue_head_t qh;

	queue_init(&qh);
	msg = dspq_condition((ISR_HF2|ISR_TRDY)<<8, ISR_TRDY<<8);
	msg->sequence = DSP_MSG_FIRST;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	msg = dspq_data();
	wp = (int *)msg->u.data.addr;
	*wp++ = host_message;
	msg->u.data.size = sizeof(int);
	msg->sequence = DSP_MSG_MIDDLE;
	queue_enter(&qh, msg, dsp_msg_t *, link);
	
	msg = dspq_hc(DSP_hc_XHM);
	msg->sequence = DSP_MSG_LAST;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	dspq_enqueue(&qh);
}

/*
 * Enqueue the specified dsp syscall message (with arg) in priority order 
 * into the queue.  (Used for internal communication).
 */
void dspq_enqueue_syscall(int arg)
{
	register dsp_msg_t *msg;
	int *wp;
	queue_head_t qh;

	queue_init(&qh);
	msg = dspq_condition( ( (CVR_HC<<16) | ((ISR_HF2|ISR_TRDY)<<8) ),
					ISR_TRDY<<8);
	msg->sequence = DSP_MSG_FIRST;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	msg = dspq_hc(DSP_hc_SYSCALL);
	msg->sequence = DSP_MSG_MIDDLE;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	msg = dspq_data();
	wp = (int *)msg->u.data.addr;
	*wp++ = arg;
	msg->u.data.size = sizeof(int);
	msg->sequence = DSP_MSG_LAST;
	queue_enter(&qh, msg, dsp_msg_t *, link);
	
	dspq_enqueue(&qh);
}

/*
 * Enqueue a message setting the specified host flag.
 * (Used for internal communication).
 */
void dspq_enqueue_hf(int mask, int flags)
{
	register dsp_msg_t *msg;
	queue_head_t qh;

	queue_init(&qh);
	msg = dspq_hf(mask, flags);
	msg->sequence = DSP_MSG_ONLY;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	dspq_enqueue(&qh);
}

/*
 * Enqueue a message to await the specified condition.
 * (Used for internal communication).
 */
void dspq_enqueue_cond(int mask, int flags)
{
	register dsp_msg_t *msg;
	queue_head_t qh;

	queue_init(&qh);
	msg = dspq_condition(mask, flags);
	msg->sequence = DSP_MSG_ONLY;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	dspq_enqueue(&qh);
}

/*
 * Enqueue a state change
 * (Used for internal communication).
 */
void dspq_enqueue_state(enum dsp_state state)
{
	register dsp_msg_t *msg = dspq_alloc_lmsg();
	queue_head_t qh;

	queue_init(&qh);
	msg->type = driver_state;
	msg->u.driver_state = state;
	msg->priority = DSP_MSG_HIGH;
	msg->internal = 1;
	msg->datafollows = 0;
	msg->atomic = 1;

	msg->sequence = DSP_MSG_ONLY;
	queue_enter(&qh, msg, dsp_msg_t *, link);

	dspq_enqueue(&qh);
}

/*
 * Enqueue a list of commands in priority order to the list of commands
 * to execute.
 */
void dspq_enqueue(register queue_t cmd_list_q)
{
	register int s;
	register int pri = ((dsp_msg_t *)queue_first(cmd_list_q))->priority;
	register dsp_msg_t *me;
	int q_was_empty, num_msgs;

	for (  me = (dsp_msg_t *)queue_next(cmd_list_q), num_msgs = 0
	    ; !queue_end(cmd_list_q, (queue_t)me)
	    ; me = (dsp_msg_t *)queue_next(&me->link), num_msgs++)
		;
	dspdbug2(("dspq_enqueue: msgq 0x%x %d/%d msgs\n",
		queue_first(cmd_list_q), num_msgs, dsp_var.cmd_q_size));
	/*
	 * Find the element after which this queue segment should be
	 * inserted.
	 */
	s = spldsp();
	q_was_empty = queue_empty(&dsp_var.cmd_q);
	me = (dsp_msg_t *)queue_first(&dsp_var.cmd_q);
	if (pri)
		dspdbug2(("dspq_enqueue: insert msg @ priority %d\n", pri));
	while (!queue_end(&dsp_var.cmd_q, (queue_t)me)) {
		/*
		 * Insert if there's a priority change and it's
		 * the first (unstarted) element of the message.
		 */
		if (   me->priority > pri
		    && (   me->sequence == DSP_MSG_ONLY
			|| me->sequence == DSP_MSG_FIRST
#if	0
			|| !me->atomic))
#else	0
			|| (!me->atomic && !(me->sequence&DSP_MSG_STARTED))))
#endif	0
			break;
		me = (dsp_msg_t *)me->link.next;
	}

	/*
	 * Insert this message starting from the end of the queue segment
	 * and working forwards to the queue head, which is the first
	 * element of the segment.
	 */
	if (q_was_empty) {
		/*
		 * Empty queue, initialize it to cmd_list_q
		 */
		dsp_var.cmd_q.next = queue_first(cmd_list_q);
		dsp_var.cmd_q.prev = queue_last(cmd_list_q);
		((dsp_msg_t *)queue_first(cmd_list_q))->link.prev
			= ((dsp_msg_t *)queue_last(cmd_list_q))->link.next
			= &dsp_var.cmd_q;
	} else if ((queue_t)me == &dsp_var.cmd_q) {
		/*
		 * Must insert at the end of the queue.
		 */
		((dsp_msg_t *)dsp_var.cmd_q.prev)->link.next
			= queue_first(cmd_list_q);
		((dsp_msg_t *)queue_first(cmd_list_q))->link.prev
			= dsp_var.cmd_q.prev;
		dsp_var.cmd_q.prev = queue_last(cmd_list_q);
		((dsp_msg_t *)queue_last(cmd_list_q))->link.next
			= &dsp_var.cmd_q;
	} else {
		/*
		 * Insert first command after previous link of insert
		 * command.
		 */
		if (&dsp_var.cmd_q == me->link.prev)
			dsp_var.cmd_q.next = queue_first(cmd_list_q);
		else
			((dsp_msg_t *)me->link.prev)->link.next
				= queue_first(cmd_list_q);
		((dsp_msg_t *)queue_first(cmd_list_q))->link.prev
			= me->link.prev;

		/*
		 * Insert last command before insert command.
		 */
		me->link.prev = queue_last(cmd_list_q);
		((dsp_msg_t *)queue_last(cmd_list_q))->link.next = (queue_t)me;
	}

	/*
	 * If the number of messages enqueued exeeds the max alowable,
	 * remove the dsp_cmd port from the port set so that we won't
	 * get any more requests.
	 */
	dsp_var.cmd_q_size += num_msgs;

	splx(s);

	if (   curipl() == 0
	    && dsp_var.cmd_q_size >= DSP_CMD_Q_MAX
	    && !(dsp_var.flags&F_NOCMDPORT))
	{
		dspdbug(("dspq_enqueue: queue full (%d), remove cmd port\n",
			dsp_var.cmd_q_size));
		port_set_remove(snd_var.task, snd_dsp_cmd_port_name());
		dsp_var.flags |= F_NOCMDPORT;
	}

	if (!(dsp_var.flags&F_NOQUEUE))
		dspq_execute();
}

/*
 * See if the first thing in the queue has the appropriate condition to
 * be executed.
 */
boolean_t dspq_check(void)
{
	register dsp_msg_t *msg;
	union {
		int		r_int;
		struct dsp_regs	r_struct;
	} lregs;
	int s = spldsp();

	if (dsp_var.state == dsp_aborted) {
	    dspdbug2(("dspq_check: DSP has aborted\n"));
	    splx(s);
	    return FALSE;
	}

	/*
	 * See if there's any asynchronous events to execute.
	 */
	if (dsp_var.event_msg) {
		if (cpu_type != MC68030) {
			lregs.r_struct.icr = regs->icr;
			lregs.r_struct.cvr = regs->cvr;
			lregs.r_struct.isr = regs->isr;
			lregs.r_struct.ivr = regs->ivr;
		} else {
			lregs.r_int = *(int *)regs;
		}
		if (   (lregs.r_int&dsp_var.event_mask)
		    == dsp_var.event_condition)
		{
			splx(s);
			return TRUE;
		}
	}

	/*
	 * See if we have sound-in data to send to dsp
	 */
	if (   (dsp_var.flags&F_SOUNDDATA)
	    && dsp_var.chan[DSP_SI_CHAN]
	    && !queue_empty(&dsp_var.chan[DSP_SI_CHAN]->ddp_q)
	    && (regs->isr&ISR_TXDE))
	{
		splx(s);
		return TRUE;
	}
	/*
	 * Find out if the first thing in the queue can be done now.
	 */
	if (queue_empty(&dsp_var.cmd_q)) {
		dspdbug2(("dspq_check: empty queue (%d)\n",
			dsp_var.cmd_q_size));
		splx(s);
		return FALSE;
	}

	/*
	 * There's something there, see if conditions match.
	 */
	msg = (dsp_msg_t *)queue_first(&dsp_var.cmd_q);
	switch (msg->type) {
	case condition:
		if (cpu_type != MC68030) {
			lregs.r_struct.icr = regs->icr;
			lregs.r_struct.cvr = regs->cvr;
			lregs.r_struct.isr = regs->isr;
 			lregs.r_struct.ivr = regs->ivr;
		} else {
			lregs.r_int = *(int *)regs;
		}
		if (   (lregs.r_int&msg->u.condition.mask)
		    != msg->u.condition.flags)
		{
			dspdbug2(("dspq_check: await 0x%x&0x%x == 0x%x\n",
				lregs.r_int, msg->u.condition.mask,
				msg->u.condition.flags));
			splx(s);
			return FALSE;
		}
		dspdbug(("dspq_check: cond 0x%x&0x%x == 0x%x satisfied\n",
			lregs.r_int, msg->u.condition.mask,
			msg->u.condition.flags));
		break;
	case data1:
	case data2:
	case data3:
	case data4:
		if (!(regs->isr&ISR_TXDE))
		{
			dspdbug2(("dspq_check: await txde\n"));
			regs->icr |= ICR_TREQ; /* cleared in dsp_dev_intr() */
			splx(s);
			return FALSE;
		}
 		/* Necessary escape for SYS_CALL datum in penddmaout0 state */
  		if (   dsp_var.state == penddmaout1
		    && msg->sequence != DSP_MSG_FIRST
		    && msg->priority == DSP_MSG_HIGH)
		{
		    dspdbug2(("dspq_check: arg data bypassing penddmaout0\n"));
		    break;
		}
		if (dsp_var.state == penddmaout0
		    || dsp_var.state == penddmaout1
		    || dsp_var.state == dmaout)
		{
			dspdbug2(("dspq_check: data awaiting dma out\n"));
			splx(s);
			return FALSE;
		}
		break;
	case rdata1:
	case rdata2:
	case rdata3:
	case rdata4:
		if (!(regs->isr&ISR_RXDF))
		{
			dspdbug2(("dspq_check: await rxdf\n"));
			regs->icr |= ICR_RREQ; /* cleared in dsp_dev_intr() */
			splx(s);
			return FALSE;
		}
		if (dsp_var.state == penddmain1
		    || dsp_var.state == dmain
		    || dsp_var.state == postdmain)
		{
			dspdbug2(("dspq_check: rdata awaiting dma in\n"));
			splx(s);
			return FALSE;
		}
		break;
	case dma_out:
	case dma_in:
		if (dsp_var.state != normal) {
			splx(s);
			dspdbug2(("dspq_check: "
				"dma awaiting normal state\n"));
			return FALSE;
		}
		break;
	case host_command:
		if (regs->cvr&CVR_HC)
		{
			dspdbug2(("dspq_check: await !hc\n"));
			splx(s);
			return FALSE;
		}
		break;
	case reset:
		if (dsp_var.state != normal) {
			splx(s);
			dspdbug2(("dspq_check: "
				  "reset awaiting normal state\n"));
			return FALSE;
		}
		break;
	case host_flag:
	case ret_msg:
	case driver_state: /* assumed enqueued ONLY BY DRIVER */
		break;
	}

	splx(s);
	return TRUE;
}

/*
 * Execute everything in the queue that can be executed.
 */
void dspq_execute(void)
{
	register dsp_msg_t *msg;
	register int s;
	static int execute_ipl = -1;
	int old_ipl;
	int rdata_complete = 0;
	union {
		int		r_int;
		struct dsp_regs	r_struct;
	} lregs;

	if (execute_ipl == curipl()) {
		dspdbug(("dspq_execute: old ipl %d, cur %d exit\n",
			execute_ipl, curipl()));
		return;
	}

	ASSERT(execute_ipl < curipl());
	old_ipl = execute_ipl;
	execute_ipl = curipl();

	/*
	 * Execute everything that can be from the front of the stack.
	 */
	dspdbug(("dspq_execute: %d messages queued\n", dsp_var.cmd_q_size));
	dspdbug2(("dspq_execute enter:" DSPREGSFMT));
	
	/*
	 * Check for async event.
	 */
	s = spldsp();

	if (cpu_type != MC68030) {
		lregs.r_struct.icr = regs->icr;
		lregs.r_struct.cvr = regs->cvr;
		lregs.r_struct.isr = regs->isr;
	} else {
		lregs.r_int = *(int *)regs;
	}
	if (   dsp_var.event_msg
	    && (lregs.r_int&dsp_var.event_mask) == dsp_var.event_condition)
	{
		dspdbug(("dspq_ex: event_msg %x\n", dsp_var.event_msg));
		softint_sched(CALLOUT_PRI_THREAD, dspq_msg_send, 
			      dsp_var.event_msg);
		dsp_var.event_msg = 0;
	}

	while (!queue_empty(&dsp_var.cmd_q) && dspq_check()) {
		queue_remove_first(&dsp_var.cmd_q, msg, dsp_msg_t *, link);
		
		switch (msg->type) {
		case condition:
			/*
			 * A condition found here must be true.
			 */
			/*
			 * If there's a reply_port specified,
			 * return the dsp regs to the user.
			 */
			if (msg->u.condition.reply_port) {
				dspdbug(("dspq_check: ret_cond_true\n"));
				if (cpu_type != MC68030) {
					lregs.r_struct.icr = regs->icr;
					lregs.r_struct.cvr = regs->cvr;
					lregs.r_struct.isr = regs->isr;
					lregs.r_struct.ivr = 0;
				} else {
					lregs.r_int = *(int *)regs;
				}
				msg->scratch = lregs.r_int;
				softint_sched(CALLOUT_PRI_THREAD,
						snd_reply_dsp_cond_true,
						(int)msg);
			}
			break;
		case data1: {
			int size = msg->u.data.size -
				(msg->u.data.loc - msg->u.data.addr);
			dspdbug(("dspq_ex: %d bytes ", size));
			while (size>0 && !xmit(*((char *)msg->u.data.loc)++)) {
				dspdbug2(("0x%x ",
					*(((char *)msg->u.data.loc)-1)));
				size--;
			}
			if (size) {
				((char *)msg->u.data.loc)--;
				regs->icr |= ICR_TREQ; // dsp_dev_intr() clears
				goto push;
			}

			dspdbug(("\n"));
			break;
		}	
		case data2: {
			int size = (msg->u.data.size -
				(msg->u.data.loc - msg->u.data.addr)) / 2;
			dspdbug(("dspq_ex: %d shorts ", size));
			while (size>0 && !xmit(*((short *)msg->u.data.loc)++))
			{
				dspdbug2(("0x%x ",
					*(((short *)msg->u.data.loc)-1)));
				size--;
			}
			if (size) {
				((short *)msg->u.data.loc)--;
				regs->icr |= ICR_TREQ; // dsp_dev_intr() clears
				goto push;
			}

			dspdbug(("\n"));
			break;
		}
		case data3: {
			unsigned char *bp = (unsigned char *)msg->u.data.loc;
			int bsize = (  msg->u.data.size
				     - (msg->u.data.loc - msg->u.data.addr));

			dspdbug(("dspq_ex: %d pack24's ", bsize/3));
			while (bsize>0) {
				unsigned int val = 0;
				val = *bp++;
				val = (val<<8)|*bp++;
				val = (val<<8)|*bp++;
				if (xmit(val))
					break;
				dspdbug2(("0x%x ",val));
				bsize -= 3;
			}
			if (bsize > 0) {
				bp--;
				((char *)msg->u.data.loc) = bp;
				regs->icr |= ICR_TREQ; // dsp_dev_intr() clears
					goto push;
			}
			dspdbug(("\n"));
			break;
		}
		case data4: {
			int size = (msg->u.data.size -
				(msg->u.data.loc - msg->u.data.addr)) / 4;
			dspdbug(("dspq_ex: %d ints ", size));
			while (size>0 && !xmit(*((int *)msg->u.data.loc)++)) {
				dspdbug2(("0x%x ",
					*(((int *)msg->u.data.loc)-1)));
				size--;
			}
			dspdbug2(("After ints written:" DSPREGSFMT));
			if (size) {
				((int *)msg->u.data.loc)--;
				regs->icr |= ICR_TREQ; // dsp_dev_intr() clears
				goto push;
			}

			dspdbug(("\n"));
			break;
		}
		case rdata1: 
		case rdata2:
		case rdata3:
		case rdata4:
		{
			/*
			 * Read data from DSP.
			 */
			int rsize = msg->type == rdata1 ? 1 :
				    msg->type == rdata2 ? 2 :
				    msg->type == rdata3 ? 3 : 4;
			int bsize = (msg->u.data.size -
				(msg->u.data.loc - msg->u.data.addr));
			void *dloc = (void *)msg->u.data.loc;

			dspdbug(("dspq_ex: read %d %s ", bsize/rsize, 
				rsize == 1 ? "bytes" : 
				rsize == 2 ? "shorts" : 
				rsize == 3 ? "packed-24's" : "ints"));

			while (bsize > 0 && (!recv((int *)dloc, rsize))) {
				(char *)dloc += rsize;
				bsize -= rsize;
				dspdbug2(("0x%x ",
					rsize == 1 ? *(((char *)dloc)-1) :
					rsize == 2 ? *(((short *)dloc)-1) :
					rsize == 3 ?
						(((*(((char *)dloc)-3)<<8)
						 |(*(((char *)dloc)-2)))<<8)
						  |(*(((char *)dloc)-1))
					: *(((int *)dloc)-1)));
			}
			(void *)msg->u.data.loc = dloc;

			if (bsize)
				goto push;
			else
				rdata_complete = 1;
						
			dspdbug(("\n"));
			break;
		}	
		case dma_out:
		case dma_in: {
			struct chan_data *cdp = dsp_var.chan[msg->u.dma.chan];
			ASSERT(cdp);
			dsp_var.pend_dma_chan = msg->u.dma.chan;
			dsp_var.pend_dma_addr = msg->u.dma.addr;
			cdp->buf_size = msg->u.dma.size;
			cdp->skip = msg->u.dma.skip;
			cdp->space = msg->u.dma.space;
			cdp->mode = msg->u.dma.mode;
			if (msg->type == dma_out)
			  dsp_var.state = penddmaout0;
			else
			  dsp_var.ui_dma_read_state = UIR_PENDING;
			/* We stay in normal state for user-initiated DMA
			   reads because the DSP can still drive us into
			   state penddmain1 with a DSP_dm_R_REQ message. 
			   Therefore, we cannot make use of the state
			   sequence until that point in the protocol
			   when a DSP_dm_R_REQ message comes in on channel
			   0 to satisfy this request.
			 */
			dspdbug(("dspq_ex: %s %d words.\n",
				msg->type == dma_out ? 
				 "dmaout --> penddmaout0" : 
				 "dmain --> uir_pending",
				msg->u.dma.size));
			if (!(queue_empty(&cdp->ddp_q))) {
			    dspdbug(("dspq_ex: buf ready. set TREQ.\n"));
			    regs->icr |= ICR_TREQ; /* get into dev_loop */
			}
			break;
		}
		case host_command:
			dspdbug(("dspq_ex: host_cmnd 0x%x\n",
				msg->u.host_command));
			regs->cvr = msg->u.host_command|CVR_HC;
			break;

		case protocol:
			dspdbug(("dspq_ex: new protocol = 0x%x\n",
				    msg->u.new_protocol));
			dsp_dev_new_proto(msg->u.new_protocol);
			break;

		case host_flag: {
			lregs.r_struct.icr = regs->icr;
			lregs.r_struct.cvr = regs->cvr;
			dspdbug(("dspq_ex:hf:%x%x????&~%x|%x",
				lregs.r_struct.icr,
				lregs.r_struct.cvr,
				msg->u.host_flag.mask,
				msg->u.host_flag.flags));
			lregs.r_int &= ~msg->u.host_flag.mask;
			lregs.r_int |= msg->u.host_flag.flags;
			regs->icr = lregs.r_struct.icr;
			regs->cvr = lregs.r_struct.cvr;

			/*
			 * If we set the ICR_INIT bit, wait for it to clear.
			 */
			while (regs->icr&ICR_INIT)
				DELAY(1);
			dspdbug(("=%02x%02x%02x%02x\n", 
				 regs->icr,
				 regs->cvr,
				 regs->isr,
				 regs->icr));
			break;
		}
		case ret_msg:
			/*
			 * FIXME: this must be send from a thread.
			 */
			dspdbug(("dspq_ex: ret_msg %x\n", msg));
			softint_sched(CALLOUT_PRI_THREAD, dspq_msg_send, msg);
			continue;	// don't deallocate msg
		case reset:
			dspdbug(("dspq_ex: reset\n"));
			/*
			 * Reset the DSP interface.
			 */
			dsp_dev_reset_chip();
			/* splx done at end of switch */
			DELAY(2);
			if ((dsp_var.flags & 
	 		     (F_MODE_DSPERR|F_MODE_DSPMSG|F_MODE_C_DMA))
				|| !(dsp_var.flags & F_MODE_RAW))
	  			regs->icr = ICR_RREQ;
			break;
		case ret_regs:
			dspdbug(("dspq_ex: ret_regs\n"));
			if (cpu_type != MC68040) {
				lregs.r_struct.icr = regs->icr;
				lregs.r_struct.cvr = regs->cvr;
				lregs.r_struct.isr = regs->isr;
				lregs.r_struct.ivr = regs->ivr;
			} else {
				lregs.r_int = *(int *)regs;
			}
			softint_sched(CALLOUT_PRI_THREAD, snd_reply_dsp_regs,
				     lregs.r_int);
			break;
		case driver_state:
			dspdbug(("dspq_ex: new state %d\n",
				msg->u.driver_state));
			dsp_var.state = msg->u.driver_state;
			break;
		}
		splx(s);
/*
 *		If we've finished an RDATA message, schedule reply
 */
		if (rdata_complete) {
			if (msg->u.data.reply_port != PORT_NULL) {
				/* PORT_NULL means flush DSP output */
				if (curipl()) {
					softint_sched(CALLOUT_PRI_THREAD,
						dspq_reply_rdata,
						(int)msg);
					dspdbug(("dspq_ex: callout return of "
						    "%d rdata bytes to user\n",
						    (int) msg->u.data.size));
				} else {
					dspq_reply_rdata(msg);
					dspdbug(("dspq_ex: direct return of "
						    "%d rdata bytes to user\n",
						    (int) msg->u.data.size));
				}
				continue;
			} else {
				dspdbug(("dspq_ex: TOSSED %d bytes\n",
						    (int) msg->u.data.size))
			}
			rdata_complete = 0;
		}

		/*
		 * Deallocate the message.
		 */
		dspq_free_msg(msg);
		s = spldsp();
	}

	goto out;

	/*
	 * Put this message back in the queue
	 */
    push:
    	dspdbug2(("dspq_ex: push msg %x\n", msg));
	switch (msg->sequence) {
	case DSP_MSG_ONLY:
	case DSP_MSG_FIRST:
		msg->sequence |= DSP_MSG_STARTED;
	case DSP_MSG_MIDDLE:
		msg->sequence = DSP_MSG_STARTED|DSP_MSG_FIRST;
	case DSP_MSG_LAST:
		msg->sequence = DSP_MSG_STARTED|DSP_MSG_ONLY;
	}
	queue_enter_first(&dsp_var.cmd_q, msg, dsp_msg_t *, link);
 
 	splx(s);
   	dspdbug2(("dspq_ex exit(push):" DSPREGSFMT));
	execute_ipl = old_ipl;
	return;

    out:
	/*
	 * Make sure we're not in the middle of some host message.
	 */
	msg = (dsp_msg_t *)queue_first(&dsp_var.cmd_q);
	if (   !queue_empty(&dsp_var.cmd_q)
	    && !(   msg->sequence == DSP_MSG_ONLY
		 || msg->sequence == DSP_MSG_FIRST
		 || (!msg->atomic && !(msg->sequence&DSP_MSG_STARTED))))
	{
		splx(s);
    		dspdbug2(("dspq_ex exit(atomic):" DSPREGSFMT));
		execute_ipl = old_ipl;
		return;
	}

	splx(s);
	/*
	 * See if we have sound-in data to send to dsp
	 */
	if (   (dsp_var.flags&F_SOUNDDATA)
	    && dsp_var.chan[DSP_SI_CHAN]
	    && !queue_empty(&dsp_var.chan[DSP_SI_CHAN]->ddp_q))
	{
		int i;
		/*
		 * For some unknown reason after initing the interface
		 * it takes an ungodly amount of time for TXDE to go true,
		 * we'll wait for it here a bit.
		 */
		for (i = 40; i && !(regs->isr&ISR_TXDE); i--)
			DELAY(1);
		if ((regs->isr&ISR_TXDE))
			dspq_output_stream(dsp_var.chan[DSP_SI_CHAN]);
	}

    	dspdbug2(("dspq_ex exit:" DSPREGSFMT));
	execute_ipl = old_ipl;
}

static kern_return_t dspq_reply_rdata(dsp_msg_t *msg)
{
	kern_return_t r;
	port_t reply_port;
    
	dspdbug(("dspq_reply_rdata: enter: ipl = %d: %d bytes to port %d\n",
		    curipl(), (int) msg->u.data.size, msg->u.data.reply_port));
	ASSERT(curipl() == 0);
	ASSERT(    (int)msg->u.data.size
		<=    MSG_SIZE_MAX
		    - sizeof(snd_recorded_data_t)
		    + sizeof(pointer_t));
    
	/*
	 * If we're called from within the sound task, we can just send to the
	 * reply port.  Otherwise, we need to translate the port into the
	 * kernel name space.
	 */
	if (current_task() != snd_var.task) {
		if (!object_copyin(snd_var.task, msg->u.data.reply_port,
				    MSG_TYPE_PORT, FALSE,
				    &(kern_obj_t)reply_port))
			return KERN_FAILURE;
	} else
		reply_port = msg->u.data.reply_port;

	r = snd_reply_recorded_data(reply_port, 0, msg->u.data.addr, 
		(int) msg->u.data.size, TRUE);

	dspdbug(("dspq_ex: Sent %d rdata bytes to user\n",
		(int)msg->u.data.size));
	/*
	 * Deallocate the message.
	 */
	dspq_free_msg(msg);

#if	DEBUG
	if (r != KERN_SUCCESS) {
		dspdbug(("*** dspq_reply_rdata: snd_reply_recorded_data "
			"returns %d\n",r));
	}
#endif	DEBUG

	return r;
}

void dspq_free_msg(dsp_msg_t *msg)
{
	int s;

	if (msg && msg->internal) {
		dsp_var.cmd_q_size--;
		dspdbug2(("dspq_free_msg: lmsg %d messages queued\n",
			dsp_var.cmd_q_size));
		dspq_free_lmsg(msg);	// get this free ASAP
		return;
	}

	if (curipl()) {
		int s = spldsp();
		dspdbug2(("dspq_free_msg: ipl too high\n"));
		ASSERT(msg);
		queue_enter(&msg_free_q, msg, dsp_msg_t *, link);
		splx(s);
		softint_sched(CALLOUT_PRI_THREAD, dspq_free_msg, NULL);
		return;
	}

	/*
	 * Free any enqueued messages needing to be freed.
	 */
	if (msg == NULL) {
		s = spldsp();
		while (!queue_empty(&msg_free_q)) {
			queue_remove_first(&msg_free_q, msg, dsp_msg_t *,
					  link);
			splx(s);
			dspq_free_msg(msg);
			s = spldsp();
		}
		splx(s);
		return;
	}

	dsp_var.cmd_q_size--;
	dspdbug2(("dspq_free_msg: msg %d messages queued\n",
		dsp_var.cmd_q_size));

	if (msg->datafollows) {
		int size = sizeof(dsp_msg_t);

		switch (msg->type) {
		case data1:
		case data2:
		case data3:
		case data4:
			size += msg->u.data.size;
			break;
		case ret_msg:
			size += msg->u.ret_msg->msg_size;
			break;
		}
		kfree((caddr_t)msg, size);
	} else {
		switch (msg->type) {
		case data1:
		case data2:
		case data3:
		case data4:
			/*
			 * Unwire the pages passed from the kernel map.
		 	 */
			(void)vm_map_pageable(kernel_map,
				trunc_page(msg->u.data.addr),
				round_page(  msg->u.data.addr
					   + msg->u.data.size),
				TRUE);

			/*
			 * Deallocate the data.
			 */
			(void)vm_deallocate(kernel_map,
				(vm_offset_t)msg->u.data.addr,
				(vm_size_t)msg->u.data.size);
			break;
		}
		kfree((caddr_t)msg, sizeof(dsp_msg_t));
	}

	if (   dsp_var.cmd_q_size < DSP_CMD_Q_MAX
	    && (dsp_var.flags&F_NOCMDPORT))
	{
		dspdbug(("dspq_enqueue: re-enable cmd port (%d msgs)\n",
			dsp_var.cmd_q_size));
		port_set_add(snd_var.task, snd_var.portset,
			snd_dsp_cmd_port_name());
		dsp_var.flags &= ~F_NOCMDPORT;
	}
}

static void dspq_msg_send(dsp_msg_t *msg)
{
	port_t remote_port = msg->u.ret_msg->msg_remote_port;

	ASSERT(msg->type == ret_msg);
	ASSERT(curipl() == 0);
	object_copyin(snd_var.task, remote_port,
		MSG_TYPE_PORT, FALSE, &msg->u.ret_msg->msg_remote_port);
	msg_send_from_kernel(msg->u.ret_msg, MSG_OPTION_NONE, 0);
//	port_deallocate(snd_var.task, remote_port);
	port_release(msg->u.ret_msg->msg_remote_port);
	dspq_free_msg(msg);
}
	
/*
 * Return an indication of the conditions that the first thing at the
 * front of the queue is waiting for.
 */
int dspq_awaited_conditions(void)
{
	int s = spldsp();
	int rcode = 0;
	register dsp_msg_t *msg = (dsp_msg_t *)queue_first(&dsp_var.cmd_q);

	if (!queue_empty(&dsp_var.cmd_q))
		switch (msg->type) {
		case condition:
			if (msg->u.condition.mask&((ISR_HF2|ISR_HF3)<<8))
				rcode |= F_AWAITBIT;
			if (msg->u.condition.mask&((ISR_TRDY|ISR_TXDE)<<8))
				rcode |= F_AWAITTXDE;
			if (msg->u.condition.mask&((ISR_RXDF)<<8))
				rcode |= F_AWAITRXDF;
			if (msg->u.condition.mask&((CVR_HC)<<16))
				rcode |= F_AWAITHC;
			break;
		case data1:
		case data2:
		case data3:
		case data4:
			rcode |= F_AWAITTXDE;
			break;
		case rdata1:
		case rdata2:
		case rdata3:
		case rdata4:
			rcode |= F_AWAITRXDF;
			break;
		case host_command:
			rcode |= F_AWAITHC;
			break;
		}

	/*
	 * If nothing else, see if we have sound data that needs to go out.
	 */
	if (   rcode == 0
	    && (dsp_var.flags&F_SOUNDDATA)
	    && dsp_var.chan[DSP_SI_CHAN]
	    && !queue_empty(&dsp_var.chan[DSP_SI_CHAN]->ddp_q))
	  rcode |= F_AWAITTXDE;

	splx(s);
	return rcode;
}

/*
 * Accept a dma buffer for reading.
 * Simple protocol.
 */
boolean_t dspq_start_simple(snd_dma_desc_t *ddp, int direction, int rate)
{
	int chan = (direction == SND_DIR_PLAY) ? DSP_SI_CHAN : DSP_SO_CHAN;
	int s;
	dspdmadbug(("dspq_start_s: ddp 0x%x, dir %d chan %d\n", ddp, direction,
		 chan));

	if (dsp_var.flags&F_SHUTDOWN)
		return 0;

	s = spldsp();
	if (direction == SND_DIR_RECORD) {
		ddp->hdr.dh_flags = 0;
		dma_enqueue(&dsp_var.dma_chan, &ddp->hdr);
	} else {
		ASSERT(dsp_var.chan[chan]);
		queue_enter(&dsp_var.chan[chan]->ddp_q, ddp,
			snd_dma_desc_t *, link);
	}

	/*
	 * See if we can output some more.
	 */
	if (   ((dsp_var.flags&F_SOUNDDATA) || !queue_empty(&dsp_var.cmd_q))
	    && dspq_check())
		softint_sched(CALLOUT_PRI_SOFTINT1, dspq_execute, 0);
	splx(s);
	return 1;
}

/*
 * Accept a dma buffer for reading or writing.
 * Complex protocol.
 */
boolean_t dspq_start_complex(snd_dma_desc_t *ddp, int direction, int rate)
{
	register struct chan_data *cdp =
		(struct chan_data *)((snd_region_t *)ddp->ptr)->sound_q->ptr;
	register int s = spldsp();
	int channel;

	if (dsp_var.flags&F_SHUTDOWN)
		return 0;

	for ( channel = 0
	    ; channel < DSP_N_CHAN && cdp != dsp_var.chan[channel]
	    ; channel++)
		;
	ASSERT(channel < DSP_N_CHAN);

	dspdmadbug(("dspq_start_c: cdp 0x%x ddp 0x%x, dir %d channel %d\n", 
		    cdp, ddp, direction, channel));
	ASSERT(ddp->size == cdp->buf_size*mode_size(cdp->mode) && cdp);

	/*
	 * Add this thing to the queue.
	 */
	queue_enter(&cdp->ddp_q, ddp, snd_dma_desc_t *, link);

#if	JOSBUG
 	if (   (direction == SND_DIR_PLAY && dsp_var.state == penddmaout0)
 	    || (direction == SND_DIR_RECORD && dsp_var.state == penddmain1))
	{
		/*
		 * Kick dsp_dev_loop() off with an interrupt to 
		 * start the first dma operation.
		 */
		regs->icr |= ICR_TREQ;
		dspdbug2(("dspq_start_c: play: "
			"set TREQ to get into dev_loop: " DSPREGSFMT));
	}
#else	JOSBUG
	if (direction == SND_DIR_PLAY) {
		/*
		 * Kick dsp_dev_loop() off with an interrupt to 
		 * start the first dma operation.
		 */
	    regs->icr |= ICR_TREQ;
	    dspdbug2(("dspq_start_c: play: "
		      "set TREQ to get into dev_loop: " DSPREGSFMT));
	}
#endif	JOSBUG
	else {
		/*
		 * Enable receive interrupts again if waiting for DSP
		 * to send the read-request message.
		 */
		if (dsp_var.state != penddmain1 && dsp_var.state != dmain)
			regs->icr |= ICR_RREQ; /* Complex DMA mode */

		if ( dsp_var.ui_dma_read_state == UIR_PENDING
		    || (( dsp_var.state == penddmain1 )
			&& channel == dsp_var.pend_dma_chan))
		{
			dspdbug2(("dspq_start_c: read: got our region, "
				  "call dev_loop\n"));
			dsp_dev_loop();
		}
	}

	/*
	 * See if we can output some more.
	 */
	if (   ((dsp_var.flags&F_SOUNDDATA) || !queue_empty(&dsp_var.cmd_q))
	    && dspq_check())
		softint_sched(CALLOUT_PRI_SOFTINT1, dspq_execute, 0);

	splx(s);
	return 1;
}

/*
 * Stream data on the given channel down to the DSP as fast as it will
 * take it.
 */
static void dspq_output_stream(struct chan_data *cdp)
{
	int s;
	register snd_dma_desc_t *ddp;
	register queue_t q = &cdp->ddp_q;
#if	DEBUG
	int byte_cnt = 0;
#endif	DEBUG
	static int output_ipl = -1;
	int old_ipl;

	if (output_ipl == curipl()) {
		dspdbug(("dspq_execute: old ipl %d, cur %d exit\n",
			output_ipl, curipl()));
		return;
	}

	ASSERT(output_ipl < curipl());
	old_ipl = output_ipl;
	output_ipl = curipl();

	s = spldsp();
	dspdbug(("dspq_o_s: ddp 0x%x, enter..\n", queue_first(&cdp->ddp_q)));
	if (cdp->mode == DSP_MODE8) {
	    while (!queue_empty(q)) {
	    	register u_char *cp;
		register int len;
		ddp = (snd_dma_desc_t *)queue_first(q);

		/*
		 * Complete completed descriptors.
		 */
		if (ddp->hdr.dh_start == ddp->hdr.dh_stop) {
			queue_remove(q, ddp, snd_dma_desc_t *, link);
			dspdbug(("\ndspq_o_s: free ddp 0x%x, next 0x%x\n", ddp,
				queue_first(q)));
			cdp->sndq.nxfer += ddp->size;
			if (queue_empty(q))
				ddp->flag |= SDD_OVERFLOW;
			(*ddp->free)(ddp);
			continue;
		}
		cp = (u_char *)ddp->hdr.dh_start;
		len = (u_char *)ddp->hdr.dh_stop - cp;
		if (len > 256) len = 256;
		while (len--) {
			if (xmit(*cp++)) {
				cp--;
				break;
			}
#ifdef	DEBUG
			byte_cnt++;
#endif	DEBUG
		}

		/*
		 * We couldn't get anything out, go do something useful.
		 */
		if (ddp->hdr.dh_start == (char *)cp)
			break;
		ddp->hdr.dh_start = (char *)cp;
		splx(s);
		s = spldsp();
	    }
	} else {
	    while (!queue_empty(q)) {
	    	register u_short *sp;
		register int len;
		ddp = (snd_dma_desc_t *)queue_first(q);

		/*
		 * Complete completed descriptors.
		 */
		if (ddp->hdr.dh_start == ddp->hdr.dh_stop) {
			queue_remove(q, ddp, snd_dma_desc_t *, link);
			dspdbug(("\ndspq_o_s: free ddp 0x%x, next 0x%x\n", ddp,
				queue_first(q)));
			cdp->sndq.nxfer += ddp->size;
			if (queue_empty(q))
				ddp->flag |= SDD_OVERFLOW;
			(*ddp->free)(ddp);
			continue;
		}
		sp = (u_short *)ddp->hdr.dh_start;
		len = (u_short *)ddp->hdr.dh_stop - sp;
		if (len > 64) len = 64;
		while (len--) {
			if (xmit(*sp++)) {
				sp--;
				break;
			}
#ifdef	DEBUG
			byte_cnt += 2;
#endif	DEBUG
		}

		/*
		 * We couldn't get anything out, go do something useful.
		 */
		if (ddp->hdr.dh_start == (char *)sp)
			break;
		ddp->hdr.dh_start = (char *)sp;
		splx(s);
		s = spldsp();
	    }
	}
	dspdbug(("dspq_o_s: exit %d bytes sent\n", byte_cnt));
	output_ipl = old_ipl;
	splx(s);
}

#endif	NDSP > 0
