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
 * OTSockets.c -- external functions for manipulating STREAM and DGRAM sockets.
 *
 * The prototypes and behavior of these calls is based on the Berkeley sockets API 
 * described in _Unix Network Programming_ by Richard Stevens.
 *
 */
 
#include <errno.h>
#include <stdlib.h>				// malloc, free
#include <string.h>				// memcpy, memset

#include <neterrno.h>
#include "SocketsInternal.h"

static OTNotifyUPP gSocketNotifyUPP = NULL;

/*
 * socket() - creates an endpoint for communication and returns a descriptor
 */
int socket(int family, int type, int protocol)
{
  int              sockFD = -1;
  OSStatus         theError = noErr;
  SocketPtr        theSocket;
  OTConfigurationRef config;
  TEndpointInfo    info;
  
  /* We only support IPv4 sockets so far */
  if(family != AF_INET)
  	{
  	  theError = EAFNOSUPPORT;
  	  goto abort;
  	}
  
  /* the protocol should be PF_UNSPEC, but PF_INET is okay */
  if(protocol != PF_INET && protocol != PF_UNSPEC)
  	{
  	  theError = EPFNOSUPPORT;
  	  goto abort;
  	}
  	
  /* We only support TCP and UDP so far */
  if(type != SOCK_STREAM && type != SOCK_DGRAM)
    {
      theError = EPROTONOSUPPORT;
      goto abort;
    }

  /* make sure that the array has been initialized */  
  if( gSockets == NULL){
  	theError = ot_OpenDriver();
  	if( theError != kOTNoError)
  		goto abort;
  }
  
  /* find an empty slot in the socket array to put the socket in */
  sockFD = 0;
  while(gSockets[sockFD] != NULL && sockFD < kNumSockets)
    sockFD++;
    
  if(sockFD >= kNumSockets)
    {
      /* too many sockets are already open */
      theError = EMFILE;
      goto abort;
    }
  
  /* allocate space for the socket structure */
  theSocket = (SocketPtr) malloc(sizeof(Socket));
  if(theSocket == NULL)
    {
      /* Out of memory */
      theError = ENOMEM;
      goto abort;
    }
  gSockets[sockFD] = theSocket;
  
  /* initialize socket state */
  theSocket->inProgress = true;
  theSocket->writeClosed = true;
  theSocket->readClosed = true;
  theSocket->type = type;
  theSocket->peerValid = false;
  
  /* create the appropriate configuration for the endpoint */
  if(theSocket->type == SOCK_STREAM)
    config = OTCreateConfiguration(kTCPName);
  else
    config = OTCreateConfiguration(kUDPName);
#if TARGET_API_MAC_CARBON
  theSocket->ref = OTOpenEndpointInContext(config, 0, &info, &theError, NULL);
#else
  theSocket->ref = OTOpenEndpoint(config, 0, &info, &theError);
#endif
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;
    
  /* the socket is unbound, bind it because we might not call connect (if udp or tcp with no bind) */
  theError = OTBind(gSockets[sockFD]->ref, nil, nil);
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;

  /* All endpoints are syncronous */
  theError = OTSetSynchronous(theSocket->ref);
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;
    
  /* For now we assume the endpoint is blocking */
  theError = OTSetBlocking(theSocket->ref);
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;

  /* Install our notifier function to get idle events */
  if (gSocketNotifyUPP == NULL) {
    gSocketNotifyUPP = NewOTNotifyUPP(_SocketNotifyProc);
  }
  theError = OTInstallNotifier(theSocket->ref, gSocketNotifyUPP, nil);
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;

  /* Tell OT we want idle events */
  theError = OTUseSyncIdleEvents(theSocket->ref, true);
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;

abort:
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, sockFD, -1));
}


/*
 * bind() - bind a local name and port to a socket
 */
int bind(int sockFD, const struct sockaddr *myAddr, int addrLength)
{
  OSStatus          theError;
  TBind             reqAddr, retAddr;
  InetAddress       inetAddr, retInetAddr;
  OTResult          sockState;

  /* Must be AF_INET */
  if(addrLength != sizeof(struct sockaddr_in))
    {
    	theError = EINVAL;
    	goto abort;
    }
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	    break; /* okay */
	  case T_IDLE:   
	      /* Need to unbind before bind will succeed */
	      do {
            _HandleOTLook(sockFD, &theError);
            theError = OTUnbind(gSockets[sockFD]->ref);
          } while(theError == kOTLookErr);
	      if(_HandleOTErrors(&theError) != noErr)
	        goto abort;
	      break;
	  case T_OUTCON:
	  case T_INCON:
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	   theError = EISCONN;
	   goto abort;
    }
  
  /* convert to an INET address structure */
  _sockaddr2InetAddress(&inetAddr, *myAddr);
  
  /* now set up reqAddr */
  reqAddr.addr.maxlen = sizeof(InetAddress);
  reqAddr.addr.len    = sizeof(InetAddress);
  reqAddr.addr.buf    = (UInt8 *) &inetAddr;
  reqAddr.qlen = 0;
  
  /* now set up retAddr.  OTBind will try to stash stuff here, so we should
     make sure all the lengths and buffers point to our memory. */
  retAddr.addr.maxlen = sizeof(InetAddress);
  retAddr.addr.len    = sizeof(InetAddress);
  retAddr.addr.buf    = (UInt8 *) &retInetAddr;
  retAddr.qlen = 0;  
    
  /* Bind the endpoint. */
  theError = OTBind(gSockets[sockFD]->ref, &reqAddr, &retAddr);
  _HandleOTLook(sockFD, &theError);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;
  
  /* If we asked for a specific address */
  /* Check to make sure we got the addresses we asked for */
  memcpy(&retInetAddr, retAddr.addr.buf, sizeof(InetAddress));
      
  if((inetAddr.fPort != 0) && (retInetAddr.fPort != inetAddr.fPort))
    {    
      theError = EADDRINUSE;
      goto abort;
    }
        
  if((inetAddr.fHost != INADDR_ANY) && (retInetAddr.fHost != inetAddr.fHost))
    {  
      theError = EADDRINUSE;
      goto abort;
    }
    
abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}



/*
 * fcntl() - Set socket flags (specific to sockets)
 *           Technically fcntl should use stdarg, but we cheat.
 */
int fcntl(int sockFD, int command, int flags)
{
  int      newFlags = 0;
  OSStatus theError = noErr;
  OTResult sockState;
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
      case T_IDLE:   
	  case T_OUTCON:
	  case T_INCON:
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	   break;
    }
  
  switch(command)
    {
    case F_GETFL:
      /* return whether the socket is nonblocking */
      if(OTIsNonBlocking(gSockets[sockFD]->ref))
        newFlags |= kO_NONBLOCK;
        
      /* Should add O_ASYNC support here */
      break;
      
	case F_SETFL:
		/* set whether the socket is non-blocking */
		if(flags & kO_NONBLOCK){
			OTSetNonBlocking(gSockets[sockFD]->ref);
		}
		else{
			OTSetBlocking(gSockets[sockFD]->ref);
		}

		/* Should add O_ASYNC support here */
		break;
      
    default:
      /* invalid command */
      theError = EINVAL;
      break;
    }
    
abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, newFlags, -1));
}


/*
 *	setsockopt()
 *
 *		Set socket options. None implemented. In Unix there are...
 *
 *          SO_REUSEADDR        toggle local address reuse
 *          SO_KEEPALIVE        toggle keep connections alive
 *          SO_DONTROUTE        toggle routing bypass for  outgoing
 *                              messages
 *          SO_LINGER           linger on close if data present
 *          SO_BROADCAST        toggle   permission   to   transmit
 *                              broadcast messages
 *          SO_OOBINLINE        toggle  reception  of   out-of-band
 *                              data in band
 *          SO_SNDBUF           set buffer size for output
 *          SO_RCVBUF           set buffer size for input
 *          SO_TYPE             get the type  of  the  socket  (get
 *                              only)
 *          SO_ERROR            get and clear error on  the  socket
 *                             (get only)
 */
int setsockopt(
	long sockFD,
	long level,
	long optname,
	char *optval,
	long optlen)
{
#pragma unused(optval)
#pragma unused(optlen)
	SocketPtr sp;

	sp = gSockets[sockFD];

	if ( sp != NULL) /* warn if this unimplemented function is called */
		return( EBADF);

	/*
	 * demultiplex to socket option handlers at other protocol levels.(None
	 * supported yet).
	 */
	switch(level) {
		case SOL_SOCKET: 		/* socket level option */
		case IPPROTO_UDP:
			switch(optname) 
			{
				default :
					return(0);
			}
			break;
		case IPPROTO_TCP:
			switch(optname)
			{
				default:
					return(0);
			}
			break;
		default :
			return( 0 ); // temp pjc ENOPROTOOPT);
	}
	return(0);
}


/*
 * close() - close a socket
 */
int close(int sockFD)
{
  OSStatus theError = noErr;
  OTResult sockState;
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	  case T_IDLE:
	    break;
	  case T_OUTCON:
	  case T_INCON:
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	   /* close the connection if it is open */
	   if(!gSockets[sockFD]->writeClosed)
        {
          do {
            _HandleOTLook(sockFD, &theError);
            theError = OTSndOrderlyDisconnect(gSockets[sockFD]->ref);
          } while(theError == kOTLookErr);
          if(_HandleOTErrors(&theError) != noErr)
            goto abort;
        }
	   break;
    }
  
  /* destroy the socket */
  do {
    _HandleOTLook(sockFD, &theError);
    theError = OTCloseProvider(gSockets[sockFD]->ref);
  } while(theError == kOTLookErr);
  if(_HandleOTErrors(&theError) != noErr)
    goto abort;
    
  free(gSockets[sockFD]);
  
  /* Mark the socket as freed */
  gSockets[sockFD] = NULL;

abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/*
 * shutdown() - shut down part of a full-duplex connection
 */
