/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *  7-Oct-90  Gregg Kellogg (gk) at NeXT
 *	Change assertion to error return if doing a stream operation on a
 *	channel that hasn't been initialized yet (bug 5567).
 *
 * 28-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Take out hack to deallocate our remaining VM space when shutting down.
 *	(It's left in for debugging purposes).
 *
 * 22-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Change protocol to not pass owner port in msg_remote_port field.
 *
 *  1-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Added stream_ndma message to set the number of dma descriptors
 *	used on a stream.
 *
 * 20-Jul-90  Matt Christiano (mchristo) at NeXT
 *	Added set ramp message
 *
 *  9-May-90  Matt Christiano (mchristo) at NeXT
 *	Miscellaneous changes for DMA out to DSP and user initiated DMA.
 *
 * 28-Aug-89  Gregg Kellogg (gk) at NeXT
 *	In snd_port_gone, when all of the owner ports have been deallocated,
 *	and all resources _should_ be freed the remaining portion of the
 *	task virtual memory space is deleted.  This should never happen,
 *	and hardly ever does.
 *
 * 25-Jun-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	fixed a bug in which snd_stream_abort was returning a multiple
 *	reply to this RPC.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sound.h>
#if	NSOUND > 0

/*
 * Kernel Sound server.
 *
 * This implements the server interface to the sound driver (DSP DAC/ADC).
 *
 * The server is accessed using several different ports
 *	Device port - used to gain further access to sound facilities
 *		and control other sound-related parameters.
 *
 *	Stream port - used for playing/recording data for a specific
 *		destination.  There are several ports for access to
 *		the DSP, one for sound input from the CODEC and two for
 *		each output rate to the DAC's.
 *
 *	DSP command port - used for sending command streams to the DSP.
 *
 * To access these services one or more ownership rights must be established:
 * to access the DAC's a port must be registered as the "sound out owner".
 * This port is used to allocate the sound out stream ports.  When this port
 * is destroyed internal resources are deallocated and the device is reset.
 * There also exist ownership ports for the DSP and sound in.
 */
#import <sys/notify.h>
#import <kern/kern_port.h>
#import <kern/ipc_copyin.h>
#import <kern/ipc_copyout.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_snd.h>
#import <nextdev/snd_dsp.h>
#import <kern/host.h>
#import <kern/xpr.h>

#if	XPR_DEBUG
extern int sdbgflag;
#define snd_dbug(f) {if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#define snd_dbug2(f) if (sdbgflag&2){if (sdbgflag&1)printf f; else XPR(XPR_SOUND, f);}
#else	XPR_DEBUG
#define snd_dbug(f)
#define snd_dbug2(f)
#endif	XPR_DEBUG

snd_var_t snd_var;
extern ll_snd_var_t ll_snd_var;
extern dsp_var_t dsp_var;

/*
 * Locally used functions
 */
static int snd_device_port_msg(msg_header_t *msg);
static int snd_stream_port_msg(snd_stream_msg_t *msg);
static void snd_port_gone(port_name_t port);

static void snd_port_alloc(port_name)
{
	port_name_t new_port;

	if (port_allocate(snd_var.task, &new_port) != KERN_SUCCESS)
		panic("sound: can't allocate port");

	if (new_port != port_name)
	    if (port_rename(snd_var.task, new_port, port_name) != KERN_SUCCESS)
		panic("sound: can't rename port");

	if (port_set_add(snd_var.task, snd_var.portset, port_name))
		panic("sound: can't add port to port_set");
}


void snd_server_loop(void)
{
#if	DEBUG
	int foo[100];
	snd_server_loop1();
}
snd_server_loop1()
{
	int foo[100];
	snd_server_loop2();
}
snd_server_loop2()
{
#endif	DEBUG
	msg_return_t	r;
	port_name_t	my_notify;
	port_name_t	new_port;
	kern_port_t	kport;
	vm_offset_t	messages;
	register
	msg_header_t	*in_msg;
	int		in_msg_size;
	int		error;
	register int	i;

	/*
	 *	Find out who we are.
	 */

	snd_var.task = current_task();
	snd_var.task->kernel_vm_space = TRUE;
	snd_var.task->kernel_ipc_space = FALSE;
	current_thread()->ipc_kernel = TRUE;

	snd_var.task_port = task_self();
	snd_var.task_map = current_task()->map;

	r = port_set_allocate(snd_var.task, &snd_var.portset);
	if (r != KERN_SUCCESS)
		panic("sound: can't allocate port set");

	r = port_allocate(snd_var.task, &my_notify);
	if (r != KERN_SUCCESS)
		panic("sound: can't allocate port notify port");

	if (!object_copyin(snd_var.task, my_notify,
			   MSG_TYPE_PORT, FALSE,
			   (kern_obj_t *) &kport))
		panic("sound: object_copyin(notify) failed");

	r = task_set_special_port(snd_var.task, TASK_NOTIFY_PORT,
		(port_t)kport);
	if (r != KERN_SUCCESS)
		panic("sound: task_set_special_port(notify)");

	r = port_set_add(snd_var.task, snd_var.portset, my_notify);
	if (r != KERN_SUCCESS)
		panic("sound: port_set_add(notify)");

	object_copyout(snd_var.task, realhost.host_priv_self,
			MSG_TYPE_PORT, &snd_var.host_priv_self);

	/*
	 * Allocate Device port.
	 */
	if (   port_allocate(snd_var.task, &snd_var.dev_port) != KERN_SUCCESS
	    ||    port_set_add(snd_var.task, snd_var.portset, snd_var.dev_port)
	       != KERN_SUCCESS)
		panic("sound: can't allocate device port\n");

	/*
	 *	Release kernel to continue.
	 */
	thread_wakeup((int) &snd_var.task_port);

	task_name("sound-device");

	/*
	 * Initialize lower levels.
	 */
	snd_device_init(SND_DIR_PLAY);
	snd_device_init(SND_DIR_RECORD);
	dsp_dev_init();
	dsp_dev_reset_hard();
	snd_stream_init();

	/*
	 *	Service loop... receive messages and process them.
	 */

	in_msg = snd_rcv_alloc_msg_frame();
	in_msg_size = in_msg->msg_size;
	for (;;) {
		/*
		 * Check for asynchronous activity requested by
		 * lower levels.
		 */

		/*
		 * Wait for the next request.
		 */
		in_msg->msg_local_port = snd_var.portset;
		in_msg->msg_size = in_msg_size;
		if (   (r = msg_receive(in_msg, MSG_OPTION_NONE, 0))
		    != RCV_SUCCESS)
		{
			if (r != RCV_INTERRUPTED) {
				printf("sound: receive failed, 0x%x.\n", r);
			}
			continue;
		}
		
		/*
		 * Dispatch the message based on it's port.
		 */
		if (in_msg->msg_local_port == snd_var.dev_port)
			error = snd_device_port_msg(in_msg);
		else if (in_msg->msg_local_port == snd_dsp_cmd_port_name())
			error = snd_dsp_cmd_port_msg(in_msg);
		else if (in_msg->msg_local_port == task_notify()) {
			notification_t	*n = (notification_t *) in_msg;
			switch(in_msg->msg_id) {
			case NOTIFY_PORT_DELETED:
				snd_port_gone(n->notify_port);
				break;
			default:
				printf("sound: wierd notification"
					" (%d) port = 0x%x.\n",
					in_msg->msg_id,
					n->notify_port);
				break;
			}
			continue;
		} else
			error =snd_stream_port_msg((snd_stream_msg_t *)in_msg);
		
		if (error)
			/*
			 * Always send a reply.
			 */
			(void) snd_reply_illegal_msg(PORT_NULL,
						in_msg->msg_remote_port,
						in_msg->msg_id, error);
	}
}

static int snd_device_port_msg(msg_header_t *msg)
{
	register int r = SND_NO_ERROR;

	switch (msg->msg_id) {
	case SND_MSG_GET_STREAM: {
		snd_get_stream_t *get_stream = (snd_get_stream_t *)msg;
		int local_port;
		if (msg->msg_size != sizeof(snd_get_stream_t))
			return SND_BAD_PARM;
		
		/*
		 * Make sure the message makes sense.
		 */
		if (   (  get_stream->stream
		        & ~(SND_GD_CHAN_MASK|SND_GD_DEVICE_MASK))
		    || (   snd_gd_isdevice(get_stream->stream)
			&&   snd_gd_chan(get_stream->stream)
			   > snd_gd_chan(SND_GD_SIN))
		    || (   !snd_gd_isdevice(get_stream->stream)
			&& snd_gd_chan(get_stream->stream) < DSP_SO_CHAN))
			return SND_BAD_PARM;

		/*
		 * Reply port must be owner port to get this guy.
		 */
		if (get_stream->stream == SND_GD_SIN) {
			if (get_stream->owner != snd_var.sndinowner)
				return SND_NOT_OWNER;
		} else if (snd_gd_isdevice(get_stream->stream)) {
			if (get_stream->owner != snd_var.sndoutowner)
				return SND_NOT_OWNER;
		} else {
			if (get_stream->owner != snd_var.dspowner)
				return SND_NOT_OWNER;
			if (!dsp_var.chan[snd_gd_chan(get_stream->stream)])
				return SND_BAD_CHAN;
		}
		
		local_port = (get_stream->stream | SND_PN_ASSIGNED);

		/*
		 * Return the port.
		 */
		(void) snd_reply_ret_stream(msg->msg_remote_port, local_port);
		r = 0;
		break;
	}
	case SND_MSG_SET_PARMS: {
		snd_set_parms_t *set_parms = (snd_set_parms_t *)msg;

		if (msg->msg_size != sizeof(snd_set_parms_t))
			return SND_BAD_PARM;

		snd_device_set_parms(set_parms->parms);
		break;
	}
	case SND_MSG_GET_PARMS: {
		snd_get_parms_t *get_parms = (snd_get_parms_t *)msg;

		if (msg->msg_size != sizeof(snd_get_parms_t))
			return SND_BAD_PARM;

		(void) snd_reply_ret_parms(msg->msg_remote_port,
				   snd_device_get_parms());
		r = 0;
		break;
	}
	case SND_MSG_SET_VOLUME: {
		snd_set_volume_t *set_volume = (snd_set_volume_t *)msg;

		if (msg->msg_size != sizeof(snd_set_volume_t))
			return SND_BAD_PARM;

		snd_device_set_volume(set_volume->volume);
		break;
	}
	case SND_MSG_GET_VOLUME: {
		snd_get_volume_t *get_volume = (snd_get_volume_t *)msg;

		if (msg->msg_size != sizeof(snd_get_volume_t))
			return SND_BAD_PARM;

		(void) snd_reply_ret_volume(msg->msg_remote_port,
				   snd_device_get_volume());
		r = 0;
		break;
	}
	case SND_MSG_SET_RAMP: {
		snd_set_parms_t *set_parms = (snd_set_parms_t *)msg;

		if (msg->msg_size != sizeof(snd_set_parms_t))
			return SND_BAD_PARM;

		snd_device_set_ramp(set_parms->parms);
		break;
	}
	case SND_MSG_SET_DSPOWNER: {
		snd_set_owner_t *set_owner = (snd_set_owner_t *)msg;
		register int i;
		if (msg->msg_size != sizeof(snd_set_owner_t))
			return SND_BAD_PARM;

		snd_dbug(("snd_device_port_msg: dsp owner 0x%x\n",
			set_owner->owner));
		if (snd_var.dspowner == set_owner->owner)
			dsp_dev_reset_hard();		// reset DSP
		else if (snd_var.dspowner != PORT_NULL) {
			/*
			 * Return the negotiation port to the user.
			 */
			(void) snd_reply_illegal_msg(snd_var.dspnegotiation,
						msg->msg_remote_port,
						msg->msg_id, SND_PORT_BUSY);
			return 0;
		} else {
			dsp_var.msgbuf = (u_int *)kalloc(
				DSP_DEF_BUFSIZE*sizeof(int));
			dsp_var.emsgbuf = dsp_var.msgbuf+DSP_DEF_BUFSIZE;
			/*
			 * Allocate stream ports.
			 */
			for (i = DSP_N_CHAN-1; i >= 0; i--)
				snd_port_alloc(snd_stream_port_name(i, 0));
			snd_port_alloc(snd_dsp_cmd_port_name());
		}

		snd_var.dspowner = set_owner->owner;
		snd_var.dspnegotiation = set_owner->negotiation;
		dsp_dev_init();
		dsp_var.msgp = dsp_var.msgbuf;
		break;
	}
	case SND_MSG_SET_SNDINOWNER: {
		snd_set_owner_t *set_owner = (snd_set_owner_t *)msg;
		if (msg->msg_size != sizeof(snd_set_owner_t))
			return SND_BAD_PARM;

		snd_dbug(("snd_device_port_msg: sndin owner 0x%x\n",
			set_owner->owner));
		/*
		 * A reset operation can be done without giving up
		 * ownership by setting the owner again.
		 */
		if (snd_var.sndinowner == set_owner->owner)
			snd_device_reset(SND_DIR_RECORD);
		else if (snd_var.sndinowner != PORT_NULL) {
			/*
			 * Return the owner of the port in the error message.
			 */
			(void) snd_reply_illegal_msg(snd_var.sndinnegotiation,
						msg->msg_remote_port,
						msg->msg_id, SND_PORT_BUSY);
			return 0;
		} else {
			snd_port_alloc(snd_stream_port_name(SND_GD_SIN, 1));
		}

		snd_var.sndinowner = set_owner->owner;
		snd_var.sndinnegotiation = set_owner->negotiation;
		snd_device_init(SND_DIR_RECORD);
		break;
	}
	case SND_MSG_SET_SNDOUTOWNER: {
		snd_set_owner_t *set_owner = (snd_set_owner_t *)msg;
		if (msg->msg_size != sizeof(snd_set_owner_t))
			return SND_BAD_PARM;

		snd_dbug(("snd_device_port_msg: sndout owner 0x%x\n",
			set_owner->owner));
		if (snd_var.sndoutowner == set_owner->owner)
			snd_device_reset(SND_DIR_PLAY);
		else if (snd_var.sndoutowner != PORT_NULL) {
			/*
			 * Return the owner of the port in the error message.
			 */
			(void) snd_reply_illegal_msg(snd_var.sndoutnegotiation,
						msg->msg_remote_port,
						msg->msg_id, SND_PORT_BUSY);
			return 0;
		} else {
			snd_port_alloc(snd_stream_port_name(SND_GD_SOUT_44,1));
			snd_port_alloc(snd_stream_port_name(SND_GD_SOUT_22,1));
		}

		snd_var.sndoutowner = set_owner->owner;
		snd_var.sndoutnegotiation = set_owner->negotiation;
		snd_device_init(SND_DIR_PLAY);

		break;
	}
	case SND_MSG_DSP_PROTO: {
		snd_dsp_proto_t *dsp_proto = (snd_dsp_proto_t *)msg;
		if (msg->msg_size != sizeof(snd_dsp_proto_t))
			return SND_BAD_PARM;
		
		/*
		 * (Unused) reply port must be owner port to set protocol level
		 */
		if (dsp_proto->owner != snd_var.dspowner)
			return SND_NOT_OWNER;

		if (dsp_proto->proto&SND_DSP_PROTO_LINKOUT) {
			if (snd_var.dspowner != snd_var.sndoutowner)
				return SND_NOT_OWNER;
			if (!dsp_var.chan[DSP_SO_CHAN])
				return SND_BAD_CHAN;
		}

		if (dsp_proto->proto&SND_DSP_PROTO_LINKIN) {
			if (snd_var.dspowner != snd_var.sndinowner)
				return SND_NOT_OWNER;
			if (!dsp_var.chan[DSP_SI_CHAN])
				return SND_BAD_CHAN;
		}

		/*
		 * Make things easy on ourselves when switching from one
		 * dma mode to another.
		 */
		if (   (   (dsp_var.flags&F_MODE_C_DMA)
			&& (dsp_proto->proto&SND_DSP_PROTO_S_DMA))
		    || (   (dsp_var.flags&F_MODE_S_DMA)
			&& (dsp_proto->proto&SND_DSP_PROTO_C_DMA)))
		{
			dsp_dev_new_proto(0);
		}
		dsp_dev_new_proto(dsp_proto->proto);
		break;
	}
	case SND_MSG_GET_DSP_CMD_PORT: {
		snd_get_dsp_cmd_port_t *get_dsp_cmd_port =
			(snd_get_dsp_cmd_port_t *)msg;
		if (msg->msg_size != sizeof(snd_get_dsp_cmd_port_t))
			return SND_BAD_PARM;

		if (get_dsp_cmd_port->owner != snd_var.dspowner)
			return SND_NOT_OWNER;

		snd_reply_dsp_cmd_port(snd_dsp_cmd_port_name(),
			msg->msg_remote_port);
		r = 0;
		break;
	}
	case SND_MSG_NEW_DEVICE_PORT: {
		snd_new_device_t *new_device = (snd_new_device_t *)msg;

		/*
		 * Re-allocate the device port.  All connections are
		 * terminated.
		 */
		if (msg->msg_size != sizeof(snd_new_device_t))
			return SND_BAD_PARM;

		/*
		 * User must pass the host privilege port for this
		 * host in order to re-allocate the device port.
		 */
		if (new_device->priv != snd_var.host_priv_self)
			return SND_BAD_HOST_PRIV;

		if (snd_var.sndoutowner) {
			snd_port_gone(snd_var.sndoutowner);
			(void) port_deallocate(snd_var.task,
					       snd_var.sndoutowner);
			snd_var.sndoutowner = PORT_NULL;
		}
		if (snd_var.sndinowner) {
			snd_port_gone(snd_var.sndinowner);
			(void) port_deallocate(snd_var.task,
					       snd_var.sndinowner);
			snd_var.sndinowner = PORT_NULL;
		}
		if (snd_var.dspowner) {
			snd_port_gone(snd_var.dspowner);
			(void) port_deallocate(snd_var.task, snd_var.dspowner);
			snd_var.dspowner = PORT_NULL;
		}
		(void) port_deallocate(snd_var.task, snd_var.dev_port);
		(void) port_allocate(snd_var.task, &snd_var.dev_port);
		(void )port_set_add(snd_var.task, snd_var.portset,
			snd_var.dev_port);
		(void) snd_reply_ret_device(msg->msg_remote_port,
				   snd_var.dev_port);
		r = 0;
		break;
	}

	case SND_MSG_RESET_DSPOWNER: {
		snd_reset_owner_t *reset_owner = (snd_reset_owner_t *)msg;
		
		/*
		 * Change the owner of this port from old_owner to
		 * new_owner;
		 */
		if (msg->msg_size != sizeof(snd_reset_owner_t))
			return SND_BAD_PARM;

		if (reset_owner->old_owner != snd_var.dspowner)
			return SND_NOT_OWNER;

		port_deallocate(snd_var.task, snd_var.dspowner);
		snd_var.dspowner = reset_owner->new_owner;
		
		port_deallocate(snd_var.task, snd_var.dspnegotiation);
		snd_var.dspnegotiation = reset_owner->new_negotiation;
		r = 0;
		break;
	}

	case SND_MSG_RESET_SNDINOWNER: {
		snd_reset_owner_t *reset_owner = (snd_reset_owner_t *)msg;
		
		/*
		 * Change the owner of this port from old_owner to
		 * new_owner;
		 */
		if (msg->msg_size != sizeof(snd_reset_owner_t))
			return SND_BAD_PARM;

		if (reset_owner->old_owner != snd_var.sndinowner)
			return SND_NOT_OWNER;

		port_deallocate(snd_var.task, snd_var.sndinowner);
		snd_var.sndinowner = reset_owner->new_owner;
		
		port_deallocate(snd_var.task, snd_var.sndinnegotiation);
		snd_var.sndinnegotiation = reset_owner->new_negotiation;
		r = 0;
		break;
	}

	case SND_MSG_RESET_SNDOUTOWNER: {
		snd_reset_owner_t *reset_owner = (snd_reset_owner_t *)msg;
		
		/*
		 * Change the owner of this port from old_owner to
		 * new_owner;
		 */
		if (msg->msg_size != sizeof(snd_reset_owner_t))
			return SND_BAD_PARM;

		if (reset_owner->old_owner != snd_var.sndoutowner)
			return SND_NOT_OWNER;

		port_deallocate(snd_var.task, snd_var.sndoutowner);
		snd_var.sndoutowner = reset_owner->new_owner;
		
		port_deallocate(snd_var.task, snd_var.sndoutnegotiation);
		snd_var.sndoutnegotiation = reset_owner->new_negotiation;
		r = 0;
		break;
	}

	default:
		r = SND_BAD_MSG;
		break;
	}

	return r;
}

static int snd_stream_port_msg(snd_stream_msg_t *msg)
{
	snd_msg_type_t *msg_type;
	vm_address_t snd_data;
	vm_size_t snd_size = 0;
	int high_water = 0;
	int low_water = 0;
	int dma_size = 0;
	register int error = 0;
	register int size;
	register port_name_t lport = msg->header.msg_local_port;
	int data_tag = msg->data_tag;
	int snd_control = 0;
	boolean_t need_nsamples = FALSE;
	boolean_t deactivate = TRUE;
	int options;
	int direction = -1;
	int chan;
	snd_region_t *new_reg;
	snd_queue_t *q;
	port_name_t reply_port;		// stored in region for event returns

	/*
	 * Figure out which queue we are.
	 */
	if (snd_gd_isdevice(lport))
		q = &ll_snd_var.dma[lport==(SND_GD_SIN|SND_PN_ASSIGNED)].sndq;
	else {
		if (!dsp_var.chan[snd_gd_chan(lport)])
			return SND_BAD_CHAN;
		q = &dsp_var.chan[snd_gd_chan(lport)]->sndq;
		
	}

	switch (msg->header.msg_id) {
	case SND_MSG_STREAM_MSG:
		break;	// handled after this
	case SND_MSG_STREAM_NSAMPLES: {
		/*
		 * Return number of samples played/recorded.
		 */
		snd_reply_ret_samples(msg->header.msg_remote_port, q->nxfer);

		return 0;
	}
	default:
		return SND_BAD_MSG;
	}

	chan = snd_gd_chan(lport);
	if (snd_gd_isdevice(lport))
		direction = (chan == snd_gd_chan(SND_GD_SIN))
			? SND_DIR_RECORD
			: SND_DIR_PLAY;

	size = msg->header.msg_size - sizeof(*msg);
	msg_type = (snd_msg_type_t *)(msg+1);
	while (size > 0) {
		switch (msg_type->type) {
		case SND_MT_PLAY_DATA: {
			snd_stream_play_data_t *stream_play_data =
				(snd_stream_play_data_t *)msg_type;

			*(int *)&msg_type += sizeof(snd_stream_play_data_t);
			size -= sizeof(snd_stream_play_data_t);

			options = stream_play_data->options;
			snd_data = (vm_address_t)stream_play_data->data;
			snd_size = (vm_size_t)
			    stream_play_data->dataType.msg_type_long_number;
			if (!error && direction == SND_DIR_RECORD)
				error = SND_BAD_PARM;

			snd_dbug(("snd_s_port_msg: play %d bytes at 0x%x\n",
			    snd_size, snd_data));

			/*
			 * DSP data must have initialized channel.
			 */
			if (!snd_gd_isdevice(lport)) {
				if (   chan < 0
				    || chan >= DSP_N_CHAN
				    || !dsp_var.chan[chan])
					error = SND_BAD_CHAN;
				else if (   !(dsp_var.flags&F_SOUNDDATA)
				 	 || snd_gd_chan(lport)!=DSP_SI_CHAN)
					dma_size =
					   dsp_var.chan[chan]->buf_size
					 * mode_size(dsp_var.chan[chan]->mode);
			}

			/*
			 * Data destined for, or coming from the DSP when
			 * it's in complex dma mode must be quad aligned.
			 * In any case data must be aligned to the channel
			 * mode size.
			 */
			if (!error && !snd_gd_isdevice(lport)) {
			    if (   (dsp_var.flags&F_SOUNDDATA)
				&& chan == DSP_SI_CHAN)
			    {
				int m_size = mode_size(
				    	dsp_var.chan[DSP_SI_CHAN]->mode);
				if (m_size == 0)
				    error = SND_BAD_PARM;
				else if (snd_data % m_size)
				    error = SND_NOTALIGNED;
			    } else  if (   (dsp_var.flags&F_MODE_C_DMA)
					&& (   dma_size == 0
					    || snd_data%dma_size
					    || snd_size%dma_size))
			    {
				    error = SND_NOTALIGNED;
			    }
			}

			direction = SND_DIR_PLAY;
			break;
		}
		case SND_MT_RECORD_DATA: {
			int r;
			char *filename;
			int filename_len;
			snd_stream_record_data_t *record_data =
				(snd_stream_record_data_t *)msg_type;

			filename_len=record_data->filenameType.msg_type_number;
			r = sizeof(*record_data) + filename_len;
			*(int *)&msg_type += r;
			size -= r;

			filename = (char *)(record_data+1);
			if (filename_len != 0)
				error = SND_NOPAGER;	// not yet.
			else
				deactivate = FALSE;

			snd_size = (vm_size_t)record_data->nbytes;
			snd_dbug(("snd_s_port_msg: record %d bytes\n",
			    snd_size));
			/*
			 * DSP data must have initialized channel.
			 */
			if (!snd_gd_isdevice(lport)) {
				if (   chan < 0
				    || chan >= DSP_N_CHAN
				    || !dsp_var.chan[chan])
					error = SND_BAD_CHAN;
				else
					dma_size =
					   dsp_var.chan[chan]->buf_size
					 * mode_size(dsp_var.chan[chan]->mode);
			}

			/*
			 * Data destined for, or coming from the DSP when
			 * it's in complex dma mode must be quad aligned.
			 */
			if (   !error
			    && !snd_gd_isdevice(lport)
			    && (dsp_var.flags&F_MODE_C_DMA)
			    && ((dma_size == 0) || (snd_size%dma_size)))
				error = SND_NOTALIGNED;

			if (!error && direction == SND_DIR_PLAY)
				error = SND_BAD_PARM;

			direction = SND_DIR_RECORD;
			break;
		}
		case SND_MT_CONTROL: {
			snd_stream_control_t *stream_control =
				(snd_stream_control_t *)msg_type;
			*(int *)&msg_type += sizeof(snd_stream_control_t);
			size -= sizeof(snd_stream_control_t);

			snd_dbug2(("snd_s_port_msg: control 0x%x\n",
			    stream_control->snd_control));
			snd_control |= stream_control->snd_control;
			break;
		}
		case SND_MT_OPTIONS: {
			snd_stream_options_t *stream_options = 
				(snd_stream_options_t *)msg_type;
			*(int *)&msg_type += sizeof(snd_stream_options_t);
			size -= sizeof(snd_stream_options_t);

			high_water = stream_options->high_water;
			low_water = stream_options->low_water;
			dma_size = stream_options->dma_size;
			snd_dbug(("snd_s_port_msg: high_water 0x%x, "
				  " low_water 0x%x, dma_size 0x%x\n",
			    high_water, low_water, dma_size));

			if (!high_water)
				high_water = snd_gd_isdevice(lport)
					? snd_device_def_high_water(direction)
					: snd_dspcmd_def_high_water(chan);
			if (!low_water)
				low_water = snd_gd_isdevice(lport)
					? snd_device_def_low_water(direction)
					: snd_dspcmd_def_low_water(chan);
			if (!dma_size)
				dma_size = snd_gd_isdevice(lport)
					? snd_device_def_dmasize(direction)
					: snd_dspcmd_def_dmasize(chan);

			/*
			 * PAGE_SIZE must be a multiple of the dma size.
			 */
			if (   dma_size == 0
			    || PAGE_SIZE%dma_size
			    || PAGE_SIZE < dma_size)
				error = SND_BAD_PARM;
			else if (!snd_gd_isdevice(lport)) {
				int bsize = dsp_var.chan[chan]->buf_size
				    * mode_size(dsp_var.chan[chan]->mode);
				if (dma_size != bsize)
					error = SND_BAD_PARM;
			}

			break;
		}
		case SND_MT_NDMA: {
			snd_stream_ndma_t *stream_ndma = 
				(snd_stream_ndma_t *)msg_type;
			*(int *)&msg_type += sizeof(snd_stream_ndma_t);
			size -= sizeof(snd_stream_ndma_t);

			snd_dbug(("snd_s_port_msg: ndma 0x%x\n",
				stream_ndma->ndma));

			if (!stream_ndma->ndma)
				q->ndma = 4;
			else
				q->ndma = stream_ndma->ndma;

			break;
		}
		default:
			error = SND_BAD_MSG;
			size = 0;
			break;
		}
	}

	/*
	 * If we found any errors, cleanup and return an error.
	 */
	if (error) {
	    error_return:
		size = msg->header.msg_size - sizeof(*msg);
		msg_type = (snd_msg_type_t *)(msg+1);
		while (size > 0) {
			switch (msg_type->type) {
			case SND_MT_PLAY_DATA: {
				snd_stream_play_data_t *stream_play_data =
					(snd_stream_play_data_t *)msg_type;
	
				*(int *)&msg_type +=
					sizeof(snd_stream_play_data_t);
				size -= sizeof(snd_stream_play_data_t);
	
				(void) vm_deallocate(snd_var.task_map,
					(vm_address_t)stream_play_data->data,
					stream_play_data->
						dataType.msg_type_long_number);
				break;
			}
			case SND_MT_RECORD_DATA:
				*(int *)&msg_type +=
					sizeof(snd_stream_record_data_t);
				size -= sizeof(snd_stream_record_data_t);
				break;

			case SND_MT_CONTROL:
				*(int *)&msg_type +=
					sizeof(snd_stream_control_t);
				size -= sizeof(snd_stream_control_t);
				break;
	
	
			case SND_MT_OPTIONS:
				*(int *)&msg_type +=
					sizeof(snd_stream_options_t);
				size -= sizeof(snd_stream_options_t);
				break;
			case SND_MT_NDMA:
				*(int *)&msg_type +=
					sizeof(snd_stream_ndma_t);
				size -= sizeof(snd_stream_ndma_t);
				break;
			default:
				/*
				 * This shouldn't happen.
				 */
				ASSERT(0);
			}
		}
		return error;
	}

	/*
	 * If an option was sent down without a corresponding region
	 * update the defaults in the stream with the new values.
	 */
	if (!snd_size && dma_size) {
		q->dmasize = dma_size;
		q->def_high_water = high_water;
		q->def_low_water = low_water;
	}

	/*
	 * Process message.
	 *
	 * Abortions first.
	 */
	if (snd_control&SND_DC_ABORT)
		snd_stream_abort(q, data_tag);


	/*
	 * AWAIT on record sends back recorded data.
	 */
	if (snd_control&SND_DC_AWAIT) {
		error = snd_stream_await(q, data_tag);
		if (error)
			goto error_return;
	}

	/*
	 * Pause play or record
	 */
	if (snd_control&SND_DC_PAUSE) {
		snd_stream_pause(q);
	}

	size = msg->header.msg_size - sizeof(*msg);
	msg_type = (snd_msg_type_t *)(msg+1);
	while (size > 0) {
		snd_size = 0;
		switch (msg_type->type) {
		case SND_MT_PLAY_DATA: {
			snd_stream_play_data_t *stream_play_data =
				(snd_stream_play_data_t *)msg_type;

			*(int *)&msg_type += sizeof(snd_stream_play_data_t);
			size -= sizeof(snd_stream_play_data_t);

			options = stream_play_data->options;
			snd_data = (vm_address_t)stream_play_data->data;
			snd_size = (vm_size_t)
			    stream_play_data->dataType.msg_type_long_number;

			/*
			 * Protect this region read-only so that we
			 * don't end up copying all the pages we play.
			 */
			(void) vm_map_protect(snd_var.task_map,
					    trunc_page(snd_data),
					    round_page(snd_data+snd_size),
					    VM_PROT_READ,
					    FALSE);
			reply_port = stream_play_data->reg_port;
			break;
		}
		case SND_MT_RECORD_DATA: {
			snd_stream_record_data_t *record_data =
				(snd_stream_record_data_t *)msg_type;

			*(int *)&msg_type += sizeof(*record_data);
			size -= sizeof(*record_data);

			options = record_data->options;
			snd_size = (vm_size_t)record_data->nbytes;

			(void) vm_allocate(snd_var.task_map, &snd_data,
					snd_size, TRUE);

			reply_port = record_data->reg_port;
			break;
		}
		case SND_MT_CONTROL:
			*(int *)&msg_type += sizeof(snd_stream_control_t);
			size -= sizeof(snd_stream_control_t);
			break;
		case SND_MT_OPTIONS:
			*(int *)&msg_type += sizeof(snd_stream_options_t);
			size -= sizeof(snd_stream_options_t);
			break;
		case SND_MT_NDMA:
			*(int *)&msg_type += sizeof(snd_stream_ndma_t);
			size -= sizeof(snd_stream_ndma_t);
			break;
		default:
			/*
			 * This shouldn't happen.
			 */
			ASSERT(0);
		}

		/*
		 * Send down a region if we have data, or if we need an await
		 * message sent in the future.
		 */
		if (snd_size) {
			queue_head_t temp_q;
			snd_region_t *rt;
	
			new_reg = (snd_region_t *)kalloc(sizeof(snd_region_t));
			new_reg->addr = snd_data;
			new_reg->size = snd_size;
			new_reg->reply_port = reply_port;
			new_reg->reg_id = data_tag;
			new_reg->completed = (options&SND_DM_COMPLETED_MSG)!=0;
			new_reg->started = (options&SND_DM_STARTED_MSG) != 0;
			new_reg->aborted = (options&SND_DM_ABORTED_MSG) != 0;
			new_reg->paused = (options&SND_DM_PAUSED_MSG) != 0;
			new_reg->resumed = (options&SND_DM_RESUMED_MSG) != 0;
			new_reg->overflow = (options&SND_DM_OVERFLOW_MSG) != 0;
			new_reg->high_water = high_water;
			new_reg->low_water = low_water;
			new_reg->dma_size = dma_size;
			new_reg->sound_q = q;
			new_reg->direction = direction;
			new_reg->deactivate = deactivate;
	
			new_reg->rate = lport==(SND_GD_SOUT_44|SND_PN_ASSIGNED)
				? SND_RATE_44
				: SND_RATE_22;
			snd_stream_enqueue_region(new_reg, chan,
						  snd_gd_isdevice(lport),
						  (options&SND_DM_PREEMPT));
	
		}
	}

	if (snd_control&SND_DC_RESUME) {
		snd_stream_resume(q);
	}

	return SND_NO_ERROR;
}

/*
 * Got notification that port we have send rights on has gone away, clean up.
 * FIXME: do something about forgeting completion ports and removing
 * record regions.
 */
static void snd_port_gone(port_name_t port)
{
	register int i;

	if (port == snd_var.dspowner) {
		snd_dbug(("snd_port_gone: dsp owner\n"));
		snd_var.dspowner = PORT_NULL;
		/*
		 * Deallocate stream ports
		 */
		for (i = DSP_N_CHAN-1; i >= 0; i--)
			port_deallocate(snd_var.task,
				snd_stream_port_name(i, 0));
		port_deallocate(snd_var.task, snd_dsp_cmd_port_name());

		dsp_dev_reset_hard();			// reset DSP
		kfree((caddr_t)dsp_var.msgbuf,
			(dsp_var.emsgbuf-dsp_var.msgbuf)*sizeof(u_int *));
		dsp_var.msgbuf = dsp_var.emsgbuf = dsp_var.msgp = 0;
		if (dsp_var.errbuf) {
			kfree((caddr_t)dsp_var.errbuf,
			    (dsp_var.eerrbuf-dsp_var.errbuf)*sizeof(u_int *));
		    dsp_var.errbuf = dsp_var.eerrbuf = dsp_var.errp = 0;
		}
		
	}
	if (port == snd_var.sndinowner) {
		snd_dbug(("snd_port_gone: sndin owner\n"));
		port_deallocate(snd_var.task,
			snd_stream_port_name(SND_GD_SIN, 1));
		snd_var.sndinowner = PORT_NULL;
		snd_device_reset(SND_DIR_RECORD);	// reset sound in
	}
	if (port == snd_var.sndoutowner) {
		snd_dbug(("snd_port_gone: sndout owner\n"));
		port_deallocate(snd_var.task,
			snd_stream_port_name(SND_GD_SOUT_44, 1));
		port_deallocate(snd_var.task,
			snd_stream_port_name(SND_GD_SOUT_22, 1));
		snd_var.sndoutowner = PORT_NULL;
		snd_device_reset(SND_DIR_PLAY);		// reset sound out
	}

	if (snd_var.sndoutowner || snd_var.sndinowner || snd_var.dspowner)
		return;

	if (snd_var.task_map->size) {
#if	DEBUG
		snd_dbug(("sound task map not empty!\n"));
		printf("sound task map not empty!\n");
		vm_map_print(snd_var.task_map);
#endif	DEBUG
		/*
		 * If all ownership has been revoked then ensure that we don't
		 * have any memory left in our task map (we shouldn't, but
		 * sometimes it doesn't seem to go away.
		 */
		vm_deallocate(snd_var.task_map,
				vm_map_min(snd_var.task_map),
				vm_map_max(snd_var.task_map));
	}
}
#endif	NSOUND > 0
