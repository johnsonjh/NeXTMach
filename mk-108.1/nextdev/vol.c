/*	@(#)vol.c	2.0	03/19/90	(c) 1990 NeXT	*/

/* 
 * vol.c - volume insertion device
 *
 * HISTORY
 * 03-19-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import <sys/types.h>
#import <sys/errno.h>
#import <sys/ioctl.h>
#import <sys/systm.h>
#import <sys/disktab.h>
#import <kern/kern_port.h> 
#import <kern/thread.h> 
#import <kern/ipc_cache.h> 
#import <kern/ipc_statistics.h> 
#import <kern/kern_obj.h> 
#import <kern/lock.h>
#import <nextdev/insertmsg.h>
#import <nextdev/disk.h>
#import <nextdev/ldd.h>
#import <nextdev/voldev.h>
#import <kern/queue.h>
#import <kern/ipc_notify.h>
#import <sys/notify.h>
	
/* #define VOL_DEBUG	1	/* */

#ifdef	VOL_DEBUG
#define vol_debug(x)	printf x
#else	VOL_DEBUG
#define vol_debug(x)
#endif	VOL_DEBUG

/*
 * Local functions.
 */
static kern_return_t vol_notify_com(int in_dev_desc,	
	dev_t block_dev, 
	dev_t raw_dev,
	kern_port_t dev_port, 
   	int vol_state,
	char *form_type,
	char *dev_str,
	int flags);
static void vol_thread();
static vpe_t vol_panel_get_entry(int tag);
static void vol_port_death(notification_t *nmsg);

/*
 * Prototypes for kernel functions.
 */
msg_return_t msg_send_from_kernel(msg_header_t *msgptr, msg_option_t option, 
	msg_timeout_t tout);

int volopen(dev_t dev);
int volclose(dev_t dev);
int volioctl(dev_t dev, 
	int cmd, 
	caddr_t data, 
	int flag);

/*
 * Globals.
 */
static char vol_dev_open;
static char vol_dev_init_flag;
static kern_port_t vol_check_port=PORT_NULL;	
					/* port to which disk insertion 
					 * messages are sent; usually allocated 
					 * by autodiskmount. */
kern_port_t panel_req_port=PORT_NULL;
					/* port to which Panel Requests are
				 	 * sent. */
static kern_port_t vol_local_port=PORT_NULL;	
					/* port on which vol_thread listens. */
static queue_head_t vol_notify_q = {&vol_notify_q, &vol_notify_q};
					/* hold notifications prior to
					 * first DKIOCMNOTIFY */
static lock_data_t vol_notify_lock;	/* protects vol_notify_q */
static queue_head_t vol_panel_q;
static int vol_panel_tag;
static vol_thread_alive=0;

/*
 * message templates.
 */
static struct insert_notify insert_notify_temp = { 
	{					/* in_header */
		0,				/* msg_unused */
		FALSE,				/* msg_simple */
		0,				/* msg_size */
		MSG_TYPE_NORMAL, 		/* msg_type */
		PORT_NULL, 			/* msg_local_port */
		PORT_NULL, 			/* msg_remote_port */
		VOL_CHECK_MSG_ID 		/* msg_id */
	},
	{					/* in_vd_type */
		MSG_TYPE_INTEGER_32, 		/* msg_type_name */
		32, 				/* msg_type_size */
		2, 				/* msg_type_number */
		TRUE, 				/* msg_type_inline */
		FALSE, 				/* msg_type_longform */
		FALSE, 				/* msg_type_deallocate */
	},
	0,	 				/* in_vol_state */
	0, 					/* in_dev_desc */
	{					/* in_flags_type */
		MSG_TYPE_INTEGER_32, 		/* msg_type_name */
		32, 				/* msg_type_size */
		1, 				/* msg_type_number */
		TRUE, 				/* msg_type_inline */
		FALSE, 				/* msg_type_longform */
		FALSE, 				/* msg_type_deallocate */
	},
	0,	 				/* in_flags */
	{					/* in_form_type */
		MSG_TYPE_CHAR,  		/* msg_type_name */
		8, 				/* msg_type_size */
		FORM_TYPE_LENGTH,  		/* msg_type_number */
		TRUE, 				/* msg_type_inline */
		FALSE, 				/* msg_type_longform */
		FALSE, 				/* msg_type_deallocate */
	},
	NULL					/* in_form_type */
};

