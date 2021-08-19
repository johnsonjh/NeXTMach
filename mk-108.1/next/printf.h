/*	@(#)printf.h	1.0	11/11/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * printf.h -- definitions for extended kernel printf
 *
 * HISTORY
 * 11-Nov-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 


#ifndef _NEXT_PRINTF_
#define	_NEXT_PRINTF_

/*
 * bit field descriptions for printf %r and %R formats
 */

/*
 * printf("%r %R", val, reg_descp);
 * struct reg_desc *reg_descp;
 *
 * the %r and %R formats allow formatted output of bit fields.
 * reg_descp points to an array of reg_desc structures, each element of the
 * array describes a range of bits within val.  the array should have a
 * final element with all structure elements 0.
 * %r outputs a string of the format "<bit field descriptions>"
 * %R outputs a string of the format "0x%x<bit field descriptions>"
 *
 * The fields in a reg_desc are:
 *	unsigned rd_mask;	An appropriate mask to isolate the bit field
 *				within a word, and'ed with val
 *
 *	int rd_shift;		A shift amount to be done to the isolated
 *				bit field.  done before printing the isolate
 *				bit field with rd_format and before searching
 *				for symbolic value names in rd_values
 *
 *	char *rd_name;		If non-null, a bit field name to label any
 *				out from rd_format or searching rd_values.
 *				if neither rd_format or rd_values is non-null
 *				rd_name is printed only if the isolated
 *				bit field is non-null.
 *
 *	char *rd_format;	If non-null, the shifted bit field value
 *				is printed using this format.
 *
 *	struct reg_values *rd_values;	If non-null, a pointer to a table
 *				matching numeric values with symbolic names.
 *				rd_values are searched and the symbolic
 *				value is printed if a match is found, if no
 *				match is found "???" is printed.
 *
 * printf("%n %N", val, reg_valuesp);
 * struct reg_values *reg_valuesp;
 *
 * the %n and %N formats allow formatted output of symbolic constants
 * Reg_valuesp is a pointer to an array of struct reg_values which pairs
 * numeric values (rv_value) with symbolic names (rv_name).  The array is
 * terminated with a reg_values entry that has a null pointer for the
 * rv_name field.  When %n or %N is used rd_values are searched and the
 * symbolic value is printed if a match is found, if no match is found
 * "???" is printed.
 *				
 * printf("%C", val);
 * int val;
 *
 * the %C format prints an int as a 4 character string.
 * The most significant byte of the int is printed first, the least
 * significant byte is printed last.
 */

/*
 * register values
 * map between numeric values and symbolic values
 */
struct reg_values {
	unsigned rv_value;
	char *rv_name;
};

/*
 * register descriptors are used for formatted prints of register values
 * rd_mask and rd_shift must be defined, other entries may be null
 */
struct reg_desc {
	unsigned rd_mask;	/* mask to extract field */
	int rd_shift;		/* shift for extracted value, - >>, + << */
	char *rd_name;		/* field name */
	char *rd_format;	/* format to print field */
	struct reg_values *rd_values;	/* symbolic names of values */
};

#endif _NEXT_PRINTF_

