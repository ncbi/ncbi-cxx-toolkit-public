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
#ifndef qDebug
#define qDebug 1
#endif
 */

#include <OpenTransport.h>
#include <OpenTptInternet.h>	// for TCP/IP
#include <OTDebug.h>			// OTAssert macro
#include <Threads.h>			// declaration for YieldToAnyThread
#include <neterrno.h>			// UNIX error codes
#include <s_types.h>
# include <neti_in.h>

# include "sock_str.h"
# include "sock_int.h"

#include "ncbiOT.h"		// added during dev, should be moved???


static short driver = 0;
static Boolean gOTInited = false;
EndpointRef gDNRep = kOTInvalidEndpointRef;


/* DNR yield notifier process
 *
 *	Using this function, we can theoretically continue to operate the 
 *	"foreground" threads of an application while waiting for blocking 
 *	DNR calls to complete (or timeout).
 *
 *	Also handles the case where the user has aborted the action with cmd-.
 */

static pascal void DNRYieldNotifier(void* contextPtr, OTEventCode code, 
                             OTResult result, void* cookie)
{
#pragma unused(contextPtr)
#pragma unused(result)
#pragma unused(cookie)
	OSStatus junk;
  
	switch (code) {
	  case kOTSyncIdleEvent:
		junk = YieldToAnyThread();
		OTAssert("YieldingNotifier: YieldToAnyThread failed", junk == noErr);
		break;
	  case kOTProviderWillClose:
	  case kOTProviderIsClosed:
		/* OT is closing the provider out from underneath us.
		 We remove our reference to it so the next time we need it we'll reopen it. */
		(void) OTCloseProvider(gDNRep);
		gDNRep = kOTInvalidProviderRef;
		break;
    
	  default:
		/* do nothing */
		break;
	}
}

/* ot_DNRInit
 *
 *	Function which initializes the domain name resolver for OpenTransport
 *	for use by gethostbyname, gethostbyaddress, and other utils
 */

OSStatus ot_DNRInit(void)
{
	OSStatus theError = kOTNoError;

	if( gDNRep != kOTInvalidProviderRef){
		return (noErr);
	}

	if (!gOTInited){
		theError = ot_OpenDriver();
	}

	/* Get the internet services reference we will use for DNS calls */
	if( theError == noErr){
		gDNRep = OTOpenInternetServices( kDefaultInternetServicesPath, 0, &theError);
	}

	/* Make the provider syncronous */
	if( theError == kOTNoError){
		theError = OTSetSynchronous( gDNRep);
	}

	/* Install our notifier function to get idle events */
	if( theError == kOTNoError){
		theError = OTInstallNotifier(gDNRep, DNRYieldNotifier, nil);
	}

	/* Tell OT we want idle events */
	if( theError == kOTNoError){
		theError = OTUseSyncIdleEvents(gDNRep, true);
	}
	return( theError);
}


OSStatus ot_DNRClose( InetSvcRef theRef)
{
	OSStatus theErr = kOTNoError;

	return theErr;
}


/* CFMTerminate
 *
 *	Function which will clean up the OT driver it the program is
 *	killed in an unexpected manner (prevents a memory leak)
 *
 *	The OT docs say I we need to set some linker option to have this
 *	function called when the app exits...  I think it only applies to 68k targets
 */

void CFMTerminate (void)
{
	if (gOTInited){
		gOTInited = false;
		(void) CloseOpenTransport();
	}
}



OSErr ot_OTErrorToSocketError( OSStatus err)
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
			newErr = ENOTSOCK;
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
//		case kENIOErr:	not defined??	//	-3204 An I/O error occurred.
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
			newErr = EINVAL;
			break;	
    }
  return(newErr);
}


/* ot_OpenDriver
 *
 *	Performs required initializations needed before OT can be used
 */

OSStatus ot_OpenDriver( void) 
{
	OSStatus err = kOTNoError;

	/* if this function has already been performed, then just return */
	if( gOTInited){
		return (err);
	}

	err = InitOpenTransport();
	gOTInited = (err == noErr);
	
	return( err);
}

