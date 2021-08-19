/* 
 * Copyright (c) 1990 NeXT, Inc.
 */
/*
 * HISTORY
 * 22-Jan-90  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

void *malloc(unsigned int size);
void *calloc(unsigned int num, unsigned int size);
void *realloc(void *addr, unsigned int size);
void free(void *data);
void malloc_good_size(unsigned int size);