int shutdown(int sockFD, int howTo)
{
  OSStatus theError = noErr;
  OTResult sockState;
 
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	  case T_IDLE:
	    theError = ENOTCONN;
	    goto abort;
	  case T_OUTCON:
	  case T_INCON:
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	    break;
    }
  
  if(!gSockets[sockFD]->readClosed && (howTo == SHUT_RD || howTo == SHUT_RDWR))
    {
      /* Mark the read side of the connection as closed */
      gSockets[sockFD]->readClosed = true;
    }
  
  if(!gSockets[sockFD]->writeClosed && (howTo == SHUT_WR || howTo == SHUT_RDWR))
    {
      /* Initiate a close to close the writing side of the connection if we are TCP */
      if(gSockets[sockFD]->type == SOCK_STREAM)
        {
          theError = OTSndOrderlyDisconnect(gSockets[sockFD]->ref);
          _HandleOTLook(sockFD, &theError);
          if(_HandleOTErrors(&theError) != noErr)
            goto abort;
        }
        
      /* Mark the write end of the connection as closed */
      gSockets[sockFD]->writeClosed = true;
    }
  
abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/* 
 * connect() - initiate a connection on a socket
 */
int connect(int sockFD, struct sockaddr *servAddr, int addrLength)
{
  TCall          sndCall;
  InetAddress    inetAddr;
  OSStatus       theError = noErr;
  OTResult       sockState;
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	    /* the socket is unbound, bind it so OTConnect will succeed */
	    theError = OTBind(gSockets[sockFD]->ref, nil, nil);
	    _HandleOTLook(sockFD, &theError);
        if(_HandleOTErrors(&theError) != noErr)
          goto abort;
        break;
	  case T_IDLE:
	    if(gSockets[sockFD]->peerValid)
	      {
	        theError = EISCONN;
	        goto abort;
	      }
	    break;
	  case T_OUTCON:
	  case T_INCON:
	    theError = EALREADY;
	    goto abort;
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	    theError = EISCONN;
	    break;
    }
  
  /* Sanity check on length of name */
  if(addrLength < sizeof(struct sockaddr_in) || servAddr == NULL)
    {
      theError = EINVAL;
      goto abort;
    }
  
  switch(gSockets[sockFD]->type)
  	{
  	/* TCP socket */
  	case SOCK_STREAM:
  	  /* All sockets are AF_INET, so the sockaddr must be a InetAddress */
      _sockaddr2InetAddress(&inetAddr, *servAddr);
  
      /* Create a TCall for the InetAddr */
      OTMemzero(&sndCall, sizeof(TCall));
      sndCall.addr.maxlen = sizeof(InetAddress);
      sndCall.addr.len    = sizeof(InetAddress);
      sndCall.addr.buf    = (UInt8 *) &inetAddr;
  
      /* Connect to the remote host specified by the newly created TCall */
      theError = OTConnect(gSockets[sockFD]->ref, &sndCall, nil);
      _HandleOTLook(sockFD, &theError);
      if(theError == T_DISCONNECT){
      	theError = ECONNREFUSED;   /* If you get an RST before the connect, return this */
        goto abort;	// temp pjc
      }
      if(_HandleOTErrors(&theError) != noErr){
        goto abort;
      }
      gSockets[sockFD]->readClosed = false;
      gSockets[sockFD]->writeClosed = false;
      break;
      
    /* connected UDP socket (a convenience) */
    case SOCK_DGRAM:
      /* just remember what we want to be connected to */
      /* Connected UDP doesn't really exist in OT under Mentat code base < 3 */
      memcpy(&gSockets[sockFD]->peer, servAddr, sizeof(struct sockaddr_in));
      gSockets[sockFD]->peerValid = true;
      
      gSockets[sockFD]->readClosed = false;
      gSockets[sockFD]->writeClosed = false;
      break;

    default:
      theError = EINVAL;
      goto abort;
  	}
      
abort:
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/*
 * listen()
 *
 *		To accept connections, a socket is first created  with
 *		socket(), a backlog for incoming connections is specified with
 *		listen() and then the connections are  accepted  with
 *		accept(). 
 *
 *		The listen() call applies only to TCP sockets.
 *
 *		The backlog parameter is supposed to define the maximum length
 *		the queue of pending connections may grow to. It is ignored.
 *		
 *		A 0 return value indicates success; -1 indicates an error.
 *		
 *		EOPNOTSUPP          s is not a TCP socket.
 */
int listen(int sockFD, int backLog)
{
	OSStatus theError = noErr;
	OTResult       sockState;

	/* Sanity Checks to validate sockFD */
	if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr){
		goto abort;
	}
	if( backLog > 1){
		// note that we may want to address these requests
	}
	return EOPNOTSUPP;	// TEMP stub out this call!

abort:  
	/* Clean up after the socket operation */
	return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/*
 * accept() - accept a connection on a socket
 */
