/*
 * Copyright (c) 1988, by NeXT, Inc.
 */

/*
 * bootparam_clnt.c
 * Client routines for bootparam RPC calls.
 */
#import <sys/param.h>
#import <sys/dir.h>
#import <sys/time.h>
#import <sys/errno.h>
#import <sys/kernel.h>

#import <rpc/types.h>
#import <netinet/in.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/rpc_msg.h>
#import <rpcsvc/bootparams.h>

#import <sys/socket.h>
#import <net/if.h>

#import <nextdev/kmreg.h>

/*
 * We have to misdefine this to avoid including sys/user.h which
 * brings in all kinds of crap that conflicts with bootparams.h.
 */
extern caddr_t rootcred;
extern int wakeup();

#define	RETRIES	5
#define RETRYSLEEP 15

#ifdef	KERNEL
/*
 * Routine: bootparams_getclient
 * Function:
 * 	Get a client for the bootparams RPC calls.
 * Returns:
 *	0 on success
 *	-1 (<0) if 'program' was not registered
 *	1 (>0) if there was some other error
 */
int
bootparams_getclient(address, client)
	struct sockaddr_in	*address;
	CLIENT	**client;
{
	int	error;

	if ((error = pmap_kgetport(address, BOOTPARAMPROG, BOOTPARAMVERS,
			  IPPROTO_UDP)) != 0) {
		return(error);
	}
	*client = clntkudp_create(address, BOOTPARAMPROG,
				  BOOTPARAMVERS, RETRIES, rootcred);
	if (*client == (CLIENT *)NULL) {
		return(1);
	}
	return(0);
}
#endif	KERNEL

static struct timeval wait = { 25, 0 };

/*
 * Routine: bootparams_getfile
 * Function:
 *	Call the bootparams GETFILE rpc with the given client name and
 *	file key.  Return the server name, file path, and server address.
 */
int
bootparams_getfile(sin, clientname, key, server_name, server_path)
	struct sockaddr_in	*sin;
	char			*clientname;
	char			*key;
	char			*server_name;
	char			*server_path;
{
	CLIENT			*client = NULL;
	XDR			xdr;
	struct bp_getfile_arg	getfile_arg;
	struct bp_getfile_res	getfile_res;
	enum clnt_stat		stat;
	bool_t			alert_printed = FALSE;
	int			error;

	while (error = bootparams_getclient(sin, &client)) {
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
			" \nThe following network problem has occurred.\n%s%s",
"    Bootparams server not registered with portmapper.\n    Retrying.\n",
			      "See your system administrator if you need help",
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

	/* Set up the getfile arguments */
	getfile_arg.client_name = clientname;
	getfile_arg.file_id = key;

	while (TRUE) {
		bzero((char *)&getfile_res, sizeof(getfile_res));
		stat = CLNT_CALL(client, BOOTPARAMPROC_GETFILE,
				 xdr_bp_getfile_arg, &getfile_arg,
				 xdr_bp_getfile_res, &getfile_res,
				 wait);
		if (stat == RPC_SUCCESS) {
			goto out;
		} else if (stat == RPC_TIMEDOUT) {
			extern char	*mach_title;

			if (!alert_printed) {
				alert(60, 8, mach_title,
		    " \nThe following network problem has occurred:\n%s%s%s%s",
"    Configuration server not responding with ", key,
" filesystem.\n    Retrying.\n",
			   "See your system administrator if you need help.\n",
				      0, 0, 0, 0);
				alert_printed = TRUE;
			}
		} else  {
			if (alert_printed) {
				alert_done();
			}
			printf("bootparams_getfile: failed\n");
			error = EIO;
			goto errout;
		}
	}

out:
	/* We got a response */
	strncpy(server_name, getfile_res.server_name, MAX_MACHINE_NAME);
	strncpy(server_path, getfile_res.server_path, MAX_PATH_LEN);
	sin->sin_addr.s_net =
		getfile_res.server_address.bp_address.ip_addr.net;
	sin->sin_addr.s_host =
		getfile_res.server_address.bp_address.ip_addr.host;
	sin->sin_addr.s_lh =
		getfile_res.server_address.bp_address.ip_addr.lh;
	sin->sin_addr.s_impno =
		getfile_res.server_address.bp_address.ip_addr.impno;

	/* Deallocate the response */
	xdr.x_op  = XDR_FREE; /* only thing looked at */
	if (!xdr_bp_getfile_res(&xdr, &getfile_res)) {
		panic("bootparams_getfile");
	}

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
