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
 *  SocketsInternal.h -- Internal structures, globals and function prototypes 
 *
 *  NOTE: Under no circumstances should your application reference anything
 *        in this file.  These are all internally referenced only!
 *
 */

#ifndef _SOCKETS_INTERNAL_
#define _SOCKETS_INTERNAL_

/* External Types, Function Prototypes and Definitions */
#include "s_socket.h"
#include "sock_ext.h"
#include "IdleInternal.h"


/* The constant that sets the maximum number of sockets that can be 
   open simultaneously.  This value is static in order to support 
   the select() API routine.  Specifically, the fd_set type 
   requires the maximum value of a socket be fixed because fd_sets 
   have no disposal function and cannot be dynamically sized */
#define kNumSockets				FD_SETSIZE


/* for conversion functions */
#define INET_SUCCESS 1
#define INET_FAILURE 0

/* Structures to hold sockets */
typedef struct Socket {
  EndpointRef ref;				/* The Open Transport reference number				*/

  Boolean     peerValid;        /* true if the "peer" field is valid                */
  struct sockaddr_in peer;		/* Place to store the remote address if SOCK_DGRAM	*/
  
  int         type;				/* Socket type (eg: SOCK_STREAM, SOCK_DGRAM)		*/
  Boolean     isListen;         /* true if endpoint is tcp,tilisten endpoint		*/
  Boolean     inProgress;		/* true if there is an OT operation in progress		*/
  Boolean     writeClosed;		/* true if we have sent the TCP close request		*/
  Boolean     readClosed;		/* true if we have received the TCP close request	*/
} Socket, *SocketPtr;

/* Internal global variables */
extern SocketPtr             *gSockets;
extern InetSvcRef             gInternetServicesRef;

// Function Prototypes in OTSocketInternals.c
OSStatus    _HandleOTLook(int sockFD, OSStatus *theError);
OSStatus    _HandleOTErrors(OSStatus *theError);
OSErr       _PrepareForSocketCall(int sockFD, OTResult *sockState);
int         _CleanUpAfterSocketCall(int sockFD, OSStatus theError, int okayVal, int errorVal);
pascal void _SocketNotifyProc(void *, OTEventCode code, OTResult result, void *cookie);
pascal void _DNSNotifyProc(void *, OTEventCode code, OTResult result, void *cookie);
OSStatus    _DNSOpenInternetServices(void);
void        _InetAddress2sockaddr(struct sockaddr *addr, InetAddress inetAddr);
void        _sockaddr2InetAddress(InetAddress *inetAddr, struct sockaddr addr);
OSStatus	_OTInetGetInterfaceInfo(InetInterfaceInfo *info, SInt32 index);

// functions in OTErrno.c
void		ncbi_ClearErrno( void);
OSErr		ncbi_GetErrno( OSStatus err);
void		ncbi_SetErrno( OSStatus err);
void		ncbi_SetOTErrno( OSStatus err);
OSErr		ncbi_OTErrorToSocketError( OSStatus err);

// functions in OTnetdb.c
OSStatus	ot_OpenDriver( void);
OSStatus	ot_DNRInit(void);
OSStatus	ot_DNRClose( InetSvcRef theRef);
void		herror(char *s);
char		*herror_str(int theErr);


#endif _SOCKETS_INTERNAL_
