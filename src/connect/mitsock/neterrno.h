/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)errno.h	7.10 (Berkeley) 6/28/90
*
*
* RCS Modification History:
* $Log$
* Revision 1.2  2001/11/07 22:35:41  juran
* Avoid redefinition of EDEADLK and EAGAIN on Mac OS X.
*
* Revision 1.1  2001/04/03 20:35:21  juran
* Phil Churchill's MIT-derived OT sockets library.  No changes prior to initial check-in.
*
* Revision 6.2  2000/11/01 18:56:00  kans
* remove colliding symbols for CodeWarrior 6
*
* Revision 6.1  1997/10/19 23:15:59  kans
* latest CodeWarrior needs EACCES defined here
*
* Revision 6.0  1997/08/25 18:37:34  madden
* Revision changed to 6.0
*
* Revision 4.1  1997/01/28 22:35:23  kans
* new symbol collisions in CodeWarrior fixed
*
 * Revision 4.0  1995/07/26  13:56:09  ostell
 * force revision to 4.0
 *
 * Revision 1.3  1995/05/17  17:56:54  epstein
 * add RCS log revision history
 *
 */

#ifndef _NETERRNO_
#define _NETERRNO_

#ifndef KERNEL
extern int errno;			/* global error number */
extern long errno_long;     /* same as errno, but of known length (for variable length ints) */
#endif

#include <errno.h>

#ifdef __MWERKS__
#define	EPERM		1		/* Operation not permitted */
/*#define	ENOENT		2*/		/* No such file or directory */
#define	ESRCH		3		/* No such process */
#define	EINTR		4		/* Interrupted system call */
/*#define	EIO		5*/		/* Input/output error */
#endif /* __MWERKS__ */

#ifndef ENXIO
#define	ENXIO		6		/* Device not configured */
#endif /* ENXIO */

#ifdef __MWERKS__
#define	E2BIG		7		/* Argument list too long */
#define	ENOEXEC		8		/* Exec format error */
/*#define	EBADF		9*/		/* Bad file descriptor */
#define	ECHILD		10		/* No child processes */
#ifndef EDEADLK
#define	EDEADLK		11		/* Resource deadlock avoided */
					/* 11 was EAGAIN */
#endif /* EDEADLK */
#endif /* __MWERKS__ */

#ifndef ENOMEM
#define	ENOMEM		12		/* Cannot allocate memory */
#endif /* ENOMEM */

#ifdef __MWERKS__
/*#define	EACCES		13*/		/* Permission denied */
#endif /* __MWERKS__ */

#ifndef EFAULT
#define	EFAULT		14		/* Bad address */
#endif /* EFAULT */

#ifdef __MWERKS__
#ifndef _POSIX_SOURCE
#define	ENOTBLK		15		/* Block device required */
#define	EBUSY		16		/* Device busy */
#endif
#define	EEXIST		17		/* File exists */
#define	EXDEV		18		/* Cross-device link */
#define	ENODEV		19		/* Operation not supported by device */
#define	ENOTDIR		20		/* Not a directory */
#define	EISDIR		21		/* Is a directory */
/*#define	EINVAL		22*/		/* Invalid argument */
/*#define	ENFILE		23*/		/* Too many open files in system */
/*#define	EMFILE		24*/		/* Too many open files */
#define	ENOTTY		25		/* Inappropriate ioctl for device */
#ifndef _POSIX_SOURCE
#define	ETXTBSY		26		/* Text file busy */
#endif
#define	EFBIG		27		/* File too large */
/*#define	ENOSPC		28*/		/* No space left on device */
#define	ESPIPE		29		/* Illegal seek */
#define	EROFS		30		/* Read-only file system */
#define	EMLINK		31		/* Too many links */
#define	EPIPE		32		/* Broken pipe */

#endif /* __MWERKS__ */

