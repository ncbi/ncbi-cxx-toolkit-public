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
 * Internal.c -- internal routines used by the sockets library
 * 
 */
 
#include <errno.h>
#include <string.h>				// memcpu

#include <neterrno.h>
#include "SocketsInternal.h"


/* Global Variables
 */
SocketPtr	*gSockets = NULL;
InetSvcRef	gInternetServicesRef = kOTInvalidProviderRef;
static OTNotifyUPP gDNSNotifyUPP = NULL;

/* 
 * _HandleOTLook() - Handle OTLook errors
 */
OSStatus _HandleOTLook(int sockFD, OSStatus *theError)
{
  OTResult lookResult;
  
  if(*theError != kOTLookErr)
    return(*theError);
    
  lookResult = OTLook(gSockets[sockFD]->ref);
  
  switch(lookResult)
    {
    case T_DISCONNECT:
      /* Received reset */
      *theError = OTRcvDisconnect(gSockets[sockFD]->ref, nil);
      
      gSockets[sockFD]->readClosed = true;
      gSockets[sockFD]->writeClosed = true;
      
      if(*theError != noErr)
        *theError = -3199 - *theError;
      break;
      
    case T_ORDREL:
      *theError = OTRcvOrderlyDisconnect(gSockets[sockFD]->ref);
      gSockets[sockFD]->readClosed = true;
      break;
      
    default:
      /* The call was interrupted by something we don't handle */
      *theError = EINTR;
      break;
    }
  
  if(*theError == noErr) 
    *theError = lookResult;  /* we're returning a state change to the caller */
  
  return(*theError);
}




/*
 * _HandleOTErrors() - Handle remapping of errors on socket calls
 */
OSStatus _HandleOTErrors(OSStatus *theError)
{
  switch(*theError)
    {    
    case noErr:												break;
    
    case T_DISCONNECT:			*theError = ECONNRESET;		break;	/* OTLookErr remapping														*/
    case T_ORDREL:				*theError = ENOTCONN;		break;	/* OTLookErr remapping														*/
    
    case kOTOutOfMemoryErr:		*theError = ENOMEM;			break;	/* -3211 Out of memory 														*/
	case kOTBadAddressErr:		*theError = EFAULT;			break;	/* -3150 A Bad address was specified										*/
	case kOTBadOptionErr:		*theError = EINVAL;			break;	/* -3151 A Bad option was specified											*/
	case kOTAccessErr:			*theError = EACCES;			break;	/* -3152 Missing access permission											*/
	case kOTBadReferenceErr:	*theError = ECONNRESET;		break;	/* -3153 Bad provider reference						OT Unloaded -- sleep?	*/
	case kOTNoAddressErr:		*theError = EADDRNOTAVAIL;	break;	/* -3154 No address was specified					bind()					*/
//	case kOTOutStateErr:		*theError = kOTOutStateErr;	break;	/* -3155 Call issued in wrong state					handled by API directly	*/
	case kOTBadSequenceErr:		*theError = EINVAL;			break;	/* -3156 Sequence specified does not exist									*/
	case kOTSysErrorErr:		*theError = EINVAL;			break;	/* -3157 A system error occurred											*/
	case kOTBadDataErr:			*theError = EMSGSIZE;		break;	/* -3159 An illegal amount of data was specified							*/
	case kOTBufferOverflowErr:	*theError = EINVAL;			break;	/* -3160 Passed buffer not big enough				bind()					*/
	case kOTFlowErr:			*theError = EINVAL; 		break;	/* -3161 Provider is flow-controlled										*/	
	case kOTNoDataErr:			*theError = EAGAIN;			break;	/* -3162 No data available for reading				non-blocking reads		*/
	case kOTNoDisconnectErr:	*theError = EINVAL;			break;	/* -3163 No disconnect indication available									*/
	case kOTNoUDErrErr:			*theError = EINVAL;			break;	/* -3164 No Unit Data Error indication available							*/
	case kOTBadFlagErr:			*theError = EINVAL;			break;	/* -3165 A Bad flag value was supplied										*/
	case kOTNoReleaseErr:		*theError = EINVAL;			break;	/* -3166 No orderly release indication available							*/
	case kOTNotSupportedErr:	*theError = EOPNOTSUPP;		break;	/* -3167 Command is not supported											*/
	case kOTStateChangeErr:		*theError = EINTR;			break;	/* -3168 State is changing - try again later		in the middle of change	*/
	case kOTNoStructureTypeErr:	*theError = EINVAL;			break;	/* -3169 Bad structure type requested for OTAlloc							*/
	case kOTBadNameErr:			*theError = ENOTSOCK;		break;	/* -3170 A bad endpoint name was supplied									*/
	case kOTBadQLenErr:			*theError = EINVAL;			break;	/* -3171 A Bind to an in-use addr with qlen > 0								*/
	case kOTAddressBusyErr:		*theError = EADDRINUSE;		break;	/* -3172 Address requested is already in use		bind()					*/
	case kOTResQLenErr:			*theError = EINVAL;			break;	/* -3175																	*/
	case kOTResAddressErr:		*theError = EINVAL;			break;	/* -3176																	*/
	case kOTQFullErr:			*theError = EINVAL;			break;	/* -3177																	*/
	case kOTProtocolErr:		*theError = EINVAL;			break;	/* -3178 An unspecified provider error occurred								*/
	case kOTBadSyncErr:			*theError = EWOULDBLOCK;	break;	/* -3179 A synchronous call at interrupt time								*/
//	case kOTCanceledErr:		*theError = ECANCEL;		break;	/* -3180 The command was cancelled					AbortSocketOperation()	*/
	
	default:					*theError = EINVAL;			break;	/* Everything else 															*/
    }
  return(*theError);
}


