/*
 * Copyright (c) 1982 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)types.h	7.11 (Berkeley) 6/25/90
*
*
* RCS Modification History:
* $Log$
* Revision 1.1  2001/04/03 20:35:37  juran
* Phil Churchill's MIT-derived OT sockets library.  No changes prior to initial check-in.
*
* Revision 6.0  1997/08/25 18:37:57  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.3  1995/05/17  17:57:43  epstein
 * add RCS log revision history
 *
 */

#ifndef _TYPES_
#define	_TYPES_

#if !defined(_NCBI_) && !defined(Int4)
#define Int4 long
#endif
 
//typedef	short	dev_t;
#ifndef _POSIX_SOURCE
					/* major part of a device */
#define	major(x)	((int)(((unsigned)(x)>>8)&0377))
					/* minor part of a device */
#define	minor(x)	((int)((x)&0377))
					/* make a device number */
#define	makedev(x,y)	((dev_t)(((x)<<8) | (y)))
#endif

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef	unsigned short	ushort;		/* Sys V compatibility */

#ifdef KERNEL
#include "machine/machtypes.h"
#else
//#include <ma_types.h>
#endif

#ifdef	_CLOCK_T_
typedef	_CLOCK_T_	clock_t;
#undef	_CLOCK_T_
#endif

#ifdef	_TIME_T_
typedef	_TIME_T_	time_t;
#undef	_TIME_T_
#endif

#ifdef	_SIZE_T_
#if !defined(THINK_C) && !defined(COMP_THINKC) && !defined(__MWERKS__) && !defined(COMP_METRO)
typedef	_SIZE_T_	size_t;
#endif
#undef	_SIZE_T_
#endif

#ifndef _POSIX_SOURCE
typedef	struct	_uquad { unsigned long val[2]; } u_quad;
typedef	struct	_quad { long val[2]; } quad;
#endif
typedef	long *	qaddr_t;	/* should be typedef quad * qaddr_t; */

typedef	long	daddr_t;
typedef	char *	caddr_t;
//typedef	u_long	ino_t;
typedef	long	swblk_t;
typedef	long	segsz_t;
//typedef	long	off_t;
//typedef	u_short	uid_t;
//typedef	u_short	gid_t;
typedef	short	pid_t;
/* size of address structures */
typedef UInt32 socklen_t;

//typedef	u_short	nlink_t;
//typedef	u_short	mode_t;
typedef u_long	fixpt_t;

#ifndef _POSIX_SOURCE
#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be >= NOFILE (param.h).
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	256
#endif

//typedef long	fd_mask;
//#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */
#define NUMBITSPERBYTE	8		/* 	Number of bits per byte			*/


#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

/* structures for select */
typedef long fd_mask;
#define NFDBITS			(sizeof(fd_mask) * NUMBITSPERBYTE)      /* bits per mask */

typedef struct fd_set {
  fd_mask fds_bits[(FD_SETSIZE + NFDBITS - 1) / NFDBITS];
} fd_set;

/* macros for select */
#define FD_SET(fd, fdset)	((fdset)->fds_bits[(fd) / NFDBITS] |= ((unsigned)1 << ((fd) % NFDBITS)))
#define FD_CLR(fd, fdset)	((fdset)->fds_bits[(fd) / NFDBITS] &= ~((unsigned)1 << ((fd) % NFDBITS)))
#define FD_ISSET(fd, fdset)	((fdset)->fds_bits[(fd) / NFDBITS] & ((unsigned)1 << ((fd) % NFDBITS)))
#define FD_ZERO(fdset)		memset((char *)(fdset), 0, sizeof(*(fdset)))


#endif /* !_POSIX_SOURCE */
#endif /* _TYPES_ */
