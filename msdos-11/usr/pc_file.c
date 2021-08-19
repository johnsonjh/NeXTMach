/*
*****************************************************************************
	PC_FILE -  Miscelaneous File and file descriptor management functions
					   
Summary:

	These functions are private functions used by the po_ file io routines.

	Map a file descriptor to a file structure. Return null if the file is
	not open. If an error has occured on the file return NULL unless 
	allow_err is true.
	PC_FILE *pc_fd2file(fd, allow_err)	
		PCFD fd;
		BOOL allow_err;
		{

	Allocate a file structure an return its handle. Return -1 if no more 
	handles are available.
	PCFD pc_allocfile()
		{

	Free all core associated with a file descriptor and make the descriptor
	available for future calls to allocfile.
	VOID pc_freefile(fd)
		PCFD fd;
		{
	
	Free all files associated with a drive. Called by abort and close.
	VOID pc_free_all_fil(pdrive)
		DDRIVE *pdrive;
		{


 Description

 Returns

Example:

*****************************************************************************
*/
#include <posix.h>

IMPORT PC_FILE *filearray[];

/*
	Map a file descriptor to a file structure. Return null if the file is
	not open. If an error has occured on the file return NULL unless 
	allow_err is true.
*/
#ifndef ANSIFND
PC_FILE *pc_fd2file(fd, allow_err)	
	PCFD fd;
	BOOL allow_err;
#else
PC_FILE *pc_fd2file(PCFD fd,BOOL allow_err)		/* _fn_ */
#endif
	{
	PC_FILE *pfile;

	if (0 <= fd && fd < MAXUFILES)
		{
		if ( (pfile = filearray[fd]) != NULL )
			{
			if (pfile->error && !allow_err)
				return (NULL);
			else
				return(pfile);
			}
		}
	return (NULL);
	}

/* Assign zeroed out file structure to an FD and return the handle. Return 
   -1 on error. */
PCFD pc_allocfile() /* _fn_ */
	{
	FAST PC_FILE *pfile;
	LOCAL BOOL initfiles = YES;
	PCFD i;

	if (initfiles)
		for (initfiles = NO,i=0;i<MAXUFILES;filearray[i] = NULL,i++);

	for (i=0;i<MAXUFILES;i++)
		{
		if (!filearray[i])
			{
			if (!(pfile = (PC_FILE *) pc_malloc(sizeof(PC_FILE))))
				break;
			pc_memfill(pfile, sizeof(PC_FILE), (UTINY) 0);
			filearray[i] = pfile;
			return(i);
			}
		}
	
	return (-1);
	}

/* Free core associated with a file descriptor. Release the FD for later use */
#ifndef ANSIFND
VOID pc_freefile(fd)
	PCFD fd;
#else
VOID pc_freefile(PCFD fd) /* _fn_ */
#endif
	{
	FAST PC_FILE *pfile;
	UCOUNT cluster;

	if ( (pfile = pc_fd2file(fd, YES)) == NULL)
		return;

	if (pfile->pobj)
		pc_freeobj(pfile->pobj);
	if (pfile->bbase)
		pc_mfree (pfile->bbase);

	pc_mfree (pfile);
	filearray[fd] = NULL;
	}

/* Release all file descriptors associated with a drive and free up all core
   associated with the files
   called by dsk_close 
*/
#ifndef ANSIFND
VOID pc_free_all_fil(pdrive)
	FAST DDRIVE *pdrive;
#else
VOID pc_free_all_fil(FAST DDRIVE *pdrive) /* _fn_ */
#endif
	{
	FAST PC_FILE *pfile;
	PCFD i;

	for (i=0; i < MAXUFILES; i++)
		{
		if (pfile = filearray[i])
			{
			if (pfile->pobj->pdrive == pdrive)
				{
				/* print a debug message since in normal operation 
				   all files should be close closed before closing the drive */
				pr_db_str("pc_free_all freeing a file","");
				pc_freefile(i);
				}
			}
		}
	}