int accept(int sockFD, struct sockaddr *peer, int *addrlen)
{
	OSStatus theError = noErr;
	OTResult       sockState;

	/* Sanity Checks to validate sockFD */
	if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr){
		goto abort;
	}

	return EOPNOTSUPP;	// TEMP stub out this call!

abort:  
	/* Clean up after the socket operation */
	return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/*
 * select() - synchronous I/O multiplexing
 */
int select(int maxFDsExamined, fd_set *readFDs, fd_set *writeFDs,
                  fd_set *exceptFDs, struct timeval *timeOut)
{
  OSStatus theError = noErr;
  int      nfds = 0, retVal, sockFD;
  fd_set   retRead, retWrite, retExcept;
  unsigned long     waitTicks = 0;
  Boolean  doIncreaseRetval, waitForever = false;
  
  /* If timeOut == NULL, wait until something happens (a read event now) */
  /* else timeOut is the time to wait (where 0 means grab state once and exit) */ 
  /* Figure out when timeOut will expire. */
  if (timeOut == NULL)
    waitForever = true; 
  else 
    {
      waitForever = false;
      waitTicks = TickCount() + timeOut->tv_sec * 60 + (timeOut->tv_usec * 60) / 2000;
    }
  
  /* make sure the maxFDsExamined is in range */
  if(maxFDsExamined >= kNumSockets)
    maxFDsExamined = kNumSockets;
  
  /* Zero the return fd_set's and return value. */
  memset(&retRead, '\0', sizeof(fd_set));
  memset(&retWrite, '\0', sizeof(fd_set));
  memset(&retExcept, '\0', sizeof(fd_set));
  retVal = 0;

  do /* Wait until we get an event */
  {
    /* loop over the sockets and check them out! */
    for(sockFD = 0; sockFD <= maxFDsExamined; sockFD++)
      {
        /* Set doIncreaseRetval to 0.  Set to nonzero if should
           be increased.  Do this so we don't inc it for each type
           of event. */
        doIncreaseRetval = false;

        /* Sanity Checks to validate sockFD */
        theError = _PrepareForSocketCall(sockFD, NULL);
        if(theError == ENOTSOCK || theError == EALREADY)
          {
            /* If the socket doesn't exist or is busy, just loop. */
            theError = noErr;
            continue;
          }
        
        /* Was there a real error? */
        if (theError != noErr) goto abort;
        
        /* Does sockFD have data to be read?  Check and set retRead if it does. */  
        if (readFDs != NULL && FD_ISSET(sockFD, readFDs))
          {
            size_t numBytes;
            
            theError = OTCountDataBytes(gSockets[sockFD]->ref, &numBytes);
            _HandleOTLook(sockFD, &theError);
            if(theError != noErr && theError != kOTNoDataErr) 
              {
                _HandleOTErrors(&theError);
                goto abort;
              }
            
            if (numBytes > 0)
              {
                FD_SET(sockFD, &retRead);
                /* We know there's data pending, so we don't need to look again. */
                FD_CLR(sockFD, readFDs); 
                doIncreaseRetval = true;
              }              
          }
        
        /* If you can write to sockFD, mark it in retWrite. */
        if (writeFDs != NULL && FD_ISSET(sockFD, writeFDs))
          {
            /* Apple...SUCKS! */
            /* Apple does not provide a mechanism to determine whether an endpoint is
               writable.  So we just assume it is.  This has never failed in the past.
               If we don't squint, it won't in the future.  Maybe. */
             FD_SET(sockFD, &retWrite);
             FD_CLR(sockFD, writeFDs);
             doIncreaseRetval = true;
          }
          
        if (doIncreaseRetval) retVal++;
        _CleanUpAfterSocketCall(sockFD, theError, retVal, -1);
      }
      
    /* If any events have occurred on this pass, stop and return! */
    if (retVal != 0) break;
    
    /* If we have to keep going, call the idle proc.  
       Gives time to other threads or applications */
    theError = Idle();
    if(theError != noErr) goto abort;
    
  } while(waitForever || waitTicks >= TickCount());

  if( readFDs != NULL)
  	memmove(readFDs, &retRead, sizeof(fd_set));  
  if( writeFDs != NULL)
  	memmove(writeFDs, &retWrite, sizeof(fd_set));  
  if( exceptFDs != NULL)
  	memmove(exceptFDs, &retExcept, sizeof(fd_set));  
  
abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, retVal, -1));
}



/*
 * getpeername() - get the name of the peer connected to a socket
 */
