/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 *  2-Aug-90  Gregg Kellogg (gk) at NeXT
 *	Changed snd_reply_recorded_data to take an inline argument to send
 *	data back inline.
 *
 *  7-Jun-90  Gregg Kellogg (gk) at NeXT
 *	Added SEND_SWITCH option to several msg_send's
 *
 * 25-Jun-89  Gregg Kellogg (gk) at NeXT, Inc.
 *	release the port after doing a object_copyin.  This was resulting
 *	in a leak in the port_zone.
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sound.h>
#if	NSOUND > 0

/*
 * Interface routines for composing messages to send in response to
 * sound facility services.
 */
#import <kern/xpr.h>
#import <next/cpu.h>
#import <nextdev/snd_var.h>
#import <nextdev/snd_dsp.h>

#import <next/spl.h>

extern snd_var_t snd_var;

#if	DEBUG || XPR_DEBUG
extern int dspdbgflag;
#endif	DEBUG || XPR_DEBUG
#if	XPR_DEBUG
#define dspdbug(f) {if (dspdbgflag&1)printf f; else XPR(XPR_DSP, f);}
#else	XPR_DEBUG
#define dspdbug(f)
#endif	XPR_DEBUG

/*
 * Templates for comonly used components.
 */
static msg_header_t snd_reply_header_template = {
	/* no name */		0,
	/* msg_simple */	TRUE,
	/* msg_size */		sizeof(msg_header_t),
	/* msg_type */		MSG_TYPE_NORMAL,
	/* msg_remote_port */	0,
	/* msg_reply_port */	0,
	/* msg_id */		0
};

static msg_type_long_t snd_type_ool_template = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		FALSE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	TRUE,
	},
	/* msg_type_long_name = */	MSG_TYPE_INTEGER_8,
	/* msg_type_long_size = */	8,
	/* msg_type_long_number = */	0,
};

static msg_type_t snd_type_int_template = {
	/* msg_type_name = */		MSG_TYPE_INTEGER_32,
	/* msg_type_size = */		32,
	/* msg_type_number = */		1,
	/* msg_type_inline = */		TRUE,
	/* msg_type_longform = */	FALSE,
	/* msg_type_deallocate = */	FALSE,
};

static msg_type_long_t snd_type_inline_template = {
	{
		/* msg_type_name = */		0,
		/* msg_type_size = */		0,
		/* msg_type_number = */		0,
		/* msg_type_inline = */		TRUE,
		/* msg_type_longform = */	TRUE,
		/* msg_type_deallocate = */	FALSE,
	},
	/* msg_type_long_name = */	MSG_TYPE_INTEGER_32,
	/* msg_type_long_size = */	32,
	/* msg_type_long_number = */	0,
};

static kern_return_t snd_reply_with_tag(port_name_t port, int tag, int msg_id);

/*
 * Generic replies
 */
/*
 * Message to return the device port.
 */
kern_return_t snd_reply_ret_device (
	port_name_t	remote_port,	// who to send it to.
	port_name_t	device_port)	// returned port.
{
	snd_ret_port_t msg;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_DEVICE;
	msg.header.msg_local_port = device_port;
	msg.header.msg_remote_port = remote_port;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT, 0);
}

/*
 * Message to return a stream port.
 */
kern_return_t snd_reply_ret_stream (
	port_name_t	remote_port,	// who to send it to.
	port_name_t	stream_port)	// returned port.
{
	snd_ret_port_t msg;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_STREAM;
	msg.header.msg_local_port = stream_port;
	msg.header.msg_remote_port = remote_port;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT, 0);
}

static kern_return_t snd_reply_with_tag(port_name_t port, int tag, int msg_id)
{
	snd_taged_reply_t msg;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = msg_id;
	msg.header.msg_size = sizeof(snd_taged_reply_t);
	msg.header.msg_remote_port = port;
	msg.data_tagType = snd_type_int_template;
	msg.data_tag = tag;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT|SEND_SWITCH, 0);
}

/*
 * Message return on illegal message.
 */
kern_return_t snd_reply_illegal_msg (
	port_name_t	local_port,	// returned port of interest
	port_name_t	remote_port,	// who to send it to.
	int		msg_id,		// message id with illegal syntax	
	int		error)		// error code	
{
	snd_illegal_msg_t msg;
	
	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_ILLEGAL_MSG;
	msg.header.msg_size = sizeof(snd_illegal_msg_t);
	msg.header.msg_local_port = local_port;
	msg.header.msg_remote_port = remote_port;

	msg.dataType = snd_type_int_template;
	msg.dataType.msg_type_number = 2;
	msg.ill_msgid = msg_id;
	msg.ill_error = error;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT, 0);
}

/*
 * Replies specific to streaming soundout/soundin
 */
/*
 * Message returning recorded data.
 */