static struct vol_panel_req vol_panel_req_temp = {
	{					/* pr_header */
		0,				/* msg_unused */
		TRUE, 				/* msg_simple */
		sizeof(struct vol_panel_req),	/* msg_size */
		MSG_TYPE_NORMAL,  		/* msg_type */
		PORT_NULL, 			/* msg_local_port */
		PORT_NULL, 			/* msg_remote_port */
		VOL_PANEL_REQ 			/* msg_id */
	},
	{					/* pr_int_desc */
		MSG_TYPE_INTEGER_32,  		/* msg_type_name */
		32, 				/* msg_type_size */
		7, 				/* msg_type_number */
		TRUE, 				/* msg_type_inline */
		FALSE, 				/* msg_type_longform */
		FALSE, 				/* msg_type_deallocate */
	},
	0, 					/* pr_panel_type */
	0, 			 		/* pr_resp_type */
	0, 			 		/* pr_tag */
	0,	 				/* p1 */
	0, 					/* p2 */
	0, 					/* p3 */
	0, 					/* p4 */
	{					/* pr_string_desc */
		MSG_TYPE_CHAR,  		/* msg_type_name */
		8, 				/* msg_type_size */
		2 * VP_STRING_LEN, 		/* msg_type_number */
		TRUE, 				/* msg_type_inline */
		FALSE, 				/* msg_type_longform */
		FALSE 				/* msg_type_deallocate */
	},
	NULL,					/* string1 */
	NULL					/* string2 */
};

static struct vol_panel_cancel vol_panel_cancel_temp = {
	{					/* pc_header */
		0,				/* msg_unused */
		TRUE, 				/* msg_simple */
		sizeof(struct vol_panel_cancel),/* msg_size */
		MSG_TYPE_NORMAL, 		/* msg_type */
		PORT_NULL, 			/* msg_local_port */
		PORT_NULL,  			/* msg_remote_port */
		VOL_PANEL_CANCEL 		/* msg_id */
	},
	{					/* pc_int_desc */
		MSG_TYPE_INTEGER_32,  		/* msg_type_name */
		32, 				/* msg_type_size */
		1, 				/* msg_type_number */
		TRUE, 				/* msg_type_inline */
		FALSE,				/* msg_type_longform */
		FALSE 				/* msg_type_deallocate */
	},
	0					/* pc_tag */
};

int volopen(dev_t dev)
{
	if(vol_dev_open)
		return(EBUSY);
	else {
		vol_dev_open = 1;
		return(0);
	}
}

int volclose(dev_t dev)
{
	vol_dev_open = 0;
	return(0);
}

int volioctl(dev_t dev, 
	int cmd, 
	caddr_t data, 
	int flag)
{
	int rtn=0;
	port_t remote_port;
	
	switch (cmd) {
	    case DKIOCMNOTIFY:
	    	/*
		 * Send disk insert notification to port specified in *data.
		 */
		vol_debug(("volioctl: registering NOTIFY port\n"));
		remote_port = *(port_t *)data;
		if(get_kern_port(current_task(), remote_port, &vol_check_port)) 
			rtn = EINVAL;
		else {
			/* 
			 * Send notification for each entry in vol_notify_q.
			 */
			vne_t vnep, vne_next_p;
			
			lock_write(&vol_notify_lock);
			vnep = (vne_t)queue_first(&vol_notify_q);
			while(!queue_end(&vol_notify_q, (queue_entry_t)vnep)) {
				vne_next_p = (vne_t)vnep->link.next;
				queue_remove(&vol_notify_q,
					vnep,
					vne_t,
					link); 
				if(vnep->block_dev != rootdev) {
					/*
					 * skip notification for root device 
					 */
					vol_notify_com(vnep->dev_desc,	
						vnep->block_dev, 
						vnep->raw_dev,
						vnep->dev_port, 
						vnep->vol_state,
						vnep->form_type,
						vnep->dev_str,
						vnep->flags);
				}
				kfree(vnep, sizeof(struct vol_notify_entry));
				vnep = vne_next_p;
			}
			lock_done(&vol_notify_lock);
		}
		/*
		 * Register this port so we'll know when it dies.
		 */
		if(rtn == 0) 
			port_request_notification(vol_check_port, 
				vol_local_port);
		break;

	    case DKIOCPANELPRT:
	    	/*
		 * register port to which we send Panel request messages.
		 */
		vol_debug(("volioctl: registering PANEL port\n"));
		remote_port = *(port_t *)data;
		if(get_kern_port(current_task(), remote_port, &panel_req_port)) 
			rtn = EINVAL;
		/*
		 * Register this port so we'll know when it dies.
		 */
		port_request_notification(panel_req_port, vol_local_port);
		break;
		
	    default:
	    	rtn = EINVAL;
		break;
	}
	return(rtn);
} /* volioctl() */

