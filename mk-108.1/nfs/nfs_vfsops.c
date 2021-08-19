/*
 * HISTORY
 *  6-Dec-89  Morris Meyer (mmeyer) at NeXT
 *	Moved the mount reference count check the beginning of nfs_unmount().
 *	An unmount that failed would nuke the vnode of the mount point.
 *	
 *  9-May-89  Peter King (king) at NeXT
 *	Sun Bugfixes: 1010518 - Close to Open consistency of cached attrs.
 *		      1014020 - The whoami routine does not free all the
 *				space it allocates.
 *		      1015493 - Backoff retransmission of broadcast
 *				pmap_rmtcall bootparams whoami calls.
 *
 * 20-Dec-88  Peter King (king) at NeXT
 *	NFS 4.0 Changes.
 *
 * 28-Oct-87  Peter King (king) at NeXT, Inc.
 *	Original Sun source, ported to  Mach.
 */ 

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#import <secure_nfs.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/user.h>
#import <sys/vfs.h>
#import <sys/vfs_stat.h>
#import <sys/vnode.h>
#import <sys/pathname.h>
#import <sys/uio.h>
#import <sys/socket.h>
#import <sys/socketvar.h>
#import <sys/kernel.h>
#if	NeXT
#import <sys/time.h>
#endif	NeXT
#import <sys/mount.h>
#import <kern/thread.h>
#import <netinet/in.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/pmap_rmt.h>
#import <rpc/pmap_prot.h>
#import <rpcsvc/bootparam.h>
#import <nfs/nfs.h>
#import <nfs/nfs_clnt.h>
#import <nfs/rnode.h>
#import <nfs/nfs_mount.h>
#import <net/if.h>
#import <net/route.h>
#import <rpcsvc/mount.h>
#import <sys/bootconf.h>
#import <sys/ioctl.h>

#if	MACH
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/if_ether.h>
#import <kern/zalloc.h>

extern struct zone *vfs_zone;
#endif	MACH

#define	DEFAULTPRIVATEDIR	"/private"

#define	satosin(sa)	((struct sockaddr_in *)(sa))

#ifdef NFSDEBUG
extern int nfsdebug;
#endif

int nfsrootvp();
extern struct vnode *makenfsnode();
#define MAPSIZE  256/NBBY
static char nfs_minmap[MAPSIZE]; /* Map for minor device allocation */

int nfsmntno;
static addr_to_str();

/*
 * nfs vfs operations.
 */
int nfs_mount();
int nfs_unmount();
int nfs_root();
int nfs_statfs();
int nfs_sync();
int nfs_mountroot();
#if	!MACH
int nfs_swapvp();
#endif	!MACH
int nfs_badop();

struct vfsops nfs_vfsops = {
	nfs_mount,
	nfs_unmount,
	nfs_root,
	nfs_statfs,
	nfs_sync,
	nfs_badop,
	nfs_mountroot,
#if	!MACH
	nfs_swapvp,
#endif	!MACH
};


/*
 * Called by vfs_mountroot when nfs is going to be mounted as root
 */
