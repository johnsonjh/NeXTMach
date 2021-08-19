/*
 * pmon_ipc.h - message structs used by performance monitor
 *
 * History
 * -------
 * 03/21/90	dmitch
 *	Added PMON_SOURCE_FD.
 * 18-Dec-89	dmitch
 *	Created.
 */
 
#import	<sys/types.h>
#import	<sys/message.h>
#import <sys/time_stamp.h>

/*
 * Each logged event is described by one of these structs. The exact 
 * definitions and meanings of each field is application specific; the
 * server which collects this data merely collects raw data.
 *
 * Warning: all fields must be int-aligned. This struct is typed as an
 * array of ints during ipc to maintain word alignment.
 */
struct pmon_event {

	int		source;		/* where event occurred (kernel, 
					 * DPS, etc.). */
	int		event_type;	/* Actual event (page out, disk read,
					 * NXPing(), etc.). Formatted as one
					 * bit out of 32; bits are in the same
					 * format as in the event_type argument
					 * to pmon_target_cntrl(). */
	struct tsval	time;		/* when event occurred */
	int		data1;		/* event-specific descriptors */
	int		data2;
	int		data3;
	
}; /* pmon_event */

typedef	struct pmon_event *pmon_event_t; 

/* 
 * event message. Sent by target to pmon server and from server to client.
 *
 * Each message can contain up to PM_EVENT_MAX event descriptors. All data is 
 * inline.
 *
 * All event message are sent by msg_send(), NOT msg_rpc() to minimize
 * ipc traffic.
 */
struct pmon_event_msg {

	msg_header_t	header;		/* generic message header */
	msg_type_t	ne_type;	/* describes num_events */
	int		num_events;	/* how many pmon_events in this 
					 * message */
	msg_type_t	e_type;		/* describes event */
	struct pmon_event event[1];	/* first event */
};

typedef struct pmon_event_msg *pmon_event_msg_t;

/*
 * control message. Sent by client to pmon server and from pmon server to
 * target. All ipc done with control messages are rpc's to ensure
 * integrity.
 */
#define PMON_STRING_LEN		40

struct pmon_cntrl_msg {

	msg_header_t	header;		/* generic message header */
	msg_type_t	ser_type;	/* describes source, event_type, 
					 * rtn_status */
	int		source;		/* same as pmon_event.source. */
	int		event_type;	/* same as pmon_event.event_type. */
	int		rtn_status;	/* status returned to client */
	msg_type_t	hd_type;	/* describes hostname, devname */
	
	/*
	 * the following two fields are only used for PM_CTL_ENABLE and 
	 * PM_CTL_DISABLE messages.
	 */
	char		hostname[PMON_STRING_LEN];	/* in ASCII */
	char 		devname[PMON_STRING_LEN];	/* in ASCII */
	
	/* 
	 * the following is only used for PM_CONNECT and PM_DISCONNECT
	 * messages.
	 */
	msg_type_t	ep_type;	/* describes event_port */
	port_name_t	event_port;	/* port to which events are to be 
					 * sent. This is a server port for 
					 * target control messages and a client 
					 * port for connect/disconnect 
					 * messages. */
};

typedef struct pmon_cntrl_msg *pmon_cntrl_msg_t;

/* 
 * msg_id values for all pmon messages
 */
#define PM_CTL_ENABLE		0x1300	/* enable events at target level */
#define PM_CTL_DISABLE		0x1301	/* disable events at target level */
#define PM_CTL_CONNECT		0x1302	/* send events to client */
#define PM_CTL_DISCONNECT	0x1303	/* stop sending events to client */
#define PM_EVENT		0x1304	/* event message */

/*
 * Standard sources for known pmon targets
 */
#define	PMON_SOURCE_NULL	0
#define PMON_SOURCE_TEST	1	/* test target */

#define PMON_SOURCE_KERN	2	/* kernel (in general) */
#define PMON_SOURCE_SCSI	3	/* scsi disk driver */
#define PMON_SOURCE_VM		4	/* vm system */
#define PMON_SOURCE_VFS		5	/* file system ops at the VFS level */
#define PMON_SOURCE_UFS		6	/* ufs activity */
#define PMON_SOURCE_NFS		7	/* nfs activity */
#define PMON_SOURCE_IPC		8	/* ipc events */
#define PMON_SOURCE_SCHED	9	/* scheduling activity */
#define PMON_SOURCE_NP		10	/* printer driver */
#define PMON_SOURCE_XP		11	/* default external pager */
#define PMON_SOURCE_NET		12	/* networking in general */
#define PMON_SOURCE_IP		13	/* ip */
#define PMON_SOURCE_TCP		14	/* tcp */
#define PMON_SOURCE_FD		15	/* floppy disk driver */
#define PMON_SOURCE_EV		16	/* ev driver */

#define PMON_SOURCE_DPS		100	/* Display PostScript main thread */
#define PMON_SOURCE_DPS_WORK	101	/* Display PostScript worker thread */

#define PMON_SOURCE_APPKIT	200	/* appkit, in general */

#define PMON_SOURCE_BAD		-1
#define PMON_SOURCES_MAX	300
#define PMON_KERNEL_SOURCES_MAX	100

/*
 * miscellaneous constants
 */
#define PMON_CMSG_SIZE	(sizeof(struct pmon_cntrl_msg))

/* event msg size (hack accounts for kern_msg) */
#define PMON_EMSG_SIZE	(8192 - 44)

#define PM_EVENT_MAX	((PMON_EMSG_SIZE - sizeof(struct pmon_event_msg)) / \
				sizeof(struct pmon_event) + 1)

#define PMON_SERVER_NAME "pmon_server"	/* netname_name_t of pmon server */
					 

/* end of pmon_ipc.h */