/*
 *	PrepareForSocketCall() - Performs basic initialization and sanity checks
 *                           needed before any socket API call (except socket).
 */
OSErr _PrepareForSocketCall(int sockFD, OTResult *sockState)
{
	OSErr theErr = kOTNoError;

	/* Is the socket number in range for gSockets? */
	if(sockFD < 0 || sockFD >= kNumSockets)
		return(ENOTSOCK);

	/* make sure we've initialized the global... */
	if( gSockets == NULL){
		theErr = ot_OpenDriver();
		if( theErr != kOTNoError)
			return theErr;
	}

	/* Does the socket already exist? We need it to be! */
	if(gSockets[sockFD] == NULL || gSockets[sockFD]->ref == kOTInvalidEndpointRef)
		return(ENOTSOCK);
  
	/* Does the socket already have an operation in progress? */  
	if(gSockets[sockFD]->inProgress == true)
		return(EALREADY);

	/* Get the endpoint state for the convenience of the caller */
	if(sockState != NULL)
		*sockState = OTGetEndpointState(gSockets[sockFD]->ref);

	/* Now mark the socket in use so that when we yield in SocketIdleProc, we
	   don't allow other threads to start other operations on this socket 

	   This step must occur last so that if an error occurs the socket is 
	   still marked not in use 
	 */
	gSockets[sockFD]->inProgress = true;

	/* the socket is fine! */
	return(theErr);
}


/*
 * _CleanUpAfterSocketCall() - restores state after a socket operation
 *                            and returns what the retVal should be.
 *                            okayVal is the return value when no errors occur.
 *                            errorVal is the return value on error (usually -1);
 */
int _CleanUpAfterSocketCall(int sockFD, OSStatus theError, int okayVal, int errorVal)
{
  int retVal;
  
  /* make sure the socket is valid before doing anything with it */
  /* we may have had a severe enough error to have no valid sockFD */
  if(sockFD >= 0 && sockFD < kNumSockets && gSockets[sockFD] != NULL)
    {
      /* Mark the socket as no longer in use -- this must be done first */
      gSockets[sockFD]->inProgress = false;
  
      /* Do any additional cleanup here */
    }
  
  /* Set errno, if needed */
  if(theError != noErr)
    {
      /* Set the global error code */
      ncbi_SetErrno(theError);
      retVal = errorVal;
    }
  else
    {
      /* clear out the global error code so it will be noErr */
      ncbi_ClearErrno();
      retVal = okayVal;
    }
  
  return(retVal);
}


/*
 * _SocketNotifyProc() - Handles idle events on sockets (TCP and UDP endpoints).
 */
pascal void _SocketNotifyProc(void *, OTEventCode code, OTResult result, void *cookie)
{
  #pragma unused(result)
  #pragma unused(cookie)
  
  OSStatus theError;
  
  switch (code)
    {
    case kOTSyncIdleEvent:
      theError = noErr; // Idle(); need to create an idle proc...
      if(theError != noErr){
        ncbi_SetErrno(theError);
        }
      break;
    
    default:
      break;
    }
}


/*
 * _DNSNotifyProc() - Handles idle events on sockets (TCP and UDP endpoints).
 */
