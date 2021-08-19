#import <stdio.h>
#import <nlist.h>

#import <sys/types.h>
#import <sys/file.h>
#import <sys/stat.h>
#import <sys/loader.h>
#import <sys/kern_return.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif TRUE


/*
 *  Data structures.
 */
struct symbolTable {
	int numEntries;
	struct nlist *symbolList;
};


struct profile {
	char *symbol;
	int ticks;
};

struct sectionTable {
	int numEntries;
	struct section sectionList[255];
};


/*
 * Globals.
 */
int debug = 0;

#define LSYS_SIZE 262144
#define LNEXT_SIZE 262144

int libSysSamples[LSYS_SIZE], libNeXTSamples[LNEXT_SIZE];
struct sampleInfo *libSysResults, *libNeXTResults;

/*
 *  Function prototypes.
 */
int processMachO(char *fileName,
	     void **dataPointer,
	     struct symbolTable **symbolTable,
	     struct sectionTable *sectionTable);
	     
void processSegment( void *fileData,
		struct sectionTable *mySectionTable,
		struct segment_command *segmentCommand);	

struct symbolTable *getSymbolTable(void *fileData,
				   struct symtab_command *symTabCmd);
				   
int compareSymbols(struct nlist *n1, struct nlist *n2);
int compareSections(struct section *s1, struct section *s2);
int compareProfilesByTicks(struct profile *r1, struct profile *r2);
int compareProfilesBySymbol(struct profile *r1, struct profile *r2);
int compareNameToProfile(char *name, struct profile *profile);

void generateResults(struct symbolTable *symTab,
	     	     int sampleOffset,
	     	     int sampleScale,
	     	     int samples[],
	     	     int numSamples,
	     	     struct profile *results,
	     	     int *numResults);
	     
void printSymbols(struct symbolTable *symTab, int startAddr, int size);
		  
int printPage(int filePageNum,
	       struct symbolTable *symTab,
	       struct sectionTable *secTab);

main(int argc, char *argv[])
{
	kern_return_t result;
	char *fileName;
	void *fileData;
	struct stat statBuf;
	struct symbolTable *mySymbolTable;
	struct sectionTable mySectionTable;
	struct profile *libSysResults;
	int numLibSysResults;
	int i=0;
	int doHistogram = 0;
	int doObjectFiles = 0;
	char objectFileName[1024];

	/*
	 *	parse args,
	 */
	if (argc < 2)
		usage(argv[0]);
	
	fileName = argv[1];
	
	/*
	 *  Process the object file
	 */
	if (processMachO(fileName, &fileData, &mySymbolTable, &mySectionTable))
		exit(1);
	
	i = 2;
	while (i < argc) {
		printPage(atoi(argv[i]), mySymbolTable, &mySectionTable);
		i++;
	}
}


usage(char *string)
{
	printf("usage: %s   library   page list\n", string);
	exit(1);
}


int
processMachO(char *fileName,
	     void **dataPointer,
	     struct symbolTable **symbolTable,
	     struct sectionTable *sectionTable)
{
	int size;
	kern_return_t result;
	struct mach_header *fileHeader;
	void *fileData;
	struct load_command *command;
	int commandNum;
	struct symtab_command *symTabCmd;
	int foundSymbolTable = FALSE;

	if (mapFile(fileName, &fileData, &size))
		return 1;
	
	fileHeader = (struct mach_header *) fileData;
	if (debug)
		printf("there are %d load commands.\n", fileHeader->ncmds);
	
	/*
	 *  Process the file.
	 *	Loop through all the commands
	 *		If it's a symbol table, read it in
	 *		Otherwise ignore the command.
	 */
	commandNum = 0;
	command = fileData + sizeof(struct mach_header);
	while (commandNum < fileHeader->ncmds) {
		switch(command->cmd) {
			struct segment_command *segCmd;
		
			case LC_SYMTAB:
				if (debug)
					printf("Processing symtab command.\n");
				symTabCmd = (struct symtab_command *) command;
				foundSymbolTable = TRUE;
				break;
				
			case LC_SEGMENT:
				if (debug)
					printf("Processing segment command.\n");
				segCmd = (struct segment_command *) command;
				processSegment(fileData, sectionTable, segCmd);
				break;
				
			default:
				if (debug)
					printf("Skipping command type %d.\n",
						command->cmd);
				break;
		}
		
		command = (struct load_command *) ((int) command+command->cmdsize);
		commandNum += 1;
	}
	
	/*
	 *  Sort the sections by increasing file offset
	 */
	qsort( (char *) sectionTable->sectionList,
	       sectionTable->numEntries,
	       sizeof(struct section),
	       compareSections);
	/*
	 *  Process the symbol table now that we've scanned the entire file.
	 */
	if (! foundSymbolTable) {
		printf("No symbol table found!\n");
		return 1;
	}
	