kern_return_t snd_reply_recorded_data (
	port_name_t	remote_port,	// who to reply to
	int		data_tag,	// tag from region
	pointer_t	data,		// recorded data
	int		nbytes,		// number of bytes of data to send
	int		in_line)	// "Send data inline" flag.
{
	snd_recorded_data_t _msg, *msg = &_msg;
	int msgsize = sizeof(snd_recorded_data_t);
	kern_return_t r;

	if (in_line)
	{
		msgsize += (nbytes - sizeof(msg->recorded_data));
		msg = (snd_recorded_data_t *) kalloc(msgsize);
	}
	msg->header.msg_simple = FALSE;
	msg->header.msg_size = msgsize;
	msg->header.msg_type = MSG_TYPE_NORMAL;
	msg->header.msg_remote_port = remote_port;
	msg->header.msg_local_port = PORT_NULL;
	msg->header.msg_id = SND_MSG_RECORDED_DATA;

	/*
	 * Add this message component to the message.
	 */
	msg->data_tagType = snd_type_int_template;
	msg->data_tag = data_tag;
	msg->dataType = snd_type_ool_template;
	msg->dataType.msg_type_long_number = nbytes;
	if (in_line)
	{	/* Fixup message type; copy the data */
		msg->dataType.msg_type_header.msg_type_inline = TRUE;
		bcopy(data, (char *) &(msg->recorded_data), nbytes);
	}
	else
		msg->recorded_data = data;

	r = msg_send((msg_header_t *)msg, SEND_TIMEOUT|SEND_SWITCH, 0);
	if (in_line) kfree((caddr_t) msg, msgsize);
	return r;
}

/*
 * Message to return a timeout indication.
 */
kern_return_t snd_reply_timed_out (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// tag from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_TIMED_OUT);
}

/*
 * Message to return an integer count of number samples played/recorded.
 */
kern_return_t snd_reply_ret_samples (
	port_name_t	remote_port,	// who to send it to.
	int		nsamples)	// number of bytes of data to record
{
	snd_ret_samples_t msg;

	msg.header = snd_reply_header_template;
	msg.header.msg_size = sizeof(snd_ret_samples_t);
	msg.header.msg_id = SND_MSG_RET_SAMPLES;
	msg.header.msg_remote_port = remote_port;

	msg.dataType = snd_type_int_template;
	msg.nsamples = nsamples;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT|SEND_SWITCH, 0);
}

/*
 * Message to return an overflow indication.
 */
kern_return_t snd_reply_overflow (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_OVERFLOW);
}

/*
 * Message to return a region started indication.
 */
kern_return_t snd_reply_started (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_STARTED);
}

/*
 * Message to return region completed indication (play only).
 */
kern_return_t snd_reply_completed (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_COMPLETED);
}

/*
 * Message to return region aborted indication (play only).
 */
kern_return_t snd_reply_aborted (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_ABORTED);
}

/*
 * Message to return region paused indication (play only).
 */
kern_return_t snd_reply_paused (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_PAUSED);
}

/*
 * Message to return region resumed indication (play only).
 */
kern_return_t snd_reply_resumed (
	port_name_t	remote_port,	// who to send it to.
	int		data_tag)	// from region
{
	return snd_reply_with_tag(remote_port, data_tag, SND_MSG_RESUMED);
}

/*
 * Replies specific to the sound device.
 */
/*
 * Message to return the set of parameters.
 */
kern_return_t snd_reply_ret_parms (
	port_name_t	remote_port,	// who to send it to.
	u_int		parms)
{
	snd_ret_parms_t msg;
	
	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_PARMS;
	msg.header.msg_size = sizeof(snd_ret_parms_t);
	msg.header.msg_remote_port = remote_port;

	msg.dataType = snd_type_int_template;
	msg.parms = parms;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT, 0);
}

/*
 * Message to return the volume.
 */
kern_return_t snd_reply_ret_volume (
	port_name_t	remote_port,	// who to send it to.
	u_int		volume)	
{
	snd_ret_volume_t msg;
	
	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_VOLUME;
	msg.header.msg_size = sizeof(snd_ret_volume_t);
	msg.header.msg_remote_port = remote_port;

	msg.dataType = snd_type_int_template;
	msg.volume = volume;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT, 0);
}

/*
 * DSP specific messages
 */
/*
 * Message to return dsp messages.
 * Must be able to be sent from kernel context.
 */
