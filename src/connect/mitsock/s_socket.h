/* $Copyright:
 *
 * Copyright © 1998-1999 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Furthermore if you modify
 * this software you must label your software as modified software and not
 * distribute it in such a fashion that it might be confused with the
 * original MIT software. M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without prior
 * written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the MIT
 * trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * $
 */

/* $Header$ */

/* 
 *
 * Sockets.h -- Main external header file for the sockets library.
 *
 */

#include <unix.h>
#include <MacTypes.h>
#include <Events.h>
#include <OpenTptInternet.h>
#include "neti_in.h"
#include "s_types.h"

#ifndef _SOCKETS_
#define _SOCKETS_

#ifdef __cplusplus
extern "C" {
#endif


/*******************/
/* API Definitions */
/*******************/

#define FD_SETSIZE		256    	/*	The maximum # of sockets -- cannot be changed	*/

/* socket types need to match PROTOCOLS in neti_in.h  */
#define SOCK_STREAM		1		/*	stream socket -- connection oriented 			*/
#define SOCK_DGRAM		2		/*	datagram socket -- connectionless				*/
#define SOCK_RAW		3		/*	raw socket 										*/
#define SOCK_RDM		4		/*	reliably delivered message socket				*/
#define SOCK_SEQPACKET	5		/*	sequenced packet socket							*/

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG		0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER		0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */

/*
 * Structure used for manipulating linger option.
 */
struct	linger {
	long	l_onoff;		/* option on/off */
	long	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * address families  
 */
#define AF_UNSPEC		0		//	Unspecified
#define AF_UNIX			1		//	Unix internal protocols
#define AF_INET			2		//  also in OpenTransportProviders.h
#define	AF_IMPLINK		3		// arpanet imp addresses
#define	AF_PUP			4		// pup protocols: e.g. BSP
#define	AF_CHAOS		5		// mit CHAOS protocols
#define	AF_NS			6		// XEROX NS protocols
#define	AF_ISO			7		// ISO protocols
#define	AF_OSI			AF_ISO
#define	AF_ECMA			8		// european computer manufacturers
#define	AF_DATAKIT		9		// datakit protocols
#define	AF_CCITT		10		// CCITT protocols, X.25 etc
#define	AF_SNA			11		// IBM SNA
#define AF_DECnet		12		// DECnet
#define AF_DLI			13		// DEC Direct data link interface
#define AF_LAT			14		// LAT
#define	AF_HYLINK		15		// NSC Hyperchannel
#define	AF_APPLETALK	16		// Apple Talk
#define	AF_ROUTE		17		// Internal Routing Protocol
#define	AF_LINK			18		// Link layer interface
#define	pseudo_AF_XTP	19		// eXpress Transfer Protocol (no AF)

/* protocol families */
#define PF_UNSPEC		AF_UNSPEC	//	Unspecified
#define PF_UNIX			AF_UNIX		//	Unix internal protocols
#define PF_INET			AF_INET		//	Internet protocols
#define	PF_IMPLINK		AF_IMPLINK
#define	PF_PUP			AF_PUP
#define	PF_CHAOS		AF_CHAOS
#define	PF_NS			AF_NS
#define	PF_ISO			AF_ISO
#define	PF_OSI			AF_ISO
#define	PF_ECMA			AF_ECMA
#define	PF_DATAKIT		AF_DATAKIT
#define	PF_CCITT		AF_CCITT
#define	PF_SNA			AF_SNA
#define PF_DECnet		AF_DECnet
#define PF_DLI			AF_DLI
#define PF_LAT			AF_LAT
#define	PF_HYLINK		AF_HYLINK
#define	PF_APPLETALK	AF_APPLETALK
#define	PF_ROUTE		AF_ROUTE
#define	PF_LINK			AF_LINK
#define	PF_XTP			pseudo_AF_XTP	/* really just proto family, no AF */

/* IP Address Wildcard */
#ifndef	INADDR_ANY	// already in neti_in.h 
#define INADDR_ANY      kOTAnyInetAddress
#endif
#define INADDR_NONE		0xffffffff

/* recv and send flags */
#define MSG_DONTROUTE	1
#define MSG_DONTWAIT	2
#define MSG_OOB			4
#define MSG_PEEK		8
#define MSG_WAITALL		16

/* s_fnctl() requests */
#define F_GETFL			3		/* Get file flags 									*/
#define F_SETFL			4		/* Set file flags 									*/

/* shutdown() flags */
#define SHUT_RD			0		/* Shutdown read side of the connection				*/
#define SHUT_WR			1		/* Shutdown write side of the connection			*/
#define SHUT_RDWR		2		/* Shutdown read and write sides of the connection	*/

/* IP address sizes */
#define INET_ADDRSTRLEN		16	/* for IPv4 dotted decimal							*/
#define INET6_ADDRSTRLEN	46	/* for IPv6 dhex string								*/
#define INADDRSZ			4	/* size of IPv4 addr in bytes 						*/
#define IN6ADDRSZ			16	/* size of IPv6 addr in bytes						*/

/* host name size */
#define MAXHOSTNAMESIZE		kMaxHostNameLen
#define MAXHOSTNAMELEN		kMaxHostNameLen


/****************************/
/* API Structures and Types */
/****************************/

/* An internet address */
typedef UInt32  in_addr_t;

/* structure used to store addresses */
struct sockaddr {
	UInt16	sa_family;
	char	sa_data[14];
};

/* INET protocol structures */
struct in_addr {
	in_addr_t	s_addr;
};

/* A TCP address -- the same as a OT InetAddress */
struct sockaddr_in {				/* struct InetAddress {				*/
	UInt16			sin_family;		/* OTAddressType	fAddressType	*/
	UInt16			sin_port;		/* InetPort			fPort			*/
	struct in_addr 	sin_addr;		/* InetHost			fHost			*/
	char 			sin_zero[8];	/* UInt8			fUnused			*/
};									/* };								*/

/* Structure for non-contiguous data */
struct iovec {
  struct iovec *next;			/* For compatibility with Open Transport */
  void         *iov_base;		/* Starting address of buffer */
  size_t        iov_len;		/* size of buffer */
};

/* For poll() */
struct pollfd {
  int   fd;
  short events;
  short revents;
};

/*
 * Internal Function Prototypes 
 */

#define SocketsLibIsPresent_ ((Ptr) socket != (Ptr) kUnresolvedCFragSymbolAddress)

/* Sockets Control API calls */
OSStatus 			   AbortSocketOperation(int sockFD);
OSStatus 			   AbortAllDNSOperations(void);
Boolean                IsValidSocket(int sockFD);


#ifdef __cplusplus
}
#endif

#endif /* _SOCKETS_ */