int
nfs_mountroot(vfsp, vpp, name)
	struct vfs *vfsp;
	struct vnode **vpp;
	char *name;
{
	struct sockaddr_in root_sin;
	struct vnode *rtvp;
	char *root_path;
	char root_hostname[MAXHOSTNAMELEN+1];
	fhandle_t root_fhandle;
	int rc;
	struct pathname pn;
#if	NeXT
	char private_name[MAX_FILEID];
	struct vnode *pvp;
	struct vfs *private_vfsp;
	char *cp;
	extern char *index();
#endif	NeXT

	/* do this BEFORE getfile which causes xid stamps to be initialized */
#if	NeXT
	{
		extern struct timeval time, rtc_get();
		time = rtc_get();
		inittodr(time.tv_sec);
	}
	getfsname("root", name);
#else	NeXT
	inittodr(-1L);          /* hack for now - until we get time svc? */
#endif	NeXT

	pn_alloc(&pn);
	root_path = pn.pn_path;
	do {
#if	NeXT
		rc = getfile(*name ? name : "root", root_hostname,
			    (struct sockaddr *)&root_sin, root_path);
#else	NeXT
		rc = getfile("root", root_hostname,
		    (struct sockaddr *)&root_sin, root_path);
#endif	NeXT
	} while (rc == ETIMEDOUT);
	if (rc) {
		pn_free(&pn);
		return (rc);
	}
	rc = mountnfs(&root_sin, root_hostname, root_path, &root_fhandle);
	if (rc) {
		pn_free(&pn);
		printf("mount root %s:%s failed, rpc status %d\n",
			root_hostname, root_path, rc);
		return (rc);
	}
	rc = nfsrootvp(&rtvp, vfsp, &root_sin, &root_fhandle, root_hostname,
	    (char *)NULL, -1, 0);
	if (rc) {
		pn_free(&pn);
		return (rc);
	}
	if (rc = vfs_add((struct vnode *)0, vfsp, 0)) {
		pn_free(&pn);
		return (rc);
	}
	/*
	 * Set maximum attribute timeouts and turn off close-to-open
	 * consistency checking
	 */
	vftomi(vfsp)->mi_acregmin = ACMINMAX;
	vftomi(vfsp)->mi_acregmax = ACMAXMAX;
	vftomi(vfsp)->mi_acdirmin = ACMINMAX;
	vftomi(vfsp)->mi_acdirmax = ACMAXMAX;
	vftomi(vfsp)->mi_nocto = 1;
	vfs_unlock(rtvp->v_vfsp);
	*vpp = rtvp;
	{
		register char *nm;
		extern char *strcpy();

		nm = strcpy(name, root_hostname);
		*nm++ = ':';
		(void) strcpy(nm, root_path);
	}

#if	NeXT
	/*
	 * Now we get our private partition and mount it up.
	 */
	*private_name = '\0';
	getfsname("private", private_name);
	do {
	    rc = getfile(*private_name ? private_name : "private",
			 root_hostname, (struct sockaddr *)&root_sin,
			 root_path);
	} while (rc == ETIMEDOUT);
	if (rc) {
		pn_free(&pn);
		return (rc);
	}
	/*
	 * Scan for place to mount the private filesystem.
	 */
	if (cp = index(root_path, '@')) {
		*cp++ = '\0';
	} else {
		cp = DEFAULTPRIVATEDIR;
	}
	rc = mountnfs(&root_sin, root_hostname, root_path, &root_fhandle);
	if (rc) {
		pn_free(&pn);
		printf("mount private %s:%s failed, rpc status %d\n",
			root_hostname, root_path, rc);
		return (rc);
	}

	/*
	 * Temporarily set up rootdir, u.u_rdist and u.u_cdir so that
	 * lookuppn works.  We will undo this because vfs_mountroot
	 * does it as well.
	 */
	rc = VFS_ROOT(rootvfs, &rootdir);
	if (rc)
		panic("nfs_mountroot: can't find root vnode");
	u.u_cdir = rootdir;
	VN_HOLD(u.u_cdir);
	u.u_rdir = NULL;

	rc = lookupname(cp, UIO_SYSSPACE, FOLLOW_LINK,
			   (struct vnode **)0, &pvp);
	if (rc || pvp == NULL) {
		printf("nfs_mountroot: no place to mount private dir\n");
		VN_RELE(u.u_cdir);
		pn_free(&pn);
		return (rc);
	}
	VN_RELE(u.u_cdir);
	VN_RELE(rootdir);
	dnlc_purge();
	private_vfsp = (struct vfs *)kalloc(sizeof (struct vfs));
	VFS_INIT(private_vfsp, &nfs_vfsops, (caddr_t)0);
	rc = nfsrootvp(&rtvp, private_vfsp, &root_sin, &root_fhandle,
		       root_hostname, (char *)NULL, -1, 0);
	if (rc) {
		pn_free(&pn);
		kfree(private_vfsp, sizeof (struct vfs));
		return (rc);
	}

	if (rc = vfs_add(pvp, private_vfsp, 0)) {
		nfs_unmount(private_vfsp);
		pn_free(&pn);
		kfree(private_vfsp, sizeof (struct vfs));
		return (rc);
	}
	vftomi(vfsp)->mi_acregmax = 100 * ACREGMAX;
	vftomi(vfsp)->mi_acdirmax = 100 * ACDIRMAX;
	strncpy(vfsp->vfs_name, pn.pn_path, MAXNAMLEN);
	vfs_unlock(rtvp->v_vfsp);

	/*
	 * Increase max client handles and prealloc
	 */
	nfs_netboot_prealloc(vftomi(vfsp));
#endif	NeXT
	pn_free(&pn);
	return (0);
}

#if	!MACH
extern struct sockaddr_in nfsdump_sin;
extern fhandle_t nfsdump_fhandle;
extern int nfsdump_maxcount;

/*
 * Set up for swapping to NFS.
 * Call nfsrootvp to set up the
 * RPC/NFS machinery.
 */
