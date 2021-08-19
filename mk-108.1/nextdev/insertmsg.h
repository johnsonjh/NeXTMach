/*	@(#)insertmsg.h	2.0	02/14/90	(c) 1990 NeXT	
 *
 * insertmsg.h -- Structs used for "Disk Inserted" messages. 
 *
 * HISTORY
 * 01-Oct-90	Doug Mitchell
 *	Added in_flags, IND_FLAGS_REMOVABLE
 * 23-Apr-90	Doug Mitchell
 *	Added oid_dev_str to of_insert_notify_dev
 * 14-Feb-90	Doug Mitchell at NeXT
 *	Created.
 *
 */

#ifndef	_INSERTMSG_
#define _INSERTMSG_

#import <sys/types.h>
#import <sys/message.h>

/*
 * msg_id's for messages used by vol driver.
 */
#define VOL_MSG_ID		0x355
#define VOL_CHECK_MSG_ID	(VOL_MSG_ID+0)	/* new disk insert */
#define VOL_PANEL_REQ		(VOL_MSG_ID+1)	/* panel request */
#define VOL_PANEL_RESP		(VOL_MSG_ID+2)	/* panel response */
#define VOL_PANEL_CANCEL	(VOL_MSG_ID+3)	/* panel cancel */

#define FORM_TYPE_LENGTH	64

/*
 * New disk insertion notification (msg_id == VOL_CHECK_MSG_ID).
 */
struct insert_notify {
	msg_header_t		in_header;
	msg_type_t		in_vd_type;
	int			in_vol_state;	/* IND_VS_xxx */
	int			in_dev_desc;	/* IND_DD_xxx */
	msg_type_t		in_flags_type;
	int			in_flags;	/* IND_FLAGS_xxx */
	msg_type_t		in_ft_type;
	/*
	 * in_form_type contains the legal densities with with the 
	 * given media can be formatted, sprintf'd as decimal numbers
	 * of KBytes and separated by spaces. String is empty if disk
	 * if formatted.
	 */
	char			in_form_type[FORM_TYPE_LENGTH];
	/*
	 * dev_t or port follows...
	 */
};

/*
 * This struct will be used by fd, od, and SCSI drivers.
 */
#define OID_DEVSTR_LEN		6

struct of_insert_notify_dev {
	struct insert_notify	oid_header;
	msg_type_t		oid_dev_type;
	dev_t			oid_bdev_t;	/* block device */
	dev_t			oid_cdev_t;	/* raw device */
	msg_type_t		oid_ds_type;
	char			oid_dev_str[OID_DEVSTR_LEN];
						/* e.g., "fd0", "fd1" */
};

/*
 * For loadable drivers.
 */
struct of_insert_notify_port {
	struct insert_notify	oip_header;
	msg_type_t		oip_p_type;
	port_t			oip_port;	/* describes other devices */
};

/*
 * in_vol_state values
 */
#define IND_VS_LABEL		0x00		/* volume contains valid Mach
						 * label */
#define IND_VS_FORMATTED	0x01		/* volume is formatted but
						 * contains no label */
#define IND_VS_UNFORMATTED	0x02		/* unformatted */

/*
 * in_dev_desc values
 */
#define IND_DD_DEV		0x00		/* oin_dev_t contains a dev_t
						 * for a device in /dev */
#define IND_DD_PORT		0x01		/* oin_port contains a port
						 * for a loadable server */

/*
 * in_flags values
 */
#define IND_FLAGS_REMOVABLE	0x00000001	/* 1 = removable; 0 = fixed */
#define IND_FLAGS_FIXED		0x00000000
#define IND_FLAGS_WP		0x00000002	/* 1 = write protected */

/*
 * Panel request (msg_id == VOL_PANEL_REQ)
 */
#define VP_STRING_LEN		40

