/* 
 * Copyright (c) 1989 NeXT, Inc.
 *
 * HISTORY
 * 17-Feb-88  John Seamons (jks) at NeXT
 *	Created (from VAX version).
 */

#import <sys/boolean.h>
#import <sys/exception.h>
#import <sys/kern_return.h>
#import <sys/signal.h>

/*
 *	machine_exception translates a mach exception to a unix exception
 *	and code.  This handles all the hardware-specific exceptions for
 *	NeXT.  unix_exception() handles the machine-independent ones.
 *
 *	FIXME: create real codes & subcodes in next/exception.h
 */

boolean_t machine_exception(exception, code, subcode, unix_signal, unix_code)
int	exception, code, subcode;
int	*unix_signal, *unix_code;
{
	switch(exception) {

	    case EXC_BAD_INSTRUCTION:
	        *unix_signal = SIGILL;
		*unix_code = code;
		break;

	    case EXC_ARITHMETIC:
	        *unix_signal = SIGFPE;
		*unix_code = code;
		break;

	    case EXC_EMULATION:
	        *unix_signal = SIGEMT;
		*unix_code = code;
		break;

	    default:
		return(FALSE);
	}
	return(TRUE);
}
