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
 */

/*
 *	Glue routines to call the MacTCP drivers
 */
 
#ifdef USEDUMP
# pragma load "socket.dump"

#else
# include <Memory.h>
# include <Files.h>
# include <Errors.h>

# include <s_types.h>
# include <neti_in.h>

# include "sock_str.h"

# include "sock_int.h"

#endif

#include <Devices.h>

static short driver = 0;

/*
 * Hack fix for MacTCP 1.0.X bug
 */
 
pascal char *ReturnA5(void) = {0x2E8D};

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
	TCPNotifyProc notify,
	TCPiopb *pb)
{	
	pb->ioCRefNum = driver;
	pb->csCode = TCPCreate;
	pb->csParam.create.rcvBuff = (char *)NewPtr(buflen);
	pb->csParam.create.rcvBuffLen = buflen;
	pb->csParam.create.notifyProc = notify;
	return (xPBControlSync(pb));
}

/*
 * start listening for a TCP connection
 */
OSErr xTCPPassiveOpen( 
	TCPiopb *pb, 
	short port,
	TCPIOCompletionProc completion)
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
	TCPIOCompletionProc completion)
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
	TCPIOCompletionProc completion)
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

OSErr xTCPBufReturn(TCPiopb *pb,rdsEntry *rds,TCPIOCompletionProc completion)
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
	TCPIOCompletionProc completion)
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
OSErr xTCPClose(TCPiopb *pb,TCPIOCompletionProc completion) 
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
		DisposPtr(pb->csParam.create.rcvBuff); /* there is no release pb */
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
OSErr xUDPRead(SocketPtr sp,UDPIOCompletionProc completion) 
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
	return (xPBControl ( (TCPiopb *)pb, (TCPIOCompletionProc)completion ));
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
	return ( xPBControl( (TCPiopb *)pb,(TCPIOCompletionProc)-1 ) );
	}

/*
 * send data
 */
OSErr xUDPWrite(SocketPtr sp,ip_addr host,ip_port port,miniwds *wds,
		UDPIOCompletionProc completion) 
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
	return (xPBControl( (TCPiopb *)pb, (TCPIOCompletionProc)completion));
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
		DisposPtr(pb->csParam.create.rcvBuff);
		}
	return(io);
	}

ip_addr xIPAddr(void) 
{
	struct IPParamBlock pbr;
	OSErr io;
	
	pbr.ioCRefNum = driver;
	pbr.csCode = ipctlGetAddr;
	io = xPBControlSync( (TCPiopb *)&pbr );
	if (io != noErr)
		return(0);
	return(pbr.ourAddress);
}

long xNetMask() 
{
	struct IPParamBlock pbr;
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
	struct UDPiopb pbr;
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
	TCPIOCompletionProc completion)
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


OSErr xPBControl(TCPiopb *pb,TCPIOCompletionProc completion) 
{ 
	pb->ioNamePtr = ReturnA5();
	
	if (completion == 0L) 
	{ 
		(pb)->ioCompletion = 0L; 
		return(PBControl((ParmBlkPtr)(pb),false));		/* sync */
	} 
	else if (completion == (TCPIOCompletionProc)-1L) 
	{ 
		(pb)->ioCompletion = 0L; 
		return(PBControl((ParmBlkPtr)(pb),true));		/* async */
	} 
	else 
	{  
		(pb)->ioCompletion = completion; 
		return(PBControl((ParmBlkPtr)(pb),true));		/* async */
	} 
}