struct vol_panel_req {
	msg_header_t		pr_header;
	msg_type_t		pr_int_desc;	/* describes following 7
						 * fields */
	int			pr_panel_type;	/* PR_PT_xxx */
	int			pr_resp_type;	/* PR_RT_xxx */
	int 			pr_tag;		/* identifies this panel */
	/*
	 * meanings of these parameters vary per pr_panel_type.
	 */
	int			pr_p1;
	int			pr_p2;
	int 			pr_p3;
	int			pr_p4;
	msg_type_t		pr_string_desc;	/* describes following 2
						 * fields */
	char			pr_string1[VP_STRING_LEN];
	char			pr_string2[VP_STRING_LEN];
};

/*
 * pr_panel_type values
 */
#define PR_PT_DISK_NUM		0	/* insert disk <p1> in 
					 * <p2>(scsi|floppy|optical) drive <p3>
					 */
#define PR_PT_DISK_LABEL	1	/* insert disk <string1> in 
					 * <p2>(scsi|floppy|optical) drive <p3>
					 */
#define PR_PT_DISK_NUM_W	2	/* wrong disk - insert disk <p1> in 
					 * <p2>(scsi|floppy|optical) drive <p3>
					 */
#define PR_PT_DISK_LABEL_W	3	/* wrong disk - insert disk <string1>
					 * in <p2>(scsi|floppy|optical) drive 
					 * <p3>
					 */
#define PR_PT_SWAPDEV_FULL	4	/* swap device full */
#define PR_PT_FILESYS_FULL	5	/* file system <string1> full */
#define PR_RT_EJECT_REQ		6	/* eject disk in <p2> drive <p3> */

/*
 * p2 values for PR_PT_DISK_NUM / PR_PT_DISK_LABEL
 */
#define PR_DRIVE_FLOPPY		0	/* floppy disk */
#define PR_DRIVE_OPTICAL	1	/* OMD-1 (5.25") optical */
#define PR_DRIVE_SCSI		2	/* removable SCSI disk */

/*
 * pr_response_type values. Describes both format of panel and expected 
 * response by Workspace Manager.
 */
#define PR_RT_NONE		0	/* no acknowledgement expected, no
					 * cancel necesary */
#define PR_RT_CANCEL		1	/* no ack; leave panel up until 
					 * vol_panel_cancel message */
#define PR_RT_ACK		2	/* just "OK" ack */
#define PR_RT_INT		3	/* integer value expected */

/*
 * Examples: 
 *   'Insert floppy disk 3 in drive 0'
 *	pr_panel_type 	 = PR_PT_DISK_NUM
 *	p1 		 = 3
 *	p2 		 = PR_DRIVE_FLOPPY
 *	p3 		 = 0
 *	pr_response_type = PR_RT_ACK (acknowledgement means "disk not 
 *		           avaliable")
 *
 *   'Insert Optical disk "MyDisk" in drive 1'
 *	pr_panel_type 	 = PR_PT_DISK_LABEL
 *	string1		 = "MyDisk"
 *	p2 		 = PR_DRIVE_OPTICAL
 *	p3 		 = 1
 *	pr_response_type = PR_RT_ACK (acknowledgement means "disk not 
 *		           avaliable")
 *
 *   'Swap Device Full'
 *	pr_panel_type 	 = PR_PT_SWAPDEV_FULL
 *	pr_response_type = PR_RT_ACK (acknowledgement means "OK to send more
 *			   of these messages") 
 */

/*
 * Panel response (msg_id == VOL_PANEL_RESP). Sent by Workspace Manager to 
 * vol driver when use responds to panels with pr_response_type of PR_RT_ACK
 * and PR_RT_INT.
 */
struct vol_panel_resp {
	msg_header_t		ps_header;
	msg_type_t		ps_int_desc;	/* describes following 2
						 * fields */
	int			ps_tag;		/* identifies panel request */
	int			ps_value;	/* n/u for PR_RT_ACK requests;
						 * integer for PR_RT_INT */
};

/*
 * Panel cancel request (msg_id == VOL_PANEL_CANCEL). Sent by vol driver to
 * remove existing panel.
 */
struct vol_panel_cancel {
	msg_header_t		pc_header;
	msg_type_t		pc_int_desc;	/* describes following field */
	int			pc_tag;		/* identifies panel request */
};

#endif	_INSERTMSG_


