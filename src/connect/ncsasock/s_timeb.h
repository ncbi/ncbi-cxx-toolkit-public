/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)timeb.h	7.1 (Berkeley) 6/4/86
*
*
* RCS Modification History:
* $Log$
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.2  1995/05/17  17:57:33  epstein
 * add RCS log revision history
 *
 */

/*
 * Structure returned by ftime system call
 */
struct timeb
{
	time_t	time;
	unsigned short millitm;
	short	timezone;
	short	dstflag;
};
