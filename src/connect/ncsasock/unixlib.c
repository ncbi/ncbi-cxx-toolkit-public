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
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.5  1995/05/23  15:31:16  kans
 * new CodeWarrior 6 errors and warnings fixed
 *
 * Revision 1.4  1995/05/17  17:58:16  epstein
 * add RCS log revision history
 *
 */
 
#ifdef USEDUMP
# pragma load "socket.dump"
#else
# include <Errors.h>
# include <Events.h>
# include <Files.h>
# include <OSUtils.h>
# include <Types.h>

# include <s_types.h>
# include <s_time.h>

#endif

#ifdef THINK_C
#include <FCntl.h>
#endif

#include <neti_in.h>
#include <sock_ext.h>
#include <sock_str.h>
#ifdef THINK_C
#include <pascal.h>
#else
#include <Strings.h>  /* for c2pstr */
#endif /* THINK_C */
#include <ToolUtils.h>

#include <s_timeb.h>

extern int errno;
extern long errno_long;
extern SpinFn spinroutine; 

/*
 * unix getwd
 *
 *	where must point to 256 bytes
 *	
 *	work up from the current directory to the root collecting 
 *	name segments as we go
 */

char *getwd(where) char *where;
{
	WDPBRec pb;
	CInfoPBRec cpb;
	char wdtemp[256],*start,*store,*trav;
	int i;

	/* get default volume and directory last set by PBSetVol or PBHSetVol */
	pb.ioNamePtr = (StringPtr) where;
	PBHGetVol(&pb,false);

	/* add a colon */
	(*where)++;
	where[*where] = ':';
	
	trav = wdtemp; /* build name here */
	cpb.dirInfo.ioCompletion = 0L;

	cpb.dirInfo.ioVRefNum = pb.ioWDVRefNum; /* vRefNum of volume on which */
											/* working dir exists */
	
	cpb.dirInfo.ioDrDirID = pb.ioWDDirID; /* directory ID of working directory */
	
	while(cpb.dirInfo.ioDrDirID != 0) 
	{
		cpb.hFileInfo.ioNamePtr = (StringPtr) trav; /* put name segment here */
		cpb.hFileInfo.ioFDirIndex = -1;
		if (PBGetCatInfo(&cpb,false) != 0)  
		{
			cpb.dirInfo.ioDrDirID = 0;
			break;
		}
		if (*trav == 0) 
		{
			cpb.dirInfo.ioDrDirID=0;
			break;
		}
		i=*trav;      /* save length */
		*trav=0;      /* null over length */
		start=trav+1; /* addr of 1st char */
		trav+=i+1;    /* point past current string for next name segment */ 
		*trav=0;      /* initially zero length */

		cpb.dirInfo.ioDrDirID = cpb.dirInfo.ioDrParID; /* point up to parent directory */
	}
	
	*wdtemp=0;
	store=where+*where+1; /* start storing after volume name */
	
	if (trav-wdtemp) 
		*where=(trav-wdtemp); /* set length of where as length of trav */
	if (trav!=wdtemp) 
		start=trav-1; 
	else 
		start=wdtemp;

	if (start!=wdtemp) 
	{
		while(*start) start--;		/* Go to beginning of string */
		if (start!=wdtemp) 
			start--;
	}

	while(start!=wdtemp) 
	{
		while(*start) /* Go to beginning of string */
			start--;		
		trav=start+1; 
		while (*trav)  /* store it */
		{
			*store=*trav; 
			store++;
			trav++; 
		} 
		*store=':';	/* Ready to move directory name */
		store++;
		if (start!=wdtemp) 
			start--;
		*store=0;
	}	
	return(p2cstr((unsigned char *) where));
}

/*
 * unix change working directory
 */
static int currentWD = 0;