/*
 * Any driver may call these functions to notify appropriate user task of disk
 * insertion. If no port has been registered via DKIOCMNOTIFY, no action
 * is taken.
 */
 
kern_return_t vol_notify_dev(dev_t block_dev, 
	dev_t raw_dev,
	char *form_type,
   	int vol_state,				/* IND_VS_LABEL, etc. */
	char *dev_str,
	int flags)
{
	return(vol_notify_com(IND_DD_DEV,
		block_dev,
		raw_dev,
		0,
		vol_state,
		form_type,
		dev_str,
		flags));
}
	
kern_return_t vol_notify_port(kern_port_t dev_port, 
	char *form_type,
   	int vol_state,				/* IND_VS_LABEL, etc. */
	int flags)
{
	return(vol_notify_com(IND_DD_PORT,
		0,
		0,
		dev_port,
		vol_state,
		form_type,
		"",
		flags));
}

static kern_return_t vol_notify_com(int in_dev_desc,	
	dev_t block_dev, 
	dev_t raw_dev,
	kern_port_t dev_port, 
   	int vol_state,				/* IND_VS_LABEL, etc. */
	char *form_type,
	char *dev_str,
	int flags)
{
	struct insert_notify *notify_msg;
	struct of_insert_notify_dev *dev_msg;
	struct of_insert_notify_port *port_msg;
	int size;	
	kern_return_t krtn;
	
	vol_debug(("vol_notify_com: block_dev=0x%x vol_state=0x%x\n", 
		block_dev, vol_state));
	if(!vol_dev_init_flag) {
 		lock_init(&vol_notify_lock, TRUE);
		vol_dev_init_flag = TRUE;
	}
	if(vol_check_port == (kern_port_t)NULL) {
		vne_t vnep;
		
		/* 
		 * nobody has registered yet. Queue up this request.
		 */
		vnep = kalloc(sizeof(struct vol_notify_entry));
		vnep->dev_desc = in_dev_desc;
		vnep->block_dev = block_dev;
		vnep->raw_dev = raw_dev;
		vnep->dev_port = dev_port;
		vnep->vol_state = vol_state;
		vnep->flags = flags;
		strcpy(vnep->form_type, form_type);
		strcpy(vnep->dev_str, dev_str);
		lock_write(&vol_notify_lock);
		queue_enter(&vol_notify_q,
			vnep,
			struct vol_notify_entry *,
			link); 
		lock_done(&vol_notify_lock);
		return(KERN_SUCCESS);
	}

	switch(in_dev_desc) {
	    case IND_DD_DEV:
	    	size = sizeof(struct of_insert_notify_dev);
		break;
	    case IND_DD_PORT:
	    	size = sizeof(struct of_insert_notify_port);
	    default:
	    	return(KERN_INVALID_ARGUMENT);
	}
	notify_msg = (struct insert_notify *)kalloc(size);
	*notify_msg = insert_notify_temp;
	notify_msg->in_header.msg_simple      = (in_dev_desc == IND_DD_PORT) ? 
					        FALSE : TRUE;
	notify_msg->in_header.msg_size        = size;
	notify_msg->in_header.msg_remote_port = (port_t)vol_check_port;
	notify_msg->in_vol_state              = vol_state;
	notify_msg->in_dev_desc               = in_dev_desc;
	notify_msg->in_flags		      = flags;
	strcpy(notify_msg->in_form_type, form_type);

	/*
	 * dev or port 
	 */
	switch(in_dev_desc) {
	    case IND_DD_DEV:
	    	dev_msg = (struct of_insert_notify_dev *)notify_msg;
		dev_msg->oid_dev_type.msg_type_name      = MSG_TYPE_INTEGER_32;
		dev_msg->oid_dev_type.msg_type_size       = 32;
		dev_msg->oid_dev_type.msg_type_number     = 2;
		dev_msg->oid_dev_type.msg_type_inline     = TRUE;
		dev_msg->oid_dev_type.msg_type_longform   = FALSE;
		dev_msg->oid_dev_type.msg_type_deallocate = FALSE;
		dev_msg->oid_bdev_t = block_dev;
		dev_msg->oid_cdev_t = raw_dev;
		dev_msg->oid_ds_type.msg_type_name       = MSG_TYPE_CHAR;
		dev_msg->oid_ds_type.msg_type_size       = 8;
		dev_msg->oid_ds_type.msg_type_number     = OID_DEVSTR_LEN;
		dev_msg->oid_ds_type.msg_type_inline     = TRUE;
		dev_msg->oid_ds_type.msg_type_longform   = FALSE;
		dev_msg->oid_ds_type.msg_type_deallocate = FALSE;
		strcpy(dev_msg->oid_dev_str, dev_str);
		break;
		
	    case IND_DD_PORT:
	    	port_msg = (struct of_insert_notify_port *)notify_msg;
		port_msg->oip_p_type.msg_type_name       = MSG_TYPE_PORT;
		port_msg->oip_p_type.msg_type_size       = 32;
		port_msg->oip_p_type.msg_type_number     = 1;
		port_msg->oip_p_type.msg_type_inline     = TRUE;
		port_msg->oip_p_type.msg_type_longform   = FALSE;
		port_msg->oip_p_type.msg_type_deallocate = FALSE;
		port_msg->oip_port = (port_t)dev_port;
	}
	krtn = msg_send_from_kernel(&notify_msg->in_header,
		SEND_TIMEOUT,
		0);			/* don't block if queue full */
#ifdef	DEBUG
	if(krtn)
		printf("vol_check_notify: msg_send returned %d\n", krtn);
#endif 	DEBUG
	return(krtn);
} /* vol_check_notify */

