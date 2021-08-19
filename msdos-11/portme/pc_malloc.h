/*
 * MSDOS malloc constants
 *
 * DEBUG malloc frame
 *
 * 	0 	size actually kalloc'd
 * 	4	magic number
 * 	0x10	start of caller data - 16-byte alligned for DMA purposes
 */
#define MALLOC_OFFSET	0x10		/* byte offset of user data */
#define SIZE_OFFSET	0		/* int index of size field */
#define MAGIC_OFFSET	1		/* int index of magic number */
#define MALLOC_MAGIC	0x11225566
