
/*
 *	author: brian pinkerton
 */
#define KERNEL_FEATURES

#import <nlist.h>
#import <stdio.h>
#import	<sys/boolean.h>
#import <sys/time.h>
#import <sys/types.h>
#import <sys/file.h>
#import <sys/stat.h>
#import <sys/table.h>
#import <sys/vnode.h>
#import <vm/vm_map.h>
#import <vm/vm_object.h>
#import <vm/vm_page.h>
#import <ufs/inode.h>

#define	TRUE	1
#define FALSE	0

#if		NEXT_1_0
#import <sys/task.h>
#import <sys/mfs.h>
#else	NEXT_1_0
#import <kern/task.h>
#import <kern/mfs.h>
#endif	NEXT_1_0

/*
 *  Prototypes
 */
void printPages(int *pageArray, int pageCount);
int compareInt(int *i1, int *i2);
int kernelGet(int kmem, int addr, void *buf, int size);

/*
 *  Stolen from the kernel...  (ufs_inode.c)
 */
#define	INOHSZ	512
#if	((INOHSZ&(INOHSZ-1)) == 0)
#define	INOHASH(dev,ino)	(((dev)+(ino))&(INOHSZ-1))
#else
#define	INOHASH(dev,ino)	(((unsigned)((dev)+(ino)))%INOHSZ)
#endif

union ihead {
	union  ihead *ih_head[2];
	struct inode *ih_chain[2];
};

char *ihead;

#define	VM_OBJECT_HASH_COUNT	521
/*
 *	vm_object_hash hashes the pager/id pair.
 */

#define vm_object_hash(pager) \
	(((unsigned)pager)%VM_OBJECT_HASH_COUNT)

queue_head_t vm_object_hashtable[VM_OBJECT_HASH_COUNT];

int vmo_hashAddr;
int vm_object_list;

int debug;


#define NAMETBLSIZE 5

char	*names[NAMETBLSIZE] = {
	"_ihead",
#define X_IHEAD	0
	"_vm_object_hashtable",
#define X_VMHASH 1
	"_vm_object_list",
#define X_VM_OBJECT_LIST 2
	""
};

main(argc, argv)
	int argc;
	char *argv[];
{
	register int i=0;
	int kmem;
	struct nlist	nl[NAMETBLSIZE];
	char **fileList;
	int fileNum;
	
	kmem = open("/dev/kmem", 0);  /* grab kernel memory */
	if (kmem < 0) {
		perror("/dev/kmem");
		exit(1);
	}
	
	initnlist(nl, names);	/* lookup kernel variables */
	nlist("/mach", nl);
	/* if (getkvars(kmem, nl) != 0)
		exit(1); */
	ihead = (char *) nl[X_IHEAD].n_value;
	vmo_hashAddr = (int) nl[X_VMHASH].n_value;
	vm_object_list = (int) nl[X_VM_OBJECT_LIST].n_value;

	/*
	 *  Run through the arguments once to find any options,
	 *  and also accumulate a list of files to do.
	 */
	fileList = (char **) malloc((argc - 1) * sizeof(char *));
	fileNum = 0;
	i = 1;
	while (i < argc) {
		if (*argv[i] == '-') {
			switch (argv[i][1]) {
				case 'd':
					debug++;
					break;
					
				default:
					usage(argv[0]);
			}
		}
		else
			fileList[fileNum++] = argv[i];
			
		++i;
	}

	if (fileNum <= 0)
		usage(argv[0]);
	
	for (i=0; i < fileNum; i++) {
		doInode(kmem, fileList[i]);
	}
}


initnlist(nl, names)
	struct nlist	nl[];
	char		*names[];
{
	int	i;

	for (i = 0; names[i] != 0 && strcmp(names[i], "") != 0; i++)
		nl[i].n_un.n_name = names[i];
	nl[i].n_un.n_name = 0;
}


getkvars(kmem, nl)
	struct nlist	nl[];
{
	
	return(0);
}

usage(s)
	char *s;
{
	printf("usage: %s [-d] file names\n", s);
	exit(1);
}


/*
 *	vm_object_lookup looks in the object list for an object with the
 *	specified pager.
 */
int
my_vm_object_lookup(kmem, pager, returnObject)
	int			kmem;
	unsigned int		pager;
	struct vm_object	*returnObject;
{
	struct vm_object	object;
	int			objectAddr;
	queue_head_t		head;

	if (kernelGet(kmem, vm_object_list, (char *)&head, sizeof(queue_head_t))) {
		printf("Couldn't read vm_object_list queue header.\n");
		exit(1);
	}
	
	objectAddr = (int) head.next;
	while (objectAddr != vm_object_list) {
		
		if (kernelGet(kmem, objectAddr, (char *)&object, sizeof(object))) {
			printf("Couldn't read object out of object list.\n");
			exit(1);
		}
		
		if (object.pager == pager) {
			*returnObject = object;
			return objectAddr;
		}
		
		objectAddr = (int) object.object_list.next;
	}
		
	return 0;
}


