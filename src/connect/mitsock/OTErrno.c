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
 * Revision 1.2  2001/11/07 22:37:43  juran
 * Phil Churchill's 2001-05-07 changes.
 * Use errno instead of errno_long.
 *
 * Revision 1.1  2001/04/03 20:35:25  juran
 * Phil Churchill's MIT-derived OT sockets library.  No changes prior to initial check-in.
 *
 * Revision 6.1  2000/03/20 21:50:36  kans
 * initial checkin - initial work on OpenTransport (Churchill)
 *
 * Revision 6.1  1997/12/12 22:39:24  kans
 * DisposPtr now DisposePtr
 *
 * Revision 6.0  1997/08/25 18:38:20  madden
 * Revision changed to 6.0
 *
 * Revision 4.0  1995/07/26 13:56:09  ostell
 * force revision to 4.0
 *
 * Revision 1.5  1995/05/18  08:23:09  epstein
 * add RCS modification history (after PowerPC port)
 *
 */
 
#include <Memory.h>
#include <Files.h>
#include <Errors.h>
#include <Devices.h>

/* when debugging with the OT debugging libraries,
 *	set this variable to 1
 */
#ifndef qDebug
#define qDebug 0
#endif

#include <OpenTransport.h>
#include <OpenTptInternet.h>	// for TCP/IP
#include <neterrno.h>			// UNIX error codes
#include "SocketsInternal.h"

void ncbi_ClearErrno( void)
{
	errno = kOTNoError;
}

OSErr ncbi_GetErrno( OSStatus err)
{
	return errno;
}

void ncbi_SetErrno( OSStatus err)
{
	errno = err;
	return;
}

void ncbi_SetOTErrno( OSStatus err)
{
	errno = ncbi_OTErrorToSocketError( err);
	return;
}

OSErr ncbi_OTErrorToSocketError( OSStatus err)
{
	OSErr newErr = noErr;

	switch( err){
	    case noErr:
	    	break;
		case kOTBadAddressErr:		//	-3150 A Bad address was specified
			newErr = EFAULT;
			break;	
		case kOTBadOptionErr:		//	-3151 A Bad option was specified
			newErr = EINVAL;
			break;
		case kOTAccessErr:			//	-3152 Missing access permission
			newErr = EACCES;
			break;
		case kOTBadReferenceErr:	//	-3153 Bad provider reference
			newErr = ECONNRESET;
			break;	 
		case kOTNoAddressErr:		//	-3154 No address was specified
			newErr = EADDRNOTAVAIL;
			break;	 
		case kOTOutStateErr:		//	-3155 Call issued in wrong state
			newErr = kOTOutStateErr;
			break;	 
		case kOTBadSequenceErr:		//	-3156 Sequence specified does not exist
			newErr = EINVAL;
			break;	 
		case kOTSysErrorErr:		//	-3157 A system error occurred
			newErr = EINVAL;
			break;	 
		case kOTBadDataErr:			//	-3159 An illegal amount of data was specified
			newErr = EMSGSIZE;
			break;	 
		case kOTBufferOverflowErr:	//	-3160 Passed buffer not big enough
			newErr = EINVAL;
			break;	 
		case kOTFlowErr:			//	-3161 Provider is flow-controlled
			newErr = EINVAL;
			break;	 
		case kOTNoDataErr:			//	-3162 No data available for reading
			newErr = EAGAIN;
			break;	 
		case kOTNoDisconnectErr:	//	-3163 No disconnect indication available
			newErr = EINVAL;
			break;	 
		case kOTNoUDErrErr:			//	-3164 No Unit Data Error indication available
			newErr = EINVAL;
			break;	 
		case kOTBadFlagErr:			//	-3165 A Bad flag value was supplied
			newErr = EINVAL;
			break;	 
		case kOTNoReleaseErr:		//	-3166 No orderly release indication available
			newErr = EINVAL;
			break;	 
		case kOTNotSupportedErr:	//	-3167 Command is not supported
			newErr = EOPNOTSUPP;
			break;	 
		case kOTStateChangeErr:		//	-3168 State is changing - try again later
			newErr = EINTR;
			break;	 
		case kOTNoStructureTypeErr:	//	-3169 Bad structure type requested for OTAlloc
			newErr = EINVAL;
			break;	 
		case kOTBadNameErr:			//	-3170 A bad endpoint name was supplied
			newErr = EHOSTUNREACH;
			break;	 
		case kOTBadQLenErr:			//	-3171 A Bind to an in-use addr with qlen > 0
			newErr = EINVAL;
			break;	 
		case kOTAddressBusyErr:		//	-3172 Address requested is already in use
			newErr = EADDRINUSE;
			break;	 
		case kOTResQLenErr:			//	-3175
			newErr = EINVAL;
			break;	 
		case kOTResAddressErr:		//	-3176
			newErr = EINVAL;
			break;	 
		case kOTQFullErr:			//	-3177
			newErr = EINVAL;
			break;	 
		case kOTProtocolErr:		//	-3178 An unspecified provider error occurred
			newErr = EINVAL;
			break;	 
		case kOTBadSyncErr:			//	-3179 A synchronous call at interrupt time
			newErr = EWOULDBLOCK;
			break;
		case kOTCanceledErr:		//	-3180 The command was cancelled
        case kOTNotFoundErr:		//	-3201 Requested information does not exist
//		case kENIOErr:				//	-3204 An I/O error occurred. not defined...
		case kENXIOErr:				//	-3205 No such device or address.
		case kEBADFErr:				//	-3208 invalid provider or stream ref
			newErr = EINVAL;
			break;	 
		case kEAGAINErr:			//	-3210 non-blocking provider, call would block 
			newErr = EWOULDBLOCK;
			break;
        case kOTOutOfMemoryErr:		//	-3211 Out of memory
        	newErr = ENOMEM;
        	break;
		
		default:				// Everything else
			DebugStr("\pmitsocklib: Unknown OpenTransport error code");
			newErr = EINVAL;
			break;	
    }
	return(newErr);
}


// end of file
