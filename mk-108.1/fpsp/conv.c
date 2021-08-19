#include "stdio.h"
#include "ctype.h"

#define	NI	64
#define	NS	128

struct sub {
	char	*from, *to;
	short	action;
} sub[] = {
"fmovemx", "fmovemx", 3,	"movem", "moveml", 0,
"leal", "lea", 0,		"jsrl", "jsr", 0,	"peal", "pea", 0,

"bneb", "bne", 0,		"bnew", "bne", 0,	"bnel", "bne", 0,
"brab", "bra", 0,		"braw", "bra", 0,	"bral", "bra", 0,
"beqb", "beq", 0,		"beqw", "beq", 0,	"beql", "beq", 0,
"bcsb", "bcs", 0,		"bcsw", "bcs", 0,	"bcsl", "bcs", 0,
"bltb", "blt", 0,		"bltw", "blt", 0,	"bltl", "blt", 0,
"bccb", "bcc", 0,		"bccw", "bcc", 0,	"bccl", "bcc", 0,
"bgeb", "bge", 0,		"bgew", "bge", 0,	"bgel", "bge", 0,
"bleb", "ble", 0,		"blew", "ble", 0,	"blel", "ble", 0,
"bgtb", "bgt", 0,		"bgtw", "bgt", 0,	"bgtl", "bgt", 0,
"bplb", "bpl", 0,		"bplw", "bpl", 0, 	"bpll", "bpl", 0,
"bmib", "bmi", 0,		"bmiw", "bmi", 0,	"bmil", "bmi", 0,
"bhib", "bhi", 0,		"bhiw", "bhi", 0,	"bhil", "bhi", 0,
"bsrb", "bsr", 0,		"bsrw", "bsr", 0,	"bsrl", "bsr", 0,
"dbfw", "dbf", 0,		"stb",	"st",  0,	"dbraw","dbra",0,

"btstb", "btst", 0,		"btstw", "btst", 0,	"btstl", "btst", 0,
"bclrb", "bclr", 0,		"bclrw", "bclr", 0,	"bclrl", "bclr", 0,
"bsetb", "bset", 0,		"bsetw", "bset", 0,	"bsetl", "bset", 0,
"bchgb", "bchg", 0,		"bchgw", "bchg", 0,	"bchgl", "bchg", 0,
"fbgtb", "fbgt", 0,		"fbgtw", "fbgt", 0,	"fbgtl", "fbgt", 0,
"fbeqb", "fbeq", 0,		"fbeqw", "fbeq", 0,	"fbeql", "fbeq", 0,
"fbltb", "fblt", 0,		"fbltw", "fblt", 0,	"fbltl", "fblt", 0,
"fbgeb", "fbge", 0,		"fbgew", "fbge", 0,	"fbgel", "fbge", 0,
"fbneb", "fbne", 0,		"fbnew", "fbne", 0,	"fbnel", "fbne", 0,
"fbleb", "fble", 0,		"fblew", "fble", 0,	"fblel", "fble", 0,

"roxl", "roxll", 0,		"roxr", "roxrl", 0,	"wfp_uskx", "fmovex", 2,
"dcl", ".int", 0,		"dcw", ".word", 0,	"dcb", ".byte", 0,
"xdef", ".globl", 0,		"rd_uskb", "movb", 1,	"rd_uskw", "movw", 1,
"rd_uskl", "movl", 1,		"wr_uskb", "movb", 2,	"wr_uskw", "movw", 2,	"wr_uskl", "movl", 2,		"ad_uskl", "lea", 1,	"fp_uskx", "fmovex", 1,
"an_uskb", "andb", 2,		"an_uskw", "andw", 2,	"an_uskl", "andl", 2,
0, 0
};