/*
 * Cancel a (possibly) queued notify message. Normally called when a device
 * is mounted.
 */
void vol_notify_cancel(dev_t device)
{
	vne_t vnep, vne_next_p;
	
	/*
	 * Inserts are always registered for partition a; mounts can be
	 * on other partitions...
	 */
	device &= ~(NPART - 1); 
	lock_write(&vol_notify_lock);
	vnep = (vne_t)queue_first(&vol_notify_q);
	while(!queue_end(&vol_notify_q, (queue_entry_t)vnep)) {
		vne_next_p = (vne_t)vnep->link.next;
		if((vnep->block_dev == device) ||
		   (vnep->raw_dev == device)) {
			queue_remove(&vol_notify_q,
				vnep,
				vne_t,
				link); 
			kfree(vnep, sizeof(struct vol_notify_entry));
		}
		vnep = vne_next_p;
	}
	lock_done(&vol_notify_lock);
}

/*
 * Functions to request and cancel alert panels via WSM
 */
 
/*
 * Generic panel request.
 */
kern_return_t vol_panel_request(vpt_func fnc,
	int panel_type,				/* PR_PT_DISK_NUM, etc. */
	int response_type,			/* PR_RT_ACK, etc. */
	int p1,
	int p2,
	int p3,
	int p4,
	char *string1,
	char *string2,
	void *param,
	int *tag) {				/* RETURNED */
	
	kern_return_t krtn;
	vpe_t vpe;
	struct vol_panel_req *panel_msgp;
	
	vol_debug(("vol_panel_request: panel_type=%d\n", panel_type));
	if(panel_req_port == (kern_port_t)NULL) {
		char *disk_string;
		
		/*
		 * Nobody registered to deal with these requests yet. Punt
		 * and send notification to the console; no ack is possible.
		 *
		 * For some panels, this switch is meaningless (but harmless).
		 */
		switch(p2) {
		    case PR_DRIVE_FLOPPY:
			disk_string = "Floppy";
			break;
		    case PR_DRIVE_OPTICAL:
			disk_string = "Optical";
			break;
		    case PR_DRIVE_SCSI:
			disk_string = "SCSI";
			break;
		    default: 
			disk_string = "";
			break;
		}  
		switch(panel_type) {
		    case PR_PT_DISK_NUM:
		    	printf("Please Insert %s Disk %d in Drive %d\n", 
				disk_string, p1, p3);
			break;
		    case PR_PT_DISK_LABEL:
		    	printf("Please Insert %s Disk \'%s\' in Drive %d\n", 
				disk_string, string1, p3);
			break;
		    case PR_PT_DISK_NUM_W:
		    	printf("Wrong Disk: Please Insert %s Disk %d in Drive "
				"%d\n", disk_string, p1, p3);
			break;
		    case PR_PT_DISK_LABEL_W:
		    	printf("Wrong Disk: Please Insert %s Disk \'%s\' in "
				"Drive %d\n", disk_string, string1, p3);
			break;
		    case PR_PT_SWAPDEV_FULL:
		    	printf("***Swap Device Full***\n");
			break;
		    case PR_PT_FILESYS_FULL:
		    	printf("***File System %s Full***\n", string1);
			break;
	 	    case PR_RT_EJECT_REQ:
			printf("Please Eject %s Disk %d\n", disk_string, p3);
			break;
		    default:
		    	/* FIXME: what do we do here? */
			printf("vol_panel_request: bogus panel_type (%d)\n",
				panel_type);
			break;
		}
		return(0);
	}
	/*
	 * Build a panel request message.
	 */
	panel_msgp = (struct vol_panel_req *)kalloc(sizeof(*panel_msgp));
	*panel_msgp = vol_panel_req_temp;
	panel_msgp->pr_header.msg_local_port  	= (port_t)vol_local_port;
	panel_msgp->pr_header.msg_remote_port 	= (port_t)panel_req_port;
	panel_msgp->pr_panel_type 		= panel_type;
	panel_msgp->pr_resp_type 		= response_type;
	panel_msgp->pr_tag	 		= vol_panel_tag++;
	panel_msgp->pr_p1 			= p1;
	panel_msgp->pr_p2			= p2;
	panel_msgp->pr_p3			= p3;
	panel_msgp->pr_p4			= p4;
	if(strlen(string1) >= VP_STRING_LEN)
		string1[VP_STRING_LEN-1] = '\0';
	if(strlen(string2) >= VP_STRING_LEN)
		string2[VP_STRING_LEN-1] = '\0';
	strcpy(panel_msgp->pr_string1, string1);
	strcpy(panel_msgp->pr_string2, string2);
	/*
	 * send the message.
	 */
	krtn = msg_send_from_kernel(&panel_msgp->pr_header,
		SEND_TIMEOUT,
		0);			/* don't block if queue full */
#ifdef	DEBUG
	if(krtn)
		printf("vol_panel_request: msg_send returned %d\n", krtn);
#endif 	DEBUG
	if(krtn)
		return(krtn);
	*tag = panel_msgp->pr_tag;
	/*
	 * Enqueue this event on vol_panel_q if we expect an ack 
	 */
	if(response_type == PR_RT_NONE)
		return(KERN_SUCCESS);
	vpe = kalloc(sizeof(struct vol_panel_entry));
	if(vpe == NULL)
		return(KERN_RESOURCE_SHORTAGE);
	vpe->tag = panel_msgp->pr_tag;
	vpe->fnc = fnc;
	vpe->param = param;
	queue_enter(&vol_panel_q, vpe, struct vol_panel_entry *, link);
	return(KERN_SUCCESS);
	
} /* vol_panel_request */