int
nfs_swapvp(vfsp, vpp, path)
	struct vfs *vfsp;
	struct vnode **vpp;
	char *path;
{
	struct sockaddr_in swap_sin;
	char *swap_path;
	char swap_hostname[MAXHOSTNAMELEN + 1];
	fhandle_t swap_fhandle;
	int rc;
	struct vattr va;
	struct vfs *dumpvfsp;

	swap_path = kmem_alloc(MAX_MACHINE_NAME + 1);
#ifdef sun
	getfsname("swap", path);
#endif
	do {
#ifdef sun
		rc = getfile(*path ? path : "swap", swap_hostname,
			    (struct sockaddr *)&swap_sin, swap_path);
#else
		rc = getfile("swap", swap_hostname,
		    (struct sockaddr *)&swap_sin, swap_path);
#endif
		
	} while (rc == ETIMEDOUT);
	if (rc != 0) 
		goto error;

	rc = mountnfs(&swap_sin, swap_hostname, swap_path, &swap_fhandle);
	if (rc != 0) {
		printf("mount swapfile %s:%s failed, rpc status %d\n",
			swap_hostname, swap_path, rc);
		goto error;
	}
	rc = nfsrootvp(vpp, vfsp, &swap_sin, &swap_fhandle,
	    swap_hostname, (char *)NULL, -1, 0);
	if (rc != 0) 
		goto error;

	hostpath(path, swap_hostname, swap_path);
	(void)VOP_GETATTR(*vpp, &va, u.u_cred);
	nfsdump_maxcount = va.va_size;
	nfsdump_fhandle = swap_fhandle;
	nfsdump_sin = swap_sin;

	/*
	 * While we're at it, configure dump too.  We need to be sure to
	 * call VOP_GETATTR for the dump file or else it'll never get done.
	 */
	rc = getfile("dump", swap_hostname, 
			(struct sockaddr *)&nfsdump_sin, swap_path);
	if (rc != 0) 
		goto done;

	rc = mountnfs(&nfsdump_sin, swap_hostname, 
				swap_path, &nfsdump_fhandle);
	if (rc != 0) {
		printf("mount dumpfile %s:%s failed, rpc status %d\n",
		       swap_hostname, swap_path, rc);
		goto done;
	}

	dumpvfsp = (struct vfs *)kmem_alloc(sizeof (*vfsp));
	VFS_INIT(dumpvfsp, &nfs_vfsops, (caddr_t)0);
	rc = nfsrootvp(&dumpfile.bo_vp, dumpvfsp, &nfsdump_sin, 
		    &nfsdump_fhandle, swap_hostname, (char *)NULL, -1, 0);
	if (rc != 0) {
		kmem_free((caddr_t)dumpvfsp, sizeof (*vfsp));
		goto done;
	}
	(void) strcpy(dumpfile.bo_fstype, "nfs");
	hostpath(dumpfile.bo_name, swap_hostname, swap_path);
	(void) VOP_GETATTR(dumpfile.bo_vp, &va, u.u_cred);
	dumplo = 0;
	nfsdump_maxcount = 0;

done:
	kmem_free(swap_path, MAX_MACHINE_NAME + 1);
	return (0);

error:
	kmem_free(swap_path, MAX_MACHINE_NAME + 1);
	return (rc);
}
#endif	!MACH


/*
 * pmapper remote-call-service interface.
 * This routine is used to call the pmapper remote call service
 * which will look up a service program in the port maps, and then
 * remotely call that routine with the given parameters.  This allows
 * programs to do a lookup and call in one step.
*/
enum clnt_stat
pmap_rmtcall(call_addr, progn, versn, procn, xdrargs, argsp, xdrres, resp, tout, resp_addr)
	struct sockaddr_in *call_addr;
	u_long progn, versn, procn;
	xdrproc_t xdrargs, xdrres;
	caddr_t argsp, resp;
	struct timeval tout;
	struct sockaddr_in *resp_addr;
{
	register CLIENT *client;
	struct rmtcallargs a;
	struct rmtcallres r;
	enum clnt_stat stat, clntkudp_callit_addr();
	u_long port;

	call_addr->sin_port = htons(PMAPPORT);
#define PMAP_RETRIES 5
	client = clntkudp_create(call_addr, PMAPPROG, PMAPVERS, PMAP_RETRIES,
				 u.u_cred);
	if (client != (CLIENT *)NULL) {
		a.prog = progn;
		a.vers = versn;
		a.proc = procn;
		a.args_ptr = argsp;
		a.xdr_args = xdrargs;
		r.port_ptr = &port;
		r.results_ptr = resp;
		r.xdr_results = xdrres;
		stat = clntkudp_callit_addr(client, PMAPPROC_CALLIT,
		    xdr_rmtcall_args, (caddr_t)&a,
		    xdr_rmtcallres, (caddr_t) &r, tout, resp_addr);
		resp_addr->sin_port = htons((u_short) port);
		CLNT_DESTROY(client);
	} else {
		panic("pmap_rmtcall: clntkudp_create failed");
	}
	return (stat);
}

struct ifnet *ifb_ifwithaf();

static struct sockaddr_in bootparam_addr;

