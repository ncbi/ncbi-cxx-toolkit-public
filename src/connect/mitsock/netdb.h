/*-
 * Copyright (c) 1980, 1983, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)netdb.h	5.11 (Berkeley) 5/21/90
*
*
* RCS Modification History:
* $Log$
* Revision 1.1  2001/04/03 20:35:19  juran
* Phil Churchill's MIT-derived OT sockets library.  No changes prior to initial check-in.
*
* Revision 6.2  2000/03/20 21:49:05  kans
* initial work on OpenTransport (Churchill)
*
* Revision 6.1  1999/11/17 20:52:50  kans
* changes to allow compilation under c++
*
* Revision 6.0  1997/08/25 18:37:33  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.2  1995/05/17  17:56:51  epstein
 * add RCS log revision history
 *
 */

/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct	hostent {
	char	*h_name;		/* official name of host */
	char	**h_aliases;	/* alias list */
	SInt32	h_addrtype;		/* host address type */
	SInt32	h_length;		/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
};

#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */


/*
 * Assumption here is that a network number
 * fits in 32 bits -- will break with IP6.
 */
struct	netent {
	char		*n_name;		/* official name of net */
	char		**n_aliases;	/* alias list */
	SInt32		n_addrtype;		/* net address type */
	unsigned long	n_net;		/* network # */
};

struct	servent {
	char	*s_name;		/* official service name */
	char	**s_aliases;	/* alias list */
	SInt32	s_port;			/* port # */
	char	*s_proto;		/* protocol to use */
};

struct	protoent {
	char	*p_name;		/* official protocol name */
	char	**p_aliases;	/* alias list */
	SInt32	p_proto;		/* protocol # */
};

/*
 *	prototypes
 */
struct hostent	*gethostbyname(const char *name);
struct hostent	*gethostbyaddr();
struct servent	*getservbyname(const char *name, const char *proto);
struct servent	*getservbyport();
struct servent	*getservent();
unsigned long   gethostid(void);

/*  
 *	not sure they belong here, but need to be somewhere...
 */
int gethostname( char* machname, size_t buflen);
void bzero( char *b, long s);

/*
 *	not implemented in version ncbiOTsock 1.0
 */
struct protoent	*getprotobyname();
struct protoent	*getprotobynumber();
struct protoent	*getprotoent();
struct hostent	*gethostent();
struct netent	*getnetbyname();
struct netent	*getnetbyaddr();
struct netent	*getnetent();

