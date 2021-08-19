/* 
 **********************************************************************
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 **********************************************************************
 * HISTORY
 **********************************************************************
 */ 

/*
 * pcbheadsw.h
 */


struct pcbheadsw {
    short protocol;
    struct inpcb *head;
};