int getpeername(int sockFD, struct sockaddr *peerAddr, int *addrLength)
{
  OSStatus    theError = noErr;
  TBind       peer;
  InetAddress peerInetAddr;
  OTResult    sockState;
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
 
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  
	  case T_UNBND:
	  case T_IDLE:
	  case T_OUTCON:
	  case T_INCON:
	    if(gSockets[sockFD]->type == SOCK_STREAM)
	      {
	        theError = ENOTCONN;
	        goto abort;
	      }
	    break;
	      
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	    break;
    }
    
  /* Sanity check on length of name */
  if(*addrLength < sizeof(struct sockaddr_in))
    {
      theError = EINVAL;
      goto abort;
    }
  
  switch(gSockets[sockFD]->type)
    {
    case SOCK_STREAM:
      /* Set up the peer structure */
      OTMemzero(&peer, sizeof(TBind));
      peer.addr.maxlen = sizeof(InetAddress);
      peer.addr.len    = sizeof(InetAddress);
      peer.addr.buf    = (UInt8 *) &peerInetAddr;
 
      /* Ask OT for the peer name */
      theError = OTGetProtAddress(gSockets[sockFD]->ref, nil, &peer);
      _HandleOTLook(sockFD, &theError);
      if(_HandleOTErrors(&theError) != noErr)
         goto abort;
         
      /* copy the port, etc, into the sockaddr */
      _InetAddress2sockaddr(peerAddr, peerInetAddr);
      break;
      
    case SOCK_DGRAM:
      /* make sure the peer is valid */
      if(!gSockets[sockFD]->peerValid)
        {
          theError = ENOTCONN;
          goto abort;
        }
      
      memcpy(peerAddr, &gSockets[sockFD]->peer, sizeof(struct sockaddr_in));
      break;
      
    default:
      theError = EINVAL;
      break;
    }
 
abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/*
 * getsockname() - get the current name of a socket
 */
int getsockname(int sockFD, struct sockaddr *localAddr, int *addrLength)
{
  OSStatus           theError = noErr;
  TBind              local;
  InetInterfaceInfo  localInfo;
  InetAddress        localInetAddr;
  OTResult           sockState;
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	    theError = EINVAL;
	    goto abort;
	  case T_IDLE:
	  case T_OUTCON:
	  case T_INCON:
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	    break;
    }
  
  /* Sanity check on length of name */
  if(*addrLength < sizeof(struct sockaddr_in))
    {
      theError = EINVAL;
      goto abort;
    }
    
  switch(gSockets[sockFD]->type)
    {
    case SOCK_STREAM:
    case SOCK_DGRAM:
      /* Set up the sock structure */
      OTMemzero(&local, sizeof(TBind));
      local.addr.maxlen = sizeof(InetAddress);
      local.addr.len    = sizeof(InetAddress);
      local.addr.buf    = (UInt8 *) &localInetAddr;
 
      /* Ask OT for the sock name */
      while(true)
        {
          theError = OTGetProtAddress(gSockets[sockFD]->ref, &local, nil);
	      _HandleOTLook(sockFD, &theError);
	      if(theError == T_ORDREL)
	        continue;
	        
	      if(_HandleOTErrors(&theError) != noErr)
	        goto abort;
	        
	      break;
        }
        
      if(localInetAddr.fHost == 0)
        {
          /* Because of multihoming, GetProtAddress doesn't return the IP address 
             we need to get the default IP address from OTInetGetInterfaceInfo() */
          theError = _OTInetGetInterfaceInfo(&localInfo, kDefaultInetInterface);
          _HandleOTLook(sockFD, &theError);
          if(_HandleOTErrors(&theError) != noErr)
            goto abort;
            
          localInetAddr.fHost = localInfo.fAddress;
        }
      
      /* copy the port, etc, into the sockaddr */
      _InetAddress2sockaddr(localAddr, localInetAddr);
      break;
      
    default:
      theError = EINVAL;
      break;
    }
    
abort:  
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, 0, -1));
}


/*
 * read() - read data from a socket
 */
int read(int sockFD, void *buffer, UInt32 numBytes)
{
  /* Read the data using recv (the same except flags == 0) */
  return( recv(sockFD, buffer, numBytes, 0));
}


/*
 * write() - write data to a socket
 */
int write(int sockFD, void *buffer, UInt32 numBytes)
{
  /* Write the data using send (the same except flags == 0) */
  return( send(sockFD, buffer, numBytes, 0));
}


/*
 * readv() - scatter read data from a socket
 */
int readv(int sockFD, struct iovec *iov, UInt32 iovCount)
{
  int     theError = noErr;
  int     i;
  OTData *iovOTData;

  /* Link up the array of iovecs to make an OTData structure */
  iovOTData = (OTData *)iov;
  for(i = 0; i < iovCount - 1; i++)
    iov[i].next = &iov[i+1];
  iovOTData[iovCount - 1].fNext = nil;
  
  /* Read data using recv.  Note that this only works because recv does not
     munge the buffer argument */
  theError = recv(sockFD, iovOTData, kNetbufDataIsOTData, 0);
  if(theError != noErr)
    goto abort;
    
abort:  
  /* Clean up after the socket operation */
  return(theError);
}


/*
 * writev() - gather write data to a socket
 */
int writev(int sockFD, struct iovec *iov, UInt32 iovCount)
{
  int     theError = noErr;
  int     i;
  OTData *iovOTData;

  /* Link up the array of iovecs to make an OTData structure */
  iovOTData = (OTData *)iov;
  for(i = 0; i < iovCount - 1; i++)
    iov[i].next = &iov[i+1];
  iovOTData[iovCount - 1].fNext = nil;
  
  /* Write data using send.  Note that this only works because send does not
     munge the buffer argument */
  theError = send(sockFD, iovOTData, kNetbufDataIsOTData, 0);
  if(theError != noErr)
    goto abort;
    
abort:  
  /* Clean up after the socket operation */
  return(theError);
}


