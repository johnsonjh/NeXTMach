/*	@(#)ipc_notify.c	2.0	05/11/90	(c) 1990 NeXT	*/

/* 
 * ipc_notify.c - port death notification server
 *
 * HISTORY
 * 05-11-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import <sys/types.h>
#import <kern/queue.h>
#import <kern/thread.h> 
#import <kern/kern_obj.h> 
#import <kern/kern_port.h>
#import <sys/notify.h>
#import <sys/task_special_ports.h>
#import <kern/ipc_notify.h>

/* #define PN_DEBUG	1	/* */

#ifdef	PN_DEBUG
#define pn_debug(x)	printf x
#else	PN_DEBUG
#define pn_debug(x)
#endif	PN_DEBUG

/*
 * This module provides a means for kernel threads to receive port death 
 * notification. This module is implemented as a task separate from the kernel;
 * kernel threads register ports in which they are interested via 
 * port_request_notification(). The notification server receives notification
 * in the normal fashion and forwards the notification on to a port registered
 * in the port_request_notification() call.
 */

/*
 * struct used to maintain list of registered ports.
 */
struct pn_register_ent {
	queue_chain_t	link;
	port_t		rights_port;
	port_t		notify_port;
	int		rights_port_rep;
};

typedef struct pn_register_ent *pn_register_ent_t;

/*
 * local functions.
 */
static void pn_panic(kern_return_t krtn, char *err_string);
static void pn_register(port_register_notify_t prn_msg);
static void pn_notify(notification_t *nmsg);

/*
 * global variables.
 */
port_t pn_register_port=PORT_NULL;	/* requests from kernel threads are
					 * sent here */
kern_port_t pn_register_port_k;		/* internal version of same */
static port_t pn_notify_port;		/* port death notifications are sent 
					 * here */
static kern_port_t pn_notify_port_k;
					/* internal version of same */
static port_set_name_t pnotify_port_set;
static queue_head_t pn_reg_q;		/* maintains registration info */

/*
 * Message templates.
 */
static struct port_register_notify prn_temp = {
	{				/* prn_header */
		0,			/* msg_unused */
		FALSE,			/* msg_simple */
		sizeof(struct port_register_notify),
		MSG_TYPE_NORMAL,	/* msg_type */
		PORT_NULL,	
		PORT_NULL,
		PORT_REGISTER_NOTIFY	/* msg_id */
	},
	{				/* prn_p_type */
		MSG_TYPE_PORT,		/* msg_type_name */
		8 * sizeof(port_t),	/* msg_type_size */
		2,			/* msg_type_number */
		TRUE,			/* msg_type_inline */
		FALSE,			/* msg_type_longform */
		FALSE,			/* msg_type_deallocate */
		0			/* msg_type_unused */
	},
	PORT_NULL,			/* prn_rights_port */
	PORT_NULL,			/* prn_notify_port */
	{				/* prn_rpr_type */
		MSG_TYPE_INTEGER_32,	/* msg_type_name */
		8 * sizeof(int),	/* msg_type_size */
		1,			/* msg_type_number */
		TRUE,			/* msg_type_inline */
		FALSE,			/* msg_type_longform */
		FALSE,			/* msg_type_deallocate */
		0			/* msg_type_unused */
	},
	0				/* prn_rp_rep */
};

/*
 * prototypes for kernel functions.
 */
void *kalloc(int size);
void kfree(void *addrs, int size);

/*
 * Remove a pn_register_ent from the queue and deallocate it.
 */
#define pn_remove(prp) 							\
{									\
	queue_remove(&pn_reg_q, prp, struct pn_register_ent *, link); 	\
	kfree(prp, sizeof(*prp)); 					\
}