#ifndef __MWERKS__
int chdir(pathName) char *pathName;
{
	WDPBRec pb;
	char tempst[256];
	long default_ioWDDirID;
	short wdToClose;

	/* 
	 * get default volume last set by PBSetVol or PBHSetVol 
	 */
	pb.ioNamePtr = 0L;
	PBHGetVol(&pb,false);
	default_ioWDDirID = pb.ioWDDirID;

	/*
	 * create a new mac working directory using the default volume and
	 * the callers partial pathname
	 */
	strcpy(tempst,pathName);
	pb.ioNamePtr = (StringPtr)c2pstr(tempst);
	pb.ioVRefNum = 0;
	pb.ioWDDirID = default_ioWDDirID;
	pb.ioCompletion = 0L;
	pb.ioWDProcID = 'UTCS';

	if ((errno = errno_long = PBOpenWD(&pb,0)) != noErr) 
		return(-1);
	
	/*
	 * make the mac working directory which has just been created in 'pb'
	 * the new default directory. destroy the mac working directory which
	 * was the previous default.
	 */
#ifdef THINK_C
	if ((errno = errno_long = SetVol(0L, pb.ioVRefNum)) != noErr) 
#else
	if ((errno = errno_long = setvol(0L, pb.ioVRefNum)) != noErr) 
#endif
	{
		wdToClose = pb.ioVRefNum;
	} 
	else 
	{
		wdToClose = currentWD;
		currentWD = pb.ioVRefNum;
	}

	/*
	 * close the previous working directory
	 */
	if (wdToClose == 0)	/* nothing more to do, return */
		return(0);

	pb.ioVRefNum = wdToClose;
	pb.ioCompletion = 0L;
	(void) PBCloseWD(&pb, false); /* ignore error */
	
	return(0);
}
#endif

/*
 * Mac version of Unix system call gettimeofday. 
 *
 * Time is converted to the Unix epoch: Jan 1, 1970.
 *
 * The current timezone is always GMT.
 */

static struct DateTimeRec unixEpochDtr = {1970,1,1, 0,0,0, 1};
gettimeofday(tp,tzp) struct timeval *tp; struct timezone *tzp;
{

	unsigned long unixEpochSecs,currentMacSecs;
	
	Date2Secs(&unixEpochDtr,&unixEpochSecs);
	GetDateTime(&currentMacSecs);
	tp->tv_sec = currentMacSecs - unixEpochSecs;
	tp->tv_usec = 0;

	if (tzp != NULL)
	{
		tzp->tz_minuteswest = 0;	/* minutes west of Greenwich */
		tzp->tz_dsttime = 0;	/* no dst correction */
	}
	return(0);
}

#if 0
/*
 * Backwards compatible time call.
 */
time_t
time(t)
	time_t *t;
{
	struct timeval tt;

	if (gettimeofday(&tt, (struct timezone *)0) < 0)
		return (-1);
	if (t)
		*t = tt.tv_sec;
	return (tt.tv_sec);
}
#endif


/*
 * The arguments are the number of minutes of time
 * you are westward from Greenwich and whether DST is in effect.
 * It returns a string
 * giving the name of the local timezone.
 *
 * Sorry, I don't know all the names.
 */

static struct zone {
	int	offset;
	char	*stdzone;
	char	*dlzone;
} zonetab[] = {
	-1*60, "MET", "MET DST",	/* Middle European */
	-2*60, "EET", "EET DST",	/* Eastern European */
	4*60, "AST", "ADT",		/* Atlantic */
	5*60, "EST", "EDT",		/* Eastern */
	6*60, "CST", "CDT",		/* Central */
	7*60, "MST", "MDT",		/* Mountain */
	8*60, "PST", "PDT",		/* Pacific */
#ifdef notdef
	/* there's no way to distinguish this from WET */
	0, "GMT", 0,			/* Greenwich */
#endif
	0*60, "WET", "WET DST",		/* Western European */
	-10*60, "EST", "EST",		/* Aust: Eastern */
	-10*60+30, "CST", "CST",	/* Aust: Central */
	-8*60, "WST", 0,		/* Aust: Western */
	-9*60, "JST", 0,		/* Japanese */
	-1
};