int
whoami()
{
	struct sockaddr_in sa;
	struct ifnet *ifp;
	struct bp_whoami_arg arg;
	struct bp_whoami_res res;
	struct timeval tv;
	struct in_addr ipaddr;
	enum clnt_stat status;
	struct rtentry rtentry;
	struct sockaddr_in *sin;
	struct ifreq req;
	int error = 0;
	int printed_waiting_msg;
	enum clnt_stat callrpc();

	bzero((caddr_t)&sa, sizeof sa);
	ifp = ifb_ifwithaf(AF_INET);
	if (ifp == 0) {
		printf("whoami: zero ifp\n");
		return (EHOSTUNREACH);
	}
#if	NeXT
	if (initrootnet()) {
		panic("whoami: initrootnet failed");
	}
#else	NeXT
	if (!address_known(ifp)) {
		revarp_myaddr(ifp);
	}
#endif	NeXT

#ifdef notdef
	sa = *((struct sockaddr_in *)&(ifp->if_broadaddr));
#else
	/*
	 * Pick up the interface broadcast address.
	 */
	if ((error = in_control((struct socket *)0, SIOCGIFBRDADDR,
	    (caddr_t)&req, ifp)) != 0) {
		printf("whoami: in_control 0x%x if_flags 0x%x\n",
			error, ifp->if_flags);
		panic("bad SIOCGIFBRDADDR in_control");
	}
	bcopy((caddr_t)&req.ifr_dstaddr, (caddr_t)&sa,
	    sizeof (struct sockaddr_in));
#endif notdef

	arg.client_address.address_type = IP_ADDR_TYPE;

#ifdef notdef
	ipaddr = ((struct sockaddr_in *)&ifp->if_addr)->sin_addr;
#else
	if (in_control((struct socket *)0, SIOCGIFADDR, (caddr_t)&req,
	    ifp) != 0)
		panic("bad SIOCGIFADDR in_control");
	ipaddr = (satosin (&req.ifr_addr)->sin_addr);
#endif notdef

	bcopy((caddr_t)&ipaddr, (caddr_t)&arg.client_address.bp_address.ip_addr,
	    sizeof (struct in_addr));

	/* initial retransmission interval */
	tv.tv_sec = 3;
	tv.tv_usec = 0;
#if	MACH
	res.client_name = (bp_machine_name_t)kalloc(MAX_MACHINE_NAME + 1);
	res.domain_name = (bp_machine_name_t)kalloc(MAX_MACHINE_NAME + 1);
#else	MACH
	res.client_name = (bp_machine_name_t)kmem_alloc(MAX_MACHINE_NAME + 1);
	res.domain_name = (bp_machine_name_t)kmem_alloc(MAX_MACHINE_NAME + 1);
#endif	MACH

	printed_waiting_msg = 0;
	do {
		status = pmap_rmtcall(&sa, (u_long)BOOTPARAMPROG,
		    (u_long)BOOTPARAMVERS, (u_long)BOOTPARAMPROC_WHOAMI,
		    xdr_bp_whoami_arg, (caddr_t)&arg,
		    xdr_bp_whoami_res, (caddr_t)&res,
		    tv, &bootparam_addr);
		if (status == RPC_TIMEDOUT && !printed_waiting_msg) {
			printf("No bootparam server responding; still trying\n");
			printf("whoami: pmap_rmtcall status 0x%x\n", status);
			printed_waiting_msg = 1;
		}
		/* 
		 * Retransmission interval for second and subsequent tries.
		 * We expect first pmap_rmtcall to retransmit and backoff to
		 * at least this value.
		 */
		tv.tv_sec = 20;
		tv.tv_usec = 0;
	} while (status == RPC_TIMEDOUT);

	if (printed_waiting_msg)
		printf("Bootparam response received\n");

	if (status != RPC_SUCCESS) {
		/*
		 * XXX should get real error here
		 */
		error = (int)status;
		printf("whoami RPC call failed with status %d\n", error);
		goto done;
	}

	hostnamelen = strlen(res.client_name);
	if (hostnamelen > sizeof hostname) {
		printf("whoami: hostname too long");
		error = ENAMETOOLONG;
		goto done;
	}
	if (hostnamelen > 0) {
		bcopy((caddr_t)res.client_name, (caddr_t)hostname,
		    (u_int)hostnamelen);
	} else {
		printf("whoami: no host name\n");
		error = ENXIO;
		goto done;
	}
	printf("hostname: %s\n", hostname);

	domainnamelen = strlen(res.domain_name);
	if (domainnamelen > sizeof domainname) {
		printf("whoami: domainname too long");
		error = ENAMETOOLONG;
		goto done;
	}
	if (domainnamelen > 0) {
		bcopy((caddr_t)res.domain_name, (caddr_t)domainname,
		    (u_int)domainnamelen);
		printf("domainname: %s\n", domainname);
	} else {
		printf("whoami: no domain name\n");
	}
#if	NeXT
#else	NeXT
	bcopy((caddr_t)&res.router_address.bp_address.ip_addr, (caddr_t)&ipaddr,
	    sizeof (struct in_addr));
	if (ipaddr.s_addr != (u_long) 0) {
		if (res.router_address.address_type == IP_ADDR_TYPE) {
			sin = (struct sockaddr_in *)&rtentry.rt_dst;
			bzero((caddr_t)sin, sizeof *sin);
			sin->sin_family = AF_INET;
			sin = (struct sockaddr_in *)&rtentry.rt_gateway;
			bzero((caddr_t)sin, sizeof *sin);
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = ipaddr.s_addr;
			rtentry.rt_flags = RTF_GATEWAY | RTF_UP;
			(void) rtrequest(SIOCADDRT, &rtentry);
		} else {
			printf("whoami: unknown gateway addr family %d\n",
			    res.router_address.address_type);
		}
	}
#endif	NeXT

done:
#if	MACH
	kfree(res.client_name, MAX_MACHINE_NAME + 1);
	kfree(res.domain_name, MAX_MACHINE_NAME + 1);
#else	MACH
	kmem_free(res.client_name, MAX_MACHINE_NAME + 1);
	kmem_free(res.domain_name, MAX_MACHINE_NAME + 1);
#endif	MACH
	return error;
}

enum clnt_stat
callrpc(sin, prognum, versnum, procnum, inproc, in, outproc, out)
	struct sockaddr_in *sin;
	u_long prognum, versnum, procnum;
	xdrproc_t inproc, outproc;
	char *in, *out;
{
	CLIENT *cl;
	struct timeval tv;
	enum clnt_stat cl_stat;

	cl = clntkudp_create(sin, prognum, versnum, 5, u.u_cred);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	cl_stat = CLNT_CALL(cl, procnum, inproc, in, outproc, out, tv);
	AUTH_DESTROY(cl->cl_auth);
	CLNT_DESTROY(cl);
	return (cl_stat);
}

