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
}
