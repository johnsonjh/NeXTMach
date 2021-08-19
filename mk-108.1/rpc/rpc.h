/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *	@(#)rpc.h 1.9 88/02/08 SMI	  
 */


/*
 * rpc.h, Just includes the billions of rpc header files necessary to 
 * do remote procedure calling.
 */
#ifndef __RPC_HEADER__
#define __RPC_HEADER__

#ifdef KERNEL
#import <../rpc/types.h>	/* some typedefs */
#import <../netinet/in.h>

/* external data representation interfaces */
#import <../rpc/xdr.h>		/* generic (de)serializer */

/* Client side only authentication */
#import <../rpc/auth.h>	/* generic authenticator (client side) */

/* Client side (mostly) remote procedure call */
#import <../rpc/clnt.h>	/* generic rpc stuff */

/* semi-private protocol headers */
#import <../rpc/rpc_msg.h>	/* protocol for rpc messages */
#import <../rpc/auth_unix.h>	/* protocol for unix style cred */
#import <../rpc/auth_des.h>	/* protocol for des style cred */

/* Server side only remote procedure callee */
#import <../rpc/svc.h>		/* service manager and multiplexer */
#import <../rpc/svc_auth.h>	/* service side authenticator */

#else

#import <rpc/types.h>		/* some typedefs */
#import <netinet/in.h>

/* external data representation interfaces */
#import <rpc/xdr.h>		/* generic (de)serializer */

/* Client side only authentication */
#import <rpc/auth.h>		/* generic authenticator (client side) */

/* Client side (mostly) remote procedure call */
#import <rpc/clnt.h>		/* generic rpc stuff */

/* semi-private protocol headers */
#import <rpc/rpc_msg.h>	/* protocol for rpc messages */
#import <rpc/auth_unix.h>	/* protocol for unix style cred */
#import <rpc/auth_des.h>	/* protocol for des style cred */

/* Server side only remote procedure callee */
#import <rpc/svc.h>		/* service manager and multiplexer */
#import <rpc/svc_auth.h>	/* service side authenticator */
#endif KERNEL

#endif /* ndef __RPC_HEADER__ */