void notify_server_loop() {
	kern_return_t krtn;
	msg_header_t *msg;
	task_t pn_task;
	
	current_thread()->ipc_kernel = TRUE;	/* no copyin() of data 
						 * necessary */
	current_task()->kernel_ipc_space = FALSE;
						/* use port_t's, not 
						 * kern_port_t's */
	current_task()->kernel_vm_space = TRUE;
	/*
	 * set up ports we'll use.
	 */
	pn_task = current_task();
	krtn = port_allocate(pn_task, &pn_notify_port);
	if(krtn) 
		pn_panic(krtn, "port_allocate");
	/*
	 * task_set_special_port needs a kern_port_t. All other ipc calls
	 * used here work with port_t's.
	 */
	get_kern_port(current_task(), pn_notify_port, &pn_notify_port_k);
	krtn = task_set_special_port(pn_task, TASK_NOTIFY_PORT, 
		pn_notify_port_k);
	if(krtn) 
		pn_panic(krtn, "task_set_special_port");
	krtn = port_allocate(pn_task, &pn_register_port);
	if(krtn) 
		pn_panic(krtn, "port_allocate");
	/*
	 * Get internal version of pn_register_port.
	 */
	get_kern_port(current_task(), pn_register_port, &pn_register_port_k);
	krtn = port_set_allocate(pn_task, &pnotify_port_set);
	if(krtn) 
		pn_panic(krtn, "port_set_allocate");
	krtn = port_set_add(pn_task, pnotify_port_set, pn_notify_port);
	if(krtn) 
		pn_panic(krtn, "port_set_add");
	krtn = port_set_add(pn_task, pnotify_port_set, pn_register_port);
	if(krtn) 
		pn_panic(krtn, "port_set_add");
	/*
	 * Other initialization.
	 */
	msg = (msg_header_t *)kalloc(MSG_SIZE_MAX);
	queue_init(&pn_reg_q);
	
	/*
	 * Main loop. Just handle incoming messages.
	 */
	while(1) {
		msg->msg_local_port = pnotify_port_set;
		msg->msg_size = MSG_SIZE_MAX;
		krtn = msg_receive(msg, MSG_OPTION_NONE, 0);
		if(krtn) {
			printf("notify_server_loop: msg_receive error (%d)\n",
				 krtn);
			continue;
		}
		if(msg->msg_local_port == pn_register_port)
			pn_register((port_register_notify_t)msg);
		else if(msg->msg_local_port == pn_notify_port)
			pn_notify((notification_t *)msg);
		else 
			printf("notify_server_loop: BOGUS msg_local_port\n");
			 
	}
	/* NOT REACHED */
} /* notify_server_loop() */

static void pn_panic(kern_return_t krtn, char *err_string)
{
	printf("notification server: %s: krtn = %d\n", err_string, krtn);
	panic("notification server"); 

}

/*
 * A kernel thread is registering a port.
 */
static void pn_register(port_register_notify_t prn_msg)
{
	pn_register_ent_t prp;
	
	switch(prn_msg->prn_header.msg_id) {
	    case PORT_REGISTER_NOTIFY:
		pn_debug(("pn_register: registering rights_port\n"));
	    	prp = kalloc(sizeof(*prp));
		prp->rights_port = (port_t)prn_msg->prn_rights_port;
		prp->notify_port = (port_t)prn_msg->prn_notify_port;
		prp->rights_port_rep = prn_msg->prn_rp_rep;
		queue_enter(&pn_reg_q, prp, struct pn_register_ent *, link);
		break;
	    default:
	    	/*
		 * We only know how to do one thing now.
		 */
		printf("notify server: bogus msg_id on pn_register_port "
			"(%d)\n", prn_msg->prn_header.msg_id);
		break;
	}
}

/*
 * A port has died. This could be one of three kinds of ports:
 *
 * 1) a port registered previously as a prn_rights_port. Forward this
 *    message to the appropriate prn_notify_port. This is the "normal"
 *    function of this task.
 * 2) a port registered previously as a prn_notify_port. A kernel thread
 *    has died.
 * 3) A port we know nothing about.
 *
 * In the first two cases, we remove a pn_register_ent entry from pn_reg_q.
 */