char *timezone(zone, dst)
{
	register struct zone *zp;
	static char czone[10];
	char *sign;
	register char *p, *q;
	char *getenv(), *strchr();

	if ((p = getenv("TZNAME")) != NULL) {
		if ((q = strchr(p, ',')) != NULL) {
			if (dst)
				return(++q);
			else {
				*q = '\0';
				strncpy(czone, p, sizeof(czone)-1);
				czone[sizeof(czone)-1] = '\0';
				*q = ',';
				return (czone);
			}
		}
		return(p);
	}
	for (zp=zonetab; zp->offset!=-1; zp++)
		if (zp->offset==zone) {
			if (dst && zp->dlzone)
				return(zp->dlzone);
			if (!dst && zp->stdzone)
				return(zp->stdzone);
		}
	if (zone<0) {
		zone = -zone;
		sign = "+";
	} else
		sign = "-";
	sprintf(czone, "GMT%s%d:%02d", sign, zone/60, zone%60);
	return(czone);
}

#ifdef JAE
/*
 * Sleep now calls the spinroutine to keep things
 * moving
 */
sleep(seconds) unsigned seconds;
	{
	long int wakeup = TickCount() + 60*seconds;
	long left;
	
	for (;;)
		{
		left = wakeup-TickCount();
		
		if (left <= 0)
			return;
			
		SPIN(false,SP_SLEEP,left)
		}
	}
#endif /* JAE */

#if 0
long int getpid()
{
	return (42);
}
#endif

long int getuid()
{
	return (0/*root*/);
}

struct passwd *getpwent()
{
	return (NULL /*not found*/);
}

struct passwd *getpwuid()
{
	return (NULL /*not found*/);
}

struct passwd *getpwnam()
{
	return (NULL /*not found*/);
}

#ifdef JAE
char *getlogin()
{
	return("macuser");
}
#endif /* JAE */

int chmod(path, mode)
	char *path;
	int mode;
{
#pragma unused(path)
#pragma unused(mode)
	return(0);
}

#if 0
access(path, mode)
	char *path;
	int mode;
{
#pragma unused(path)
#pragma unused(mode)
	return(0);
}

char *mktemp(template)
	char *template;
{
	return(template);
}

abort()
{
	exit(-1);
}
#endif

void bzero( b, s)
	char *b;
	long s;
{
	for( ; s ; ++b, --s)
		*b = 0;
}

bfill( b, s, fill)
	char *b;
	long s;
	char fill;
{
	for( ; s ; ++b, --s)
		*b = fill;
	return(0);
}

void bcopy (c1, c2, s)
	char		*c1, *c2;
	long		s;
{
	for ( ; s ; --s)
		*c2++ = *(c1++);
}


bcmp (c1, c2, s)
	char		*c1, *c2;
	long		s;
{
	for ( ; s ; --s)
		if(*c2++ != *(c1++)) return(1);
	return(0);
}

