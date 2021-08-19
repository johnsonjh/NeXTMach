/* 
 * Copyright (c) 1987 NeXT, Inc.
 */ 

#ifndef	_VM_TYPES_MACHINE_
#define	_VM_TYPES_MACHINE_	1

#ifdef	ASSEMBLER
#else	ASSEMBLER
typedef	unsigned int	vm_offset_t;
typedef	unsigned int	vm_size_t;
#endif	ASSEMBLER

#endif	_VM_TYPES_MACHINE_