#if 0
/* ot_TCPCreate
 *
 *	Creates an OT endpoint for use with TCP
 *	use ot_UDPCreate for creation of a UDP endpoint
 */

OSErr ot_TCPCreate(
	int buflen,				// size of buffer to allocate
	TCPNotifyProc notify,	// function for async notifications
	TCPiopb *pb)			// obsolete???  Perhaps should be *ep??
{	
	OSStatus	err = noErr;
	Ptr		transferBuffer = nil;
	EndpointRef ep = kOTInvalidEndpointRef;

	transferBuffer = OTAllocMem(buflen);
	if( transferBuffer == nil ){
		err = kENOMEMErr;
	}
	/* Open a TCP endpoint. */
	if (err == noErr){
		ep = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);
	}

	// need to install the notifier and set some other options...
	if (err == noErr){
		OSErr junk;

		// set the endpoint to run syncronously, i.e. wait for results
		junk = OTSetSynchronous(ep);
		OTAssert("xTCPCreate: OTSetSynchronous failed", junk == noErr);

		// set the endpoing to block on pending calls
		junk = OTSetBlocking(ep);
		OTAssert("xTCPCreate: OTSetBlocking failed", junk == noErr);

		// install the notifier function to yield time to other threads
		junk = OTInstallNotifier(ep, YieldingNotifier, nil);
		OTAssert("xTCPCreate: OTInstallNotifier failed", junk == noErr);

		// after setting that, now tell OT to generate the idle events that
		//	cause the yield notifier to be invoked
		junk = OTUseSyncIdleEvents(ep, true);
		OTAssert("xTCPCreate: OTUseSyncIdleEvents failed", junk == noErr);

	/* Bind the endpoint. probably shouldn't be done in this function...
		err = OTBind(ep, nil, nil);
		bound = (err == noErr);
	 */
	}
	return (err);

}
#endif // 0 commented out tcpcreate() pjc 1/22/00


// MacTCP functions from here on (will eventually be deleted) of the same
OSErr xOpenDriver() 
{ 
	if (driver == 0) 
	{ 
		ParamBlockRec pb; 
		OSErr io; 
		
		pb.ioParam.ioCompletion = 0L; 
		pb.ioParam.ioNamePtr = "\p.IPP"; 
		pb.ioParam.ioPermssn = fsCurPerm; 
		io = PBOpen(&pb,false); 
		if (io != noErr) 
			return(io); 
		driver = pb.ioParam.ioRefNum; 
	}
	return noErr;
}

/*
 * create a TCP stream
 */
OSErr xTCPCreate(
	int buflen,
	TCPNotifyUPP notify,
	TCPiopb *pb)
{	
	pb->ioCRefNum = driver;
	pb->csCode = TCPCreate;
	pb->csParam.create.rcvBuff = (char *)NewPtr(buflen);
	pb->csParam.create.rcvBuffLen = buflen;
	pb->csParam.create.notifyProc = (TCPNotifyUPP) notify;
	return (xPBControlSync(pb));
}

/*
 * start listening for a TCP connection
 */
OSErr xTCPPassiveOpen( 
	TCPiopb *pb, 
	short port,
	TCPIOCompletionUPP completion)
{
	if (driver == 0)
		return(invalidStreamPtr);

	pb->ioCRefNum = driver;
	pb->csCode = TCPPassiveOpen;
	pb->csParam.open.validityFlags = timeoutValue | timeoutAction;
	pb->csParam.open.ulpTimeoutValue = 255 /* seconds */;
	pb->csParam.open.ulpTimeoutAction = 0 /* 1:abort 0:report */;
	pb->csParam.open.commandTimeoutValue = 0 /* infinity */;
	pb->csParam.open.remoteHost = 0;
	pb->csParam.open.remotePort = 0;
	pb->csParam.open.localHost = 0;
	pb->csParam.open.localPort = port;
	pb->csParam.open.dontFrag = 0;
	pb->csParam.open.timeToLive = 0;
	pb->csParam.open.security = 0;
	pb->csParam.open.optionCnt = 0;
	return (xPBControl(pb,completion));
}