doInode(kmem, fileName)
	int kmem;
	char *fileName;
{
	int hashValue;
	union ihead chainHead;
	struct inode ip;
	int headOffset, nextInode;
	struct vm_info vmInfo;
	int pager;
	int vmInfoPtr, pagerPtr;
	struct vm_object object;
	int objectPtr;
	int inodeNumber;
	dev_t inodeDevice;
	struct stat statBuf;
	int error, pageCount;
	struct vm_page page;
	int pagePtr;
	int pageSize;
	int *pageArray;
	
	/*
	 *  Find out the device and inode numbers
	 */
	error = stat(fileName, &statBuf);
	if (error < 0) {
		perror(fileName);
		exit(1);
	}
	inodeNumber = statBuf.st_ino;
	inodeDevice = statBuf.st_dev;
	
	/*
	 *  Compute the offset into the hash chain, the get the chain head
	 *  at that offset.
	 */
	hashValue = INOHASH(inodeDevice, inodeNumber);
	headOffset = (int) ihead + hashValue * sizeof(union ihead);
	if (kernelGet(kmem, headOffset, &chainHead, sizeof(union ihead))) {
		printf("doInode: can't read chainHead value at 0x%x\n",
			headOffset);
		return(1);
	}
	
	/*
	 *  Traverse the chain of inodes at this hash offset to find the one we
	 *  want.
	 */
	nextInode = (int) chainHead.ih_chain[0];
	while ( (ip.i_number != inodeNumber || ip.i_dev != inodeDevice) &&
		nextInode != headOffset) {
		
		if (kernelGet(kmem, nextInode, &ip, sizeof(struct inode))) {
			printf("doInode: can't read next inode at 0x%x\n",
				nextInode);
			return(1);
		}
		
		if (debug)
			printf("read inode number %d.\n", ip.i_number);
		
		nextInode = (int) ip.i_forw;
	}
	
	if (ip.i_number != inodeNumber || ip.i_dev != inodeDevice) {
		printf("Didn't find inode in core.\n");
		return (1);
	}
	
	/*
	 *  Get the vm_info struct, and use that to grab the object.
	 */
	vmInfoPtr = (int) ip.i_vnode.vm_info;
	if (kernelGet(kmem, vmInfoPtr, &vmInfo, sizeof(struct vm_info))) {
		printf("doInode: can't read vm_info at 0x%x\n", vmInfoPtr);
		return(1);
	}

	printf("\n%s: ", fileName);
	if (vmInfo.pager == 0) {
		printf("file is not resident.\n");
		return 1;
	}

	/*
	 *  Grab the object, print out the number of resident pages, and
	 *  traverse the page list to see what's present.
	 */
	objectPtr = (int) vmInfo.object;
	if (kernelGet(kmem, objectPtr, &object, sizeof(struct vm_object))) {
		printf("doInode: can't read object at 0x%x\n", objectPtr);
		return (1);
	}

	if (vmInfo.pager != object.pager) {			/* sanity check */
		
		objectPtr = my_vm_object_lookup(kmem, vmInfo.pager, &object);
		if (objectPtr == 0) {
			printf("\nObject not found, sorry.\n");
			return 1;
		}
	}
		
	if (object.resident_page_count == 0) {
		printf("no pages are resident.\n");
		return 1;
	}
	
	pageArray = (int *) malloc(sizeof(int) * object.resident_page_count);
	pageSize = getpagesize();
	
	pageCount = 0;
	pagePtr = (int) object.memq.next;
	do {
		/* get next page */
		if (kernelGet(kmem, pagePtr, &page, sizeof(struct vm_page))) {
			printf("Couldn't read page at 0x%x\n", pagePtr);
			return 1;
		}
		
		pageArray[pageCount] = page.offset / pageSize;
		pageCount += 1;
		pagePtr = (int) page.listq.next;

	} while (pagePtr != objectPtr);
	
	if (pageCount != object.resident_page_count) {
		printf("Resident page count was %d but %d pages were found.\n",
			object.resident_page_count, pageCount);
	}
	
	qsort(pageArray, pageCount, sizeof(int), compareInt);

	printPages(pageArray, pageCount);
}


int
kernelGet(int kmem, int addr, void *buf, int size)
{
	lseek(kmem, (long) addr, 0);
	if (read(kmem, (char *) buf, size) != size)
		return 1;
	else
		return 0;
}


int
compareInt(int *i1, int *i2)
{
	return *i1 - *i2;
}

void
printPages(int *pageArray, int pageCount)
{
	int pagesPerLine = 19;
	int numLines = pageCount / pagesPerLine;
	int lastLine = pageCount % pagesPerLine;
	int line, item;
	
	printf("%d resident pages.\n", pageCount);
	
	for (line=0; line < numLines; line++) {
		for (item=0; item < pagesPerLine; item++)
			printf("%4d", pageArray[line * pagesPerLine + item]);
		printf("\n");
	}
	
	for (item = 0;  item < lastLine;  item++)
		printf("%4d", pageArray[numLines * pagesPerLine + item]);
		
	printf("\n\n");
}