	*symbolTable = getSymbolTable(fileData, symTabCmd);
	*dataPointer = fileData;
	
	return 0;
}


struct symbolTable *
getSymbolTable(void *fileData, struct symtab_command *symTabCmd)
{
	int numSymbols, i;
	struct symbolTable *result;
	struct nlist *symbol;
	
	numSymbols = symTabCmd->nsyms;
	
	/*
	 *  Allocate memory for our internal symbol table.  For now, we just
	 *  allocate enough to hold all the symbols, even though we'll throw
	 *  a lot out.
	 */
	result = (struct symbolTable *) malloc(sizeof(struct symbolTable));
	result->numEntries = 0;
	result->symbolList = (struct nlist *)
				calloc(numSymbols, sizeof(struct nlist));
	
	/*
	 *  Loop through the symbol table and grab all the interesting symbols.
	 *  For now, a symbol is only interesting if it of type N_SECT.
	 */
	symbol = (struct nlist *) ((int) fileData + (int) symTabCmd->symoff);
	for (i = 0;  i < numSymbols;  i++) {
		if ((symbol[i].n_type & N_TYPE) == N_SECT) {
			struct nlist temp = symbol[i];
			
			temp.n_un.n_strx += (int) fileData + symTabCmd->stroff;
			result->symbolList[result->numEntries] = temp;
			result->numEntries += 1;
		}
	}
	
	/*
	 *  Sort the symbols by increasing value.
	 */
	qsort( (char *) result->symbolList,
		result->numEntries,
		sizeof(struct nlist),
		compareSymbols);
		
	return result;
}


void
processSegment( void *fileData,
		struct sectionTable *mySectionTable,
		struct segment_command *segmentCommand)
{
	int numSections = segmentCommand->nsects;
	int i;
	struct section *sectionOffset;

	sectionOffset = (struct section *)
			((char *) segmentCommand + sizeof(struct segment_command));
	
	for (i = 0;  i < numSections;  i++) {
					
		mySectionTable->sectionList[mySectionTable->numEntries] = 
			*sectionOffset;
			
		mySectionTable->numEntries += 1;	
		sectionOffset += 1;
	}
}


void
generateResults(struct symbolTable *symTab,
	     int sampleOffset,
	     int sampleScale,
	     int samples[],
	     int numSamples,
	     struct profile *results,
	     int *numResults)
{
	struct nlist *symList;			/* list of symbols */
	int symIndex;				/* index into list of symbols */
	int lastSymIndex = -1;			/* last symbol we used */
	int sampleNum;				/* index into list of samples */
	int samplePC;				/* a sample's PC */

	if (symTab->numEntries <= 0) {
		printf("No symbols to report!\n");
		return;
	}
	
	*numResults = 0;
	symIndex = 0;
	symList = symTab->symbolList;
	for (sampleNum = 0;  sampleNum < numSamples;  sampleNum++) {
		
		samplePC = sampleOffset + (sampleNum << sampleScale);
		
		/*
		 *  Skip over irrelevant symbols:
		 *	while symIndex is still in bounds, and
		 *	samplePC is not between the current symbol and the next one
		 *      go to the next symbol.
		 */
		while (symIndex + 1 < symTab->numEntries &&
			! ((samplePC < symList[symIndex + 1].n_value) &&
			   (samplePC >= symList[symIndex].n_value)))
			symIndex++;
		
		if (symIndex < symTab->numEntries) {	/* got one */
			if (symIndex == lastSymIndex)
				results[*numResults-1].ticks += samples[sampleNum];
			else {
				results[*numResults].ticks = samples[sampleNum];
				results[*numResults].symbol =
						symList[symIndex].n_un.n_name;
				*numResults += 1;
				lastSymIndex = symIndex;
			}
		}
	
	}
	
	/*
	 *  Sort the results by increasing tick value.
	 */
	qsort( (char *) results,
		*numResults,
		sizeof(struct profile),
		compareProfilesByTicks);
}


int
printPage(int filePageNum,
	       struct symbolTable *symTab,
	       struct sectionTable *secTab)
{
	int pageSize = getpagesize();
	int fileStartAddr = filePageNum * pageSize;
	int section;
	struct section *secList;
	int virtualStartAddr;
	int sectionOffset;

	/*
	 *  Run through the sections to find the virtual address associated
	 *  with our file address
	 */
	section = 0;
	secList = secTab->sectionList;
	while (section < secTab->numEntries &&
	       fileStartAddr >= secList[section].offset + secList[section].size)
	       section += 1;
	
	if (section == secTab->numEntries) {		/* not found */
		printf("Page %d is past the end of known sections.\n",
			filePageNum);
		return -1;
	}
	