/*
 * recv() - receive a message from a connected socket
 */
int recv(int sockFD, void *buffer, UInt32 numBytes, int flags)
{
  OSStatus           theError = noErr;
  OTResult           bytesRead;
  OTFlags            otFlags = 0;
  Boolean            resetToBlocking = false;
  struct sockaddr_in peer;
  socklen_t          addrLen;
  OTResult           sockState;
  UInt32             readBytes; 
  
  switch(gSockets[sockFD]->type)
    {
    case SOCK_STREAM:
      /* Sanity Checks to validate sockFD */
      if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
        goto abort_STREAM;
  
      /* What is the state of the socket? */
      switch(sockState)
        {
          case T_UNINIT:
            theError = ENOTSOCK;
            goto abort_STREAM;
	      case T_UNBND:
	      case T_IDLE:
	      case T_OUTCON:
	      case T_INCON:
	        theError = ENOTCONN;
	        goto abort_STREAM;
	      case T_DATAXFER:
	      case T_OUTREL:
	      case T_INREL:
	        break;
        }
  
      /* check for flags we don't support */
      if((flags & MSG_PEEK) || (flags & MSG_WAITALL))
        {
          theError = EINVAL;
          goto abort_STREAM;
        }
  
      /* Handle those flags which we support */
      if(flags & MSG_DONTWAIT && OTIsBlocking(gSockets[sockFD]->ref))
        {
          OTSetNonBlocking(gSockets[sockFD]->ref);
          resetToBlocking = true;
        }

      if(OTIsBlocking(gSockets[sockFD]->ref))
        {
          /* If we are blocking, we need to make sure we only ask for the amount available.
             This is because OTRcv will wait until it reads the entire amount requested! */
          readBytes = 0;
          while(theError == kOTNoDataErr || readBytes <= 0) 
            {
              theError = OTCountDataBytes(gSockets[sockFD]->ref, &readBytes);
              if((theError != noErr) && (theError != kOTNoDataErr)){
		          _HandleOTLook(sockFD, &theError);  /* Is it an OTLook Error? */
	              _HandleOTErrors(&theError);
		          goto abort_STREAM;
			  }
		      theError = Idle();
            }
          
          /* Some data is available... make sure it is no more than the amount the caller wants */
          if(readBytes > numBytes) 
            readBytes = numBytes;
        }
      else
        {
          /* Non-blocking endpoints can ask for more data than is available 
             because OT will always return immediately. */
          readBytes = numBytes;
        }
retry:
      /* read the data into buffer */
      bytesRead = OTRcv(gSockets[sockFD]->ref, buffer, readBytes, &otFlags);
      if(bytesRead <= 0)
        {
          theError = bytesRead;  /* we have a real error */
          
          if(theError == kOTBadSyncErr)
	        {
	          theError = Idle();
	          goto retry;  /* retry... RAMDoubler is installed */
	        }
          
	      _HandleOTLook(sockFD, &theError);  /* Is it an OTLook Error? */
	      _HandleOTErrors(&theError);
	      if( theError == EAGAIN || theError == EWOULDBLOCK) {
		   	  /*  If the call would block (we've requested non-blocking support), then
	   		   *  abort the read, making sure the error is returned to the caller via
		   	   *  [errno] to allow them to deal with retries.
		   	   */
	      	  bytesRead = 0;
	      	  goto abort_STREAM;
	      	  /*  Alternatively, use the following two lines and register an (optional) 
	      	   *  idle handler with the idle library...  This would permit the creation
	      	   *  of UI features like a spinning cursor or a "cmd-period" abort
	      	   *
	      	  theError = Idle();	// yield time to other stuff while we spin...
	      	  goto retry;
	      	   *
	      	   */
	    	}
	      if(theError == ECONNRESET || theError == ENOTCONN || theError == kOTOutStateErr)
	        {
	          /* Just return EOF over and over for now */
	          bytesRead = 0;
	          theError = noErr;
	        }
	      
	      if(theError != noErr)
	        goto abort_STREAM;  /* something bad happened */
	    }
        
abort_STREAM:
      /* restore blocking state if necessary */
      if(resetToBlocking)
        OTSetBlocking(gSockets[sockFD]->ref);
    
      /* Clean up after the socket operation */
      theError = _CleanUpAfterSocketCall(sockFD, theError, bytesRead, -1);
      break;
    
    case SOCK_DGRAM:
      /* Sanity Checks to validate sockFD */
      if((theError = _PrepareForSocketCall(sockFD, NULL)) != noErr)
        goto abort_DGRAM;
      
      /* special kludge because we are going to call recv */
      gSockets[sockFD]->inProgress = false;
      
      /* where are we reading from? */  
      if(!gSockets[sockFD]->peerValid)
        {
          theError = ENOTCONN;
          goto abort_DGRAM;
        }
        
      /* read data from peer into the buffer (drop all non-peer data) */
      do
        {
          addrLen = sizeof(peer);
          theError = recvfrom(sockFD, buffer, numBytes, flags, 
                                     (struct sockaddr *) &peer, &addrLen);
          if(theError <= 0)
            goto abort_DGRAM;
        } 
      while(gSockets[sockFD]->peer.sin_port != peer.sin_port);

abort_DGRAM:
      break;
      
    default:
      theError = EINVAL;
    }
    
  return(theError);
}