/*
 * remove an existing panel.
 */
kern_return_t vol_panel_remove(int tag) {
	struct vol_panel_cancel *msgp;
	kern_return_t krtn;
	vpe_t vpe;
	
	vol_debug(("vol_panel_remove\n"));
	if(panel_req_port != (kern_port_t)NULL) {
		msgp = (struct vol_panel_cancel *)kalloc(sizeof(*msgp));
		*msgp = vol_panel_cancel_temp;
		msgp->pc_header.msg_local_port        = (port_t)vol_local_port;
		msgp->pc_header.msg_remote_port       = (port_t)panel_req_port;
		msgp->pc_tag 			      = tag;
		krtn = msg_send_from_kernel(&msgp->pc_header,
			SEND_TIMEOUT,
			0);			/* don't block if queue full */
		if(krtn)
			printf("vol_panel_remove: msg_send returned %d\n",
				krtn);
	}
	/*
	 * Remove entry in vol_panel_q for this panel.
	 */
	vpe = vol_panel_get_entry(tag);
	if(vpe) 
		kfree(vpe, sizeof(*vpe));
	else {
		/*
		 * This is not an error; it could happen in a race condition in 
		 * which this function were called just after we received a
		 * vol_panel_cancel message.
		 */
		vol_debug(("vol_panel_remove: panel resp, tag not found\n"));
	}
	return(krtn);
}

