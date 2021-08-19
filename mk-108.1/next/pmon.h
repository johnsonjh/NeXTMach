/*
 * pmon.h - structs and #defines for performance monitor library interfaces.
 *
 * History
 * -------
 * 18-Dec-89	dmitch
 *	Created.
 */
 
#import	<sys/types.h>
#import	<sys/message.h>
#import <next/pmon_ipc.h>
#import <sys/boolean.h>

/* 
 *	pmon_vars struct. One per client, initialized in pmon_open().
 */
struct pmon_vars {
	
	port_t		server_port;	/* pmon server's port */
	port_t		reply_port;	/* port thru which we communicate with
					 * pmon server */
	port_t		event_port;	/* port to which events are sent */
	pmon_event_msg_t emsg;		/* contains the last event message 
					 * received in event_port */
	pmon_event_t 	event_ptr;	/* points to an element in 
					 * emsg->event[], or NULL if no valid 
					 * events. */
	int 		num_events;	/* number of valid, unused pmon_events
					 * in emsg */
}; 

typedef struct pmon_vars *pmon_vars_t;

typedef int 		pmon_return_t;

/*
 * 	values for pmon_return_t
 */
#define PMR_SUCCESS	   0		/* OK */
#define PMR_RESOURCE	-900		/* resource (memory, port) allocation
					 * failure */
#define PMR_IPCFAIL	-901		/* mach IPC error */
#define PMR_ACCESS	-902		/* desired events already destined for
					 * another viewer */
#define PMR_BADEVENT	-903		/* bad source/event */
#define PMR_SERVCONN	-904		/* couldn't connect to server */
#define PMR_BADMSG	-905		/* bad message */
#define PMR_QUEUEFULL	-906		/* destination queue full on
					 * msg_send() */
#define PMR_BADPARAM	-907		/* bad parameter */
#define PMR_INTERNAL	-908		/* pmon internal error */
#define PMR_TARGCONN	-909		/* couldn't connect to client */
#define PMR_DISCONN	-910		/* not connected to specified
					 *  source/event_type */
#define PMR_BADSTATUS	  -1		/* not used */

/*
 *	prototypes for library functions
 */
 
/*
 * client library functions
 */
pmon_return_t pmon_open(char *hostname, 
		pmon_vars_t *pvpp);
pmon_return_t pmon_target_cntrl(pmon_vars_t pvp,
		char *hostname,
		char *devname,
		int source,
		int msg_id,
		int event_type);
pmon_return_t pmon_connect(pmon_vars_t pvp,
		int source,	
		int event_type);
pmon_return_t pmon_disconnect(pmon_vars_t pvp,
		int source,
		int event_type);
void pmon_close(pmon_vars_t pvp);
pmon_return_t pmon_get_event(pmon_vars_t pvp,
		pmon_event_t event_p);
void pmon_error(char *err_str, pmon_return_t err);

/* 
 * target library functions
 */
void pmon_build_emsg(pmon_event_msg_t msgp,
		port_t local_port,
		port_t server_port);
int pmon_add_event(pmon_event_msg_t msgp,
		pmon_event_t ep,
		boolean_t send);
pmon_return_t pmon_send_emsg(pmon_event_msg_t msgp);

/* end of pmon.h */