pascal void _DNSNotifyProc(void *, OTEventCode code, OTResult result, void *cookie)
{
  #pragma unused(result)
  #pragma unused(cookie)
  
  OSStatus theError;
  
  switch (code)
    {
    case kOTSyncIdleEvent:
      theError = noErr;
    //  Idle();
      if(theError != noErr){
        ncbi_SetErrno(theError);
        }
      break;
    
    case kOTProviderWillClose:
    case kOTProviderIsClosed:
      /* OT is closing the provider out from underneath us.
         We remove our reference to it so the next time we need it we'll reopen it. */
      (void) OTCloseProvider(gInternetServicesRef);
      gInternetServicesRef = kOTInvalidProviderRef;
      break;
    
    default:
      break;
    }
}


/*
 * _DNSOpenInternetServices() - Opens the internet services provider for use by the NetdbHosts functions
 */
OSStatus _DNSOpenInternetServices(void)
{
  OSStatus theError = noErr;

  if(gInternetServicesRef != kOTInvalidProviderRef)
    goto abort;
  
  /* Get the internet services reference we will use for DNS calls */
#if TARGET_API_MAC_CARBON
// This should NOT be necessary.  Something screwy is going on and I haven't figured out what.
  gInternetServicesRef = OTOpenInternetServicesInContext(
  	kDefaultInternetServicesPath, 0, &theError, NULL);
#else
  gInternetServicesRef = OTOpenInternetServices(kDefaultInternetServicesPath, 0, &theError);
#endif
  if(theError != noErr)
    {
      gInternetServicesRef = kOTInvalidProviderRef;
      goto abort;
    }
  
  /* Provider is syncronous */
  theError = OTSetSynchronous(gInternetServicesRef);
  if(theError != noErr)
    goto abort;
  
  /* Install our notifier function to get idle events */
  // 2001-03-28:  Joshua Juran
  // Carbon procs must be UPP's.
  //theError = OTInstallNotifier(gInternetServicesRef, _DNSNotifyProc, nil);
  gDNSNotifyUPP = NewOTNotifyUPP(_DNSNotifyProc);
  theError = OTInstallNotifier(gInternetServicesRef, gDNSNotifyUPP, nil);
  if(theError != noErr)
    goto abort;

  /* Tell OT we want idle events */
  theError = OTUseSyncIdleEvents(gInternetServicesRef, true);
  if(theError != noErr)
    goto abort;
  
abort:
  if(theError == ENXIO || theError == EHOSTUNREACH || theError == EINVAL)
  	theError = ENETDOWN;
  
  return(theError);
}


/*
 * _InetAddress2sockaddr() - Translates between an InetAddress and a sockaddr
 *                          True, the structures are the same, but this is safer
 *                          because on some compilers they might be different.
 */
void _InetAddress2sockaddr(struct sockaddr *addr, InetAddress inetAddr)
{
  struct sockaddr_in *addrIn;
  long                i;
  
  addrIn = (struct sockaddr_in *) addr;
  
  addrIn->sin_family      = inetAddr.fAddressType;
  addrIn->sin_port        = inetAddr.fPort;
  addrIn->sin_addr.s_addr = inetAddr.fHost;
  for(i = 0; i < 8; i++)
    addrIn->sin_zero[i]        = inetAddr.fUnused[i];
  
  return;
}


/*
 * _sockaddr2InetAddress() - Translates between a sockaddr and an InetAddress
 */
void _sockaddr2InetAddress(InetAddress *inetAddr, struct sockaddr addr)
{
  struct sockaddr_in addrIn;
  long               i;
  
  memcpy(&addrIn, &addr, sizeof(struct sockaddr_in));

  inetAddr->fAddressType = addrIn.sin_family;
  inetAddr->fPort        = addrIn.sin_port;
  inetAddr->fHost        = addrIn.sin_addr.s_addr;
  for(i = 0; i < 8; i++)
    inetAddr->fUnused[i]      = addrIn.sin_zero[i];
  
  return;
}

/*
// Magic types for creating UPPs and suchlike
typedef OSStatus (*OTInetGetInterfaceInfo_ProcPtrType)(InetInterfaceInfo *, SInt32);

enum {
  OTInetGetInterfaceInfo_ProcInfo = kPascalStackBased
  | RESULT_SIZE(SIZE_CODE(sizeof(OSStatus)))
  | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(InetInterfaceInfo *)))
  | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(SInt32)))
};

OSStatus _OTInetGetInterfaceInfo(InetInterfaceInfo *inInfo, SInt32 inIndex)
{
  return( OTInetGetInterfaceInfo(inInfo, inIndex));

}
*/
