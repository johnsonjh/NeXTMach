/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 04-Apr-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef _KERNLOAD_SERVER_
#define _KERNLOAD_SERVER_
#import <mach.h>
#import <syslog.h>
#import <kern/queue.h>
#import <objc/hashtable.h>
#import <sys/loader.h>
#import <kernserv/kern_loader_types.h>
#import <nlist.h>
#import <cthreads.h>

/*
 * This module parses the server script file and maintains all information
 * needed to load and unload the server.
 */

/*
 * Queue locking protocol:
 *	Lock when changing states.
 *	Lock whenever access is in a non-changing state
 *	(eg: Loaded, Allocated) not when in a changing state
 *	(eg: Loading, Allocating)
 */
typedef struct server {
	queue_head_t	load_cmds;	// initialization commands
	queue_head_t	unload_cmds;	// shutdown commands
	queue_head_t	notifications;	// notify forwarding requests
	volatile queue_chain_t link;	// queue of servers
	mutex_t		lock;		// mutex access to structure
	condition_t	state_changed;	// signaled on state change
	NXAtom		name;		// server name
	NXAtom		executable;	// executable file to load w.r.t.
	volatile server_state_t	state;
	u_int		wire:1,		// should server be wired?
			load:1,		// load on initialization?
			please_die:1,	// when Allocating, check to abort
			alloc_in_exec:1,// vm alloc'd in executable
			alloc_in_self:1,// vm alloc'd in self
			kernel:1;	// is executable the kernel?
	int		version;	// server version.
	NXAtom		*symbols;	// required symbols in this reloc
	u_int		nsymbols;	// number of symbols used.
	NXAtom		reloc;		// filename of relocatable (for ld)
	u_int		reloc_date;	// modify time of reloc
	u_int		reloc_size;	// size of relocatable
	NXAtom		loadable;	// filename of loadable
	NXAtom		instance;	// name of instance var in kernel
	struct mach_header *header;	// Mach-O header
	vm_offset_t	vm_addr;	// objects text/data from segment
	vm_size_t	vm_size;	// size of vm portion of loadable
	vm_size_t	rld_size;	// size of rld output.
	vm_offset_t	server_start;	// where to startup server.
	port_name_t	server_port;	// port name of connection to kern srvr
	port_name_t	boot_port;	// kern server's bootport
	port_name_t	log_reply_port;	// port to send formatted log data to
	task_t		executable_task;// task port of executable to load into
	task_t		server_task;	// task port of server
	thread_t	server_thread;	// thread port of kern server
} server_t;

#if	DEBUG
#define server_lock(s)		{printf("locking server %s "		\
					"in file %s line %d\n",		\
					(s)->name, __FILE__, __LINE__);	\
				 mutex_lock((s)->lock);}
#define server_unlock(s)	{printf("unlocking server %s "		\
					"in file %s line %d\n",		\
					(s)->name, __FILE__, __LINE__);	\
				 mutex_unlock((s)->lock);}
#else	DEBUG
#define server_lock(s)		mutex_lock((s)->lock)
#define server_unlock(s)	mutex_unlock((s)->lock)
#endif	DEBUG

/*
 * Entry describing a message to be sent to kernel server
 */
typedef struct server_command {	// initialization/shutdown commands
	queue_chain_t	link;	// link entry
	enum {
		S_C_HMAP,
		S_C_SMAP,
		S_C_CALL,
		S_C_DEATH,
	} msg_type;
	NXAtom		port_name;
	NXAtom		function;
	NXAtom		arg;
	vm_address_t	funaddr;
	int		argval;
	port_name_t	port;
	boolean_t	advertised;
} server_command_t;

/*
 * Entry describing a notification forward request.
 */
typedef struct server_notify {
	queue_chain_t	link;		// link entry
	port_name_t	req_port;	// port to notify upon
	port_name_t	reply_port;	// where to reply
} server_notify_t;

/*
 * Types of ports that server owns.
 */
typedef enum server_port_type {
	BootPort,
	ServerPort,
	NotificationRequest,
	LogReplyPort,
	MappedPort,
	UnknownPort
} server_port_type_t;

/*
 * Protect all calls upon UNIX stuff.
 */
mutex_t unix_lock;
#define unix_lock()	mutex_lock(unix_lock)
#define unix_unlock()	mutex_unlock(unix_lock)

/*
 * Initialize the server stuff.
 */
void server_queue_init(void);
void obj_init(void);

/*
 * Read a script file describing the load/unload instructions for a
 * given kernel server and advertise the exported ports with the
 * netname server (nmserver).  The exported ports are added to the
 * port_set.
 */
kern_return_t server_init(const char *reloc, server_t **server);

/*
 * Given a port_name, find the server structure representing this port.
 */
server_t *server_by_port(port_name_t port_name);

/*
 * Return the server structure of the first server using this port as
 * a notification request port.
 */
server_t *server_by_notify_req(port_name_t port_name);

/*
 * Given a server name, return the server structure representing the server.
 */
server_t *server_by_name(const char *name);

/*
 * Given a port name and a server, find the server_command describing it.
 */
server_command_t *server_command_by_port_name(
	server_t	*server,
	const char	*name);

/*
 * Return the notification port for this notification request.  This
 * servers to remove this mapping from the server as well.
 */
port_t server_notify(server_t *server, port_t req_port);

/*
 * Record a notification request for this server.
 */
void server_req_notify(server_t *server, port_t req_port, port_t reply_port);

/*
 * Load the server associated with the given server structure into the kernel.
 * Once the server is loaded initialize it.
 */
kern_return_t server_load(server_t *server);

/*
 * Shutdown and unload the given server.
 */
kern_return_t server_unload(server_t *server);

/*
 * Deallocate the server.
 */
kern_return_t server_deallocate(server_t *server);

/*
 * Remove the server from the queue.
 */
void server_delete(server_t *server);

/*
 * Return the names of all known servers.
 */
kern_return_t server_list (
	server_name_t	**names,
	unsigned int	*count);

/*
 * Return information about a particular server.
 */
kern_return_t server_info (
	server_t	*server,
	port_name_t	**port_list,
	server_name_t	**names,
	boolean_t	**advertised,
	unsigned int	*count);

#endif _KERNLOAD_SERVER_