/*
 * send() - send a message from a connected socket
 */
int send(int sockFD, void *buffer, UInt32 numBytes, int flags)
{
  OSStatus           theError = noErr;
  OTResult           bytesWritten;
  OTFlags            realFlags = 0;
  Boolean            resetToBlocking = false;
  socklen_t          addrLen = sizeof(struct sockaddr_in);
  struct sockaddr_in peer;
  OTResult           sockState;

  switch(gSockets[sockFD]->type)
    {
    case SOCK_STREAM:
      /* Sanity Checks to validate sockFD */
      if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
        goto abort_STREAM;
      
      /* What is the state of the socket? */
      switch(sockState)
        {
          case T_UNINIT:
            theError = ENOTSOCK;
            goto abort_STREAM;
	      case T_UNBND:
	      case T_IDLE:
	      case T_OUTCON:
	      case T_INCON:
	        theError = ENOTCONN;
	        goto abort_STREAM;
	      case T_DATAXFER:
	      case T_OUTREL:
	      case T_INREL:
	        break;
        }
        
      /* check for flags we don't support */
      if((flags & MSG_DONTROUTE))
        {
          theError = EINVAL;
          goto abort_STREAM;
        }
  
      /* Handle those flags which we support */
      if(flags & MSG_OOB) 
        realFlags |= T_EXPEDITED;
      if(flags & MSG_DONTWAIT && OTIsBlocking(gSockets[sockFD]->ref))
        {
          OTSetNonBlocking(gSockets[sockFD]->ref);
          resetToBlocking = true;
        }
      
      /* write the data out of buffer */
      while(true)
        {
	      bytesWritten = OTSnd(gSockets[sockFD]->ref, buffer, numBytes, realFlags);
	      
	      if(bytesWritten > 0)
	        break;     /* We wrote some data!  success! */
	      
	      _HandleOTLook(sockFD, &bytesWritten);
	      if(bytesWritten == T_ORDREL)
	        continue;  /* the read end closed but we can still write */
	      
	      /* Fatal error */
	      theError = _HandleOTErrors(&bytesWritten);
	      goto abort_STREAM;
        }
        
abort_STREAM: 
      /* restore blocking state if necessary */
      if(resetToBlocking)
        OTSetBlocking(gSockets[sockFD]->ref);
    
      /* Clean up after the socket operation */
      theError = _CleanUpAfterSocketCall(sockFD, theError, bytesWritten, -1);
      break;
    
    case SOCK_DGRAM:
      /* Sanity Checks to validate sockFD */
      if((theError = _PrepareForSocketCall(sockFD, NULL)) != noErr)
        goto abort_DGRAM;
      
      /* special kludge because we are going to call recv */
      gSockets[sockFD]->inProgress = false;
      
      /* where are we sending to? */
      if(!gSockets[sockFD]->peerValid)
        {
          theError = ENOTCONN;
          goto abort_DGRAM;
        }
      peer = gSockets[sockFD]->peer;
      
      /* send the data using sendto */
      theError = sendto(sockFD, buffer, numBytes, flags, 
                        (struct sockaddr *) &peer, addrLen);
      if(theError <= 0)
        goto abort_DGRAM;

abort_DGRAM:
      break;
      
    default:
      theError = EINVAL;
      break;
    }
    
  return(theError);
}


/*
 * recvfrom() - receive a message from a socket
 */
int recvfrom(int sockFD, void *buffer, UInt32 numBytes, int flags, 
                    struct sockaddr *fromAddr, socklen_t *addrLength)
{
  OSStatus    theError = noErr;
  OTFlags     flag;
  Boolean     resetToBlocking = false;
  TUnitData   uData;
  InetAddress fromInetAddr;
  OTResult    sockState;

  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	  case T_IDLE:
	    /* Eventually we will support SOCK_STREAM */
	    if(gSockets[sockFD]->type == SOCK_STREAM)
	      {
	        theError = ENOTCONN;
	        goto abort;
	      }
	    break;
	  case T_OUTCON:
	  case T_INCON:
	    theError = ENOTCONN;
	    goto abort;
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	    break;
    }
  
  /* check for flags we don't support */
  if((flags & MSG_PEEK) || (flags & MSG_WAITALL))
    {
      theError = EINVAL;
      goto abort;
    }
  
  /* Handle those flags which we support */
  /*if(flags & MSG_OOB) 
    realFlags |= T_EXPEDITED;*/
  if(flags & MSG_DONTWAIT && !OTIsNonBlocking(gSockets[sockFD]->ref))
    {
      OTSetNonBlocking(gSockets[sockFD]->ref);
      resetToBlocking = true;
    }
  
  /* sanity check of addrLength */
  if(*addrLength < sizeof(struct sockaddr_in))
    {
      theError = EINVAL;
      goto abort;
    }
  
  /* Set up the address */
  uData.addr.maxlen = uData.addr.len = sizeof(InetAddress);
  uData.addr.buf = (UInt8 *) &fromInetAddr;
  
  /* Set up the buffer */
  uData.udata.maxlen = uData.udata.len = numBytes;
  uData.udata.buf = buffer;
  
  /* Tell OT we are not using the options field */
  uData.opt.len = 0;
  
