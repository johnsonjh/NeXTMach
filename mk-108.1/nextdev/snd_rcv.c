/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

/*
 * Interface routines for dealing with recieved messages.
 */
#import <sound.h>
#if	NSOUND > 0

#import <nextdev/snd_var.h>

union snd_rcv_msg {
	msg_header_t	header;
	char		foo[MSG_SIZE_MAX];
} snd_rcv_msg;

/*
 * "Allocate" a message frame for recieving a message from the user.
 */
msg_header_t *snd_rcv_alloc_msg_frame()
{
	((msg_header_t *)&snd_rcv_msg)->msg_size = sizeof(snd_rcv_msg);
	return (msg_header_t *)&snd_rcv_msg;
}
#endif	NSOUND > 0