int
getfile(fileid, server_name, server_address, server_path)
	char *fileid;
	char *server_name;
	struct sockaddr *server_address;
	char *server_path;
{
	struct bp_getfile_arg arg;
	struct bp_getfile_res res;
	enum clnt_stat status;
	int tries;
	int error;
	struct in_addr ipaddr;

	arg.client_name = hostname;
	arg.file_id = fileid;
	bzero((caddr_t)&res, sizeof res);
	if (bootparam_addr.sin_addr.s_addr == 0) {
		if ((error = whoami()) != 0) 
			return error;
	}
#if	MACH
	res.server_name = (bp_machine_name_t)kalloc(MAX_MACHINE_NAME + 1);
	res.server_path = (bp_machine_name_t)kalloc(MAX_MACHINE_NAME + 1);
#else	MACH
	res.server_name = (bp_machine_name_t)kmem_alloc(MAX_MACHINE_NAME + 1);
	res.server_path = (bp_machine_name_t)kmem_alloc(MAX_MACHINE_NAME + 1);
#endif	MACH
	for (tries = 0; tries < 5; tries++) {
		status = callrpc(&bootparam_addr, (u_long)BOOTPARAMPROG,
		    (u_long)BOOTPARAMVERS, (u_long)BOOTPARAMPROC_GETFILE,
		    xdr_bp_getfile_arg, (caddr_t)&arg,
		    xdr_bp_getfile_res, (caddr_t)&res);
		if (status != RPC_TIMEDOUT)
			break;
	}
	if (status == RPC_SUCCESS) {
		(void) strcpy(server_name, res.server_name);
		(void) strcpy(server_path, res.server_path);
	}
#if	MACH
	kfree(res.server_name, MAX_MACHINE_NAME + 1);
	kfree(res.server_path, MAX_MACHINE_NAME + 1);
#else	MACH
	kmem_free(res.server_name, MAX_MACHINE_NAME + 1);
	kmem_free(res.server_path, MAX_MACHINE_NAME + 1);
#endif	MACH
	if (status != RPC_SUCCESS)
		return (status == RPC_TIMEDOUT) ? ETIMEDOUT : (int)status;
	bcopy((caddr_t)&res.server_address.bp_address.ip_addr, (caddr_t)&ipaddr,
	    sizeof (struct in_addr));

	if (*server_name == '\0' || *server_path == '\0' ||
	    ipaddr.s_addr == 0) {
		return (EINVAL);
	}
	switch (res.server_address.address_type) {
	case IP_ADDR_TYPE:
		bzero((caddr_t)server_address, sizeof *server_address);
		server_address->sa_family = AF_INET;
		satosin(server_address)->sin_addr.s_addr = ipaddr.s_addr;
		break;
	default:
		printf("getfile: unknown address type %d\n",
			res.server_address.address_type);
		return (EPROTONOSUPPORT);
	}
	return (0);
}

/*
 * Call mount daemon on server sin to mount path.
 * sin_port is set to nfs port and fh is the fhandle
 * returned from the server.
 */

mountnfs(sin, server, path, fh)
	struct sockaddr_in *sin;
	char *server;
	char *path;
	fhandle_t *fh;
{
	struct fhstatus fhs;
	int error;
	enum clnt_stat status;

	do {
		error = pmap_kgetport(sin, (u_long)MOUNTPROG,
		    (u_long)MOUNTVERS, (u_long)IPPROTO_UDP);
		if (error == -1) {
			return ((int)RPC_PROGNOTREGISTERED);
		} else if (error == 1) {
			printf("mountnfs: %s:%s portmap not responding\n",
			    server, path);
		}
	} while (error == 1);
	do {
		status = callrpc(sin, (u_long)MOUNTPROG, (u_long)MOUNTVERS,
		    (u_long)MOUNTPROC_MNT,
		    xdr_bp_path_t, (caddr_t)&path,
		    xdr_fhstatus, (caddr_t)&fhs);
		if (status == RPC_TIMEDOUT) {
			printf("mountnfs: %s:%s mount server not responding\n",
			    server, path);
		}
	} while (status == RPC_TIMEDOUT);
	if (status != RPC_SUCCESS) {
		return ((int)status);
	}
	sin->sin_port = htons(NFS_PORT);
	*fh = fhs.fhs_fh;
	return ((int)fhs.fhs_status);
}