	if (fileStartAddr < secList[section].offset) {
		printf("Page %d was between sections.\n", filePageNum);
		return -1;
	}
	
	sectionOffset = fileStartAddr - secList[section].offset;
	virtualStartAddr = secList[section].addr + sectionOffset;
	
	printf("\nSymbols on file page %d, virtual address 0x%x.\n",
		filePageNum, virtualStartAddr);
	printSymbols(symTab, virtualStartAddr, pageSize);
}


void
printSymbols(struct symbolTable *symTab, int startAddr, int size)
{
	int numSymbols = symTab->numEntries;
	struct nlist *symList = symTab->symbolList;
	int symIndex = 0;
	
	/*
	 *  Skip over all the symbols we don't want to print
	 */
	while (symIndex < numSymbols && symList[symIndex].n_value < startAddr)
		symIndex += 1;
	
	/*
	 *  Print the symbols starting at startAddr
	 */
	while (symIndex < numSymbols &&
	       symList[symIndex].n_value < startAddr + size) {
	       printf("  0x%08x %s\n", symList[symIndex].n_value,
	       			      symList[symIndex].n_un.n_name);
	       symIndex += 1;
	}
}



int
mapFile(char *fileName, int *address, int *size)
{
	struct stat statBuf;
	int result;
	int fd;
	
	fd = open(fileName, O_RDONLY, 0);
	if (fd < 0) {
		perror(fileName);
		return 1;
	}
	
	if (fstat(fd, &statBuf) < 0) {
		perror(fileName);
		close(fd);
		return 1;
	}
	
	*size = statBuf.st_size;
	result = map_fd(fd, 0, (vm_offset_t *) address, 1, statBuf.st_size);
	if (result != KERN_SUCCESS) {
		printf("Could not map file %s.\n", fileName);
		close(fd);
		return 1;
	}
	
	close(fd);
	return 0;
}
	
	
int
compareSymbols(struct nlist *n1, struct nlist *n2)
{
	return n1->n_value - n2->n_value;
}

int
compareProfilesByTicks(struct profile *r1, struct profile *r2)
{
	return r2->ticks - r1->ticks;
}

int
compareProfilesBySymbol(struct profile *r1, struct profile *r2)
{
	return strcmp(r1->symbol, r2->symbol);
}

int
compareNameToProfile(char *name, struct profile *profile)
{
	return strcmp(name, profile->symbol);
}

int
compareSections(struct section *s1, struct section *s2)
{
	return s1->offset - s2->offset;
}


/*
 *  Icky namelist stuff
 */

#define NAMETBLSIZE 3

char	*names[NAMETBLSIZE] = {
	"_libSysData",
#define X_LSYS	0
	"_libNeXTData",
#define X_LNEXT 1
	""
};

int
bigRead(int fd, char *buf, int size)
{
#define CHUNKSIZE 8192 * 8
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

	int cc = 1;
	int bytesRead = 0;
	
	while (cc > 0 && bytesRead < size) {
		cc = read(fd, buf + bytesRead, MIN(size - bytesRead, CHUNKSIZE));
		bytesRead += cc;
	}
	
	if (cc < 0)
		return -1;
	else
		return bytesRead;
}


getShlibSamples()
{
	static int kmem;
	static int initialized = FALSE;
	struct nlist nl[NAMETBLSIZE];
	int libSysHits, libNeXTHits, i;

	if (!initialized) {
	
		kmem = open("/dev/kmem", 0);  /* grab kernel memory */
		if (kmem < 0) {
			perror("/dev/kmem");
			exit(1);
		}
		
		initnlist(nl, names);	/* lookup kernel variables */
		nlist("/mach", nl);
		
		initialized = TRUE;
	}
	
	lseek(kmem, (long)(nl[X_LSYS].n_value), 0);
	if (bigRead(kmem, (char *)libSysSamples, sizeof(libSysSamples)) != sizeof(libSysSamples)) {
		printf("getkvars:  can't read libsys samples at 0x%x\n",
		nl[X_LSYS].n_value);
		return(1);
	}
	
	lseek(kmem, (long)(nl[X_LNEXT].n_value), 0);
	if (bigRead(kmem, (char *)libNeXTSamples, sizeof(libNeXTSamples)) != sizeof(libNeXTSamples)) {
		printf("getkvars:  can't read libNeXT samples at 0x%x\n",
		nl[X_LNEXT].n_value);
		return(1);
	}

	libNeXTHits = 0;
	libSysHits = 0;
	for (i=0; i<LSYS_SIZE; i++) {
		libSysHits += libSysSamples[i];
		libNeXTHits += libNeXTSamples[i];
	}
	
	printf("hits on libsys: %d\n", libSysHits);
	printf("hits on libNeXT: %d\n", libNeXTHits);

	return(0);
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

