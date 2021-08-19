/*
 *	File:	next/reboot.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	NeXT specific reboot flags.
 *
 * HISTORY
 * 28-Feb-90  John Seamons (jks) at NeXT
 *	Added RB_COMMAND flag that allows a specific reboot command to be used.
 *
 * 06-Jul-88  Avadis Tevanian (avie) at NeXT, Inc.
 *	Created.
 */

/*
 *	Use most significant 16 bits to avoid collisions with
 *	machine independent flags.
 */
#define RB_POWERDOWN	0x00010000	/* power down on halt */
#define	RB_NOBOOTRC	0x00020000	/* don't run '/etc/rc.boot' */
#define	RB_DEBUG	0x00040000	/* drop into mini monitor on panic */
#define	RB_EJECT	0x00080000	/* eject disks on halt */
#define	RB_COMMAND	0x00100000	/* new boot command specified */

