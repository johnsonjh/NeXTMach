/*
 * Simple loadable kernel server example.
 *
 * This server accepts two messages:
 *	simple_puts() prints the (inline string) argument on the console.
 *	simple_vers() returns the running kernel's version string.
 */
#import <kernserv/kern_server_types.h>

/*
 * Allocate an instance variable to be used by the kernel server interface
 * routines for initializing and accessing this service.
 */
kern_server_t instance;

/*
 * Stamp our arival.
 */
void mythread_init(void)
{
	printf("Thread kernel server initialized\n");
}

void mythread(void)
{
	printf("New kernel thread\n");
	(void)thread_terminate((thread_t)thread_self());
	thread_halt_self();
}

void mythread_fork(void)
{
	kernel_thread(current_task(), mythread);
}

/*
 * Notify the world that we're going away.
 */
void mythread_signoff(void)
{
	printf("Thread kernel server unloaded\n");
}


