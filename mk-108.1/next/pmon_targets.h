/*
 * pmon_targets.h - Kernel pmon event_types
 */

#ifndef	KPMON_TYPES
#define KPMON_TYPES 1


#define KPMON_DEVICE	"kpmon"

#define KP_NONE		0		/* no event */

/*
 * PMON_SOURCE_KERN
 *
 * data1 = reserved
 * data2 = reserved
 * data3 = reserved
 */
/* event_type bits TBD */

/* 
 * PMON_SOURCE_SCSI, PMON_SOURCE_FD
 * 	data1 = file system block #
 * 	data2 = byte count
 * 	data3 = unit number
 */
#define KP_SCSI_WRITEQ		0x0001		/* queue write request */
#define KP_SCSI_WRITEDQ		0x0002		/* dequeue write request */
#define KP_SCSI_WRITE 		0x0004		/* start write */
#define KP_SCSI_WRITE_CMP 	0x0008		/* write complete */
#define KP_SCSI_READQ		0x0010		/* queue read request */
#define KP_SCSI_READDQ		0x0020		/* dequeue read request */
#define KP_SCSI_READ 		0x0040		/* start read */
#define KP_SCSI_READ_CMP 	0x0080		/* read complete */
/*
 *	for floppy only (PMON_SOURCE_FD)
 */
#define KP_SCSI_WAITIOC		0x0100		/* wait for controller I/O
						 * complete */
/*
 * SCSI controller events
 *	data1 = target #
 *	data2 = cdb opcode
 *	data3 = cdb lbn (assumed to be 10 byte cdb)
 */
#define	KP_SCSI_CTLR_START	0x0200		/* controller start */
#define	KP_SCSI_CTLR_DISC	0x0400		/* controller disconnect */
#define	KP_SCSI_CTLR_RSLCT	0x0800		/* controller reselect */
#define	KP_SCSI_CTLR_CMPL	0x1000		/* controller complete */
/*
 * SCSI controller interrupt
 *	data1 = target #
 *	data2 = old state
 *	data3 = new state
 */
#define	KP_SCSI_CTLR_INTR	0x2000		/* controller interrupt */
 
#define KP_SCSI_ALL		0x2FFF


/*
 * PMON_SOURCE_VM
 * 	data1 = UNIX pid of task causing activity (1 if pageout task)
 * 	data2 = virtual address or page
 * 	data3 = pager (0 if device, vnode otherwise)
 */
#define KP_VM_FAULT		0x1		/* page fault */
#define KP_VM_REACTIVE		0x2		/* inactive page reactivation */
#define KP_VM_FREE_REACTIVE	0x4		/* free page reactivation */
#define KP_VM_DEACTIVE		0x8		/* page deactivation */
#define KP_VM_PAGEOUT		0x10		/* page was written to a pager */
#define KP_VM_PAGEFREE		0x20		/* page was freed (no write) */
#define KP_VM_PAGEIN_START	0x40		/* pagein began on a page */
#define KP_VM_PAGEIN_DONE	0x80		/* pagein completed on a page */
#define KP_VM_SWAP_PAGEIN	0x100		/* pagein from swap file */
#define	KP_VM_VNODE_PAGEIN	0x200		/* pagein from a normal file */

#define KP_VM_ALL		0x3FF


/*
 * PMON_SOURCE_SCHED
 * 	data1 = UNIX pid
 * 	data2 = task
 *	date3 = thread
 */
#define KP_SCHED_THREAD_BLOCK	0x1	/* current thread blocked */
#define KP_SCHED_THREAD_PREEMPT	0x2	/* current thread was preempted */
#define KP_SCHED_THREAD_TERM	0x4	/* current thread died */
#define KP_SCHED_THREAD_YIELD	0x8	/* current thread yielded somehow */
#define KP_SCHED_THREAD_WAIT	0x10	/* thread is waiting for something */
#define KP_SCHED_THREAD_GO	0x20	/* new thread running (CONTEXT SWITCH) */
#define KP_SCHED_IDLE_GO	0x40	/* the idle thread */
#define KP_SCHED_IDLE_END	0x80	/* the idle thread found some work */

#define KP_SCHED_ALL		0x1FF


/*
 * PMON_SOURCE_IPC
 * 	data1 = source task
 * 	data2 = dest task
 *	date3 = global port name
 */
#define KP_IPC_MSG_SEND_1	0x1	/* just entered msg_send_trap() */
#define KP_IPC_MSG_SEND_2	0x2	/* in msg_send_trap() after the copyin */
#define KP_IPC_MSG_SEND_3	0x4	/* in msg_send_trap() just before exit */

#define KP_IPC_MSG_RECEIVE_1	0x30	/* just entered msg_receive_trap() */
#define KP_IPC_MSG_RECEIVE_2	0x31	/* just before msg_dequeue (and sleep) */
#define KP_IPC_MSG_RECEIVE_3	0x32	/* just after msg_dequeue (awoke) */
#define KP_IPC_MSG_RECEIVE_4	0x34	/* just before exit */

#define KP_IPC_MSG_RPC		0x40	/* a task initiated RPC */
#define KP_IPC_MSG_REPLY	0x80	/* the reply half of an RPC */

#define KP_IPC_SWITCH_GO	0x100	/* CONTEXT SWITCH on message send */

#define KP_IPC_ALL		0x1F


/*
 * PMON_SOURCE_VFS
 *	data1 = task
 *	data2,3 = 0
 */
#define KP_VFS_STAT_BEGIN_LOOKUP	0x1	/* start pn lookup in stat */
#define KP_VFS_STAT_END_LOOKUP		0x2	/* end ditto */
#define KP_VFS_STAT_BEGIN_STAT		0x4	/* start vfs specific stat */
#define KP_VFS_STAT_END_STAT		0x8	/* end vfs specific stat  */

#define KP_VFS_ALL			0xF

/*
 * PMON_SOURCE_EV
 *	data1 = task
 *	data2,3 = 0
 */
#define KP_EV_POST_EVENT		0x1	/* post an event */
#define KP_EV_QUEUE_FULL		0x2	/* queue full */
/*	data1 = mouse (0) or keyboard (1)
 *	data2 = hw_event data
 *	data3 = 0
 */
#define KP_EV_PREQUEUE_FULL		0x4	/* pre-queue full */

#define KP_EV_ALL			0x7

/*
 * PMON_SOURCE_IP
 *	data1 = source addr
 *	data2 = dest addr
 *	data3 = 0
 */
#define KP_IP_BROADCAST			0x1	/* we got a broadcast packet */

#define KP_IP_ALL			0x1
#define KPMON
			
#endif	KPMON_TYPES