/*
 * connect to a remote TCP
 */
OSErr xTCPActiveOpen( 
	TCPiopb *pb, 
	short port,
	long rhost,
	short rport,
	TCPIOCompletionUPP completion)
{
	if (driver == 0)
		return(invalidStreamPtr);

	pb->ioCRefNum = driver;
	pb->csCode = TCPActiveOpen;
	pb->csParam.open.validityFlags = timeoutValue | timeoutAction;
	pb->csParam.open.ulpTimeoutValue = 60 /* seconds */;
	pb->csParam.open.ulpTimeoutAction = 1 /* 1:abort 0:report */;
	pb->csParam.open.commandTimeoutValue = 0;
	pb->csParam.open.remoteHost = rhost;
	pb->csParam.open.remotePort = rport;
	pb->csParam.open.localHost = 0;
	pb->csParam.open.localPort = port;
	pb->csParam.open.dontFrag = 0;
	pb->csParam.open.timeToLive = 0;
	pb->csParam.open.security = 0;
	pb->csParam.open.optionCnt = 0;
	return (xPBControl(pb,completion));
}

OSErr xTCPNoCopyRcv( 
	TCPiopb *pb,
	rdsEntry *rds, 
	int rdslen,
	int	timeout,
	TCPIOCompletionUPP completion)
{
	
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPNoCopyRcv;
	pb->csParam.receive.commandTimeoutValue = timeout; /* seconds, 0 = blocking */
	pb->csParam.receive.rdsPtr = (Ptr)rds;
	pb->csParam.receive.rdsLength = rdslen;
	return (xPBControl(pb,completion));
}

OSErr xTCPBufReturn(TCPiopb *pb,rdsEntry *rds,TCPIOCompletionUPP completion)
	{
	pb->ioCRefNum = driver;
	pb->csCode = TCPRcvBfrReturn;
	pb->csParam.receive.rdsPtr = (Ptr)rds;
	
	return (xPBControl(pb,completion));
	}
	
/*
 * send data
 */
OSErr xTCPSend( 
	TCPiopb *pb,
	wdsEntry *wds, 
	Boolean push,
	Boolean urgent,
	TCPIOCompletionUPP completion)
{
	if (driver == 0)
		return invalidStreamPtr;
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPSend;
	pb->csParam.send.validityFlags = timeoutValue | timeoutAction;
	pb->csParam.send.ulpTimeoutValue = 60 /* seconds */;
	pb->csParam.send.ulpTimeoutAction = 0 /* 0:abort 1:report */;
	pb->csParam.send.pushFlag = push;
	pb->csParam.send.urgentFlag = urgent;
	pb->csParam.send.wdsPtr = (Ptr)wds;
	return (xPBControl(pb,completion));
}


/*
 * close a connection
 */
OSErr xTCPClose(TCPiopb *pb,TCPIOCompletionUPP completion) 
{
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPClose;
	pb->csParam.close.validityFlags = timeoutValue | timeoutAction;
	pb->csParam.close.ulpTimeoutValue = 60 /* seconds */;
	pb->csParam.close.ulpTimeoutAction = 1 /* 1:abort 0:report */;
	return (xPBControl(pb,completion));
}

/*
 * abort a connection
 */
OSErr xTCPAbort(TCPiopb *pb) 
{
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPAbort;
	return (xPBControlSync(pb));
}

/*
 * close down a TCP stream (aborting a connection, if necessary)
 */
OSErr xTCPRelease( 
	TCPiopb *pb)
{
	OSErr io;
	
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPRelease;
	io = xPBControlSync(pb);
	if (io == noErr)
		DisposePtr(pb->csParam.create.rcvBuff); /* there is no release pb */
	return(io);
}