/*
 * Panel requests specifically for disk drivers.
 */
kern_return_t vol_panel_disk_num(vpt_func fnc,
	int volume_num,
	int drive_type,				/* PR_DRIVE_FLOPPY, etc. */
	int drive_num,
	void *param,
	boolean_t wrong_disk,
	int *tag) {				/* RETURNED */
	
	return(vol_panel_request(fnc,
		wrong_disk ? PR_PT_DISK_NUM_W : PR_PT_DISK_NUM,
		PR_RT_ACK,
		volume_num,
		drive_type,
		drive_num,
		0,
		"",
		"",
		param,
		tag));	
}

kern_return_t vol_panel_disk_label(vpt_func fnc,
	char *label,
	int drive_type,				/* PR_DRIVE_FLOPPY, etc. */
	int drive_num,
	void *param,
	boolean_t wrong_disk,
	int *tag) {				/* RETURNED */

	return(vol_panel_request(fnc,
		wrong_disk ? PR_PT_DISK_LABEL_W : PR_PT_DISK_LABEL,
		PR_RT_ACK,
		0,
		drive_type,
		drive_num,
		0,
		label,
		"",
		param,
		tag));	
} 

void vol_thread() {
	/*
	 * The job here is to receive and handle two kinds of messages:
	 *
	 * -- messages from the Workspace (like vol_panel_resp messages). 
	 * -- port death messages, forwarded to us from voltask. The death of 
	 *    panel_req_port will cause all pending panels in vol_panel_q to be
	 *    acked and removed. The death of vol_check_port isn't very 
	 *    interesting; we just NULL it out so we don't send any more
	 *    insertion events.
	 */
	 
	struct vol_panel_resp *msgp;
	kern_return_t krtn;
	vpe_t vpe;

	msgp = (struct vol_panel_resp *)kalloc(MSG_SIZE_MAX);
	
	while(1) {
		msgp->ps_header.msg_local_port = (port_t)vol_local_port;
		msgp->ps_header.msg_size       = MSG_SIZE_MAX;
		krtn = msg_receive(&msgp->ps_header, MSG_OPTION_NONE, 0);
		if(krtn) {
			printf("vol_thread: msg_receive() returned %d\n", 
				krtn);
			continue;
		}
		vol_debug(("vol_thread: msg_id = 0x%x\n", 
			msgp->ps_header.msg_id));
		switch(msgp->ps_header.msg_id) {
		    case VOL_PANEL_RESP:
		    	/*
			 * First we have to see if we know about a panel with 
			 * this tag.
			 */
			vpe = vol_panel_get_entry(msgp->ps_tag);
			if(vpe == NULL) {
				/*
				 * This is not an error; it could happen
				 * in a race condition in which this message
				 * came in just after a driver sent a 
				 * vol_panel_cancel message.
				 */
				vol_debug(("vol_thread: panel resp, tag not"
					" found\n"));
				break;
			}
			/*
			 * Perform callout if necessary, then dispose of
			 * entry.
			 */
			vol_debug(("vol_thread: doing VOL_PANEL_RESP "
				"callout\n"));
			if(vpe->fnc) {
				(*vpe->fnc)(vpe->param, 
					vpe->tag, 
					msgp->ps_value);
			}
			kfree(vpe, sizeof(*vpe));
			break;
			
		    case NOTIFY_PORT_DELETED:
			vol_debug(("vol_thread: port death\n"));
			vol_port_death((notification_t *)msgp);
			break;
			
		    default:
		    	printf("vol_thread: bogus message rec'd (msg_id = "
				"%d)\n", msgp->ps_header.msg_id);
			break;
		}
	}
} /* vol_thread() */