kern_return_t snd_reply_dsp_msg (
	port_name_t	remote_port)	// who to send it to.
{
	snd_dsp_msg_t msg;
	register int s, r;
	extern dsp_var_t dsp_var;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_DSP_MSG;
	msg.header.msg_local_port = PORT_NULL;
	msg.header.msg_size = (char *)&msg.data[0] - (char *)&msg;

	msg.dataType = snd_type_inline_template;
	s = spldsp();
	msg.dataType.msg_type_long_number =
		dsp_var.msgp - dsp_var.msgbuf;
	bcopy((char *)dsp_var.msgbuf, &msg.data[0],
		msg.dataType.msg_type_long_number * sizeof(int));
	dsp_var.msgp = dsp_var.msgbuf;
	dsp_dev_loop();
	splx(s);
	msg.header.msg_size += msg.dataType.msg_type_long_number * sizeof(int);

	if (object_copyin(snd_var.task, remote_port, MSG_TYPE_PORT, FALSE,
			  &msg.header.msg_remote_port))
	{
		r = msg_send_from_kernel((msg_header_t *)&msg,SEND_TIMEOUT,0);
		port_release(msg.header.msg_remote_port);
		dspdbug(("snd_reply_dsp_msg: msg sent status %d\n", r));
	} else {
		dspdbug(("snd_reply_dsp_msg: obj copyin failed\n"));
		r = KERN_FAILURE;
	}

	return r;
}

/*
 * Message to return error messages.
 * Must be able to be sent from kernel context.
 */
kern_return_t snd_reply_dsp_err (
	port_name_t	remote_port)	// who to send it to.
{
	snd_dsp_err_t msg;
	register int s, r;
	extern dsp_var_t dsp_var;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_DSP_ERR;
	msg.header.msg_local_port = PORT_NULL;
	msg.header.msg_size = (char *)&msg.data[0] - (char *)&msg;

	msg.dataType = snd_type_inline_template;
	s = spldsp();
	msg.dataType.msg_type_long_number =
		dsp_var.errp - dsp_var.errbuf;
	bcopy((char *)dsp_var.errbuf, &msg.data[0],
		msg.dataType.msg_type_long_number * sizeof(int));
	dsp_var.errp = dsp_var.errbuf;
	dsp_dev_loop();
	splx(s);
	msg.header.msg_size += msg.dataType.msg_type_long_number * sizeof(int);

	if (object_copyin(snd_var.task, remote_port, MSG_TYPE_PORT, FALSE,
			  &msg.header.msg_remote_port))
	{
		r = msg_send_from_kernel((msg_header_t *)&msg,SEND_TIMEOUT,0);
		port_release(msg.header.msg_remote_port);
		dspdbug(("snd_reply_dsp_err: msg sent status %d\n", r));
	} else {
		dspdbug(("snd_reply_dsp_err: obj copyin failed\n"));
		r = KERN_FAILURE;
	}

	return r;
}

kern_return_t snd_reply_dsp_cmd_port(
	port_name_t	cmd_port,	// port to return.
	port_name_t	remote_port)	// where to return it
{
	snd_ret_port_t msg;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_RET_CMD;
	msg.header.msg_local_port = cmd_port;
	msg.header.msg_remote_port = remote_port;

	return msg_send((msg_header_t *)&msg, SEND_TIMEOUT, 0);
}

/*
 * Send the DSP host interface registers to the dsp owner.
 */
kern_return_t snd_reply_dsp_regs (
	int		regs)		// DSP host I/F registers (not recieve)
{
	snd_ret_dsp_regs_t msg;
	register int s, r;
	extern dsp_var_t dsp_var;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_DSP_REGS;
	msg.header.msg_local_port = PORT_NULL;
	msg.header.msg_size = sizeof(msg);

	msg.dataType = snd_type_int_template;
	msg.dsp_regs = regs;

	if (object_copyin(snd_var.task, snd_var.dspowner, MSG_TYPE_PORT, FALSE,
			  &msg.header.msg_remote_port))
	{
		r = msg_send_from_kernel((msg_header_t *)&msg, SEND_TIMEOUT,0);
		port_release(msg.header.msg_remote_port);
	} else
		r = KERN_FAILURE;

	return r;
}

/*
 * Message to indicate a condition becoming true.  A tag may be
 * returned, but is unused at this time.  In the future it may contain
 * the state of the dsp registers at the time that the condition was
 * satisfied.
 */
kern_return_t snd_reply_dsp_cond_true (	// reply indicating condition true
	vm_address_t	retCond)	// reply_port, status and conditions
{
	dsp_msg_t *ret_cond = (dsp_msg_t *)retCond;
	snd_dsp_cond_true_t msg;
	register int s, r;
	extern dsp_var_t dsp_var;

	msg.header = snd_reply_header_template;
	msg.header.msg_id = SND_MSG_DSP_COND_TRUE;
	msg.header.msg_local_port = PORT_NULL;
	msg.header.msg_size = sizeof(msg);

	msg.dataType = snd_type_int_template;
	msg.dataType.msg_type_number = 3;
	msg.mask = ret_cond->u.condition.mask;
	msg.flags = ret_cond->u.condition.flags;
	msg.value = ret_cond->scratch;

	if (object_copyin(snd_var.task, ret_cond->u.condition.reply_port,
			  MSG_TYPE_PORT, FALSE, &msg.header.msg_remote_port))
	{
		r = msg_send_from_kernel((msg_header_t *)&msg,SEND_TIMEOUT,0);
		port_release(msg.header.msg_remote_port);
	} else
		r = KERN_FAILURE;

	return r;
}

#endif	NSOUND > 0