int
xTCPBytesUnread(SocketPtr sp) 
{
	TCPiopb	*pb;
	OSErr io;
	
	if (!(pb = sock_fetch_pb(sp)))
		return -1;		/* panic */
	
	if (driver == 0)
		return(-1);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPStatus;
	io = xPBControlSync(pb);
	if (io != noErr)
		return(-1);
	return(pb->csParam.status.amtUnreadData);
}

int
xTCPBytesWriteable(SocketPtr sp)
	{
	TCPiopb *pb;
	OSErr	io;
	long	amount;
	
	if (!(pb = sock_fetch_pb(sp)))
		return -1;		/* panic */
	
	if (driver == 0)
		return(-1);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPStatus;
	io = xPBControlSync(pb);
	if (io != noErr)
		return(-1);
	amount = pb->csParam.status.sendWindow-pb->csParam.status.amtUnackedData;
	if (amount < 0)
		amount = 0;
	return amount;
	}
	
int xTCPWriteBytesLeft(SocketPtr sp)
	{
	TCPiopb *pb;
	OSErr	io;
	
	if (!(pb = sock_fetch_pb(sp)))
		return -1;		/* panic */
	
	if (driver == 0)
		return(-1);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPStatus;
	io = xPBControlSync(pb);
	if (io != noErr)
		return(-1);
	return (pb->csParam.status.amtUnackedData);
	}

int xTCPState(TCPiopb *pb)
	{
	OSErr io;
	
	if (driver == 0)
		return(-1);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPStatus;
	io = xPBControlSync(pb);
	if (io != noErr)
		return(-1);
	return(pb->csParam.status.connectionState);
	}


/*
 * create a UDP stream, hook it to a socket.
 */
OSErr xUDPCreate(SocketPtr sp,int buflen,ip_port port)
	{	
	UDPiopb *pb;
	OSErr   io;
	
	if ( !(pb = sock_fetch_pb(sp) ) )
		return -1;
	
	pb->ioCRefNum = driver;
	pb->csCode = UDPCreate;
	pb->csParam.create.rcvBuff = (char *)NewPtr(buflen);
	pb->csParam.create.rcvBuffLen = buflen;
	pb->csParam.create.notifyProc = NULL;
	pb->csParam.create.localPort = port;
	if ( (io = xPBControlSync( (TCPiopb *)pb ) ) != noErr)
		return io;
		
	sp->stream = pb->udpStream;
	sp->sa.sin_port = pb->csParam.create.localPort;
	return noErr;
	}

/*
 * ask for incoming data
 */
OSErr xUDPRead(SocketPtr sp,UDPIOCompletionUPP completion) 
	{
	UDPiopb *pb;
	
	if ( !(pb = sock_fetch_pb(sp) ))
		return -1;
	
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = UDPRead;
	pb->csParam.receive.timeOut = 0 /* infinity */;
	pb->csParam.receive.secondTimeStamp = 0/* must be zero */;
	return (xPBControl ( (TCPiopb *)pb, (TCPIOCompletionUPP)completion ));
	}

OSErr xUDPBfrReturn(SocketPtr sp) 
	{
	UDPiopb *pb;

	if ( !(pb = sock_fetch_pb(sp) ))
		return -1;

	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = UDPBfrReturn;
	pb->csParam.receive.rcvBuff = sp->recvBuf;
	sp->recvBuf = 0;
	sp->recvd   = 0;
	return ( xPBControl( (TCPiopb *)pb,(TCPIOCompletionUPP)-1 ) );
	}

/*
 * send data
 */
OSErr xUDPWrite(SocketPtr sp,ip_addr host,ip_port port,miniwds *wds,
		UDPIOCompletionUPP completion) 
	{
	UDPiopb	*pb;
	
	if ( !(pb = sock_fetch_pb(sp) ))
		return -1;
		
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = UDPWrite;
	pb->csParam.send.remoteHost = host;
	pb->csParam.send.remotePort = port;
	pb->csParam.send.wdsPtr = (Ptr)wds;
	pb->csParam.send.checkSum = true;
	pb->csParam.send.sendLength = 0/* must be zero */;
	return (xPBControl( (TCPiopb *)pb, (TCPIOCompletionUPP)completion));
	}

