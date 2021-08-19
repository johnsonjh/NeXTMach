/*
 * Copyright (c) 1988, by NeXT, Inc.
 */

/*
 * mount_clnt.c
 * Client routines for mount RPC calls.
 */
#import <sys/param.h>
#import <sys/dir.h>
#import <sys/user.h>
#import <sys/proc.h>
#import <sys/errno.h>
#import <sys/kernel.h>

#import <rpc/types.h>
#import <netinet/in.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/rpc_msg.h>
#import <nfs/nfs.h>
#import <rpcsvc/mount.h>

#import <sys/socket.h>
#import <net/if.h>

#define NRETRY	4	/* Number of times to retry getting a client */
#define RETRYSLEEP 15	/* Seconds to wait for program to register itself. */

static struct timeval wait = { 20, 0 };

extern struct ucred *rootcred;
extern int wakeup();

/*
 * Routine: mount_getclient
 * Function:
 *	Return a krpc client for the MOUNT rpc program.
 * Returns:
 *	0 on success
 *	-1 (<0) if 'program' was not registered
 *	1 (>0) if there was some other error
 */
int
mount_getclient(address, client)
	struct sockaddr_in	*address;
	CLIENT	**client;
{
	int error;

	if ((error = pmap_kgetport(address, MOUNTPROG, MOUNTVERS,
				   IPPROTO_UDP)) != 0) {
		return(error);
	}
	*client = clntkudp_create(address, MOUNTPROG, MOUNTVERS, NRETRY,
				  rootcred);
	if (*client == (CLIENT *) NULL) {
		return(1);
	}
	return(0);
}

/*
 * Routine: mount_mnt
 * Function:
 *	Call the MOUNT program MNT rpc.
 */
enum clnt_stat
mount_mnt(sin, pathp, server_fh)
	struct sockaddr_in	*sin;
	char			**pathp;
	fhandle_t		*server_fh;
{
	CLIENT			*client = NULL;
	XDR			xdr;
	enum clnt_stat		stat;
	struct fhstatus		mount_res;
	int			error;
	bool_t			alert_printed = FALSE;
	extern char		*mach_title;

	while (error = mount_getclient(sin, &client)) {
		if (error > 0) {
			error = EIO;
			goto errout;
		}
		/*
		 * Since error < 0, the program is not registered.
		 * Sleep for a while and then try again.  The server
		 * may be in the middle of booting.
		 */
		if (!alert_printed) {
			alert(60, 8, mach_title,
			" \nThe following network problem has occurred:\n%s%s",
"    NFS mount server not registered with portmapper.\n    Retrying.\n",
			   "See your system administrator if you need help.\n",
			      0, 0, 0, 0, 0, 0);
			alert_printed = TRUE;
		}
		timeout(wakeup, client, RETRYSLEEP * hz);
		sleep(client, PZERO);
	}

	if (alert_printed) {
		alert_done();
		alert_printed = FALSE;
	}

	while (TRUE) {
		bzero((caddr_t)&mount_res, sizeof(mount_res));
		stat = CLNT_CALL(client, MOUNTPROC_MNT, xdr_path, pathp,
				 xdr_fhstatus, &mount_res, wait);
		if (stat == RPC_SUCCESS) {
			goto out;
		} else if (stat == RPC_TIMEDOUT) {
			if (!alert_printed) {
				alert(60, 8, mach_title,
			" \nThe following network problem has occurred:\n%s%s",
"    NFS mount server not responding.\n    Retrying.\n",
			   "See your system administrator if you need help.\n",
				      0, 0, 0, 0, 0, 0);
				alert_printed = TRUE;
			}
		} else  {
			if (alert_printed) {
				alert_done();
			}
			printf("mount_mnt: failed\n");
			error = EIO;
			goto errout;
		}
	}

out:
	if (mount_res.fhs_status != 0) {
		if (alert_printed) {
			alert_done();
		}
		printf("mount_mnt: remote mount error = %d\n",
		       mount_res.fhs_status);
		error = mount_res.fhs_status;
		goto errout;
	}

	/* Things look good */
	*server_fh = mount_res.fhs_fh;
errout:
	if (alert_printed) {
		alert_done();
	}
	if (client) {
		AUTH_DESTROY(client->cl_auth);
		CLNT_DESTROY(client);
	}
	return (error);
}
