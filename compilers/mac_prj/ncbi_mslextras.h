#ifndef NCBI_MSLEXTRAS__H
#define NCBI_MSLEXTRAS__H

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Robert Smith
 *
 * File Description:
 *
 *  Declarations needed by Metrowerks Codewarrior on Mac OSX
 *  when compiling with the Metrowerks Standard Library (MSL) headers
 *  as opposed to the BSD (Apple provided) headers found in /usr/include. 
 *
 *  The following declarations are needed by the C++ toolkit but 
 *  not available in MSL.  They were copied from the indicated
 *  BSD headers in /usr/include.  Since CW links with the BSD libs
 *  after MSL this works.
 *
 */

/* ============================================================
 * from sys/signal.h 
 */
 
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGXCPU	24	/* exceeded CPU time limit */

struct	sigaction {
#if defined(__cplusplus)
	void	(*sa_handler)(int);	/* signal handler */
#else
	void	(*sa_handler)();	/* signal handler */
#endif /* __cplusplus */
	sigset_t sa_mask;		/* signal mask to apply */
	int	sa_flags;		/* see signal options below */
};

/* ============================================================
 * from signal.h 
 */
int     sigaction (int, const struct sigaction *, struct sigaction *);

/* ============================================================
 * from netdb.h 
 */
 
/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
extern int h_errno;

struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritative Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */

struct hostent	*gethostbyaddr (const char *, int, int);
struct hostent	*gethostbyname (const char *);


/* ============================================================
 * from arpa/inet.h 
 */
unsigned long	 inet_addr (const char *);

/* ============================================================
 * from string.h 
 */
 
void    bzero (void *, size_t);

/* ============================================================
 * from stdio.h 
 */
 
/* 
   I am nervous about these, particularly fileno(), since BSD and MSL have 
   different implementations of stdio.
*/
 
#include <stdio.h>
char    *tempnam (const char *, const char *);
int      fileno (FILE *);

#endif /* NCBI_MSLEXTRAS__H */
