/*****************************************************************************
	PC_MKFS  -  Make a file system on a disk

					   
Summary
	#include <pcdisk.h>

	BOOL pc_mkfs(driveno, pfmt )
		COUNT driveno;
		FMTPARMS *pfmt;

 Description
 	Given a drive number and a format parameter block. Put an MS-DOS
 	file system on the drive:
	The disk MUST already have a low level format. All blocks on the drive
	should be intitialize with E5's or zeros. 

	NOTE: NOT (4.0) compatible

	Some common paramaters. Note: For other drive types use debug to get the
	parameters from block zero after FORMAT has been run. 
								DRIVE SIZE
			360		720		20M
oemname		=====  UP TO YOU. ONLY 8 Chars matter. Right filled with ' ' 
secpalloc	2		2		4
secreserved	1		1		1
numfats		2		2		2
numroot		0x70	0x70	0x200
mediadesc	0xFD	0xF9	0xF8
secptrk		9		9		0x11
numhead		2		2		4
numtrk		40		80		612

 Returns
 	Returns YES if the filesystem disk was successfully initialized.
Example:
	#include <pcdisk.h>
	FMTPARMS fmt;
  
    Make a file system on a 360 K floppy drive. (assumed to be drive 0)
	strcpy(&fmt.oemname[0], "EBS");
	fmt.secpalloc = (UTINY) 2;
	fmt.secreserved = (UCOUNT)	1;
	fmt.numfats	 =	(UTINY) 2;
	fmt.numroot	 =	(UCOUNT) 0x70;
	fmt.mediadesc =	(UTINY) 0xFD;
	fmt.secptrk	 =  (UCOUNT) 9;
	fmt.numhead	 =	(UCOUNT) 2;
	fmt.numtrk	 =	(UCOUNT) 40;
	
	if (!pc_mkfs(0, &fmt ))
		printf("Format failed \n");
*****************************************************************************
*/
#include <pcdisk.h>

#ifndef ANSIFND
BOOL pc_mkfs(driveno, pfmt )
	COUNT driveno;
	FAST FMTPARMS *pfmt;
	{
#else
BOOL pc_mkfs(COUNT driveno, FAST FMTPARMS *pfmt) /* _fn_ */
	{
#endif
	UTINY b[512];
	UCOUNT totsecs;
	UCOUNT nclusters;
	UCOUNT data_area;
	COUNT fausize;
	COUNT i,j;
	UCOUNT blockno;
	
	/* Build up a block 0 */
	pc_memfill(&b[0], 512, '\0');
	b[0] = (UTINY) 0xe9;	/* Jump vector. Used to id MS-DOS disk */
	b[1] = (UTINY) 0x00;
	b[2] = (UTINY) 0x00;

	/* Copy the OEM name */
	pc_cppad(&b[3], &(pfmt->oemname[0]), 8);
	/* bytes per sector */
	fr_WORD ( &(b[11]), (UCOUNT) 512);
	/* sectors / cluster */
	b[13] = pfmt->secpalloc;
	/* Number of reserved sectors. (Including block 0) */
	fr_WORD ( &(b[14]), pfmt->secreserved);
	/* number of dirents in root */
	fr_WORD ( &(b[17]), pfmt->numroot);
	/* total sectors in the volume */
	fr_WORD ( &(b[19]), (UCOUNT)(totsecs = 
		 (pfmt->numtrk * pfmt->secptrk * pfmt->numhead)));
	/* Media descriptor */
	b[21] = pfmt->mediadesc;
	/* sectors per trak */
	fr_WORD ( &(b[24]), pfmt->secptrk);
	/* number heads */
	fr_WORD ( &(b[26]), pfmt->numhead);
	/* number hidden sectors */
	fr_WORD ( &(b[28]), (UCOUNT) 0);
	/* number of duplicate fats */
	b[16] = pfmt->numfats;
	/* Sectors per fat */
	fr_WORD ( &(b[22]), pfmt->secpfat);


	data_area = totsecs;
	data_area -= pfmt->numfats * pfmt->secpfat;
	data_area -= pfmt->secreserved;
	data_area -= pfmt->numroot/INOPBLOCK;

	/* Nibbles/fat entry if < 4087 clusters then 12 bit else 16 */
	nclusters =  data_area/pfmt->secpalloc;
	fausize =  (nclusters < 4087) ? 3 : 4;

	/* Check the FAT. 
	if ( (nibbles needed) > (nibbles if fatblocks)
			trouble;
	*/
	if ( (fausize * nclusters) > (pfmt->secpfat << 10) )
			{
			pr_er_putstr("File allocation Table Too Small, Can't Format\n");
			return (NO);
			}

	if (pfmt->numroot % INOPBLOCK)
			{
			pr_er_putstr("Numroot must be an even multiple of INOPBLOCK\n");
			return (NO);
			}


	/* Now write block 0 */
	if (!(pblock(driveno, (UCOUNT) 0, &(b[0]), 1) ))
		{
		pr_er_putstr("Failed writing block 0 \n");
		return (NO);
		}

	/* Now write the fats out */
	for (i = 0; i < pfmt->numfats; i++)
		{
		pc_memfill(&b[0], 512, '\0');
		/* The first 4(3) bytes of a fat are MEDIADESC,FF,FF,(FF) */
		b[0] = pfmt->mediadesc;
		b[1] = (UTINY) 0xff;
		b[2] = (UTINY) 0xff;
		if (fausize == 4)
			b[3] = (UTINY) 0xff;

		blockno = pfmt->secreserved + (i * pfmt->secpfat);
		for ( j = 0; j < pfmt->secpfat; j++)
			{
			if (!(pblock(driveno, blockno, &(b[0]), 1) ))
				{
				pr_er_putstr("Failed writing FAT block\n");
				return (NO);
				}
			blockno += 1;
			pc_memfill(&b[0], 512, '\0');
			}
		}
	
	/* Now write the root sectors */
	blockno = pfmt->secreserved + pfmt->numfats * pfmt->secpfat;
	pc_memfill(&b[0], 512, '\0');
	for ( j = 0; j < (pfmt->numroot/INOPBLOCK) ; j++)
		{
		if (!(pblock(driveno, blockno, &(b[0]), 1) ))
			{
			pr_er_putstr("Failed writing root block\n");
			return (NO);
			}
		blockno += 1;
		}
	
	return (YES);
	}