char	*sys_errlist[] = {
	"Error 0",
	"Not owner",				/* 1 - EPERM */
	"No such file or directory",		/* 2 - ENOENT */
	"No such process",			/* 3 - ESRCH */
	"Interrupted system call",		/* 4 - EINTR */
	"I/O error",				/* 5 - EIO */
	"No such device or address",		/* 6 - ENXIO */
	"Arg list too long",			/* 7 - E2BIG */
	"Exec format error",			/* 8 - ENOEXEC */
	"Bad file number",			/* 9 - EBADF */
	"No children",				/* 10 - ECHILD */
	"No more processes",			/* 11 - EAGAIN */
	"Not enough core",			/* 12 - ENOMEM */
	"Permission denied",			/* 13 - EACCES */
	"Bad address",				/* 14 - EFAULT */
	"Block device required",		/* 15 - ENOTBLK */
	"Mount device busy",			/* 16 - EBUSY */
	"File exists",				/* 17 - EEXIST */
	"Cross-device link",			/* 18 - EXDEV */
	"No such device",			/* 19 - ENODEV */
	"Not a directory",			/* 20 - ENOTDIR */
	"Is a directory",			/* 21 - EISDIR */
	"Invalid argument",			/* 22 - EINVAL */
	"File table overflow",			/* 23 - ENFILE */
	"Too many open files",			/* 24 - EMFILE */
	"Not a typewriter",			/* 25 - ENOTTY */
	"Text file busy",			/* 26 - ETXTBSY */
	"File too large",			/* 27 - EFBIG */
	"No space left on device",		/* 28 - ENOSPC */
	"Illegal seek",				/* 29 - ESPIPE */
	"Read-only file system",		/* 30 - EROFS */
	"Too many links",			/* 31 - EMLINK */
	"Broken pipe",				/* 32 - EPIPE */

/* math software */
	"Argument too large",			/* 33 - EDOM */
	"Result too large",			/* 34 - ERANGE */

/* non-blocking and interrupt i/o */
	"Operation would block",		/* 35 - EWOULDBLOCK */
	"Operation now in progress",		/* 36 - EINPROGRESS */
	"Operation already in progress",	/* 37 - EALREADY */

/* ipc/network software */

	/* argument errors */
	"Socket operation on non-socket",	/* 38 - ENOTSOCK */
	"Destination address required",		/* 39 - EDESTADDRREQ */
	"Message too long",			/* 40 - EMSGSIZE */
	"Protocol wrong type for socket",	/* 41 - EPROTOTYPE */
	"Protocol not available",		/* 42 - ENOPROTOOPT */
	"Protocol not supported",		/* 43 - EPROTONOSUPPORT */
	"Socket type not supported",		/* 44 - ESOCKTNOSUPPORT */
	"Operation not supported on socket",	/* 45 - EOPNOTSUPP */
	"Protocol family not supported",	/* 46 - EPFNOSUPPORT */
	"Address family not supported by protocol family",
						/* 47 - EAFNOSUPPORT */
	"Address already in use",		/* 48 - EADDRINUSE */
	"Can't assign requested address",	/* 49 - EADDRNOTAVAIL */

	/* operational errors */
	"Network is down",			/* 50 - ENETDOWN */
	"Network is unreachable",		/* 51 - ENETUNREACH */
	"Network dropped connection on reset",	/* 52 - ENETRESET */
	"Software caused connection abort",	/* 53 - ECONNABORTED */
	"Connection reset by peer",		/* 54 - ECONNRESET */
	"No buffer space available",		/* 55 - ENOBUFS */
	"Socket is already connected",		/* 56 - EISCONN */
	"Socket is not connected",		/* 57 - ENOTCONN */
	"Can't send after socket shutdown",	/* 58 - ESHUTDOWN */
	"Too many references: can't splice",	/* 59 - ETOOMANYREFS */
	"Connection timed out",			/* 60 - ETIMEDOUT */
	"Connection refused",			/* 61 - EREFUSED */
	"Too many levels of symbolic links",	/* 62 - ELOOP */
	"File name too long",			/* 63 - ENAMETOOLONG */
	"Host is down",				/* 64 - EHOSTDOWN */
	"Host is unreachable",			/* 65 - EHOSTUNREACH */
	"Directory not empty",			/* 66 - ENOTEMPTY */
	"Too many processes",			/* 67 - EPROCLIM */
	"Too many users",			/* 68 - EUSERS */
	"Disc quota exceeded",			/* 69 - EDQUOT */
	"Stale NFS file handle",		/* 70 - ESTALE */
	"Too many levels of remote in path",	/* 71 - EREMOTE */
};

int	sys_nerr = { sizeof sys_errlist/sizeof sys_errlist[0] };

/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strcasecmp.c	5.6 (Berkeley) 6/27/88";
#endif /* LIBC_SCCS and not lint */

#include <s_types.h>

/*
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static u_char charmap[] = {
	'\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
	'\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
	'\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
	'\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
	'\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
	'\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
	'\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
	'\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
	'\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
	'\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

strcasecmp(s1, s2)
	char *s1, *s2;
{
	register u_char	*cm = charmap,
			*us1 = (u_char *)s1,
			*us2 = (u_char *)s2;

	while (cm[*us1] == cm[*us2++])
		if (*us1++ == '\0')
			return(0);
	return(cm[*us1] - cm[*--us2]);
}

strncasecmp(s1, s2, n)
	char *s1, *s2;
	register int n;
{
	register u_char	*cm = charmap,
			*us1 = (u_char *)s1,
			*us2 = (u_char *)s2;

	while (--n >= 0 && cm[*us1] == cm[*us2++])
		if (*us1++ == '\0')
			return(0);
	return(n < 0 ? 0 : cm[*us1] - cm[*--us2]);
}