retry:
  /* read the data into buffer */
  theError = OTRcvUData(gSockets[sockFD]->ref, &uData, &flag);
  
  if(theError == kOTBadSyncErr)
    {
      theError = Idle();
      goto retry;
    }
  
  _HandleOTLook(sockFD, &theError);  /* Is it an OTLook Error? */
  _HandleOTErrors(&theError);
  if(theError == ECONNRESET || theError == ENOTCONN || theError == kOTOutStateErr)
    {
      /* Just return EOF over and over for now */
      uData.udata.len = 0;
      theError = 0;
    }
    
  if(theError != noErr)
    goto abort;

  /* if fromAddr is not NULL, put the receiver in it */
  if(fromAddr != NULL)
    _InetAddress2sockaddr(fromAddr, fromInetAddr);

abort:  
  /* restore blocking state if necessary */
  if(resetToBlocking)
    OTSetBlocking(gSockets[sockFD]->ref);
    
  /* Clean up after the socket operation */
  return(_CleanUpAfterSocketCall(sockFD, theError, (int)uData.udata.len, -1));
}


/*
 * sendto() - send a message from a socket
 */
int sendto(int sockFD, void *buffer, UInt32 numBytes, int flags, 
                  struct sockaddr *toAddr, socklen_t addrLength)
{
  OSStatus    theError = noErr;
  OTFlags     realFlags = 0;
  Boolean     resetToBlocking = false;
  TUnitData   uData;
  InetAddress toInetAddr;
  OTResult    sockState;
  
  /* Sanity Checks to validate sockFD */
  if((theError = _PrepareForSocketCall(sockFD, &sockState)) != noErr)
    goto abort;
  
  /* What is the state of the socket? */
  switch(sockState)
    {
      case T_UNINIT:
        theError = ENOTSOCK;
        goto abort;
	  case T_UNBND:
	  case T_IDLE:
	    /* Eventually we will support SOCK_STREAM */
	    if(gSockets[sockFD]->type == SOCK_STREAM)
	      {
	        theError = ENOTCONN;
	        goto abort;
	      }
	    break;
	  case T_OUTCON:
	  case T_INCON:
	    theError = ENOTCONN;
	    goto abort;
	  case T_DATAXFER:
	  case T_OUTREL:
	  case T_INREL:
	    break;
    }
  
  /* check for flags we don't support */
  if((flags & MSG_DONTROUTE))
    {
      theError = EINVAL;
      goto abort;
    }
  
  /* Handle those flags which we support */
  if(flags & MSG_OOB) 
    realFlags |= T_EXPEDITED;
  if(flags & MSG_DONTWAIT && !OTIsNonBlocking(gSockets[sockFD]->ref))
    {
      OTSetNonBlocking(gSockets[sockFD]->ref);
      resetToBlocking = true;
    }
    
  /* sanity check of addrLength */
  if(addrLength < sizeof(struct sockaddr_in))
    {
      theError = EINVAL;
      goto abort;
    }
  
  /* Set up the address */
  _sockaddr2InetAddress(&toInetAddr, *toAddr);
  uData.addr.maxlen = uData.addr.len = sizeof(InetAddress);
  uData.addr.buf = (UInt8 *) &toInetAddr;
  
  /* Set up the buffer */
  uData.udata.maxlen = uData.udata.len = numBytes;
  uData.udata.buf = buffer;
  
  /* Tell OT we are not using the options field */
  uData.opt.len = 0;
  
  /* write the data out of buffer */
  while(true)
    {
      theError = OTSndUData(gSockets[sockFD]->ref, &uData /* , &realFlags */);
      
      if(theError == noErr)
        break;     /* We wrote some data!  success! */
        
      _HandleOTLook(sockFD, &theError);
      if(theError == T_ORDREL)
        continue;  /* the read end closed but we can still write */
      
      /* Fatal error */
      _HandleOTErrors(&theError);
      goto abort;
    }

abort:  
  /* Clean up after the socket operation */
  // Assert_(uData.udata.len > 65535);		// unsigned is being cast to int...
  return(_CleanUpAfterSocketCall(sockFD, theError, (int)uData.udata.len, -1));
}

// end of file
