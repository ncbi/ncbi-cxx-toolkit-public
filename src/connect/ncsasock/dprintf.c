/*
 * BSD-style socket emulation library for the Mac
 * Original author: Tom Milligan
 * Current author: Charlie Reiman - creiman@ncsa.uiuc.edu
 *
 * This source file is placed in the public domian.
 * Any resemblance to NCSA Telnet, living or dead, is purely coincidental.
 *
 *      National Center for Supercomputing Applications
 *      152 Computing Applications Building
 *      605 E. Springfield Ave.
 *      Champaign, IL  61820
*
*
* RCS Modification History:
* $Log$
* Revision 6.0  1997/08/25 18:37:25  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.3  1995/06/02  16:29:03  kans
 * *** empty log message ***
 *
 * Revision 1.2  1995/05/17  17:56:38  epstein
 * add RCS log revision history
 *
 */

/*
 * dprintf - 
 *  debugging printf. Most of the debugging code is in disrepair, use
 *  with caution. 
 */
 
#include <stdio.h>
#include <StdArg.h>

dprintf(char *fmt,...)
{
	va_list	nextArg;
	
	va_start(nextArg,fmt);
	
	(void) vfprintf(stderr,fmt,nextArg);
	va_end(nextArg);   /* this is actually a null macro */
	return 0;
}