/*
 * nfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
int
nfs_mount(vfsp, path, data)
	struct vfs *vfsp;
	char *path;
	caddr_t data;
{
	int error;
	struct vnode *rtvp = NULL;	/* the server's root */
	struct mntinfo *mi;		/* mount info, pointed at by vfs */
	fhandle_t fh;			/* root fhandle */
	struct sockaddr_in saddr;	/* server's address */
	char shostname[HOSTNAMESZ];	/* server's hostname */
	int hlen;			/* length of hostname */
	char netname[MAXNETNAMELEN+1];	/* server's netname */
	int nlen;			/* length of netname */
	struct nfs_args args;		/* nfs mount arguments */

	/*
	 * For now, ignore remount option.
	 */
	if (vfsp->vfs_flag & VFS_REMOUNT) {
		return (0);
	}
	/*
	 * get arguments
	 */
	error = copyin(data, (caddr_t)&args, sizeof (args));
	if (error) {
		goto errout;
	}

	/*
	 * Get server address
	 */
	error = copyin((caddr_t)args.addr, (caddr_t)&saddr,
	    sizeof(saddr));
	if (error) {
		goto errout;
	}
	/*
	 * For now we just support AF_INET
	 */
	if (saddr.sin_family != AF_INET) {
		error = EPFNOSUPPORT;
		goto errout;
	}

	/*
	 * Get the root fhandle
	 */
	error = copyin((caddr_t)args.fh, (caddr_t)&fh, sizeof(fh));
	if (error) {
		goto errout;
	}

	/*
	 * Get server's hostname
	 */
	if (args.flags & NFSMNT_HOSTNAME) {
		error = copyinstr(args.hostname, shostname,
			sizeof (shostname), (u_int *)&hlen);
		if (error) {
			goto errout;
		}
	} else {
		addr_to_str(&saddr, shostname);
	}

	/*
	 * Get server's netname
	 */
	if (args.flags & NFSMNT_SECURE) {
		error = copyinstr(args.netname, netname, sizeof (netname),
			(u_int *)&nlen);
	} else {
		nlen = -1;
	}

	/*
	 * Get root vnode.
	 */
	error = nfsrootvp(&rtvp, vfsp, &saddr, &fh, shostname, netname, nlen,
	    args.flags);
	if (error)
		return (error);

	/*
	 * Set option fields in mount info record
	 */
	mi = vtomi(rtvp);
	mi->mi_noac = ((args.flags & NFSMNT_NOAC) != 0);
	mi->mi_nocto = ((args.flags & NFSMNT_NOCTO) != 0);
	if (args.flags & NFSMNT_RETRANS) {
		mi->mi_retrans = args.retrans;
		if (args.retrans < 0) {
			error = EINVAL;
			goto errout;
		}
	}
	if (args.flags & NFSMNT_TIMEO) {
		mi->mi_timeo = args.timeo;
		if (args.timeo <= 0) {
			error = EINVAL;
			goto errout;
		}
	}
	if (args.flags & NFSMNT_RSIZE) {
		if (args.rsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_tsize = MIN(mi->mi_tsize, args.rsize);
	}
	if (args.flags & NFSMNT_WSIZE) {
		if (args.wsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_stsize = MIN(mi->mi_stsize, args.wsize);
	}
	if (args.flags & NFSMNT_ACREGMIN) {
		if (args.acregmin < 0) {
			mi->mi_acregmin = ACMINMAX;
		} else if (args.acregmin == 0) {
			error = EINVAL;
			printf("nfs_mount: acregmin == 0\n");
			goto errout;
		} else {
			mi->mi_acregmin = min(args.acregmin, ACMINMAX);
		}
	}
	if (args.flags & NFSMNT_ACREGMAX) {
		if (args.acregmax < 0) {
			mi->mi_acregmax = ACMAXMAX;
		} else if (args.acregmax < mi->mi_acregmin) {
			error = EINVAL;
			printf("nfs_mount: acregmax < acregmin\n");
			goto errout;
		} else {
			mi->mi_acregmax = min(args.acregmax, ACMAXMAX);
		}
	}
	if (args.flags & NFSMNT_ACDIRMIN) {
		if (args.acdirmin < 0) {
			mi->mi_acdirmin = ACMINMAX;
		} else if (args.acdirmin == 0) {
			error = EINVAL;
			printf("nfs_mount: acdirmin == 0\n");
			goto errout;
		} else {
			mi->mi_acdirmin = min(args.acdirmin, ACMINMAX);
		}
	}
	if (args.flags & NFSMNT_ACDIRMAX) {
		if (args.acdirmax < 0) {
			mi->mi_acdirmax = ACMAXMAX;
		} else if (args.acdirmax < mi->mi_acdirmin) {
			error = EINVAL;
			printf("nfs_mount: acdirmax < acdirmin\n");
			goto errout;
		} else {
			mi->mi_acdirmax = min(args.acdirmax, ACMAXMAX);
		}
	}
#ifdef NFSDEBUG
	dprint(nfsdebug, 1,
	    "nfs_mount: hard %d timeo %d retries %d wsize %d rsize %d\n",
	    mi->mi_hard, mi->mi_timeo, mi->mi_retrans, mi->mi_stsize,
	    mi->mi_tsize);
	dprint(nfsdebug, 1,
	    "           regmin %d regmax %d dirmin %d dirmax %d\n",
	    mi->mi_acregmin, mi->mi_acregmax, mi->mi_acdirmin, mi->mi_acdirmax);
#endif

errout:
	if (error) {
		if (rtvp) {
			VN_RELE(rtvp);
		}
	}
	return (error);
}

#if	PROTO
int	slownfs = 0;
#endif	PROTO

int
nfsrootvp(rtvpp, vfsp, sin, fh, shostname, netname, nlen, flags)
	struct vnode **rtvpp;		/* where to return root vp */
	register struct vfs *vfsp;	/* vfs of fs, if NULL make one */
	struct sockaddr_in *sin;	/* server address */
	fhandle_t *fh;			/* swap file fhandle */
	char *shostname;		/* server's hostname */
	char *netname;			/* server's netname */
	int nlen;			/* length of netname, -1 if none */
	int flags;			/* mount flags */
{
	register struct vnode *rtvp = NULL;	/* the server's root */
	register struct mntinfo *mi = NULL;	/* mount info, pointed at by vfs */
	struct vattr va;		/* root vnode attributes */
	struct nfsfattr na;		/* root vnode attributes in nfs form */
	struct statfs sb;		/* server's file system stats */
	register int error;

	/*
	 * Create a mount record and link it to the vfs struct
	 */
#if	MACH
	mi = (struct mntinfo *)kalloc(sizeof (*mi));
	bzero((caddr_t)mi, sizeof (*mi));
#else	MACH
	mi = (struct mntinfo *)kmem_zalloc(sizeof (*mi));
#endif	MACH
	mi->mi_hard = ((flags & NFSMNT_SOFT) == 0);
	mi->mi_int = ((flags & NFSMNT_INT) != 0);
	mi->mi_addr = *sin;
	mi->mi_retrans = NFS_RETRIES;
	mi->mi_timeo = NFS_TIMEO;
	mi->mi_mntno = vfs_getnum(nfs_minmap, MAPSIZE);
	bcopy(shostname, mi->mi_hostname, HOSTNAMESZ);
	mi->mi_acregmin = ACREGMIN;
	mi->mi_acregmax = ACREGMAX;
	mi->mi_acdirmin = ACDIRMIN;
	mi->mi_acdirmax = ACDIRMAX;
#if	SECURE_NFS
	mi->mi_authflavor =
		(flags & NFSMNT_SECURE) ? AUTH_DES : AUTH_UNIX;
#else	SECURE_NFS
	if (flags & NFSMNT_SECURE) {
		error = EINVAL;
		goto bad;
	}
	mi->mi_authflavor = AUTH_UNIX;
#endif	SECURE_NFS
	mi->mi_netnamelen = nlen;
	if (nlen >= 0) {
#if	MACH
		mi->mi_netname = (char *)kalloc((u_int)nlen);
#else	MACH
		mi->mi_netname = (char *)kmem_alloc((u_int)nlen);
#endif	MACH
		bcopy(netname, mi->mi_netname, (u_int)nlen);
	}

	/*
	 * Make a vfs struct for nfs.  We do this here instead of below
	 * because rtvp needs a vfs before we can do a getattr on it.
	 */
	vfsp->vfs_fsid.val[0] = mi->mi_mntno;
	vfsp->vfs_fsid.val[1] = MOUNT_NFS;
	vfsp->vfs_data = (caddr_t)mi;

	/*
	 * Make the root vnode, use it to get attributes,
	 * then remake it with the attributes.
	 */
	rtvp = makenfsnode(fh, (struct nfsfattr *)0, vfsp);
	if ((rtvp->v_flag & VROOT) != 0) {
		error = EINVAL;
		goto bad;
	}
	rtvp->v_flag |= VROOT;
	error = VOP_GETATTR(rtvp, &va, u.u_cred);
	if (error)
		goto bad;
	VN_RELE(rtvp);
	vattr_to_nattr(&va, &na);
	rtvp = makenfsnode(fh, &na, vfsp);
	rtvp->v_flag |= VROOT;
	mi->mi_rootvp = rtvp;

	/*
	 * Get server's filesystem stats.  Use these to set transfer
	 * sizes, filesystem block size, and read-only.
	 */
	error = VFS_STATFS(vfsp, &sb);
	if (error)
		goto bad;
	mi->mi_tsize = min(NFS_MAXDATA, (u_int)nfstsize());

	/*
	 * Set filesystem block size to maximum data transfer size
	 */
	mi->mi_bsize = NFS_MAXDATA;
	vfsp->vfs_bsize = mi->mi_bsize;

	/*
	 * Need credentials in the rtvp so do_bio can find them.
	 */
	crhold(u.u_cred);
	vtor(rtvp)->r_cred = u.u_cred;

	*rtvpp = rtvp;
	return (0);
bad:
	if (mi) {
#if	MACH
		if (mi->mi_netnamelen >= 0) {
			kfree((caddr_t)mi->mi_netname,
			      (u_int)mi->mi_netnamelen);
		}
		kfree((caddr_t)mi, sizeof (*mi));
#else	MACH
		if (mi->mi_netnamelen >= 0) {
			kmem_free((caddr_t)mi->mi_netname,
				  (u_int)mi->mi_netnamelen);
		}
		kmem_free((caddr_t)mi, sizeof (*mi));
#endif	MACH
	}
	if (rtvp) {
		VN_RELE(rtvp);
	}
	*rtvpp = NULL;
	return (error);
}


/*
 * vfs operations
 */
int
nfs_unmount(vfsp)
	struct vfs *vfsp;
{
	struct mntinfo *mi = (struct mntinfo *)vfsp->vfs_data;

#ifdef NFSDEBUG
        dprint(nfsdebug, 4, "nfs_unmount(%x) mi = %x\n", vfsp, mi);
#endif
#ifdef NeXT
	if (mi->mi_refct != 1 || mi->mi_rootvp->v_count != 1) {
		return (EBUSY);
	}
#endif NeXT
	rflush(vfsp);
	rinval(vfsp);
#ifndef NeXT
	if (mi->mi_refct != 1 || mi->mi_rootvp->v_count != 1) {
		return (EBUSY);
	}
#endif NeXT
	VN_RELE(mi->mi_rootvp);
	rp_rmhash(vtor(mi->mi_rootvp));
	rinactive(vtor(mi->mi_rootvp));
	vfs_putnum(nfs_minmap, mi->mi_mntno);
#if	MACH
	if (mi->mi_netnamelen >= 0) {
		kfree((caddr_t)mi->mi_netname, (u_int)mi->mi_netnamelen);
	}
	kfree((caddr_t)mi, sizeof (*mi));
#else	MACH
	if (mi->mi_netnamelen >= 0) {
		kmem_free((caddr_t)mi->mi_netname, (u_int)mi->mi_netnamelen);
	}
	kmem_free((caddr_t)mi, sizeof (*mi));
#endif	MACH
	return(0);
}

/*
 * find root of nfs
 */
int
nfs_root(vfsp, vpp)
	struct vfs *vfsp;
	struct vnode **vpp;
{

	*vpp = (struct vnode *)((struct mntinfo *)vfsp->vfs_data)->mi_rootvp;
	VN_HOLD((*vpp));
#ifdef NFSDEBUG
        dprint(nfsdebug, 4, "nfs_root(0x%x) = %x\n", vfsp, *vpp);
#endif
	VFS_RECORD(vfsp, VS_ROOT, VS_CALL);
	return (0);
}

/*
 * Get file system statistics.
 */
int
nfs_statfs(vfsp, sbp)
	register struct vfs *vfsp;
	struct statfs *sbp;
{
	struct nfsstatfs fs;
	struct mntinfo *mi;
	fhandle_t *fh;
	int error = 0;

	mi = vftomi(vfsp);
	fh = vtofh(mi->mi_rootvp);
#ifdef NFSDEBUG
        dprint(nfsdebug, 4, "nfs_statfs vfs %x\n", vfsp);
#endif
	error = rfscall(mi, RFS_STATFS, xdr_fhandle,
	    (caddr_t)fh, xdr_statfs, (caddr_t)&fs, u.u_cred);
	if (!error) {
		error = geterrno(fs.fs_status);
	}
	if (!error) {
		if (mi->mi_stsize) {
			mi->mi_stsize = MIN(mi->mi_stsize, fs.fs_tsize);
		} else {
			mi->mi_stsize = fs.fs_tsize;
		}
		sbp->f_bsize = fs.fs_bsize;
		sbp->f_blocks = fs.fs_blocks;
		sbp->f_bfree = fs.fs_bfree;
		sbp->f_bavail = fs.fs_bavail;
		sbp->f_files = -1;
		sbp->f_ffree = -1;
		/*
		 * XXX - This is wrong, should be a real fsid
		 */
		bcopy((caddr_t)&vfsp->vfs_fsid,
		    (caddr_t)&sbp->f_fsid, sizeof (fsid_t));
	}
#ifdef NFSDEBUG
        dprint(nfsdebug, 5, "nfs_statfs returning %d\n", error);
#endif
	return (error);
}

/*
 * Flush dirty nfs files for file system vfsp.
 * If vfsp == NULL, all nfs files are flushed.
 */
int
nfs_sync(vfsp)
	struct vfs *vfsp;
{
	static int nfslock;

	if (nfslock == 0) {
#ifdef NFSDEBUG
		dprint(nfsdebug, 5, "nfs_sync\n");
#endif
		nfslock++;
		rflush(vfsp);
		nfslock = 0;
	}
	return (0);
}

int
nfs_badop()
{

	panic("nfs_badop");
}

char *
itoa(n, str)
	u_short n;
	char *str;
{
	char prbuf[11];
	register char *cp;

	cp = prbuf;
	do {
		*cp++ = "0123456789"[n%10];
		n /= 10;
	} while (n);
	do {
		*str++ = *--cp;
	} while (cp > prbuf);
	return (str);
}

hostpath(name, shostname, path)
	char *name;
	char *shostname;
	char *path;
{
	register char *nm;

	(void) strcpy(name, shostname);
	for (nm = name; *nm; nm++)
		;
	*nm++ = ':';
	(void) strcpy(nm, path);
}

/*
 * Convert a INET address into a string for printing
 */
addr_to_str(addr, str)
	struct sockaddr_in *addr;
	char *str;
{
	str = itoa((u_short)((addr->sin_addr.s_addr >> 24) & 0xff), str);
	*str++ = '.';
	str = itoa((u_short)((addr->sin_addr.s_addr >> 16) & 0xff), str);
	*str++ = '.';
	str = itoa((u_short)((addr->sin_addr.s_addr >> 8) & 0xff), str);
	*str++ = '.';
	str = itoa((u_short)(addr->sin_addr.s_addr & 0xff), str);
	*str = '\0';
}