main()
{
	char str[NS], *s, ss[NI][NS], *s1, *sp, t[NS], *cp, c,
		id1[NS], id2[NS], *i1, *i2, *index();
	int i, macro = 0, predec = 0, in_comment, si, j, seen_size;
	
	while (gets(s = str) != NULL) {
	
		/* empty lines */
		if (*s == 0) {
			printf ("\n");
			continue;
		}
		
		/* embedded preprocessor commands */
		if (*s == '#') {
			printf ("%s\n", s);
			continue;
		}
		
		/* labels starting in col 1 might not have terminating ':'s */
		if (*s && *s != ' ' && *s != '\t' && *s != '*' && *s != '#') {
			while (*s && *s != ':' && *s != ' ' && *s != '\t')
				s++;
			if (*s != ':') {
				strcpy (t, s);
				cp = s;
				*s++ = ':';
				*s++ = ' ';
				strcpy (s, t);
			}
			s = str;
		}
		
		/* full line comments */
		if (*s == '*') {
			printf ("//%s%s\n", ++s, macro? ";\\" : "");
			continue;
		}
		in_comment = 0;

		/* convert everything to lower case */
		while (*s) {
			if (isupper (*s))
				*s = tolower (*s);
			s++;
		}

		/* do inline substitutions, then rescan from beginning */
		s = str;
		while (*s) {
		
			/* 'fpiar' -> 'fpi' */
			if (strncmp (s, "fpiar", 5) == 0)
				strcpy (s+3, s+5);
		
			/* '$' -> "0x" hex constant */
			if (*s == '$') {
				for (i = strlen (s+1) + 1; i > 0; i--)
					*(s+i+1) = *(s+i);
				*s++ = '0';
				*s = 'x';
			}
			
			/* ';' -> "//" comment delimiter */
			if (*s == ';') {
				for (i = strlen (s+1) + 1; i > 0; i--)
					*(s+i+1) = *(s+i);
				*s++ = '/';
				*s = '/';
				in_comment = 1;
			}
			
			/* "..." -> "//" comment delimiter */
			/* also remove single '.' from opcodes */
			if (*s == '.') {
				if (*(s+1) == '.') {
					*s = '/';
					*(s+1) = '/';
					*(s+2) = ' ';
					in_comment = 1;
				} else {
					strcpy (t, s+1);
					strcpy (s, t);
				}
			}

			/* register indirect addressing */
			if (*s == '(' && !in_comment) {
				if (*(s-1) == '-') {
					predec = 1;
					strcpy (t, s+1);
					strcpy (s-1, t);
				} else {
				
					/* save stuff before '(' in *i1 */
					s1 = s-1;
					i1 = &id1[NS-1];
					*i1 = 0;
					while (*s1 != ' ' && *s1 != ',' &&
					    *s1 != '\t') {
						*(--i1) = *s1--;
					}
					s1++;
					
					/* remove the *i1 stuff and '(' */
					strcpy (t, s+1);
					strcpy (s1, t);
					
					/* skip over register specification */
					i2 = id2;
					*i2 = 0;
					s = s1;
					
					/* save any leading base displacement first */
					if (*(s+2) != ',' && *(s+2) != ')') {
						while (*s != ',')
							*i2++ = *s++;
						*i2++ = ',';
						*i2 = 0;
						strcpy (t, s+1);
						strcpy (s1, t);
						s = s1;
					}
					while (*s != ' ' && *s != ',' &&
					    *s != '\t' && *s != ')')
						s++;
					s1 = s + 1;
					if (*s == ',') {
						seen_size = 0;
						while (*s1 != ')') {
							if (*s1 == '.')
								seen_size = 1;
							if (*s1 == '*' &&
							    seen_size == 0) {
								strcpy (i2, ":l");
								i2 += 2;
							}
							*i2++ = *s1 == '.'? ':' :
								*s1 == '*'? ':' : *s1;
							s1++;
						}
						*i2 = 0;
						strcpy (t, s1);
						strcpy (s, t);
					}
					i2 = id2;
				}
			}
			if (*s == ')' && !in_comment) {
				if (*(s+1) == '+') {
					*s = '@';
				} else {
					if (predec) {
						strcpy (t, s+1);
						strcpy (s+2, t);
						*s++ = '@';
						*s = '-';
						predec = 0;
					} else {
						strcpy (t, s+1);
						*s++ = '@';
						if (*i1 || *i2)
							*s++ = '(';
						if (*i1) {
							strcpy (s, i1);
							s += strlen (i1);
						}
						if (*i2) {
							if (*i1)
								*s++ = ',';
							strcpy (s, i2);
							s += strlen (i2);
						}
						if (*i1 || *i2)
							*s++ = ')';
						strcpy (s, t);
					}
				}
			}
			
			/* "{n:m}" -> "{#n,#m}" bitfield expressions */
			if (*s == '{') {
				s1 = s+1;
				if (*s1 >= '0' && *s1 <= '9') {
					strcpy (t, s1);
					*s1++ = '#';
					strcpy (s1, t);
				}
				while (*s1 != ':')
					s1++;
				s1++;
				if (*s1 >= '0' && *s1 <= '9') {
					strcpy (t, s1);
					*s1++ = '#';
					strcpy (s1, t);
				}
				s = s1;
			}
			
			/* "#:" -> '$' inline constant */
			if (*s == '#' && *(s+1) == ':') {
				*(s+1) = '$';
			}
			if (*s)
				s++;
		}
		
		/* break up tokens seperated by white space */
		si = 0;
		s = str;
		while (*s) {
			if (*s && (*s == ' ' || *s == '\t')) {
				i = 0;
				while (*s && (*s == ' ' || *s == '\t'))
					ss[si][i++] = *s++;
				ss[si++][i] = 0;
			}
			if (*s && (*s != ' ' && *s != '\t')) {
				i = 0;
				while (*s && (*s != ' ' && *s != '\t'))
					ss[si][i++] = *s++;
				ss[si++][i] = 0;
			}
		}
		for (i = si; i < NI; i++)
			ss[i][0] = 0;
		i = 0;
		
		/* skip leading white space */
		c = ss[i][0];
		if (c == ' ' || c == '\t')
			printf ("%s", ss[i++]);

		/* translations that might eliminate label */
		cp = &ss[i][strlen(ss[i])-1];
		if (strcmp(ss[i+2], "equ") == 0 || strcmp(ss[i+2], "fequ") == 0) {
			if (ss[i+4][0] == '*') {
				printf ("%s\n", ss[i]);
			} else {
				if (*cp == ':')
					*cp = 0;
				printf ("#define %s %s\n", ss[i], ss[i+4]);
			}
			continue;
		} else
		if (strcmp(ss[i+2], "macro") == 0) {
			if (*cp == ':')
				*cp = 0;
			printf ("#define %s", ss[i]);
			i += 3;
			macro = 2;
		} else
		if (strcmp(ss[i+2], "idnt") == 0) {
			continue;
		}

		/* emit label (if any) */
		cp = &ss[i][strlen(ss[i])-1];
		if (*cp == ':') {
			printf ("%s", ss[i++]);
			if (i < si)
				printf ("%s", ss[i++]);
		}
		if (i >= si) {
			printf ("\n");
			continue;
		}
		
		/* force opcodes to be lower case */
		for (cp = ss[i]; *cp; cp++)
			if (isupper (*cp))
				*cp = tolower (*cp);
		
		/* do specific translations */
		i1 = ss[i];

		if (strcmp(i1, "section") == 0) {
			switch (atoi (ss[i+2])) {
			case 15:	printf (".data\n");  break;
			case 8:		printf (".text\n");  break;
			}
			continue;
		} else
		if (strcmp(i1, "xref") == 0 ||
		    strcmp(i1, "end") == 0 ||
		    strcmp(i1, "opt") == 0 ||
		    strcmp(i1, "nopage") == 0) {
			printf ("\n");
			continue;
		} else
		if (strcmp(i1, "include") == 0) {
			i += 2;
			sp = (char*)rindex(ss[i], '/');
			if (sp)
				sp++;
			else
				sp = ss[i];
			*(sp + strlen (sp) - 1) = 0;
			printf ("\n#include \"%s.h\"\n", sp);
			continue;
		} else
		if (strcmp(i1, "endm") == 0) {
			putchar (';');
			macro = 0;
		} else
		if (strcmp(i1, "dsx") == 0) {
			for (j = 0; j < atoi(ss[i+2])*3; j++)
				printf ("\n\t.long 0");
			i += 2;
		} else
		if (strcmp(i1, "dsl") == 0) {
			for (j = 0; j < atoi(ss[i+2]); j++)
				printf ("\n\t.long 0");
			i += 2;
		} else
		if (strcmp(i1, "dsw") == 0) {
			for (j = 0; j < atoi(ss[i+2]); j++)
				printf ("\n\t.word 0");
			i += 2;
		} else
		if (strcmp(i1, "dsb") == 0) {
			for (j = 0; j < atoi(ss[i+2]); j++)
				printf ("\n\t.byte 0");
			i += 2;
		} else {

			/* add missing comment indicator */
			if (si - i >= 4 && ss[i+4][0] != '/') {
				strcpy (t, ss[i+4]);
				strcpy (ss[i+4], "//");
				strcat (ss[i+4], t);
			}

			/* opcode substitutions */
			for (j = 0; sub[j].from; j++) {
				if (strcmp (i1, sub[j].from) == 0) {
					if (sub[j].action == 3) {
						char *dash, *sl, *comma;
						
						printf ("%s", i1);
						dash = index (ss[i+2], '-');
						sl = index (ss[i+2], '/');
						comma = index (ss[i+2], ',');
						if (sl || (dash && *(dash+1) == 'f') ||
						    *ss[i+2] == 'd' || *(comma+1) == 'd') {
							;
						} else {
							if (strncmp (ss[i+2], "fp", 2) == 0) {
								strcpy (t, comma);
								strcpy (comma, "-fp");
								comma[3] = comma[-1];
								strcpy (&comma[4], t);
							} else {
								strcpy (&comma[4], "-fp");
								comma[7] = comma[3];
								comma[8] = 0;
							}
						}
					} else
						printf ("%s", sub[j].to);
					if (sub[j].action == 1) {
						strcpy (t, ss[i+2]);
						strcpy (ss[i+2], "a6@(");
						strcat (ss[i+2], t);
						s = index (ss[i+2], ',');
						strcpy (t, s);
						*s++ = ')';
						strcpy (s, t);
					} else
					if (sub[j].action == 2) {
						s = index (ss[i+2], ',');
						strcpy (t, ++s);
						strcpy (s, "a6@(");
						s += 4;
						strcpy (s, t);
						strcat (s, ")");
					}
					break;
				}
			}
			if (sub[j].from == 0)
				i--;
		}
		i++;

		/* emit remaining tokens */
		for (; i < si; i++)
			printf ("%s", ss[i]);
			
		/* emit end-of-line */
		if (macro == 2) {
			printf ("\\\n");
			macro = 1;
		}
		if (macro == 1)
			printf (";\\\n");
		else
			putchar ('\n');
	}
}



