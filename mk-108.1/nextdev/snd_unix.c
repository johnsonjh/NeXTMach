/* 
 * Copyright (c) 1988 NeXT, Inc.
 */
/*
 * HISTORY
 * 14-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Get rid of snd_task_name in favor of new task_name in bsd/init_main.c
 *
 * 10-Dec-88  Gregg Kellogg (gk) at NeXT
 *	Created.
 *
 */ 

#import <sound.h>
#if	NSOUND > 0

/*
 * Routines to make unix happy.
 * ioctl	- to get the device port returned.
 */

#import <sys/param.h>
#import <sys/ioctl.h>
#import <sys/errno.h>
#import <kern/kern_port.h>
#import <nextdev/snd_var.h>
#import <sys/user.h>

extern snd_var_t snd_var;

snd_unix_ioctl(dev, cmd, datap, flag)
	dev_t	dev;
	int	cmd, flag;
	int	*datap;
{
	msg_header_t msg;
	port_t lport;
	task_t newtask;

	if (cmd != SOUNDIOCDEVPORT)
		return(EINVAL);

	if (!snd_var.task_port) {
		thread_t snd_th;
		
		snd_var.task_port = PORT_NULL;
		(void) task_create(kernel_task, FALSE, &newtask);
		(void) thread_create(newtask, &snd_th);
		thread_start(snd_th, snd_server_loop, THREAD_SYSTEMMODE);
		(void) thread_resume(snd_th);
		if (snd_var.task_port == PORT_NULL) {
			assert_wait((int)&snd_var.task_port, FALSE);
			thread_block();
		}
	}
	object_copyin(current_task(), *datap, MSG_TYPE_PORT, FALSE, datap);
	object_copyin(snd_var.task, snd_var.dev_port, MSG_TYPE_PORT, FALSE,
			&lport);

	msg.msg_simple = TRUE;
	msg.msg_size = sizeof(msg);
	msg.msg_type = MSG_TYPE_NORMAL;
	msg.msg_local_port = (port_t)lport;
	msg.msg_remote_port = *(port_t *)datap;
	msg.msg_id = SND_MSG_RET_DEVICE;

	msg_send_from_kernel(&msg, MSG_OPTION_NONE, 0);

	port_deallocate(current_task(), *(port_t *)datap);
	port_deallocate(snd_var.task, lport);

	return(0);
}
#endif	NSOUND > 0


