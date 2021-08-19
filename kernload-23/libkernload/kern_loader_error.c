/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 07-Jun-89  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <stdio.h>
#import <mach_error.h>
#import <kernserv/kern_loader_error.h>
#import <kernserv/kern_loader_types.h>

void kern_loader_error(const char *s, kern_return_t r)
{
	if (r < KERN_LOADER_NO_PERMISSION || r > KERN_LOADER_SERVER_WONT_LOAD)
		mach_error(s, r);
	else
		fprintf(stderr, "%s : %s (%d)\n", s,
			kern_loader_error_string(r), r);
}

static const char *kern_loader_error_list[] = {
	"permission required",
	"unknown server",
	"server loaded",
	"server unloaded",
	"need a server name",
	"server already exists",
	"port already advertised",
	"server won't relocate",
	"server won't load",
	"can't open server relocatable",
	"relocatable malformed",
	"can't allocate server memory",
	"server deleted during operation",
	"server won't unload properly",
};

const char *kern_loader_error_string(kern_return_t r)
{
	if (r < KERN_LOADER_NO_PERMISSION || r > KERN_LOADER_SERVER_WONT_LOAD)
		return mach_error_string(r);
	else
		return kern_loader_error_list[r - KERN_LOADER_NO_PERMISSION];
}
