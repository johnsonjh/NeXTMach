/*
 * Copyright (c) 1987 NeXT, INC
 */

#import <next/cframe.h>

PROCENTRY (curipl)
	movw	sr,d0
	lsrl	#8,d0
	andl	#7,d0
	PROCEXIT

