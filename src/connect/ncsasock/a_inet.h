/*
 * Copyright (c) 1983 Regents of the University of California.
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
 *	@(#)inet.h	5.4 (Berkeley) 6/1/90
*
*
* RCS Modification History:
* $Log$
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.2  1995/05/17  17:56:29  epstein
 * add RCS log revision history
 *
 */

/* External definitions for functions in inet(3) */

#ifdef __STDC__
extern unsigned long inet_addr(const char *);
extern char *inet_ntoa(struct in_addr);
extern struct in_addr inet_makeaddr(int , int);
extern unsigned long inet_network(const char *);
extern unsigned long inet_lnaof(struct in_addr);
extern unsigned long inet_netof(struct in_addr);
#else
extern unsigned long inet_addr();
extern char *inet_ntoa();
extern struct in_addr inet_makeaddr();
extern unsigned long inet_network();
extern unsigned long inet_lnaof();
extern unsigned long inet_netof();
#endif
