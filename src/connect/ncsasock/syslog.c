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
* Revision 6.0  1997/08/25 18:38:17  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.3  1995/06/02  16:29:03  kans
 * *** empty log message ***
 *
 * Revision 1.2  1995/05/17  17:58:10  epstein
 * add RCS log revision history
 *
 */
 
/*
 * syslog, openlog, closelog - hacked from Unix - use dprintf
 * perror - hacked from Unix - uses dprintf
 * Modified to use StdArg by Charlie Reiman.
 * Wednesday, August 8, 1990 2:55:43 PM
 */

#include <syslog.h>
#include <StdArg.h>
#include <stdio.h>

extern int errno;
extern int sys_nerr;
extern char *sys_errlist[];

/*
 * a version of dprintf was here, but it was also defined in
 * dprintf.c. I've removed this one.
 * Charlie Reiman
 * Wednesday, August 8, 1990 2:53:49 PM
 */

#define MAXLINE 1000

static int LogMask = LOG_DEBUG;
static char LogTag[100] = "";

openlog(char *ident, int logstat)
{
	if (logstat >= LOG_ALERT && logstat <= LOG_DEBUG)
		LogMask = logstat;
		
	if (ident)
		strcpy(LogTag,ident);
	return 0;
}

closelog()
{
	return 0;
}

syslog(int pri,char *fmt,...)
{
	register char *b, *f = fmt, c;
	char buf[MAXLINE+50];
	char oline[MAXLINE];
	va_list nextArg;
	
	va_start(nextArg,fmt);
	
	if (pri > LogMask)
		return 0;

	b = buf;
	while ((c = *f++) != '\0' && b < buf + MAXLINE) 
	{
		if (c != '%') 
		{
			*b++ = c;
			continue;
		}
		c = *f++;
		if (c != 'm') 
		{
			*b++ = '%', *b++ = c;
			continue;
		}
		if ((unsigned)errno > sys_nerr)
			sprintf(b, "error %d", errno);
		else
			sprintf(b, "%s", sys_errlist[errno]);
		b += strlen(b);
	}
	if (b[-1] != '\n')
		*b++ = '\n';
	*b = '\0';
		
	vsprintf(oline, buf, nextArg);

	dprintf("%s: %s\n", LogTag,oline);
	return 0;
}

