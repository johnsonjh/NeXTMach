/*
 * Copyright (c) 1988, NeXT Inc.
 */

/*
 **********************************************************************
 * HISTORY
 *  6-Sep-88  Peter King (king) at NeXT, Inc.
 *	Created.
 **********************************************************************
 */

#import <sys/types.h>
#import <sys/boolean.h>
#import <sys/systm.h>
#import <sys/reboot.h>
#import <sys/errno.h>
#import <sys/time.h>
#import <sys/socket.h>
#import <sys/param.h>
#import <rpc/rpc.h>
#import <nfs/nfs.h>
#import <nfs/nfs_clnt.h>
#import <rpcsvc/mount.h>
#import <rpcsvc/bootparams.h>


/*
 * Routine: getclient
 * Function:
 *	Get the clientname to use for the bootparams work.
 */
static void
getclientname(cpp)
	char	**cpp;
{
	char		*cp;
	extern char	hostname[];

	if (*cpp == NULL) {
		cp = (char *)kalloc(MAX_MACHINE_NAME+1);
		bzero(cp, MAX_MACHINE_NAME+1);
		printf("client name [%s]? ", hostname);
		gets(cp);
		if (*cp == '\0') {
			strncpy(cp, hostname, MAX_MACHINE_NAME);
		}
		*cpp = cp;
	}
}

/*
 * Routine: getfilekey
 * Function:
 *	Get the key to use for a particular filename.
 */
static char *
getfilekey(filename)
{
	char		*cp;

	cp = (char *)kalloc(MAX_FILEID+1);
	bzero(cp, MAX_FILEID+1);
	printf("%s key [%s]? ", filename, filename);
	gets(cp);
	if (*cp == '\0') {
		strncpy(cp, filename, MAX_FILEID);
	}
	return (cp);
}

	

/*
 * Routine: nfs_getremotevfs
 * Function:
 *	Get the necessary information from the BOOTPARAMS server to mount
 *	a remote VFS.
 */
nfs_getremotevfs(filename, server_name, server_sin, server_fh, mount_path)
	char *filename;
	char *server_name;
	struct sockaddr_in *server_sin;
	fhandle_t *server_fh;
	char *mount_path;
{
	static char		*clientname = NULL;
	char			*keyname = NULL;
	char			*name = NULL;
	char			*path;
	char			*cp;
	struct sockaddr_in	sin;
	register int		error;
	extern struct in_addr	bootparam_server;
	extern char 		hostname[];
	extern char		*index();

	/* If we have booted "-a" ask for a client name and key */
	if (boothowto & RB_ASKNAME) {
		getclientname(&clientname);
		keyname = getfilekey(filename);
	}

	/* Place to hold the GETFILE info */
	name = (char *)kalloc(MAX_MACHINE_NAME);
	path = (char *)kalloc(MAX_PATH_LEN);

	/* RPC the server that answered the WHOAMI */
	bzero(&sin, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_addr = bootparam_server;
	if (error = bootparams_getfile(&sin,
				       (clientname ? clientname : hostname),
				       (keyname ? keyname : filename),
				       name, path)) {
		goto errout;
	}

	printf("remote %s filesystem on %s:%s\n", filename, name, path);

	/* Check the path for a mount_point */
	if (cp = index(path, '@')) {
		*cp++ = '\0';
		if (mount_path) {
			strncpy(mount_path, cp, MAXPATHLEN);
		}
	}

	/* Here we are with the right information to do the MOUNT RPC. */

	if (error = mount_mnt(&sin, &path, server_fh)) {
		goto errout;
	}

	strncpy(server_name, name, HOSTNAMESZ);
	sin.sin_port = htons(NFS_PORT);
	*server_sin = sin;
errout:
	if (name) {
		kfree(name, MAX_FILEID+1);
		kfree(path, MAX_PATH_LEN);
	}
	if (keyname) {
		kfree(keyname, MAX_FILEID+1);
	}
	return(error);
}

/*
 * Routine: inet_aton
 * Function:
 *	Convert an ASCII network address into the real thing.
 */
int
inet_aton(string, addr)
	char *string;
	struct in_addr	*addr;
{
	register char *bp;
	register u_char *np;
	struct in_addr laddr;
	int	n;

	np = (u_char *)&laddr;
	n = 0;
	for (bp = string; *bp; bp++) {
		if (*bp == '.') {
			if (np >= ((u_char *)&laddr) + 3)
				return(FALSE);
			*np++ = (u_char) n;
			n = 0;
			continue;
		}
		if (*bp < '0' || *bp > '9') {
			return(FALSE);
		}
		n *= 10;
		n += *bp - '0';
		if (n >= 256 || n < 0)
			return(FALSE);
	}
	if (np != ((u_char *)&laddr) + 3)
		return(FALSE);
	*np = (u_char) n;
	*addr = laddr;
	return(TRUE);
}
