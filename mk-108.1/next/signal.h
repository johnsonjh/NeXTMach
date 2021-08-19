/*
 *	Copyright (C) 1989, NeXT, Inc.
 *
 *	Machine specific signal information.
 *
 * HISTORY
 * 21-May-89  Avadis Tevanian, Jr. (avie) at NeXT, Inc.
 *	Created.
 */

#ifndef	_MACHINE_SIGNAL_
#define	_MACHINE_SIGNAL_ 1

#ifndef	ASSEMBLER
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to properly restore state if
 * a non-standard exit is performed.
 */
struct	sigcontext {
	int	sc_onstack;		/* sigstack state to restore */
	int	sc_mask;		/* signal mask to restore */
	int	sc_sp;			/* sp to restore */
	int	sc_pc;			/* pc to restore */
	int	sc_ps;			/* psl to restore */
	int	sc_d0;			/* d0 to restore */
};
#endif	ASSEMBLER
#endif	_MACHINE_SIGNAL_

