/* 
 * Copyright (c) 1990 NeXT, Inc.
 *
 * HISTORY
 * 13-Apr-90  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#ifndef	_KERNLOAD_OBJ_
#define	_KERNLOAD_OBJ_
#import "server.h"

/*
 * Allocate space for and map in the Mach-O file on a page-aligned boundary.
 */
boolean_t obj_map_reloc (server_t *server);

/*
 * Link the .o file against the executable.
 */
boolean_t obj_link(server_t *server);

/*
 * Return the address of the symbol in filename with the
 * given name (or NULL).
 */
vm_address_t obj_symbol_value(const char *filename, const char *name);

/*
 * Find the modification date of the relocatable.
 */
boolean_t obj_date(const char *reloc, u_int *date);
#endif	_KERNLOAD_OBJ_

