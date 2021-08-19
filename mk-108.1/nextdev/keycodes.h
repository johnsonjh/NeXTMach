/*	@(#)keycode.h	1.0	10/12/87	(c) 1987 NeXT	*/

/* 
 * HISTORY
 *  6-Nov-90  John Seamons (jks) at NeXT
 *	For ISO keyboard: sft-keypad-'=' is '|', sft-keypad-'/' is '\',
 * 	ctrl-sft-keypad-'=' same as ctrl-'|', ctrl-sft-keypad-'/' same as
 *	ctrl-'\'.  Consistency changes: ctrl-'-' same as ctrl-'_', ctrl-'6'
 *	same as ctrl-'^'.
 *
 *  6-Aug-89  John Seamons (jks) at NeXT
 *	Made shift-esc generate tilda per Leo.
 *
 *  7-Jul-88  John Seamons (jks) at NeXT
 *	Finally exchanged codes for the escape and '~' keys.
 *	Swap your keycaps!
 *
 * 12-Oct-87  John Seamons (jks) at NeXT
 *	Created.
 */

#ifndef	_KEYCODES_
#define	_KEYCODES_

#define	nul	0x000
#define	soh	0x001
#define	stx	0x002
#define	etx	0x003
#define	eot	0x004
#define	enq	0x005
#define	ack	0x006
#define	bel	0x007
#define	bs	0x008
#define	ht	0x009
#define	nl	0x00a
#define	vt	0x00b
#define	np	0x00c
#define	cr	0x00d
#define	so	0x00e
#define	si	0x00f
#define	dle	0x010
#define	dc1	0x011
#define	dc2	0x012
#define	dc3	0x013
#define	dc4	0x014
#define	nak	0x015
#define	syn	0x016
#define	etb	0x017
#define	can	0x018
#define	em	0x019
#define	sub	0x01a
#define	esc	0x01b
#define	fs	0x01c
#define	gs	0x01d
#define	rs	0x01e
#define	us	0x01f
#define	del	0x07f
#define	inv	0x100		/* invalid key code */
#define	dim	0x101
#define	quiet	0x102
#define	left	0x103
#define	down	0x104
#define	right	0x105
#define	up	0x106
#define	bright	0x107
#define	loud	0x108
#define	mouse_l	0x109

u_short ascii[] = {

	/* each non-meta code followed by its shifted value */
	inv, inv, dim, dim, quiet, quiet, '\\', '|',
	']', '}', '[', '{', 'i', 'I', 'o', 'O',
	'p', 'P', left, left, inv, inv, '0', '0',
	'.', '.', '\r', '\r', inv, inv, down, down,
	right, right, '1', '1', '4', '4', '6', '6',
	'3', '3', '+', '+', up, up, '2', '2',
	'5', '5', bright, bright, loud, loud, del, bs,
	'=', '+', '-', '_', '8', '*', '9', '(',
	'0', ')', '7', '7', '8', '8', '9', '9',
	'-', '-', '*', '*', '`', '~', '=', '|',
	'/', '\\', inv, inv, '\r', '\r', '\'', '"',
	';', ':', 'l', 'L', ',', '<', '.', '>',
	'/', '?', 'z', 'Z', 'x', 'X', 'c', 'C',
	'v', 'V', 'b', 'B', 'm', 'M', 'n', 'N',
	' ', ' ', 'a', 'A', 's', 'S', 'd', 'D',
	'f', 'F', 'g', 'G', 'k', 'K', 'j', 'J',
	'h', 'H', '\t', '\t', 'q', 'Q', 'w', 'W',
	'e', 'E', 'r', 'R', 'u', 'U', 'y', 'Y',
	't', 'T', esc, '~', '1', '!', '2', '@',
	'3', '#', '4', '$', '7', '&', '6', '^',
	'5', '%',

	/* control & control-shift codes */
	inv, inv, dim, dim, quiet, quiet, fs, fs,
	gs, gs, esc, esc, ht, ht, si, si,
	dle, dle, left, left, inv, inv, '0', '0',
	'.', '.', '\r', '\r', inv, inv, down, down,
	right, right, '1', '1', '4', '4', '6', '6',
	'3', '3', '+', '+', up, up, '2', '2',
	'5', '5', bright, bright, loud, loud, inv, inv,
	'=', '+', us, us, '8', '*', '9', '(',
	'0', ')', '7', '7', '8', '8', '9', '9',
	'-', '-', '*', '*', '`', '~', '=', fs,
	'/', fs, inv, inv, '\r', '\r', '\'', '"',
	';', ':', np, np, ',', '<', '.', '>',
	'/', '?', sub, sub, can, can, etx, etx,
	syn, syn, stx, stx, cr, cr, so, so,
	nul, nul, soh, soh, dc3, dc3, eot, eot,
	ack, ack, bel, bel, vt, vt, nl, nl,
	bs, bs, '\t', '\t', dc1, dc1, etb, etb,
	enq, enq, dc2, dc2, nak, nak, em, em,
	dc4, dc4, esc, '~', '1', '!', nul, nul,
	'3', '#', '4', '$', '7', '&', rs, rs,
	'5', '%',
};

#endif	_KEYCODES_