#ifdef NOWAY
/* math software */
#define	EDOM		33		/* Numerical argument out of domain */
#define	ERANGE		34		/* Numerical result out of range */
#endif /* NOWAY */

/* non-blocking and interrupt i/o */
#ifndef EAGAIN
#define	EAGAIN		35		/* Resource temporarily unavailable */
#endif /* EAGAIN */
#ifndef _POSIX_SOURCE
#define	EWOULDBLOCK	EAGAIN		/* Operation would block */
#define	EINPROGRESS	36		/* Operation now in progress */
#define	EALREADY	37		/* Operation already in progress */

/* ipc/network software -- argument errors */
#define	ENOTSOCK	38		/* Socket operation on non-socket */
#define	EDESTADDRREQ	39		/* Destination address required */
#define	EMSGSIZE	40		/* Message too long */
#define	EPROTOTYPE	41		/* Protocol wrong type for socket */
#define	ENOPROTOOPT	42		/* Protocol not available */
#define	EPROTONOSUPPORT	43		/* Protocol not supported */
#define	ESOCKTNOSUPPORT	44		/* Socket type not supported */
#define	EOPNOTSUPP	45		/* Operation not supported on socket */
#define	EPFNOSUPPORT	46		/* Protocol family not supported */
#define	EAFNOSUPPORT	47		/* Address family not supported by protocol family */
#define	EADDRINUSE	48		/* Address already in use */
#define	EADDRNOTAVAIL	49		/* Can't assign requested address */

/* ipc/network software -- operational errors */
#define	ENETDOWN	50		/* Network is down */
#define	ENETUNREACH	51		/* Network is unreachable */
#define	ENETRESET	52		/* Network dropped connection on reset */
#define	ECONNABORTED	53		/* Software caused connection abort */
#define	ECONNRESET	54		/* Connection reset by peer */
#define	ENOBUFS		55		/* No buffer space available */
#define	EISCONN		56		/* Socket is already connected */
#define	ENOTCONN	57		/* Socket is not connected */
#define	ESHUTDOWN	58		/* Can't send after socket shutdown */
#define	ETOOMANYREFS	59		/* Too many references: can't splice */
#define	ETIMEDOUT	60		/* Connection timed out */
#define	ECONNREFUSED	61		/* Connection refused */

#define	ELOOP		62		/* Too many levels of symbolic links */
#endif /* _POSIX_SOURCE */
#/*define	ENAMETOOLONG	63*/		/* File name too long */

/* should be rearranged */
#ifndef _POSIX_SOURCE
#define	EHOSTDOWN	64		/* Host is down */
#define	EHOSTUNREACH	65		/* No route to host */
#endif /* _POSIX_SOURCE */
/*#define	ENOTEMPTY	66*/		/* Directory not empty */

/* quotas & mush */
#ifndef _POSIX_SOURCE
#define	EPROCLIM	67		/* Too many processes */
#define	EUSERS		68		/* Too many users */
#define	EDQUOT		69		/* Disc quota exceeded */

/* Network File System */
#define	ESTALE		70		/* Stale NFS file handle */
#define	EREMOTE		71		/* Too many levels of remote in path */
#define	EBADRPC		72		/* RPC struct is bad */
#define	ERPCMISMATCH	73		/* RPC version wrong */
#define	EPROGUNAVAIL	74		/* RPC prog. not avail */
#define	EPROGMISMATCH	75		/* Program version wrong */
#define	EPROCUNAVAIL	76		/* Bad procedure for program */
#endif /* _POSIX_SOURCE */

#define	ENOLCK		77		/* No locks available */
#ifdef ENOSYS
#undef ENOSYS
#endif
#define	ENOSYS		78		/* Function not implemented */

#ifdef KERNEL
/* pseudo-errors returned inside kernel to modify return to process */
#define	ERESTART	-1		/* restart syscall */
#define	EJUSTRETURN	-2		/* don't modify regs, just return */
#endif

#endif /* defined _NETERRNO_ */

