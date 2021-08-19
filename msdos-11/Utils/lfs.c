/*	@(#)lfs.c	2.0	26/06/90	(c) 1990 NeXT	*/

/* 
 * lfs.c -- Load Loadable File System
 *
 * HISTORY
 * 26-Jun-90	Doug Mitchell at NeXT
 *	Created.
 */

#import <sys/types.h>
#import <mach.h>
#import <stdlib.h>
#import <libc.h>
#import <sys/port.h>
#import <servers/netname.h>
#import <next/loadable_fs.h>

extern void usage(char **argv);
extern void mach_error(char *s, kern_return_t krtn);

static struct load_fs_msg lmsg = {
	{					/* lfm_header */
		0,				/* msg_unused */
		TRUE,				/* msg_simple */
		sizeof(struct load_fs_msg),	/* msg_size */
		MSG_TYPE_NORMAL,		/* msg_type */
		(port_t)0,			/* msg_local_port */
		(port_t)0,			/* msg_remote_port */
		LFM_ID				/* msg_id */
	},
	{					/* lfm_status_t */
		MSG_TYPE_INTEGER_32,		/* msg_type_name */
		32,				/* msg_type_size */
		1,				/* msg_type_number */
		TRUE,				/* msg_type_inline */
		FALSE,				/* msg_type_longform */
		FALSE,				/* msg_type_deallocate */
		0				/* msg_type_unused */
	},
	LFM_STAT_UNDEFINED			/* lfm_status */
};


main(int argc, char **argv)
{
	kern_return_t krtn;
	int arg;
	char *hostname = "";
	char c;
	port_t local_port, fs_port;
	int msg_id=LFM_ID;
	
	if(argc < 2) 
		usage(argv);
	for(arg=2; arg<argc; arg++) {
		c = argv[arg][0];
		switch(c) {
		    case 'h':
		    	hostname = &argv[arg][2];
			break;
		    case 'd':
		    	msg_id = LFM_DUMP_MOUNT;
			break;
		    case 't':
		    	msg_id = LFM_TERMINATE;
			break;
		    case 'p':
		    	msg_id = LFM_PURGE;
			break;
		    case 'l':
		    	msg_id = LFM_DUMP_LOCAL;
			break;
		    case 'e':
		    	msg_id = LFM_ENABLE_TRACE;
			break;
		    case 'D':
		    	msg_id = LFM_DISABLE_TRACE;
			break;
		    case 'i':
		    	msg_id = atoi(&argv[arg][2]) + LFM_ID;
			break;
		    default:
		    	usage(argv);
		}
	}
	krtn = port_allocate(task_self(), &local_port);
	if(krtn) {
		mach_error("port_allocate", krtn);
		exit(1);
	}
	krtn = netname_look_up(name_server_port, hostname, argv[1], &fs_port);
	if(krtn) {
	    	mach_error("netname_look_up", krtn);
		exit(1);
	}
	lmsg.lfm_header.msg_local_port = local_port;
	lmsg.lfm_header.msg_remote_port = fs_port;
	lmsg.lfm_header.msg_id = msg_id;
	krtn = msg_rpc(&lmsg.lfm_header,
		MSG_OPTION_NONE,	
		lmsg.lfm_header.msg_size,		
		0,			/* send_timeout */
		0);			/* rcv_timeout */
	if(krtn) {
		mach_error("msg_rpc", krtn);
		exit(1);
	}
	if(lmsg.lfm_status) {
		printf("RPC ERROR: lfm_status = %d\n", lmsg.lfm_status);
		exit(1);
	}
	printf("...OK\n");
	exit(0);
}

void usage(char **argv)
{
	printf("usage: %s file_system_name [h=hostname] [action]\n", argv[0]);
	printf("action:\n");
	printf("       d(dump mount info)\n");
	printf("       t(terminate)\n");
	printf("       p(purge vnodes)\n");
	printf("       l(dump local info)\n");
	printf("       e(enable debug trace)\n");
	printf("       D(disable debug trace)\n");
	printf("       i=msg_id_offset\n");
	exit(1);
}