static void pn_notify(notification_t *nmsg)
{
	pn_register_ent_t prp, next_prp;
	kern_return_t krtn;
	int port_found = 0;
	
	if(nmsg->notify_header.msg_id != NOTIFY_PORT_DELETED) {
		/*
		 * Huh? What's this?!
		 */
		printf("pn_notify: msg_id = %d, unrecognized\n", 
			nmsg->notify_header.msg_id);
		return;
	}
	prp = (pn_register_ent_t)queue_first(&pn_reg_q);
	while(!queue_end(&pn_reg_q, (queue_t)prp)) {
		next_prp = (pn_register_ent_t)prp->link.next;
		if(nmsg->notify_port == prp->notify_port) {
			/*
			 * A kernel thread either died or deallocated its
			 * notify port. Forget this entry.
			 */
			pn_debug(("pn_notify: NOTIFY_PORT DEATH\n"));
			pn_remove(prp);
			port_found++;
			goto next_elt;
		}
		else if(nmsg->notify_port == prp->rights_port) {
			/*
			 * A kernel thread needs to be notified of this. Then
			 * we can forget this pn_register_ent since the
			 * rights_port is dead.
			 */
			pn_debug(("pn_notify: RIGHTS_PORT DEATH\n"));
			nmsg->notify_header.msg_remote_port = prp->notify_port;
			nmsg->notify_header.msg_local_port = PORT_NULL;
			/*
			 * We don't pass a port_t to the target thread since
			 * this port is already dead. Convert message to one
			 * which passes an int.
			 */
			nmsg->notify_header.msg_simple = TRUE;	
			nmsg->notify_type.msg_type_name = MSG_TYPE_INTEGER_32;
			nmsg->notify_type.msg_type_size = 8 * sizeof(int);
			nmsg->notify_port = (port_t)prp->rights_port_rep;
			
			krtn = msg_send(&nmsg->notify_header, SEND_TIMEOUT, 0);
			if(krtn) 
				printf("pn_notify: msg_send returned "
					"%d\n",	krtn);
			pn_remove(prp);
			port_found++;
		}
next_elt:
		prp = next_prp;		/* next in the queue */
	}
	if(!port_found)
	    printf("pn_notify: PORT NOT FOUND\n");

} /* pn_notify() */

/*
 * Register reg_port for notification on notify_port. 
 */
void port_request_notification(kern_port_t reg_port, 
	kern_port_t notify_port)
{
	struct port_register_notify pnmsg;
	kern_return_t krtn;
	
	pnmsg = prn_temp;
	pnmsg.prn_rights_port = reg_port;
	pnmsg.prn_notify_port = notify_port;
	pnmsg.prn_header.msg_local_port = PORT_NULL;
	pnmsg.prn_header.msg_remote_port = (port_t)pn_register_port_k;
	/*
	 * Provide a non-translated version of reg_port; this will be 
	 * sent to the registering thread upon port death.
	 */
	pnmsg.prn_rp_rep = (int)reg_port;
	krtn = msg_send_from_kernel(&pnmsg.prn_header,
		SEND_TIMEOUT,
		0);			/* don't block if queue full */
	if(krtn)
		printf("port_request_notification: msg_send returned %d\n",
			krtn);
}

void pnotify_start()
{
	/*
	 * Start up the notification server.
	 */
	task_t newtask;
	thread_t newthread;
	kern_return_t krtn;
	
	(void) task_create(kernel_task, FALSE, &newtask);
	(void) thread_create(newtask, &newthread);
	thread_start(newthread, notify_server_loop, THREAD_SYSTEMMODE);
	(void) thread_resume(newthread);
	pn_debug(("pnotify_start: notification server started\n"));
}

/*
 * Convert port_t to kern_port_t.
 */
kern_return_t get_kern_port(task_t task, port_t in_port, kern_port_t *kport)
{
	kern_obj_t port_obj;
		
	if(in_port == (port_t)0) {
		*kport = PORT_NULL;
		return(KERN_SUCCESS);
	}
	if(!object_copyin(task, in_port, MSG_TYPE_PORT, FALSE, kport))
		return(KERN_INVALID_ARGUMENT);
	else
		return(KERN_SUCCESS);
}

/* end of ipc_notify.c */
 