static vpe_t vol_panel_get_entry(int tag) {
	/*
	 * get vol_panel_entry associated with 'tag' from vol_panel_q. Removes
	 * entry from vol_panel_q. Returns NULL if entry not found.
	 */
	vpe_t vpe;

	vpe = (vpe_t)queue_first(&vol_panel_q);
	while(!queue_end(&vol_panel_q, (queue_entry_t)vpe)) {
		if(vpe->tag == tag) {
			/*
			 * Found it. 
			 */
			queue_remove(&vol_panel_q,
				vpe,
				struct vol_panel_entry *,
				link);
			return(vpe);
		}
		vpe = (vpe_t)vpe->link.next;
	}
	/*
	 * Not found. 
	 */
	return((vpe_t)NULL);
}

static void vol_port_death(notification_t *nmsg)
{
	/*
	 * voltask has detected port death of one of our ports.
	 */
	vol_debug(("vol_port_death: "));
	if((kern_port_t)nmsg->notify_port == vol_check_port) {
		vol_debug(("vol_check_port dead\n"));
		vol_check_port = PORT_NULL;
	}
	else if ((kern_port_t)nmsg->notify_port == panel_req_port) {
		vpe_t vpe, vpe_next;
		
		vol_debug(("panel_req_port dead\n"));
		/*
		 * Ack and dispose of every vol_panel_entry in vol_panel_q.
		 */
		vpe = (vpe_t)queue_first(&vol_panel_q);
		while(!queue_end(&vol_panel_q, (queue_entry_t)vpe)) {
			vol_debug(("vol_port_death: doing VOL_PANEL_RESP "
				"callout\n"));
			vpe_next = (vpe_t)vpe->link.next;
			if(vpe->fnc) {
				(*vpe->fnc)(vpe->param, 
					vpe->tag, 
					0);	/* as in "disk not 
						 * available" */
			}
			queue_remove(&vol_panel_q,
				vpe,
				struct vol_panel_entry *,
				link);
			kfree(vpe, sizeof(*vpe));
			vpe = vpe_next;
		}
		/*
		 * Finally, inhibit further panel request messages.
		 */
		panel_req_port = PORT_NULL;
	}
	else 
		vol_debug(("bogus notification\n"));
}

void vol_start_thread()
{
	kern_return_t krtn;
	
	krtn = port_alloc(kernel_task, &vol_local_port);
	if(krtn) {
		printf("vol_start_thread: port_alloc returned %d\n", 
			krtn);
		return;
	}
	port_unlock(vol_local_port);
	kernel_thread_noblock(kernel_task, vol_thread);
	queue_init(&vol_panel_q);
	vol_thread_alive = 1;
	if(!vol_dev_init_flag) {
 		lock_init(&vol_notify_lock, TRUE);
		vol_dev_init_flag = TRUE;
	}
}

/* end of vol.c */
