/*	@(#)voldev.h	2.0	03/19/90	(c) 1990 NeXT	*/

/* 
 * HISTORY
 * 19-Sep-90	Doug Mitchell
 *	Added prototype for vol_notify_cancel.
 * 03-20-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import <sys/types.h>
#import <sys/kern_return.h>
#import <kern/kern_port.h>
#import <nextdev/insertmsg.h>

#ifndef	_VOLDEV_
#define _VOLDEV_

#ifndef	NULL
#define NULL 0
#endif  NULL

/*
 * Struct used for queueing notifications prior to first DKIOCMNOTIFY
 */
struct vol_notify_entry {
	queue_chain_t		link;
	int 			dev_desc;	/* IND_DD_DEV, IND_DD_PORT */
	dev_t 			block_dev;	/* for IND_DD_DEV */
	dev_t 			raw_dev;	/* ditto */
	kern_port_t 		dev_port;	/* for IND_DD_PORT */
   	int 			vol_state;	/* IND_VS_LABEL, etc. */
	char			form_type[FORM_TYPE_LENGTH];
	char 			dev_str[OID_DEVSTR_LEN];
	int 			flags;
};

typedef struct vol_notify_entry *vne_t;

/*
 * Struct for maintaining state of panel requests
 */
typedef	void (*vpt_func)(void *param, int tag, int response_value);

struct vol_panel_entry {
	queue_chain_t		link;
	int			tag;
	vpt_func		fnc;		/* to be called upon receipt of
						 * vol_panel_resp message */ 	
	void			*param;
};

typedef struct vol_panel_entry *vpe_t;

/*
 * Public Functions
 */
kern_return_t vol_notify_dev(dev_t block_dev, 
	dev_t raw_dev,
	char *form_type,
   	int vol_state,				/* IND_VS_LABEL, etc. */
	char *dev_str,
	int flags);
kern_return_t vol_notify_port(kern_port_t dev_port, 
	char *form_type,
   	int vol_state,				/* IND_VS_LABEL, etc. */
	int flags);
void vol_notify_cancel(dev_t device);
kern_return_t vol_panel_request(vpt_func fnc,
	int panel_type,				/* PR_PT_DISK_NUM, etc. */
	int response_type,			/* PR_RT_ACK, atc. */
	int p1,
	int p2,
	int p3,
	int p4,
	char *string1,
	char *string2,
	void *param,
	int *tag);				/* RETURNED */
kern_return_t vol_panel_disk_num(vpt_func fnc,
	int volume_num,
	int drive_type,				/* PR_DRIVE_FLOPPY, etc. */
	int drive_num,
	void *param,
	boolean_t wrong_disk,
	int *tag);				/* RETURNED */
kern_return_t vol_panel_disk_label(vpt_func fnc,
	char *label,
	int drive_type,				/* PR_DRIVE_FLOPPY, etc. */
	int drive_num,
	void *param,
	boolean_t wrong_disk,
	int *tag);				/* RETURNED */
kern_return_t vol_panel_remove(int tag);

extern kern_port_t panel_req_port;		/* PORT_NULL if no port to 
						 * which to send panel requests
						 * has been registered */

#endif	_VOLDEV_