/*
 * close down a UDP stream (aborting a read, if necessary)
 */
OSErr xUDPRelease(SocketPtr sp) {
	UDPiopb *pb;
	OSErr io;

	if ( !(pb = sock_fetch_pb(sp) ))
		return -1;
	
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = UDPRelease;
	io = xPBControlSync( (TCPiopb *)pb );
	if (io == noErr) {
		DisposePtr(pb->csParam.create.rcvBuff);
		}
	return(io);
	}

/* xIPAddr
 *
 *	This function returns the internet IP address of the default
 *	endpoint which is configured for this system.  In most all cases
 *	there will be only one physical interface, and most hosts will
 *	only be assigned a single IP address, but if that is not the case
 *	then it is possible that this call could return the information 
 *	for the wrong interface.
 */
ip_addr xIPAddr(void) 
{
	OSStatus	err;
	InetInterfaceInfo info;
	
	err = OTInetGetInterfaceInfo ( &info, kDefaultInetInterface);
	if (err != noErr)
		return(0);
	return( info.fAddress);
}

long xNetMask() 
{
#if !defined(__GETMYIPADDR__) && !defined (__MWERKS__)
	struct IPParamBlock pbr;
#else
#ifdef __MWERKS__
	GetAddrParamBlock pbr;
#else
	struct GetAddrParamBlock pbr;
#endif
#endif
	OSErr io;
	
	pbr.ioCRefNum = driver;
	pbr.csCode = ipctlGetAddr;
	io = xPBControlSync( (TCPiopb *)&pbr);
	if (io != noErr)
		return(0);
	return(pbr.ourNetMask);
}

unsigned short xMaxMTU()
{
#ifdef __MWERKS__
	UDPiopb pbr;
#else
	struct UDPiopb pbr;
#endif
	OSErr io;
	
	pbr.ioCRefNum = driver;
	pbr.csCode = UDPMaxMTUSize;
	pbr.csParam.mtu.remoteHost = xIPAddr();
	io = xPBControlSync( (TCPiopb *)&pbr );
	if (io != noErr)
		return(0);
	return(pbr.csParam.mtu.mtuSize);
}

OSErr xPBControlSync(TCPiopb *pb) 
{ 
	(pb)->ioCompletion = 0L; 
	return PBControl((ParmBlkPtr)(pb),false); 
}

#pragma segment SOCK_RESIDENT

OSErr xTCPRcv( 
	TCPiopb *pb,
	Ptr buf, 
	int buflen,
	int	timeout,
	TCPIOCompletionUPP completion)
{
	
	if (driver == 0)
		return(invalidStreamPtr);
	
	pb->ioCRefNum = driver;
	pb->csCode = TCPRcv;
	pb->csParam.receive.commandTimeoutValue = timeout; /* seconds, 0 = blocking */
	pb->csParam.receive.rcvBuff = buf;
	pb->csParam.receive.rcvBuffLen = buflen;
	return (xPBControl(pb,completion));
}


OSErr xPBControl(TCPiopb *pb,TCPIOCompletionUPP completion) 
{ 

#if !defined(powerc) && !defined(__powerc) && !defined(__POWERPC)
	pb->ioNamePtr = ReturnA5();
#endif
	
	if (completion == 0L) 
	{ 
		(pb)->ioCompletion = 0L; 
		return(PBControl((ParmBlkPtr)(pb),false));		/* sync */
	} 
	else if (completion == (TCPIOCompletionUPP)-1L) 
	{ 
		(pb)->ioCompletion = 0L; 
		return(PBControl((ParmBlkPtr)(pb),true));		/* async */
	} 
	else 
	{  
		(pb)->ioCompletion = (TCPIOCompletionUPP) completion; 
		return(PBControl((ParmBlkPtr)(pb),true));		/* async */
	} 
}

